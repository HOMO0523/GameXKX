#include "Misc/AutomationTest.h"

#include "GameXXKCompanionRules.h"
#include "GameXXKRouteBalanceRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKYueBaiTaskLoopBalanceTest,
	"GameXXK.RouteBalance.NpcTuning.YueBaiTaskLoopSeed1300216",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKYueBaiTaskLoopBalanceTest::RunTest(const FString& Parameters)
{
	TArray<FGameXXKRouteBalanceCase> Cases;
	FString Error;
	if (!TestTrue(TEXT("orthogonal cases build for the fixed YueBai regression seed"),
		FGameXXKRouteBalanceRules::MakeOrthogonalCases(Cases, &Error)))
	{
		return false;
	}
	const FGameXXKRouteBalanceCase* Case = Cases.FindByPredicate([](const FGameXXKRouteBalanceCase& Candidate)
	{
		return Candidate.DimensionId == TEXT("QuestNpc")
			&& Candidate.VariantId == TEXT("YueBai")
			&& Candidate.NodeKind == EGameXXKNodeKind::Boss
			&& Candidate.Seed == 1300216;
	});
	if (!TestNotNull(TEXT("the locked YueBai boss seed exists"), Case))
	{
		return false;
	}
	FGameXXKRouteBalanceCaseResult Result;
	TArray<FGameXXKSimulationTraceEntry> Trace;
	if (!TestTrue(FString::Printf(TEXT("the YueBai fixed seed reaches a normal terminal state: %s"), *Error),
		FGameXXKRouteBalanceRules::RunCase(*Case, Result, &Error, nullptr, &Trace)))
	{
		return false;
	}
	if (Result.Metrics.AutomaticResolutionCount < 4)
	{
		for (const FGameXXKSimulationTraceEntry& Entry : Trace)
		{
			AddInfo(FString::Printf(
				TEXT("[YueBaiTaskLoopTrace] round=%d action=%s source=%s card=%s target=%s hp=%d mana=%d armor=%d"),
				Entry.Round,
				*Entry.Action.ToString(),
				*Entry.SourceUnitId.ToString(),
				*Entry.CardOrIntentId.ToString(),
				*Entry.TargetUnitId.ToString(),
				Entry.HealthDelta,
				Entry.ManaDelta,
				Entry.ArmorDelta));
		}
	}
	TestTrue(TEXT("YueBai's three-card loop completes at least once before the fixed boss seed ends"),
		Result.Metrics.AutomaticResolutionCount >= 4);
	TestEqual(TEXT("the YueBai loop never creates an unplayable target stall"),
		Result.Metrics.StrandedTargetFailures,
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKYueBaiSurvivalBudgetTest,
	"GameXXK.RouteBalance.NpcTuning.YueBaiSurvivalBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKYueBaiSurvivalBudgetTest::RunTest(const FString& Parameters)
{
	TArray<FGameXXKRouteBalanceCase> Cases;
	FString Error;
	if (!TestTrue(TEXT("orthogonal cases build for the YueBai survival budget"),
		FGameXXKRouteBalanceRules::MakeOrthogonalCases(Cases, &Error)))
	{
		return false;
	}

	int32 YueBaiCaseCount = 0;
	int32 BattleVictories = 0;
	int32 EliteVictories = 0;
	int32 BossVictories = 0;
	int32 StrandedTargetFailures = 0;
	for (const FGameXXKRouteBalanceCase& Case : Cases)
	{
		if (Case.DimensionId != TEXT("QuestNpc") || Case.VariantId != TEXT("YueBai"))
		{
			continue;
		}
		++YueBaiCaseCount;
		FGameXXKRouteBalanceCaseResult Result;
		if (!FGameXXKRouteBalanceRules::RunCase(Case, Result, &Error))
		{
			AddError(FString::Printf(TEXT("YueBai case seed %d failed an invariant: %s"), Case.Seed, *Error));
			continue;
		}
		StrandedTargetFailures += Result.Metrics.StrandedTargetFailures;
		if (!Result.Metrics.bVictory)
		{
			continue;
		}
		switch (Case.NodeKind)
		{
		case EGameXXKNodeKind::Battle:
			++BattleVictories;
			break;
		case EGameXXKNodeKind::Elite:
			++EliteVictories;
			break;
		case EGameXXKNodeKind::Boss:
			++BossVictories;
			break;
		default:
			AddError(TEXT("YueBai survival budget encountered an unexpected node kind."));
			break;
		}
	}
	AddInfo(FString::Printf(
		TEXT("[NpcBudget] YueBai cases=%d battle=%d elite=%d boss=%d stranded=%d"),
		YueBaiCaseCount,
		BattleVictories,
		EliteVictories,
		BossVictories,
		StrandedTargetFailures));

	TestEqual(TEXT("the YueBai orthogonal slice contains exactly ninety cases"), YueBaiCaseCount, 90);
	TestEqual(TEXT("YueBai keeps all thirty normal encounters"), BattleVictories, 30);
	TestTrue(TEXT("YueBai keeps at least eighteen elite victories"), EliteVictories >= 18);
	TestTrue(TEXT("YueBai reaches the first bounded boss-survival step of ten victories"), BossVictories >= 10);
	TestEqual(TEXT("YueBai never strands a selected target across the slice"), StrandedTargetFailures, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKZhouGuangZuDamageBudgetTest,
	"GameXXK.RouteBalance.NpcTuning.ZhouGuangZuDamageBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKZhouGuangZuDamageBudgetTest::RunTest(const FString& Parameters)
{
	TArray<FGameXXKRouteBalanceCase> Cases;
	FString Error;
	if (!TestTrue(TEXT("orthogonal cases build for the ZhouGuangZu damage budget"),
		FGameXXKRouteBalanceRules::MakeOrthogonalCases(Cases, &Error)))
	{
		return false;
	}

	int32 ZhouCaseCount = 0;
	int32 BattleCaseCount = 0;
	int32 EliteCaseCount = 0;
	int32 BossCaseCount = 0;
	int32 BattleVictories = 0;
	int32 EliteVictories = 0;
	int32 BossVictories = 0;
	int32 BossCasesWithoutYanFen = 0;
	int32 BossVictoriesWithoutYanFen = 0;
	int32 StrandedTargetFailures = 0;
	for (const FGameXXKRouteBalanceCase& Case : Cases)
	{
		if (Case.DimensionId != TEXT("QuestNpc") || Case.VariantId != TEXT("ZhouGuangZu"))
		{
			continue;
		}
		++ZhouCaseCount;
		switch (Case.NodeKind)
		{
		case EGameXXKNodeKind::Battle:
			++BattleCaseCount;
			break;
		case EGameXXKNodeKind::Elite:
			++EliteCaseCount;
			break;
		case EGameXXKNodeKind::Boss:
			++BossCaseCount;
			break;
		default:
			AddError(TEXT("ZhouGuangZu damage budget encountered an unexpected node kind."));
			break;
		}
		FGameXXKRouteBalanceCaseResult Result;
		if (!FGameXXKRouteBalanceRules::RunCase(Case, Result, &Error))
		{
			AddError(FString::Printf(TEXT("ZhouGuangZu case seed %d failed an invariant: %s"), Case.Seed, *Error));
			continue;
		}
		StrandedTargetFailures += Result.Metrics.StrandedTargetFailures;
		if (Case.NodeKind == EGameXXKNodeKind::Boss)
		{
			TArray<FName> SelectedCardIds;
			FString SelectionError;
			if (!FGameXXKCompanionRules::BuildQuestNpcRouteCardSelection(
				Case.QuestNpcId,
				Case.Seed,
				SelectedCardIds,
				&SelectionError))
			{
				AddError(FString::Printf(TEXT("ZhouGuangZu seed %d cannot reconstruct its route cards: %s"), Case.Seed, *SelectionError));
			}
			else if (!SelectedCardIds.Contains(TEXT("Npc.ZhouGuangZu.YanFenFengMai")))
			{
				++BossCasesWithoutYanFen;
				BossVictoriesWithoutYanFen += Result.Metrics.bVictory ? 1 : 0;
			}
		}
		if (!Result.Metrics.bVictory)
		{
			continue;
		}
		switch (Case.NodeKind)
		{
		case EGameXXKNodeKind::Battle:
			++BattleVictories;
			break;
		case EGameXXKNodeKind::Elite:
			++EliteVictories;
			break;
		case EGameXXKNodeKind::Boss:
			++BossVictories;
			break;
		default:
			AddError(TEXT("ZhouGuangZu damage budget encountered an unexpected node kind."));
			break;
		}
	}
	AddInfo(FString::Printf(
		TEXT("[NpcBudget] ZhouGuangZu cases=%d battle=%d/%d elite=%d/%d boss=%d/%d boss_without_yanfen=%d/%d stranded=%d"),
		ZhouCaseCount,
		BattleVictories,
		BattleCaseCount,
		EliteVictories,
		EliteCaseCount,
		BossVictories,
		BossCaseCount,
		BossVictoriesWithoutYanFen,
		BossCasesWithoutYanFen,
		StrandedTargetFailures));

	TestEqual(TEXT("the ZhouGuangZu orthogonal slice contains exactly ninety cases"), ZhouCaseCount, 90);
	TestEqual(TEXT("the ZhouGuangZu slice contains exactly thirty normal encounters"), BattleCaseCount, 30);
	TestEqual(TEXT("the ZhouGuangZu slice contains exactly thirty elite encounters"), EliteCaseCount, 30);
	TestEqual(TEXT("the ZhouGuangZu slice contains exactly thirty boss encounters"), BossCaseCount, 30);
	TestEqual(TEXT("ZhouGuangZu keeps all thirty normal encounters"), BattleVictories, 30);
	TestTrue(TEXT("ZhouGuangZu reaches the first bounded elite-damage step of ten victories"), EliteVictories >= 10);
	TestTrue(TEXT("ZhouGuangZu reaches the next bounded boss-damage step of six victories"), BossVictories >= 6);
	TestEqual(TEXT("the ZhouGuangZu boss slice contains seven routes without YanFenFengMai"), BossCasesWithoutYanFen, 7);
	TestTrue(TEXT("the healing-terrain trio can defeat at least one boss without YanFenFengMai"), BossVictoriesWithoutYanFen >= 1);
	TestEqual(TEXT("ZhouGuangZu never strands a selected target across the slice"), StrandedTargetFailures, 0);
	return true;
}

#endif
