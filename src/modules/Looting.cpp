#include "pch.h"
#include "Looting.hpp"
#include "Inventory.hpp"
#include "Utils.hpp"
#include "UObject.hpp"
#include "FName.hpp"

static void SetupLDSForPackage(std::vector<FFortItemEntry>& lootDrops, FName package, int i, FName tierGroup, int worldLevel) {
    std::vector<FFortLootPackageData*> lpGroups;
    for (auto const& val : Looting::LPGroupsAll) {
        if (!val) continue;
        if (val->LootPackageID != package) continue;
        if (i != -1 && val->LootPackageCategory != i) continue;
        if (worldLevel >= 0) {
            if (val->MaxWorldLevel >= 0 && worldLevel > val->MaxWorldLevel) continue;
            if (val->MinWorldLevel >= 0 && worldLevel < val->MinWorldLevel) continue;
        }
        lpGroups.push_back(val);
    }
    if (lpGroups.empty()) return;

    auto pickWeighted = [](std::vector<FFortLootPackageData*>& map) -> FFortLootPackageData* {
        float totalWeight = 0.f;
        for (auto* p : map) totalWeight += p->Weight;
        float randomNumber = ((float)rand() / 32767.f) * totalWeight;

        for (auto* element : map) {
            float weight = element->Weight;
            if (weight == 0) continue;
            if (randomNumber <= weight) return element;
            randomNumber -= weight;
        }
        return nullptr;
    };

    auto lootPackage = pickWeighted(lpGroups);
    if (!lootPackage) return;

    if (lootPackage->LootPackageCall.Num() > 1) {
        for (int c = 0; c < lootPackage->Count; c++) {
            std::wstring callPath = lootPackage->LootPackageCall.ToWString();
            FName call = MakeFName(callPath.c_str());
            SetupLDSForPackage(lootDrops, call, 0, tierGroup, worldLevel);
        }
        return;
    }

    std::wstring itemDefPath = FNameToWString(lootPackage->ItemDefinition.ObjectID.AssetPathName);
    auto id = Utils::Find<UFortItemDefinition>(itemDefPath.c_str());
    auto itemDefinition = CastSDK<UFortWorldItemDefinition>(id);
    if (!itemDefinition) return;

    bool found = false;
    for (auto& lootDrop : lootDrops) {
        if (lootDrop.ItemDefinition == itemDefinition) {
            lootDrop.Count += lootPackage->Count;

            int maxStack = (int)Utils::EvaluateScalableFloat(itemDefinition->MaxStackSize);
            if (lootDrop.Count > maxStack) {
                auto ogCount = lootDrop.Count;
                lootDrop.Count = maxStack;

                if (Inventory::GetQuickbar(lootDrop.ItemDefinition) == EEFortQuickBars::Secondary) {
                    FFortItemEntry* extra = Inventory::MakeItemEntry(itemDefinition, ogCount - maxStack,
                        std::clamp(Inventory::GetLevel(itemDefinition->LootLevelData), itemDefinition->MinLevel, itemDefinition->MaxLevel));
                    lootDrops.push_back(*extra);
                    delete extra;
                }
            }

            if (Inventory::GetQuickbar(lootDrop.ItemDefinition) == EEFortQuickBars::Secondary)
                found = true;
        }
    }

    if (!found && lootPackage->Count > 0) {
        FFortItemEntry* entry = Inventory::MakeItemEntry(itemDefinition, lootPackage->Count,
            std::clamp(Inventory::GetLevel(itemDefinition->LootLevelData), itemDefinition->MinLevel, itemDefinition->MaxLevel));
        lootDrops.push_back(*entry);
        delete entry;
    }
}

std::vector<FFortItemEntry> Looting::ChooseLootForContainer(FName tierGroup, int lootTier, int worldLevel) {
    std::vector<FFortItemEntry> lootDrops;

    auto gameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
    if (worldLevel == 0 && gameState) worldLevel = gameState->WorldLevel;

    std::vector<FFortLootTierData*> tierDataGroups;
    for (auto const& val : TierDataAllGroups) {
        if (val->TierGroup == tierGroup && (lootTier == -1 ? true : lootTier == val->LootTier))
            tierDataGroups.push_back(val);
    }

    auto pickWeightedTier = [](std::vector<FFortLootTierData*>& map) -> FFortLootTierData* {
        float totalWeight = 0.f;
        for (auto* p : map) totalWeight += p->Weight;
        float randomNumber = ((float)rand() / 32767.f) * totalWeight;

        for (auto* element : map) {
            float weight = element->Weight;
            if (weight == 0) continue;
            if (randomNumber <= weight) return element;
            randomNumber -= weight;
        }
        return nullptr;
    };

    auto lootTierData = pickWeightedTier(tierDataGroups);
    if (!lootTierData) return lootDrops;

    float dropCount = 0;
    if (lootTierData->NumLootPackageDrops > 0) {
        dropCount = lootTierData->NumLootPackageDrops < 1 ? 1 : (float)((int)((lootTierData->NumLootPackageDrops * 2) - .5f) >> 1);

        if (lootTierData->NumLootPackageDrops > 1) {
            float idk = lootTierData->NumLootPackageDrops - dropCount;
            if (idk > 0.0000099999997f)
                dropCount += idk >= ((float)rand() / 32767);
        }
    }

    float amountOfLootDrops = 0;
    for (auto& min : lootTierData->LootPackageCategoryMinArray)
        amountOfLootDrops += min;

    int sumWeights = 0;
    for (int i = 0; i < lootTierData->LootPackageCategoryWeightArray.Num(); ++i)
        if (lootTierData->LootPackageCategoryWeightArray[i] > 0 && lootTierData->LootPackageCategoryMaxArray[i] != 0)
            sumWeights += lootTierData->LootPackageCategoryWeightArray[i];

    while (sumWeights > 0) {
        amountOfLootDrops++;
        if (amountOfLootDrops >= lootTierData->NumLootPackageDrops)
            break;
        sumWeights--;
    }

    for (int i = 0; i < amountOfLootDrops && i < lootTierData->LootPackageCategoryMinArray.Num(); i++)
        for (int j = 0; j < lootTierData->LootPackageCategoryMinArray[i] && lootTierData->LootPackageCategoryMinArray[i] >= 1; j++)
            SetupLDSForPackage(lootDrops, lootTierData->LootPackage, i, tierGroup, worldLevel);

    std::map<UFortWorldItemDefinition*, int32_t> ammoMap;
    for (auto& item : lootDrops) {
        auto rangedDef = CastSDK<UFortWeaponRangedItemDefinition>(item.ItemDefinition);
        if (!rangedDef || rangedDef->IsStackable()) continue;
        auto ammoDefinition = ((UFortWorldItemDefinition*)item.ItemDefinition)->GetAmmoWorldItemDefinition_BP();
        if (!ammoDefinition) continue;

        int i = 0;
        bool hasAmmoEntry = false;
        for (auto& entry : lootDrops) {
            if (ammoMap[ammoDefinition] > 0 && i < ammoMap[ammoDefinition]) {
                i++;
                continue;
            }
            if (entry.ItemDefinition == ammoDefinition) {
                hasAmmoEntry = true;
                ammoMap[ammoDefinition]++;
                break;
            }
        }

        if (hasAmmoEntry)
            continue;

        FFortLootPackageData* group = nullptr;
        static auto ammoSmall = MakeFName(L"WorldList.AthenaAmmoSmall");
        std::string ammoFullName = ammoDefinition->GetFullName();
        std::wstring ammoPath(ammoFullName.begin(), ammoFullName.end());
        for (auto const& val : LPGroupsAll) {
            if (val->LootPackageID == ammoSmall && FNameToWString(val->ItemDefinition.ObjectID.AssetPathName).find(ammoPath) != std::wstring::npos) {
                group = val;
                break;
            }
        }

        if (group) {
            FFortItemEntry* entry = Inventory::MakeItemEntry(ammoDefinition, group->Count, 0);
            lootDrops.push_back(*entry);
            delete entry;
        }
    }

    return lootDrops;
}

void Looting::SpawnLoot(FName& tierGroup, FVector loc) {
    auto gameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
    FName realTierGroup = tierGroup;

    if (gameMode) {
        for (auto& pair : gameMode->RedirectAthenaLootTierGroups) {
            if (pair.Key() == tierGroup) {
                realTierGroup = pair.Value();
                break;
            }
        }
    }

    for (auto& lootDrop : ChooseLootForContainer(realTierGroup)) {
        Inventory::SpawnPickup(loc, lootDrop);
    }
}

void Looting::SpawnFloorLootForContainer(UBlueprintGeneratedClass* containerType) {
    if (!containerType) return;

    auto containers = Utils::GetAll<ABuildingContainer>(containerType);
    for (auto& container : containers) {
        if (container)
            container->BP_SpawnLoot(nullptr);
    }
}

void Looting::ServerAttemptInteract(UObject* context, Params::UFortControllerComponent_Interaction_ServerAttemptInteract* params) {
    auto component = (UFortControllerComponent_Interaction*)context;
    auto pc = CastSDK<AFortPlayerController>(component->GetOwner());

    auto container = CastSDK<ABuildingContainer>(params->ReceivingActor);
    if (container) {
        if (!container->bAlreadySearched) {
            SpawnLoot(container->SearchLootTierGroup,
                container->K2_GetActorLocation() + container->GetActorRightVector() * 70.f + FVector(0, 0, 50));

            container->bAlreadySearched = true;
            container->OnRep_bAlreadySearched();
            container->SearchBounceData.SearchAnimationCount++;
            container->BounceContainer();
        }
        return;
    }

    Sarah::CallProcessEvent(context, (UFunction*)Sarah::UObjectManager::Find(L"/Script/FortniteGame.FortControllerComponent_Interaction.ServerAttemptInteract"), params);
}

bool Looting::PickLootDrops(UObject* context, Params::UFortKismetLibrary_PickLootDrops* params) {
    auto lootDrops = ChooseLootForContainer(params->TierGroupName, params->ForcedLootTier, params->WorldLevel);

    for (auto& lootDrop : lootDrops) {
        params->OutLootToDrop.AddGrow(lootDrop);
    }

    params->ReturnValue = true;
    return true;
}

AFortPickup* Looting::K2_SpawnPickupInWorld(UObject* context, Params::UFortKismetLibrary_K2_SpawnPickupInWorld* params) {
    int loadedAmmo = 0;
    if (auto weaponDef = CastSDK<UFortWeaponItemDefinition>(params->ItemDefinition)) {
        auto stats = Inventory::GetStats(weaponDef);
        loadedAmmo = stats ? stats->ClipSize : 0;
    }

    return Inventory::SpawnPickup(params->Position, params->ItemDefinition, params->NumberToSpawn, loadedAmmo,
        params->SourceType, params->Source,
        params->OptionalOwnerPC ? params->OptionalOwnerPC->MyFortPawn : nullptr, params->bToss, params->bRandomRotation);
}

AFortPickup* Looting::SpawnItemVariantPickupInWorld(UObject* context, Params::UFortKismetLibrary_SpawnItemVariantPickupInWorld* params) {
    return Inventory::SpawnPickup(params->Params_0.Position, params->Params_0.WorldItemDefinition, params->Params_0.NumberToSpawn, 0,
        params->Params_0.SourceType, params->Params_0.Source, nullptr, params->Params_0.bToss, params->Params_0.bRandomRotation);
}

AFortPickup* Looting::SupplyDropSpawnPickup(UObject* context, Params::AFortAthenaSupplyDrop_SpawnPickup* params) {
    int loadedAmmo = 0;
    if (auto weaponDef = CastSDK<UFortWeaponItemDefinition>(params->ItemDefinition)) {
        auto stats = Inventory::GetStats(weaponDef);
        loadedAmmo = stats ? stats->ClipSize : 0;
    }

    return Inventory::SpawnPickup(params->Position, params->ItemDefinition, params->NumberToSpawn, loadedAmmo,
        EEFortPickupSourceTypeFlag::Other, EEFortPickupSpawnSource::SupplyDrop);
}

void Looting::Hook() {
}
