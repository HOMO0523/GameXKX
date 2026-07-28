#include "GameXXKCardBattleAdapter.h"
#include "GameXXKMVPRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool BeginLinearCardBattleForRewardChoice(FGameXXKRuntimeState& State)
	{
		State = UGameXXKMVPRules::CreateNewGame();
		if (!UGameXXKMVPRules::OpenWorldMap(State)
			|| !UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan())
			|| !UGameXXKMVPRules::AcceptTownQuest(State)
			|| !UGameXXKMVPRules::EnterDungeon(State))
		{
			return false;
		}
		State.bHasGeneratedRouteMap = false;
		State.RouteMapNodes.Reset();
		State.RouteMapEdges.Reset();
		State.ReachableRouteNodeIds.Reset();
		State.DungeonNodeIndex = 1;
		return UGameXXKMVPRules::AdvanceDungeonNode(State, EGameXXKNodeKind::Battle);
	}

	void ForceCardBattleVictory(FGameXXKRuntimeState& State)
	{
		for (FGameXXKCardCombatUnit& Unit : State.CardRun.ActiveBattle.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Enemy)
			{
				Unit.HP = 0;
				Unit.bLiving = false;
			}
		}
		State.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardRouteRewardChoiceTest,
	"GameXXK.Integration.CardRoute.RewardChoice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardRouteRewardChoiceTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	TestTrue(TEXT("the reward-choice flow starts a real card battle"), BeginLinearCardBattleForRewardChoice(State));
	ForceCardBattleVictory(State);
	TestTrue(TEXT("victory first opens the saved three-card offer"), UGameXXKMVPRules::ResolveBattleVictory(State, false));
	TestEqual(TEXT("the saved offer contains three choices before selection"), State.CardRun.PendingReward.CardIds.Num(), 3);
	const FName ChosenCardId = State.CardRun.PendingReward.CardIds.IsEmpty() ? NAME_None : State.CardRun.PendingReward.CardIds[0];
	const int32 NextEntryBefore = State.CardRun.NextRouteCardEntryOrdinal;
	const int32 AcquisitionCountBefore = State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount;
	TestTrue(TEXT("an explicit route reward selection commits exactly the visible card"), FGameXXKCardBattleAdapter::ChoosePendingRouteReward(State, ChosenCardId, NAME_None));
	TestTrue(TEXT("only a resolved reward allows the main route to finalize the winning battle"), UGameXXKMVPRules::ResolveBattleVictory(State, false));
	TestEqual(TEXT("the completed reward advances back to the route map"), State.Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("the selected CardId persists in stable route-entry authority, including merge survivors"),
		State.CardRun.RouteCardEntries.ContainsByPredicate([ChosenCardId](const FGameXXKRouteCardEntry& Entry)
		{
			return Entry.CardId == ChosenCardId;
		}));
	TestEqual(TEXT("reward choice advances the stable-entry sequence exactly once"),
		State.CardRun.NextRouteCardEntryOrdinal,
		NextEntryBefore + 1);
	TestEqual(TEXT("reward choice advances acquisition history exactly once"),
		State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount,
		AcquisitionCountBefore + 1);
	TestTrue(TEXT("reward choice leaves legacy RouteCardIds empty"), State.CardRun.RouteCardIds.IsEmpty());
	TestFalse(TEXT("the completed battle clears only its active card-combat session"), State.CardRun.bHasActiveCardBattle);
	return true;
}

#endif
