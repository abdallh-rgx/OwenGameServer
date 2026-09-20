#include "pch.h"
#include "Inventory.hpp"
#include "Player.hpp"
#include "Utils.hpp"
#include "UObject.hpp"
#include "FName.hpp"

FFortRangedWeaponStats* Inventory::GetStats(UFortWeaponItemDefinition* def) {
    if (!def) return nullptr;
    if (!def->WeaponStatHandle.DataTable) return nullptr;
    auto val = def->WeaponStatHandle.DataTable->RowMap.Search([def](FName& key, uint8_t* value) {
        return def->WeaponStatHandle.RowName == key && value;
    });
    return val ? *(FFortRangedWeaponStats**)val : nullptr;
}

int Inventory::GetLevel(const FDataTableCategoryHandle& categoryHandle) {
    auto gameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
    auto gameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;

    if (!gameState) return 0;
    if (!categoryHandle.DataTable) return 0;
    if (!categoryHandle.ColumnName.ComparisonIndex) return 0;
    if (!categoryHandle.RowContents.ComparisonIndex) return 0;

    int level = 0;
    FFortLootLevelData* lootLevelData = nullptr;
    for (auto& pair : categoryHandle.DataTable->RowMap) {
        FFortLootLevelData* row = (FFortLootLevelData*)pair.Value();
        if (!row) continue;
        if (row->category != categoryHandle.RowContents || row->LootLevel > gameState->WorldLevel || row->LootLevel <= level)
            continue;
        level = row->LootLevel;
        lootLevelData = row;
    }

    if (lootLevelData) {
        auto subbed = lootLevelData->MaxItemLevel - lootLevelData->MinItemLevel;

        if (subbed <= -1)
            subbed = 0;
        else {
            auto calc = (int)(((float)rand() / 32767) * (float)(subbed + 1));
            if (calc <= subbed)
                subbed = calc;
        }

        return subbed + lootLevelData->MinItemLevel;
    }

    return 0;
}

FFortItemEntry* Inventory::MakeItemEntry(UFortItemDefinition* def, int32_t count, int32_t level) {
    FFortItemEntry* entry = new FFortItemEntry();
    memset(entry, 0, sizeof(FFortItemEntry));

    entry->MostRecentArrayReplicationKey = -1;
    entry->ReplicationID = -1;
    entry->ReplicationKey = -1;
    entry->ItemDefinition = def;
    entry->Count = count;
    entry->Durability = 1.f;
    { FGameplayAbilitySpecHandle specHandle{}; specHandle.Handle = -1; entry->GameplayAbilitySpecHandle = specHandle; }
    entry->Level = level;

    if (auto weapon = CastSDK<UFortWeaponItemDefinition>(def)) {
        auto stats = GetStats(weapon);
        if (stats) {
            entry->LoadedAmmo = stats->ClipSize;
            if (weapon->bUsesPhantomReserveAmmo)
                entry->PhantomReserveAmmo = stats->InitialClips * stats->ClipSize;
        }
    }
    return entry;
}

void Inventory::TriggerInventoryUpdate(AFortPlayerController* pc, FFortItemEntry* entry) {
    if (!pc || !pc->WorldInventory) return;
    pc->WorldInventory->bRequiresLocalUpdate = true;
    pc->WorldInventory->HandleInventoryLocalUpdate();
    if (entry)
        Utils::MarkItemDirty(pc->WorldInventory->Inventory, *entry);
    else
        Utils::MarkArrayDirty(pc->WorldInventory->Inventory);
}

UFortWorldItem* Inventory::GiveItem(AFortPlayerController* pc, UFortItemDefinition* def,
    int count, int loadedAmmo, int level, bool showPickupNoti, bool updateInventory, int phantomReserve) {
    if (!pc || !pc->WorldInventory || !def || !count)
        return nullptr;

    // Engine-managed path ONLY: GiveItemToInventoryOwner (ProcessEvent) lets
    // the engine grow the inventory arrays with ITS OWN allocator (FMalloc).
    //
    // The old "manual mode" used TArray<T>::AddGrow -> UC::ContainerRealloc
    // -> std::realloc on arrays owned by the game (WorldInventory->Inventory),
    // i.e. libc's realloc on UE-allocator memory: undefined behaviour that
    // corrupted the heap and crashed the game later inside libUnreal.so on
    // the GameThread. That path is removed for good - if the item is not a
    // UFortWorldItemDefinition, spawn a pickup instead of touching the arrays.
    if (auto worldDef = CastSDK<UFortWorldItemDefinition>(def)) {
        Params::UFortKismetLibrary_GiveItemToInventoryOwner params{};
        params.InventoryOwner.ObjectPointer = pc;
        params.InventoryOwner.InterfacePointer = pc;
        params.ItemDefinition = worldDef;
        params.ItemVariantGuid = FGuid();
        params.NumberToGive = count;
        params.bNotifyPlayer = showPickupNoti;
        params.ItemLevel = level;
        params.PickupInstigatorHandle = 0;
        params.bUseItemPickupAnalyticEvent = false;

        Sarah::CallProcessEvent(UFortKismetLibrary::GetDefaultObj(),
            (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortKismetLibrary.GiveItemToInventoryOwner"),
            &params);
        return nullptr;
    }

    // Not a world item (trap/gadget definitions): drop it as a pickup at the
    // player's location instead of manually editing engine-owned arrays.
    if (pc->MyFortPawn) {
        FVector loc = pc->MyFortPawn->K2_GetActorLocation();
        FFortItemEntry* entry = MakeItemEntry(def, count, level);
        if (entry) {
            entry->LoadedAmmo = loadedAmmo;
            entry->PhantomReserveAmmo = phantomReserve;
            SpawnPickup(loc, *entry, EEFortPickupSourceTypeFlag::Player, EEFortPickupSpawnSource::Unset,
                pc->MyFortPawn, -1, true, true);
            delete entry;
        }
    }
    return nullptr;
}

UFortWorldItem* Inventory::GiveItem(AFortPlayerController* pc, FFortItemEntry entry, int count,
    bool showPickupNoti, bool updateInventory) {
    if (count == -1)
        count = entry.Count;

    return GiveItem(pc, entry.ItemDefinition, count, entry.LoadedAmmo, entry.Level,
        showPickupNoti, updateInventory, entry.PhantomReserveAmmo);
}

EEFortQuickBars Inventory::GetQuickbar(UFortItemDefinition* def) {
    if (!def)
        return EEFortQuickBars::Max_None;

    if (def->IsA(UFortWeaponMeleeItemDefinition::StaticClass()) ||
        def->IsA(UFortEditToolItemDefinition::StaticClass()))
        return EEFortQuickBars::Primary;

    if (def->IsA(UFortResourceItemDefinition::StaticClass()) ||
        def->IsA(UFortAmmoItemDefinition::StaticClass()) ||
        def->IsA(UFortTrapItemDefinition::StaticClass()) ||
        def->IsA(UFortBuildingItemDefinition::StaticClass()) ||
        def->IsA(UFortEditToolItemDefinition::StaticClass()))
        return EEFortQuickBars::Secondary;

    if (((UFortWorldItemDefinition*)def)->bForceIntoOverflow)
        return EEFortQuickBars::Secondary;

    return EEFortQuickBars::Primary;
}

AFortPickupAthena* Inventory::SpawnPickup(FVector loc, FFortItemEntry& entry,
    EEFortPickupSourceTypeFlag sourceType, EEFortPickupSpawnSource spawnSource,
    AFortPlayerPawn* pawn, int overrideCount, bool toss, bool randomRotation, bool bCombine) {

    AFortPickupAthena* newPickup = Utils::SpawnActor<AFortPickupAthena>(loc);
    if (!newPickup)
        return nullptr;

    newPickup->bRandomRotation = randomRotation;
    newPickup->PrimaryPickupItemEntry.ItemDefinition = entry.ItemDefinition;
    newPickup->PrimaryPickupItemEntry.LoadedAmmo = entry.LoadedAmmo;
    newPickup->PrimaryPickupItemEntry.Count = overrideCount != -1 ? overrideCount : entry.Count;
    newPickup->PrimaryPickupItemEntry.PhantomReserveAmmo = entry.PhantomReserveAmmo;
    newPickup->OnRep_PrimaryPickupItemEntry();
    newPickup->PawnWhoDroppedPickup = MakeWeakPtr(static_cast<AFortPawn*>(pawn));

    newPickup->TossPickup(loc, pawn, -1, toss, true, sourceType, spawnSource);
    newPickup->bTossedFromContainer = spawnSource == EEFortPickupSpawnSource::Chest || spawnSource == EEFortPickupSpawnSource::AmmoBox;
    if (newPickup->bTossedFromContainer)
        newPickup->OnRep_TossedFromContainer();

    newPickup->SetNetDormancy(EENetDormancy::DORM_DormantAll);

    return newPickup;
}

AFortPickupAthena* Inventory::SpawnPickup(FVector loc, UFortItemDefinition* def, int count, int loadedAmmo,
    EEFortPickupSourceTypeFlag sourceType, EEFortPickupSpawnSource spawnSource,
    AFortPlayerPawn* pawn, bool toss, bool randomRotation) {
    FFortItemEntry* entry = MakeItemEntry(def, count, 0);
    AFortPickupAthena* ret = SpawnPickup(loc, *entry, sourceType, spawnSource, pawn, -1, toss, randomRotation);
    delete entry;
    return ret;
}

AFortPickupAthena* Inventory::SpawnPickupFromContainer(ABuildingContainer* container, FFortItemEntry entry,
    AFortPlayerPawn* pawn, int overrideCount) {
    if (!container)
        return nullptr;

    auto containerLoc = container->K2_GetActorLocation();
    auto loc = containerLoc
        + (container->GetActorForwardVector() * container->LootSpawnLocation_Athena.X)
        + (container->GetActorRightVector() * container->LootSpawnLocation_Athena.Y)
        + (container->GetActorUpVector() * container->LootSpawnLocation_Athena.Z);

    AFortPickupAthena* newPickup = Utils::SpawnActor<AFortPickupAthena>(loc);
    if (!newPickup)
        return nullptr;

    newPickup->bRandomRotation = true;
    newPickup->PrimaryPickupItemEntry.ItemDefinition = entry.ItemDefinition;
    newPickup->PrimaryPickupItemEntry.LoadedAmmo = entry.LoadedAmmo;
    newPickup->PrimaryPickupItemEntry.Count = overrideCount != -1 ? overrideCount : entry.Count;
    newPickup->PrimaryPickupItemEntry.PhantomReserveAmmo = entry.PhantomReserveAmmo;
    newPickup->OnRep_PrimaryPickupItemEntry();
    newPickup->PawnWhoDroppedPickup = MakeWeakPtr(static_cast<AFortPawn*>(pawn));

    UFortKismetLibrary::TossPickupFromContainer(UWorld::GetWorld(), container, newPickup, 1, 0,
        container->LootTossConeHalfAngle_Athena, container->LootTossDirection_Athena, container->LootTossSpeed_Athena, false);
    newPickup->bTossedFromContainer = true;
    newPickup->OnRep_TossedFromContainer();

    newPickup->SetNetDormancy(EENetDormancy::DORM_DormantAll);

    return newPickup;
}

void Inventory::ReplaceEntry(AFortPlayerController* pc, FFortItemEntry& entry) {
    if (!pc || !pc->WorldInventory) return;

    auto ent = pc->WorldInventory->Inventory.ItemInstances.Search([&](UFortWorldItem* item) {
        return item->ItemEntry.ItemGuid == entry.ItemGuid;
    });
    if (ent)
        (*ent)->ItemEntry = entry;

    TriggerInventoryUpdate(pc, &entry);
}

void Inventory::Remove(AFortPlayerController* pc, FGuid guid) {
    if (!pc || !pc->WorldInventory) return;

    auto itemEntryIdx = pc->WorldInventory->Inventory.ReplicatedEntries.SearchIndex([&](FFortItemEntry& entry) {
        return entry.ItemGuid == guid;
    });
    if (itemEntryIdx != -1)
        pc->WorldInventory->Inventory.ReplicatedEntries.Remove(itemEntryIdx);

    auto itemInstanceIdx = pc->WorldInventory->Inventory.ItemInstances.SearchIndex([&](UFortWorldItem* entry) {
        return entry->ItemEntry.ItemGuid == guid;
    });
    if (itemInstanceIdx != -1)
        pc->WorldInventory->Inventory.ItemInstances.Remove(itemInstanceIdx);

    TriggerInventoryUpdate(pc, nullptr);
}
