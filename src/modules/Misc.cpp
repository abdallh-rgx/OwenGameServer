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

    struct FNameRaw {
        uint32_t ComparisonIndex;
        uint32_t Number;
    };

    struct FNetDriverDefLocal {
        FNameRaw DefName;
        FNameRaw DriverClassName;
    };
    static_assert(sizeof(FNetDriverDefLocal) == 16, "FNetDriverDef must be 16 bytes");
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

    UClass* ipNetDriverClass = (UClass*)Utils::FindObject(L"/Script/OnlineSubsystemUtils.IpNetDriver");
    MLOG("[Listen] J IpNetDriver class=%p", ipNetDriverClass);

    if (!ipNetDriverClass) {
        MLOG("[Listen] FAIL: IpNetDriver class not found");
        *pGIsClient = savedClient;
        *pGIsServer = savedServer;
        return false;
    }

    uint8_t* engBase = (uint8_t*)engine;
    FNetDriverDefLocal** pData = (FNetDriverDefLocal**)(engBase + 0xC40);
    int32_t* pNum = (int32_t*)(engBase + 0xC48);
    int32_t* pMax = (int32_t*)(engBase + 0xC4C);

    MLOG("[Listen] J1 Before: Data=%p Num=%d Max=%d", *pData, *pNum, *pMax);

    FName ipClassName = MakeFName(L"IpNetDriver");
    MLOG("[Listen] J2 FName IpNetDriver=0x%x", ipClassName.ComparisonIndex);

    FNetDriverDefLocal* savedData = *pData;
    int32_t savedNum = *pNum;
    int32_t savedMax = *pMax;

    FNetDriverDefLocal* localDefs = (FNetDriverDefLocal*)malloc(sizeof(FNetDriverDefLocal) * 4);
    if (!localDefs) {
        MLOG("[Listen] FAIL: malloc failed");
        *pGIsClient = savedClient;
        *pGIsServer = savedServer;
        return false;
    }

    memset(localDefs, 0, sizeof(FNetDriverDefLocal) * 4);
    localDefs[0].DefName.ComparisonIndex = (uint32_t)driverName.ComparisonIndex;
    localDefs[0].DefName.Number = 0;
    localDefs[0].DriverClassName.ComparisonIndex = (uint32_t)ipClassName.ComparisonIndex;
    localDefs[0].DriverClassName.Number = 0;

    *pData = localDefs;
    *pNum = 1;
    *pMax = 4;

    MLOG("[Listen] J3 Populated: Data=%p Num=%d Max=%d", *pData, *pNum, *pMax);

    using CreateND_t = void* (*)(void*, void*, FName);
    CreateND_t createND = (CreateND_t)(Sarah::ImageBase + Off::CreateNetDriver);
    MLOG("[Listen] J4 CreateNetDriver fn=%p", (void*)createND);

    void* netDriver = createND(engine, worldCtx, driverName);
    MLOG("[Listen] K netDriver=%p", netDriver);

    *pData = savedData;
    *pNum = savedNum;
    *pMax = savedMax;
    MLOG("[Listen] Restored engine->NetDriverDefinitions (Data=%p Num=%d Max=%d)",
         *pData, *pNum, *pMax);

    *pGIsClient = savedClient;
    *pGIsServer = savedServer;
    MLOG("[Listen] Restored GIsClient=%d GIsServer=%d", savedClient, savedServer);

    if (!netDriver) {
        MLOG("[Listen] FAIL: CreateNetDriver returned null");
        free(localDefs);
        return false;
    }

    {
        uint8_t* base = (uint8_t*)netDriver;
        uint64_t w = (uint64_t)world;
        uint64_t e = (uint64_t)engine;
        uint64_t c = (uint64_t)worldCtx;

        MLOG("[Listen] === Scanning netDriver memory (0x30..0x800) ===");

        for (int off = 0x30; off < 0x800; off += 8) {
            uint64_t val = 0;
            memcpy(&val, base + off, 8);

            if (val < 0x1000 || val > 0x7FFFFFFFFFFFULL) continue;

            const char* label = nullptr;
            if (val == w)      label = "WORLD";
            else if (val == e) label = "ENGINE";
            else if (val == c) label = "WORLDCTX";

            if (label) {
                MLOG("[Listen]   [%03x] = 0x%016llx  * %s",
                     off, (unsigned long long)val, label);
            }
        }

        MLOG("[Listen] === End scan ===");
    }

    *(uint32_t*)((uint8_t*)netDriver + 0x208) = (uint32_t)driverName.ComparisonIndex;
    *(uint32_t*)((uint8_t*)netDriver + 0x20C) = 0;
    uint32_t readBackName = *(uint32_t*)((uint8_t*)netDriver + 0x208);
    MLOG("[Listen] K2 NetDriverName write=0x%x readback=0x%x %s",
         (uint32_t)driverName.ComparisonIndex, readBackName,
         (readBackName == (uint32_t)driverName.ComparisonIndex) ? "OK" : "FAIL");

    FURLLocal url = {};
    url.Port = g_Port;
    url.Valid = 1;

    using InitListen_t = bool (*)(void*, void*, FURLLocal*, bool, FStringLocal*);
    InitListen_t initListen = (InitListen_t)(Sarah::ImageBase + Off::InitListen);
    MLOG("[Listen] M calling InitListen on port %d...", g_Port);

    bool listenOk = initListen(netDriver, world, &url, false, nullptr);
    MLOG("[Listen] N InitListen returned %d", (int)listenOk);

    if (!listenOk) {
        MLOG("[Listen] FAIL: InitListen returned false");

        {
            uint8_t* base = (uint8_t*)netDriver;
            uint64_t w = (uint64_t)world;
            MLOG("[Listen] === Post-fail scan ===");
            for (int off = 0x30; off < 0x800; off += 8) {
                uint64_t val = 0;
                memcpy(&val, base + off, 8);
                if (val == w) {
                    MLOG("[Listen]   candidate [%03x] = WORLD", off);
                }
            }
            MLOG("[Listen] === End post-fail scan ===");
        }

        free(localDefs);
        return false;
    }

    world->NetDriver = (UNetDriver*)netDriver;

    for (auto& collection : world->LevelCollections) {
        collection.NetDriver = (UNetDriver*)netDriver;
    }
    MLOG("[Listen] L collections set");

    MLOG("[Listen] === Server listening on port %d ===", g_Port);
    free(localDefs);
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
