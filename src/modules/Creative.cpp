#include "pch.h"
#include "Creative.hpp"
#include "Inventory.hpp"
#include "options.h"
#include "Utils.hpp"
#include "UObject.hpp"
#include "FName.hpp"

void Creative::ServerTeleportToPlaygroundLobbyIslandHook(UObject* Context, FFrame& Stack) {
    STACK_SAVE(Stack, _saved);
    Stack.IncrementCode();

    auto controller = (AFortPlayerControllerAthena*)Context;
    if (controller && bCreative) {
        auto gameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
        if (gameMode) {
            auto pawn = controller->GetPlayerPawn();
            if (pawn) {
                if (controller->WarmupPlayerStart) {
                    pawn->K2_TeleportTo(controller->WarmupPlayerStart->K2_GetActorLocation(), pawn->K2_GetActorRotation());
                } else {
                    auto actor = gameMode->ChoosePlayerStart(controller);
                    if (actor) pawn->K2_TeleportTo(actor->K2_GetActorLocation(), actor->K2_GetActorRotation());
                }
            }
            auto num = controller->WorldInventory->Inventory.ReplicatedEntries.Num();
            if (num != 0) {
                controller->WorldInventory->Inventory.ReplicatedEntries.ResetNum();
                controller->WorldInventory->Inventory.ItemInstances.ResetNum();
            }
            if (controller->CosmeticLoadoutPC.Pickaxe) Inventory::GiveItem(controller, controller->CosmeticLoadoutPC.Pickaxe->WeaponDefinition);
            for (auto& startingItem : gameMode->StartingItems) {
                if (startingItem.Count && startingItem.Item && !startingItem.Item->IsA(UFortSmartBuildingItemDefinition::StaticClass())) {
                    Inventory::GiveItem(controller, startingItem.Item, startingItem.Count);
                }
            }
        }
    }

    CALL_OG_VOID(Stack, Context, ServerTeleportToPlaygroundLobbyIslandOG, _saved);
}

void Creative::TeleportPlayerToLinkedVolumeHook(UObject* Context, FFrame& Stack) {
    STACK_SAVE(Stack, _saved);
    AFortPlayerPawn* PlayerPawn = nullptr;
    bool bUseSpawnTags = false;
    Stack.StepCompiledIn(&PlayerPawn);
    Stack.StepCompiledIn(&bUseSpawnTags);
    Stack.IncrementCode();

    auto portal = (AFortAthenaCreativePortal*)Context;
    if (portal && PlayerPawn && portal->LinkedVolume) {
        auto location = portal->LinkedVolume->K2_GetActorLocation();
        location.Z = 10000;
        PlayerPawn->K2_TeleportTo(location, FRotator());
        PlayerPawn->BeginSkydiving(false);
    }

    CALL_OG_VOID(Stack, Context, TeleportPlayerToLinkedVolumeOG, _saved);
}

void Creative::MakeNewCreativePlotHook(UObject* Context, FFrame& Stack) {
    STACK_SAVE(Stack, _saved);
    UFortCreativeRealEstatePlotItemDefinition PlotType;
    FString Locale, Title;
    Stack.StepCompiledIn(&PlotType);
    Stack.StepCompiledIn(&Locale);
    Stack.StepCompiledIn(&Title);
    Stack.IncrementCode();

    auto controller = (AFortPlayerControllerAthena*)Context;
    if (controller) {
        controller->ClientBroadcastOnMakeNewCreativePlotFinished(true, UKismetTextLibrary::Conv_StringToText(Utils::ToFString(L"")));
    }

    CALL_OG_VOID(Stack, Context, MakeNewCreativePlotOG, _saved);
}

void Creative::UpdateCreativePlotNameHook(UObject* Context, FFrame& Stack) {
    STACK_SAVE(Stack, _saved);
    FString IslandId, Locale, Title;
    Stack.StepCompiledIn(&IslandId);
    Stack.StepCompiledIn(&Locale);
    Stack.StepCompiledIn(&Title);
    Stack.IncrementCode();

    auto controller = (AFortPlayerControllerAthena*)Context;
    if (controller) {
        controller->ClientBroadcastOnUpdateCreativePlotName(true, UKismetTextLibrary::Conv_StringToText(Utils::ToFString(L"")));
    }

    CALL_OG_VOID(Stack, Context, UpdateCreativePlotNameOG, _saved);
}

void Creative::ServerTeleportToPlaygroundLobbyIsland(AFortPlayerControllerAthena* controller) {
    if (!controller || !bCreative) return;
    auto gameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
    if (!gameMode) return;
    auto pawn = controller->GetPlayerPawn();
    if (!pawn) return;
    if (controller->WarmupPlayerStart) {
        pawn->K2_TeleportTo(controller->WarmupPlayerStart->K2_GetActorLocation(), pawn->K2_GetActorRotation());
    }
    auto num = controller->WorldInventory->Inventory.ReplicatedEntries.Num();
    if (num != 0) {
        controller->WorldInventory->Inventory.ReplicatedEntries.ResetNum();
        controller->WorldInventory->Inventory.ItemInstances.ResetNum();
    }
    if (controller->CosmeticLoadoutPC.Pickaxe) Inventory::GiveItem(controller, controller->CosmeticLoadoutPC.Pickaxe->WeaponDefinition);
    for (auto& startingItem : gameMode->StartingItems) {
        if (startingItem.Count && startingItem.Item && !startingItem.Item->IsA(UFortSmartBuildingItemDefinition::StaticClass())) {
            Inventory::GiveItem(controller, startingItem.Item, startingItem.Count);
        }
    }
}

void Creative::Hook() {
    Utils::ExecHook(L"/Script/FortniteGame.FortAthenaCreativePortal.TeleportPlayerToLinkedVolume", (void*)TeleportPlayerToLinkedVolumeHook, TeleportPlayerToLinkedVolumeOG);
    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerControllerAthena.ServerTeleportToPlaygroundLobbyIsland", (void*)ServerTeleportToPlaygroundLobbyIslandHook, ServerTeleportToPlaygroundLobbyIslandOG);
    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerControllerAthena.MakeNewCreativePlot", (void*)MakeNewCreativePlotHook, MakeNewCreativePlotOG);
    Utils::ExecHook(L"/Script/FortniteGame.FortPlayerControllerAthena.UpdateCreativePlotName", (void*)UpdateCreativePlotNameHook, UpdateCreativePlotNameOG);
}
