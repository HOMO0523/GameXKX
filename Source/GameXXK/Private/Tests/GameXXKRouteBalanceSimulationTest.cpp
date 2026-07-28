#include "Misc/AutomationTest.h"

#include "GameXXKEncounterRules.h"
#include "GameXXKEnemyCatalog.h"
#include "GameXXKRouteBalanceRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	uint32 FoldRouteBalanceFingerprint(uint32 Hash, const FString& Text)
	{
		for (const TCHAR Character : Text)
		{
			Hash ^= static_cast<uint32>(Character);
			Hash *= 16777619u;
		}
		Hash ^= 0xFFu;
		Hash *= 16777619u;
		return Hash;
	}

	void AppendSortedMetricMap(FString& InOutText, const TCHAR* Label, const TMap<FName, int64>& Metrics)
	{
		TArray<FName> Keys;
		Metrics.GenerateKeyArray(Keys);
		Keys.Sort([](const FName& Left, const FName& Right)
		{
			return Left.LexicalLess(Right);
		});
		InOutText += FString::Printf(TEXT("|%s="), Label);
		for (const FName& Key : Keys)
		{
			InOutText += FString::Printf(TEXT("%s:%lld,"), *Key.ToString(), Metrics.FindChecked(Key));
		}
	}

	uint32 MakeRouteBalanceResultFingerprint(const FGameXXKRouteBalanceCaseResult& Result)
	{
		const FGameXXKSimulationMetrics& Metrics = Result.Metrics;
		FString Text = FString::Printf(
			TEXT("cohort=%s|seed=%d|chapter=%d|node=%d|victory=%d|rounds=%d|party_hp=%d|first_round_deaths=%d|failure=%s"),
			*Result.Case.CohortId.ToString(),
			Result.Case.Seed,
			Result.Case.Chapter,
			static_cast<int32>(Result.Case.NodeKind),
			Metrics.bVictory ? 1 : 0,
			Metrics.Rounds,
			Metrics.RemainingPartyHealth,
			Metrics.FirstRoundDeaths,
			*Metrics.FailureReason.ToString());
		AppendSortedMetricMap(Text, TEXT("damage"), Metrics.DamageBySource);
		AppendSortedMetricMap(Text, TEXT("healing"), Metrics.HealingBySource);
		AppendSortedMetricMap(Text, TEXT("armor"), Metrics.ArmorBySource);
		AppendSortedMetricMap(Text, TEXT("status_produced"), Metrics.StatusProduced);
		AppendSortedMetricMap(Text, TEXT("status_consumed"), Metrics.StatusConsumed);
		for (const FGameXXKRouteBalanceInitialEnemy& Enemy : Result.InitialEnemies)
		{
			Text += FString::Printf(
				TEXT("|enemy=%s:%d:%d:%d:%d:%d"),
				*Enemy.DefinitionId.ToString(),
				Enemy.BattleSlotNumber,
				Enemy.CombatLevel,
				Enemy.MaxHP,
				Enemy.Attack,
				Enemy.Defense);
		}
		return FoldRouteBalanceFingerprint(2166136261u, Text);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteBalanceFullMatrixContractTest,
	"GameXXK.RouteBalance.FullMatrixContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteBalanceFullMatrixContractTest::RunTest(const FString& Parameters)
{
	const FGameXXKRouteBalanceMatrix Matrix = FGameXXKRouteBalanceRules::MakeLockedFullMatrix();
	TArray<FGameXXKRouteBalanceCase> Cases;
	FString Error;
	TestTrue(TEXT("the locked full matrix expands"), FGameXXKRouteBalanceRules::ExpandCases(Matrix, Cases, &Error));
	TestEqual(TEXT("the locked full matrix contains exactly 2400 real-rule cases"), Cases.Num(), 2400);
	for (const FGameXXKRouteBalanceCase& Case : Cases)
	{
		const int32 ExpectedRouteLevel = Case.Chapter == 1 ? 5 : Case.Chapter == 2 ? 10 : 15;
		TestTrue(TEXT("every expanded fixture has a fixed positive seed"), Case.Seed > 0);
		TestEqual(TEXT("every fixture's route snapshot matches its chapter"), Case.RouteLevel, ExpectedRouteLevel);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteBalanceSingleCaseRealRulesTest,
	"GameXXK.RouteBalance.SingleCaseRealRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteBalanceSingleCaseRealRulesTest::RunTest(const FString& Parameters)
{
	TArray<FGameXXKRouteBalanceCase> Cases;
	FString Error;
	TestTrue(TEXT("the fixture source expands before a real-rule case runs"),
		FGameXXKRouteBalanceRules::ExpandCases(FGameXXKRouteBalanceRules::MakeLockedFullMatrix(), Cases, &Error));
	if (Cases.IsEmpty())
	{
		return false;
	}

	FGameXXKRouteBalanceCaseResult Result;
	TArray<FGameXXKSimulationTraceEntry> Trace;
	TestTrue(FString::Printf(TEXT("one fixed case reaches a terminal real-rule battle: %s"), *Error),
		FGameXXKRouteBalanceRules::RunCase(Cases[0], Result, &Error, nullptr, &Trace));
	TestTrue(TEXT("the fixed case ends in a victory or normal defeat"),
		Result.Metrics.bVictory || Result.Metrics.FailureReason == TEXT("Simulation.Defeat"));
	TestTrue(TEXT("the fixed case records a finite positive round count"), Result.Metrics.Rounds > 0 && Result.Metrics.Rounds <= 100);
	TestTrue(TEXT("the fixed case exposes a non-empty rule action trace for diagnostics"), !Trace.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteBalanceNakedBaselineTraceTest,
	"GameXXK.RouteBalance.Diagnostics.NakedBaseline900001",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteBalanceNakedBaselineTraceTest::RunTest(const FString& Parameters)
{
	TArray<FGameXXKRouteBalanceCase> Cases;
	FString Error;
	TestTrue(TEXT("the locked matrix expands before the focused trace diagnostic"),
		FGameXXKRouteBalanceRules::ExpandCases(FGameXXKRouteBalanceRules::MakeLockedFullMatrix(), Cases, &Error));
	const FGameXXKRouteBalanceCase* Case = Cases.FindByPredicate([](const FGameXXKRouteBalanceCase& Candidate)
	{
		return Candidate.CohortId == TEXT("NakedBaseline") && Candidate.Seed == 900001;
	});
	TestNotNull(TEXT("the focused cross-restart case exists"), Case);
	if (!Case)
	{
		return false;
	}

	FGameXXKRouteBalanceCaseResult Result;
	TArray<FGameXXKSimulationTraceEntry> Trace;
	TestTrue(FString::Printf(TEXT("the focused case resolves: %s"), *Error),
		FGameXXKRouteBalanceRules::RunCase(*Case, Result, &Error, nullptr, &Trace));
	for (const FGameXXKRouteBalanceInitialEnemy& Enemy : Result.InitialEnemies)
	{
		AddInfo(FString::Printf(
			TEXT("[RouteBalanceTraceEnemy] id=%s slot=%d level=%d hp=%d attack=%d defense=%d"),
			*Enemy.DefinitionId.ToString(),
			Enemy.BattleSlotNumber,
			Enemy.CombatLevel,
			Enemy.MaxHP,
			Enemy.Attack,
			Enemy.Defense));
	}
	for (const FGameXXKSimulationTraceEntry& Entry : Trace)
	{
		AddInfo(FString::Printf(
			TEXT("[RouteBalanceTraceAction] round=%d action=%s source=%s card_or_intent=%s target=%s hp=%d mana=%d armor=%d"),
			Entry.Round,
			*Entry.Action.ToString(),
			*Entry.SourceUnitId.ToString(),
			*Entry.CardOrIntentId.ToString(),
			*Entry.TargetUnitId.ToString(),
			Entry.HealthDelta,
			Entry.ManaDelta,
			Entry.ArmorDelta));
	}
	AddInfo(FString::Printf(
		TEXT("[RouteBalanceTraceSummary] victory=%d rounds=%d party_hp=%d first_round_deaths=%d failure=%s trace_entries=%d"),
		Result.Metrics.bVictory ? 1 : 0,
		Result.Metrics.Rounds,
		Result.Metrics.RemainingPartyHealth,
		Result.Metrics.FirstRoundDeaths,
		*Result.Metrics.FailureReason.ToString(),
		Trace.Num()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteBalanceChapterTwoNormalReplayTest,
	"GameXXK.RouteBalance.Determinism.ChapterTwoNormalReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteBalanceChapterTwoNormalReplayTest::RunTest(const FString& Parameters)
{
	TArray<FGameXXKRouteBalanceCase> ExpandedCases;
	FString Error;
	TestTrue(TEXT("the locked matrix expands before replay verification"),
		FGameXXKRouteBalanceRules::ExpandCases(FGameXXKRouteBalanceRules::MakeLockedFullMatrix(), ExpandedCases, &Error));

	TArray<FGameXXKRouteBalanceCase> Cases;
	for (const FGameXXKRouteBalanceCase& Candidate : ExpandedCases)
	{
		if (Candidate.Chapter == 2 && Candidate.NodeKind == EGameXXKNodeKind::Battle)
		{
			Cases.Add(Candidate);
		}
	}
	TestEqual(TEXT("the chapter-two normal diagnostic covers the fixed stratified slice"), Cases.Num(), 267);
	if (Cases.Num() != 267)
	{
		return false;
	}

	TArray<uint32> BaselineFingerprints;
	BaselineFingerprints.Reserve(Cases.Num());
	uint32 BaselineAggregate = 2166136261u;
	for (const FGameXXKRouteBalanceCase& Case : Cases)
	{
		FGameXXKRouteBalanceCaseResult Result;
		if (!FGameXXKRouteBalanceRules::RunCase(Case, Result, &Error))
		{
			AddError(FString::Printf(TEXT("baseline diagnostic case did not resolve: %s"), *Error));
			return false;
		}
		const uint32 Fingerprint = MakeRouteBalanceResultFingerprint(Result);
		BaselineFingerprints.Add(Fingerprint);
		BaselineAggregate = FoldRouteBalanceFingerprint(BaselineAggregate, FString::Printf(TEXT("%08X"), Fingerprint));
		AddInfo(FString::Printf(
			TEXT("[RouteBalanceDeterminismCase] cohort=%s seed_ordinal=%d seed=%d fingerprint=0x%08X"),
			*Case.CohortId.ToString(),
			Case.SeedOrdinal,
			Case.Seed,
			Fingerprint));
	}

	uint32 ReplayAggregate = 2166136261u;
	for (int32 Index = 0; Index < Cases.Num(); ++Index)
	{
		FGameXXKRouteBalanceCaseResult ReplayResult;
		if (!FGameXXKRouteBalanceRules::RunCase(Cases[Index], ReplayResult, &Error))
		{
			AddError(FString::Printf(TEXT("replay diagnostic case did not resolve: %s"), *Error));
			return false;
		}
		const uint32 ReplayFingerprint = MakeRouteBalanceResultFingerprint(ReplayResult);
		TestEqual(
			FString::Printf(TEXT("fixed replay remains identical for %s seed %d"), *Cases[Index].CohortId.ToString(), Cases[Index].Seed),
			ReplayFingerprint,
			BaselineFingerprints[Index]);
		ReplayAggregate = FoldRouteBalanceFingerprint(ReplayAggregate, FString::Printf(TEXT("%08X"), ReplayFingerprint));
	}
	AddInfo(FString::Printf(
		TEXT("[RouteBalanceDeterminism] chapter=2 node=Battle cases=%d baseline=0x%08X replay=0x%08X"),
		Cases.Num(),
		BaselineAggregate,
		ReplayAggregate));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteBalanceRouteSnapshotFormationTest,
	"GameXXK.RouteBalance.RouteSnapshotFormation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteBalanceRouteSnapshotFormationTest::RunTest(const FString& Parameters)
{
	constexpr int32 RouteLevel = 15;
	TArray<FGameXXKEncounterSlot> Formation;
	FString Error;
	TestTrue(TEXT("a chapter-three boss formation accepts the route level snapshot"),
		FGameXXKEncounterRules::BuildFormation(
			3,
			EGameXXKNodeKind::Boss,
			FGameXXKEncounterRules::DeriveChapterSeed(910001, 3),
			910001,
			RouteLevel,
			Formation,
			&Error));
	TestEqual(TEXT("a boss formation retains all three battle slots"), Formation.Num(), 3);
	for (const FGameXXKEncounterSlot& Slot : Formation)
	{
		const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Slot.EnemyDefinitionId);
		TestNotNull(TEXT("every generated formation slot resolves to a catalog enemy"), Definition);
		if (Definition)
		{
			TestEqual(
				TEXT("every generated enemy uses its tier-adjusted route level"),
				Slot.CombatLevel,
				FGameXXKEncounterRules::GetCombatLevel(Definition->Tier, RouteLevel));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteBalanceCalibrationProjectionTest,
	"GameXXK.RouteBalance.CalibrationProfileProjectsExactEnemyStats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteBalanceCalibrationProjectionTest::RunTest(const FString& Parameters)
{
	TArray<FGameXXKRouteBalanceCase> Cases;
	FString Error;
	TestTrue(TEXT("the locked cases expand before calibration projection"),
		FGameXXKRouteBalanceRules::ExpandCases(FGameXXKRouteBalanceRules::MakeLockedFullMatrix(), Cases, &Error));
	const FGameXXKRouteBalanceCase* Case = Cases.FindByPredicate([](const FGameXXKRouteBalanceCase& Candidate)
	{
		return Candidate.Chapter == 3 && Candidate.NodeKind == EGameXXKNodeKind::Boss;
	});
	TestNotNull(TEXT("a deterministic chapter-three boss fixture exists"), Case);
	if (!Case)
	{
		return false;
	}

	FGameXXKRouteBalanceCaseResult Baseline;
	TestTrue(FString::Printf(TEXT("the unscaled boss fixture resolves: %s"), *Error),
		FGameXXKRouteBalanceRules::RunCase(*Case, Baseline, &Error));

	FGameXXKRouteBalanceCalibrationProfile Profile;
	Profile.ProfileId = TEXT("Test.Ch3BossScale");
	Profile.EncounterScales.Add(
		FGameXXKRouteBalanceCalibrationProfile::MakeEncounterKey(3, EGameXXKNodeKind::Boss),
		FGameXXKRouteBalanceStatScale{175, 125, 150});
	FGameXXKRouteBalanceCaseResult Scaled;
	TestTrue(FString::Printf(TEXT("the scaled boss fixture resolves through the same battle rules: %s"), *Error),
		FGameXXKRouteBalanceRules::RunCase(*Case, Scaled, &Error, &Profile));

	TestEqual(TEXT("the scaled projection preserves its three enemy identities"), Scaled.InitialEnemies.Num(), Baseline.InitialEnemies.Num());
	for (int32 Index = 0; Index < FMath::Min(Baseline.InitialEnemies.Num(), Scaled.InitialEnemies.Num()); ++Index)
	{
		TestEqual(TEXT("calibration never changes the selected enemy definition"),
			Scaled.InitialEnemies[Index].DefinitionId, Baseline.InitialEnemies[Index].DefinitionId);
		TestEqual(TEXT("calibration never changes the route combat level"),
			Scaled.InitialEnemies[Index].CombatLevel, Baseline.InitialEnemies[Index].CombatLevel);
		const FGameXXKEnemyComputedStats RawStats = FGameXXKEnemyCatalog::ComputeStats(
			Baseline.InitialEnemies[Index].DefinitionId,
			Baseline.InitialEnemies[Index].CombatLevel);
		TestEqual(TEXT("calibration scales initial max health deterministically"),
			Scaled.InitialEnemies[Index].MaxHP, FGameXXKEncounterRules::ScaleStat(RawStats.MaxHP, 175, 1));
		TestEqual(TEXT("calibration scales initial attack deterministically"),
			Scaled.InitialEnemies[Index].Attack, FGameXXKEncounterRules::ScaleStat(RawStats.Attack, 125, 1));
		TestEqual(TEXT("calibration scales initial defense deterministically"),
			Scaled.InitialEnemies[Index].Defense, FGameXXKEncounterRules::ScaleStat(RawStats.Defense, 150, 0));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteBalanceCalibrationCoarseSweepTest,
	"GameXXK.RouteBalance.CalibrationCoarseSweep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteBalanceCalibrationCoarseSweepTest::RunTest(const FString& Parameters)
{
	struct FCalibrationSweepFixture
	{
		FName Id = NAME_None;
	FGameXXKRouteBalanceCalibrationProfile Profile;
	};

	auto AddScale = [](FGameXXKRouteBalanceCalibrationProfile& Profile,
		const int32 Chapter,
		const EGameXXKNodeKind NodeKind,
		const int32 MaxHPPercent,
		const int32 AttackPercent,
		const int32 DefensePercent)
	{
		Profile.EncounterScales.Add(
			FGameXXKRouteBalanceCalibrationProfile::MakeEncounterKey(Chapter, NodeKind),
			FGameXXKRouteBalanceStatScale{MaxHPPercent, AttackPercent, DefensePercent});
	};

	auto MakePressureProfile = [&AddScale](
		const FName Id,
		const int32 NormalHP,
		const int32 NormalAttack,
		const int32 EliteHP,
		const int32 EliteAttack,
		const int32 EliteDefense,
		const int32 ChapterOneBossHP,
		const int32 ChapterOneBossAttack,
		const int32 ChapterThreeBossHP,
		const int32 ChapterThreeBossAttack)
	{
		FCalibrationSweepFixture Fixture;
		Fixture.Id = Id;
		Fixture.Profile.ProfileId = Id;
		for (int32 Chapter = 1; Chapter <= 3; ++Chapter)
		{
			AddScale(Fixture.Profile, Chapter, EGameXXKNodeKind::Battle, NormalHP, NormalAttack, 100);
			AddScale(Fixture.Profile, Chapter, EGameXXKNodeKind::Elite, EliteHP, EliteAttack, EliteDefense);
		}
		AddScale(Fixture.Profile, 1, EGameXXKNodeKind::Boss, ChapterOneBossHP, ChapterOneBossAttack, 100);
		AddScale(Fixture.Profile, 3, EGameXXKNodeKind::Boss, ChapterThreeBossHP, ChapterThreeBossAttack, 100);
		return Fixture;
	};

	auto OverrideEliteScale = [&AddScale](FCalibrationSweepFixture& Fixture,
		const int32 Chapter,
		const int32 MaxHPPercent,
		const int32 AttackPercent,
		const int32 DefensePercent)
	{
		AddScale(Fixture.Profile, Chapter, EGameXXKNodeKind::Elite, MaxHPPercent, AttackPercent, DefensePercent);
	};

	auto OverrideNormalAttack = [&AddScale](FCalibrationSweepFixture& Fixture, const int32 Chapter, const int32 AttackPercent)
	{
		AddScale(Fixture.Profile, Chapter, EGameXXKNodeKind::Battle, 140, AttackPercent, 100);
	};

	TArray<FGameXXKRouteBalanceCase> Cases;
	FString Error;
	TestTrue(TEXT("the locked cases expand before a calibration coarse sweep"),
		FGameXXKRouteBalanceRules::ExpandCases(FGameXXKRouteBalanceRules::MakeLockedFullMatrix(), Cases, &Error));
	if (Cases.IsEmpty())
	{
		return false;
	}

	TArray<FCalibrationSweepFixture> Fixtures;
	Fixtures.Reserve(3);
	auto AddRefinementFixture = [&MakePressureProfile, &OverrideNormalAttack, &OverrideEliteScale, &Fixtures](
		const FName Id,
		const int32 ChapterOneNormalAttack,
		const int32 ChapterTwoNormalAttack,
		const int32 ChapterThreeNormalAttack)
	{
		FCalibrationSweepFixture Fixture = MakePressureProfile(Id, 140, 150, 160, 180, 110, 120, 120, 80, 90);
		OverrideNormalAttack(Fixture, 1, ChapterOneNormalAttack);
		OverrideNormalAttack(Fixture, 2, ChapterTwoNormalAttack);
		OverrideNormalAttack(Fixture, 3, ChapterThreeNormalAttack);
		OverrideEliteScale(Fixture, 1, 160, 265, 100);
		OverrideEliteScale(Fixture, 2, 160, 170, 105);
		OverrideEliteScale(Fixture, 3, 160, 180, 110);
		Fixtures.Add(MoveTemp(Fixture));
	};
	AddRefinementFixture(TEXT("RefineA"), 650, 500, 500);
	AddRefinementFixture(TEXT("RefineB"), 750, 525, 525);
	AddRefinementFixture(TEXT("RefineC"), 850, 550, 550);

	for (const FCalibrationSweepFixture& Fixture : Fixtures)
	{
		int32 BucketCount[4][3] = {};
		int32 BucketVictories[4][3] = {};
		for (const FGameXXKRouteBalanceCase& Case : Cases)
		{
			FGameXXKRouteBalanceCaseResult Result;
			if (!FGameXXKRouteBalanceRules::RunCase(Case, Result, &Error, &Fixture.Profile))
			{
				AddError(FString::Printf(TEXT("sweep profile %s failed to resolve a real-rule fixture: %s"), *Fixture.Id.ToString(), *Error));
				return false;
			}
			const int32 NodeIndex = Case.NodeKind == EGameXXKNodeKind::Battle ? 0 : Case.NodeKind == EGameXXKNodeKind::Elite ? 1 : 2;
			++BucketCount[Case.Chapter][NodeIndex];
			BucketVictories[Case.Chapter][NodeIndex] += Result.Metrics.bVictory ? 1 : 0;
		}
		for (int32 Chapter = 1; Chapter <= 3; ++Chapter)
		{
			for (int32 NodeIndex = 0; NodeIndex < 3; ++NodeIndex)
			{
				const TCHAR* NodeLabel = NodeIndex == 0 ? TEXT("Normal") : NodeIndex == 1 ? TEXT("Elite") : TEXT("Boss");
				const double VictoryRate = BucketCount[Chapter][NodeIndex] > 0
					? static_cast<double>(BucketVictories[Chapter][NodeIndex]) / static_cast<double>(BucketCount[Chapter][NodeIndex])
					: 0.0;
				AddInfo(FString::Printf(
					TEXT("[RouteBalanceSweep] Profile=%s Chapter=%d Node=%s Win=%d/%d Rate=%.4f"),
					*Fixture.Id.ToString(),
					Chapter,
					NodeLabel,
					BucketVictories[Chapter][NodeIndex],
					BucketCount[Chapter][NodeIndex],
					VictoryRate));
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteBalanceFinalCandidateTest,
	"GameXXK.RouteBalance.FinalCandidateTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteBalanceFinalCandidateTest::RunTest(const FString& Parameters)
{
	FGameXXKRouteBalanceCalibrationProfile Profile;
	Profile.ProfileId = TEXT("RouteBalance.FinalCandidate.v1");
	auto AddScale = [&Profile](const int32 Chapter,
		const EGameXXKNodeKind NodeKind,
		const int32 MaxHPPercent,
		const int32 AttackPercent,
		const int32 DefensePercent)
	{
		Profile.EncounterScales.Add(
			FGameXXKRouteBalanceCalibrationProfile::MakeEncounterKey(Chapter, NodeKind),
			FGameXXKRouteBalanceStatScale{MaxHPPercent, AttackPercent, DefensePercent});
	};
	AddScale(1, EGameXXKNodeKind::Battle, 140, 850, 100);
	AddScale(2, EGameXXKNodeKind::Battle, 140, 540, 100);
	AddScale(3, EGameXXKNodeKind::Battle, 140, 540, 100);
	AddScale(1, EGameXXKNodeKind::Elite, 160, 270, 100);
	AddScale(2, EGameXXKNodeKind::Elite, 160, 170, 105);
	AddScale(3, EGameXXKNodeKind::Elite, 160, 180, 110);
	AddScale(1, EGameXXKNodeKind::Boss, 120, 120, 100);
	AddScale(3, EGameXXKNodeKind::Boss, 80, 90, 100);

	TArray<FGameXXKRouteBalanceCase> Cases;
	FString Error;
	TestTrue(TEXT("the locked cases expand before final-candidate certification"),
		FGameXXKRouteBalanceRules::ExpandCases(FGameXXKRouteBalanceRules::MakeLockedFullMatrix(), Cases, &Error));
	if (Cases.Num() != 2400)
	{
		return false;
	}

	int32 BucketCount[4][3] = {};
	int32 BucketVictories[4][3] = {};
	for (const FGameXXKRouteBalanceCase& Case : Cases)
	{
		FGameXXKRouteBalanceCaseResult Result;
		if (!FGameXXKRouteBalanceRules::RunCase(Case, Result, &Error, &Profile))
		{
			AddError(FString::Printf(TEXT("the final candidate failed a real-rule fixture: %s"), *Error));
			return false;
		}
		const int32 NodeIndex = Case.NodeKind == EGameXXKNodeKind::Battle ? 0 : Case.NodeKind == EGameXXKNodeKind::Elite ? 1 : 2;
		++BucketCount[Case.Chapter][NodeIndex];
		BucketVictories[Case.Chapter][NodeIndex] += Result.Metrics.bVictory ? 1 : 0;
	}

	const double MinimumRates[3] = {0.55, 0.35, 0.15};
	const double MaximumRates[3] = {0.70, 0.50, 0.35};
	for (int32 Chapter = 1; Chapter <= 3; ++Chapter)
	{
		for (int32 NodeIndex = 0; NodeIndex < 3; ++NodeIndex)
		{
			const double VictoryRate = static_cast<double>(BucketVictories[Chapter][NodeIndex]) / static_cast<double>(BucketCount[Chapter][NodeIndex]);
			const TCHAR* NodeLabel = NodeIndex == 0 ? TEXT("Normal") : NodeIndex == 1 ? TEXT("Elite") : TEXT("Boss");
			AddInfo(FString::Printf(
				TEXT("[RouteBalanceFinal] Chapter=%d Node=%s Win=%d/%d Rate=%.4f Target=[%.2f,%.2f]"),
				Chapter,
				NodeLabel,
				BucketVictories[Chapter][NodeIndex],
				BucketCount[Chapter][NodeIndex],
				VictoryRate,
				MinimumRates[NodeIndex],
				MaximumRates[NodeIndex]));
			TestTrue(
				FString::Printf(TEXT("chapter %d %s final win rate is within the locked target range"), Chapter, NodeLabel),
				VictoryRate >= MinimumRates[NodeIndex] && VictoryRate <= MaximumRates[NodeIndex]);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteBalanceFullMatrixExecutionTest,
	"GameXXK.RouteBalance.FullMatrixExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteBalanceFullMatrixExecutionTest::RunTest(const FString& Parameters)
{
	FGameXXKRouteBalanceReport Report;
	FString Error;
	TestTrue(FString::Printf(TEXT("all locked balance cases resolve through the real rules: %s"), *Error),
		FGameXXKRouteBalanceRules::RunFullMatrix(
			FGameXXKRouteBalanceRules::MakeLockedFullMatrix(),
			Report,
			&Error));
	TestEqual(TEXT("full execution records exactly 2400 fixed cases"), Report.Results.Num(), 2400);
	TestEqual(TEXT("full execution aggregates every chapter and node-kind combination"), Report.Aggregates.Num(), 9);
	int32 AggregatedCaseCount = 0;
	for (const FGameXXKRouteBalanceAggregate& Aggregate : Report.Aggregates)
	{
		AggregatedCaseCount += Aggregate.CaseCount;
		TestTrue(TEXT("every chapter/node aggregate has enough stratified samples"), Aggregate.CaseCount >= 264);
		int32 TotalRounds = 0;
		int32 TotalRemainingPartyHealth = 0;
		int32 FirstRoundDeaths = 0;
		for (const FGameXXKRouteBalanceCaseResult& Result : Report.Results)
		{
			if (Result.Case.Chapter == Aggregate.Chapter && Result.Case.NodeKind == Aggregate.NodeKind)
			{
				TotalRounds += Result.Metrics.Rounds;
				TotalRemainingPartyHealth += Result.Metrics.RemainingPartyHealth;
				FirstRoundDeaths += Result.Metrics.FirstRoundDeaths;
			}
		}
		AddInfo(FString::Printf(
			TEXT("[RouteBalanceFull] chapter=%d node=%d samples=%d wins=%d win_rate=%.4f mean_rounds=%.2f mean_party_hp=%.2f first_round_deaths=%d"),
			Aggregate.Chapter,
			static_cast<int32>(Aggregate.NodeKind),
			Aggregate.CaseCount,
			Aggregate.VictoryCount,
			Aggregate.VictoryRate,
			Aggregate.CaseCount > 0 ? static_cast<double>(TotalRounds) / static_cast<double>(Aggregate.CaseCount) : 0.0,
			Aggregate.CaseCount > 0 ? static_cast<double>(TotalRemainingPartyHealth) / static_cast<double>(Aggregate.CaseCount) : 0.0,
			FirstRoundDeaths));
	}
	TestEqual(TEXT("the aggregate rows account for every case exactly once"), AggregatedCaseCount, 2400);
	for (const FGameXXKRouteBalanceCohort& Cohort : FGameXXKRouteBalanceRules::MakeLockedFullMatrix().Cohorts)
	{
		int32 CohortCases = 0;
		int32 CohortWins = 0;
		for (const FGameXXKRouteBalanceCaseResult& Result : Report.Results)
		{
			if (Result.Case.CohortId == Cohort.CohortId)
			{
				++CohortCases;
				CohortWins += Result.Metrics.bVictory ? 1 : 0;
			}
		}
		AddInfo(FString::Printf(
			TEXT("[RouteBalanceCohort] cohort=%s samples=%d wins=%d win_rate=%.4f"),
			*Cohort.CohortId.ToString(),
			CohortCases,
			CohortWins,
			CohortCases > 0 ? static_cast<double>(CohortWins) / static_cast<double>(CohortCases) : 0.0));
		for (const FGameXXKRouteBalanceAggregate& Aggregate : Report.Aggregates)
		{
			int32 BucketCases = 0;
			int32 BucketWins = 0;
			for (const FGameXXKRouteBalanceCaseResult& Result : Report.Results)
			{
				if (Result.Case.CohortId == Cohort.CohortId
					&& Result.Case.Chapter == Aggregate.Chapter
					&& Result.Case.NodeKind == Aggregate.NodeKind)
				{
					++BucketCases;
					BucketWins += Result.Metrics.bVictory ? 1 : 0;
				}
			}
			AddInfo(FString::Printf(
				TEXT("[RouteBalanceBucket] cohort=%s chapter=%d node=%d samples=%d wins=%d win_rate=%.4f"),
				*Cohort.CohortId.ToString(),
				Aggregate.Chapter,
				static_cast<int32>(Aggregate.NodeKind),
				BucketCases,
				BucketWins,
				BucketCases > 0 ? static_cast<double>(BucketWins) / static_cast<double>(BucketCases) : 0.0));
		}
	}
	AddInfo(FString::Printf(TEXT("[RouteBalanceFull] cases=%d elapsed_seconds=%.3f"), Report.Results.Num(), Report.ElapsedSeconds));
	return true;
}

#endif
