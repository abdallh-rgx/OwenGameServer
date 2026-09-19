#pragma once
#include "pch.h"

class Player {
public:
    static inline void (*ClientOnPawnDiedOG)(AFortPlayerControllerAthena*, FFortPlayerDeathReport&) = nullptr;

    static inline void (*ServerAcknowledgePossessionOG)(UObject*, FFrame&) = nullptr;
    static inline void (*ServerExecuteInventoryItemOG)(UObject*, FFrame&) = nullptr;
    static inline void (*ServerReturnToMainMenuOG)(UObject*, FFrame&) = nullptr;
    static inline void (*ServerAttemptAircraftJumpOG)(UObject*, FFrame&) = nullptr;
    static inline void (*ServerPlayEmoteItemOG)(UObject*, FFrame&) = nullptr;
    static inline void (*ServerSendZiplineStateOG)(UObject*, FFrame&) = nullptr;
    static inline void (*ServerHandlePickupInfoOG)(UObject*, FFrame&) = nullptr;
    static inline void (*MovingEmoteStoppedOG)(UObject*, FFrame&) = nullptr;
    static inline void (*ServerAttemptInventoryDropOG)(UObject*, FFrame&) = nullptr;
    static inline void (*ServerClientIsReadyToRespawnOG)(UObject*, FFrame&) = nullptr;
    static inline void (*ServerChangeNameOG)(UObject*, FFrame&) = nullptr;
    static inline void (*OnCapsuleBeginOverlapOG)(UObject*, FFrame&) = nullptr;
    static inline void (*TeleportPlayerPawnOG)(UObject*, FFrame&, bool*) = nullptr;

    static void ServerAcknowledgePossessionHook(UObject*, FFrame&);
    static void ServerExecuteInventoryItemHook(UObject*, FFrame&);
    static void ServerReturnToMainMenuHook(UObject*, FFrame&);
    static void ServerAttemptAircraftJumpHook(UObject*, FFrame&);
    static void ServerPlayEmoteItemHook(UObject*, FFrame&);
    static void ServerSendZiplineStateHook(UObject*, FFrame&);
    static void ServerHandlePickupInfoHook(UObject*, FFrame&);
    static void MovingEmoteStoppedHook(UObject*, FFrame&);
    static void ServerAttemptInventoryDropHook(UObject*, FFrame&);
    static void ServerClientIsReadyToRespawnHook(UObject*, FFrame&);
    static void ServerChangeNameHook(UObject*, FFrame&);
    static void OnCapsuleBeginOverlapHook(UObject*, FFrame&);
    static void TeleportPlayerPawnHook(UObject*, FFrame&, bool*);

    static void ClientOnPawnDied(AFortPlayerControllerAthena*, FFortPlayerDeathReport&);
    static void InternalPickup(AFortPlayerControllerAthena*, FFortItemEntry);

    static void Hook();
};
