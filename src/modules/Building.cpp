#include "pch.h"
#include "Building.hpp"
#include "Inventory.hpp"
#include "XP.hpp"
#include "Utils.hpp"
#include "UObject.hpp"
#include "FName.hpp"


bool Building::CanBePlacedByPlayer(UClass* buildClass) {
    auto gameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
    if (!gameState) return false;
    return gameState->AllPlayerBuildableClasses.Search([buildClass](UClass* cls) { return cls == buildClass; }) != nullptr;
}

static void SetEditingPlayer(ABuildingSMActor* building, AFortPlayerStateZone* newEditingPlayer) {
    if (building->Role != EENetRole::ROLE_Authority || (building->EditingPlayer && newEditingPlayer))
        return;

    building->SetNetDormancy(EENetDormancy((2 - (newEditingPlayer != 0))));
    building->ForceNetUpdate();

    if (building->EditingPlayer) {
        auto handle = building->EditingPlayer->Owner;
        if (auto playerController = CastSDK<AFortPlayerController>(handle)) {
            building->EditingPlayer = newEditingPlayer;
        }
    } else {
        building->EditingPlayer = newEditingPlayer;
    }
}

void Building::ServerCreateBuildingActor(UObject* context, Params::AFortPlayerController_ServerCreateBuildingActor* params) {
    auto playerController = (AFortPlayerController*)context;
    if (!playerController)
        return;

    auto& createBuildingData = params->CreateBuildingData;

    auto gameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
    if (!gameState) return;

    UClass* buildingClass = nullptr;
    auto found = gameState->AllPlayerBuildableClassesIndexLookup.SearchForKey([&](UClass* cls, int32_t handle) {
        return handle == createBuildingData.BuildingClassHandle;
    });
    if (found)
        buildingClass = found->Key();

    if (!buildingClass)
        return;

    auto smDefault = (ABuildingSMActor*)buildingClass->ClassDefaultObject;
    if (!smDefault) return;

    auto resource = UFortKismetLibrary::K2_GetResourceItemDefinition(smDefault->ResourceType);

    FFortItemEntry* itemEntry = nullptr;
    if (!playerController->bBuildFree) {
        itemEntry = playerController->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry) {
            return entry.ItemDefinition == resource;
        });
        if (!itemEntry || itemEntry->Count < 10) {
            playerController->ClientSendMessage(UKismetTextLibrary::Conv_StringToText(Utils::ToFString(L"Not enough resources to build! Change building material or gather more!")), nullptr);
            playerController->ClientTriggerUIFeedbackEvent(MakeFName(L"BuildPreviewUnableToAfford"));
            return;
        }
    }

    ABuildingSMActor* building = Utils::SpawnActor<ABuildingSMActor>(buildingClass, createBuildingData.BuildLoc, createBuildingData.BuildRot, playerController);
    if (!building)
        return;

    building->CurrentBuildingLevel = createBuildingData.BuildingClassData.UpgradeLevel;
    building->OnRep_CurrentBuildingLevel();
    building->SetMirrored(createBuildingData.bMirrored);
    building->bPlayerPlaced = true;
    building->InitializeKismetSpawnedBuildingActor(building, playerController, true, nullptr);

    if (!playerController->bBuildFree && itemEntry) {
        itemEntry->Count -= 10;
        if (itemEntry->Count <= 0)
            Inventory::Remove(playerController, itemEntry->ItemGuid);
        else
            Inventory::ReplaceEntry(playerController, *itemEntry);
    }

    building->TeamIndex = ((AFortPlayerStateAthena*)playerController->PlayerState)->TeamIndex;
    building->Team = (EEFortTeam)building->TeamIndex;
}

void Building::ServerBeginEditingBuildingActor(UObject* context, Params::AFortPlayerController_ServerBeginEditingBuildingActor* params) {
    auto playerController = (AFortPlayerController*)context;
    auto building = params->BuildingActorToEdit;

    if (!playerController || !playerController->MyFortPawn || !building)
        return;

    auto playerState = (AFortPlayerStateAthena*)playerController->PlayerState;
    if (!playerState || building->TeamIndex != playerState->TeamIndex)
        return;

    SetEditingPlayer(building, playerState);

    auto editToolEntry = playerController->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry) {
        return entry.ItemDefinition && entry.ItemDefinition->IsA(UFortEditToolItemDefinition::StaticClass());
    });
    if (!editToolEntry) return;

    playerController->MyFortPawn->EquipWeaponDefinition((UFortWeaponItemDefinition*)editToolEntry->ItemDefinition, editToolEntry->ItemGuid, editToolEntry->TrackerGuid, false);

    if (auto editTool = CastSDK<AFortWeap_EditingTool>(playerController->MyFortPawn->CurrentWeapon)) {
        editTool->EditActor = building;
        editTool->OnRep_EditActor();
    }
}

void Building::ServerEditBuildingActor(UObject* context, Params::AFortPlayerController_ServerEditBuildingActor* params) {
    auto playerController = (AFortPlayerController*)context;
    auto building = params->BuildingActorToEdit;
    auto newClass = params->NewBuildingClass;
    auto rotationIterations = params->RotationIterations;
    auto bMirrored = params->bMirrored;

    if (!playerController || !building || !newClass || !CanBePlacedByPlayer(newClass))
        return;

    auto playerState = (AFortPlayerStateAthena*)playerController->PlayerState;
    if (!playerState || building->TeamIndex != playerState->TeamIndex || building->bDestroyed)
        return;

    if (!building->IsA(ABuildingSMActor::StaticClass()))
        return;

    SetEditingPlayer(building, nullptr);

    using ReplaceBuildingActor_t = ABuildingSMActor* (*)(ABuildingSMActor*, unsigned int, UObject*, unsigned int, int, bool, AFortPlayerController*);
    static ReplaceBuildingActor_t replaceBuildingActor = nullptr;
    if (!replaceBuildingActor)
        replaceBuildingActor = (ReplaceBuildingActor_t)(Sarah::ImageBase + Off::ReplaceBuildingActor);

    ABuildingSMActor* newBuild = replaceBuildingActor(building, 1, newClass, building->CurrentBuildingLevel, rotationIterations, bMirrored, playerController);

    if (newBuild)
        newBuild->bPlayerPlaced = true;
}

void Building::ServerEndEditingBuildingActor(UObject* context, Params::AFortPlayerController_ServerEndEditingBuildingActor* params) {
    auto playerController = (AFortPlayerController*)context;
    auto building = params->BuildingActorToStopEditing;

    if (!playerController || !playerController->MyFortPawn || !building)
        return;

    auto playerState = (AFortPlayerStateAthena*)playerController->PlayerState;
    if (!playerState || building->EditingPlayer != playerState || building->TeamIndex != playerState->TeamIndex || building->bDestroyed)
        return;

    SetEditingPlayer(building, nullptr);

    auto editToolEntry = playerController->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry) {
        return entry.ItemDefinition && entry.ItemDefinition->IsA(UFortEditToolItemDefinition::StaticClass());
    });
    if (!editToolEntry) return;

    playerController->MyFortPawn->EquipWeaponDefinition((UFortWeaponItemDefinition*)editToolEntry->ItemDefinition, editToolEntry->ItemGuid, editToolEntry->TrackerGuid, false);

    if (auto editTool = CastSDK<AFortWeap_EditingTool>(playerController->MyFortPawn->CurrentWeapon)) {
        editTool->EditActor = nullptr;
        editTool->OnRep_EditActor();
    }
}

void Building::ServerRepairBuildingActor(UObject* context, Params::AFortPlayerController_ServerRepairBuildingActor* params) {
    auto playerController = (AFortPlayerController*)context;
    auto building = params->BuildingActorToRepair;

    if (!playerController || !building)
        return;

    auto price = (int32_t)std::floor((10.f * (1.f - building->GetHealthPercent())) * 0.75f);
    auto res = UFortKismetLibrary::K2_GetResourceItemDefinition(building->ResourceType);
    auto itemEntry = playerController->WorldInventory->Inventory.ReplicatedEntries.Search([res](FFortItemEntry& entry) {
        return entry.ItemDefinition == res;
    });
    if (!itemEntry) return;

    itemEntry->Count -= price;
    if (itemEntry->Count <= 0)
        Inventory::Remove(playerController, itemEntry->ItemGuid);
    else
        Inventory::ReplaceEntry(playerController, *itemEntry);

    building->RepairBuilding(playerController, price);

    if (auto controllerAthena = CastSDK<AFortPlayerControllerAthena>(playerController))
        controllerAthena->BuildingsRepaired++;
}

void Building::ServerSpawnDeco(UObject* context, Params::AFortDecoTool_ServerSpawnDeco* params) {
    auto decoTool = (AFortDecoTool*)context;
    auto attachedActor = params->AttachedActor;
    auto inBuildingAttachmentType = params->InBuildingAttachmentType;

    if (!decoTool || !attachedActor)
        return;

    auto itemDefinition = (UFortDecoItemDefinition*)decoTool->ItemDefinition;

    if (auto contextTrapTool = CastSDK<AFortDecoTool_ContextTrap>(decoTool)) {
        switch ((int)inBuildingAttachmentType) {
        case 0:
        case 6:
            itemDefinition = contextTrapTool->ContextTrapItemDefinition->FloorTrap;
            break;
        case 7:
        case 2:
            itemDefinition = contextTrapTool->ContextTrapItemDefinition->CeilingTrap;
            break;
        case 1:
            itemDefinition = contextTrapTool->ContextTrapItemDefinition->WallTrap;
            break;
        case 8:
            itemDefinition = contextTrapTool->ContextTrapItemDefinition->StairTrap;
            break;
        }
    }

    if (!itemDefinition) return;

    auto newTrap = Utils::SpawnActor<ABuildingActor>(itemDefinition->BlueprintClass.Get(), params->Location, params->Rotation, attachedActor);
    if (!newTrap) return;

    attachedActor->AttachBuildingActorToMe(newTrap, true);
    attachedActor->bHiddenDueToTrapPlacement = itemDefinition->bReplacesBuildingWhenPlaced;
    if (itemDefinition->bReplacesBuildingWhenPlaced)
        attachedActor->bActorEnableCollision = false;
    attachedActor->ForceNetUpdate();

    auto pawn = (APawn*)decoTool->Owner;
    if (!pawn)
        return;
    auto playerController = CastSDK<AFortPlayerControllerAthena>(pawn->Controller);
    if (!playerController)
        return;

    auto itemEntry = playerController->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry) {
        return entry.ItemDefinition == decoTool->ItemDefinition;
    });
    if (!itemEntry)
        return;

    itemEntry->Count--;
    if (itemEntry->Count <= 0)
        Inventory::Remove(playerController, itemEntry->ItemGuid);
    else
        Inventory::ReplaceEntry(playerController, *itemEntry);

    auto playerState = (AFortPlayerStateAthena*)playerController->PlayerState;
    if (playerState && newTrap->TeamIndex != playerState->TeamIndex) {
        newTrap->TeamIndex = playerState->TeamIndex;
        newTrap->Team = (EEFortTeam)newTrap->TeamIndex;
    }
}

static EEFortBuildingType GetBuildingTypeFromBuildingAttachmentType(EEBuildingAttachmentType buildingAttachmentType) {
    if (uint8_t(buildingAttachmentType) <= 7) {
        uint32_t val = 0xC5;
        if (val & (1u << uint8_t(buildingAttachmentType)))
            return EEFortBuildingType::Floor;
    }
    if (buildingAttachmentType == EEBuildingAttachmentType::ATTACH_Wall)
        return EEFortBuildingType::Wall;
    return EEFortBuildingType::None;
}

void Building::ServerCreateBuildingAndSpawnDeco(UObject* context, Params::AFortDecoTool_ServerCreateBuildingAndSpawnDeco* params) {
    auto tool = (AFortDecoTool*)context;
    if (!tool) return;

    auto pawn = (APawn*)tool->Owner;
    if (!pawn) return;
    auto playerController = CastSDK<AFortPlayerControllerAthena>(pawn->Controller);
    if (!playerController) return;

    auto itemDefinition = (UFortDecoItemDefinition*)tool->ItemDefinition;

    if (auto contextTrapTool = CastSDK<AFortDecoTool_ContextTrap>(tool)) {
        switch ((int)params->InBuildingAttachmentType) {
        case 0:
        case 6:
            itemDefinition = contextTrapTool->ContextTrapItemDefinition->FloorTrap;
            break;
        case 7:
        case 2:
            itemDefinition = contextTrapTool->ContextTrapItemDefinition->CeilingTrap;
            break;
        case 1:
            itemDefinition = contextTrapTool->ContextTrapItemDefinition->WallTrap;
            break;
        case 8:
            itemDefinition = contextTrapTool->ContextTrapItemDefinition->StairTrap;
            break;
        }
    }

    if (!itemDefinition) return;

    auto gameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
    if (!gameState) return;

    auto wantedType = GetBuildingTypeFromBuildingAttachmentType(params->InBuildingAttachmentType);

    UClass* buildClass = nullptr;
    for (auto cls : gameState->AllPlayerBuildableClasses) {
        if (!cls) continue;
        auto cdo = (ABuildingSMActor*)cls->ClassDefaultObject;
        if (cdo && cdo->BuildingType == wantedType) {
            buildClass = cls;
            break;
        }
    }
    if (!buildClass) return;

    auto smDefault = (ABuildingSMActor*)buildClass->ClassDefaultObject;
    auto resource = UFortKismetLibrary::K2_GetResourceItemDefinition(smDefault->ResourceType);
    auto itemEntry = playerController->WorldInventory->Inventory.ReplicatedEntries.Search([resource](FFortItemEntry& entry) {
        return entry.ItemDefinition == resource;
    });
    if (!itemEntry) return;

    ABuildingSMActor* building = Utils::SpawnActor<ABuildingSMActor>(buildClass, params->BuildingLocation, params->BuildingRotation, playerController);
    if (!building) return;

    building->bPlayerPlaced = true;
    building->InitializeKismetSpawnedBuildingActor(building, playerController, true, nullptr);

    if (!playerController->bBuildFree) {
        itemEntry->Count -= 10;
        if (itemEntry->Count <= 0)
            Inventory::Remove(playerController, itemEntry->ItemGuid);
        else
            Inventory::ReplaceEntry(playerController, *itemEntry);
    }

    auto playerState = (AFortPlayerStateAthena*)playerController->PlayerState;
    if (playerState) {
        building->TeamIndex = playerState->TeamIndex;
        building->Team = (EEFortTeam)building->TeamIndex;
    }

    tool->ServerSpawnDeco(params->Location, params->Rotation, building, params->InBuildingAttachmentType);
}

void Building::OnDamageServer(ABuildingSMActor* actor, float damage, FGameplayTagContainer damageTags, FVector momentum, FHitResult hitInfo, AFortPlayerControllerAthena* instigatedBy, AActor* damageCauser, FGameplayEffectContextHandle effectContext) {
    auto gameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
    if (!instigatedBy || !actor || actor->bPlayerPlaced || actor->GetHealth() == 1) {
        if (BuildingActor_OnDamageServerOG) BuildingActor_OnDamageServerOG(actor, damage, damageTags, momentum, hitInfo, instigatedBy, damageCauser, effectContext);
        return;
    }

    auto weapon = CastSDK<AFortWeapon>(damageCauser);
    if (!weapon || !weapon->WeaponData || !weapon->WeaponData->IsA(UFortWeaponMeleeItemDefinition::StaticClass())) {
        if (BuildingActor_OnDamageServerOG) BuildingActor_OnDamageServerOG(actor, damage, damageTags, momentum, hitInfo, instigatedBy, damageCauser, effectContext);
        return;
    }

    FName pickaxeTag = MakeFName(L"Weapon.Melee.Impact.Pickaxe");
    bool hasPickaxeTag = false;
    for (auto& tag : damageTags.GameplayTags) {
        if (tag.TagName == pickaxeTag) {
            hasPickaxeTag = true;
            break;
        }
    }
    if (!hasPickaxeTag) {
        if (BuildingActor_OnDamageServerOG) BuildingActor_OnDamageServerOG(actor, damage, damageTags, momentum, hitInfo, instigatedBy, damageCauser, effectContext);
        return;
    }

    auto resource = UFortKismetLibrary::K2_GetResourceItemDefinition(actor->ResourceType);
    if (!resource) {
        if (BuildingActor_OnDamageServerOG) BuildingActor_OnDamageServerOG(actor, damage, damageTags, momentum, hitInfo, instigatedBy, damageCauser, effectContext);
        return;
    }

    int maxMat = (int)Utils::EvaluateScalableFloat(resource->MaxStackSize);
    int resCount = 0;

    if (actor->BuildingResourceAmountOverride.RowName.ComparisonIndex > 0) {
        float out = Utils::EvaluateCurve(actor->BuildingResourceAmountOverride, 0.f);
        float rc = out / (actor->GetMaxHealth() / damage);
        resCount = (int)round(rc);
    }

    if (resCount > 0) {
        auto itemEntry = instigatedBy->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry) {
            return entry.ItemDefinition == resource;
        });

        if (itemEntry) {
            itemEntry->Count += resCount;
            if (itemEntry->Count > maxMat) {
                Inventory::SpawnPickup(instigatedBy->MyFortPawn ? instigatedBy->MyFortPawn->K2_GetActorLocation() : actor->K2_GetActorLocation(), itemEntry->ItemDefinition, itemEntry->Count - maxMat, 0, EEFortPickupSourceTypeFlag::Tossed, EEFortPickupSpawnSource::Unset, instigatedBy->MyFortPawn);
                itemEntry->Count = maxMat;
            }
            Inventory::ReplaceEntry(instigatedBy, *itemEntry);
        } else {
            if (resCount > maxMat) {
                Inventory::SpawnPickup(instigatedBy->MyFortPawn ? instigatedBy->MyFortPawn->K2_GetActorLocation() : actor->K2_GetActorLocation(), resource, resCount - maxMat, 0, EEFortPickupSourceTypeFlag::Tossed, EEFortPickupSpawnSource::Unset, instigatedBy->MyFortPawn);
                resCount = maxMat;
            }
            Inventory::GiveItem(instigatedBy, resource, resCount, 0, 0, false);
        }
    }

    instigatedBy->ClientReportDamagedResourceBuilding(actor, resCount == 0 ? EEFortResourceType::None : actor->ResourceType, resCount, false, damage == 100.f);

    if (BuildingActor_OnDamageServerOG)
        BuildingActor_OnDamageServerOG(actor, damage, damageTags, momentum, hitInfo, instigatedBy, damageCauser, effectContext);
}

void Building::Hook() {
}
