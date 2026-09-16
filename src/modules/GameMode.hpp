#pragma once
#include "pch.h"
#include <vector>

class GameMode {
public:
    static inline std::vector<UFortAbilitySet*> AbilitySets;
    static inline uint8_t CurrentTeam = 3;
    static inline uint8_t PlayersOnCurTeam = 0;

    static UFortPlaylistAthena* GetPlaylist();
    static void SetPlaylist(AFortGameModeAthena* gameMode);
    static void ReadyToStartMatch(UObject* context, Params::AGameMode_ReadyToStartMatch* params);
    static APawn* SpawnDefaultPawnFor(AGameModeBase* gameMode, AController* newPlayer, AActor* startSpot);
    static void HandleStartingNewPlayer(UObject* context, Params::AGameModeBase_HandleStartingNewPlayer* params);
    static void OnAircraftEnteredDropZone(UObject* context, Params::AFortGameModeAthena_OnAircraftEnteredDropZone* params);
    static void OnAircraftExitedDropZone(UObject* context, Params::AFortGameModeAthena_OnAircraftExitedDropZone* params);
    static EEFortTeam PickTeam(AFortGameModeAthena* gameMode, uint8_t preferredTeam, AFortPlayerControllerAthena* controller);
    static void Hook();
};
