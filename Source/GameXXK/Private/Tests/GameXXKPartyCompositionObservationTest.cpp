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
	FGameXXKPartyCompositionObservationTest,
	"GameXXK.Diagnostics.PartyCompositionObservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartyCompositionObservationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKBalanceObservationCsvTestSupport;

	FString RunId = TEXT("party_composition_manual");
	FParse::Value(FCommandLine::Get(), TEXT("GameXXKPartyCompositionObservationId="), RunId);
	if (!IsSafeRunId(RunId))
	{
		AddError(TEXT("GameXXKPartyCompositionObservationId must contain only letters, digits, dash, or underscore."));
		return false;
	}

	TArray<FGameXXKRouteBalanceCase> OrthogonalCases;
	FString Error;
	if (!FGameXXKRouteBalanceRules::MakeOrthogonalCases(OrthogonalCases, &Error))
	{
		AddError(FString::Printf(TEXT("could not build profession controls: %s"), *Error));
		return false;
	}

	const FName QuestNpcIds[] = {
		TEXT("Npc.TusiChief"),
		TEXT("Npc.SongJinBao"),
		TEXT("Npc.YueBai"),
		TEXT("Npc.ZhouGuangZu"),
		TEXT("Npc.JinGui"),
		TEXT("Npc.QiongMeiEr")};
	TArray<FGameXXKRouteBalanceCase> Cases;
	Cases.Reserve(3240);
	for (const FGameXXKRouteBalanceCase& ProfessionCase : OrthogonalCases)
	{
		if (ProfessionCase.DimensionId != TEXT("Profession"))
		{
			continue;
		}
		for (const FName QuestNpcId : QuestNpcIds)
		{
			FGameXXKRouteBalanceCase& Case = Cases.Add_GetRef(ProfessionCase);
			Case.DimensionId = TEXT("PartyComposition");
			Case.VariantId = FName(*FString::Printf(
				TEXT("%s+%s"),
				*ProfessionCase.VariantId.ToString(),
				*QuestNpcId.ToString().RightChop(4)));
			Case.CohortId = TEXT("PartyComposition.StandardHero.L10.Plain.NoEquipment");
			Case.QuestNpcId = QuestNpcId;
		}
	}
	if (Cases.Num() != 3240)
	{
		AddError(FString::Printf(TEXT("expected 3,240 composition cases but built %d"), Cases.Num()));
		return false;
	}

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
				TEXT("composition case failed variant=%s node=%d seed=%d: %s"),
				*Case.VariantId.ToString(),
				static_cast<int32>(Case.NodeKind),
				Case.Seed,
				*Error));
			return false;
		}
		if (Result.Metrics.CompanionTemplateId != Case.CompanionTemplateId
			|| Result.Metrics.CompanionBirthCardIds.Num() != 6
			|| Result.Metrics.CompanionSelectedCardIds.Num() != 5
			|| Result.Metrics.StrandedTargetFailures != 0)
		{
			AddError(FString::Printf(
				TEXT("composition identity mismatch variant=%s seed=%d"),
				*Case.VariantId.ToString(),
				Case.Seed));
			return false;
		}
		VictoryCount += Result.Metrics.bVictory ? 1 : 0;
		Csv += BuildCaseCsvRow(
			Case,
			Result.Metrics,
			Result.Metrics.bVictory ? TEXT("Victory") : TEXT("Defeat"),
			true);

		if (Index % 647 == 0)
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
					TEXT("composition replay is not deterministic variant=%s seed=%d: %s"),
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
		AddError(FString::Printf(TEXT("failed to create composition output directory: %s"), *OutputDirectory));
		return false;
	}
	const FString OutputPath = FPaths::Combine(OutputDirectory, TEXT("cases.csv"));
	if (!FFileHelper::SaveStringToFile(
		Csv,
		*OutputPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		AddError(FString::Printf(TEXT("failed to save composition CSV: %s"), *OutputPath));
		return false;
	}

	AddInfo(FString::Printf(
		TEXT("[PartyCompositionObservation] run=%s cases=%d victories=%d defeats=%d output=%s"),
		*RunId,
		Cases.Num(),
		VictoryCount,
		Cases.Num() - VictoryCount,
		*OutputPath));
	return true;
}

#endif
