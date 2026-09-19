#pragma once
#include "pch.h"

class Misc {
public:
    static inline bool PlayersToDestroyLocked = false;
    static inline std::vector<AFortPlayerPawn*> PlayersToDestroy;

    static inline void (*TickFlushOG)(void*, float) = nullptr;
    static inline bool (*StartAircraftPhaseOG)(AFortGameModeAthena*, char) = nullptr;
    static inline void (*SetDynamicFoundationEnabledOG)(UObject*, FFrame&) = nullptr;
    static inline void (*SetDynamicFoundationTransformOG)(UObject*, FFrame&) = nullptr;

    static bool Listen();
    static int GetNetMode(void* world);
    static void TickFlush(void* driver, float dt);
    static bool StartAircraftPhase(AFortGameModeAthena* gameMode, char a2);

    static void SetDynamicFoundationEnabledHook(UObject*, FFrame&);
    static void SetDynamicFoundationTransformHook(UObject*, FFrame&);

    static void Hook();
};
