#pragma once
#include "pch.h"

class Building {
public:
    static inline void (*OnDamageServerOG)(ABuildingSMActor*, float, FGameplayTagContainer, FVector, FHitResult, AFortPlayerControllerAthena*, AActor*, FGameplayEffectContextHandle) = nullptr;

    static inline void (*ServerCreateBuildingActorOG)(UObject*, FFrame&) = nullptr;
    static inline void (*ServerBeginEditingBuildingActorOG)(UObject*, FFrame&) = nullptr;
    static inline void (*ServerEditBuildingActorOG)(UObject*, FFrame&) = nullptr;
    static inline void (*ServerEndEditingBuildingActorOG)(UObject*, FFrame&) = nullptr;
    static inline void (*ServerRepairBuildingActorOG)(UObject*, FFrame&) = nullptr;
    static inline void (*ServerSpawnDecoOG)(UObject*, FFrame&) = nullptr;
    static inline void (*ServerCreateBuildingAndSpawnDecoOG)(UObject*, FFrame&) = nullptr;

    static bool CanBePlacedByPlayer(UClass* BuildClass);
    static void ServerCreateBuildingActorHook(UObject*, FFrame&);
    static void ServerBeginEditingBuildingActorHook(UObject*, FFrame&);
    static void ServerEditBuildingActorHook(UObject*, FFrame&);
    static void ServerEndEditingBuildingActorHook(UObject*, FFrame&);
    static void ServerRepairBuildingActorHook(UObject*, FFrame&);
    static void ServerSpawnDecoHook(UObject*, FFrame&);
    static void ServerCreateBuildingAndSpawnDecoHook(UObject*, FFrame&);
    static void OnDamageServer(ABuildingSMActor*, float, FGameplayTagContainer, FVector, FHitResult, AFortPlayerControllerAthena*, AActor*, FGameplayEffectContextHandle);
    static void Hook();
};
