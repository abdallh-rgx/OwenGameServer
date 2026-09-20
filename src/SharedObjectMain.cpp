#include "pch.h"
#include "options.h"
#include "Utils.hpp"
#include "UObject.hpp"
#include "FName.hpp"
#include "Misc.hpp"
#include "GameMode.hpp"
#include "Player.hpp"
#include "Building.hpp"
#include "Looting.hpp"
#include "Creative.hpp"
#include "Abilities.hpp"
#include "CrashForensics.hpp"
#include "RuntimeConfig.hpp"

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <mutex>
#include <unistd.h>

static FILE* g_logfile = nullptr;
static std::mutex g_log_mutex;
static char   g_logPath[512] = {0};
static char   g_logDir[480] = {0};

// MobileDumper-7 FName bridge (src/core/mobile_dumper_bridge.cpp + libMobileDumperCore.a)
extern "C" int MD7_Setup(uintptr_t imageBase);

static void InitLogFile() {
    const char* paths[] = {
        "/storage/emulated/0/Android/data/com.epicgames.fortnite/files/OwenGameServer.txt",
        "/sdcard/Android/data/com.epicgames.fortnite/files/OwenGameServer.txt",
        "/data/data/com.epicgames.fortnite/files/OwenGameServer.txt",
        "/data/local/tmp/OwenGameServer.txt"
    };
    for (auto path : paths) {
        FILE* f = fopen(path, "w");
        if (f) {
            g_logfile = f;
            setvbuf(f, nullptr, _IONBF, 0);
            strncpy(g_logPath, path, sizeof(g_logPath) - 1);
            // remember the directory for the .cfg lookup
            const char* slash = strrchr(path, '/');
            if (slash) {
                size_t n = (size_t)(slash - path);
                if (n >= sizeof(g_logDir)) n = sizeof(g_logDir) - 1;
                memcpy(g_logDir, path, n);
                g_logDir[n] = 0;
            }
            return;
        }
    }
}

static void LOGF(const char* fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    LOGI("%s", buf);
    if (g_logfile) {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        fprintf(g_logfile, "%s\n", buf);
        fflush(g_logfile);
        fsync(fileno(g_logfile));
    }
}

static int (*GetNetModeOG)(void*) = nullptr;
static int GetNetModeHook(void* world) {
    // High-frequency game-thread pump for deferred tasks: the engine calls
    // GetNetMode constantly on the GameThread, making it a reliable place to
    // drain the RunOnGameThread queue (world travel, RPCs, ...).
    Sarah::DrainGameThreadQueue();
    // honest_netmode: report the engine's real answer instead of the
    // unconditional NM_DedicatedServer lie (bisect option for the frontend
    // crash). After map travel GIsClient=0 already makes the true value
    // NM_DedicatedServer for the server world.
    if (OwenCfg.honest_netmode && GetNetModeOG) {
        return GetNetModeOG(world);
    }
    return 1;
}

static EEFortTeam (*PickTeamOG)(AFortGameModeAthena*, uint8_t, AFortPlayerControllerAthena*) = nullptr;
static EEFortTeam PickTeamHook(AFortGameModeAthena* gameMode, uint8_t preferredTeam, AFortPlayerControllerAthena* controller) {
    return GameMode::PickTeam(gameMode, preferredTeam, controller);
}

static APawn* (*SpawnDefaultPawnForOG)(AGameModeBase*, AController*, AActor*) = nullptr;
static APawn* SpawnDefaultPawnForHook(AGameModeBase* gameMode, AController* newPlayer, AActor* startSpot) {
    APawn* result = GameMode::SpawnDefaultPawnFor(gameMode, newPlayer, startSpot);
    if (result) return result;
    if (SpawnDefaultPawnForOG) return SpawnDefaultPawnForOG(gameMode, newPlayer, startSpot);
    return nullptr;
}

static void WaitForWorld() {
    LOGF("[CORE] Waiting for World");
    for (int i = 0; i < 300; i++) {
        void* rawWorld = *(void**)(Sarah::ImageBase + Off::GWorld);
        void* rawEngine = *(void**)(Sarah::ImageBase + Off::GEngine);
        if ((i % 20) == 0) {
            LOGF("[CORE] iter %d (~%ds): rawWorld=%p rawEngine=%p", i, i / 2, rawWorld, rawEngine);
        }
        if (rawWorld && rawEngine) {
            LOGF("[CORE] World and Engine ready at iter %d (~%ds)", i, i / 2);
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    LOGF("[CORE] World wait timeout after 150s");
}

// Sleep with a 1-second heartbeat: the log then shows exactly WHEN a crash
// happened relative to our own timeline (the old single 15s/60s sleeps left
// the crash moment anywhere inside a blind window).
static void HeartbeatSleep(int seconds, const char* tag) {
    for (int t = 1; t <= seconds; t++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!Sarah::ImageBase) continue;
        void* world = *(void**)(Sarah::ImageBase + Off::GWorld);
        LOGF("[HB] %s t=%d/%ds world=%p GIsClient=%d GIsServer=%d",
             tag, t, seconds, world, GetGIsClient(), GetGIsServer());
    }
}

static bool ExecuteOpenCommand(const wchar_t* cmd) {
    UWorld* world = UWorld::GetWorld();
    if (!world) { LOGF("[MAP] FAIL: world is null"); return false; }

    static char16_t cmdBuf[512];
    int len = 0;
    for (const wchar_t* p = cmd; *p && len < 500; p++) cmdBuf[len++] = (char16_t)(*p);
    cmdBuf[len] = 0;

    struct FStringLocal { char16_t* Data; int32_t Num; int32_t Max; };
    struct ExecCmdParams { UObject* WorldContextObject; FStringLocal Command; APlayerController* SpecificPlayer; };

    UFunction* execFn = (UFunction*)Utils::FindObject(L"/Script/Engine.KismetSystemLibrary.ExecuteConsoleCommand");
    UClass* kslClass = (UClass*)Utils::FindObject(L"/Script/Engine.KismetSystemLibrary");
    if (!execFn || !kslClass || !kslClass->ClassDefaultObject) {
        LOGF("[MAP] FAIL: ExecuteConsoleCommand not available");
        return false;
    }

    ExecCmdParams parms = {};
    parms.WorldContextObject = world;
    parms.Command.Data = cmdBuf;
    parms.Command.Num = len;
    parms.Command.Max = len + 1;
    parms.SpecificPlayer = nullptr;

    Sarah::CallProcessEvent(kslClass->ClassDefaultObject, execFn, &parms);
    return true;
}

static void InstallExecHooks() {
    LOGF("[HOOKS] Installing ExecFunction hooks (no ProcessEvent)");
    GameMode::Hook();
    LOGF("[HOOKS]   GameMode::Hook DONE");
    Player::Hook();
    LOGF("[HOOKS]   Player::Hook DONE");
    Building::Hook();
    LOGF("[HOOKS]   Building::Hook DONE");
    Looting::Hook();
    LOGF("[HOOKS]   Looting::Hook DONE");
    Creative::Hook();
    LOGF("[HOOKS]   Creative::Hook DONE");
    Misc::Hook();
    LOGF("[HOOKS]   Misc::Hook DONE");
    LOGF("[HOOKS] All ExecFunction hooks installed");
}

static void InstallNativeHooks() {
    LOGF("[HOOKS] Installing native hooks");

    if (OwenCfg.no_getnetmode_hook) {
        LOGF("[HOOKS]   GetNetMode SKIPPED (cfg no_getnetmode_hook)");
    } else {
        DobbyHook((void*)(Sarah::ImageBase + Off::GetNetMode), (void*)GetNetModeHook, (void**)&GetNetModeOG);
        LOGF("[HOOKS]   GetNetMode OK%s", OwenCfg.honest_netmode ? " (honest_netmode: returns the TRUE value)" : "");
    }

    if (OwenCfg.no_tickflush_hook) {
        LOGF("[HOOKS]   TickFlush SKIPPED (cfg no_tickflush_hook)");
    } else {
        DobbyHook((void*)(Sarah::ImageBase + Off::TickFlush), (void*)Misc::TickFlush, (void**)&Misc::TickFlushOG);
        LOGF("[HOOKS]   TickFlush OK");
    }

    DobbyHook((void*)(Sarah::ImageBase + Off::ClientOnPawnDied), (void*)Player::ClientOnPawnDied, (void**)&Player::ClientOnPawnDiedOG);
    LOGF("[HOOKS]   ClientOnPawnDied OK");

    DobbyHook((void*)(Sarah::ImageBase + Off::BuildingActor_OnDamageServer), (void*)Building::OnDamageServer, (void**)&Building::OnDamageServerOG);
    LOGF("[HOOKS]   BuildingActor_OnDamageServer OK");

    DobbyHook((void*)(Sarah::ImageBase + Off::PickTeam), (void*)PickTeamHook, (void**)&PickTeamOG);
    LOGF("[HOOKS]   PickTeam OK");

    DobbyHook((void*)(Sarah::ImageBase + Off::StartAircraftPhase), (void*)Misc::StartAircraftPhase, (void**)&Misc::StartAircraftPhaseOG);
    LOGF("[HOOKS]   StartAircraftPhase OK");

    DobbyHook((void*)(Sarah::ImageBase + Off::SpawnDefaultPawnFor), (void*)SpawnDefaultPawnForHook, (void**)&SpawnDefaultPawnForOG);
    LOGF("[HOOKS]   SpawnDefaultPawnFor OK");

    LOGF("[HOOKS] All native hooks installed");
}

static void MainThread() {
    LOGF("[MAIN] MainThread ENTER");

    std::this_thread::sleep_for(std::chrono::seconds(5));
    LOGF("[MAIN] after initial sleep");

    LOGF("[CFG] safe_mode=%d no_setclientoffonly=%d late_flip=%d no_getnetmode_hook=%d "
         "honest_netmode=%d no_tickflush_hook=%d no_native_hooks=%d no_exec_hooks=%d "
         "no_map_travel=%d no_fatal_api_hooks=%d",
         OwenCfg.safe_mode, OwenCfg.no_setclientoffonly, OwenCfg.late_flip,
         OwenCfg.no_getnetmode_hook, OwenCfg.honest_netmode, OwenCfg.no_tickflush_hook,
         OwenCfg.no_native_hooks, OwenCfg.no_exec_hooks, OwenCfg.no_map_travel,
         OwenCfg.no_fatal_api_hooks);

    if (!InitImageBase()) { LOGF("[MAIN] InitImageBase FAILED"); return; }
    LOGF("[MAIN] ImageBase=0x%lx", Sarah::ImageBase);

    // MobileDumper-7 FName bridge: must be initialized before any name reads
    // (InSDKUtils::GetNameByIndex and everything built on it). MD7_ReadFName
    // also self-bootstraps, but setting up here guarantees correct behaviour
    // from the very first lookup.
    {
        const int md7rc = MD7_Setup(Sarah::ImageBase);
        LOGF("[MAIN] MD7_Setup -> %d (MobileDumper-7 FName bridge)", md7rc);
    }

    if (OwenCfg.safe_mode) {
        LOGF("[CFG] SAFE MODE: pure observation. No GIsClient flip, no hooks, no map travel.");
        LOGF("[CFG] If the game still crashes now, the cause is NOT this library's active code.");
        int t = 0;
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            t += 5;
            void* world = *(void**)(Sarah::ImageBase + Off::GWorld);
            LOGF("[HB][SAFE] t=%ds alive world=%p GIsClient=%d GIsServer=%d",
                 t, world, GetGIsClient(), GetGIsServer());
        }
    }

    WaitForWorld();
    LOGF("[MAIN] WaitForWorld returned");

    // The engine is fully initialized now (its own crash handlers are in
    // place): re-arm the forensics so the module snapshot includes libUnreal
    // and the chain runs engine -> us, and hook-site correlation works.
    Sarah::Forensics::InstallSignalHandlers();
    LOGF("[FORENSICS] re-armed after engine init");

    if (!Sarah::InitGObjectsLayout()) { LOGF("[MAIN] GObjects FAILED"); return; }
    LOGF("[MAIN] GObjects Num = %d", Sarah::UObjectManager::Num());

    if (OwenCfg.no_setclientoffonly) {
        LOGF("[CFG] SetClientOffOnly SKIPPED (cfg no_setclientoffonly)");
    } else if (OwenCfg.late_flip) {
        LOGF("[CFG] SetClientOffOnly DEFERRED to map travel (cfg late_flip)");
    } else {
        LOGF("[MAIN] Before SetClientOffOnly: GIsEditor=%d GIsClient=%d GIsServer=%d",
             GetGIsEditor(), GetGIsClient(), GetGIsServer());

        SetClientOffOnly();

        LOGF("[MAIN] After SetClientOffOnly: GIsEditor=%d GIsClient=%d GIsServer=%d",
             GetGIsEditor(), GetGIsClient(), GetGIsServer());
    }

    srand((uint32_t)time(nullptr));

    if (OwenCfg.no_native_hooks) {
        LOGF("[CFG] native hooks SKIPPED (cfg no_native_hooks)");
    } else {
        InstallNativeHooks();
    }

    if (OwenCfg.no_exec_hooks) {
        LOGF("[CFG] ExecFunction hooks SKIPPED (cfg no_exec_hooks)");
    } else {
        InstallExecHooks();
    }

    if (bGameSessions) {
        PatchBytes<uint8_t>(Off::GameSessionPatch, 0x85);
        LOGF("[PATCH] GameSession patch applied");
    }

    LOGF("[MAP] Waiting 15s for Frontend to settle");
    HeartbeatSleep(15, "settle");

    if (OwenCfg.no_map_travel) {
        LOGF("[CFG] map travel SKIPPED (cfg no_map_travel) - observing in frontend");
        int t = 0;
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            t += 5;
            void* world = *(void**)(Sarah::ImageBase + Off::GWorld);
            LOGF("[HB][NO-TRAVEL] t=%ds alive world=%p GIsClient=%d GIsServer=%d",
                 t, world, GetGIsClient(), GetGIsServer());
        }
    }

    LOGF("[MAP] Requesting map travel");
    const wchar_t* cmd = bCreative ? L"open Creative_NoApollo_Terrain" : L"open Artemis_Terrain";

    // UE is not thread-safe: world travel (ExecuteConsoleCommand "open ...")
    // MUST run on the game thread. MainThread is a background std::thread, so
    // the command is deferred through the GameThread scheduler and executed
    // by one of the installed hook pumps (GetNetMode / TickFlush). Calling it
    // directly from here corrupted engine state and crashed the game with a
    // SIGSEGV (SI_TKILL) deep inside libUnreal.so on the GameThread.
    Sarah::RunOnGameThread([cmd]() {
        if (OwenCfg.late_flip) {
            // Flip only now, on the game thread, right before travel: the
            // frontend finished loading as an honest client.
            LOGF("[CFG] late_flip: applying SetClientOffOnly on GameThread now");
            SetClientOffOnly();
            LOGF("[CFG] late_flip: GIsClient=%d GIsServer=%d", GetGIsClient(), GetGIsServer());
        }
        if (ExecuteOpenCommand(cmd)) {
            LOGF("[MAP] Map travel requested");
        } else {
            LOGF("[MAP] Map travel FAILED");
        }
    });

    LOGF("[MAP] Waiting 60s for map to load");
    HeartbeatSleep(60, "mapload");

    LOGF("[MAIN] MainThread DONE");
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    InitLogFile();
    LOGF("========================================");
    LOGF("OwenGameServer loading...");
    LOGF("PID: %d", getpid());
    LOGF("========================================");

    // Runtime config (bisect support) + crash forensics: armed as early as
    // possible so even crashes during frontend load are captured.
    LoadOwenConfig(g_logDir);
    Sarah::Forensics::SetLogPath(g_logPath);
    Sarah::Forensics::InstallSignalHandlers();
    if (!OwenCfg.no_fatal_api_hooks && !OwenCfg.safe_mode) {
        Sarah::Forensics::InstallFatalAPIHooks();
    } else {
        LOGI("[FORENSICS] fatal-API hooks skipped (cfg)");
    }

    std::thread(MainThread).detach();
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    LOGF("OwenGameServer unloading");
    if (g_logfile) {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        fflush(g_logfile);
        fclose(g_logfile);
        g_logfile = nullptr;
    }
}
