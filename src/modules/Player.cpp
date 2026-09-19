#include "pch.h"
#include "Player.hpp"
#include "Abilities.hpp"
#include "AC.hpp"
#include "Inventory.hpp"
#include "options.h"
#include "Lategame.hpp"
#include "Results.hpp"
#include "Tournaments.hpp"
#include "XP.hpp"
#include "Misc.hpp"
#include "Utils.hpp"
#include "UObject.hpp"
#include "FName.hpp"

void Player::ServerAcknowledgePossession(UObject* context, Params::APlayerController_ServerAcknowledgePossession* params) {
    auto playerController = (AFortPlayerController*)context;
    if (!playerController) return;

    playerController->AcknowledgedPawn = params->P;

    AC::CheckUser((AFortPlayerControllerAthena*)playerController);
}

void Player::ServerExecuteInventoryItem(UObject* context, Params::AFortPlayerController_ServerExecuteInventoryItem* params) {
    auto playerController = (AFortPlayerController*)context;
    if (!playerController || !playerController->WorldInventory || !playerController->MyFortPawn) return;

    auto entry = playerController->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry) {
        return entry.ItemGuid == params->ItemGuid;
    });

    if (!entry) return;

    UFortWeaponItemDefinition* itemDefinition = (UFortWeaponItemDefinition*)entry->ItemDefinition;
    playerController->MyFortPawn->EquipWeaponDefinition(itemDefinition, params->ItemGuid, entry->TrackerGuid, false);
}

void Player::ServerReturnToMainMenu(UObject* context) {
    auto playerController = (AFortPlayerController*)context;
    if (!playerController) return;
    if (!playerController->NetConnection) return;
    playerController->ClientReturnToMainMenu(Utils::ToFString(L""));
}

void Player::ServerAttemptAircraftJump(UObject* context, Params::UFortControllerComponent_Aircraft_ServerAttemptAircraftJump* params) {
    auto component = (UFortControllerComponent_Aircraft*)context;
    if (!component) return;

    auto playerController = CastSDK<AFortPlayerController>(component->GetOwner());
    if (!playerController) return;

    auto gameMode = (AFortGameModeAthena*)UWorld::GetWorld()->AuthorityGameMode;
    auto gameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;

    if (gameMode) gameMode->RestartPlayer(playerController);

    if (bLateGame && playerController->MyFortPawn && gameState && gameState->Aircrafts.Num() > 0 && gameState->Aircrafts[0]) {
        FVector aircraftLocation = gameState->Aircrafts[0]->K2_GetActorLocation();

        float angle = UKismetMathLibrary::RandomFloatInRange(0.0f, 6.2831853f);
        float radius = (float)(rand() % 1000);

        float offsetX = cosf(angle) * radius;
        float offsetY = sinf(angle) * radius;

        FVector newLoc = aircraftLocation + FVector(offsetX, offsetY, 0.0);
        playerController->MyFortPawn->K2_SetActorLocation(newLoc, false, nullptr, false);
    }

    playerController->ClientSetRotation(params->ClientRotation, true);

    if (playerController->MyFortPawn) {
        playerController->MyFortPawn->BeginSkydiving(true);
        playerController->MyFortPawn->SetHealth(100);

        if (bLateGame) {
            playerController->MyFortPawn->SetShield(100);

            auto shotgun = Lategame::GetShotguns();
            auto assaultRifle = Lategame::GetAssaultRifles();
            auto sniper = Lategame::GetSnipers();
            auto heal = Lategame::GetHeals();
            auto healSlot2 = Lategame::GetHeals();

            int shotgunClipSize = 0, assaultRifleClipSize = 0, sniperClipSize = 0, healClipSize = 0, healSlot2ClipSize = 0;
            if (auto w = CastSDK<UFortWeaponItemDefinition>(shotgun.Item)) { auto s = Inventory::GetStats(w); shotgunClipSize = s ? s->ClipSize : 0; }
            if (auto w = CastSDK<UFortWeaponItemDefinition>(assaultRifle.Item)) { auto s = Inventory::GetStats(w); assaultRifleClipSize = s ? s->ClipSize : 0; }
            if (auto w = CastSDK<UFortWeaponItemDefinition>(sniper.Item)) { auto s = Inventory::GetStats(w); sniperClipSize = s ? s->ClipSize : 0; }
            if (auto w = CastSDK<UFortWeaponItemDefinition>(heal.Item)) { auto s = Inventory::GetStats(w); healClipSize = s ? s->ClipSize : 0; }
            if (auto w = CastSDK<UFortWeaponItemDefinition>(healSlot2.Item)) { auto s = Inventory::GetStats(w); healSlot2ClipSize = s ? s->ClipSize : 0; }

            Inventory::GiveItem(playerController, Lategame::GetResource(EEFortResourceType::Wood), 500);
            Inventory::GiveItem(playerController, Lategame::GetResource(EEFortResourceType::Stone), 500);
            Inventory::GiveItem(playerController, Lategame::GetResource(EEFortResourceType::Metal), 500);

            Inventory::GiveItem(playerController, Lategame::GetAmmo(EAmmoType::Assault), 250);
            Inventory::GiveItem(playerController, Lategame::GetAmmo(EAmmoType::Shotgun), 50);
            Inventory::GiveItem(playerController, Lategame::GetAmmo(EAmmoType::Submachine), 400);
            Inventory::GiveItem(playerController, Lategame::GetAmmo(EAmmoType::Rocket), 6);
            Inventory::GiveItem(playerController, Lategame::GetAmmo(EAmmoType::Sniper), 20);

            if (assaultRifle.Item) Inventory::GiveItem(playerController, assaultRifle.Item, assaultRifle.Count, assaultRifleClipSize, 0, true, true, 0);
            if (shotgun.Item) Inventory::GiveItem(playerController, shotgun.Item, shotgun.Count, shotgunClipSize, 0, true, true, 0);
            if (sniper.Item) Inventory::GiveItem(playerController, sniper.Item, sniper.Count, sniperClipSize, 0, true, true, 0);
            if (heal.Item) Inventory::GiveItem(playerController, heal.Item, heal.Count, healClipSize, 0, true, true, 0);
            if (healSlot2.Item) Inventory::GiveItem(playerController, healSlot2.Item, healSlot2.Count, healSlot2ClipSize, 0, true, true, 0);
        }
    }
}

void Player::ServerPlayEmoteItem(UObject* context, Params::AFortPlayerController_ServerPlayEmoteItem* params) {
    auto playerController = (AFortPlayerController*)context;
    auto asset = params->EmoteAsset;

    if (!playerController || !playerController->MyFortPawn || !asset) return;

    auto playerState = (AFortPlayerStateAthena*)playerController->PlayerState;
    if (!playerState) return;

    auto abilitySystemComponent = playerState->AbilitySystemComponent;
    if (!abilitySystemComponent) return;

    UObject* abilityToUse = nullptr;

    if (asset->IsA(UAthenaSprayItemDefinition::StaticClass())) {
        auto sprayAbilityClass = Utils::Find<UBlueprintGeneratedClass>(L"/Game/Abilities/Sprays/GAB_Spray_Generic.GAB_Spray_Generic_C");
        if (sprayAbilityClass)
            abilityToUse = sprayAbilityClass->ClassDefaultObject;
    } else if (auto danceAsset = CastSDK<UAthenaDanceItemDefinition>(asset)) {
        playerController->MyFortPawn->bMovingEmote = danceAsset->bMovingEmote;
        playerController->MyFortPawn->EmoteWalkSpeed = danceAsset->WalkForwardSpeed;
        auto emoteAbilityClass = Utils::Find<UBlueprintGeneratedClass>(L"/Game/Abilities/Emotes/GAB_Emote_Generic.GAB_Emote_Generic_C");
        if (emoteAbilityClass)
            abilityToUse = emoteAbilityClass->ClassDefaultObject;
    }

    if (abilityToUse && abilitySystemComponent) {
        FGameplayAbilitySpecHandle handle = Abilities::GiveAbility(abilitySystemComponent, abilityToUse);
        FPredictionKey predictionKey{};
        abilitySystemComponent->ServerTryActivateAbility(handle, true, predictionKey);
    }
}

void Player::ServerSendZiplineState(UObject* context, Params::AFortPlayerPawn_ServerSendZiplineState* params) {
    auto pawn = (AFortPlayerPawn*)context;
    if (!pawn) return;

    pawn->ZiplineState = params->InZiplineState;

    if (params->InZiplineState.bJumped) {
        auto velocity = pawn->CharacterMovement->Velocity;
        auto velocityX = velocity.X * -0.5f;
        auto velocityY = velocity.Y * -0.5f;
        pawn->LaunchCharacterJump(
            FVector(
                velocityX >= -750 ? (velocityX < 750 ? velocityX : 750) : -750,
                velocityY >= -750 ? (velocityY < 750 ? velocityY : 750) : -750,
                1200),
            false, false, true, true);
    }
}

void Player::ServerHandlePickupInfo(UObject* context, Params::AFortPlayerPawn_ServerHandlePickupInfo* params) {
    auto pawn = (AFortPlayerPawn*)context;
    auto pickup = params->PickUp;

    if (!pawn || !pickup || pickup->bPickedUp)
        return;

    if ((params->Params_0.bTrySwapWithWeapon || params->Params_0.bUseRequestedSwap) && pawn->CurrentWeapon) {
        auto pc = (AFortPlayerControllerAthena*)pawn->Controller;
        if (pc && Inventory::GetQuickbar(pawn->CurrentWeapon->WeaponData) == EEFortQuickBars::Primary
            && Inventory::GetQuickbar(pickup->PrimaryPickupItemEntry.ItemDefinition) == EEFortQuickBars::Primary) {
            pc->SwappingItemDefinition = (UFortWorldItemDefinition*)nullptr;
        }
    }

    pawn->IncomingPickups.Add(pickup);

    pickup->PickupLocationData.bPlayPickupSound = params->Params_0.bPlayPickupSound;
    pickup->PickupLocationData.FlyTime = 0.4f;
    pickup->PickupLocationData.ItemOwner = MakeWeakPtr(static_cast<AFortPawn*>(pawn));
    pickup->PickupLocationData.PickupGuid = pickup->PrimaryPickupItemEntry.ItemGuid;
    pickup->PickupLocationData.PickupTarget = MakeWeakPtr(static_cast<AFortPawn*>(pawn));
    pickup->OnRep_PickupLocationData();

    pickup->bPickedUp = true;
    pickup->OnRep_bPickedUp();

    Player::InternalPickup((AFortPlayerControllerAthena*)pawn->Controller, pickup->PrimaryPickupItemEntry);
}

void Player::MovingEmoteStopped(UObject* context) {
    auto pawn = (AFortPawn*)context;
    if (!pawn) return;

    pawn->bMovingEmote = false;
    pawn->bMovingEmoteFollowingOnly = false;
}

void Player::InternalPickup(AFortPlayerControllerAthena* pc, FFortItemEntry pickupEntry) {
    if (!pc || !pc->WorldInventory || !pickupEntry.ItemDefinition)
        return;

    int maxStack = (int)Utils::EvaluateScalableFloat(pickupEntry.ItemDefinition->MaxStackSize);
    if (maxStack <= 0) maxStack = 1;

    int itemCount = 0;
    for (auto& item : pc->WorldInventory->Inventory.ReplicatedEntries) {
        if (Inventory::GetQuickbar(item.ItemDefinition) == EEFortQuickBars::Primary)
            itemCount += ((UFortWorldItemDefinition*)item.ItemDefinition)->NumberOfSlotsToTake;
    }

    if (pickupEntry.ItemDefinition->IsStackable()) {
        auto itemEntry = pc->WorldInventory->Inventory.ReplicatedEntries.Search([pickupEntry, maxStack](FFortItemEntry& entry) {
            return entry.ItemDefinition == pickupEntry.ItemDefinition && entry.Count < maxStack;
        });
        if (itemEntry) {
            itemEntry->Count += pickupEntry.Count;
            if (itemEntry->Count > maxStack) {
                int originalCount = itemEntry->Count;
                itemEntry->Count = maxStack;
                if (pickupEntry.ItemDefinition->bAllowMultipleStacks && itemCount < 5)
                    Inventory::GiveItem(pc, pickupEntry, originalCount - maxStack, true);
                else if (pc->MyFortPawn)
                    Inventory::SpawnPickup(pc->MyFortPawn->K2_GetActorLocation(), pickupEntry, EEFortPickupSourceTypeFlag::Player, EEFortPickupSpawnSource::Unset, pc->MyFortPawn, originalCount - maxStack);
            }
            Inventory::ReplaceEntry(pc, *itemEntry);
        } else {
            if (pickupEntry.Count > maxStack) {
                int originalCount = pickupEntry.Count;
                pickupEntry.Count = maxStack;
                if (pickupEntry.ItemDefinition->bAllowMultipleStacks && itemCount < 5)
                    Inventory::GiveItem(pc, pickupEntry, originalCount - maxStack, true);
                else if (pc->MyFortPawn)
                    Inventory::SpawnPickup(pc->MyFortPawn->K2_GetActorLocation(), pickupEntry, EEFortPickupSourceTypeFlag::Player, EEFortPickupSpawnSource::Unset, pc->MyFortPawn, originalCount - maxStack);
            }
            Inventory::GiveItem(pc, pickupEntry, pickupEntry.Count, true);
        }
    } else {
        if (itemCount == 5 && Inventory::GetQuickbar(pickupEntry.ItemDefinition) == EEFortQuickBars::Primary) {
            if (pc->MyFortPawn && pc->MyFortPawn->CurrentWeapon &&
                Inventory::GetQuickbar(pc->MyFortPawn->CurrentWeapon->WeaponData) == EEFortQuickBars::Primary) {
                auto itemEntry = pc->WorldInventory->Inventory.ReplicatedEntries.Search([pc](FFortItemEntry& entry) {
                    return entry.ItemGuid == pc->MyFortPawn->CurrentWeapon->ItemEntryGuid;
                });
                if (itemEntry) {
                    FVector loc = pc->MyFortPawn->K2_GetActorLocation();
                    Inventory::SpawnPickup(loc, *itemEntry, EEFortPickupSourceTypeFlag::Player, EEFortPickupSpawnSource::Unset, pc->MyFortPawn);
                    Inventory::Remove(pc, pc->MyFortPawn->CurrentWeapon->ItemEntryGuid);
                }
                Inventory::GiveItem(pc, pickupEntry, pickupEntry.Count, true);
            } else if (pc->MyFortPawn) {
                Inventory::SpawnPickup(pc->MyFortPawn->K2_GetActorLocation(), pickupEntry, EEFortPickupSourceTypeFlag::Player, EEFortPickupSpawnSource::Unset, pc->MyFortPawn);
            }
        } else {
            Inventory::GiveItem(pc, pickupEntry, pickupEntry.Count, true);
        }
    }
}

void Player::ServerAttemptInventoryDrop(UObject* context, Params::AFortPlayerController_ServerAttemptInventoryDrop* params) {
    auto playerController = (AFortPlayerControllerAthena*)context;

    if (!playerController || !playerController->Pawn)
        return;

    auto itemEntry = playerController->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry) {
        return entry.ItemGuid == params->ItemGuid;
    });
    if (!itemEntry || (itemEntry->Count - params->Count) < 0)
        return;

    itemEntry->Count -= params->Count;

    FVector dropLoc = playerController->Pawn->K2_GetActorLocation()
        + playerController->Pawn->GetActorForwardVector() * 70.f
        + FVector(0, 0, 50);

    Inventory::SpawnPickup(dropLoc, *itemEntry, EEFortPickupSourceTypeFlag::Player, EEFortPickupSpawnSource::Unset, playerController->MyFortPawn, params->Count);

    if (itemEntry->Count == 0)
        Inventory::Remove(playerController, params->ItemGuid);
    else
        Inventory::ReplaceEntry(playerController, *itemEntry);
}

void Player::ServerClientIsReadyToRespawn(UObject* context) {
    auto playerController = (AFortPlayerControllerAthena*)context;
    if (!playerController) return;

    auto playerState = CastSDK<AFortPlayerStateAthena>(playerController->PlayerState);
    if (!playerState) return;

    if (playerState->RespawnData.bRespawnDataAvailable && playerState->RespawnData.bServerIsReady) {
        playerState->RespawnData.bClientIsReady = true;

        CoreUObject::FTransform transform = MakeTransform(playerState->RespawnData.RespawnLocation, playerState->RespawnData.RespawnRotation);
        auto gameMode = UWorld::GetWorld()->AuthorityGameMode;
        if (!gameMode) return;

        auto pawn = (AFortPlayerPawnAthena*)gameMode->SpawnDefaultPawnAtTransform(playerController, transform);
        playerController->Possess(pawn);
        if (pawn) {
            pawn->SetHealth(100);
            pawn->SetShield(100);
            pawn->BeginSkydiving(true);
        }

        playerController->RespawnPlayerAfterDeath(true);
    }
}

void Player::OnCapsuleBeginOverlap(UObject* context, Params::AFortPlayerPawn_OnCapsuleBeginOverlap* params) {
    auto pawn = (AFortPlayerPawn*)context;
    if (!pawn || !pawn->Controller)
        return;

    auto pickup = CastSDK<AFortPickup>(params->OtherActor);
    if (!pickup || !pickup->PrimaryPickupItemEntry.ItemDefinition)
        return;

    auto maxStack = (int)Utils::EvaluateScalableFloat(pickup->PrimaryPickupItemEntry.ItemDefinition->MaxStackSize);
    if (maxStack <= 0) maxStack = 1;

    auto itemEntry = ((AFortPlayerControllerAthena*)pawn->Controller)->WorldInventory->Inventory.ReplicatedEntries.Search([&](FFortItemEntry& entry) {
        return entry.ItemDefinition == pickup->PrimaryPickupItemEntry.ItemDefinition && entry.Count <= maxStack;
    });

    if (pickup->PawnWhoDroppedPickup != pawn) {
        if ((!itemEntry && Inventory::GetQuickbar(pickup->PrimaryPickupItemEntry.ItemDefinition) == EEFortQuickBars::Secondary) || (itemEntry && itemEntry->Count < maxStack)) {
            FVector dir{};
            pawn->ServerHandlePickup(pickup, 0.4f, dir, true);
        }
    }
}

void Player::TeleportPlayerPawn(UObject* context, Params::UFortMissionLibrary_TeleportPlayerPawn* params) {
    if (!params->PlayerPawn) return;

    params->PlayerPawn->K2_TeleportTo(params->DestLocation, params->DestRotation);
    params->ReturnValue = true;
}

void Player::ServerChangeName(UObject* context, Params::APlayerController_ServerChangeName* params) {
    return;
}

static void GiveElimHeal(AFortPlayerPawnAthena* killerPawn) {
    if (!killerPawn) return;

    auto health = killerPawn->GetHealth();
    auto shield = killerPawn->GetShield();

    if (health >= 100) {
        shield += 50;
    } else if (health + 50 > 100) {
        shield += (health + 50) - 100;
        health = 100;
    } else {
        health += 50;
    }

    killerPawn->SetHealth(health);
    killerPawn->SetShield(shield);
}

void Player::ClientOnPawnDied(AFortPlayerControllerAthena* playerController, FFortPlayerDeathReport& deathReport) {
    if (!playerController) {
        if (ClientOnPawnDiedOG) ClientOnPawnDiedOG(playerController, deathReport);
        return;
    }

    auto world = UWorld::GetWorld();
    if (!world) {
        if (ClientOnPawnDiedOG) ClientOnPawnDiedOG(playerController, deathReport);
        return;
    }

    auto gameMode = (AFortGameModeAthena*)world->AuthorityGameMode;
    auto gameState = (AFortGameStateAthena*)world->GameState;
    auto playerState = (AFortPlayerStateAthena*)playerController->PlayerState;

    if (!gameMode || !gameState || !playerState) {
        if (ClientOnPawnDiedOG) ClientOnPawnDiedOG(playerController, deathReport);
        return;
    }

    bool respawningAllowed = gameState->IsRespawningAllowed(playerState);

    if (!respawningAllowed && playerController->WorldInventory && playerController->MyFortPawn) {
        for (auto& entry : playerController->WorldInventory->Inventory.ReplicatedEntries) {
            if (!entry.ItemDefinition) continue;
            if (!entry.ItemDefinition->IsA(UFortWeaponMeleeItemDefinition::StaticClass()) &&
                (entry.ItemDefinition->IsA(UFortResourceItemDefinition::StaticClass()) ||
                    entry.ItemDefinition->IsA(UFortWeaponRangedItemDefinition::StaticClass()) ||
                    entry.ItemDefinition->IsA(UFortConsumableItemDefinition::StaticClass()) ||
                    entry.ItemDefinition->IsA(UFortAmmoItemDefinition::StaticClass()))) {
                Inventory::SpawnPickup(playerController->MyFortPawn->K2_GetActorLocation(), entry, EEFortPickupSourceTypeFlag::Player, EEFortPickupSpawnSource::PlayerElimination, playerController->MyFortPawn);
            }
        }
    }

    auto killerPlayerState = (AFortPlayerStateAthena*)deathReport.KillerPlayerState;
    auto killerPawn = (AFortPlayerPawnAthena*)deathReport.KillerPawn;

    playerState->PawnDeathLocation = playerController->MyFortPawn ? playerController->MyFortPawn->K2_GetActorLocation() : FVector();
    playerState->DeathInfo.bDBNO = playerController->MyFortPawn ? playerController->MyFortPawn->bIsDBNO : false;
    playerState->DeathInfo.DeathLocation = playerState->PawnDeathLocation;
    playerState->DeathInfo.DeathTags = playerController->MyFortPawn ? playerController->MyFortPawn->GameplayTags : deathReport.Tags;
    playerState->DeathInfo.DeathCause = AFortPlayerStateAthena::ToDeathCause(playerState->DeathInfo.DeathTags, playerState->DeathInfo.bDBNO);
    playerState->DeathInfo.Downer = MakeWeakPtr(static_cast<AActor*>(killerPlayerState));
    playerState->DeathInfo.FinisherOrDowner = MakeWeakPtr(static_cast<AActor*>(killerPlayerState ? killerPlayerState : playerState));
    playerState->DeathInfo.Distance = playerController->MyFortPawn ? (playerState->DeathInfo.DeathCause != EEDeathCause::FallDamage ? (killerPawn ? killerPawn->GetDistanceTo(playerController->MyFortPawn) : 0) : ((AFortPlayerPawnAthena*)playerController->MyFortPawn)->LastFallDistance) : 0;
    playerState->DeathInfo.bInitialized = true;
    playerState->OnRep_DeathInfo();

    int playerCount = gameMode->AlivePlayers.Num() - 1;

    if (playerCount == 5 || playerCount == 10 || playerCount == 25) {
        int points = 10;
        if (playerCount == 10)
            points = 15;

        for (auto& player : gameMode->AlivePlayers) {
            if (!player) continue;
            player->ClientReportTournamentPlacementPointsScored(5, points);
        }

        Tournaments::Placement(playerCount, points);
    }

    if (killerPlayerState && killerPawn && killerPawn->Controller && killerPawn->Controller->IsA(AFortPlayerControllerAthena::StaticClass()) && killerPawn->Controller != playerController) {
        killerPlayerState->KillScore++;
        killerPlayerState->OnRep_Kills();
        killerPlayerState->TeamKillScore++;
        killerPlayerState->OnRep_TeamKillScore();

        killerPlayerState->ClientReportKill(playerState);
        killerPlayerState->ClientReportTeamKill(killerPlayerState->TeamKillScore);

        auto killerPC = (AFortPlayerControllerAthena*)killerPlayerState->Owner;
        Tournaments::Kill(killerPC);
        if (bTournament) {
            std::string victimName = playerState->GetPlayerName().ToString();
            Results::SendEventMatchResults(victimName, eventId, { victimName }, {}, "", playerState->Place, playerState->KillScore, 0);
        }
        LOGI("[Player] %s eliminated %s",
            killerPlayerState->GetPlayerName().ToString().c_str(),
            playerController->PlayerState->GetPlayerName().ToString().c_str());

        GiveElimHeal(killerPawn);
    }

    if (!respawningAllowed && (playerController->MyFortPawn ? !playerController->MyFortPawn->bIsDBNO : true)) {
        playerState->Place = gameState->PlayersLeft;
        playerState->OnRep_Place();

        if (playerController->MatchReport) {
            FAthenaMatchStats& stats = playerController->MatchReport->MatchStats;
            FAthenaMatchTeamStats& teamStats = playerController->MatchReport->TeamStats;

            stats.Stats[3] = playerState->KillScore;
            stats.Stats[8] = playerState->SquadId;
            playerController->ClientSendMatchStatsForPlayer(stats);

            teamStats.Place = playerState->Place;
            teamStats.TotalPlayers = gameState->TotalPlayers;
            playerController->ClientSendTeamStatsForPlayer(teamStats);

            playerController->ClientSendEndBattleRoyaleMatchForPlayer(true, playerController->MatchReport->EndOfMatchResults);
        }

        playerController->StateName = MakeFName(L"Spectating");

        if (playerController->MyFortPawn && ((killerPlayerState && killerPlayerState->Place == 1) || playerState->Place == 1)) {
            if (playerState->Place == 1) {
                killerPlayerState = playerState;
                killerPawn = (AFortPlayerPawnAthena*)playerController->MyFortPawn;
            }

            if (killerPlayerState) {
                auto killerPlayerController = (AFortPlayerControllerAthena*)killerPlayerState->Owner;

                if (killerPlayerController) {
                    Tournaments::PlacementForController(killerPlayerController, 1);

                    killerPlayerController->PlayWinEffects(killerPawn, nullptr, playerState->DeathInfo.DeathCause, false);
                    killerPlayerController->ClientNotifyWon(killerPawn, nullptr, playerState->DeathInfo.DeathCause);
                    killerPlayerController->ClientNotifyTeamWon(killerPawn, nullptr, playerState->DeathInfo.DeathCause);

                    gameState->WinningTeam = killerPlayerState->TeamIndex;
                    gameState->OnRep_WinningTeam();
                    gameState->WinningPlayerState = killerPlayerState;
                    gameState->OnRep_WinningPlayerState();

                    if (killerPlayerController != playerController && killerPlayerController->MatchReport) {
                        auto crown = Utils::Find<UFortItemDefinition>(L"/VictoryCrownsGameplay/Items/AGID_VictoryCrown.AGID_VictoryCrown");
                        if (crown) Inventory::GiveItem(killerPlayerController, crown, 1);

                        killerPlayerController->ClientSendEndBattleRoyaleMatchForPlayer(true, killerPlayerController->MatchReport->EndOfMatchResults);

                        FAthenaMatchStats& killerStats = killerPlayerController->MatchReport->MatchStats;
                        FAthenaMatchTeamStats& killerTeamStats = killerPlayerController->MatchReport->TeamStats;

                        killerStats.Stats[3] = killerPlayerState->KillScore;
                        killerStats.Stats[8] = killerPlayerState->SquadId;
                        killerPlayerController->ClientSendMatchStatsForPlayer(killerStats);

                        killerTeamStats.Place = killerPlayerState->Place;
                        killerTeamStats.TotalPlayers = gameState->TotalPlayers;
                        killerPlayerController->ClientSendTeamStatsForPlayer(killerTeamStats);
                    }
                }
            }
        }
    }

    std::thread([playerController]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (playerController && playerController->MyFortPawn) {
            Misc::PlayersToDestroyLocked = true;
            Misc::PlayersToDestroy.push_back(playerController->MyFortPawn);
            Misc::PlayersToDestroyLocked = false;
        }
    }).detach();

    if (ClientOnPawnDiedOG)
        ClientOnPawnDiedOG(playerController, deathReport);
}

void Player::Hook() {
}
