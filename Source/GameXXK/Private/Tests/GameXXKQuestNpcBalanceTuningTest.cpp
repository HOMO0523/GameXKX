#include "Misc/AutomationTest.h"

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

#endif
