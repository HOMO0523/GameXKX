#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GameXXKBalanceObservationCsvTestSupport.h"
#include "GameXXKRouteBalanceRules.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
	FString MakeUniqueCaseKey(const FGameXXKRouteBalanceCase& Case)
	{
		return FString::Printf(
			TEXT("%s|%s|%d|%d"),
			*Case.DimensionId.ToString(),
			*Case.VariantId.ToString(),
			static_cast<int32>(Case.NodeKind),
			Case.SeedOrdinal);
	}

	FString MakeControlGroupKey(const FGameXXKRouteBalanceCase& Case)
	{
		return FString::Printf(
			TEXT("%s|%d|%d"),
			*Case.DimensionId.ToString(),
			static_cast<int32>(Case.NodeKind),
			Case.SeedOrdinal);
	}

	bool HasMatchingControls(
		const FGameXXKRouteBalanceCase& Baseline,
		const FGameXXKRouteBalanceCase& Candidate,
		FString& OutMismatch)
	{
		auto Require = [&OutMismatch](const bool bCondition, const TCHAR* Field)
		{
			if (!bCondition && OutMismatch.IsEmpty())
			{
				OutMismatch = Field;
			}
		};

		Require(Baseline.CohortId == Candidate.CohortId, TEXT("CohortId"));
		Require(Baseline.NodeKind == Candidate.NodeKind, TEXT("NodeKind"));
		Require(Baseline.SeedOrdinal == Candidate.SeedOrdinal, TEXT("SeedOrdinal"));
		Require(Baseline.Seed == Candidate.Seed, TEXT("Seed"));
		Require(Baseline.CompanionCardSeed == Candidate.CompanionCardSeed, TEXT("CompanionCardSeed"));
		Require(Baseline.EquipmentQualityId == Candidate.EquipmentQualityId, TEXT("EquipmentQualityId"));
		Require(Baseline.EnhancementLevel == Candidate.EnhancementLevel, TEXT("EnhancementLevel"));

		const FName Dimension = Baseline.DimensionId;
		Require(Dimension == Candidate.DimensionId, TEXT("DimensionId"));
		if (Dimension != TEXT("Profession"))
		{
			Require(Baseline.CompanionTemplateId == Candidate.CompanionTemplateId, TEXT("CompanionTemplateId"));
		}
		if (Dimension != TEXT("EquipmentSet"))
		{
			Require(Baseline.EquipmentSetId == Candidate.EquipmentSetId, TEXT("EquipmentSetId"));
		}
		if (Dimension != TEXT("QuestNpc"))
		{
			Require(Baseline.QuestNpcId == Candidate.QuestNpcId, TEXT("QuestNpcId"));
		}
		if (Dimension != TEXT("Terrain"))
		{
			Require(Baseline.Terrain == Candidate.Terrain, TEXT("Terrain"));
		}
		if (Dimension != TEXT("Progression"))
		{
			Require(Baseline.Chapter == Candidate.Chapter, TEXT("Chapter"));
			Require(Baseline.RouteLevel == Candidate.RouteLevel, TEXT("RouteLevel"));
		}
		return OutMismatch.IsEmpty();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKOrthogonalBalanceObservationTest,
	"GameXXK.Diagnostics.OrthogonalBalanceObservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKOrthogonalBalanceObservationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBalanceObservationCsvTestSupport;
	FString RunId = TEXT("orthogonal_manual");
	FParse::Value(FCommandLine::Get(), TEXT("GameXXKBalanceObservationId="), RunId);
	if (!IsSafeRunId(RunId))
	{
		AddError(TEXT("GameXXKBalanceObservationId must contain only letters, digits, dash, or underscore."));
		return false;
	}

	TArray<FGameXXKRouteBalanceCase> Cases;
	FString Error;
	if (!TestTrue(
		FString::Printf(TEXT("orthogonal cases build from real route-balance fixtures: %s"), *Error),
		FGameXXKRouteBalanceRules::MakeOrthogonalCases(Cases, &Error)))
	{
		return false;
	}
	TestEqual(TEXT("the orthogonal matrix has the locked 2,520 cases"), Cases.Num(), 2520);
	if (Cases.Num() != 2520)
	{
		return false;
	}

	TMap<FName, int32> DimensionCounts;
	TMap<FName, TSet<FName>> DimensionVariants;
	TSet<FString> UniqueCaseKeys;
	TMap<FString, FGameXXKRouteBalanceCase> ControlBaselines;
	for (const FGameXXKRouteBalanceCase& Case : Cases)
	{
		++DimensionCounts.FindOrAdd(Case.DimensionId);
		DimensionVariants.FindOrAdd(Case.DimensionId).Add(Case.VariantId);
		const FString UniqueKey = MakeUniqueCaseKey(Case);
		if (UniqueCaseKeys.Contains(UniqueKey))
		{
			AddError(FString::Printf(TEXT("duplicate orthogonal case: %s"), *UniqueKey));
			return false;
		}
		UniqueCaseKeys.Add(UniqueKey);

		TestFalse(TEXT("orthogonal cases carry a dimension"), Case.DimensionId.IsNone());
		TestFalse(TEXT("orthogonal cases carry a variant"), Case.VariantId.IsNone());
		TestFalse(TEXT("orthogonal cases request a concrete companion template"), Case.CompanionTemplateId.IsNone());
		TestTrue(TEXT("orthogonal cases request a positive companion card seed"), Case.CompanionCardSeed > 0);
		TestTrue(TEXT("orthogonal cases request a concrete starting terrain"), Case.Terrain != EGameXXKCardTerrain::Invalid);

		const FString ControlKey = MakeControlGroupKey(Case);
		if (const FGameXXKRouteBalanceCase* Baseline = ControlBaselines.Find(ControlKey))
		{
			FString Mismatch;
			if (!HasMatchingControls(*Baseline, Case, Mismatch))
			{
				AddError(FString::Printf(
					TEXT("orthogonal group %s changes non-target field %s"),
					*ControlKey,
					*Mismatch));
				return false;
			}
		}
		else
		{
			ControlBaselines.Add(ControlKey, Case);
		}
	}

	TestEqual(TEXT("profession dimension cases"), DimensionCounts.FindRef(TEXT("Profession")), 540);
	TestEqual(TEXT("equipment-set dimension cases"), DimensionCounts.FindRef(TEXT("EquipmentSet")), 630);
	TestEqual(TEXT("quest-NPC dimension cases"), DimensionCounts.FindRef(TEXT("QuestNpc")), 540);
	TestEqual(TEXT("terrain dimension cases"), DimensionCounts.FindRef(TEXT("Terrain")), 540);
	TestEqual(TEXT("progression dimension cases"), DimensionCounts.FindRef(TEXT("Progression")), 270);
	TestEqual(TEXT("profession variants"), DimensionVariants.FindRef(TEXT("Profession")).Num(), 6);
	TestEqual(TEXT("equipment-set variants"), DimensionVariants.FindRef(TEXT("EquipmentSet")).Num(), 7);
	TestTrue(TEXT("equipment-set diagnostics include a six-piece mixed control"),
		DimensionVariants.FindRef(TEXT("EquipmentSet")).Contains(TEXT("MixedNoBonus")));
	TestFalse(TEXT("equipment-set diagnostics no longer compare full sets against a naked NoSet control"),
		DimensionVariants.FindRef(TEXT("EquipmentSet")).Contains(TEXT("NoSet")));
	TestEqual(TEXT("quest-NPC variants"), DimensionVariants.FindRef(TEXT("QuestNpc")).Num(), 6);
	TestEqual(TEXT("terrain variants"), DimensionVariants.FindRef(TEXT("Terrain")).Num(), 6);
	TestEqual(TEXT("progression variants"), DimensionVariants.FindRef(TEXT("Progression")).Num(), 3);

	FString Csv = BuildCaseCsvHeader(true);
	int32 VictoryCount = 0;
	for (int32 Index = 0; Index < Cases.Num(); ++Index)
	{
		const FGameXXKRouteBalanceCase& Case = Cases[Index];
		FGameXXKRouteBalanceCaseResult Result;
		Error.Reset();
		if (!FGameXXKRouteBalanceRules::RunCase(Case, Result, &Error))
		{
			AddError(FString::Printf(
				TEXT("orthogonal case failed dimension=%s variant=%s node=%d seed=%d: %s"),
				*Case.DimensionId.ToString(),
				*Case.VariantId.ToString(),
				static_cast<int32>(Case.NodeKind),
				Case.Seed,
				*Error));
			return false;
		}
		VictoryCount += Result.Metrics.bVictory ? 1 : 0;
		if (Result.Metrics.CompanionTemplateId != Case.CompanionTemplateId
			|| Result.Metrics.Terrain != Case.Terrain
			|| Result.Metrics.CompanionBirthCardIds.Num() != 6
			|| Result.Metrics.CompanionSelectedCardIds.Num() != 5
			|| Result.Metrics.StrandedTargetFailures != 0)
		{
			AddError(FString::Printf(
				TEXT("orthogonal identity/runtime metrics mismatch dimension=%s variant=%s seed=%d"),
				*Case.DimensionId.ToString(),
				*Case.VariantId.ToString(),
				Case.Seed));
			return false;
		}
		if (Case.DimensionId == TEXT("EquipmentSet"))
		{
			TestEqual(TEXT("every equipment-set diagnostic uses the same six-piece item budget"),
				Result.InitialHeroEquippedPieceCount,
				6);
			const int32 ExpectedHighestSetPieceCount = Case.VariantId == TEXT("MixedNoBonus") ? 1 : 6;
			TestEqual(TEXT("mixed control suppresses set thresholds while full-set variants keep six pieces"),
				Result.InitialHeroHighestSetPieceCount,
				ExpectedHighestSetPieceCount);
		}
		Csv += BuildCaseCsvRow(
			Case,
			Result.Metrics,
			Result.Metrics.bVictory ? TEXT("Victory") : TEXT("Defeat"),
			true);

		if (Index % 503 == 0)
		{
			FGameXXKRouteBalanceCaseResult Replay;
			Error.Reset();
			if (!FGameXXKRouteBalanceRules::RunCase(Case, Replay, &Error)
				|| !FGameXXKSimulationMetrics::StaticStruct()->CompareScriptStruct(
					&Result.Metrics,
					&Replay.Metrics,
					PPF_None))
			{
				AddError(FString::Printf(
					TEXT("orthogonal replay is not deterministic dimension=%s variant=%s seed=%d: %s"),
					*Case.DimensionId.ToString(),
					*Case.VariantId.ToString(),
					Case.Seed,
					*Error));
				return false;
			}
		}
	}

	const FString OutputDirectory = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("BalanceObservation"),
		RunId);
	if (!IFileManager::Get().MakeDirectory(*OutputDirectory, true))
	{
		AddError(FString::Printf(TEXT("failed to create orthogonal observation directory: %s"), *OutputDirectory));
		return false;
	}
	const FString OutputPath = FPaths::Combine(OutputDirectory, TEXT("cases.csv"));
	if (!FFileHelper::SaveStringToFile(
		Csv,
		*OutputPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		AddError(FString::Printf(TEXT("failed to save orthogonal observation CSV: %s"), *OutputPath));
		return false;
	}

	AddInfo(FString::Printf(
		TEXT("[OrthogonalBalanceObservation] run=%s cases=%d victories=%d defeats=%d output=%s"),
		*RunId,
		Cases.Num(),
		VictoryCount,
		Cases.Num() - VictoryCount,
		*OutputPath));
	return true;
}

#endif
