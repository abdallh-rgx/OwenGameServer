#pragma once
#include "pch.h"

class Creative {
public:
    static void ServerTeleportToPlaygroundLobbyIsland(AFortPlayerControllerAthena* controller);
    static void TeleportPlayerToLinkedVolume(UObject* context, Params::AFortAthenaCreativePortal_TeleportPlayerToLinkedVolume* params);
    static void MakeNewCreativePlot(UObject* context, Params::AFortPlayerControllerAthena_MakeNewCreativePlot* params);
    static void UpdateCreativePlotName(UObject* context, Params::AFortPlayerControllerAthena_UpdateCreativePlotName* params);
    static void Hook();
};
