#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKMVPRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool RecruitAndActivateCompanion(
		FGameXXKRuntimeState& State,
		const FName TemplateId,
		const int32 Seed,
		FGameXXKPermanentCompanion& OutCompanion,
		FString& OutError)
	{
		FGameXXKCompanionRecruitResult RecruitResult;
		if (!FGameXXKCompanionRules::RecruitPermanentCompanion(
			State.CardRun.CompanionRoster,
			TemplateId,
			Seed,
			RecruitResult,
			&OutError)
			|| RecruitResult.Outcome != EGameXXKCompanionRecruitOutcome::Recruited)
		{
			return false;
		}
		OutCompanion = RecruitResult.Companion;
		return FGameXXKCompanionRules::SetActivePermanentCompanion(
			State.CardRun.CompanionRoster,
			OutCompanion.InstanceId,
			&OutError);
	}

	bool BeginLinearCardBattle(FGameXXKRuntimeState& State)
	{
		const bool bOpenedRoute = UGameXXKMVPRules::OpenWorldMap(State)
			&& UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan())
			&& UGameXXKMVPRules::AcceptTownQuest(State)
			&& UGameXXKMVPRules::EnterDungeon(State);
		if (!bOpenedRoute)
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
	FGameXXKCompanionBattleProgressionRewardTest,
	"GameXXK.Integration.CardRoute.CompanionBattleProgression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionBattleProgressionRewardTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("the permanent card-run state initializes before recruiting"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));

	FGameXXKPermanentCompanion ActiveCompanion;
	if (!TestTrue(FString::Printf(TEXT("an active permanent companion can be recruited before the route: %s"), *Error),
		RecruitAndActivateCompanion(State, TEXT("Companion.Blade.01"), 57311, ActiveCompanion, Error)))
	{
		return false;
	}

	FGameXXKCompanionRecruitResult BenchRecruitResult;
	TestTrue(TEXT("a non-active permanent bench companion can also be retained"),
		FGameXXKCompanionRules::RecruitPermanentCompanion(
			State.CardRun.CompanionRoster,
			TEXT("Companion.Guard.01"),
			57312,
			BenchRecruitResult,
			&Error));
	if (BenchRecruitResult.Outcome != EGameXXKCompanionRecruitOutcome::Recruited)
	{
		AddError(TEXT("the permanent companion progression fixture needs a distinct bench companion."));
		return false;
	}

	TestTrue(TEXT("the one permanent partner joins the normal card battle"), BeginLinearCardBattle(State));
	ForceCardBattleVictory(State);
	TestTrue(TEXT("victory first creates the explicit three-card reward gate"), UGameXXKMVPRules::ResolveBattleVictory(State, false));
	const FGameXXKPermanentCompanion* BeforeReward = State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([&ActiveCompanion](const FGameXXKPermanentCompanion& Candidate)
	{
		return Candidate.InstanceId == ActiveCompanion.InstanceId;
	});
	TestNotNull(TEXT("the active permanent partner persists through the pending reward"), BeforeReward);
	if (!BeforeReward)
	{
		return false;
	}
	TestEqual(TEXT("pending card reward does not prematurely grant companion experience"), BeforeReward->Experience, 0);

	TestTrue(TEXT("skipping the visible reward resolves the post-battle gate"),
		FGameXXKCardBattleAdapter::SkipPendingRouteReward(State, &Error));
	TestTrue(TEXT("the resolved victory returns to the route map"), UGameXXKMVPRules::ResolveBattleVictory(State, false));

	const FGameXXKPermanentCompanion* RewardedActive = State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([&ActiveCompanion](const FGameXXKPermanentCompanion& Candidate)
	{
		return Candidate.InstanceId == ActiveCompanion.InstanceId;
	});
	const FGameXXKPermanentCompanion* BenchCompanion = State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([&BenchRecruitResult](const FGameXXKPermanentCompanion& Candidate)
	{
		return Candidate.InstanceId == BenchRecruitResult.Companion.InstanceId;
	});
	TestNotNull(TEXT("the active partner remains in the persistent roster after victory"), RewardedActive);
	TestNotNull(TEXT("the bench partner remains in the persistent roster after victory"), BenchCompanion);
	if (!RewardedActive || !BenchCompanion)
	{
		return false;
	}
	TestTrue(TEXT("only the active permanent partner earns battle experience"), RewardedActive->Experience > 0);
	TestEqual(TEXT("a non-participating bench partner does not receive battle experience"), BenchCompanion->Experience, 0);
	TestEqual(TEXT("the accepted Qingshan route keeps its fixed task-NPC support after battle rewards"),
		State.CardRun.PartySelection.QuestNpc.NpcId, FName(TEXT("Npc.TusiChief")));
	return true;
}

#endif
