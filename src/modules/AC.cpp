#include "pch.h"
#include "AC.hpp"
#include "API.hpp"
#include "options.h"
#include "Utils.hpp"

void AC::CheckUser(AFortPlayerControllerAthena* controller) {
    if (bDev) return;
    if (!controller) return;

    AFortPlayerStateAthena* playerState = (AFortPlayerStateAthena*)controller->PlayerState;
    if (!playerState) return;

    std::string name = playerState->GetPlayerName().ToString();

    std::thread([controller, name]() {
        auto response = API::GetResponse(BackendUrl + "/api/v1/checkUser/" + name);
        if (response != "Valid") {
            if (controller) {
                controller->ClientReturnToMainMenu(Utils::ToFString(L""));
            }
        }
    }).detach();
}
