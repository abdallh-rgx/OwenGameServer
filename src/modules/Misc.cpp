#include "pch.h"
#include "Misc.hpp"
#include "API.hpp"
#include "options.h"
#include "Utils.hpp"
#include "UObject.hpp"
#include "FName.hpp"

#include <cstdlib>
#include <cstdarg>

static FILE* g_miscLog = nullptr;

static void MLOG(const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    __android_log_print(ANDROID_LOG_INFO, "OwenGameServer", "%s", buf);

    if (!g_miscLog) {
        const char* paths[] = {
            "/storage/emulated/0/Android/data/com.epicgames.fortnite/files/OwenGameServer.txt",
            "/sdcard/Android/data/com.epicgames.fortnite/files/OwenGameServer.txt",
        };
        for (auto p : paths) {
            g_miscLog = fopen(p, "a");
            if (g_miscLog) break;
        }
    }
    if (g_miscLog) {
        fprintf(g_miscLog, "%s\n", buf);
        fflush(g_miscLog);
    }
}

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
    return 2;
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

        if (!hasAClientConnected && numConnections > 0 && clientConnections != nullptr) {
            hasAClientConnected = true;
            MLOG("[TickFlush] First client connected");
        }

        if (hasAClientConnected && numConnections == 0) {
            MLOG("[TickFlush] All clients disconnected, staying alive (Listen mode)");
        }
    }

    if (!PlayersToDestroyLocked && PlayersToDestroy.size() > 0) {
        for (size_t i = 0; i < PlayersToDestroy.size(); i++) {
            if (PlayersToDestroy[i]) {
                PlayersToDestroy[i]->K2_DestroyActor();
            }
        }
        PlayersToDestroy.clear();
    }

    if (TickFlushOG) TickFlushOG(driver, dt);
}

bool Misc::StartAircraftPhase(AFortGameModeAthena* gameMode, char a2) {
    bool ret = false;
    if (StartAircraftPhaseOG)
        ret = StartAircraftPhaseOG(gameMode, a2);

    if (!gameMode) return ret;

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

            {
                FVector_NetQuantize100 flightStart{};
                static_cast<FVector&>(flightStart) = loc;
                aircraft->FlightInfo.FlightStartLocation = flightStart;
            }
            aircraft->FlightInfo.FlightTime = 7.f;
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
    volatile uint8_t* pGIsClient = (volatile uint8_t*)(Sarah::ImageBase + Off::GIsClient);
    volatile uint8_t* pGIsServer = (volatile uint8_t*)(Sarah::ImageBase + Off::GIsServer);

    uint8_t savedClient = *pGIsClient;
    uint8_t savedServer = *pGIsServer;

    MLOG("[Listen] === Starting Listen ===");
    MLOG("[Listen] Saved GIsClient=%d GIsServer=%d", savedClient, savedServer);

    *pGIsClient = 0;
    *pGIsServer = 1;
    MLOG("[Listen] Temp set Dedicated (Client=0, Server=1)");

    UWorld* world = *(UWorld**)(Sarah::ImageBase + Off::GWorld);
    MLOG("[Listen] B world=%p", world);

    UEngine* engine = *(UEngine**)(Sarah::ImageBase + Off::GEngine);
    MLOG("[Listen] D engine=%p", engine);

    if (!world || !engine) {
        MLOG("[Listen] FAIL: world or engine null");
        *pGIsClient = savedClient;
        *pGIsServer = savedServer;
        return false;
    }

    if (!world->PersistentLevel) {
        MLOG("[Listen] FAIL: PersistentLevel is null");
        *pGIsClient = savedClient;
        *pGIsServer = savedServer;
        return false;
    }

    using GetWorldCtx_t = void* (*)(void*, void*);
    GetWorldCtx_t getWorldCtx = (GetWorldCtx_t)(Sarah::ImageBase + Off::GetWorldContext);
    void* worldCtx = getWorldCtx(engine, world);
    MLOG("[Listen] H worldCtx=%p", worldCtx);

    if (!worldCtx) {
        MLOG("[Listen] FAIL: worldCtx null");
        *pGIsClient = savedClient;
        *pGIsServer = savedServer;
        return false;
    }

    FName driverName = MakeFName(L"GameNetDriver");
    MLOG("[Listen] I FName=0x%x", driverName.ComparisonIndex);

    if (driverName.ComparisonIndex == 0) {
        MLOG("[Listen] FAIL: GameNetDriver name not found");
        *pGIsClient = savedClient;
        *pGIsServer = savedServer;
        return false;
    }

    using CreateND_t = void* (*)(void*, void*, FName);
    CreateND_t createND = (CreateND_t)(Sarah::ImageBase + Off::CreateNetDriver);
    MLOG("[Listen] J CreateNetDriver fn=%p", (void*)createND);

    void* netDriver = createND(engine, worldCtx, driverName);
    MLOG("[Listen] K netDriver=%p", netDriver);

    *pGIsClient = savedClient;
    *pGIsServer = savedServer;
    MLOG("[Listen] Restored GIsClient=%d GIsServer=%d", savedClient, savedServer);

    if (!netDriver) {
        MLOG("[Listen] FAIL: CreateNetDriver returned null");
        return false;
    }

    *(FName*)((uint8_t*)netDriver + 0x190) = driverName;
    *(void**)((uint8_t*)netDriver + 0x140) = world;

    for (auto& collection : world->LevelCollections) {
        collection.NetDriver = (UNetDriver*)netDriver;
    }
    MLOG("[Listen] L collections set");

    FURLLocal url = {};
    url.Port = g_Port;
    url.Valid = 1;

    using InitListen_t = bool (*)(void*, void*, FURLLocal*, bool, FStringLocal*);
    InitListen_t initListen = (InitListen_t)(Sarah::ImageBase + Off::InitListen);
    MLOG("[Listen] M InitListen fn=%p, port=%d", (void*)initListen, g_Port);

    bool listenOk = initListen(netDriver, world, &url, false, nullptr);
    MLOG("[Listen] N InitListen returned %d", (int)listenOk);

    if (!listenOk) {
        MLOG("[Listen] FAIL: InitListen returned false");
        return false;
    }

    world->NetDriver = (UNetDriver*)netDriver;

    MLOG("[Listen] === Server listening on port %d ===", g_Port);
    return true;
}

void Misc::SetDynamicFoundationEnabled(UObject* context, Params::ABuildingFoundation_SetDynamicFoundationEnabled* params) {
    auto foundation = (ABuildingFoundation*)context;
    if (!foundation) return;

    foundation->DynamicFoundationRepData.EnabledState = params->bEnabled
        ? EEDynamicFoundationEnabledState::Enabled
        : EEDynamicFoundationEnabledState::Disabled;
    foundation->OnRep_DynamicFoundationRepData();
    foundation->FoundationEnabledState = params->bEnabled
        ? EEDynamicFoundationEnabledState::Enabled
        : EEDynamicFoundationEnabledState::Disabled;
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
