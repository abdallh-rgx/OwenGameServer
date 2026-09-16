#include "pch.h"
#include "Creative.hpp"
#include "Inventory.hpp"
#include "options.h"
#include "Utils.hpp"
#include "UObject.hpp"
#include "FName.hpp"

void Creative::ServerTeleportToPlaygroundLobbyIsland(AFortPlayerControllerAthena* controller) {
    if (!controller || !bCreative) return;

    auto gameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
    if (!gameMode) return;

    auto pawn = controller->GetPlayerPawn();
    if (!pawn) return;

    if (controller->WarmupPlayerStart) {
        pawn->K2_TeleportTo(controller->WarmupPlayerStart->K2_GetActorLocation(), pawn->K2_GetActorRotation());
    } else {
        auto starts = Utils::GetAll<AActor>();
        if (!starts.empty()) {
            pawn->K2_TeleportTo(starts[0]->K2_GetActorLocation(), pawn->K2_GetActorRotation());
        }
    }

    auto num = controller->WorldInventory->Inventory.ReplicatedEntries.Num();
    if (num != 0) {
        controller->WorldInventory->Inventory.ReplicatedEntries.ResetNum();
        controller->WorldInventory->Inventory.ItemInstances.ResetNum();
    }

    if (controller->CosmeticLoadoutPC.Pickaxe) {
        Inventory::GiveItem(controller, controller->CosmeticLoadoutPC.Pickaxe->WeaponDefinition);
    }

    for (auto& startingItem : gameMode->StartingItems) {
        if (startingItem.Count && startingItem.Item && !startingItem.Item->IsA(UFortSmartBuildingItemDefinition::StaticClass())) {
            Inventory::GiveItem(controller, startingItem.Item, startingItem.Count);
        }
    }
}

void Creative::TeleportPlayerToLinkedVolume(UObject* context, Params::AFortAthenaCreativePortal_TeleportPlayerToLinkedVolume* params) {
    auto portal = (AFortAthenaCreativePortal*)context;
    auto playerPawn = params->PlayerPawn;

    if (!portal || !playerPawn) return;

    auto volume = portal->LinkedVolume;
    if (!volume) return;

    auto location = volume->K2_GetActorLocation();
    location.Z = 10000;

    playerPawn->K2_TeleportTo(location, FRotator());
    playerPawn->BeginSkydiving(false);
}

void Creative::MakeNewCreativePlot(UObject* context, Params::AFortPlayerControllerAthena_MakeNewCreativePlot* params) {
    auto controller = (AFortPlayerControllerAthena*)context;
    if (!controller) return;
    controller->ClientBroadcastOnMakeNewCreativePlotFinished(true, UKismetTextLibrary::Conv_StringToText(Utils::ToFString(L"")));
}

void Creative::UpdateCreativePlotName(UObject* context, Params::AFortPlayerControllerAthena_UpdateCreativePlotName* params) {
    auto controller = (AFortPlayerControllerAthena*)context;
    if (!controller) return;
    controller->ClientBroadcastOnUpdateCreativePlotName(true, UKismetTextLibrary::Conv_StringToText(Utils::ToFString(L"")));
}

void Creative::Hook() {
}
