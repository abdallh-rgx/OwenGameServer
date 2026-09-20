#pragma once
#include "pch.h"

class Building {
public:
    static inline void (*BuildingActor_OnDamageServerOG)(ABuildingSMActor*, float, FGameplayTagContainer, FVector, FHitResult, AFortPlayerControllerAthena*, AActor*, FGameplayEffectContextHandle) = nullptr;

    static bool CanBePlacedByPlayer(UClass* buildClass);
    static void ServerCreateBuildingActor(UObject* context, Params::AFortPlayerController_ServerCreateBuildingActor* params);
    static void ServerBeginEditingBuildingActor(UObject* context, Params::AFortPlayerController_ServerBeginEditingBuildingActor* params);
    static void ServerEditBuildingActor(UObject* context, Params::AFortPlayerController_ServerEditBuildingActor* params);
    static void ServerEndEditingBuildingActor(UObject* context, Params::AFortPlayerController_ServerEndEditingBuildingActor* params);
    static void ServerRepairBuildingActor(UObject* context, Params::AFortPlayerController_ServerRepairBuildingActor* params);
    static void ServerSpawnDeco(UObject* context, Params::AFortDecoTool_ServerSpawnDeco* params);
    static void ServerCreateBuildingAndSpawnDeco(UObject* context, Params::AFortDecoTool_ServerCreateBuildingAndSpawnDeco* params);
    static void OnDamageServer(ABuildingSMActor* actor, float damage, FGameplayTagContainer damageTags, FVector momentum, FHitResult hitInfo, AFortPlayerControllerAthena* instigatedBy, AActor* damageCauser, FGameplayEffectContextHandle effectContext);
    static void Hook();
};
