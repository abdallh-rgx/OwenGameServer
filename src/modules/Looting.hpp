#pragma once
#include "pch.h"
#include <vector>

class Looting {
public:
    static inline std::vector<FFortLootTierData*> TierDataAllGroups;
    static inline std::vector<FFortLootPackageData*> LPGroupsAll;

    static inline void (*ServerAttemptInteractOG)(UObject*, FFrame&) = nullptr;
    static inline void (*PickLootDropsOG)(UObject*, FFrame&, bool*) = nullptr;
    static inline void (*K2_SpawnPickupInWorldOG)(UObject*, FFrame&, AFortPickup**) = nullptr;
    static inline void (*SpawnItemVariantPickupInWorldOG)(UObject*, FFrame&, AFortPickup**) = nullptr;
    static inline void (*SupplyDropSpawnPickupOG)(UObject*, FFrame&, AFortPickup**) = nullptr;

    static std::vector<FFortItemEntry> ChooseLootForContainer(FName tierGroup, int lootTier = -1, int worldLevel = 0);
    static void SpawnLoot(FName& tierGroup, FVector loc);
    static void SpawnFloorLootForContainer(UBlueprintGeneratedClass* containerType);

    static void ServerAttemptInteractHook(UObject*, FFrame&);
    static void PickLootDropsHook(UObject*, FFrame&, bool*);
    static void K2_SpawnPickupInWorldHook(UObject*, FFrame&, AFortPickup**);
    static void SpawnItemVariantPickupInWorldHook(UObject*, FFrame&, AFortPickup**);
    static void SupplyDropSpawnPickupHook(UObject*, FFrame&, AFortPickup**);

    static void Hook();
};
