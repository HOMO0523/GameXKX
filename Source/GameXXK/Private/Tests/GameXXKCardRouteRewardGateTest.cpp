#include "GameXXKMVPRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKPermanentPartyTestFixtures.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool BeginLinearCardBattle(
		FGameXXKRuntimeState& State,
		const EGameXXKNodeKind NodeKind = EGameXXKNodeKind::Battle)
	{
		State = GameXXKPermanentPartyTestFixtures::MakeStartedState();
		return UGameXXKMVPRules::OpenWorldMap(State)
			&& UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan())
			&& UGameXXKMVPRules::AcceptTownQuest(State)
			&& UGameXXKMVPRules::EnterDungeon(State)
			&& (State.bHasGeneratedRouteMap = false, State.RouteMapNodes.Reset(), State.RouteMapEdges.Reset(), State.ReachableRouteNodeIds.Reset(), State.DungeonNodeIndex = 1, true)
			&& UGameXXKMVPRules::AdvanceDungeonNode(State, NodeKind);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardRouteRewardGateTest,
	"GameXXK.Integration.CardRoute.RewardGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardRouteRewardGateTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	TestTrue(TEXT("the reward gate begins from a real card battle"), BeginLinearCardBattle(State));
	for (FGameXXKCardCombatUnit& Unit : State.CardRun.ActiveBattle.Units)
	{
		if (Unit.Side == EGameXXKCardTargetSide::Enemy)
		{
			Unit.HP = 0;
			Unit.bLiving = false;
		}
	}
	State.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;

	TestTrue(TEXT("a card-runtime victory is recognized by the existing main-route completion entry"), UGameXXKMVPRules::ResolveBattleVictory(State, false));
	TestEqual(TEXT("a route victory stays on the battle screen until the player makes the tiered three-choice pick"), State.Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("the legacy battle projection remains present while the reward choice is pending"), State.bHasActiveBattle);
	TestEqual(TEXT("a victory creates exactly three tiered reward options"), State.CardRun.PendingReward.Options.Num(), 3);
	TestEqual(TEXT("a normal battle offers a relic as its first option"), State.CardRun.PendingReward.Options[0].Kind, EGameXXKBattleRewardKind::Relic);
	TestEqual(TEXT("a normal battle offers a relic as its second option"), State.CardRun.PendingReward.Options[1].Kind, EGameXXKBattleRewardKind::Relic);
	TestEqual(TEXT("a normal battle offers a deck-card upgrade as its third option"), State.CardRun.PendingReward.Options[2].Kind, EGameXXKBattleRewardKind::DeckCardUpgrade);
	TestTrue(TEXT("the legacy CardIds payload stays empty for a tiered offer"), State.CardRun.PendingReward.CardIds.IsEmpty());
	TestEqual(TEXT("the pending offer does not advance acquisition history"),
		State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount,
		0);

	FGameXXKRuntimeState SingleMapBossState;
	if (TestTrue(TEXT("single-map Boss reward fixture begins from a real victory gate"),
		BeginLinearCardBattle(SingleMapBossState)))
	{
		SingleMapBossState.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
		if (TestTrue(TEXT("single-map Boss victory creates only non-Boss compatibility choices"),
			FGameXXKCardBattleAdapter::CreateTieredBattleRewardOffer(
				SingleMapBossState,
				EGameXXKNodeKind::Boss,
				SingleMapBossState.DungeonNodeIndex,
				773401)))
		{
			TestFalse(TEXT("single-map Boss reward never offers a Boss card"),
				SingleMapBossState.CardRun.PendingReward.Options.ContainsByPredicate([](const FGameXXKBattleRewardOption& Option)
				{
					return Option.Kind == EGameXXKBattleRewardKind::BossCard
						|| Option.CardId.ToString().StartsWith(TEXT("Route.Boss."));
				}));
		}
	}
	return true;
}

#endif
