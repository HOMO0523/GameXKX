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

	FString CompanionRoleLabel(const EGameXXKCharacterRole Role)
	{
		switch (Role)
		{
		case EGameXXKCharacterRole::Blade: return TEXT("Blade");
		case EGameXXKCharacterRole::Guard: return TEXT("Guard");
		case EGameXXKCharacterRole::Healer: return TEXT("Healer");
		case EGameXXKCharacterRole::Hunter: return TEXT("Hunter");
		case EGameXXKCharacterRole::Sorcerer: return TEXT("Sorcerer");
		case EGameXXKCharacterRole::FormationMaster: return TEXT("FormationMaster");
		default: return TEXT("");
		}
	}

	FString TerrainLabel(const EGameXXKCardTerrain Terrain)
	{
		switch (Terrain)
		{
		case EGameXXKCardTerrain::Plain: return TEXT("Plain");
		case EGameXXKCardTerrain::Cliff: return TEXT("Cliff");
		case EGameXXKCardTerrain::Forest: return TEXT("Forest");
		case EGameXXKCardTerrain::WaterShore: return TEXT("WaterShore");
		case EGameXXKCardTerrain::Ferry: return TEXT("Ferry");
		case EGameXXKCardTerrain::Village: return TEXT("Village");
		case EGameXXKCardTerrain::Cave: return TEXT("Cave");
		default: return TEXT("");
		}
	}

	FString SerializeNameArray(const TArray<FName>& Values)
	{
		TArray<FString> Strings;
		Strings.Reserve(Values.Num());
		for (const FName Value : Values)
		{
			Strings.Add(Value.ToString());
		}
		return FString::Join(Strings, TEXT(";"));
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
		const FString CompanionTemplate = Metrics ? Metrics->CompanionTemplateId.ToString() : TEXT("");
		const FString CompanionRole = Metrics ? CompanionRoleLabel(Metrics->CompanionRole) : TEXT("");
		const FString CompanionArchetype = Metrics ? Metrics->CompanionPrimaryArchetypeId.ToString() : TEXT("");
		const FString CompanionBirthCards = Metrics ? SerializeNameArray(Metrics->CompanionBirthCardIds) : TEXT("");
		const FString CompanionSelectedCards = Metrics ? SerializeNameArray(Metrics->CompanionSelectedCardIds) : TEXT("");
		const FString Terrain = Metrics ? TerrainLabel(Metrics->Terrain) : TEXT("");
		const int64 ActiveCards = Metrics ? Metrics->ActivelyPlayedCards : 0;
		const int64 AutomaticResolutions = Metrics ? Metrics->AutomaticResolutionCount : 0;
		const int64 EnergySpent = Metrics ? Metrics->EnergySpent : 0;
		const int64 EnergyGained = Metrics ? Metrics->EnergyGained : 0;
		const int64 ManaSpent = Metrics ? Metrics->ManaSpent : 0;
		const int64 ManaGained = Metrics ? Metrics->ManaGained : 0;
		const int64 EnergyUnspent = Metrics ? Metrics->EnergyUnspentAtPhaseEnd : 0;
		const int64 ManaUnspent = Metrics ? Metrics->ManaUnspentAtPhaseEnd : 0;
		const int64 HealingGenerated = Metrics ? Metrics->HealingGenerated : 0;
		const int64 ArmorGenerated = Metrics ? Metrics->ArmorGenerated : 0;
		const int64 OverkillDamage = Metrics ? Metrics->OverkillDamage : 0;
		const int64 Overhealing = Metrics ? Metrics->Overhealing : 0;
		const int64 StrandedTargetFailures = Metrics ? Metrics->StrandedTargetFailures : 0;
		const int32 MaximumQueueDepth = Metrics ? Metrics->MaximumAutomaticQueueDepth : 0;
		const int32 MaximumHandSize = Metrics ? Metrics->MaximumHandSize : 0;
		const TMap<FName, int64>& Damage = Metrics ? Metrics->DamageBySource : EmptyMetrics;
		const TMap<FName, int64>& DamageOrigins = Metrics ? Metrics->DamageByOrigin : EmptyMetrics;
		const TMap<FName, int64>& Healing = Metrics ? Metrics->HealingBySource : EmptyMetrics;
		const TMap<FName, int64>& Armor = Metrics ? Metrics->ArmorBySource : EmptyMetrics;
		const TMap<FName, int64>& Produced = Metrics ? Metrics->StatusProduced : EmptyMetrics;
		const TMap<FName, int64>& Consumed = Metrics ? Metrics->StatusConsumed : EmptyMetrics;
		const TMap<FName, int64>& CardsSeen = Metrics ? Metrics->CardsSeenById : EmptyMetrics;
		const TMap<FName, int64>& CardsPlayed = Metrics ? Metrics->CardsPlayedById : EmptyMetrics;
		const TMap<FName, int64>& DamageByCard = Metrics ? Metrics->DamageByCardId : EmptyMetrics;
		const TMap<FName, int64>& HealingByCard = Metrics ? Metrics->HealingByCardId : EmptyMetrics;
		const TMap<FName, int64>& ArmorByCard = Metrics ? Metrics->ArmorByCardId : EmptyMetrics;
		const FString Failure = Metrics && !Metrics->FailureReason.IsNone()
			? Metrics->FailureReason.ToString()
			: Error;

		const auto Integer = [](const int64 Value)
		{
			return FString::Printf(TEXT("%lld"), Value);
		};
		TArray<FString> Columns = {
			TEXT("3"),
			EscapeCsv(Case.CohortId.ToString()),
			EscapeCsv(Case.QuestNpcId.ToString()),
			EscapeCsv(Case.EquipmentSetId.ToString()),
			EscapeCsv(Case.EquipmentQualityId.ToString()),
			Integer(Case.EnhancementLevel),
			Integer(Case.Chapter),
			EscapeCsv(NodeKindLabel(Case.NodeKind)),
			Integer(Case.SeedOrdinal),
			Integer(Case.Seed),
			EscapeCsv(Outcome),
			Integer(Rounds),
			Integer(RemainingPartyHealth),
			Integer(FirstRoundDeaths),
			EscapeCsv(CompanionTemplate),
			EscapeCsv(CompanionRole),
			EscapeCsv(CompanionArchetype),
			EscapeCsv(CompanionBirthCards),
			EscapeCsv(CompanionSelectedCards),
			EscapeCsv(Terrain),
			Integer(ActiveCards),
			Integer(AutomaticResolutions),
			Integer(EnergySpent),
			Integer(EnergyGained),
			Integer(ManaSpent),
			Integer(ManaGained),
			Integer(EnergyUnspent),
			Integer(ManaUnspent),
			Integer(HealingGenerated),
			Integer(ArmorGenerated),
			Integer(OverkillDamage),
			Integer(Overhealing),
			Integer(StrandedTargetFailures),
			Integer(MaximumQueueDepth),
			Integer(MaximumHandSize),
			EscapeCsv(SerializeMetricMap(Damage)),
			EscapeCsv(SerializeMetricMap(DamageOrigins)),
			EscapeCsv(SerializeMetricMap(Healing)),
			EscapeCsv(SerializeMetricMap(Armor)),
			EscapeCsv(SerializeMetricMap(Produced)),
			EscapeCsv(SerializeMetricMap(Consumed)),
			EscapeCsv(SerializeMetricMap(CardsSeen)),
			EscapeCsv(SerializeMetricMap(CardsPlayed)),
			EscapeCsv(SerializeMetricMap(DamageByCard)),
			EscapeCsv(SerializeMetricMap(HealingByCard)),
			EscapeCsv(SerializeMetricMap(ArmorByCard)),
			EscapeCsv(Failure)};
		return FString::Join(Columns, TEXT(",")) + TEXT("\n");
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

	FString Csv = TEXT("schema_version,cohort,quest_npc,equipment_set,equipment_quality,enhancement,chapter,node,seed_ordinal,seed,outcome,rounds,remaining_party_health,first_round_deaths,companion_template,companion_role,companion_primary_archetype,companion_birth_cards,companion_selected_cards,terrain,active_cards,automatic_resolutions,energy_spent,energy_gained,mana_spent,mana_gained,energy_unspent_at_phase_end,mana_unspent_at_phase_end,healing_generated,armor_generated,overkill_damage,overhealing,stranded_target_failures,maximum_queue_depth,maximum_hand_size,damage_by_source,damage_by_origin,healing_by_source,armor_by_source,status_produced,status_consumed,cards_seen_by_id,cards_played_by_id,damage_by_card_id,healing_by_card_id,armor_by_card_id,error\n");
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
		if (bResolved
			&& (Result.Metrics.CompanionTemplateId.IsNone()
				|| Result.Metrics.CompanionRole == EGameXXKCharacterRole::Invalid
				|| Result.Metrics.CompanionPrimaryArchetypeId.IsNone()
				|| Result.Metrics.CompanionBirthCardIds.Num() != 6
				|| Result.Metrics.CompanionSelectedCardIds.Num() != 5
				|| Result.Metrics.Terrain == EGameXXKCardTerrain::Invalid))
		{
			AddError(FString::Printf(
				TEXT("schema v3 identity is incomplete for cohort=%s chapter=%d seed=%d"),
				*Case.CohortId.ToString(),
				Case.Chapter,
				Case.Seed));
			return false;
		}
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
	TestEqual(TEXT("schema v3 locked observation has no stalemates"), StalemateCount, 0);
	TestEqual(TEXT("schema v3 locked observation has no runner errors"), ErrorCount, 0);
	return StalemateCount == 0 && ErrorCount == 0;
}

#endif
