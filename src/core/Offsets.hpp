#pragma once
#include <cstdint>

namespace Off {

constexpr uint64_t GObjects  = 0x0DCD6E08;
constexpr uint64_t GNames    = 0x0DC960C0;
constexpr uint64_t GEngine   = 0x0DE34970;
constexpr uint64_t GWorld    = 0x0DE38568;

constexpr uint64_t GIsEditor = 0x0DC80824;
constexpr uint64_t GIsClient = 0x0DC80832;
constexpr uint64_t GIsServer = 0x0DC80833;

constexpr uint64_t ProcessEvent          = 0x07611B5C;
constexpr int32_t  ProcessEventIdx       = 0x4E;
constexpr uint64_t StaticFindObject      = 0x762A664;
constexpr uint64_t StaticLoadObjectPub   = 0x761C568;
constexpr uint64_t StaticLoadObjectInt   = 0x762CA34;

constexpr uint64_t CreateNetDriver       = 0x98BDE00;
constexpr uint64_t GetWorldContext       = 0x98ACD50;
constexpr uint64_t InitListen            = 0x3852A0C;
constexpr uint64_t InitListenEOS         = 0x4EDBE38;
constexpr uint64_t TickFlush             = 0x952CA44;
constexpr uint64_t GetNetMode            = 0x992F0E0;

constexpr uint64_t ReadyToStartMatch         = 0x99E72B0;
constexpr uint64_t ReadyToEndMatch           = 0x99E7270;
constexpr uint64_t HandleStartingNewPlayer   = 0x99E82C0;
constexpr uint64_t SpawnDefaultPawnFor       = 0x99E790C;
constexpr uint64_t SpawnDefaultPawnAtTransform = 0x6CD58DC;
constexpr uint64_t RestartPlayer             = 0x99E7C24;
constexpr uint64_t ChoosePlayerStart         = 0x99E7EEC;

constexpr uint64_t PickTeam              = 0x550D590;
constexpr uint64_t GameSessionPatch      = 0x5554088;
constexpr uint64_t StartAircraftPhase    = 0x550A1F4;
constexpr uint64_t HandleMatchHasStarted = 0x550DFCC;
constexpr uint64_t EnterAircraft         = 0x5A8227C;
constexpr uint64_t ServerAttemptAircraftJump = 0x6D66788;

constexpr uint64_t ServerExecuteInventoryItem = 0x7057390;
constexpr uint64_t ServerAttemptInventoryDrop = 0x7057EE0;
constexpr uint64_t ServerRemoveInventoryItem  = 0x7057148;
constexpr uint64_t ServerCreateBuildingActor  = 0x7059EAC;
constexpr uint64_t ServerEditBuildingActor    = 0x705A524;
constexpr uint64_t ServerBeginEditingBuilding = 0x705A898;
constexpr uint64_t ServerEndEditingBuilding   = 0x705A7B0;
constexpr uint64_t ServerRepairBuildingActor  = 0x7059C8C;
constexpr uint64_t ServerAcknowledgePossession = 0x9ADDA40;
constexpr uint64_t ServerHandlePickup         = 0x70AB21C;
constexpr uint64_t ServerHandlePickupInfo     = 0x70AB094;

constexpr uint64_t CreateItemEntry          = 0x6EF061C;
constexpr uint64_t SupplyDrop_SpawnPickup   = 0x6CC1C78;
constexpr uint64_t SupplyDrop_SpawnLoot     = 0x6C2BF58;

constexpr uint64_t ClientOnPawnDied         = 0x709C4C8;
constexpr uint64_t ClientReportKill         = 0x70CAF54;
constexpr uint64_t ClientReportTeamKill     = 0x70CAEA4;
constexpr uint64_t IsRespawningAllowed      = 0x6E95D4C;

constexpr uint64_t GiveAbility              = 0x3176514;
constexpr uint64_t ServerTryActivateAbility = 0x3207180;

constexpr uint64_t BuildingActor_OnDamageServer = 0x6B41304;
constexpr uint64_t InitializeBuildingActor      = 0x591C5E4;
constexpr uint64_t ReplaceBuildingActor         = 0x5932630;
constexpr uint64_t SpawnDeco                    = 0x6A91214;
constexpr uint64_t FortDecoTool_ServerSpawnDeco = 0x6E0DE70;

constexpr uint64_t AActor_K2_DestroyActor   = 0x995CA78;
constexpr uint64_t UWorld_SpawnActor        = 0x994C3B0;
constexpr uint64_t UWorld_SpawnActorDeferred = 0x994C784;
constexpr uint64_t GetAllActorsOfClass      = 0x99F5E34;
constexpr uint64_t BeginDeferredActorSpawn  = 0x99F6408;
constexpr uint64_t FinishSpawningActor      = 0x99F62F0;
constexpr uint64_t SpawnObject              = 0x99F67A4;
constexpr uint64_t ExecuteConsoleCommand    = 0x9A78FCC;
constexpr uint64_t GetTimeSeconds           = 0x99EC1D4;

constexpr uint64_t GiveItemToInventoryOwner = 0x6E9C24C;
constexpr uint64_t K2_RemoveItemFromPlayer  = 0x6E9C8B0;
constexpr uint64_t K2_SpawnPickupInWorld    = 0x6E9B2C4;
constexpr uint64_t K2_GetResourceItemDefinition = 0x6E9C44C;
constexpr uint64_t TossPickupFromContainer  = 0x6E9B548;
constexpr uint64_t EvaluateCurveTableRow    = 0x9A73B60;

constexpr uint32_t FNamePool_Blocks         = 0x0040;
constexpr uint32_t FNamePool_ByteCursor     = 0x003C;
constexpr uint32_t FNamePool_BlocksBit      = 0x0010;

constexpr uint32_t FNameEntry_Stride        = 0x0004;
constexpr uint32_t FNameEntry_Header        = 0x0000;
constexpr uint32_t FNameEntry_String        = 0x0004;
constexpr uint32_t FNameEntry_NameWideMask  = 0x0001;
constexpr uint32_t FNameEntry_LengthShift   = 0x0006;

}
