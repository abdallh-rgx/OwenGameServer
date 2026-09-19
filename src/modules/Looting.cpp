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
                    FFortItemEntry* extra = Inventory::MakeItemEntry(itemDefinition, ogCount - maxStack, std::clamp(Inventory::GetLevel(itemDefinition->LootLevelData), itemDefinition->MinLevel, itemDefinition->MaxLevel));
                    lootDrops.push_back(*extra);
                    delete extra;
                }
            }
            if (Inventory::GetQuickbar(lootDrop.ItemDefinition) == EEFortQuickBars::Secondary) found = true;
        }
    }

    if (!found && lootPackage->Count > 0) {
        FFortItemEntry* entry = Inventory::MakeItemEntry(itemDefinition, lootPackage->Count, std::clamp(Inventory::GetLevel(itemDefinition->LootLevelData), itemDefinition->MinLevel, itemDefinition->MaxLevel));
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
        if (val->TierGroup == tierGroup && (lootTier == -1 ? true : lootTier == val->LootTier)) tierDataGroups.push_back(val);
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
            if (idk > 0.0000099999997f) dropCount += idk >= ((float)rand() / 32767);
        }
    }

    float amountOfLootDrops = 0;
    for (auto& min : lootTierData->LootPackageCategoryMinArray) amountOfLootDrops += min;

    int sumWeights = 0;
    for (int i = 0; i < lootTierData->LootPackageCategoryWeightArray.Num(); ++i)
        if (lootTierData->LootPackageCategoryWeightArray[i] > 0 && lootTierData->LootPackageCategoryMaxArray[i] != 0)
            sumWeights += lootTierData->LootPackageCategoryWeightArray[i];

    while (sumWeights > 0) {
        amountOfLootDrops++;
        if (amountOfLootDrops >= lootTierData->NumLootPackageDrops) break;
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
            if (ammoMap[ammoDefinition] > 0 && i < ammoMap[ammoDefinition]) { i++; continue; }
            if (entry.ItemDefinition == ammoDefinition) { hasAmmoEntry = true; ammoMap[ammoDefinition]++; break; }
        }
        if (hasAmmoEntry) continue;

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
            if (pair.Key() == tierGroup) { realTierGroup = pair.Value(); break; }
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
        if (container) container->BP_SpawnLoot(nullptr);
    }
}

void Looting::ServerAttemptInteractHook(UObject* Context, FFrame& Stack) {
    STACK_SAVE(Stack, _saved);
    UObject* ReceivingActor_Obj = nullptr;
    UObject* InteractComponent = nullptr;
    UObject* InteractionType = nullptr;
    Stack.StepCompiledIn(&ReceivingActor_Obj);
    Stack.StepCompiledIn(&InteractComponent);
    Stack.StepCompiledIn(&InteractionType);
    Stack.IncrementCode();

    auto container = CastSDK<ABuildingContainer>(ReceivingActor_Obj);
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

    CALL_OG_VOID(Stack, Context, ServerAttemptInteractOG, _saved);
}

void Looting::PickLootDropsHook(UObject* Context, FFrame& Stack, bool* Ret) {
    STACK_SAVE(Stack, _saved);
    UObject* WorldContextObject = nullptr;
    FName TierGroupName;
    int32 WorldLevel = 0;
    int32 ForcedLootTier = 0;
    Stack.StepCompiledIn(&WorldContextObject);
    auto& OutLootToDrop = Stack.StepCompiledInRef<TArray<FFortItemEntry>>();
    Stack.StepCompiledIn(&TierGroupName);
    Stack.StepCompiledIn(&WorldLevel);
    Stack.StepCompiledIn(&ForcedLootTier);
    Stack.IncrementCode();

    auto LootDrops = ChooseLootForContainer(TierGroupName, ForcedLootTier, WorldLevel);
    for (auto& ld : LootDrops) OutLootToDrop.Add(ld);
    if (Ret) *Ret = true;

    CALL_OG_RET(Stack, Context, PickLootDropsOG, _saved, Ret);
}

void Looting::K2_SpawnPickupInWorldHook(UObject* Context, FFrame& Stack, AFortPickup** Ret) {
    STACK_SAVE(Stack, _saved);
    UObject* WorldContextObject = nullptr;
    UFortWorldItemDefinition* ItemDefinition = nullptr;
    int32 NumberToSpawn = 0;
    FVector Position, Direction;
    int32 OverrideMaxStackCount = 0;
    bool bToss = false, bRandomRotation = false, bBlockedFromAutoPickup = false;
    int32 PickupInstigatorHandle = 0;
    EEFortPickupSourceTypeFlag SourceType{};
    EEFortPickupSpawnSource Source{};
    AFortPlayerController* OptionalOwnerPC = nullptr;
    bool bPickupOnlyRelevantToOwner = false;
    Stack.StepCompiledIn(&WorldContextObject);
    Stack.StepCompiledIn(&ItemDefinition);
    Stack.StepCompiledIn(&NumberToSpawn);
    Stack.StepCompiledIn(&Position);
    Stack.StepCompiledIn(&Direction);
    Stack.StepCompiledIn(&OverrideMaxStackCount);
    Stack.StepCompiledIn(&bToss);
    Stack.StepCompiledIn(&bRandomRotation);
    Stack.StepCompiledIn(&bBlockedFromAutoPickup);
    Stack.StepCompiledIn(&PickupInstigatorHandle);
    Stack.StepCompiledIn(&SourceType);
    Stack.StepCompiledIn(&Source);
    Stack.StepCompiledIn(&OptionalOwnerPC);
    Stack.StepCompiledIn(&bPickupOnlyRelevantToOwner);
    Stack.IncrementCode();

    int loadedAmmo = 0;
    if (auto weaponDef = CastSDK<UFortWeaponItemDefinition>(ItemDefinition)) {
        auto stats = Inventory::GetStats(weaponDef);
        loadedAmmo = stats ? stats->ClipSize : 0;
    }
    if (Ret) *Ret = Inventory::SpawnPickup(Position, ItemDefinition, NumberToSpawn, loadedAmmo, SourceType, Source, OptionalOwnerPC ? OptionalOwnerPC->MyFortPawn : nullptr, bToss, bRandomRotation);

    CALL_OG_RET(Stack, Context, K2_SpawnPickupInWorldOG, _saved, Ret);
}

void Looting::SpawnItemVariantPickupInWorldHook(UObject* Context, FFrame& Stack, AFortPickup** Ret) {
    STACK_SAVE(Stack, _saved);
    UObject* WorldContextObject = nullptr;
    FSpawnItemVariantParams Params;
    Stack.StepCompiledIn(&WorldContextObject);
    Stack.StepCompiledIn(&Params);
    Stack.IncrementCode();

    if (Ret) *Ret = Inventory::SpawnPickup(Params.Position, Params.WorldItemDefinition, Params.NumberToSpawn, 0, Params.SourceType, Params.Source, nullptr, Params.bToss, Params.bRandomRotation);

    CALL_OG_RET(Stack, Context, SpawnItemVariantPickupInWorldOG, _saved, Ret);
}

void Looting::SupplyDropSpawnPickupHook(UObject* Context, FFrame& Stack, AFortPickup** Ret) {
    STACK_SAVE(Stack, _saved);
    UFortWorldItemDefinition* ItemDefinition = nullptr;
    int32 NumberToSpawn = 0;
    FVector Position, Direction;
    Stack.StepCompiledIn(&ItemDefinition);
    Stack.StepCompiledIn(&NumberToSpawn);
    Stack.StepCompiledIn(&Position);
    Stack.StepCompiledIn(&Direction);
    Stack.IncrementCode();

    int loadedAmmo = 0;
    if (auto weaponDef = CastSDK<UFortWeaponItemDefinition>(ItemDefinition)) {
        auto stats = Inventory::GetStats(weaponDef);
        loadedAmmo = stats ? stats->ClipSize : 0;
    }
    if (Ret) *Ret = Inventory::SpawnPickup(Position, ItemDefinition, NumberToSpawn, loadedAmmo, EEFortPickupSourceTypeFlag::Other, EEFortPickupSpawnSource::SupplyDrop);

    CALL_OG_RET(Stack, Context, SupplyDropSpawnPickupOG, _saved, Ret);
}

void Looting::Hook() {
    Utils::ExecHook(L"/Script/FortniteGame.FortControllerComponent_Interaction.ServerAttemptInteract", (void*)ServerAttemptInteractHook, ServerAttemptInteractOG);
    Utils::ExecHook(L"/Script/FortniteGame.FortKismetLibrary.PickLootDrops", (void*)PickLootDropsHook, PickLootDropsOG);
    Utils::ExecHook(L"/Script/FortniteGame.FortKismetLibrary.K2_SpawnPickupInWorld", (void*)K2_SpawnPickupInWorldHook, K2_SpawnPickupInWorldOG);
    Utils::ExecHook(L"/Script/FortniteGame.FortKismetLibrary.SpawnItemVariantPickupInWorld", (void*)SpawnItemVariantPickupInWorldHook, SpawnItemVariantPickupInWorldOG);
    Utils::ExecHook(L"/Script/FortniteGame.FortAthenaSupplyDrop.SpawnPickup", (void*)SupplyDropSpawnPickupHook, SupplyDropSpawnPickupOG);
}
