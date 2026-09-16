#pragma once
#include "pch.h"

class Tournaments {
public:
    static void Kill(AFortPlayerControllerAthena* controller);
    static void Placement(int32_t placement, int32_t points);
    static void PlacementForController(AFortPlayerControllerAthena* controller, int32_t placement);
};
