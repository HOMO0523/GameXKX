#include "GameXXKMVPRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool StartGeneratedBattleVictory(
		FGameXXKRuntimeState& OutState,
		const int32 SourceNodeId,
		const int32 RouteRandomSeed,
		const int32 NextRewardOrdinal)
	{
		OutState = UGameXXKMVPRules::CreateNewGame();
		if (!UGameXXKMVPRules::OpenWorldMap(OutState)
			|| !UGameXXKMVPRules::EnterWorldRegion(OutState, UGameXXKMVPRules::RegionQingshan())
			|| !UGameXXKMVPRules::AcceptTownQuest(OutState)
			|| !UGameXXKMVPRules::EnterDungeon(OutState))
		{
			return false;
		}

		OutState.Screen = EGameXXKScreen::DungeonMap;
		OutState.CurrentMapId = TEXT("HuangshanRoute");
		OutState.CurrentRouteNodeId = SourceNodeId;
		OutState.PendingRouteNodeId = INDEX_NONE;
		OutState.RouteMapNodes.Reset();
		OutState.RouteMapEdges.Reset();
		OutState.VisitedRouteNodeIds.Reset();
		OutState.ReachableRouteNodeIds.Reset();
		OutState.ReachableRouteNodeIds.Add(SourceNodeId);
		OutState.RouteMapNodes.Add(FGameXXKRouteMapNode(
			SourceNodeId,
			1,
			0,
			EGameXXKNodeKind::Battle,
			FVector2D(0.5f, 0.5f),
			{}));

		if (!UGameXXKMVPRules::SelectRouteNodeById(OutState, SourceNodeId))
		{
			return false;
		}
		OutState.CardRun.RouteRandomSeed = RouteRandomSeed;
		OutState.CardRun.NextRewardOrdinal = NextRewardOrdinal;
		for (FGameXXKCardCombatUnit& Unit : OutState.CardRun.ActiveBattle.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Enemy)
			{
				Unit.HP = 0;
				Unit.bLiving = false;
			}
		}
		OutState.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteRewardChoiceSeedDeterminismTest,
	"GameXXK.Route.Reward.ChoiceSeed.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteRewardChoiceSeedDeterminismTest::RunTest(const FString& Parameters)
{
	constexpr int32 RouteRandomSeed = 0x13579BDF;
	constexpr int32 LargeRewardOrdinal = MAX_int32 - 1;
	FGameXXKRuntimeState BaseState;
	if (!TestTrue(
		TEXT("the choice-seed fixture enters a generated battle victory at source node two"),
		StartGeneratedBattleVictory(BaseState, 2, RouteRandomSeed, LargeRewardOrdinal)))
	{
		return false;
	}

	FGameXXKRuntimeState FirstResolution = BaseState;
	FGameXXKRuntimeState RepeatedResolution = BaseState;
	TestTrue(
		TEXT("the facade creates a reward offer for source node two and a large valid ordinal"),
		UGameXXKMVPRules::ResolveBattleVictory(FirstResolution, false));
	TestTrue(
		TEXT("the same facade inputs create a second reward offer"),
		UGameXXKMVPRules::ResolveBattleVictory(RepeatedResolution, false));
	const int32 StableChoiceSeed = FirstResolution.CardRun.PendingReward.ChoiceSeed;
	TestNotEqual(TEXT("the facade persists a non-zero int32 choice seed"), StableChoiceSeed, 0);
	TestEqual(
		TEXT("identical facade inputs persist the same choice seed"),
		RepeatedResolution.CardRun.PendingReward.ChoiceSeed,
		StableChoiceSeed);
	TestEqual(
		TEXT("identical facade inputs persist the same three-card offer"),
		RepeatedResolution.CardRun.PendingReward.CardIds,
		FirstResolution.CardRun.PendingReward.CardIds);
	TestEqual(
		TEXT("creating an offer at MAX minus one advances the persisted ordinal to MAX"),
		FirstResolution.CardRun.NextRewardOrdinal,
		MAX_int32);
	const FGameXXKRuntimeState PendingAtLimitBefore = FirstResolution;
	TestTrue(
		TEXT("a saved pending offer remains readable after its ordinal advances to MAX"),
		UGameXXKMVPRules::ResolveBattleVictory(FirstResolution, false));
	TestTrue(
		TEXT("reading the saved MAX-ordinal offer is a complete runtime no-op"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&FirstResolution,
			&PendingAtLimitBefore,
			PPF_None));

	FGameXXKRuntimeState DifferentOrdinalState;
	if (TestTrue(
		TEXT("the ordinal comparison fixture enters the same source node"),
		StartGeneratedBattleVictory(DifferentOrdinalState, 2, RouteRandomSeed, 0)))
	{
		TestTrue(
			TEXT("the facade creates the comparison reward for a different ordinal"),
			UGameXXKMVPRules::ResolveBattleVictory(DifferentOrdinalState, false));
		TestNotEqual(
			TEXT("different reward ordinals produce distinct choice seeds"),
			DifferentOrdinalState.CardRun.PendingReward.ChoiceSeed,
			StableChoiceSeed);
	}

	FGameXXKRuntimeState DifferentSourceState;
	if (TestTrue(
		TEXT("the source comparison fixture enters generated source node three"),
		StartGeneratedBattleVictory(DifferentSourceState, 3, RouteRandomSeed, LargeRewardOrdinal)))
	{
		TestTrue(
			TEXT("the facade creates the comparison reward for source node three"),
			UGameXXKMVPRules::ResolveBattleVictory(DifferentSourceState, false));
		TestNotEqual(
			TEXT("different source nodes produce distinct choice seeds"),
			DifferentSourceState.CardRun.PendingReward.ChoiceSeed,
			StableChoiceSeed);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteRewardChoiceSeedOrdinalRollbackTest,
	"GameXXK.Route.Reward.ChoiceSeed.OrdinalRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteRewardChoiceSeedOrdinalRollbackTest::RunTest(const FString& Parameters)
{
	constexpr int32 RouteRandomSeed = 0x13579BDF;
	FGameXXKRuntimeState BaseState;
	if (!TestTrue(
		TEXT("the rollback fixture enters a generated battle victory at source node two"),
		StartGeneratedBattleVictory(BaseState, 2, RouteRandomSeed, 0)))
	{
		return false;
	}

	FGameXXKRuntimeState NegativeOrdinalState = BaseState;
	NegativeOrdinalState.CardRun.NextRewardOrdinal = -1;
	const FGameXXKRuntimeState NegativeOrdinalBefore = NegativeOrdinalState;
	TestFalse(
		TEXT("the victory facade rejects a negative next reward ordinal"),
		UGameXXKMVPRules::ResolveBattleVictory(NegativeOrdinalState, false));
	TestTrue(
		TEXT("negative-ordinal rejection preserves the complete runtime"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&NegativeOrdinalState,
			&NegativeOrdinalBefore,
			PPF_None));

	FGameXXKRuntimeState ExhaustedOrdinalState = BaseState;
	ExhaustedOrdinalState.CardRun.NextRewardOrdinal = MAX_int32;
	const FGameXXKRuntimeState ExhaustedOrdinalBefore = ExhaustedOrdinalState;
	TestFalse(
		TEXT("the victory facade rejects an exhausted next reward ordinal"),
		UGameXXKMVPRules::ResolveBattleVictory(ExhaustedOrdinalState, false));
	TestTrue(
		TEXT("MAX ordinal rejection preserves the complete runtime"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&ExhaustedOrdinalState,
			&ExhaustedOrdinalBefore,
			PPF_None));
	return true;
}

#endif
