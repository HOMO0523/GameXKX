#include "GameXXKMVPRules.h"
#include "GameXXKRouteEconomyRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool StartMaterializedFormationGame(FGameXXKRuntimeState& OutState)
	{
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		if (!Subsystem || !Subsystem->StartGame())
		{
			return false;
		}
		OutState = Subsystem->GetRuntimeState();
		return true;
	}

	const FGameXXKRouteMapNode* FindRouteNode(const FGameXXKRuntimeState& State, const int32 NodeId)
	{
		return State.RouteMapNodes.FindByPredicate([NodeId](const FGameXXKRouteMapNode& Node)
		{
			return Node.NodeId == NodeId;
		});
	}

	bool IsCombatNode(const EGameXXKNodeKind Kind)
	{
		return Kind == EGameXXKNodeKind::Battle
			|| Kind == EGameXXKNodeKind::Elite
			|| Kind == EGameXXKNodeKind::Boss;
	}

	bool BuildGeneratedCombatFixture(
		FGameXXKRuntimeState& OutBeforeBattle,
		FGameXXKRuntimeState& OutBattle,
		int32& OutEncounterNodeId)
	{
		FGameXXKRuntimeState State;
		if (!StartMaterializedFormationGame(State))
		{
			return false;
		}
		State.RouteSeed = 1;
		if (!UGameXXKMVPRules::OpenWorldMap(State)
			|| !UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan())
			|| !UGameXXKMVPRules::AcceptTownQuest(State)
			|| !UGameXXKMVPRules::EnterDungeon(State)
			|| State.ReachableRouteNodeIds.IsEmpty())
		{
			return false;
		}

		const int32 StartNodeId = State.ReachableRouteNodeIds[0];
		if (!UGameXXKMVPRules::SelectRouteNodeById(State, StartNodeId))
		{
			return false;
		}

		OutEncounterNodeId = INDEX_NONE;
		for (const int32 CandidateNodeId : State.ReachableRouteNodeIds)
		{
			const FGameXXKRouteMapNode* CandidateNode = FindRouteNode(State, CandidateNodeId);
			if (CandidateNode && IsCombatNode(CandidateNode->NodeKind))
			{
				OutEncounterNodeId = CandidateNodeId;
				break;
			}
		}
		if (OutEncounterNodeId == INDEX_NONE)
		{
			return false;
		}

		OutBeforeBattle = State;
		if (!UGameXXKMVPRules::SelectRouteNodeById(State, OutEncounterNodeId))
		{
			return false;
		}
		OutBattle = MoveTemp(State);
		return true;
	}

	FGameXXKBattleEntryCheckpoint MakeCheckpoint(
		const FGameXXKRuntimeState& BeforeBattle,
		const int32 EncounterNodeId)
	{
		FGameXXKBattleEntryCheckpoint Checkpoint;
		Checkpoint.bValid = true;
		Checkpoint.SourceNodeId = EncounterNodeId;
		Checkpoint.PreviousCurrentRouteNodeId = BeforeBattle.CurrentRouteNodeId;
		Checkpoint.PreviousDungeonNodeIndex = BeforeBattle.DungeonNodeIndex;
		Checkpoint.PreviousPlayerHP = BeforeBattle.PlayerHP;
		Checkpoint.PreviousPlayerMP = BeforeBattle.PlayerMP;
		Checkpoint.PreviousVisitedRouteNodeIds = BeforeBattle.VisitedRouteNodeIds;
		Checkpoint.PreviousReachableRouteNodeIds = BeforeBattle.ReachableRouteNodeIds;
		return Checkpoint;
	}

	bool CheckpointsEqual(
		const FGameXXKBattleEntryCheckpoint& Left,
		const FGameXXKBattleEntryCheckpoint& Right)
	{
		return FGameXXKBattleEntryCheckpoint::StaticStruct()->CompareScriptStruct(
			&Left,
			&Right,
			PPF_None);
	}

	bool RuntimeStatesEqual(const FGameXXKRuntimeState& Left, const FGameXXKRuntimeState& Right)
	{
		return FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&Left, &Right, PPF_None);
	}

	bool InventoriesEqual(const TMap<FName, int32>& Left, const TMap<FName, int32>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (const TPair<FName, int32>& Pair : Left)
		{
			const int32* RightValue = Right.Find(Pair.Key);
			if (!RightValue || *RightValue != Pair.Value)
			{
				return false;
			}
		}
		return true;
	}

	bool RelicArraysEqual(const TArray<FGameXXKRelicInstance>& Left, const TArray<FGameXXKRelicInstance>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (!FGameXXKRelicInstance::StaticStruct()->CompareScriptStruct(
				&Left[Index],
				&Right[Index],
				PPF_None))
			{
				return false;
			}
		}
		return true;
	}

	bool BuildAuthoredEncounterRoute(
		const EGameXXKNodeKind EncounterKind,
		FGameXXKRuntimeState& OutBeforeBattle,
		int32& OutEncounterNodeId)
	{
		FGameXXKRuntimeState State;
		if (!StartMaterializedFormationGame(State))
		{
			return false;
		}
		State.RouteSeed = 1;
		if (!UGameXXKMVPRules::OpenWorldMap(State)
			|| !UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan())
			|| !UGameXXKMVPRules::AcceptTownQuest(State)
			|| !UGameXXKMVPRules::EnterDungeon(State))
		{
			return false;
		}

		constexpr int32 PreviousNodeId = 100;
		constexpr int32 EncounterNodeId = 101;
		constexpr int32 SiblingNodeId = 102;
		State.RouteMapNodes = {
			FGameXXKRouteMapNode(
				PreviousNodeId,
				0,
				0,
				EGameXXKNodeKind::Start,
				FVector2D(0.5f, 0.0f),
				TArray<int32>{EncounterNodeId, SiblingNodeId}),
			FGameXXKRouteMapNode(
				EncounterNodeId,
				1,
				0,
				EncounterKind,
				FVector2D(0.35f, 0.2f),
				TArray<int32>{}),
			FGameXXKRouteMapNode(
				SiblingNodeId,
				1,
				1,
				EGameXXKNodeKind::Camp,
				FVector2D(0.65f, 0.2f),
				TArray<int32>{})};
		State.RouteMapEdges = {
			FGameXXKRouteMapEdge(PreviousNodeId, EncounterNodeId),
			FGameXXKRouteMapEdge(PreviousNodeId, SiblingNodeId)};
		State.VisitedRouteNodeIds = {PreviousNodeId};
		State.ReachableRouteNodeIds = {EncounterNodeId, SiblingNodeId};
		// CurrentRouteNodeId is a pre-click presentation cursor, not necessarily the
		// unique visited inbound parent. Exact rollback must preserve this value.
		State.CurrentRouteNodeId = SiblingNodeId;
		State.PendingRouteNodeId = INDEX_NONE;
		State.DungeonNodeIndex = 7;
		State.PlayerHP = FMath::Min(77, State.PlayerMaxHP);
		State.PlayerMP = FMath::Min(17, State.PlayerMaxMP);
		State.Screen = EGameXXKScreen::DungeonMap;
		State.CurrentMapId = TEXT("HuangshanRoute");
		State.BattleEntryCheckpoint = FGameXXKBattleEntryCheckpoint{};
		OutEncounterNodeId = EncounterNodeId;
		OutBeforeBattle = MoveTemp(State);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleRetreatCheckpointSchemaRoundTripTest,
	"GameXXK.Route.BattleRetreat.Checkpoint.SchemaRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleRetreatCheckpointSchemaRoundTripTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState BeforeBattle;
	FGameXXKRuntimeState BattleState;
	int32 EncounterNodeId = INDEX_NONE;
	if (!TestTrue(
		TEXT("checkpoint round-trip fixture enters a generated combat node"),
		BuildGeneratedCombatFixture(BeforeBattle, BattleState, EncounterNodeId)))
	{
		return false;
	}

	BattleState.BattleEntryCheckpoint = MakeCheckpoint(BeforeBattle, EncounterNodeId);
	const FGameXXKSaveState Source = UGameXXKMVPRules::MakeSaveState(BattleState);
	TestEqual(
		TEXT("checkpoint save uses the current append-only schema"),
		Source.SaveVersion,
		FGameXXKSaveMigration::CurrentSaveVersion);

	FGameXXKRuntimeState Restored;
	FGameXXKSaveMigrationReport Report;
	TestTrue(
		FString::Printf(TEXT("current checkpoint save restores: %s"), *Report.Error),
		FGameXXKSaveMigration::TryRestoreRuntimeState(Source, Restored, Report));
	TestTrue(
		TEXT("every checkpoint field survives save restore"),
		CheckpointsEqual(BattleState.BattleEntryCheckpoint, Restored.BattleEntryCheckpoint));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleRetreatLegacySaveMigrationTest,
	"GameXXK.Route.BattleRetreat.LegacySaveMigration.UniqueParent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleRetreatLegacySaveMigrationTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState BeforeBattle;
	FGameXXKRuntimeState BattleState;
	int32 EncounterNodeId = INDEX_NONE;
	if (!TestTrue(
		TEXT("legacy migration fixture enters a generated combat node"),
		BuildGeneratedCombatFixture(BeforeBattle, BattleState, EncounterNodeId)))
	{
		return false;
	}

	BattleState.BattleEntryCheckpoint = FGameXXKBattleEntryCheckpoint{};
	FGameXXKSaveState Source = UGameXXKMVPRules::MakeSaveState(BattleState);
	Source.SaveVersion = FGameXXKSaveMigration::QuestNpcEquipmentOwnerIntroducedSaveVersion;

	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	if (!TestTrue(
		FString::Printf(TEXT("v22 combat save migrates: %s"), *Report.Error),
		FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report)))
	{
		return false;
	}

	const FGameXXKBattleEntryCheckpoint& Checkpoint = Migrated.RuntimeState.BattleEntryCheckpoint;
	TestEqual(TEXT("migration writes v23"), Migrated.SaveVersion, FGameXXKSaveMigration::CurrentSaveVersion);
	TestTrue(TEXT("one visited inbound parent enables legacy retreat"), Checkpoint.bValid);
	TestEqual(TEXT("legacy checkpoint records selected encounter"), Checkpoint.SourceNodeId, EncounterNodeId);
	TestTrue(
		TEXT("legacy checkpoint parent is a visited node"),
		BattleState.VisitedRouteNodeIds.Contains(Checkpoint.PreviousCurrentRouteNodeId));
	TestTrue(
		TEXT("legacy checkpoint parent owns the inbound encounter edge"),
		BattleState.RouteMapEdges.ContainsByPredicate([&Checkpoint](const FGameXXKRouteMapEdge& Edge)
		{
			return Edge.FromNodeId == Checkpoint.PreviousCurrentRouteNodeId
				&& Edge.ToNodeId == Checkpoint.SourceNodeId;
		}));
	TestEqual(TEXT("legacy checkpoint keeps load-time dungeon index"), Checkpoint.PreviousDungeonNodeIndex, BattleState.DungeonNodeIndex);
	TestEqual(TEXT("legacy checkpoint keeps load-time HP"), Checkpoint.PreviousPlayerHP, BattleState.PlayerHP);
	TestEqual(TEXT("legacy checkpoint keeps load-time MP"), Checkpoint.PreviousPlayerMP, BattleState.PlayerMP);
	TestEqual(TEXT("legacy checkpoint snapshots visited nodes"), Checkpoint.PreviousVisitedRouteNodeIds, BattleState.VisitedRouteNodeIds);
	TestEqual(TEXT("legacy checkpoint snapshots reachable nodes"), Checkpoint.PreviousReachableRouteNodeIds, BattleState.ReachableRouteNodeIds);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleRetreatLegacyAmbiguousMigrationTest,
	"GameXXK.Route.BattleRetreat.LegacySaveMigration.AmbiguousParent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleRetreatLegacyAmbiguousMigrationTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState BeforeBattle;
	FGameXXKRuntimeState BattleState;
	int32 EncounterNodeId = INDEX_NONE;
	if (!TestTrue(
		TEXT("ambiguous migration fixture enters a generated combat node"),
		BuildGeneratedCombatFixture(BeforeBattle, BattleState, EncounterNodeId)))
	{
		return false;
	}

	const int32 AmbiguousParentNodeId = 9001;
	BattleState.RouteMapNodes.Add(FGameXXKRouteMapNode(
		AmbiguousParentNodeId,
		0,
		1,
		EGameXXKNodeKind::Start,
		FVector2D(0.75f, 0.0f),
		TArray<int32>{EncounterNodeId}));
	BattleState.RouteMapEdges.Add(FGameXXKRouteMapEdge(AmbiguousParentNodeId, EncounterNodeId));
	BattleState.VisitedRouteNodeIds.Add(AmbiguousParentNodeId);
	BattleState.BattleEntryCheckpoint = FGameXXKBattleEntryCheckpoint{};

	FGameXXKSaveState Source = UGameXXKMVPRules::MakeSaveState(BattleState);
	Source.SaveVersion = FGameXXKSaveMigration::QuestNpcEquipmentOwnerIntroducedSaveVersion;
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	if (!TestTrue(
		FString::Printf(TEXT("ambiguous v22 save remains loadable: %s"), *Report.Error),
		FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report)))
	{
		return false;
	}

	TestFalse(
		TEXT("ambiguous parent safely disables battle retreat"),
		Migrated.RuntimeState.BattleEntryCheckpoint.bValid);
	TestTrue(
		TEXT("ambiguous migration records an explicit warning"),
		Report.Warnings.ContainsByPredicate([](const FString& Warning)
		{
			return Warning.Contains(TEXT("battle retreat"), ESearchCase::IgnoreCase)
				|| Warning.Contains(TEXT("战斗回退"));
		}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleRetreatCheckpointValidationTest,
	"GameXXK.Route.BattleRetreat.Checkpoint.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleRetreatCheckpointValidationTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState BeforeBattle;
	FGameXXKRuntimeState ValidState;
	int32 EncounterNodeId = INDEX_NONE;
	if (!TestTrue(
		TEXT("validation fixture enters a generated combat node"),
		BuildGeneratedCombatFixture(BeforeBattle, ValidState, EncounterNodeId)))
	{
		return false;
	}
	ValidState.BattleEntryCheckpoint = MakeCheckpoint(BeforeBattle, EncounterNodeId);

	FString Error;
	TestTrue(
		FString::Printf(TEXT("matching current checkpoint validates: %s"), *Error),
		FGameXXKSaveMigration::ValidateRuntimeState(ValidState, Error));

	FGameXXKRuntimeState MismatchedSource = ValidState;
	MismatchedSource.BattleEntryCheckpoint.SourceNodeId = EncounterNodeId + 10000;
	Error.Reset();
	TestFalse(
		TEXT("checkpoint source must match the pending encounter"),
		FGameXXKSaveMigration::ValidateRuntimeState(MismatchedSource, Error));

	FGameXXKRuntimeState DuplicateReachable = ValidState;
	DuplicateReachable.BattleEntryCheckpoint.PreviousReachableRouteNodeIds.Add(EncounterNodeId);
	Error.Reset();
	TestFalse(
		TEXT("checkpoint reachable snapshot rejects duplicate IDs"),
		FGameXXKSaveMigration::ValidateRuntimeState(DuplicateReachable, Error));

	FGameXXKRuntimeState NoGeneratedRoute = ValidState;
	NoGeneratedRoute.bHasGeneratedRouteMap = false;
	Error.Reset();
	TestFalse(
		TEXT("valid checkpoint requires a generated route"),
		FGameXXKSaveMigration::ValidateRuntimeState(NoGeneratedRoute, Error));

	FGameXXKRuntimeState NoActiveBattle = ValidState;
	NoActiveBattle.CardRun.bHasActiveCardBattle = false;
	Error.Reset();
	TestFalse(
		TEXT("valid checkpoint requires active card battle authority"),
		FGameXXKSaveMigration::ValidateRuntimeState(NoActiveBattle, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleRetreatCheckpointCaptureKindsTest,
	"GameXXK.Route.BattleRetreat.Checkpoint.CaptureKinds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleRetreatCheckpointCaptureKindsTest::RunTest(const FString& Parameters)
{
	for (const EGameXXKNodeKind EncounterKind : {
		EGameXXKNodeKind::Battle,
		EGameXXKNodeKind::Elite,
		EGameXXKNodeKind::Boss})
	{
		FGameXXKRuntimeState BeforeBattle;
		int32 EncounterNodeId = INDEX_NONE;
		if (!TestTrue(
			TEXT("authored capture fixture builds"),
			BuildAuthoredEncounterRoute(EncounterKind, BeforeBattle, EncounterNodeId)))
		{
			return false;
		}

		FGameXXKRuntimeState State = BeforeBattle;
		if (!TestTrue(TEXT("combat node opens"), UGameXXKMVPRules::SelectRouteNodeById(State, EncounterNodeId)))
		{
			continue;
		}
		const FGameXXKBattleEntryCheckpoint Expected = MakeCheckpoint(BeforeBattle, EncounterNodeId);
		TestTrue(TEXT("combat entry captures the exact pre-click state"), CheckpointsEqual(State.BattleEntryCheckpoint, Expected));
	}

	FGameXXKRuntimeState CampBefore;
	int32 CampNodeId = INDEX_NONE;
	TestTrue(TEXT("non-combat fixture builds"), BuildAuthoredEncounterRoute(EGameXXKNodeKind::Camp, CampBefore, CampNodeId));
	FGameXXKRuntimeState CampState = CampBefore;
	TestTrue(TEXT("non-combat node opens"), UGameXXKMVPRules::SelectRouteNodeById(CampState, CampNodeId));
	TestFalse(TEXT("non-combat entry never captures a battle checkpoint"), CampState.BattleEntryCheckpoint.bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleRetreatCheckpointCaptureAtomicityTest,
	"GameXXK.Route.BattleRetreat.Checkpoint.CaptureFailureAtomicity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleRetreatCheckpointCaptureAtomicityTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	int32 EncounterNodeId = INDEX_NONE;
	if (!TestTrue(
		TEXT("atomic capture fixture builds"),
		BuildAuthoredEncounterRoute(EGameXXKNodeKind::Elite, State, EncounterNodeId)))
	{
		return false;
	}
	// Intentional corruption fixture: invalid v29 provenance must remain atomic.
	State.CardRun.ActiveTemporaryQuestNpcId = TEXT("Npc.Invalid.Atomicity");
	State.CardRun.PartySelection.QuestNpc = FGameXXKQuestNpcCardSelection{};
	const FGameXXKRuntimeState Before = State;
	TestFalse(TEXT("invalid card configuration rejects battle construction"), UGameXXKMVPRules::SelectRouteNodeById(State, EncounterNodeId));
	TestTrue(TEXT("rejected battle entry leaves no checkpoint or route mutation"), RuntimeStatesEqual(State, Before));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleRetreatRollbackKindsTest,
	"GameXXK.Route.BattleRetreat.Rollback.NormalEliteBoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleRetreatRollbackKindsTest::RunTest(const FString& Parameters)
{
	for (const EGameXXKNodeKind EncounterKind : {
		EGameXXKNodeKind::Battle,
		EGameXXKNodeKind::Elite,
		EGameXXKNodeKind::Boss})
	{
		FGameXXKRuntimeState BeforeBattle;
		int32 EncounterNodeId = INDEX_NONE;
		if (!TestTrue(
			TEXT("rollback fixture builds"),
			BuildAuthoredEncounterRoute(EncounterKind, BeforeBattle, EncounterNodeId)))
		{
			return false;
		}
		FGameXXKRuntimeState State = BeforeBattle;
		if (!TestTrue(TEXT("rollback fixture enters combat"), UGameXXKMVPRules::SelectRouteNodeById(State, EncounterNodeId)))
		{
			continue;
		}
		State.PlayerHP = 1;
		State.PlayerMP = 0;
		State.CardRun.ActiveBattle.RoundNumber += 3;

		if (!TestTrue(TEXT("player retreat succeeds"), UGameXXKMVPRules::RetreatCurrentBattleToRoute(State)))
		{
			continue;
		}
		TestEqual(TEXT("retreat returns to route map"), State.Screen, EGameXXKScreen::DungeonMap);
		TestEqual(TEXT("retreat restores route map id"), State.CurrentMapId, FName(TEXT("HuangshanRoute")));
		TestEqual(TEXT("retreat restores exact pre-click current node"), State.CurrentRouteNodeId, BeforeBattle.CurrentRouteNodeId);
		TestEqual(TEXT("retreat restores exact dungeon index"), State.DungeonNodeIndex, BeforeBattle.DungeonNodeIndex);
		TestEqual(TEXT("retreat restores HP"), State.PlayerHP, BeforeBattle.PlayerHP);
		TestEqual(TEXT("retreat restores MP"), State.PlayerMP, BeforeBattle.PlayerMP);
		TestEqual(TEXT("retreat restores visited nodes"), State.VisitedRouteNodeIds, BeforeBattle.VisitedRouteNodeIds);
		TestEqual(TEXT("retreat restores every reachable branch"), State.ReachableRouteNodeIds, BeforeBattle.ReachableRouteNodeIds);
		TestTrue(TEXT("abandoned encounter remains retryable"), State.ReachableRouteNodeIds.Contains(EncounterNodeId));
		TestFalse(TEXT("abandoned encounter remains unvisited"), State.VisitedRouteNodeIds.Contains(EncounterNodeId));
		TestEqual(TEXT("retreat clears pending node"), State.PendingRouteNodeId, INDEX_NONE);
		TestFalse(TEXT("retreat clears legacy battle"), State.bHasActiveBattle);
		TestTrue(TEXT("retreat clears legacy enemies"), State.ActiveBattleEnemies.IsEmpty());
		TestTrue(TEXT("retreat clears legacy party"), State.ActiveBattleParty.IsEmpty());
		TestFalse(TEXT("retreat clears card battle"), State.CardRun.bHasActiveCardBattle);
		TestTrue(TEXT("retreat clears enemy intents"), State.CardRun.EnemyIntents.IsEmpty());
		TestTrue(TEXT("retreat clears pending reward"), State.CardRun.PendingReward.Options.IsEmpty());
		TestFalse(TEXT("retreat consumes its checkpoint"), State.BattleEntryCheckpoint.bValid);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleRetreatPendingRewardTest,
	"GameXXK.Route.BattleRetreat.Rollback.PendingVictoryReward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleRetreatPendingRewardTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState BeforeBattle;
	int32 EncounterNodeId = INDEX_NONE;
	if (!TestTrue(
		TEXT("pending reward fixture builds"),
		BuildAuthoredEncounterRoute(EGameXXKNodeKind::Elite, BeforeBattle, EncounterNodeId)))
	{
		return false;
	}
	FGameXXKRuntimeState State = BeforeBattle;
	TestTrue(TEXT("pending reward fixture enters Elite"), UGameXXKMVPRules::SelectRouteNodeById(State, EncounterNodeId));
	State.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
	TestTrue(TEXT("Victory opens the saved reward gate"), UGameXXKMVPRules::ResolveBattleVictory(State, false));
	TestEqual(TEXT("Victory gate exposes three options"), State.CardRun.PendingReward.Options.Num(), 3);
	TestTrue(TEXT("player can abandon an unclaimed Victory reward"), UGameXXKMVPRules::RetreatCurrentBattleToRoute(State));
	TestEqual(TEXT("pending-reward retreat restores current node"), State.CurrentRouteNodeId, BeforeBattle.CurrentRouteNodeId);
	TestTrue(TEXT("pending-reward retreat discards all options"), State.CardRun.PendingReward.Options.IsEmpty());
	TestEqual(TEXT("pending-reward retreat awards no travel money"), State.CardRun.RouteTravelMoney, BeforeBattle.CardRun.RouteTravelMoney);
	TestEqual(TEXT("pending-reward retreat awards no acquisition count"), State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount, BeforeBattle.CardRun.RouteProgress.ActualRouteCardAcquisitionCount);
	TestFalse(TEXT("pending-reward encounter is not visited"), State.VisitedRouteNodeIds.Contains(EncounterNodeId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleRetreatPreservesProgressTest,
	"GameXXK.Route.BattleRetreat.Rollback.PreservesCompletedProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleRetreatPreservesProgressTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState BeforeBattle;
	int32 EncounterNodeId = INDEX_NONE;
	if (!TestTrue(
		TEXT("progress fixture builds"),
		BuildAuthoredEncounterRoute(EGameXXKNodeKind::Battle, BeforeBattle, EncounterNodeId)))
	{
		return false;
	}
	bool bAwarded = false;
	TestTrue(
		TEXT("fixture records completed-node route money"),
		FGameXXKRouteEconomyRules::AwardNodeOnce(
			BeforeBattle.CardRun,
			BeforeBattle.CardRun.RouteProgress.CurrentChapter,
			BeforeBattle.VisitedRouteNodeIds[0],
			40,
			bAwarded));
	TestTrue(TEXT("route money receipt is new"), bAwarded);
	BeforeBattle.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 3;
	const int32 RouteMoneyBefore = BeforeBattle.CardRun.RouteTravelMoney;
	const int32 AcquisitionCountBefore = BeforeBattle.CardRun.RouteProgress.ActualRouteCardAcquisitionCount;
	const TMap<FName, int32> InventoryBefore = BeforeBattle.Inventory;
	const TArray<FGameXXKRelicInstance> RelicsBefore = BeforeBattle.CardRun.Relics;

	FGameXXKRuntimeState State = BeforeBattle;
	TestTrue(TEXT("progress fixture enters battle"), UGameXXKMVPRules::SelectRouteNodeById(State, EncounterNodeId));
	TestTrue(TEXT("progress fixture retreats"), UGameXXKMVPRules::RetreatCurrentBattleToRoute(State));
	TestEqual(TEXT("retreat preserves completed route money"), State.CardRun.RouteTravelMoney, RouteMoneyBefore);
	TestEqual(TEXT("retreat preserves prior acquisition count"), State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount, AcquisitionCountBefore);
	TestTrue(TEXT("retreat preserves inventory"), InventoriesEqual(State.Inventory, InventoryBefore));
	TestTrue(TEXT("retreat preserves prior relics"), RelicArraysEqual(State.CardRun.Relics, RelicsBefore));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleRetreatRejectsInvalidTest,
	"GameXXK.Route.BattleRetreat.Rollback.RejectsInvalidAtomically",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleRetreatRejectsInvalidTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState BeforeBattle;
	int32 EncounterNodeId = INDEX_NONE;
	TestTrue(TEXT("invalid retreat fixture builds"), BuildAuthoredEncounterRoute(EGameXXKNodeKind::Elite, BeforeBattle, EncounterNodeId));
	FGameXXKRuntimeState State = BeforeBattle;
	TestTrue(TEXT("invalid retreat fixture enters combat"), UGameXXKMVPRules::SelectRouteNodeById(State, EncounterNodeId));
	State.BattleEntryCheckpoint.SourceNodeId += 1000;
	const FGameXXKRuntimeState BeforeRejectedRetreat = State;
	TestFalse(TEXT("mismatched checkpoint rejects retreat"), UGameXXKMVPRules::RetreatCurrentBattleToRoute(State));
	TestTrue(TEXT("rejected retreat is atomic"), RuntimeStatesEqual(State, BeforeRejectedRetreat));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleRetreatCheckpointClearTest,
	"GameXXK.Route.BattleRetreat.Checkpoint.ClearOnCommitAndTerminal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleRetreatCheckpointClearTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState RewardBefore;
	int32 RewardNodeId = INDEX_NONE;
	TestTrue(TEXT("reward clear fixture builds"), BuildAuthoredEncounterRoute(EGameXXKNodeKind::Battle, RewardBefore, RewardNodeId));
	FGameXXKRuntimeState RewardState = RewardBefore;
	TestTrue(TEXT("reward clear fixture enters battle"), UGameXXKMVPRules::SelectRouteNodeById(RewardState, RewardNodeId));
	RewardState.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
	TestTrue(TEXT("reward clear fixture opens reward"), UGameXXKMVPRules::ResolveBattleVictory(RewardState, false));
	FString RewardError;
	TestTrue(
		FString::Printf(TEXT("player skips reward and commits node: %s"), *RewardError),
		UGameXXKMVPRules::SkipPendingRouteRewardAndFinish(RewardState, &RewardError));
	TestFalse(TEXT("committed reward clears checkpoint"), RewardState.BattleEntryCheckpoint.bValid);

	for (const bool bDefeated : {false, true})
	{
		FGameXXKRuntimeState TerminalBefore;
		int32 TerminalNodeId = INDEX_NONE;
		TestTrue(TEXT("terminal clear fixture builds"), BuildAuthoredEncounterRoute(EGameXXKNodeKind::Elite, TerminalBefore, TerminalNodeId));
		FGameXXKRuntimeState TerminalState = TerminalBefore;
		TestTrue(TEXT("terminal clear fixture enters battle"), UGameXXKMVPRules::SelectRouteNodeById(TerminalState, TerminalNodeId));
		const bool bSettled = bDefeated
			? UGameXXKMVPRules::FailDungeonToTown(TerminalState)
			: UGameXXKMVPRules::AbandonDungeonToTown(TerminalState);
		TestTrue(TEXT("terminal route settles"), bSettled);
		TestFalse(TEXT("terminal settlement clears checkpoint"), TerminalState.BattleEntryCheckpoint.bValid);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteExitAcceptanceFixtureTest,
	"GameXXK.Route.BattleRetreat.DevelopmentFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteExitAcceptanceFixtureTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestTrue(TEXT("development fixture starts a game"), Subsystem->StartGame());
	TestTrue(TEXT("development fixture enters Qingshan"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("development fixture accepts quest"), Subsystem->AcceptQuest());
	TestTrue(TEXT("development fixture enters generated route"), Subsystem->OpenDungeonFromTownExit());
	TestTrue(TEXT("development fixture completes Start as the player"), Subsystem->SelectRouteNodeById(0));
	const FGameXXKRuntimeState BeforeFixture = Subsystem->GetRuntimeState();

	FString Error;
	TestTrue(
		FString::Printf(TEXT("route-exit acceptance fixture applies: %s"), *Error),
		Subsystem->ApplyRouteExitAcceptanceFixtureForTest(Error));
	TestTrue(TEXT("route-exit fixture reports active"), Subsystem->IsRouteExitAcceptanceFixtureActiveForTest());
	const FGameXXKRuntimeState& Applied = Subsystem->GetRuntimeState();
	const FGameXXKRouteMapNode* EliteNode = Applied.RouteMapNodes.FindByPredicate([&Applied](const FGameXXKRouteMapNode& Node)
	{
		return Applied.ReachableRouteNodeIds.Contains(Node.NodeId)
			&& Node.NodeKind == EGameXXKNodeKind::Elite;
	});
	TestNotNull(TEXT("fixture exposes a player-clickable Elite"), EliteNode);
	TestEqual(TEXT("fixture seeds a visible nonzero abandon conversion"), Applied.CardRun.RouteTravelMoney, 99);
	TestEqual(TEXT("fixture seeds a visible stone conversion"), Applied.CardRun.RouteProgress.ActualRouteCardAcquisitionCount, 29);
	TestFalse(TEXT("fixture never starts or selects the Elite"), Applied.CardRun.bHasActiveCardBattle);

	TestTrue(TEXT("route-exit acceptance fixture clears"), Subsystem->ClearRouteExitAcceptanceFixtureForTest(Error));
	TestFalse(TEXT("cleared route-exit fixture reports inactive"), Subsystem->IsRouteExitAcceptanceFixtureActiveForTest());
	TestTrue(TEXT("clearing fixture restores every authoritative field"), RuntimeStatesEqual(Subsystem->GetRuntimeState(), BeforeFixture));
	return true;
}

#endif
