#include "pch.h"
#include "GameMode.hpp"
#include "Misc.hpp"
#include "Abilities.hpp"
#include "API.hpp"
#include "Creative.hpp"
#include "Player.hpp"
#include "Inventory.hpp"
#include "Looting.hpp"
#include "options.h"
#include "Utils.hpp"
#include "UObject.hpp"
#include "FName.hpp"

UFortPlaylistAthena* GameMode::GetPlaylist() {
    if (bDuos)
        return Utils::Find<UFortPlaylistAthena>(L"FortPlaylistAthena Playlist_DefaultDuo.Playlist_DefaultDuo");
    if (bCreative)
        return Utils::Find<UFortPlaylistAthena>(L"/Game/Athena/Playlists/Creative/Playlist_PlaygroundV2.Playlist_PlaygroundV2");
    return Utils::Find<UFortPlaylistAthena>(L"/Game/Athena/Playlists/Showdown/Playlist_ShowdownAlt_Solo.Playlist_ShowdownAlt_Solo");
}

void GameMode::SetPlaylist(AFortGameModeAthena* gameMode) {
    UFortPlaylistAthena* playlist = GetPlaylist();
    if (!playlist) {
        LOGE("[GameMode] Playlist not found");
        return;
    }
    AFortGameStateAthena* gameState = (AFortGameStateAthena*)gameMode->GameState;
    if (!gameState) return;

    gameState->CurrentPlaylistInfo.BasePlaylist = playlist;
    gameState->CurrentPlaylistInfo.OverridePlaylist = playlist;
    gameState->CurrentPlaylistInfo.PlaylistReplicationKey++;
    Utils::MarkArrayDirty(gameState->CurrentPlaylistInfo);

    gameState->CurrentPlaylistId = gameMode->CurrentPlaylistId = playlist->PlaylistId;
    gameMode->CurrentPlaylistName = playlist->PlaylistName;
    gameState->OnRep_CurrentPlaylistInfo();
    gameState->OnRep_CurrentPlaylistId();

    gameMode->GameSession->MaxPlayers = playlist->MaxPlayers;

    gameState->AirCraftBehavior = playlist->AirCraftBehavior;
    gameState->WorldLevel = playlist->LootLevel;
    gameState->CachedSafeZoneStartUp = playlist->SafeZoneStartUp;

    if (bDuos) {
        gameMode->bDBNOEnabled = true;
        gameState->bDBNODeathEnabled = true;
        gameState->SetIsDBNODeathEnabled(true);
    }

    gameMode->AISettings = playlist->AISettings.Get();

    LOGI("[GameMode] Playlist set: %s", gameMode->CurrentPlaylistName.ToString().c_str());
}

static bool bLootInitialized = false;

void GameMode::ReadyToStartMatchHook(UObject* Context, FFrame& Stack, bool* Ret) {
    Stack.IncrementCode();
    if (Ret) *Ret = false;

    auto gameMode = CastSDK<AFortGameModeAthena>(Context);
    if (!gameMode) {
        if (ReadyToStartMatchOG) ReadyToStartMatchOG(Context, Stack, Ret);
        return;
    }

    auto gameState = (AFortGameStateAthena*)gameMode->GameState;
    if (!gameState) return;

    if (gameMode->CurrentPlaylistId == -1) {
        gameMode->WarmupRequiredPlayerCount = 1;
        SetPlaylist(gameMode);

        UFortPlaylistAthena* playlist = GetPlaylist();
        if (!bCreative && playlist) {
            for (auto& level : playlist->AdditionalLevels) {
                bool success = false;
                FString outLevelName;
                ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(UWorld::GetWorld(), level, FVector(), FRotator(), &success, outLevelName, nullptr);
            }
            for (auto& level : playlist->AdditionalLevelsServerOnly) {
                bool success = false;
                FString outLevelName;
                ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(UWorld::GetWorld(), level, FVector(), FRotator(), &success, outLevelName, nullptr);
            }
        }

        if (Ret) *Ret = false;
        return;
    }

    if (!gameMode->bWorldIsReady) {
        int startsNum = 0;
        if (bCreative) {
            startsNum = (int)Utils::GetAll<AFortPlayerStartCreative>().size();
        } else {
            startsNum = (int)Utils::GetAll<AFortPlayerStartWarmup>().size();
        }

        if (startsNum == 0) {
            if (Ret) *Ret = false;
            return;
        }

        if (!bCreative && !gameState->MapInfo) {
            if (Ret) *Ret = false;
            return;
        }

        if (!bLootInitialized) {
            bLootInitialized = true;

            gameState->DefaultParachuteDeployTraceForGroundDistance = 10000;

            auto gas = Utils::Find<UFortAbilitySet>(L"/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer");
            if (gas) AbilitySets.push_back(gas);

            auto addTierData = [&](UDataTable* table) {
                if (!table) return;
                if (auto compositeTable = CastSDK<UCompositeDataTable>(table)) {
                    for (auto parentTable : compositeTable->ParentTables) {
                        if (!parentTable) continue;
                        for (auto& pair : parentTable->RowMap) {
                            FFortLootTierData* row = (FFortLootTierData*)pair.Value();
                            if (row) Looting::TierDataAllGroups.push_back(row);
                        }
                    }
                }
                for (auto& pair : table->RowMap) {
                    FFortLootTierData* row = (FFortLootTierData*)pair.Value();
                    if (row) Looting::TierDataAllGroups.push_back(row);
                }
            };

            auto addPackages = [&](UDataTable* table) {
                if (!table) return;
                if (auto compositeTable = CastSDK<UCompositeDataTable>(table)) {
                    for (auto parentTable : compositeTable->ParentTables) {
                        if (!parentTable) continue;
                        for (auto& pair : parentTable->RowMap) {
                            FFortLootPackageData* row = (FFortLootPackageData*)pair.Value();
                            if (row) Looting::LPGroupsAll.push_back(row);
                        }
                    }
                }
                for (auto& pair : table->RowMap) {
                    FFortLootPackageData* row = (FFortLootPackageData*)pair.Value();
                    if (row) Looting::LPGroupsAll.push_back(row);
                }
            };

            UFortPlaylistAthena* playlist = GetPlaylist();
            auto lootTierData = playlist ? playlist->LootTierData.Get() : nullptr;
            if (!lootTierData)
                lootTierData = Utils::Find<UDataTable>(L"/Game/Items/Datatables/AthenaLootTierData_Client.AthenaLootTierData_Client");
            if (lootTierData)
                addTierData(lootTierData);

            auto lootPackages = playlist ? playlist->LootPackages.Get() : nullptr;
            if (!lootPackages)
                lootPackages = Utils::Find<UDataTable>(L"/Game/Items/Datatables/AthenaLootPackages_Client.AthenaLootPackages_Client");
            if (lootPackages)
                addPackages(lootPackages);

            int numObjects = Sarah::UObjectManager::Num();
            for (int i = 0; i < numObjects; i++) {
                auto object = Sarah::UObjectManager::GetByIndex(i);
                if (!object || !object->Class || object->IsDefaultObject())
                    continue;

                if (auto gameFeatureData = CastSDK<UFortGameFeatureData>(object)) {
                    auto& lootTableData = gameFeatureData->DefaultLootTableData;

                    std::wstring ltdPath = FNameToWString(lootTableData.LootTierData.ObjectID.AssetPathName);
                    std::wstring abilitySetPath = FNameToWString(gameFeatureData->PlayerAbilitySet.ObjectID.AssetPathName);
                    std::wstring lpdPath = FNameToWString(lootTableData.LootPackageData.ObjectID.AssetPathName);

                    auto abilitySet = Utils::Find<UFortAbilitySet>(abilitySetPath.c_str());
                    auto ltdFeatureData = Utils::Find<UDataTable>(ltdPath.c_str());
                    auto lpdFeatureData = Utils::Find<UDataTable>(lpdPath.c_str());

                    if (abilitySet) AbilitySets.push_back(abilitySet);
                    if (ltdFeatureData) addTierData(ltdFeatureData);
                    if (lpdFeatureData) addPackages(lpdFeatureData);
                }
            }

            Looting::SpawnFloorLootForContainer(Utils::Find<UBlueprintGeneratedClass>(L"/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_Warmup.Tiered_Athena_FloorLoot_Warmup_C"));
            Looting::SpawnFloorLootForContainer(Utils::Find<UBlueprintGeneratedClass>(L"/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_01.Tiered_Athena_FloorLoot_01_C"));
        }

        gameMode->bWorldIsReady = true;
        LOGI("[GameMode] World is ready");

        if (!bDev && bGameSessions) {
            API::GameServer(BackendUrl + "/solstice/api/v1/matchmaking/start/by-address", IP, g_Port);
        }
    }

    if (Ret) *Ret = gameMode->AlivePlayers.Num() >= gameMode->WarmupRequiredPlayerCount;
}

void GameMode::HandleStartingNewPlayerHook(UObject* Context, FFrame& Stack) {
    AFortPlayerControllerAthena* NewPlayer = nullptr;
    Stack.StepCompiledIn(&NewPlayer);
    Stack.IncrementCode();

    auto gameMode = (AFortGameModeAthena*)Context;
    if (!gameMode || !NewPlayer) {
        if (HandleStartingNewPlayerOG) HandleStartingNewPlayerOG(Context, Stack);
        return;
    }

    auto gameState = (AFortGameStateAthena*)gameMode->GameState;
    AFortPlayerStateAthena* playerState = (AFortPlayerStateAthena*)NewPlayer->PlayerState;

    if (!playerState || !gameState) {
        if (HandleStartingNewPlayerOG) HandleStartingNewPlayerOG(Context, Stack);
        return;
    }

    playerState->SquadId = playerState->TeamIndex - 3;
    playerState->OnRep_SquadId();

    FGameMemberInfo Member;
    Member.MostRecentArrayReplicationKey = -1;
    Member.ReplicationID = -1;
    Member.ReplicationKey = -1;
    Member.TeamIndex = playerState->TeamIndex;
    Member.SquadId = playerState->SquadId;
    Member.MemberUniqueId = playerState->UniqueId;

    gameState->GameMemberInfoArray.Members.Add(Member);
    Utils::MarkItemDirty(gameState->GameMemberInfoArray, Member);

    StartingCount++;

    if (!NewPlayer->MatchReport) {
        UClass* reportClass = UAthenaPlayerMatchReport::StaticClass();
        if (reportClass) {
            NewPlayer->MatchReport = (UAthenaPlayerMatchReport*)UGameplayStatics::SpawnObject(reportClass, NewPlayer);
            LOGI("[GameMode] MatchReport (newPlayer) = %p", NewPlayer->MatchReport);
        }
    }

    if (HandleStartingNewPlayerOG) HandleStartingNewPlayerOG(Context, Stack);
}

void GameMode::OnAircraftEnteredDropZoneHook(UObject* Context, FFrame& Stack) {
    Stack.IncrementCode();

    if (!bDev && bGameSessions) {
        API::GameServer(BackendUrl + "/solstice/api/v1/matchmaking/stop/by-address", IP, g_Port);
    }

    if (OnAircraftEnteredDropZoneOG) OnAircraftEnteredDropZoneOG(Context, Stack);
}

void GameMode::OnAircraftExitedDropZoneHook(UObject* Context, FFrame& Stack) {
    Stack.IncrementCode();

    auto gameMode = (AFortGameModeAthena*)Context;
    if (!gameMode) {
        if (OnAircraftExitedDropZoneOG) OnAircraftExitedDropZoneOG(Context, Stack);
        return;
    }

    for (auto& player : gameMode->AlivePlayers) {
        if (player && player->IsInAircraft()) {
            auto aircraftComponent = player->GetAircraftComponent();
            if (aircraftComponent) {
                FRotator rot{};
                aircraftComponent->ServerAttemptAircraftJump(rot);
            }
        }
    }

    if (OnAircraftExitedDropZoneOG) OnAircraftExitedDropZoneOG(Context, Stack);
}

APawn* GameMode::SpawnDefaultPawnFor(AGameModeBase* context, AController* newPlayer, AActor* startSpot) {
    auto gameMode = (AFortGameModeAthena*)context;
    if (!gameMode) return nullptr;

    CoreUObject::FTransform transform;
    if (startSpot) {
        transform = MakeTransform(startSpot->K2_GetActorLocation(), startSpot->K2_GetActorRotation());
    } else {
        transform = MakeTransform(FVector());
    }

    APawn* pawn = gameMode->SpawnDefaultPawnAtTransform(newPlayer, transform);
    if (!pawn) return nullptr;

    auto playerController = CastSDK<AFortPlayerController>(newPlayer);
    if (!playerController) return pawn;

    if (!playerController->WorldInventory) return pawn;

    auto num = playerController->WorldInventory->Inventory.ReplicatedEntries.Num();
    if (num != 0) {
        playerController->WorldInventory->Inventory.ReplicatedEntries.ResetNum();
        playerController->WorldInventory->Inventory.ItemInstances.ResetNum();
    }

    if (playerController->CosmeticLoadoutPC.Pickaxe) {
        Inventory::GiveItem(playerController, playerController->CosmeticLoadoutPC.Pickaxe->WeaponDefinition);
    }

    for (auto& startingItem : gameMode->StartingItems) {
        if (startingItem.Count && startingItem.Item && !startingItem.Item->IsA(UFortSmartBuildingItemDefinition::StaticClass())) {
            Inventory::GiveItem(playerController, startingItem.Item, startingItem.Count);
        }
    }

    if (num == 0) {
        auto playerState = (AFortPlayerStateAthena*)playerController->PlayerState;
        if (!playerState) return pawn;

        for (auto& abilitySet : AbilitySets) {
            if (abilitySet && playerState->AbilitySystemComponent) {
                Abilities::GiveAbilitySet(playerState->AbilitySystemComponent, abilitySet);
            }
        }

        auto athenaController = (AFortPlayerControllerAthena*)playerController;
        if (athenaController->XPComponent) {
            playerState->SeasonLevelUIDisplay = athenaController->XPComponent->CurrentLevel;
            playerState->OnRep_SeasonLevelUIDisplay();
        }
    }

    if (auto athenaController = CastSDK<AFortPlayerControllerAthena>(playerController)) {
        if (!athenaController->MatchReport) {
            UClass* reportClass = UAthenaPlayerMatchReport::StaticClass();
            if (reportClass) {
                athenaController->MatchReport = (UAthenaPlayerMatchReport*)UGameplayStatics::SpawnObject(reportClass, athenaController);
                LOGI("[GameMode] MatchReport (pawn) = %p", athenaController->MatchReport);
            }
        }
    }

    LOGI("[GameMode] Spawned pawn = %p", pawn);
    return pawn;
}

EEFortTeam GameMode::PickTeam(AFortGameModeAthena* gameMode, uint8_t preferredTeam, AFortPlayerControllerAthena* controller) {
    uint8_t ret = CurrentTeam;

    auto gameState = (AFortGameStateAthena*)UWorld::GetWorld()->GameState;
    int maxSquad = 1;
    if (gameState && gameState->CurrentPlaylistInfo.BasePlaylist)
        maxSquad = gameState->CurrentPlaylistInfo.BasePlaylist->MaxSquadSize;

    if (maxSquad <= 0) maxSquad = 1;

    if (++PlayersOnCurTeam >= maxSquad) {
        CurrentTeam++;
        PlayersOnCurTeam = 0;
    }

    return (EEFortTeam)ret;
}

void GameMode::Hook() {
    Utils::ExecHook(L"/Script/Engine.GameMode.ReadyToStartMatch",
                    (void*)ReadyToStartMatchHook, ReadyToStartMatchOG);
    Utils::ExecHook(L"/Script/Engine.GameModeBase.HandleStartingNewPlayer",
                    (void*)HandleStartingNewPlayerHook, HandleStartingNewPlayerOG);
    Utils::ExecHook(L"/Script/FortniteGame.FortGameModeAthena.OnAircraftEnteredDropZone",
                    (void*)OnAircraftEnteredDropZoneHook, OnAircraftEnteredDropZoneOG);
    Utils::ExecHook(L"/Script/FortniteGame.FortGameModeAthena.OnAircraftExitedDropZone",
                    (void*)OnAircraftExitedDropZoneHook, OnAircraftExitedDropZoneOG);
}
