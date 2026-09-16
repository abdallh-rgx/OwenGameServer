#pragma once
#include "pch.h"

class Player {
public:
    static inline void (*ClientOnPawnDiedOG)(AFortPlayerControllerAthena*, FFortPlayerDeathReport&) = nullptr;

    static void ServerAcknowledgePossession(UObject* context, Params::APlayerController_ServerAcknowledgePossession* params);
    static void ServerExecuteInventoryItem(UObject* context, Params::AFortPlayerController_ServerExecuteInventoryItem* params);
    static void ServerReturnToMainMenu(UObject* context);
    static void ServerAttemptAircraftJump(UObject* context, Params::UFortControllerComponent_Aircraft_ServerAttemptAircraftJump* params);
    static void ServerPlayEmoteItem(UObject* context, Params::AFortPlayerController_ServerPlayEmoteItem* params);
    static void ServerSendZiplineState(UObject* context, Params::AFortPlayerPawn_ServerSendZiplineState* params);
    static void ServerHandlePickupInfo(UObject* context, Params::AFortPlayerPawn_ServerHandlePickupInfo* params);
    static void MovingEmoteStopped(UObject* context);
    static void ServerAttemptInventoryDrop(UObject* context, Params::AFortPlayerController_ServerAttemptInventoryDrop* params);
    static void ServerClientIsReadyToRespawn(UObject* context);
    static void ServerChangeName(UObject* context, Params::APlayerController_ServerChangeName* params);
    static void OnCapsuleBeginOverlap(UObject* context, Params::AFortPlayerPawn_OnCapsuleBeginOverlap* params);
    static void TeleportPlayerPawn(UObject* context, Params::UFortMissionLibrary_TeleportPlayerPawn* params);
    static void ClientOnPawnDied(AFortPlayerControllerAthena* playerController, FFortPlayerDeathReport& deathReport);
    static void InternalPickup(AFortPlayerControllerAthena* pc, FFortItemEntry pickupEntry);
    static void Hook();
};
