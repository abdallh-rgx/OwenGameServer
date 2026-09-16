#pragma once
#include "pch.h"

class Inventory {
public:
    static inline bool bManualMode = false;
    static inline bool bModeTested = false;

    static FFortRangedWeaponStats* GetStats(UFortWeaponItemDefinition* def);
    static int GetLevel(const FDataTableCategoryHandle& categoryHandle);
    static FFortItemEntry* MakeItemEntry(UFortItemDefinition* def, int32_t count, int32_t level);
    static void TriggerInventoryUpdate(AFortPlayerController* pc, FFortItemEntry* entry);
    static UFortWorldItem* GiveItem(AFortPlayerController* pc, UFortItemDefinition* def,
        int count = 1, int loadedAmmo = 0, int level = 0,
        bool showPickupNoti = true, bool updateInventory = true, int phantomReserve = 0);
    static UFortWorldItem* GiveItem(AFortPlayerController* pc, FFortItemEntry entry,
        int count = -1, bool showPickupNoti = true, bool updateInventory = true);
    static EEFortQuickBars GetQuickbar(UFortItemDefinition* def);
    static AFortPickupAthena* SpawnPickup(FVector loc, FFortItemEntry& entry,
        EEFortPickupSourceTypeFlag sourceType = EEFortPickupSourceTypeFlag::Tossed,
        EEFortPickupSpawnSource spawnSource = EEFortPickupSpawnSource::Unset,
        AFortPlayerPawn* pawn = nullptr, int overrideCount = -1,
        bool toss = true, bool randomRotation = true, bool bCombine = true);
    static AFortPickupAthena* SpawnPickup(FVector loc, UFortItemDefinition* def, int count, int loadedAmmo,
        EEFortPickupSourceTypeFlag sourceType = EEFortPickupSourceTypeFlag::Tossed,
        EEFortPickupSpawnSource spawnSource = EEFortPickupSpawnSource::Unset,
        AFortPlayerPawn* pawn = nullptr, bool toss = true, bool randomRotation = true);
    static AFortPickupAthena* SpawnPickupFromContainer(ABuildingContainer* container, FFortItemEntry entry,
        AFortPlayerPawn* pawn = nullptr, int overrideCount = -1);
    static void ReplaceEntry(AFortPlayerController* pc, FFortItemEntry& entry);
    static void Remove(AFortPlayerController* pc, FGuid guid);
};
