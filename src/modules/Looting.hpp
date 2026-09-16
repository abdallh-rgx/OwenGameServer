#pragma once
#include "pch.h"
#include <vector>

class Looting {
public:
    static inline std::vector<FFortLootTierData*> TierDataAllGroups;
    static inline std::vector<FFortLootPackageData*> LPGroupsAll;

    static std::vector<FFortItemEntry> ChooseLootForContainer(FName tierGroup, int lootTier = -1, int worldLevel = 0);
    static void SpawnLoot(FName& tierGroup, FVector loc);
    static void SpawnFloorLootForContainer(UBlueprintGeneratedClass* containerType);
    static void ServerAttemptInteract(UObject* context, Params::UFortControllerComponent_Interaction_ServerAttemptInteract* params);
    static bool PickLootDrops(UObject* context, Params::UFortKismetLibrary_PickLootDrops* params);
    static AFortPickup* K2_SpawnPickupInWorld(UObject* context, Params::UFortKismetLibrary_K2_SpawnPickupInWorld* params);
    static AFortPickup* SpawnItemVariantPickupInWorld(UObject* context, Params::UFortKismetLibrary_SpawnItemVariantPickupInWorld* params);
    static AFortPickup* SupplyDropSpawnPickup(UObject* context, Params::AFortAthenaSupplyDrop_SpawnPickup* params);
    static void Hook();
};
