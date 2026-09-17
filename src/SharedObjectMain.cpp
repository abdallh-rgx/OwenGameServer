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
#include <mutex>
#include <unistd.h>

static FILE* g_logfile = nullptr;
static std::mutex g_log_mutex;

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
    }
}

static void (*ProcessEventOG)(UObject*, UFunction*, void*) = nullptr;

namespace Hooks {

UFunction* ReadyToStartMatch = nullptr;
UFunction* HandleStartingNewPlayer = nullptr;
UFunction* OnAircraftEnteredDropZone = nullptr;
UFunction* OnAircraftExitedDropZone = nullptr;
UFunction* ServerAcknowledgePossession = nullptr;
UFunction* ServerExecuteInventoryItem = nullptr;
UFunction* ServerReturnToMainMenu = nullptr;
UFunction* ServerAttemptAircraftJump = nullptr;
UFunction* ServerPlayEmoteItem = nullptr;
UFunction* ServerSendZiplineState = nullptr;
UFunction* ServerHandlePickupInfo = nullptr;
UFunction* MovingEmoteStopped = nullptr;
UFunction* ServerAttemptInventoryDrop = nullptr;
UFunction* ServerClientIsReadyToRespawn = nullptr;
UFunction* ServerChangeName = nullptr;
UFunction* OnCapsuleBeginOverlap = nullptr;
UFunction* TeleportPlayerPawn = nullptr;
UFunction* ServerCreateBuildingActor = nullptr;
UFunction* ServerBeginEditingBuildingActor = nullptr;
UFunction* ServerEditBuildingActor = nullptr;
UFunction* ServerEndEditingBuildingActor = nullptr;
UFunction* ServerRepairBuildingActor = nullptr;
UFunction* ServerSpawnDeco = nullptr;
UFunction* ServerSpawnDecoContextTrap = nullptr;
UFunction* ServerCreateBuildingAndSpawnDeco = nullptr;
UFunction* ServerCreateBuildingAndSpawnDecoContextTrap = nullptr;
UFunction* ServerAttemptInteract = nullptr;
UFunction* PickLootDrops = nullptr;
UFunction* K2_SpawnPickupInWorld = nullptr;
UFunction* SpawnItemVariantPickupInWorld = nullptr;
UFunction* SupplyDropSpawnPickup = nullptr;
UFunction* SetDynamicFoundationEnabled = nullptr;
UFunction* SetDynamicFoundationTransform = nullptr;
UFunction* TeleportPlayerToLinkedVolume = nullptr;
UFunction* ServerTeleportToPlaygroundLobbyIsland = nullptr;
UFunction* MakeNewCreativePlot = nullptr;
UFunction* UpdateCreativePlotName = nullptr;

void CacheFunctions() {
    ReadyToStartMatch = (UFunction*)Utils::FindObject(L"/Script/Engine.GameMode.ReadyToStartMatch");
    HandleStartingNewPlayer = (UFunction*)Utils::FindObject(L"/Script/Engine.GameModeBase.HandleStartingNewPlayer");
    OnAircraftEnteredDropZone = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortGameModeAthena.OnAircraftEnteredDropZone");
    OnAircraftExitedDropZone = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortGameModeAthena.OnAircraftExitedDropZone");
    ServerAcknowledgePossession = (UFunction*)Utils::FindObject(L"/Script/Engine.PlayerController.ServerAcknowledgePossession");
    ServerExecuteInventoryItem = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortPlayerController.ServerExecuteInventoryItem");
    ServerReturnToMainMenu = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortPlayerController.ServerReturnToMainMenu");
    ServerAttemptAircraftJump = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortControllerComponent_Aircraft.ServerAttemptAircraftJump");
    ServerPlayEmoteItem = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortPlayerController.ServerPlayEmoteItem");
    ServerSendZiplineState = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortPlayerPawn.ServerSendZiplineState");
    ServerHandlePickupInfo = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortPlayerPawn.ServerHandlePickupInfo");
    MovingEmoteStopped = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortPawn.MovingEmoteStopped");
    ServerAttemptInventoryDrop = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortPlayerController.ServerAttemptInventoryDrop");
    ServerClientIsReadyToRespawn = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortPlayerControllerAthena.ServerClientIsReadyToRespawn");
    ServerChangeName = (UFunction*)Utils::FindObject(L"/Script/Engine.PlayerController.ServerChangeName");
    OnCapsuleBeginOverlap = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortPlayerPawn.OnCapsuleBeginOverlap");
    TeleportPlayerPawn = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortMissionLibrary.TeleportPlayerPawn");
    ServerCreateBuildingActor = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortPlayerController.ServerCreateBuildingActor");
    ServerBeginEditingBuildingActor = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortPlayerController.ServerBeginEditingBuildingActor");
    ServerEditBuildingActor = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortPlayerController.ServerEditBuildingActor");
    ServerEndEditingBuildingActor = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortPlayerController.ServerEndEditingBuildingActor");
    ServerRepairBuildingActor = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortPlayerController.ServerRepairBuildingActor");
    ServerSpawnDeco = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortDecoTool.ServerSpawnDeco");
    ServerSpawnDecoContextTrap = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortDecoTool_ContextTrap.ServerSpawnDeco_Implementation");
    ServerCreateBuildingAndSpawnDeco = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortDecoTool.ServerCreateBuildingAndSpawnDeco");
    ServerCreateBuildingAndSpawnDecoContextTrap = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortDecoTool_ContextTrap.ServerCreateBuildingAndSpawnDeco_Implementation");
    ServerAttemptInteract = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortControllerComponent_Interaction.ServerAttemptInteract");
    PickLootDrops = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortKismetLibrary.PickLootDrops");
    K2_SpawnPickupInWorld = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortKismetLibrary.K2_SpawnPickupInWorld");
    SpawnItemVariantPickupInWorld = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortKismetLibrary.SpawnItemVariantPickupInWorld");
    SupplyDropSpawnPickup = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortAthenaSupplyDrop.SpawnPickup");
    SetDynamicFoundationEnabled = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.BuildingFoundation.SetDynamicFoundationEnabled");
    SetDynamicFoundationTransform = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.BuildingFoundation.SetDynamicFoundationTransform");
    TeleportPlayerToLinkedVolume = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortAthenaCreativePortal.TeleportPlayerToLinkedVolume");
    ServerTeleportToPlaygroundLobbyIsland = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortPlayerControllerAthena.ServerTeleportToPlaygroundLobbyIsland");
    MakeNewCreativePlot = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortPlayerControllerAthena.MakeNewCreativePlot");
    UpdateCreativePlotName = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortPlayerControllerAthena.UpdateCreativePlotName");

    int total = 0;

    UFunction* all[] = {
        ReadyToStartMatch,
        HandleStartingNewPlayer,
        OnAircraftEnteredDropZone,
        OnAircraftExitedDropZone,
        ServerAcknowledgePossession,
        ServerExecuteInventoryItem,
        ServerReturnToMainMenu,
        ServerAttemptAircraftJump,
        ServerPlayEmoteItem,
        ServerSendZiplineState,
        ServerHandlePickupInfo,
        MovingEmoteStopped,
        ServerAttemptInventoryDrop,
        ServerClientIsReadyToRespawn,
        ServerChangeName,
        OnCapsuleBeginOverlap,
        TeleportPlayerPawn,
        ServerCreateBuildingActor,
        ServerBeginEditingBuildingActor,
        ServerEditBuildingActor,
        ServerEndEditingBuildingActor,
        ServerRepairBuildingActor,
        ServerAttemptInteract,
        PickLootDrops,
        K2_SpawnPickupInWorld,
        SpawnItemVariantPickupInWorld,
        SupplyDropSpawnPickup,
        SetDynamicFoundationEnabled,
        SetDynamicFoundationTransform,
        TeleportPlayerToLinkedVolume,
        ServerTeleportToPlaygroundLobbyIsland
    };

    for (auto* fn : all)
        if (fn)
            total++;

    LOGF("[HOOKS] Cached %d/%d UFunctions",
          total,
          (int)(sizeof(all) / sizeof(all[0])));
}

}

void ProcessEventHook(UObject* context, UFunction* function, void* parms) {
    if (function && parms) {
        if (function == Hooks::ReadyToStartMatch) {
            LOGF("[PE] ReadyToStartMatch");
            GameMode::ReadyToStartMatch(context, (Params::AGameMode_ReadyToStartMatch*)parms);
            return;
        }

        if (function == Hooks::HandleStartingNewPlayer) {
            LOGF("[PE] HandleStartingNewPlayer");
            GameMode::HandleStartingNewPlayer(context, (Params::AGameModeBase_HandleStartingNewPlayer*)parms);
            return;
        }

        if (function == Hooks::OnAircraftEnteredDropZone) {
            LOGF("[PE] OnAircraftEnteredDropZone");
            GameMode::OnAircraftEnteredDropZone(context, (Params::AFortGameModeAthena_OnAircraftEnteredDropZone*)parms);
            return;
        }

        if (function == Hooks::OnAircraftExitedDropZone) {
            LOGF("[PE] OnAircraftExitedDropZone");
            GameMode::OnAircraftExitedDropZone(context, (Params::AFortGameModeAthena_OnAircraftExitedDropZone*)parms);
            return;
        }

        if (function == Hooks::ServerAcknowledgePossession) {
            LOGF("[PE] ServerAcknowledgePossession");
            Player::ServerAcknowledgePossession(context, (Params::APlayerController_ServerAcknowledgePossession*)parms);
            return;
        }

        if (function == Hooks::ServerExecuteInventoryItem) {
            LOGF("[PE] ServerExecuteInventoryItem");
            Player::ServerExecuteInventoryItem(context, (Params::AFortPlayerController_ServerExecuteInventoryItem*)parms);
            return;
        }

        if (function == Hooks::ServerReturnToMainMenu) {
            LOGF("[PE] ServerReturnToMainMenu");
            Player::ServerReturnToMainMenu(context);
            return;
        }

        if (function == Hooks::ServerAttemptAircraftJump) {
            LOGF("[PE] ServerAttemptAircraftJump");
            Player::ServerAttemptAircraftJump(context, (Params::UFortControllerComponent_Aircraft_ServerAttemptAircraftJump*)parms);
            return;
        }

        if (function == Hooks::ServerPlayEmoteItem) {
            LOGF("[PE] ServerPlayEmoteItem");
            Player::ServerPlayEmoteItem(context, (Params::AFortPlayerController_ServerPlayEmoteItem*)parms);
            return;
        }

        if (function == Hooks::ServerSendZiplineState) {
            LOGF("[PE] ServerSendZiplineState");
            Player::ServerSendZiplineState(context, (Params::AFortPlayerPawn_ServerSendZiplineState*)parms);
            return;
        }

        if (function == Hooks::ServerHandlePickupInfo) {
            LOGF("[PE] ServerHandlePickupInfo");
            Player::ServerHandlePickupInfo(context, (Params::AFortPlayerPawn_ServerHandlePickupInfo*)parms);
            return;
        }

        if (function == Hooks::MovingEmoteStopped) {
            LOGF("[PE] MovingEmoteStopped");
            Player::MovingEmoteStopped(context);
            return;
        }

        if (function == Hooks::ServerAttemptInventoryDrop) {
            LOGF("[PE] ServerAttemptInventoryDrop");
            Player::ServerAttemptInventoryDrop(context, (Params::AFortPlayerController_ServerAttemptInventoryDrop*)parms);
            return;
        }

        if (function == Hooks::ServerClientIsReadyToRespawn) {
            LOGF("[PE] ServerClientIsReadyToRespawn");
            Player::ServerClientIsReadyToRespawn(context);
            return;
        }

        if (function == Hooks::ServerChangeName) {
            LOGF("[PE] ServerChangeName");
            Player::ServerChangeName(context, (Params::APlayerController_ServerChangeName*)parms);
            return;
        }

        if (function == Hooks::OnCapsuleBeginOverlap) {
            LOGF("[PE] OnCapsuleBeginOverlap");
            Player::OnCapsuleBeginOverlap(context, (Params::AFortPlayerPawn_OnCapsuleBeginOverlap*)parms);
            return;
        }

        if (function == Hooks::TeleportPlayerPawn) {
            LOGF("[PE] TeleportPlayerPawn");
            Player::TeleportPlayerPawn(context, (Params::UFortMissionLibrary_TeleportPlayerPawn*)parms);
            return;
        }

        if (function == Hooks::ServerCreateBuildingActor) {
            LOGF("[PE] ServerCreateBuildingActor");
            Building::ServerCreateBuildingActor(context, (Params::AFortPlayerController_ServerCreateBuildingActor*)parms);
            return;
        }

        if (function == Hooks::ServerBeginEditingBuildingActor) {
            LOGF("[PE] ServerBeginEditingBuildingActor");
            Building::ServerBeginEditingBuildingActor(context, (Params::AFortPlayerController_ServerBeginEditingBuildingActor*)parms);
            return;
        }

        if (function == Hooks::ServerEditBuildingActor) {
            LOGF("[PE] ServerEditBuildingActor");
            Building::ServerEditBuildingActor(context, (Params::AFortPlayerController_ServerEditBuildingActor*)parms);
            return;
        }

        if (function == Hooks::ServerEndEditingBuildingActor) {
            LOGF("[PE] ServerEndEditingBuildingActor");
            Building::ServerEndEditingBuildingActor(context, (Params::AFortPlayerController_ServerEndEditingBuildingActor*)parms);
            return;
        }

        if (function == Hooks::ServerRepairBuildingActor) {
            LOGF("[PE] ServerRepairBuildingActor");
            Building::ServerRepairBuildingActor(context, (Params::AFortPlayerController_ServerRepairBuildingActor*)parms);
            return;
        }

        if (function == Hooks::ServerSpawnDeco || function == Hooks::ServerSpawnDecoContextTrap) {
            LOGF("[PE] ServerSpawnDeco");
            Building::ServerSpawnDeco(context, (Params::AFortDecoTool_ServerSpawnDeco*)parms);
            return;
        }

        if (function == Hooks::ServerCreateBuildingAndSpawnDeco || function == Hooks::ServerCreateBuildingAndSpawnDecoContextTrap) {
            LOGF("[PE] ServerCreateBuildingAndSpawnDeco");
            Building::ServerCreateBuildingAndSpawnDeco(context, (Params::AFortDecoTool_ServerCreateBuildingAndSpawnDeco*)parms);
            return;
        }

        if (function == Hooks::ServerAttemptInteract) {
            LOGF("[PE] ServerAttemptInteract");
            Looting::ServerAttemptInteract(context, (Params::UFortControllerComponent_Interaction_ServerAttemptInteract*)parms);
            return;
        }

        if (function == Hooks::PickLootDrops) {
            LOGF("[PE] PickLootDrops");
            Looting::PickLootDrops(context, (Params::UFortKismetLibrary_PickLootDrops*)parms);
            return;
        }

        if (function == Hooks::K2_SpawnPickupInWorld) {
            LOGF("[PE] K2_SpawnPickupInWorld");
            Looting::K2_SpawnPickupInWorld(context, (Params::UFortKismetLibrary_K2_SpawnPickupInWorld*)parms);
            return;
        }

        if (function == Hooks::SpawnItemVariantPickupInWorld) {
            LOGF("[PE] SpawnItemVariantPickupInWorld");
            Looting::SpawnItemVariantPickupInWorld(context, (Params::UFortKismetLibrary_SpawnItemVariantPickupInWorld*)parms);
            return;
        }

        if (function == Hooks::SupplyDropSpawnPickup) {
            LOGF("[PE] SupplyDropSpawnPickup");
            Looting::SupplyDropSpawnPickup(context, (Params::AFortAthenaSupplyDrop_SpawnPickup*)parms);
            return;
        }

        if (function == Hooks::SetDynamicFoundationEnabled) {
            LOGF("[PE] SetDynamicFoundationEnabled");
            Misc::SetDynamicFoundationEnabled(context, (Params::ABuildingFoundation_SetDynamicFoundationEnabled*)parms);
            return;
        }

        if (function == Hooks::SetDynamicFoundationTransform) {
            LOGF("[PE] SetDynamicFoundationTransform");
            Misc::SetDynamicFoundationTransform(context, (Params::ABuildingFoundation_SetDynamicFoundationTransform*)parms);
            return;
        }

        if (function == Hooks::TeleportPlayerToLinkedVolume) {
            LOGF("[PE] TeleportPlayerToLinkedVolume");
            Creative::TeleportPlayerToLinkedVolume(context, (Params::AFortAthenaCreativePortal_TeleportPlayerToLinkedVolume*)parms);
            return;
        }

        if (function == Hooks::ServerTeleportToPlaygroundLobbyIsland) {
            LOGF("[PE] ServerTeleportToPlaygroundLobbyIsland");
            Creative::ServerTeleportToPlaygroundLobbyIsland((AFortPlayerControllerAthena*)context);
            return;
        }

        if (function == Hooks::MakeNewCreativePlot) {
            LOGF("[PE] MakeNewCreativePlot");
            Creative::MakeNewCreativePlot(context, (Params::AFortPlayerControllerAthena_MakeNewCreativePlot*)parms);
            return;
        }

        if (function == Hooks::UpdateCreativePlotName) {
            LOGF("[PE] UpdateCreativePlotName");
            Creative::UpdateCreativePlotName(context, (Params::AFortPlayerControllerAthena_UpdateCreativePlotName*)parms);
            return;
        }
    }

    if (ProcessEventOG)
        ProcessEventOG(context, function, parms);
}

static int (*GetNetModeOG)(void*) = nullptr;

static int GetNetModeHook(void* world) {
    return 1;
}

static EEFortTeam (*PickTeamOG)(AFortGameModeAthena*, uint8_t, AFortPlayerControllerAthena*) = nullptr;

static EEFortTeam PickTeamHook(AFortGameModeAthena* gameMode, uint8_t preferredTeam, AFortPlayerControllerAthena* controller) {
    LOGF("[HOOK] PickTeam");
    return GameMode::PickTeam(gameMode, preferredTeam, controller);
}

static APawn* (*SpawnDefaultPawnForOG)(AGameModeBase*, AController*, AActor*) = nullptr;

static APawn* SpawnDefaultPawnForHook(AGameModeBase* gameMode, AController* newPlayer, AActor* startSpot) {
    LOGF("[HOOK] SpawnDefaultPawnFor");

    APawn* result = GameMode::SpawnDefaultPawnFor(gameMode, newPlayer, startSpot);

    if (result) {
        LOGF("[HOOK] SpawnDefaultPawnFor returned pawn");
        return result;
    }

    LOGF("[HOOK] SpawnDefaultPawnFor returned null");

    if (SpawnDefaultPawnForOG)
        return SpawnDefaultPawnForOG(gameMode, newPlayer, startSpot);

    return nullptr;
}

static void WaitForWorld() {
    LOGF("[CORE] Waiting for World");

    for (int i = 0; i < 120; i++) {
        if (UWorld::GetWorld() && UEngine::GetEngine()) {
            LOGF("[CORE] World and Engine ready");
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    LOGF("[CORE] World wait timeout");
}

static void MainThread() {
    LOGF("[MAIN] MainThread started");

    std::this_thread::sleep_for(std::chrono::seconds(5));

    LOGF("[MAIN] Initial delay finished");

    if (!InitImageBase()) {
        LOGF("[MAIN] InitImageBase FAILED");
        LOGE("libUnreal.so not found");
        return;
    }

    LOGF("[MAIN] libUnreal found");
    LOGF("[MAIN] ImageBase = 0x%lx", Sarah::ImageBase);

    WaitForWorld();

    LOGF("[CORE] Initializing GObjects");

    if (!Sarah::InitGObjectsLayout()) {
        LOGF("[CORE] GObjects layout validation FAILED");
    } else {
        LOGF("[CORE] GObjects layout OK");
        LOGF("[CORE] GObjects Num = %d", Sarah::UObjectManager::Num());
    }

    LOGF("[CORE] Testing FName");
    {
        FName testName = MakeFName(L"PlayerController");
        LOGF("[CORE] FName PlayerController index=%d", testName.ComparisonIndex);

        FName testName2 = MakeFName(L"GameNetDriver");
        LOGF("[CORE] FName GameNetDriver index=%d", testName2.ComparisonIndex);

        FName testName3 = MakeFName(L"None");
        LOGF("[CORE] FName None index=%d", testName3.ComparisonIndex);
    }

    LOGF("[CORE] Before SetDedicatedServerMode");
    LOGF("[CORE] GIsEditor=%d GIsClient=%d GIsServer=%d",
         GetGIsEditor(),
         GetGIsClient(),
         GetGIsServer());

    SetDedicatedServerMode();

    LOGF("[CORE] After SetDedicatedServerMode");
    LOGF("[CORE] GIsEditor=%d GIsClient=%d GIsServer=%d",
         GetGIsEditor(),
         GetGIsClient(),
         GetGIsServer());

    srand((uint32_t)time(nullptr));

    LOGF("[HOOKS] Caching functions");

    Hooks::CacheFunctions();

    LOGF("[HOOKS] Installing ProcessEvent");
    DobbyHook(
        (void*)(Sarah::ImageBase + Off::ProcessEvent),
        (void*)ProcessEventHook,
        (void**)&ProcessEventOG
    );
    LOGF("[HOOKS] ProcessEvent installed");

    LOGF("[HOOKS] Installing GetNetMode");
    DobbyHook(
        (void*)(Sarah::ImageBase + Off::GetNetMode),
        (void*)GetNetModeHook,
        (void**)&GetNetModeOG
    );
    LOGF("[HOOKS] GetNetMode installed");

    LOGF("[HOOKS] Installing TickFlush");
    DobbyHook(
        (void*)(Sarah::ImageBase + Off::TickFlush),
        (void*)Misc::TickFlush,
        (void**)&Misc::TickFlushOG
    );
    LOGF("[HOOKS] TickFlush installed");

    LOGF("[HOOKS] Installing ClientOnPawnDied");
    DobbyHook(
        (void*)(Sarah::ImageBase + Off::ClientOnPawnDied),
        (void*)Player::ClientOnPawnDied,
        (void**)&Player::ClientOnPawnDiedOG
    );
    LOGF("[HOOKS] ClientOnPawnDied installed");

    LOGF("[HOOKS] Installing BuildingActor_OnDamageServer");
    DobbyHook(
        (void*)(Sarah::ImageBase + Off::BuildingActor_OnDamageServer),
        (void*)Building::OnDamageServer,
        (void**)&Building::BuildingActor_OnDamageServerOG
    );
    LOGF("[HOOKS] BuildingActor_OnDamageServer installed");

    LOGF("[HOOKS] Installing PickTeam");
    DobbyHook(
        (void*)(Sarah::ImageBase + Off::PickTeam),
        (void*)PickTeamHook,
        (void**)&PickTeamOG
    );
    LOGF("[HOOKS] PickTeam installed");

    LOGF("[HOOKS] Installing StartAircraftPhase");
    DobbyHook(
        (void*)(Sarah::ImageBase + Off::StartAircraftPhase),
        (void*)Misc::StartAircraftPhase,
        (void**)&Misc::StartAircraftPhaseOG
    );
    LOGF("[HOOKS] StartAircraftPhase installed");

    LOGF("[HOOKS] Installing SpawnDefaultPawnFor");
    DobbyHook(
        (void*)(Sarah::ImageBase + Off::SpawnDefaultPawnFor),
        (void*)SpawnDefaultPawnForHook,
        (void**)&SpawnDefaultPawnForOG
    );
    LOGF("[HOOKS] SpawnDefaultPawnFor installed");

    if (bGameSessions) {
        LOGF("[PATCH] Applying GameSession patch");
        PatchBytes<uint8_t>(Off::GameSessionPatch, 0x85);
        LOGF("[PATCH] GameSession patch applied");
    }

    LOGF("[CORE] All hooks installed");

    LOGF("[CORE] Starting Listen");

    if (Misc::Listen()) {
        LOGF("[CORE] Server is listening");
    } else {
        LOGF("[CORE] Listen FAILED");
    }

    LOGF("[MAP] Starting map travel");

    UKismetSystemLibrary::ExecuteConsoleCommand(
        UWorld::GetWorld(),
        Utils::ToFString(
            bCreative
                ? L"open Creative_NoApollo_Terrain"
                : L"open Artemis_Terrain"
        ),
        nullptr
    );

    LOGF("[MAP] Map travel requested");
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
