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
	TestTrue(TEXT("victory first opens the saved tiered three-choice offer"), UGameXXKMVPRules::ResolveBattleVictory(State, false));
	TestEqual(TEXT("the saved offer contains three choices before selection"), State.CardRun.PendingReward.Options.Num(), 3);
	TestEqual(TEXT("the normal battle first choice is a relic"), State.CardRun.PendingReward.Options[0].Kind, EGameXXKBattleRewardKind::Relic);
	TestEqual(TEXT("the normal battle second choice is a relic"), State.CardRun.PendingReward.Options[1].Kind, EGameXXKBattleRewardKind::Relic);
	TestEqual(TEXT("the normal battle third choice upgrades a deck card"), State.CardRun.PendingReward.Options[2].Kind, EGameXXKBattleRewardKind::DeckCardUpgrade);
	constexpr int32 UpgradeOptionIndex = 2;
	const FName UpgradedCardId = State.CardRun.PendingReward.Options[UpgradeOptionIndex].CardId;
	TestFalse(TEXT("the upgrade option names a configured card"), UpgradedCardId.IsNone());
	const EGameXXKCardQuality QualityBefore = FGameXXKCardBattleAdapter::GetConfiguredCardQuality(State.CardRun, UpgradedCardId);
	TestTrue(TEXT("the upgrade candidate is below maximum quality"), QualityBefore < EGameXXKCardQuality::Epic);
	const int32 NextEntryBefore = State.CardRun.NextRouteCardEntryOrdinal;
	const int32 AcquisitionCountBefore = State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount;
	TestTrue(TEXT("an explicit option choice commits the saved offer and finishes the victory"),
		UGameXXKMVPRules::ResolvePendingBattleRewardChoiceAndFinish(State, UpgradeOptionIndex, NAME_None));
	TestEqual(TEXT("the chosen deck card records its one-step quality upgrade"),
		State.CardRun.UpgradedCardQualities.FindRef(UpgradedCardId),
		FGameXXKCardBattleAdapter::GetNextCardQuality(QualityBefore));
	TestTrue(TEXT("the completed reward clears the saved tiered offer"), State.CardRun.PendingReward.Options.IsEmpty());
	TestEqual(TEXT("the completed reward advances back to the route map"), State.Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("a deck-card upgrade never advances the stable-entry sequence"), State.CardRun.NextRouteCardEntryOrdinal, NextEntryBefore);
	TestEqual(TEXT("a deck-card upgrade never advances acquisition history"), State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount, AcquisitionCountBefore);
	TestTrue(TEXT("reward choice leaves legacy RouteCardIds empty"), State.CardRun.RouteCardIds.IsEmpty());
	TestFalse(TEXT("the completed battle clears only its active card-combat session"), State.CardRun.bHasActiveCardBattle);
	return true;
}

#endif
