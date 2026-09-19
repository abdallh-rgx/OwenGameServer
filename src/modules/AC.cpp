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
        LOGI("[AC] checkUser '%s' -> '%s' (len=%d)", name.c_str(), response.c_str(), (int)response.size());
        if (!response.empty() && response != "Valid") {
            LOGI("[AC] REJECTING '%s'", name.c_str());
            if (controller && controller->NetConnection) {
                controller->ClientReturnToMainMenu(Utils::ToFString(L""));
            }
        }
    }).detach();
}
