#include "pch.h"
#include "Misc.hpp"
#include "API.hpp"
#include "options.h"
#include "Utils.hpp"
#include "UObject.hpp"
#include "FName.hpp"

#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <string>

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

    struct FName4 {
        uint32_t Index;
    };

    struct FNetDriverDefLocal {
        FName4 DefName;
        FName4 DriverClassName;
        FName4 DriverClassNameFallback;
        int32_t MaxChannelsOverride;
    };
    static_assert(sizeof(FNetDriverDefLocal) == 16, "FNetDriverDefinition must be 16 bytes");
}

int Misc::GetNetMode(void* world) {
    // High-frequency game-thread pump for deferred tasks: the engine calls
    // GetNetMode constantly on the GameThread, making it a reliable place to
    // drain the RunOnGameThread queue (world travel, RPCs, ...).
    Sarah::DrainGameThreadQueue();
    return 1;
}

void Misc::TickFlush(void* driver, float dt) {
    // GameThread pump: UNetDriver::TickFlush runs on the game thread every
    // net tick - drain deferred tasks before touching the driver.
    Sarah::DrainGameThreadQueue();

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
            MLOG("[TickFlush] All clients disconnected, staying alive");
        }
    }

    if (!PlayersToDestroyLocked && PlayersToDestroy.size() > 0) {
        for (size_t i = 0; i < PlayersToDestroy.size(); i++) {
            if (PlayersToDestroy[i]) PlayersToDestroy[i]->K2_DestroyActor();
        }
        PlayersToDestroy.clear();
    }

    if (TickFlushOG) TickFlushOG(driver, dt);
}

bool Misc::StartAircraftPhase(AFortGameModeAthena* gameMode, char a2) {
    bool ret = false;
    if (StartAircraftPhaseOG) ret = StartAircraftPhaseOG(gameMode, a2);
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
    MLOG("[Listen] === Starting Listen ===");
    UWorld* world = UWorld::GetWorld();
    UEngine* engine = UEngine::GetEngine();
    MLOG("[Listen] world=%p engine=%p", world, engine);
    if (!world || !engine) { MLOG("[Listen] FAIL: world or engine null"); return false; }
    if (!world->PersistentLevel) { MLOG("[Listen] FAIL: PersistentLevel null"); return false; }

    using GetWorldCtx_t = void* (*)(void*, void*);
    GetWorldCtx_t getWorldCtx = (GetWorldCtx_t)(Sarah::ImageBase + Off::GetWorldContext);
    void* worldCtx = getWorldCtx(engine, world);
    if (!worldCtx) { MLOG("[Listen] FAIL: worldCtx null"); return false; }

    uint8_t* engBase = (uint8_t*)engine;
    FNetDriverDefLocal** pData = (FNetDriverDefLocal**)(engBase + 0xC40);
    int32_t* pNum = (int32_t*)(engBase + 0xC48);
    int32_t* pMax = (int32_t*)(engBase + 0xC4C);
    MLOG("[Listen] NetDriverDefinitions: Data=%p Num=%d Max=%d", *pData, *pNum, *pMax);
    if (!*pData || *pNum <= 0) { MLOG("[Listen] FAIL: NetDriverDefinitions empty"); return false; }

    using CreateND_t = void* (*)(void*, void*, FName);
    CreateND_t createND = (CreateND_t)(Sarah::ImageBase + Off::CreateNetDriver);
    void* netDriver = nullptr;
    for (int i = 0; i < *pNum && i < 8; i++) {
        FNetDriverDefLocal& e = (*pData)[i];
        FName testName{};
        testName.ComparisonIndex = e.DefName.Index;
        void* nd = createND(engine, worldCtx, testName);
        MLOG("[Listen] try[%d] -> %p", i, nd);
        if (nd) { netDriver = nd; break; }
    }
    if (!netDriver) { MLOG("[Listen] FAIL: no NetDriver"); return false; }

    world->NetDriver = (UNetDriver*)netDriver;
    for (int i = 0; i < world->LevelCollections.Num(); i++) {
        world->LevelCollections[i].NetDriver = (UNetDriver*)netDriver;
    }

    static char16_t hostBuf[] = u"0.0.0.0";
    static char16_t protoBuf[] = u"unreal";
    static char16_t emptyBuf[] = u"";

    FURLLocal url = {};
    url.Protocol.Data = protoBuf; url.Protocol.Num = 6; url.Protocol.Max = 7;
    url.Host.Data = hostBuf; url.Host.Num = 7; url.Host.Max = 8;
    url.Port = g_Port; url.Valid = 1;
    url.Map.Data = emptyBuf; url.Map.Num = 0; url.Map.Max = 1;
    url.RedirectURL.Data = emptyBuf; url.RedirectURL.Num = 0; url.RedirectURL.Max = 1;
    url.Op.Data = emptyBuf; url.Op.Num = 0; url.Op.Max = 1;
    url.Portal.Data = emptyBuf; url.Portal.Num = 0; url.Portal.Max = 1;

    static char16_t errBuf[512] = {};
    FStringLocal errStr = {};
    errStr.Data = errBuf; errStr.Num = 0; errStr.Max = 512;

    using InitListen_t = bool (*)(void*, void*, FURLLocal*, bool, FStringLocal*);
    InitListen_t initListen = (InitListen_t)(Sarah::ImageBase + Off::InitListen);
    bool listenOk = initListen(netDriver, worldCtx, &url, false, &errStr);
    MLOG("[Listen] InitListen returned %d", (int)listenOk);
    if (!listenOk) { MLOG("[Listen] FAIL: InitListen false, errLen=%d", errStr.Num); return false; }
    MLOG("[Listen] === Server listening on port %d ===", g_Port);
    return true;
}

void Misc::SetDynamicFoundationEnabledHook(UObject* Context, FFrame& Stack) {
    STACK_SAVE(Stack, _saved);
    bool bEnabled = false;
    Stack.StepCompiledIn(&bEnabled);
    Stack.IncrementCode();

    auto foundation = (ABuildingFoundation*)Context;
    if (foundation) {
        foundation->DynamicFoundationRepData.EnabledState = bEnabled ? EEDynamicFoundationEnabledState::Enabled : EEDynamicFoundationEnabledState::Disabled;
        foundation->OnRep_DynamicFoundationRepData();
        foundation->FoundationEnabledState = bEnabled ? EEDynamicFoundationEnabledState::Enabled : EEDynamicFoundationEnabledState::Disabled;
    }

    CALL_OG_VOID(Stack, Context, SetDynamicFoundationEnabledOG, _saved);
}

void Misc::SetDynamicFoundationTransformHook(UObject* Context, FFrame& Stack) {
    STACK_SAVE(Stack, _saved);
    CoreUObject::FTransform NewTransform;
    Stack.StepCompiledIn(&NewTransform);
    Stack.IncrementCode();

    auto foundation = (ABuildingFoundation*)Context;
    if (foundation) {
        foundation->DynamicFoundationTransform = NewTransform;
        foundation->DynamicFoundationRepData.Rotation = QuatToRotator(NewTransform.Rotation);
        foundation->DynamicFoundationRepData.Translation = NewTransform.Translation;
        foundation->StreamingData.FoundationLocation = NewTransform.Translation;
        foundation->OnRep_DynamicFoundationRepData();
    }

    CALL_OG_VOID(Stack, Context, SetDynamicFoundationTransformOG, _saved);
}

void Misc::Hook() {
    Utils::ExecHook(L"/Script/FortniteGame.BuildingFoundation.SetDynamicFoundationEnabled", (void*)SetDynamicFoundationEnabledHook, SetDynamicFoundationEnabledOG);
    Utils::ExecHook(L"/Script/FortniteGame.BuildingFoundation.SetDynamicFoundationTransform", (void*)SetDynamicFoundationTransformHook, SetDynamicFoundationTransformOG);
}
