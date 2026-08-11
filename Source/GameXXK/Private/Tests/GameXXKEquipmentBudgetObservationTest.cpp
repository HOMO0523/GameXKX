#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "GameXXKBalanceObservationCsvTestSupport.h"
#include "GameXXKRouteBalanceRules.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentBudgetObservationTest,
	"GameXXK.Diagnostics.EquipmentBudgetObservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentBudgetObservationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBalanceObservationCsvTestSupport;
	FString RunId = TEXT("equipment_budget_manual");
	FParse::Value(FCommandLine::Get(), TEXT("GameXXKBalanceObservationId="), RunId);
	if (!IsSafeRunId(RunId))
	{
		AddError(TEXT("GameXXKBalanceObservationId must contain only letters, digits, dash, or underscore."));
		return false;
	}

	TArray<FGameXXKRouteBalanceCase> Cases;
	FString Error;
	if (!TestTrue(
		FString::Printf(TEXT("equipment-budget cases build: %s"), *Error),
		FGameXXKRouteBalanceRules::MakeEquipmentBudgetCases(Cases, &Error)))
	{
		return false;
	}
	TestEqual(TEXT("naked plus three qualities by three item levels produce 900 cases"), Cases.Num(), 900);
	if (Cases.Num() != 900)
	{
		return false;
	}

	TSet<FName> Variants;
	FString Csv = BuildCaseCsvHeader(true);
	int32 VictoryCount = 0;
	TMap<FName, FGameXXKRouteBalanceCaseResult> FirstResultByVariant;
	for (const FGameXXKRouteBalanceCase& Case : Cases)
	{
		Variants.Add(Case.VariantId);
		TestEqual(TEXT("equipment-budget cases stay in chapter two"), Case.Chapter, 2);
		TestEqual(TEXT("equipment-budget cases stay at route level ten"), Case.RouteLevel, 10);
		FGameXXKRouteBalanceCaseResult Result;
		Error.Reset();
		if (!FGameXXKRouteBalanceRules::RunCase(Case, Result, &Error))
		{
			AddError(FString::Printf(
				TEXT("equipment-budget case failed variant=%s node=%d seed=%d: %s"),
				*Case.VariantId.ToString(),
				static_cast<int32>(Case.NodeKind),
				Case.Seed,
				*Error));
			return false;
		}
		const bool bNaked = Case.VariantId == TEXT("Naked");
		TestEqual(TEXT("equipment-budget item count matches its declared budget"),
			Result.InitialHeroEquippedPieceCount,
			bNaked ? 0 : 6);
		TestEqual(TEXT("mixed equipment-budget fixtures never activate a two-piece threshold"),
			Result.InitialHeroHighestSetPieceCount,
			bNaked ? 0 : 1);
		if (!bNaked)
		{
			TestTrue(TEXT("geared fixtures declare an explicit item level"), Case.EquipmentItemLevel > 0);
		}
		TestTrue(TEXT("initial hero HP is recorded"), Result.InitialHeroMaxHP > 0);
		TestTrue(TEXT("initial hero Attack is recorded"), Result.InitialHeroAttack > 0);
		TestTrue(TEXT("initial hero Defense is recorded"), Result.InitialHeroDefense >= 0);
		TestTrue(TEXT("initial hero MaxMP is recorded"), Result.InitialHeroMaxMP > 0);
		FirstResultByVariant.FindOrAdd(Case.VariantId, Result);
		VictoryCount += Result.Metrics.bVictory ? 1 : 0;
		Csv += BuildCaseCsvRow(
			Case,
			Result.Metrics,
			Result.Metrics.bVictory ? TEXT("Victory") : TEXT("Defeat"),
			true);
	}
	TestEqual(TEXT("equipment-budget grid has exactly ten variants"), Variants.Num(), 10);

	const auto RequireStatGrowth = [this, &FirstResultByVariant](const FName LowerId, const FName HigherId)
	{
		const FGameXXKRouteBalanceCaseResult* Lower = FirstResultByVariant.Find(LowerId);
		const FGameXXKRouteBalanceCaseResult* Higher = FirstResultByVariant.Find(HigherId);
		if (!TestNotNull(FString::Printf(TEXT("lower budget exists: %s"), *LowerId.ToString()), Lower)
			|| !TestNotNull(FString::Printf(TEXT("higher budget exists: %s"), *HigherId.ToString()), Higher))
		{
			return;
		}
		TestTrue(FString::Printf(TEXT("%s to %s raises at least one combat stat"), *LowerId.ToString(), *HigherId.ToString()),
			Higher->InitialHeroMaxHP > Lower->InitialHeroMaxHP
			|| Higher->InitialHeroAttack > Lower->InitialHeroAttack
			|| Higher->InitialHeroDefense > Lower->InitialHeroDefense
			|| Higher->InitialHeroMaxMP > Lower->InitialHeroMaxMP);
	};
	for (const FString Quality : {FString(TEXT("Common")), FString(TEXT("Rare")), FString(TEXT("Epic"))})
	{
		RequireStatGrowth(FName(*FString::Printf(TEXT("Mixed%sL1"), *Quality)), FName(*FString::Printf(TEXT("Mixed%sL5"), *Quality)));
		RequireStatGrowth(FName(*FString::Printf(TEXT("Mixed%sL5"), *Quality)), FName(*FString::Printf(TEXT("Mixed%sL10"), *Quality)));
	}
	FString BudgetSummary = TEXT("variant,equipment_quality,item_level,hero_max_hp,hero_attack,hero_defense,hero_max_mp\n");
	TArray<FName> SortedVariants = Variants.Array();
	SortedVariants.Sort(FNameLexicalLess());
	for (const FName VariantId : SortedVariants)
	{
		const FGameXXKRouteBalanceCaseResult& Result = FirstResultByVariant.FindChecked(VariantId);
		BudgetSummary += FString::Printf(
			TEXT("%s,%s,%d,%d,%d,%d,%d\n"),
			*VariantId.ToString(),
			*Result.Case.EquipmentQualityId.ToString(),
			Result.Case.EquipmentItemLevel,
			Result.InitialHeroMaxHP,
			Result.InitialHeroAttack,
			Result.InitialHeroDefense,
			Result.InitialHeroMaxMP);
	}

	const FString OutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BalanceObservation"), RunId);
	if (!IFileManager::Get().MakeDirectory(*OutputDirectory, true)
		|| !FFileHelper::SaveStringToFile(
			Csv,
			*FPaths::Combine(OutputDirectory, TEXT("cases.csv")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
		|| !FFileHelper::SaveStringToFile(
			BudgetSummary,
			*FPaths::Combine(OutputDirectory, TEXT("equipment_budget.csv")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		AddError(TEXT("failed to save equipment-budget observation CSV"));
		return false;
	}
	AddInfo(FString::Printf(
		TEXT("[EquipmentBudgetObservation] run=%s cases=%d victories=%d defeats=%d"),
		*RunId,
		Cases.Num(),
		VictoryCount,
		Cases.Num() - VictoryCount));
	return true;
}

#endif
