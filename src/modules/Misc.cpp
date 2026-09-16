#include "pch.h"
#include "Misc.hpp"
#include "options.h"
#include "Utils.hpp"
#include "UObject.hpp"
#include "FName.hpp"

#include <cstdlib>

namespace {
    struct FStringLocal {
        char16_t* Data;
        int32_t Num;
        int32_t Max;
    };

    struct FURLLocal {
        FStringLocal Protocol;
        FStringLocal Host;
        int32_t Port;
        int32_t Valid;
        FStringLocal Map;
        FStringLocal RedirectURL;
        FStringLocal Op;
        FStringLocal Portal;
    };
}

int Misc::GetNetMode(void* world) {
    return 1;
}

void Misc::TickFlush(void* driver, float dt) {
    if (!driver) {
        if (TickFlushOG) TickFlushOG(driver, dt);
        return;
    }

    if (!bDev) {
        static bool hasAClientConnected = false;
        void** clientConnections = *(void***)((uint8_t*)driver + 0x90);
        int32_t numConnections = *(int32_t*)((uint8_t*)driver + 0x98);
        if (!hasAClientConnected && numConnections > 0) hasAClientConnected = true;
        else if (hasAClientConnected && numConnections == 0) {
            _exit(0);
        }
    }

    if (!PlayersToDestroyLocked && PlayersToDestroy.size() > 0) {
        for (size_t i = 0; i < PlayersToDestroy.size(); i++)
            PlayersToDestroy[i]->K2_DestroyActor();

        PlayersToDestroy.clear();
    }

    if (TickFlushOG) TickFlushOG(driver, dt);
}

bool Misc::StartAircraftPhase(AFortGameModeAthena* gameMode, char a2) {
    bool ret = false;
    if (StartAircraftPhaseOG)
        ret = StartAircraftPhaseOG(gameMode, a2);

    if (!bDev && bGameSessions) {
        API::GameServer(BackendUrl + "/solstice/api/v1/matchmaking/stop/by-address", IP, g_Port);
    }

    auto gameState = (AFortGameStateAthena*)gameMode->GameState;

    if (bLateGame && gameState) {
        gameState->GamePhase = EEAthenaGamePhase::SafeZones;
        gameState->GamePhaseStep = EEAthenaGamePhaseStep::StormHolding;
        gameState->OnRep_GamePhase(EEAthenaGamePhase::Aircraft);

        if (gameState->Aircrafts.Num() > 0 && gameState->Aircrafts[0]) {
            auto aircraft = gameState->Aircrafts[0];
            aircraft->FlightInfo.FlightSpeed = 0.f;

            FVector loc = gameMode->SafeZoneLocations.Num() > 3 ? gameMode->SafeZoneLocations[3] : FVector();
            loc.Z = 17500.f;

            aircraft->FlightInfo.FlightStartLocation = (FVector_NetQuantize100)loc;
            aircraft->FlightInfo.TimeTillFlightEnd = 7.f;
            aircraft->FlightInfo.TimeTillDropEnd = 0.f;
            aircraft->FlightInfo.TimeTillDropStart = 0.f;
            aircraft->FlightStartTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld());
            aircraft->FlightEndTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld()) + 7.f;
        }

        gameState->SafeZonesStartTime = UGameplayStatics::GetTimeSeconds(UWorld::GetWorld()) + 7.f;
    }

    return ret;
}

bool Misc::Listen() {
    UWorld* world = UWorld::GetWorld();
    UEngine* engine = UEngine::GetEngine();

    if (!engine || !world) {
        LOGE("[Listen] engine=%p world=%p", engine, world);
        return false;
    }

    using GetWorldCtx_t = void* (*)(void*, void*);
    GetWorldCtx_t getWorldCtx = (GetWorldCtx_t)(Sarah::ImageBase + Off::GetWorldContext);
    void* worldCtx = getWorldCtx(engine, world);
    if (!worldCtx) {
        LOGE("[Listen] worldCtx null");
        return false;
    }

    FName driverName = MakeFName(L"GameNetDriver");
    if (driverName.ComparisonIndex == 0) {
        LOGE("[Listen] FName 'GameNetDriver' not found");
        return false;
    }

    using CreateND_t = void* (*)(void*, void*, FName*);
    CreateND_t createND = (CreateND_t)(Sarah::ImageBase + Off::CreateNetDriver);
    void* netDriver = createND(engine, worldCtx, &driverName);
    if (!netDriver) {
        LOGE("[Listen] CreateNetDriver failed");
        return false;
    }

    *(FName*)((uint8_t*)netDriver + 0x190) = driverName;
    *(void**)((uint8_t*)netDriver + 0x140) = world;

    for (auto& collection : world->LevelCollections) {
        collection.NetDriver = (UNetDriver*)netDriver;
    }

    FURLLocal url = {};
    url.Port = g_Port;
    url.Valid = 1;

    using InitListen_t = bool (*)(void*, void*, FURLLocal*, bool, FStringLocal*);
    InitListen_t initListen = (InitListen_t)(Sarah::ImageBase + Off::InitListen);

    if (!initListen(netDriver, world, &url, false, nullptr)) {
        LOGE("[Listen] InitListen failed");
        return false;
    }

    world->NetDriver = (UNetDriver*)netDriver;

    LOGI("[Listen] Server listening on port %d", g_Port);
    return true;
}

void Misc::SetDynamicFoundationEnabled(UObject* context, Params::ABuildingFoundation_SetDynamicFoundationEnabled* params) {
    auto foundation = (ABuildingFoundation*)context;
    if (!foundation) return;

    foundation->DynamicFoundationRepData.EnabledState = params->bEnabled ? EEDynamicFoundationEnabledState::Enabled : EEDynamicFoundationEnabledState::Disabled;
    foundation->OnRep_DynamicFoundationRepData();
    foundation->FoundationEnabledState = params->bEnabled ? EEDynamicFoundationEnabledState::Enabled : EEDynamicFoundationEnabledState::Disabled;
}

void Misc::SetDynamicFoundationTransform(UObject* context, Params::ABuildingFoundation_SetDynamicFoundationTransform* params) {
    auto foundation = (ABuildingFoundation*)context;
    if (!foundation) return;

    foundation->DynamicFoundationTransform = params->NewTransform;
    foundation->DynamicFoundationRepData.Rotation = QuatToRotator(params->NewTransform.Rotation);
    foundation->DynamicFoundationRepData.Translation = params->NewTransform.Translation;
    foundation->StreamingData.FoundationLocation = params->NewTransform.Translation;
    foundation->OnRep_DynamicFoundationRepData();
}

void Misc::Hook() {
}
