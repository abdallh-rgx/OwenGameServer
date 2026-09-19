#pragma once
#include "pch.h"

class Creative {
public:
    static inline void (*TeleportPlayerToLinkedVolumeOG)(UObject*, FFrame&) = nullptr;
    static inline void (*ServerTeleportToPlaygroundLobbyIslandOG)(UObject*, FFrame&) = nullptr;
    static inline void (*MakeNewCreativePlotOG)(UObject*, FFrame&) = nullptr;
    static inline void (*UpdateCreativePlotNameOG)(UObject*, FFrame&) = nullptr;

    static void ServerTeleportToPlaygroundLobbyIslandHook(UObject*, FFrame&);
    static void TeleportPlayerToLinkedVolumeHook(UObject*, FFrame&);
    static void MakeNewCreativePlotHook(UObject*, FFrame&);
    static void UpdateCreativePlotNameHook(UObject*, FFrame&);

    static void ServerTeleportToPlaygroundLobbyIsland(AFortPlayerControllerAthena*);
    static void Hook();
};
