#include "GameXXKMVPRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKRouteEconomyRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveGame.h"
#include "MVP/GameXXKSaveMigration.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString BackupSlotFor(const FString& MainSlot)
	{
		return FString::Printf(
			TEXT("%s.PreV%dBackup"),
			*MainSlot,
			FGameXXKSaveMigration::CurrentSaveVersion);
	}

	FString BackupAttemptSlotFor(const FString& MainSlot, const int32 AttemptNumber)
	{
		return FString::Printf(TEXT("%s.%03d"), *BackupSlotFor(MainSlot), AttemptNumber);
	}

	int32 RouteEconomyDependencySaveVersion()
	{
		return FGameXXKSaveMigration::RouteEconomyIntroducedSaveVersion - 1;
	}

	void DeleteMainAndBackup(const FString& MainSlot, const int32 UserIndex)
	{
		UGameplayStatics::DeleteGameInSlot(MainSlot, UserIndex);
		UGameplayStatics::DeleteGameInSlot(BackupSlotFor(MainSlot), UserIndex);
		for (int32 AttemptNumber = 1; AttemptNumber <= 8; ++AttemptNumber)
		{
			UGameplayStatics::DeleteGameInSlot(BackupAttemptSlotFor(MainSlot, AttemptNumber), UserIndex);
		}
	}

	UGameXXKSaveGame* MakeVersionedSaveObject(const int32 Version, const int32 GoldSentinel)
	{
		UGameXXKSaveGame* SaveGame = NewObject<UGameXXKSaveGame>();
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.PlayerGold = GoldSentinel;
		SaveGame->SaveState = UGameXXKMVPRules::MakeSaveState(State);
		SaveGame->SaveState.SaveVersion = Version;
		return SaveGame;
	}

	UGameXXKSaveGame* LoadTypedSave(const FString& SlotName, const int32 UserIndex)
	{
		return Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	}

	TArray<uint8> SerializeRuntimeState(const FGameXXKRuntimeState& Source)
	{
		FGameXXKRuntimeState Copy = Source;
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		FGameXXKRuntimeState::StaticStruct()->SerializeItem(Writer, &Copy, nullptr);
		return Bytes;
	}

	TArray<uint8> SerializeSaveObject(USaveGame* SaveGame)
	{
		TArray<uint8> Bytes;
		UGameplayStatics::SaveGameToMemory(SaveGame, Bytes);
		return Bytes;
	}

	bool RuntimeStatesEqual(const FGameXXKRuntimeState& Left, const FGameXXKRuntimeState& Right)
	{
		return FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&Left, &Right, PPF_None);
	}

	bool BuildActiveHudFixture(UGameXXKMVPSubsystem* Subsystem, FGameXXKRuntimeState* OutRawState = nullptr)
	{
		if (!Subsystem || !Subsystem->StartGame()
			|| !Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan())
			|| !Subsystem->AcceptQuest()
			|| !Subsystem->OpenDungeonFromTownExit()
			|| !Subsystem->SelectDungeonNode(EGameXXKNodeKind::Start)
			|| !Subsystem->SelectDungeonNode(EGameXXKNodeKind::Battle))
		{
			return false;
		}
		if (OutRawState)
		{
			*OutRawState = Subsystem->GetRuntimeStateCopy();
		}
		FString Error;
		return Subsystem->ApplyBattleHudFixtureForTest(Error);
	}
}

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

	DeleteMainAndBackup(RoundTripSlot, UserIndex);
	DeleteMainAndBackup(FollowerSlot, UserIndex);
	DeleteMainAndBackup(NewGameSlot, UserIndex);
	DeleteMainAndBackup(DeleteSlot, UserIndex);

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
	SourceState.bFollowerJoined = true;
	SourceState.bDungeonActive = true;
	SourceState.DungeonNodeIndex = 2;
	SourceState.CurrentMapId = TEXT("HuangshanRoute");
	SourceState.bHasGeneratedRouteMap = true;
	SourceState.RouteSeed = 17;
	SourceState.RouteMapNodes.Add(FGameXXKRouteMapNode{0, 0, 0, EGameXXKNodeKind::Start, FVector2D(0.5f, 0.0f), TArray<int32>{1, 2}});
	SourceState.RouteMapNodes.Add(FGameXXKRouteMapNode{1, 1, 0, EGameXXKNodeKind::Battle, FVector2D(0.35f, 0.2f), TArray<int32>{3}});
	SourceState.RouteMapNodes.Add(FGameXXKRouteMapNode{2, 1, 1, EGameXXKNodeKind::Event, FVector2D(0.65f, 0.2f), TArray<int32>{3}});
	SourceState.RouteMapNodes.Add(FGameXXKRouteMapNode{3, 2, 0, EGameXXKNodeKind::Boss, FVector2D(0.5f, 0.4f), TArray<int32>{}});
	SourceState.RouteMapEdges.Add(FGameXXKRouteMapEdge{0, 1});
	SourceState.RouteMapEdges.Add(FGameXXKRouteMapEdge{0, 2});
	SourceState.RouteMapEdges.Add(FGameXXKRouteMapEdge{1, 3});
	SourceState.RouteMapEdges.Add(FGameXXKRouteMapEdge{2, 3});
	SourceState.CurrentRouteNodeId = INDEX_NONE;
	SourceState.PendingRouteNodeId = 1;
	SourceState.VisitedRouteNodeIds = TArray<int32>{0};
	SourceState.ReachableRouteNodeIds = TArray<int32>{1, 2};
	TestTrue(
		TEXT("round-trip fixture initializes route economy"),
		FGameXXKRouteEconomyRules::InitializeRoute(SourceState.CardRun, 60));
	bool bSourceTravelMoneyAwarded = false;
	TestTrue(
		TEXT("round-trip fixture records a chapter-scoped travel-money receipt"),
		FGameXXKRouteEconomyRules::AwardNodeOnce(
			SourceState.CardRun,
			2,
			42,
			35,
			bSourceTravelMoneyAwarded));
	TestTrue(TEXT("round-trip fixture reports its travel-money award"), bSourceTravelMoneyAwarded);
	SourceState.bHasPlayerLocation = true;
	SourceState.PlayerLocation = FVector(120.0f, -34.0f, 88.0f);
	SourceState.UnlockedRegions.Add(UGameXXKMVPRules::RegionTanjiang());
	SourceState.Inventory.Add(UGameXXKMVPRules::ItemHealingPowder(), 2);
	const FGameXXKEquipmentInstance* StarterWeapon = SourceState.EquipmentCollection.EquipmentInstances.FindByPredicate(
		[](const FGameXXKEquipmentInstance& Instance)
		{
			return Instance.BaseEquipmentId == TEXT("Equipment.Starter.Weapon");
		});
	if (!TestNotNull(TEXT("round-trip fixture exposes the modern starter weapon"), StarterWeapon))
	{
		return false;
	}
	const FName ExpectedStarterWeaponInstanceId = StarterWeapon->InstanceId;
	const FName ExpectedStarterWeaponBaseId = StarterWeapon->BaseEquipmentId;
	FGameXXKEquipmentTransactionResult EquipResult;
	TestTrue(
		TEXT("round-trip fixture equips the modern starter weapon"),
		FGameXXKEquipmentEconomyRules::Equip(
			SourceState,
			FGameXXKEquipmentRules::HeroCharacterId(),
			EGameXXKEquipmentSlot::Weapon,
			ExpectedStarterWeaponInstanceId,
			EquipResult));
	const int32 ExpectedPlayerHP = SourceState.PlayerHP;
	const int32 ExpectedPlayerMaxHP = SourceState.PlayerMaxHP;
	const int32 ExpectedPlayerAttack = SourceState.PlayerAttack;
	const int32 ExpectedPlayerDefense = SourceState.PlayerDefense;
	const int32 ExpectedPlayerSpeed = SourceState.PlayerSpeed;

	const FGameXXKSaveState DirectSaveState = UGameXXKMVPRules::MakeSaveState(SourceState);
	TestEqual(TEXT("save state writes the current version"), DirectSaveState.SaveVersion, FGameXXKSaveMigration::CurrentSaveVersion);
	TestTrue(TEXT("save state mirrors player location flag for slot previews and probes"), DirectSaveState.bHasPlayerLocation);
	TestEqual(TEXT("save state mirrors player location for slot previews and probes"), DirectSaveState.PlayerLocation, FVector(120.0f, -34.0f, 88.0f));
	FGameXXKSaveState DirectValidatedState;
	FGameXXKSaveMigrationReport DirectValidationReport;
	const bool bDirectStateValid = FGameXXKSaveMigration::MigrateToCurrent(
		DirectSaveState,
		DirectValidatedState,
		DirectValidationReport);
	TestTrue(
		FString::Printf(TEXT("the direct current-version save validates before slot I/O: %s"), *DirectValidationReport.Error),
		bDirectStateValid);

	TestEqual(TEXT("manual save slot count is five"), UGameXXKMVPSubsystem::GetManualSaveSlotCount(), 5);
	TestEqual(TEXT("manual save slot 0 name"), UGameXXKMVPSubsystem::GetManualSaveSlotName(0), FString(TEXT("GameXXK_MVP_SaveSlot_1")));
	TestEqual(TEXT("manual save slot 4 name"), UGameXXKMVPSubsystem::GetManualSaveSlotName(4), FString(TEXT("GameXXK_MVP_SaveSlot_5")));

	TestTrue(TEXT("custom slot save succeeds"), SourceSubsystem->SaveCurrentGame(RoundTripSlot, UserIndex));
	TestTrue(TEXT("custom slot exists after save"), UGameplayStatics::DoesSaveGameExist(RoundTripSlot, UserIndex));
	TestFalse(TEXT("current-version save creates no migration backup"), UGameplayStatics::DoesSaveGameExist(BackupSlotFor(RoundTripSlot), UserIndex));

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
	TestEqual(TEXT("HP persists"), LoadedState.PlayerHP, ExpectedPlayerHP);
	TestEqual(TEXT("max HP persists"), LoadedState.PlayerMaxHP, ExpectedPlayerMaxHP);
	TestEqual(TEXT("attack persists"), LoadedState.PlayerAttack, ExpectedPlayerAttack);
	TestEqual(TEXT("defense persists"), LoadedState.PlayerDefense, ExpectedPlayerDefense);
	TestEqual(TEXT("speed persists"), LoadedState.PlayerSpeed, ExpectedPlayerSpeed);
	TestTrue(TEXT("follower flag persists"), LoadedState.bFollowerJoined);
	TestTrue(TEXT("dungeon active persists"), LoadedState.bDungeonActive);
	TestEqual(TEXT("dungeon node index persists"), LoadedState.DungeonNodeIndex, 2);
	TestEqual(TEXT("current map id persists"), LoadedState.CurrentMapId, FName(TEXT("HuangshanRoute")));
	TestTrue(TEXT("generated route map flag persists"), LoadedState.bHasGeneratedRouteMap);
	TestEqual(TEXT("route seed persists"), LoadedState.RouteSeed, 17);
	TestEqual(TEXT("route nodes persist"), LoadedState.RouteMapNodes.Num(), 4);
	TestEqual(TEXT("route edges persist"), LoadedState.RouteMapEdges.Num(), 4);
	TestEqual(TEXT("pending route node persists"), LoadedState.PendingRouteNodeId, 1);
	TestTrue(TEXT("visited route node persists"), LoadedState.VisitedRouteNodeIds.Contains(0));
	TestTrue(TEXT("reachable route branch persists"), LoadedState.ReachableRouteNodeIds.Contains(2));
	TestTrue(TEXT("player location flag persists"), LoadedState.bHasPlayerLocation);
	TestEqual(TEXT("player location persists"), LoadedState.PlayerLocation, FVector(120.0f, -34.0f, 88.0f));
	TestTrue(TEXT("Tanjiang unlock persists"), LoadedState.UnlockedRegions.Contains(UGameXXKMVPRules::RegionTanjiang()));
	TestEqual(TEXT("modern starter equipment creates no legacy wooden-sword inventory count"), LoadedState.Inventory.FindRef(UGameXXKMVPRules::ItemWoodenSword()), 0);
	TestEqual(TEXT("inventory consumable count persists"), LoadedState.Inventory.FindRef(UGameXXKMVPRules::ItemHealingPowder()), 2);
	TestEqual(TEXT("equipment compatibility mirror persists"), LoadedState.EquippedWeapon, ExpectedStarterWeaponBaseId);
	const FGameXXKEquipmentLoadout* LoadedHeroLoadout = LoadedState.EquipmentCollection.CharacterLoadouts.Find(
		FGameXXKEquipmentRules::HeroCharacterId());
	if (TestNotNull(TEXT("loaded state keeps the hero equipment loadout"), LoadedHeroLoadout))
	{
		TestEqual(TEXT("modern starter weapon instance persists"), LoadedHeroLoadout->WeaponInstanceId, ExpectedStarterWeaponInstanceId);
	}
	TestTrue(
		TEXT("the complete modern equipment collection persists"),
		FGameXXKEquipmentCollectionState::StaticStruct()->CompareScriptStruct(
			&LoadedState.EquipmentCollection,
			&SourceState.EquipmentCollection,
			PPF_None));
	TestEqual(TEXT("route travel-money balance persists"), LoadedState.CardRun.RouteTravelMoney, 95);
	TestTrue(TEXT("route-economy initialization persists"), LoadedState.CardRun.bRouteEconomyInitialized);
	TestEqual(TEXT("route travel-money receipt count persists"), LoadedState.CardRun.RewardedTravelMoneyNodes.Num(), 1);
	if (LoadedState.CardRun.RewardedTravelMoneyNodes.Num() == 1)
	{
		const FGameXXKRouteTravelMoneyReceipt& LoadedReceipt = LoadedState.CardRun.RewardedTravelMoneyNodes[0];
		TestEqual(TEXT("route travel-money receipt chapter persists"), LoadedReceipt.Chapter, 2);
		TestEqual(TEXT("route travel-money receipt node persists"), LoadedReceipt.NodeId, 42);
		TestEqual(TEXT("route travel-money receipt amount persists"), LoadedReceipt.Amount, 35);
	}
	FGameXXKRuntimeState& MutableLoadedState = LoadedSubsystem->GetMutableRuntimeState();
	const FGameXXKCardRunState BeforeDuplicateAward = MutableLoadedState.CardRun;
	bool bDuplicateAwarded = true;
	TestTrue(
		TEXT("replaying a loaded receipt succeeds as a no-op"),
		FGameXXKRouteEconomyRules::AwardNodeOnce(
			MutableLoadedState.CardRun,
			2,
			42,
			999,
			bDuplicateAwarded));
	TestFalse(TEXT("replaying a loaded receipt reports no award"), bDuplicateAwarded);
	TestTrue(
		TEXT("replaying a loaded receipt changes no card-run state"),
		FGameXXKCardRunState::StaticStruct()->CompareScriptStruct(
			&MutableLoadedState.CardRun,
			&BeforeDuplicateAward,
			PPF_None));

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
	FGameXXKRuntimeState AcceptedWithoutFollowerRuntimeState;
	FGameXXKSaveMigrationReport AcceptedWithoutFollowerReport;
	if (!TestTrue(
		TEXT("legacy accepted quest save restores through the typed migration boundary"),
		FGameXXKSaveMigration::TryRestoreRuntimeState(
			AcceptedWithoutFollowerSaveState,
			AcceptedWithoutFollowerRuntimeState,
			AcceptedWithoutFollowerReport)))
	{
		return false;
	}
	TestEqual(TEXT("legacy accepted quest save restores accepted quest state"), AcceptedWithoutFollowerRuntimeState.QuestState, EGameXXKQuestState::Accepted);
	TestTrue(TEXT("legacy accepted quest save restores the required task NPC follower"), AcceptedWithoutFollowerRuntimeState.bFollowerJoined);
	TestFalse(TEXT("legacy accepted quest save does not invent a task NPC location"), AcceptedWithoutFollowerRuntimeState.bHasQuestNpcLocation);

	UGameInstance* StartGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* StartGameSubsystem = NewObject<UGameXXKMVPSubsystem>(StartGameInstance);
	TestTrue(TEXT("StartGame starts a fresh new game"), StartGameSubsystem->StartGame());
	TestEqual(TEXT("StartGame enters Qingshan town for new game"), StartGameSubsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
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
	TestEqual(TEXT("new game enters Qingshan town"), NewGameSubsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestTrue(TEXT("new game unlocks Qingshan"), NewGameSubsystem->IsRegionUnlocked(UGameXXKMVPRules::RegionQingshan()));
	TestFalse(TEXT("new game keeps Tanjiang locked"), NewGameSubsystem->IsRegionUnlocked(UGameXXKMVPRules::RegionTanjiang()));

	TestFalse(TEXT("delete slot starts absent"), SourceSubsystem->DoesSaveGameExist(DeleteSlot, UserIndex));
	TestTrue(TEXT("save delete slot succeeds"), SourceSubsystem->SaveCurrentGame(DeleteSlot, UserIndex));
	TestTrue(TEXT("delete slot exists after save"), SourceSubsystem->DoesSaveGameExist(DeleteSlot, UserIndex));
	TestTrue(TEXT("delete slot succeeds"), SourceSubsystem->DeleteSaveGame(DeleteSlot, UserIndex));
	TestFalse(TEXT("delete slot absent after delete"), SourceSubsystem->DoesSaveGameExist(DeleteSlot, UserIndex));

	DeleteMainAndBackup(RoundTripSlot, UserIndex);
	DeleteMainAndBackup(FollowerSlot, UserIndex);
	DeleteMainAndBackup(NewGameSlot, UserIndex);
	DeleteMainAndBackup(DeleteSlot, UserIndex);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSaveGameMigrationTransactionTest,
	"GameXXK.MVP.SaveGame.MigrationTransaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSaveGameMigrationTransactionTest::RunTest(const FString& Parameters)
{
	const int32 UserIndex = 0;
	const FString SuccessSlot(TEXT("GameXXK_MVP_Automation_V7Migration_Success"));
	const FString ExistingBackupSlot(TEXT("GameXXK_MVP_Automation_V7Migration_ExistingBackup"));
	const FString BackupFailureSlot(TEXT("GameXXK_MVP_Automation_V7Migration_BackupFailure"));
	const FString MainFailureSlot(TEXT("GameXXK_MVP_Automation_V7Migration_MainFailure"));
	const FString RollbackFailureSlot(TEXT("GameXXK_MVP_Automation_V7Migration_RollbackFailure"));
	const FString CurrentSlot(TEXT("GameXXK_MVP_Automation_V7Migration_Current"));
	const FString FutureSlot(TEXT("GameXXK_MVP_Automation_V7Migration_Future"));
	const FString InvalidSlot(TEXT("GameXXK_MVP_Automation_V7Migration_Invalid"));
	const FString MismatchSlot(TEXT("GameXXK_MVP_Automation_V7Migration_Mismatch"));
	const FString MigrationFailureSlot(TEXT("GameXXK_MVP_Automation_V7Migration_InvalidLegacy"));
	const TArray<FString> Slots{
		SuccessSlot,
		ExistingBackupSlot,
		BackupFailureSlot,
		MainFailureSlot,
		RollbackFailureSlot,
		CurrentSlot,
		FutureSlot,
		InvalidSlot,
		MismatchSlot,
		MigrationFailureSlot};
	for (const FString& Slot : Slots)
	{
		DeleteMainAndBackup(Slot, UserIndex);
	}

	// Successful v8 load keeps the exact source in a backup, upgrades main, then commits live state.
	UGameXXKSaveGame* SuccessSource = MakeVersionedSaveObject(RouteEconomyDependencySaveVersion(), 601);
	TestTrue(TEXT("seed successful dependency-version source"), UGameplayStatics::SaveGameToSlot(SuccessSource, SuccessSlot, UserIndex));
	UGameXXKMVPSubsystem* SuccessSubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	SuccessSubsystem->GetMutableRuntimeState().PlayerGold = 999;
	bool bObservedCommitLast = false;
	SuccessSubsystem->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
		[&](USaveGame* Object, const FString& Slot, const int32 Index)
		{
			if (Slot == SuccessSlot)
			{
				bObservedCommitLast = SuccessSubsystem->GetRuntimeState().PlayerGold == 999;
			}
			return UGameplayStatics::SaveGameToSlot(Object, Slot, Index);
		}));
	TestTrue(TEXT("v8 load transaction succeeds"), SuccessSubsystem->LoadGameFromSlot(SuccessSlot, UserIndex));
	TestTrue(TEXT("live state is unchanged while upgraded main is written"), bObservedCommitLast);
	const UGameXXKSaveGame* SuccessBackup = LoadTypedSave(BackupSlotFor(SuccessSlot), UserIndex);
	const UGameXXKSaveGame* SuccessMain = LoadTypedSave(SuccessSlot, UserIndex);
	TestNotNull(TEXT("successful migration creates backup"), SuccessBackup);
	TestNotNull(TEXT("successful migration keeps main readable"), SuccessMain);
	if (SuccessBackup && SuccessMain)
	{
		TestEqual(TEXT("backup remains dependency version"), SuccessBackup->SaveState.SaveVersion, RouteEconomyDependencySaveVersion());
		TestEqual(TEXT("backup keeps exact source gold"), SuccessBackup->SaveState.RuntimeState.PlayerGold, 601);
		TestEqual(TEXT("main upgrades to version nine"), SuccessMain->SaveState.SaveVersion, FGameXXKSaveMigration::CurrentSaveVersion);
	}
	TestEqual(TEXT("live state commits only migrated source"), SuccessSubsystem->GetRuntimeState().PlayerGold, 601);
	TestTrue(TEXT("successful migration clears error"), SuccessSubsystem->GetLastSaveLoadError().IsEmpty());

	// An already verified matching backup is never overwritten.
	UGameXXKSaveGame* ExistingSource = MakeVersionedSaveObject(RouteEconomyDependencySaveVersion(), 602);
	TestTrue(TEXT("seed existing-backup main"), UGameplayStatics::SaveGameToSlot(ExistingSource, ExistingBackupSlot, UserIndex));
	TestTrue(TEXT("seed matching existing backup"), UGameplayStatics::SaveGameToSlot(ExistingSource, BackupSlotFor(ExistingBackupSlot), UserIndex));
	int32 ExistingBackupWrites = 0;
	UGameXXKMVPSubsystem* ExistingSubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	ExistingSubsystem->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
		[&](USaveGame* Object, const FString& Slot, const int32 Index)
		{
			ExistingBackupWrites += Slot == BackupSlotFor(ExistingBackupSlot) ? 1 : 0;
			return UGameplayStatics::SaveGameToSlot(Object, Slot, Index);
		}));
	TestTrue(TEXT("matching existing backup is reusable"), ExistingSubsystem->LoadGameFromSlot(ExistingBackupSlot, UserIndex));
	TestEqual(TEXT("matching existing backup is not overwritten"), ExistingBackupWrites, 0);
	const UGameXXKSaveGame* ExistingBackupAfter = LoadTypedSave(BackupSlotFor(ExistingBackupSlot), UserIndex);
	TestEqual(TEXT("existing backup sentinel remains unchanged"), ExistingBackupAfter ? ExistingBackupAfter->SaveState.RuntimeState.PlayerGold : -1, 602);

	// Deterministic backup failure leaves disk/live state untouched.
	UGameXXKSaveGame* BackupFailureSource = MakeVersionedSaveObject(RouteEconomyDependencySaveVersion(), 603);
	TestTrue(TEXT("seed backup-failure main"), UGameplayStatics::SaveGameToSlot(BackupFailureSource, BackupFailureSlot, UserIndex));
	UGameXXKMVPSubsystem* BackupFailureSubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	BackupFailureSubsystem->GetMutableRuntimeState().PlayerGold = 7003;
	const TArray<uint8> BackupFailureLiveBefore = SerializeRuntimeState(BackupFailureSubsystem->GetRuntimeStateCopy());
	BackupFailureSubsystem->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
		[&](USaveGame* Object, const FString& Slot, const int32 Index)
		{
			return Slot == BackupSlotFor(BackupFailureSlot) ? false : UGameplayStatics::SaveGameToSlot(Object, Slot, Index);
		}));
	TestFalse(TEXT("backup write failure rejects load"), BackupFailureSubsystem->LoadGameFromSlot(BackupFailureSlot, UserIndex));
	TestEqual(TEXT("backup failure keeps live bytes"), SerializeRuntimeState(BackupFailureSubsystem->GetRuntimeStateCopy()), BackupFailureLiveBefore);
	TestFalse(TEXT("failed backup is absent"), UGameplayStatics::DoesSaveGameExist(BackupSlotFor(BackupFailureSlot), UserIndex));
	const UGameXXKSaveGame* BackupFailureMain = LoadTypedSave(BackupFailureSlot, UserIndex);
	TestEqual(TEXT("backup failure keeps main at dependency version"), BackupFailureMain ? BackupFailureMain->SaveState.SaveVersion : -1, RouteEconomyDependencySaveVersion());

	// A failed upgraded-main write triggers a second main write that restores the verified v6 backup.
	UGameXXKSaveGame* MainFailureSource = MakeVersionedSaveObject(RouteEconomyDependencySaveVersion(), 604);
	const TArray<uint8> MainFailureSourceBytes = SerializeSaveObject(MainFailureSource);
	TestTrue(TEXT("seed upgraded-main failure source"), UGameplayStatics::SaveGameToSlot(MainFailureSource, MainFailureSlot, UserIndex));
	UGameXXKMVPSubsystem* MainFailureSubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	MainFailureSubsystem->GetMutableRuntimeState().PlayerGold = 7004;
	const TArray<uint8> MainFailureLiveBefore = SerializeRuntimeState(MainFailureSubsystem->GetRuntimeStateCopy());
	int32 MainWriteAttempts = 0;
	MainFailureSubsystem->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
		[&](USaveGame* Object, const FString& Slot, const int32 Index)
		{
			if (Slot == MainFailureSlot && MainWriteAttempts++ == 0)
			{
				// Simulate an I/O layer that touched the main file but then reported failure.
				UGameplayStatics::SaveGameToSlot(Object, Slot, Index);
				return false;
			}
			return UGameplayStatics::SaveGameToSlot(Object, Slot, Index);
		}));
	TestFalse(TEXT("upgraded-main write failure rejects load"), MainFailureSubsystem->LoadGameFromSlot(MainFailureSlot, UserIndex));
	TestEqual(TEXT("failure attempts upgraded write then rollback"), MainWriteAttempts, 2);
	TestEqual(TEXT("upgraded-main failure keeps live bytes"), SerializeRuntimeState(MainFailureSubsystem->GetRuntimeStateCopy()), MainFailureLiveBefore);
	const UGameXXKSaveGame* RestoredMain = LoadTypedSave(MainFailureSlot, UserIndex);
	TestEqual(TEXT("rollback restores source version"), RestoredMain ? RestoredMain->SaveState.SaveVersion : -1, RouteEconomyDependencySaveVersion());
	TestEqual(TEXT("rollback restores source payload"), RestoredMain ? RestoredMain->SaveState.RuntimeState.PlayerGold : -1, 604);
	TestEqual(TEXT("rollback restores source serialization exactly"), SerializeSaveObject(const_cast<UGameXXKSaveGame*>(RestoredMain)), MainFailureSourceBytes);

	// If the main write touched disk and rollback itself fails, keep the verified backup and
	// report the exceptional recovery state truthfully instead of claiming the main was preserved.
	UGameXXKSaveGame* RollbackFailureSource = MakeVersionedSaveObject(RouteEconomyDependencySaveVersion(), 641);
	TestTrue(TEXT("seed rollback-failure source"), UGameplayStatics::SaveGameToSlot(RollbackFailureSource, RollbackFailureSlot, UserIndex));
	UGameXXKMVPSubsystem* RollbackFailureSubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	RollbackFailureSubsystem->GetMutableRuntimeState().PlayerGold = 7641;
	int32 RollbackFailureMainWrites = 0;
	RollbackFailureSubsystem->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
		[&](USaveGame* Object, const FString& Slot, const int32 Index)
		{
			if (Slot != RollbackFailureSlot)
			{
				return UGameplayStatics::SaveGameToSlot(Object, Slot, Index);
			}
			++RollbackFailureMainWrites;
			if (RollbackFailureMainWrites == 1)
			{
				UGameplayStatics::SaveGameToSlot(Object, Slot, Index);
			}
			return false;
		}));
	TestFalse(TEXT("rollback-write failure rejects load"), RollbackFailureSubsystem->LoadGameFromSlot(RollbackFailureSlot, UserIndex));
	TestEqual(TEXT("rollback-write failure attempts migrated main then rollback"), RollbackFailureMainWrites, 2);
	TestEqual(TEXT("rollback-write failure keeps live state unchanged"), RollbackFailureSubsystem->GetRuntimeState().PlayerGold, 7641);
	const UGameXXKSaveGame* RollbackFailureBackup = LoadTypedSave(BackupSlotFor(RollbackFailureSlot), UserIndex);
	TestEqual(TEXT("rollback-write failure retains the verified dependency backup"), RollbackFailureBackup ? RollbackFailureBackup->SaveState.SaveVersion : -1, RouteEconomyDependencySaveVersion());
	TestEqual(
		TEXT("rollback-write failure surfaces a truthful recovery diagnostic"),
		RollbackFailureSubsystem->GetLastSaveLoadError().ToString(),
		FString(TEXT("存档迁移失败，原存档仍保存在迁移备份中，请勿覆盖当前存档。")));

	// Current v9 load is read-only: no backup and no save writes.
	UGameXXKSaveGame* CurrentSource = MakeVersionedSaveObject(FGameXXKSaveMigration::CurrentSaveVersion, 605);
	TestTrue(TEXT("seed current v9 source"), UGameplayStatics::SaveGameToSlot(CurrentSource, CurrentSlot, UserIndex));
	UGameXXKMVPSubsystem* CurrentSubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	int32 CurrentWrites = 0;
	CurrentSubsystem->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
		[&](USaveGame*, const FString&, const int32)
		{
			++CurrentWrites;
			return false;
		}));
	TestTrue(TEXT("current v9 load succeeds without writer"), CurrentSubsystem->LoadGameFromSlot(CurrentSlot, UserIndex));
	TestEqual(TEXT("current v9 load performs zero writes"), CurrentWrites, 0);
	TestFalse(TEXT("current v9 load creates no migration backup"), UGameplayStatics::DoesSaveGameExist(BackupSlotFor(CurrentSlot), UserIndex));

	// Future and invalid-current saves preserve the copied HUD view and live state byte-for-byte.
	UGameXXKSaveGame* FutureSource = MakeVersionedSaveObject(FGameXXKSaveMigration::CurrentSaveVersion + 1, 606);
	TestTrue(TEXT("seed future-version source"), UGameplayStatics::SaveGameToSlot(FutureSource, FutureSlot, UserIndex));
	UGameXXKMVPSubsystem* FutureSubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	FGameXXKRuntimeState FutureRawBefore;
	TestTrue(TEXT("future-version test prepares HUD fixture"), BuildActiveHudFixture(FutureSubsystem, &FutureRawBefore));
	const FGameXXKRuntimeState FutureViewBefore = FutureSubsystem->GetRuntimeStateCopy();
	int32 FutureWrites = 0;
	FutureSubsystem->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
		[&](USaveGame*, const FString&, const int32)
		{
			++FutureWrites;
			return false;
		}));
	TestFalse(TEXT("future-version save is rejected"), FutureSubsystem->LoadGameFromSlot(FutureSlot, UserIndex));
	TestTrue(TEXT("future-version rejection keeps HUD fixture active"), FutureSubsystem->IsBattleHudFixtureActiveForTest());
	TestTrue(TEXT("future-version rejection keeps every visible property"), RuntimeStatesEqual(FutureSubsystem->GetRuntimeStateCopy(), FutureViewBefore));
	TestEqual(TEXT("future-version rejection performs zero writes"), FutureWrites, 0);
	FutureSubsystem->ClearBattleHudFixtureForTest();
	TestTrue(TEXT("future-version rejection keeps every underlying live property"), RuntimeStatesEqual(FutureSubsystem->GetRuntimeStateCopy(), FutureRawBefore));

	UGameXXKSaveGame* InvalidSource = MakeVersionedSaveObject(FGameXXKSaveMigration::CurrentSaveVersion, 607);
	InvalidSource->SaveState.RuntimeState.PlayerHP = InvalidSource->SaveState.RuntimeState.PlayerMaxHP + 1;
	TestTrue(TEXT("seed invalid-current source"), UGameplayStatics::SaveGameToSlot(InvalidSource, InvalidSlot, UserIndex));
	UGameXXKMVPSubsystem* InvalidSubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	FGameXXKRuntimeState InvalidRawBefore;
	TestTrue(TEXT("invalid-current test prepares HUD fixture"), BuildActiveHudFixture(InvalidSubsystem, &InvalidRawBefore));
	const FGameXXKRuntimeState InvalidViewBefore = InvalidSubsystem->GetRuntimeStateCopy();
	int32 InvalidWrites = 0;
	InvalidSubsystem->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
		[&](USaveGame*, const FString&, const int32)
		{
			++InvalidWrites;
			return false;
		}));
	TestFalse(TEXT("invalid-current save is rejected"), InvalidSubsystem->LoadGameFromSlot(InvalidSlot, UserIndex));
	TestTrue(TEXT("invalid-current rejection keeps HUD fixture active"), InvalidSubsystem->IsBattleHudFixtureActiveForTest());
	TestTrue(TEXT("invalid-current rejection keeps every visible property"), RuntimeStatesEqual(InvalidSubsystem->GetRuntimeStateCopy(), InvalidViewBefore));
	TestEqual(TEXT("invalid-current rejection performs zero writes"), InvalidWrites, 0);
	InvalidSubsystem->ClearBattleHudFixtureForTest();
	TestTrue(TEXT("invalid-current rejection keeps every underlying live property"), RuntimeStatesEqual(InvalidSubsystem->GetRuntimeStateCopy(), InvalidRawBefore));
	TestEqual(TEXT("rejections surface the approved Chinese error"), InvalidSubsystem->GetLastSaveLoadError().ToString(), FString(TEXT("存档迁移失败，已保留原存档。")));

	// A mismatched dynamic base backup is preserved and forces the first numbered attempt.
	UGameXXKSaveGame* MismatchMainSource = MakeVersionedSaveObject(RouteEconomyDependencySaveVersion(), 608);
	UGameXXKSaveGame* MismatchBackupSource = MakeVersionedSaveObject(RouteEconomyDependencySaveVersion(), 9608);
	TestTrue(TEXT("seed mismatched-backup main"), UGameplayStatics::SaveGameToSlot(MismatchMainSource, MismatchSlot, UserIndex));
	TestTrue(TEXT("seed mismatched backup sentinel"), UGameplayStatics::SaveGameToSlot(MismatchBackupSource, BackupSlotFor(MismatchSlot), UserIndex));
	UGameXXKMVPSubsystem* MismatchSubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	MismatchSubsystem->GetMutableRuntimeState().PlayerGold = 7608;
	int32 NumberedBackupWrites = 0;
	MismatchSubsystem->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
		[&](USaveGame* Object, const FString& Slot, const int32 Index)
		{
			if (Slot == BackupAttemptSlotFor(MismatchSlot, 1))
			{
				++NumberedBackupWrites;
			}
			return UGameplayStatics::SaveGameToSlot(Object, Slot, Index);
		}));
	TestTrue(TEXT("mismatched base backup allocates a numbered migration attempt"), MismatchSubsystem->LoadGameFromSlot(MismatchSlot, UserIndex));
	TestEqual(TEXT("the first numbered backup receives exactly one write"), NumberedBackupWrites, 1);
	TestEqual(TEXT("mismatched base backup remains unmodified"), LoadTypedSave(BackupSlotFor(MismatchSlot), UserIndex)->SaveState.RuntimeState.PlayerGold, 9608);
	const UGameXXKSaveGame* NumberedBackup = LoadTypedSave(BackupAttemptSlotFor(MismatchSlot, 1), UserIndex);
	TestNotNull(TEXT("mismatch creates the first numbered backup"), NumberedBackup);
	TestEqual(TEXT("numbered backup preserves original source payload"), NumberedBackup ? NumberedBackup->SaveState.RuntimeState.PlayerGold : -1, 608);
	TestEqual(TEXT("mismatched backup migration commits live state only after disk verification"), MismatchSubsystem->GetRuntimeState().PlayerGold, 608);

	// Migration/validation failure occurs after a verified backup but before the main write.
	UGameXXKSaveGame* MigrationFailureSource = MakeVersionedSaveObject(RouteEconomyDependencySaveVersion(), 609);
	MigrationFailureSource->SaveState.RuntimeState.EquipmentCollection = FGameXXKEquipmentCollectionState();
	MigrationFailureSource->SaveState.RuntimeState.Inventory.Reset();
	MigrationFailureSource->SaveState.RuntimeState.Inventory.Add(UGameXXKMVPRules::ItemEnhancementStone(), 10);
	MigrationFailureSource->SaveState.RuntimeState.EnhancementMaterial = 10;
	MigrationFailureSource->SaveState.RuntimeState.EquippedWeapon = TEXT("Item.UnknownLegacyWeapon");
	TestTrue(TEXT("seed invalid legacy main"), UGameplayStatics::SaveGameToSlot(MigrationFailureSource, MigrationFailureSlot, UserIndex));
	UGameXXKMVPSubsystem* MigrationFailureSubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	MigrationFailureSubsystem->GetMutableRuntimeState().PlayerGold = 7609;
	int32 MigrationFailureMainWrites = 0;
	MigrationFailureSubsystem->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
		[&](USaveGame* Object, const FString& Slot, const int32 Index)
		{
			MigrationFailureMainWrites += Slot == MigrationFailureSlot ? 1 : 0;
			return UGameplayStatics::SaveGameToSlot(Object, Slot, Index);
		}));
	TestFalse(TEXT("invalid legacy migration is rejected"), MigrationFailureSubsystem->LoadGameFromSlot(MigrationFailureSlot, UserIndex));
	TestTrue(TEXT("invalid legacy source still receives verified backup"), UGameplayStatics::DoesSaveGameExist(BackupSlotFor(MigrationFailureSlot), UserIndex));
	TestEqual(TEXT("invalid migration never writes main"), MigrationFailureMainWrites, 0);
	TestEqual(TEXT("invalid migration keeps main at source version"), LoadTypedSave(MigrationFailureSlot, UserIndex)->SaveState.SaveVersion, RouteEconomyDependencySaveVersion());
	TestEqual(TEXT("invalid migration keeps live state"), MigrationFailureSubsystem->GetRuntimeState().PlayerGold, 7609);

	for (const FString& Slot : Slots)
	{
		DeleteMainAndBackup(Slot, UserIndex);
	}
	return true;
}

#endif
