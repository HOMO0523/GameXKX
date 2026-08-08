#include "Misc/AutomationTest.h"

#include "GameXXKRouteBalanceRules.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString EscapeCsv(const FString& Value)
	{
		FString Escaped = Value;
		Escaped.ReplaceInline(TEXT("\""), TEXT("\"\""), ESearchCase::CaseSensitive);
		return FString::Printf(TEXT("\"%s\""), *Escaped);
	}

	FString SerializeMetricMap(const TMap<FName, int64>& Metrics)
	{
		TArray<FName> Keys;
		Metrics.GenerateKeyArray(Keys);
		Keys.Sort([](const FName& Left, const FName& Right)
		{
			return Left.LexicalLess(Right);
		});

		FString Serialized;
		for (const FName& Key : Keys)
		{
			if (!Serialized.IsEmpty())
			{
				Serialized += TEXT(";");
			}
			Serialized += FString::Printf(TEXT("%s=%lld"), *Key.ToString(), Metrics.FindChecked(Key));
		}
		return Serialized;
	}

	FString NodeKindLabel(const EGameXXKNodeKind NodeKind)
	{
		switch (NodeKind)
		{
		case EGameXXKNodeKind::Battle:
			return TEXT("Battle");
		case EGameXXKNodeKind::Elite:
			return TEXT("Elite");
		case EGameXXKNodeKind::Boss:
			return TEXT("Boss");
		default:
			return FString::Printf(TEXT("Node%d"), static_cast<int32>(NodeKind));
		}
	}

	bool IsSafeRunId(const FString& RunId)
	{
		if (RunId.IsEmpty() || RunId.Len() > 80)
		{
			return false;
		}
		for (const TCHAR Character : RunId)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-'))
			{
				return false;
			}
		}
		return true;
	}

	FString BuildCaseCsvRow(
		const FGameXXKRouteBalanceCase& Case,
		const FGameXXKSimulationMetrics* Metrics,
		const FString& Outcome,
		const FString& Error)
	{
		const TMap<FName, int64> EmptyMetrics;
		const int32 Rounds = Metrics ? Metrics->Rounds : (Outcome == TEXT("Stalemate") ? 100 : 0);
		const int32 RemainingPartyHealth = Metrics ? Metrics->RemainingPartyHealth : 0;
		const int32 FirstRoundDeaths = Metrics ? Metrics->FirstRoundDeaths : 0;
		const TMap<FName, int64>& Damage = Metrics ? Metrics->DamageBySource : EmptyMetrics;
		const TMap<FName, int64>& Healing = Metrics ? Metrics->HealingBySource : EmptyMetrics;
		const TMap<FName, int64>& Armor = Metrics ? Metrics->ArmorBySource : EmptyMetrics;
		const TMap<FName, int64>& Produced = Metrics ? Metrics->StatusProduced : EmptyMetrics;
		const TMap<FName, int64>& Consumed = Metrics ? Metrics->StatusConsumed : EmptyMetrics;
		const FString Failure = Metrics && !Metrics->FailureReason.IsNone()
			? Metrics->FailureReason.ToString()
			: Error;

		return FString::Printf(
			TEXT("%s,%s,%s,%s,%d,%d,%s,%d,%d,%s,%d,%d,%d,%s,%s,%s,%s,%s,%s\n"),
			*EscapeCsv(Case.CohortId.ToString()),
			*EscapeCsv(Case.QuestNpcId.ToString()),
			*EscapeCsv(Case.EquipmentSetId.ToString()),
			*EscapeCsv(Case.EquipmentQualityId.ToString()),
			Case.EnhancementLevel,
			Case.Chapter,
			*EscapeCsv(NodeKindLabel(Case.NodeKind)),
			Case.SeedOrdinal,
			Case.Seed,
			*EscapeCsv(Outcome),
			Rounds,
			RemainingPartyHealth,
			FirstRoundDeaths,
			*EscapeCsv(SerializeMetricMap(Damage)),
			*EscapeCsv(SerializeMetricMap(Healing)),
			*EscapeCsv(SerializeMetricMap(Armor)),
			*EscapeCsv(SerializeMetricMap(Produced)),
			*EscapeCsv(SerializeMetricMap(Consumed)),
			*EscapeCsv(Failure));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBalanceObservationTest,
	"GameXXK.Diagnostics.CardBalanceObservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBalanceObservationTest::RunTest(const FString& Parameters)
{
	FString RunId = TEXT("manual");
	FParse::Value(FCommandLine::Get(), TEXT("GameXXKBalanceObservationId="), RunId);
	if (!IsSafeRunId(RunId))
	{
		AddError(TEXT("GameXXKBalanceObservationId must contain only letters, digits, dash, or underscore."));
		return false;
	}

	const FGameXXKRouteBalanceMatrix Matrix = FGameXXKRouteBalanceRules::MakeLockedFullMatrix();
	TArray<FGameXXKRouteBalanceCase> Cases;
	FString ExpandError;
	if (!FGameXXKRouteBalanceRules::ExpandCases(Matrix, Cases, &ExpandError))
	{
		AddError(FString::Printf(TEXT("locked observation matrix did not expand: %s"), *ExpandError));
		return false;
	}
	TestEqual(TEXT("diagnostic attempts every locked route-balance case"), Cases.Num(), 2400);
	if (Cases.Num() != 2400)
	{
		return false;
	}

	FString Csv = TEXT("cohort,quest_npc,equipment_set,equipment_quality,enhancement,chapter,node,seed_ordinal,seed,outcome,rounds,remaining_party_health,first_round_deaths,damage_by_source,healing_by_source,armor_by_source,status_produced,status_consumed,error\n");
	int32 VictoryCount = 0;
	int32 DefeatCount = 0;
	int32 StalemateCount = 0;
	int32 ErrorCount = 0;
	const double StartSeconds = FPlatformTime::Seconds();

	for (const FGameXXKRouteBalanceCase& Case : Cases)
	{
		FGameXXKRouteBalanceCaseResult Result;
		FString CaseError;
		const bool bResolved = FGameXXKRouteBalanceRules::RunCase(Case, Result, &CaseError);
		FString Outcome;
		const FGameXXKSimulationMetrics* Metrics = nullptr;
		if (bResolved)
		{
			Metrics = &Result.Metrics;
			Outcome = Result.Metrics.bVictory ? TEXT("Victory") : TEXT("Defeat");
			Result.Metrics.bVictory ? ++VictoryCount : ++DefeatCount;
		}
		else if (CaseError.Contains(TEXT("MaxRounds"), ESearchCase::CaseSensitive))
		{
			Outcome = TEXT("Stalemate");
			++StalemateCount;
		}
		else
		{
			Outcome = TEXT("Error");
			++ErrorCount;
		}
		Csv += BuildCaseCsvRow(Case, Metrics, Outcome, CaseError);
	}

	const FString OutputDirectory = FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("BalanceObservation"),
		RunId);
	if (!IFileManager::Get().MakeDirectory(*OutputDirectory, true))
	{
		AddError(FString::Printf(TEXT("failed to create observation directory: %s"), *OutputDirectory));
		return false;
	}
	const FString OutputPath = FPaths::Combine(OutputDirectory, TEXT("cases.csv"));
	const bool bSaved = FFileHelper::SaveStringToFile(
		Csv,
		*OutputPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	TestTrue(TEXT("diagnostic case CSV saves"), bSaved);
	if (!bSaved)
	{
		return false;
	}

	AddInfo(FString::Printf(
		TEXT("[CardBalanceObservation] run=%s cases=%d victory=%d defeat=%d stalemate=%d error=%d elapsed=%.3f output=%s"),
		*RunId,
		Cases.Num(),
		VictoryCount,
		DefeatCount,
		StalemateCount,
		ErrorCount,
		FPlatformTime::Seconds() - StartSeconds,
		*OutputPath));
	return true;
}

#endif
