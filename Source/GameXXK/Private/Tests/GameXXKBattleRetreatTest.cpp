#include "GameXXKMVPRules.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
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
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
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
		TEXT("checkpoint schema advances the save boundary to v23"),
		Source.SaveVersion,
		FGameXXKSaveMigration::BattleRetreatCheckpointIntroducedSaveVersion);

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

#endif
