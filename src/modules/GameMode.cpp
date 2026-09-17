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

static UFunction* g_ReadyToStartMatch = nullptr;
static UFunction* g_HandleStartingNewPlayer = nullptr;
static UFunction* g_OnAircraftEnteredDropZone = nullptr;
static UFunction* g_OnAircraftExitedDropZone = nullptr;

static UFunction* GetReadyToStartMatch() {
    if (!g_ReadyToStartMatch)
        g_ReadyToStartMatch = (UFunction*)Utils::FindObject(L"/Script/Engine.GameMode.ReadyToStartMatch");
    return g_ReadyToStartMatch;
}

static UFunction* GetHandleStartingNewPlayer() {
    if (!g_HandleStartingNewPlayer)
        g_HandleStartingNewPlayer = (UFunction*)Utils::FindObject(L"/Script/Engine.GameModeBase.HandleStartingNewPlayer");
    return g_HandleStartingNewPlayer;
}

static UFunction* GetOnAircraftEnteredDropZone() {
    if (!g_OnAircraftEnteredDropZone)
        g_OnAircraftEnteredDropZone = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortGameModeAthena.OnAircraftEnteredDropZone");
    return g_OnAircraftEnteredDropZone;
}

static UFunction* GetOnAircraftExitedDropZone() {
    if (!g_OnAircraftExitedDropZone)
        g_OnAircraftExitedDropZone = (UFunction*)Utils::FindObject(L"/Script/FortniteGame.FortGameModeAthena.OnAircraftExitedDropZone");
    return g_OnAircraftExitedDropZone;
}

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

static bool bReady = false;
static bool bLootInitialized = false;

void GameMode::ReadyToStartMatch(UObject* context, Params::AGameMode_ReadyToStartMatch* params) {
    params->ReturnValue = false;

    auto gameMode = CastSDK<AFortGameModeAthena>(context);
    if (!gameMode) {
        UFunction* pfn = GetReadyToStartMatch();
        if (pfn) Sarah::CallProcessEvent(context, pfn, params);
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

        params->ReturnValue = false;
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
            params->ReturnValue = false;
            return;
        }

        if (!bCreative && !gameState->MapInfo) {
            params->ReturnValue = false;
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

    params->ReturnValue = gameMode->AlivePlayers.Num() >= gameMode->WarmupRequiredPlayerCount;
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

void GameMode::HandleStartingNewPlayer(UObject* context, Params::AGameModeBase_HandleStartingNewPlayer* params) {
    auto gameMode = (AFortGameModeAthena*)context;
    auto newPlayer = (AFortPlayerControllerAthena*)params->NewPlayer;

    UFunction* pfn = GetHandleStartingNewPlayer();

    if (!gameMode || !newPlayer) {
        if (pfn) Sarah::CallProcessEvent(context, pfn, params);
        return;
    }

    auto gameState = (AFortGameStateAthena*)gameMode->GameState;
    AFortPlayerStateAthena* playerState = (AFortPlayerStateAthena*)newPlayer->PlayerState;

    if (!playerState || !gameState) {
        if (pfn) Sarah::CallProcessEvent(context, pfn, params);
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
    gameState->GameMemberInfoArray.MarkItemDirty(Member);

    StartingCount++;

    if (!newPlayer->MatchReport) {
        UClass* reportClass = UAthenaPlayerMatchReport::StaticClass();
        if (reportClass) {
            newPlayer->MatchReport = (UAthenaPlayerMatchReport*)UGameplayStatics::SpawnObject(reportClass, newPlayer);
            LOGI("[GameMode] MatchReport (newPlayer) = %p", newPlayer->MatchReport);
        } else {
            LOGE("[GameMode] MatchReport StaticClass is null");
        }
    }

    if (pfn) Sarah::CallProcessEvent(context, pfn, params);
}

void GameMode::OnAircraftEnteredDropZone(UObject* context, Params::AFortGameModeAthena_OnAircraftEnteredDropZone* params) {
    if (!bDev && bGameSessions) {
        API::GameServer(BackendUrl + "/solstice/api/v1/matchmaking/stop/by-address", IP, g_Port);
    }

    UFunction* pfn = GetOnAircraftEnteredDropZone();
    if (pfn) Sarah::CallProcessEvent(context, pfn, params);
}

void GameMode::OnAircraftExitedDropZone(UObject* context, Params::AFortGameModeAthena_OnAircraftExitedDropZone* params) {
    auto gameMode = (AFortGameModeAthena*)context;
    if (!gameMode) return;

    for (auto& player : gameMode->AlivePlayers) {
        if (player && player->IsInAircraft()) {
            auto aircraftComponent = player->GetAircraftComponent();
            if (aircraftComponent) {
                FRotator rot{};
                aircraftComponent->ServerAttemptAircraftJump(rot);
            }
        }
    }

    UFunction* pfn = GetOnAircraftExitedDropZone();
    if (pfn) Sarah::CallProcessEvent(context, pfn, params);
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
}
