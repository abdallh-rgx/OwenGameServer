#pragma once
#include "pch.h"
#include <vector>

class GameMode {
public:
    static inline std::vector<UFortAbilitySet*> AbilitySets;
    static inline uint8_t CurrentTeam = 3;
    static inline uint8_t PlayersOnCurTeam = 0;

    static inline void (*ReadyToStartMatchOG)(UObject*, FFrame&, bool*) = nullptr;
    static inline void (*HandleStartingNewPlayerOG)(UObject*, FFrame&) = nullptr;
    static inline void (*OnAircraftEnteredDropZoneOG)(UObject*, FFrame&) = nullptr;
    static inline void (*OnAircraftExitedDropZoneOG)(UObject*, FFrame&) = nullptr;

    static UFortPlaylistAthena* GetPlaylist();
    static void SetPlaylist(AFortGameModeAthena* gameMode);

    static void ReadyToStartMatchHook(UObject* Context, FFrame& Stack, bool* Ret);
    static void HandleStartingNewPlayerHook(UObject* Context, FFrame& Stack);
    static void OnAircraftEnteredDropZoneHook(UObject* Context, FFrame& Stack);
    static void OnAircraftExitedDropZoneHook(UObject* Context, FFrame& Stack);

    static APawn* SpawnDefaultPawnFor(AGameModeBase* gameMode, AController* newPlayer, AActor* startSpot);
    static EEFortTeam PickTeam(AFortGameModeAthena* gameMode, uint8_t preferredTeam, AFortPlayerControllerAthena* controller);

    static void Hook();
};
