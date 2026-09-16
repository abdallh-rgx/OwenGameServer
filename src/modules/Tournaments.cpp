#include "pch.h"
#include "Tournaments.hpp"
#include "Results.hpp"
#include "options.h"

void Tournaments::Kill(AFortPlayerControllerAthena* controller) {
    if (bDev || !controller) return;
    AFortPlayerStateAthena* playerState = (AFortPlayerStateAthena*)controller->PlayerState;
    if (!playerState) return;
    if (playerState->IsPlayerDead()) return;

    auto name = playerState->GetPlayerName().ToString();
    Results::SendBattleRoyaleResult(name, 0, 1, 0, StartingCount);
}

void Tournaments::Placement(int32_t placement, int32_t points) {
    if (bDev) return;
    AFortGameModeAthena* gameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
    if (!gameMode) return;

    auto& alivePlayers = gameMode->AlivePlayers;

    for (int i = 0; i < alivePlayers.Num(); i++) {
        auto* ctrl = alivePlayers[i];
        if (!ctrl) continue;
        ctrl->ClientReportTournamentPlacementPointsScored(placement, points);
        AFortPlayerStateAthena* ps = (AFortPlayerStateAthena*)ctrl->PlayerState;
        if (!ps) continue;
        auto name = ps->GetPlayerName().ToString();
        if (!bTournament) {
            Results::SendBattleRoyaleResult(name, placement, 0, 0, StartingCount);
        }
    }
}

void Tournaments::PlacementForController(AFortPlayerControllerAthena* controller, int32_t placement) {
    if (bDev || !controller) return;
    AFortPlayerStateAthena* ps = (AFortPlayerStateAthena*)controller->PlayerState;
    if (!ps) return;
    auto name = ps->GetPlayerName().ToString();
    Results::SendBattleRoyaleResult(name, placement, 0, 0, StartingCount);
}
