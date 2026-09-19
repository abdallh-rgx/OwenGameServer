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

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <mutex>
#include <unistd.h>

static FILE* g_logfile = nullptr;
static std::mutex g_log_mutex;

static void InitLogFile() {
    const char* paths[] = {
        "/storage/emulated/0/Android/data/com.epicgames.fortnite2130GameServer/files/OwenGameServer.txt",
        "/sdcard/Android/data/com.epicgames.fortnite2130GameServer/files/OwenGameServer.txt",
        "/data/data/com.epicgames.fortnite2130GameServer/files/OwenGameServer.txt",
        "/data/local/tmp/OwenGameServer.txt"
    };
    for (auto path : paths) {
        FILE* f = fopen(path, "w");
        if (f) {
            g_logfile = f;
            setvbuf(f, nullptr, _IONBF, 0);
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
static int GetNetModeHook(void* world) { return 1; }

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

static bool ExecuteOpenCommand(const wchar_t* cmd) {
    UWorld* world = UWorld::GetWorld();
    if (!world) { LOGF("[MAP] FAIL: world is null"); return false; }

    static char16_t cmdBuf[512];
    int len = 0;
    for (const wchar_t* p = cmd; *p && len < 500; p++) {
        cmdBuf[len++] = (char16_t)(*p);
    }
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

static void InstallHooks_NoProcessEvent() {
    LOGF("[HOOKS] Installing ExecFunction hooks (no ProcessEvent)");

    Utils::ExecHook(L"/Script/Engine.GameMode.ReadyToStartMatch",
                    (void*)GameMode::ReadyToStartMatchHook, GameMode::ReadyToStartMatchOG);
    LOGF("[HOOKS]   ReadyToStartMatch OK");

    Utils::ExecHook(L"/Script/Engine.GameModeBase.HandleStartingNewPlayer",
                    (void*)GameMode::HandleStartingNewPlayerHook, GameMode::HandleStartingNewPlayerOG);
    LOGF("[HOOKS]   HandleStartingNewPlayer OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortGameModeAthena.OnAircraftEnteredDropZone",
                    (void*)GameMode::OnAircraftEnteredDropZoneHook, GameMode::OnAircraftEnteredDropZoneOG);
    LOGF("[HOOKS]   OnAircraftEnteredDropZone OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortGameModeAthena.OnAircraftExitedDropZone",
                    (void*)GameMode::OnAircraftExitedDropZoneHook, GameMode::OnAircraftExitedDropZoneOG);
    LOGF("[HOOKS]   OnAircraftExitedDropZone OK");

    Utils::ExecHook(L"/Script/Engine.PlayerController.ServerAcknowledgePossession",
                    (void*)Player::ServerAcknowledgePossessionHook, Player::ServerAcknowledgePossessionOG);
    LOGF("[HOOKS]   ServerAcknowledgePossession OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerController.ServerExecuteInventoryItem",
                    (void*)Player::ServerExecuteInventoryItemHook, Player::ServerExecuteInventoryItemOG);
    LOGF("[HOOKS]   ServerExecuteInventoryItem OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerController.ServerReturnToMainMenu",
                    (void*)Player::ServerReturnToMainMenuHook, Player::ServerReturnToMainMenuOG);
    LOGF("[HOOKS]   ServerReturnToMainMenu OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortControllerComponent_Aircraft.ServerAttemptAircraftJump",
                    (void*)Player::ServerAttemptAircraftJumpHook, Player::ServerAttemptAircraftJumpOG);
    LOGF("[HOOKS]   ServerAttemptAircraftJump OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerController.ServerPlayEmoteItem",
                    (void*)Player::ServerPlayEmoteItemHook, Player::ServerPlayEmoteItemOG);
    LOGF("[HOOKS]   ServerPlayEmoteItem OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerPawn.ServerSendZiplineState",
                    (void*)Player::ServerSendZiplineStateHook, Player::ServerSendZiplineStateOG);
    LOGF("[HOOKS]   ServerSendZiplineState OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerPawn.ServerHandlePickupInfo",
                    (void*)Player::ServerHandlePickupInfoHook, Player::ServerHandlePickupInfoOG);
    LOGF("[HOOKS]   ServerHandlePickupInfo OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortPawn.MovingEmoteStopped",
                    (void*)Player::MovingEmoteStoppedHook, Player::MovingEmoteStoppedOG);
    LOGF("[HOOKS]   MovingEmoteStopped OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerController.ServerAttemptInventoryDrop",
                    (void*)Player::ServerAttemptInventoryDropHook, Player::ServerAttemptInventoryDropOG);
    LOGF("[HOOKS]   ServerAttemptInventoryDrop OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerControllerAthena.ServerClientIsReadyToRespawn",
                    (void*)Player::ServerClientIsReadyToRespawnHook, Player::ServerClientIsReadyToRespawnOG);
    LOGF("[HOOKS]   ServerClientIsReadyToRespawn OK");

    Utils::ExecHook(L"/Script/Engine.PlayerController.ServerChangeName",
                    (void*)Player::ServerChangeNameHook, Player::ServerChangeNameOG);
    LOGF("[HOOKS]   ServerChangeName OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerPawn.OnCapsuleBeginOverlap",
                    (void*)Player::OnCapsuleBeginOverlapHook, Player::OnCapsuleBeginOverlapOG);
    LOGF("[HOOKS]   OnCapsuleBeginOverlap OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortMissionLibrary.TeleportPlayerPawn",
                    (void*)Player::TeleportPlayerPawnHook, Player::TeleportPlayerPawnOG);
    LOGF("[HOOKS]   TeleportPlayerPawn OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerController.ServerCreateBuildingActor",
                    (void*)Building::ServerCreateBuildingActorHook, Building::ServerCreateBuildingActorOG);
    LOGF("[HOOKS]   ServerCreateBuildingActor OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerController.ServerBeginEditingBuildingActor",
                    (void*)Building::ServerBeginEditingBuildingActorHook, Building::ServerBeginEditingBuildingActorOG);
    LOGF("[HOOKS]   ServerBeginEditingBuildingActor OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerController.ServerEditBuildingActor",
                    (void*)Building::ServerEditBuildingActorHook, Building::ServerEditBuildingActorOG);
    LOGF("[HOOKS]   ServerEditBuildingActor OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerController.ServerEndEditingBuildingActor",
                    (void*)Building::ServerEndEditingBuildingActorHook, Building::ServerEndEditingBuildingActorOG);
    LOGF("[HOOKS]   ServerEndEditingBuildingActor OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerController.ServerRepairBuildingActor",
                    (void*)Building::ServerRepairBuildingActorHook, Building::ServerRepairBuildingActorOG);
    LOGF("[HOOKS]   ServerRepairBuildingActor OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortDecoTool.ServerSpawnDeco",
                    (void*)Building::ServerSpawnDecoHook, Building::ServerSpawnDecoOG);
    LOGF("[HOOKS]   ServerSpawnDeco OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortDecoTool_ContextTrap.ServerSpawnDeco_Implementation",
                    (void*)Building::ServerSpawnDecoHook, Building::ServerSpawnDecoOG);
    LOGF("[HOOKS]   ServerSpawnDeco (ContextTrap) OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortDecoTool.ServerCreateBuildingAndSpawnDeco",
                    (void*)Building::ServerCreateBuildingAndSpawnDecoHook, Building::ServerCreateBuildingAndSpawnDecoOG);
    LOGF("[HOOKS]   ServerCreateBuildingAndSpawnDeco OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortDecoTool_ContextTrap.ServerCreateBuildingAndSpawnDeco_Implementation",
                    (void*)Building::ServerCreateBuildingAndSpawnDecoHook, Building::ServerCreateBuildingAndSpawnDecoOG);
    LOGF("[HOOKS]   ServerCreateBuildingAndSpawnDeco (ContextTrap) OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortControllerComponent_Interaction.ServerAttemptInteract",
                    (void*)Looting::ServerAttemptInteractHook, Looting::ServerAttemptInteractOG);
    LOGF("[HOOKS]   ServerAttemptInteract OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortKismetLibrary.PickLootDrops",
                    (void*)Looting::PickLootDropsHook, Looting::PickLootDropsOG);
    LOGF("[HOOKS]   PickLootDrops OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortKismetLibrary.K2_SpawnPickupInWorld",
                    (void*)Looting::K2_SpawnPickupInWorldHook, Looting::K2_SpawnPickupInWorldOG);
    LOGF("[HOOKS]   K2_SpawnPickupInWorld OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortKismetLibrary.SpawnItemVariantPickupInWorld",
                    (void*)Looting::SpawnItemVariantPickupInWorldHook, Looting::SpawnItemVariantPickupInWorldOG);
    LOGF("[HOOKS]   SpawnItemVariantPickupInWorld OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortAthenaSupplyDrop.SpawnPickup",
                    (void*)Looting::SupplyDropSpawnPickupHook, Looting::SupplyDropSpawnPickupOG);
    LOGF("[HOOKS]   SupplyDropSpawnPickup OK");

    Utils::ExecHook(L"/Script/FortniteGame.BuildingFoundation.SetDynamicFoundationEnabled",
                    (void*)Misc::SetDynamicFoundationEnabledHook, Misc::SetDynamicFoundationEnabledOG);
    LOGF("[HOOKS]   SetDynamicFoundationEnabled OK");

    Utils::ExecHook(L"/Script/FortniteGame.BuildingFoundation.SetDynamicFoundationTransform",
                    (void*)Misc::SetDynamicFoundationTransformHook, Misc::SetDynamicFoundationTransformOG);
    LOGF("[HOOKS]   SetDynamicFoundationTransform OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortAthenaCreativePortal.TeleportPlayerToLinkedVolume",
                    (void*)Creative::TeleportPlayerToLinkedVolumeHook, Creative::TeleportPlayerToLinkedVolumeOG);
    LOGF("[HOOKS]   TeleportPlayerToLinkedVolume OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerControllerAthena.ServerTeleportToPlaygroundLobbyIsland",
                    (void*)Creative::ServerTeleportToPlaygroundLobbyIslandHook, Creative::ServerTeleportToPlaygroundLobbyIslandOG);
    LOGF("[HOOKS]   ServerTeleportToPlaygroundLobbyIsland OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerControllerAthena.MakeNewCreativePlot",
                    (void*)Creative::MakeNewCreativePlotHook, Creative::MakeNewCreativePlotOG);
    LOGF("[HOOKS]   MakeNewCreativePlot OK");

    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerControllerAthena.UpdateCreativePlotName",
                    (void*)Creative::UpdateCreativePlotNameHook, Creative::UpdateCreativePlotNameOG);
    LOGF("[HOOKS]   UpdateCreativePlotName OK");

    LOGF("[HOOKS] All ExecFunction hooks installed");
}

static void InstallNativeHooks() {
    LOGF("[HOOKS] Installing native hooks");

    DobbyHook((void*)(Sarah::ImageBase + Off::GetNetMode), (void*)GetNetModeHook, (void**)&GetNetModeOG);
    LOGF("[HOOKS]   GetNetMode OK");

    DobbyHook((void*)(Sarah::ImageBase + Off::TickFlush), (void*)Misc::TickFlush, (void**)&Misc::TickFlushOG);
    LOGF("[HOOKS]   TickFlush OK");

    DobbyHook((void*)(Sarah::ImageBase + Off::ClientOnPawnDied), (void*)Player::ClientOnPawnDied, (void**)&Player::ClientOnPawnDiedOG);
    LOGF("[HOOKS]   ClientOnPawnDied OK");

    DobbyHook((void*)(Sarah::ImageBase + Off::BuildingActor_OnDamageServer), (void*)Building::OnDamageServer, (void**)&Building::BuildingActor_OnDamageServerOG);
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

    if (!InitImageBase()) {
        LOGF("[MAIN] InitImageBase FAILED");
        return;
    }
    LOGF("[MAIN] libUnreal found, ImageBase=0x%lx", Sarah::ImageBase);

    WaitForWorld();
    LOGF("[MAIN] WaitForWorld returned");

    if (!Sarah::InitGObjectsLayout()) {
        LOGF("[MAIN] GObjects layout FAILED");
        return;
    }
    LOGF("[MAIN] GObjects Num = %d", Sarah::UObjectManager::Num());

    LOGF("[MAIN] Before SetClientOffOnly");
    LOGF("[MAIN] GIsEditor=%d GIsClient=%d GIsServer=%d",
         GetGIsEditor(), GetGIsClient(), GetGIsServer());

    SetClientOffOnly();

    LOGF("[MAIN] After SetClientOffOnly");
    LOGF("[MAIN] GIsEditor=%d GIsClient=%d GIsServer=%d",
         GetGIsEditor(), GetGIsClient(), GetGIsServer());

    srand((uint32_t)time(nullptr));

    InstallNativeHooks();

    InstallHooks_NoProcessEvent();

    if (bGameSessions) {
        PatchBytes<uint8_t>(Off::GameSessionPatch, 0x85);
        LOGF("[PATCH] GameSession patch applied");
    }

    LOGF("[MAP] Waiting 15s for Frontend to settle");
    std::this_thread::sleep_for(std::chrono::seconds(15));

    LOGF("[MAP] Requesting map travel to Artemis_Terrain");
    const wchar_t* cmd = bCreative ? L"open Creative_NoApollo_Terrain" : L"open Artemis_Terrain";

    if (ExecuteOpenCommand(cmd)) {
        LOGF("[MAP] Map travel requested");
    } else {
        LOGF("[MAP] Map travel FAILED");
    }

    LOGF("[MAP] Waiting 60s for Artemis_Terrain to load");
    std::this_thread::sleep_for(std::chrono::seconds(60));

    LOGF("[MAIN] MainThread DONE");
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    InitLogFile();
    LOGF("========================================");
    LOGF("OwenGameServer loading...");
    LOGF("PID: %d", getpid());
    LOGF("========================================");
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
