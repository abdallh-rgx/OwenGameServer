#pragma once
#include "pch.h"
#include <string>
#include <vector>

class Results {
public:
    static void SendBattleRoyaleResult(const std::string& username, int placement, int elims, int xp, int startingCount);
    static void SendEventMatchResults(const std::string& username, const std::string& eventId,
        const std::vector<std::string>& teammateIds, const std::vector<std::string>& matchHistory,
        const std::string& sessionId, int placement, int elims, int timeAlive);
};
