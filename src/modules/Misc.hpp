#pragma once
#include "pch.h"

class Misc {
public:
    static inline bool PlayersToDestroyLocked = false;
    static inline std::vector<AFortPlayerPawn*> PlayersToDestroy;

    static inline void (*TickFlushOG)(void*, float) = nullptr;
    static inline bool (*StartAircraftPhaseOG)(AFortGameModeAthena*, char) = nullptr;

    static bool Listen();
    static int GetNetMode(void* world);
    static void TickFlush(void* driver, float dt);
    static bool StartAircraftPhase(AFortGameModeAthena* gameMode, char a2);
    static void SetDynamicFoundationEnabled(UObject* context, Params::ABuildingFoundation_SetDynamicFoundationEnabled* params);
    static void SetDynamicFoundationTransform(UObject* context, Params::ABuildingFoundation_SetDynamicFoundationTransform* params);
    static void Hook();
};
