#include "GameXXKMVPRules.h"
#include "GameXXKRouteCardRecipe.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool BeginLinearCardBattle(FGameXXKRuntimeState& State)
	{
		State = UGameXXKMVPRules::CreateNewGame();
		return UGameXXKMVPRules::OpenWorldMap(State)
			&& UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan())
			&& UGameXXKMVPRules::AcceptTownQuest(State)
			&& UGameXXKMVPRules::EnterDungeon(State)
			&& (State.bHasGeneratedRouteMap = false, State.RouteMapNodes.Reset(), State.RouteMapEdges.Reset(), State.ReachableRouteNodeIds.Reset(), State.DungeonNodeIndex = 1, true)
			&& UGameXXKMVPRules::AdvanceDungeonNode(State, EGameXXKNodeKind::Battle);
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
	TestEqual(TEXT("a route victory stays on the battle screen until the player makes the three-card choice"), State.Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("the legacy battle projection remains present while the reward choice is pending"), State.bHasActiveBattle);
	TestEqual(TEXT("a victory creates exactly three saved reward card choices"), State.CardRun.PendingReward.CardIds.Num(), 3);
	int32 AcquiredCapacityEntryCount = 0;
	for (const FGameXXKRouteCardEntry& Entry : State.CardRun.RouteCardEntries)
	{
		AcquiredCapacityEntryCount += Entry.bConsumesRouteCapacity ? 1 : 0;
	}
	TestEqual(TEXT("no acquired capacity entry is granted before an explicit player choice"),
		AcquiredCapacityEntryCount,
		0);
	TestEqual(TEXT("the pending offer does not advance the stable-entry sequence"),
		State.CardRun.NextRouteCardEntryOrdinal,
		FGameXXKRouteCardRecipe::BaseEntryCount);
	TestEqual(TEXT("the pending offer does not advance acquisition history"),
		State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount,
		0);
	TestTrue(TEXT("the canonical pending offer keeps legacy RouteCardIds empty"), State.CardRun.RouteCardIds.IsEmpty());
	return true;
}

#endif
