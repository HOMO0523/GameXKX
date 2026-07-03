#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveGame.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSaveGameSlotRoundTripTest,
	"GameXXK.MVP.SaveGame.SlotRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSaveGameSlotRoundTripTest::RunTest(const FString& Parameters)
{
	const FString RoundTripSlot = TEXT("GameXXK_MVP_Automation_SaveGame_FullStateRoundTrip");
	const FString FollowerSlot = TEXT("GameXXK_MVP_Automation_SaveGame_QuestNpcFollower");
	const FString NewGameSlot = TEXT("GameXXK_MVP_Automation_SaveGame_NewGame");
	const FString DeleteSlot = TEXT("GameXXK_MVP_Automation_SaveGame_Delete");
	const int32 UserIndex = 0;

	UGameplayStatics::DeleteGameInSlot(RoundTripSlot, UserIndex);
	UGameplayStatics::DeleteGameInSlot(FollowerSlot, UserIndex);
	UGameplayStatics::DeleteGameInSlot(NewGameSlot, UserIndex);
	UGameplayStatics::DeleteGameInSlot(DeleteSlot, UserIndex);

	UGameInstance* SourceGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* SourceSubsystem = NewObject<UGameXXKMVPSubsystem>(SourceGameInstance);
	TestNotNull(TEXT("source subsystem exists"), SourceSubsystem);

	FGameXXKRuntimeState& SourceState = SourceSubsystem->GetMutableRuntimeState();
	SourceState.Screen = EGameXXKScreen::Battle;
	SourceState.QuestState = EGameXXKQuestState::Completed;
	SourceState.CurrentRegion = UGameXXKMVPRules::RegionHuangshan();
	SourceState.PlayerLevel = 3;
	SourceState.PlayerXP = 42;
	SourceState.PlayerGold = 123;
	SourceState.PlayerHP = 37;
	SourceState.PlayerMaxHP = 140;
	SourceState.PlayerAttack = 33;
	SourceState.PlayerDefense = 17;
	SourceState.PlayerSpeed = 14;
	SourceState.bFollowerJoined = true;
	SourceState.bDungeonActive = true;
	SourceState.DungeonNodeIndex = 2;
	SourceState.CurrentMapId = TEXT("HuangshanRoute");
	SourceState.bHasGeneratedRouteMap = true;
	SourceState.RouteSeed = 17;
	SourceState.RouteMapNodes.Add(FGameXXKRouteMapNode{0, 0, 0, EGameXXKNodeKind::Start, FVector2D(0.5f, 0.0f), TArray<int32>{1, 2}});
	SourceState.RouteMapNodes.Add(FGameXXKRouteMapNode{1, 1, 0, EGameXXKNodeKind::Battle, FVector2D(0.35f, 0.2f), TArray<int32>{3}});
	SourceState.RouteMapNodes.Add(FGameXXKRouteMapNode{2, 1, 1, EGameXXKNodeKind::Event, FVector2D(0.65f, 0.2f), TArray<int32>{3}});
	SourceState.RouteMapEdges.Add(FGameXXKRouteMapEdge{0, 1});
	SourceState.RouteMapEdges.Add(FGameXXKRouteMapEdge{0, 2});
	SourceState.RouteMapEdges.Add(FGameXXKRouteMapEdge{1, 3});
	SourceState.CurrentRouteNodeId = INDEX_NONE;
	SourceState.PendingRouteNodeId = 1;
	SourceState.VisitedRouteNodeIds = TArray<int32>{0};
	SourceState.ReachableRouteNodeIds = TArray<int32>{1, 2};
	SourceState.bHasPlayerLocation = true;
	SourceState.PlayerLocation = FVector(120.0f, -34.0f, 88.0f);
	SourceState.UnlockedRegions.Add(UGameXXKMVPRules::RegionTanjiang());
	SourceState.Inventory.Add(UGameXXKMVPRules::ItemIronSword(), 1);
	SourceState.Inventory.Add(UGameXXKMVPRules::ItemHealingPowder(), 2);
	SourceState.EquippedWeapon = UGameXXKMVPRules::ItemIronSword();

	const FGameXXKSaveState DirectSaveState = UGameXXKMVPRules::MakeSaveState(SourceState);
	TestTrue(TEXT("save state mirrors player location flag for slot previews and probes"), DirectSaveState.bHasPlayerLocation);
	TestEqual(TEXT("save state mirrors player location for slot previews and probes"), DirectSaveState.PlayerLocation, FVector(120.0f, -34.0f, 88.0f));

	TestEqual(TEXT("manual save slot count is five"), UGameXXKMVPSubsystem::GetManualSaveSlotCount(), 5);
	TestEqual(TEXT("manual save slot 0 name"), UGameXXKMVPSubsystem::GetManualSaveSlotName(0), FString(TEXT("GameXXK_MVP_SaveSlot_1")));
	TestEqual(TEXT("manual save slot 4 name"), UGameXXKMVPSubsystem::GetManualSaveSlotName(4), FString(TEXT("GameXXK_MVP_SaveSlot_5")));

	TestTrue(TEXT("custom slot save succeeds"), SourceSubsystem->SaveCurrentGame(RoundTripSlot, UserIndex));
	TestTrue(TEXT("custom slot exists after save"), UGameplayStatics::DoesSaveGameExist(RoundTripSlot, UserIndex));

	UGameInstance* LoadedGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* LoadedSubsystem = NewObject<UGameXXKMVPSubsystem>(LoadedGameInstance);
	TestNotNull(TEXT("loaded subsystem exists"), LoadedSubsystem);
	TestTrue(TEXT("continue custom slot succeeds"), LoadedSubsystem->ContinueGameFromSlot(RoundTripSlot, UserIndex));

	const FGameXXKRuntimeState& LoadedState = LoadedSubsystem->GetRuntimeState();
	TestEqual(TEXT("continue restores saved screen"), LoadedState.Screen, EGameXXKScreen::Battle);
	TestEqual(TEXT("quest completion persists"), LoadedState.QuestState, EGameXXKQuestState::Completed);
	TestEqual(TEXT("current region persists"), LoadedState.CurrentRegion, UGameXXKMVPRules::RegionHuangshan());
	TestEqual(TEXT("player level persists"), LoadedState.PlayerLevel, 3);
	TestEqual(TEXT("player XP persists"), LoadedState.PlayerXP, 42);
	TestEqual(TEXT("gold persists"), LoadedState.PlayerGold, 123);
	TestEqual(TEXT("HP persists"), LoadedState.PlayerHP, 37);
	TestEqual(TEXT("max HP persists"), LoadedState.PlayerMaxHP, 140);
	TestEqual(TEXT("attack persists"), LoadedState.PlayerAttack, 33);
	TestEqual(TEXT("defense persists"), LoadedState.PlayerDefense, 17);
	TestEqual(TEXT("speed persists"), LoadedState.PlayerSpeed, 14);
	TestTrue(TEXT("follower flag persists"), LoadedState.bFollowerJoined);
	TestTrue(TEXT("dungeon active persists"), LoadedState.bDungeonActive);
	TestEqual(TEXT("dungeon node index persists"), LoadedState.DungeonNodeIndex, 2);
	TestEqual(TEXT("current map id persists"), LoadedState.CurrentMapId, FName(TEXT("HuangshanRoute")));
	TestTrue(TEXT("generated route map flag persists"), LoadedState.bHasGeneratedRouteMap);
	TestEqual(TEXT("route seed persists"), LoadedState.RouteSeed, 17);
	TestEqual(TEXT("route nodes persist"), LoadedState.RouteMapNodes.Num(), 3);
	TestEqual(TEXT("route edges persist"), LoadedState.RouteMapEdges.Num(), 3);
	TestEqual(TEXT("pending route node persists"), LoadedState.PendingRouteNodeId, 1);
	TestTrue(TEXT("visited route node persists"), LoadedState.VisitedRouteNodeIds.Contains(0));
	TestTrue(TEXT("reachable route branch persists"), LoadedState.ReachableRouteNodeIds.Contains(2));
	TestTrue(TEXT("player location flag persists"), LoadedState.bHasPlayerLocation);
	TestEqual(TEXT("player location persists"), LoadedState.PlayerLocation, FVector(120.0f, -34.0f, 88.0f));
	TestTrue(TEXT("Tanjiang unlock persists"), LoadedState.UnlockedRegions.Contains(UGameXXKMVPRules::RegionTanjiang()));
	TestEqual(TEXT("inventory item count persists"), LoadedState.Inventory.FindRef(UGameXXKMVPRules::ItemIronSword()), 1);
	TestEqual(TEXT("inventory consumable count persists"), LoadedState.Inventory.FindRef(UGameXXKMVPRules::ItemHealingPowder()), 2);
	TestEqual(TEXT("equipment persists"), LoadedState.EquippedWeapon, UGameXXKMVPRules::ItemIronSword());

	UGameInstance* FollowerSourceGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* FollowerSourceSubsystem = NewObject<UGameXXKMVPSubsystem>(FollowerSourceGameInstance);
	FGameXXKRuntimeState& FollowerSourceState = FollowerSourceSubsystem->GetMutableRuntimeState();
	const FVector SavedQuestNpcLocation(321.0f, -48.0f, 72.0f);
	FollowerSourceState.QuestState = EGameXXKQuestState::Accepted;
	FollowerSourceState.bFollowerJoined = true;
	FollowerSourceState.bHasQuestNpcLocation = true;
	FollowerSourceState.QuestNpcLocation = SavedQuestNpcLocation;
	TestTrue(TEXT("quest NPC follower slot save succeeds"), FollowerSourceSubsystem->SaveCurrentGame(FollowerSlot, UserIndex));

	UGameInstance* LoadedFollowerGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* LoadedFollowerSubsystem = NewObject<UGameXXKMVPSubsystem>(LoadedFollowerGameInstance);
	TestTrue(TEXT("quest NPC follower slot load succeeds"), LoadedFollowerSubsystem->LoadGameFromSlot(FollowerSlot, UserIndex));
	const FGameXXKRuntimeState& LoadedFollowerState = LoadedFollowerSubsystem->GetRuntimeState();
	TestEqual(TEXT("quest NPC follower save restores accepted quest"), LoadedFollowerState.QuestState, EGameXXKQuestState::Accepted);
	TestTrue(TEXT("quest NPC follower save restores follower joined"), LoadedFollowerState.bFollowerJoined);
	TestTrue(TEXT("quest NPC follower save restores location flag"), LoadedFollowerState.bHasQuestNpcLocation);
	TestEqual(TEXT("quest NPC follower save restores task NPC location"), LoadedFollowerState.QuestNpcLocation, SavedQuestNpcLocation);

	FGameXXKSaveState AcceptedWithoutFollowerSaveState;
	AcceptedWithoutFollowerSaveState.QuestState = EGameXXKQuestState::Accepted;
	AcceptedWithoutFollowerSaveState.bFollowerJoined = false;
	const FGameXXKRuntimeState AcceptedWithoutFollowerRuntimeState = UGameXXKMVPRules::RestoreFromSaveState(AcceptedWithoutFollowerSaveState);
	TestEqual(TEXT("legacy accepted quest save restores accepted quest state"), AcceptedWithoutFollowerRuntimeState.QuestState, EGameXXKQuestState::Accepted);
	TestFalse(TEXT("legacy accepted quest save without follower flag does not auto-join task NPC"), AcceptedWithoutFollowerRuntimeState.bFollowerJoined);

	UGameInstance* StartGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* StartGameSubsystem = NewObject<UGameXXKMVPSubsystem>(StartGameInstance);
	TestTrue(TEXT("StartGame starts a fresh new game"), StartGameSubsystem->StartGame());
	TestEqual(TEXT("StartGame opens world map for new game"), StartGameSubsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestFalse(TEXT("StartGame does not continue saved Tanjiang unlock"), StartGameSubsystem->IsRegionUnlocked(UGameXXKMVPRules::RegionTanjiang()));

	UGameInstance* ContinueGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* ContinueSubsystem = NewObject<UGameXXKMVPSubsystem>(ContinueGameInstance);
	TestTrue(TEXT("StartGameFromSlot continues saved slot"), ContinueSubsystem->StartGameFromSlot(RoundTripSlot, UserIndex));
	TestEqual(TEXT("StartGameFromSlot keeps saved screen"), ContinueSubsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("StartGameFromSlot keeps persisted Tanjiang unlock"), ContinueSubsystem->IsRegionUnlocked(UGameXXKMVPRules::RegionTanjiang()));

	UGameInstance* NewGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* NewGameSubsystem = NewObject<UGameXXKMVPSubsystem>(NewGameInstance);
	TestFalse(TEXT("Continue missing slot fails"), NewGameSubsystem->ContinueGameFromSlot(NewGameSlot, UserIndex));
	TestTrue(TEXT("explicit new game starts missing slot state"), NewGameSubsystem->StartGame());
	TestEqual(TEXT("new game opens world map"), NewGameSubsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestTrue(TEXT("new game unlocks Qingshan"), NewGameSubsystem->IsRegionUnlocked(UGameXXKMVPRules::RegionQingshan()));
	TestFalse(TEXT("new game keeps Tanjiang locked"), NewGameSubsystem->IsRegionUnlocked(UGameXXKMVPRules::RegionTanjiang()));

	TestFalse(TEXT("delete slot starts absent"), SourceSubsystem->DoesSaveGameExist(DeleteSlot, UserIndex));
	TestTrue(TEXT("save delete slot succeeds"), SourceSubsystem->SaveCurrentGame(DeleteSlot, UserIndex));
	TestTrue(TEXT("delete slot exists after save"), SourceSubsystem->DoesSaveGameExist(DeleteSlot, UserIndex));
	TestTrue(TEXT("delete slot succeeds"), SourceSubsystem->DeleteSaveGame(DeleteSlot, UserIndex));
	TestFalse(TEXT("delete slot absent after delete"), SourceSubsystem->DoesSaveGameExist(DeleteSlot, UserIndex));

	UGameplayStatics::DeleteGameInSlot(RoundTripSlot, UserIndex);
	UGameplayStatics::DeleteGameInSlot(FollowerSlot, UserIndex);
	UGameplayStatics::DeleteGameInSlot(NewGameSlot, UserIndex);
	UGameplayStatics::DeleteGameInSlot(DeleteSlot, UserIndex);

	return true;
}

#endif
