#pragma once

#include "GameXXKRouteBalanceTypes.h"

namespace GameXXKBalanceObservationCsvTestSupport
{
	inline FString EscapeCsv(const FString& Value)
	{
		FString Escaped = Value;
		Escaped.ReplaceInline(TEXT("\""), TEXT("\"\""), ESearchCase::CaseSensitive);
		return FString::Printf(TEXT("\"%s\""), *Escaped);
	}

	inline FString SerializeMetricMap(const TMap<FName, int64>& Metrics)
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

	inline FString NodeKindLabel(const EGameXXKNodeKind NodeKind)
	{
		switch (NodeKind)
		{
		case EGameXXKNodeKind::Battle: return TEXT("Battle");
		case EGameXXKNodeKind::Elite: return TEXT("Elite");
		case EGameXXKNodeKind::Boss: return TEXT("Boss");
		default: return FString::Printf(TEXT("Node%d"), static_cast<int32>(NodeKind));
		}
	}

	inline FString CompanionRoleLabel(const EGameXXKCharacterRole Role)
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

	inline FString TerrainLabel(const EGameXXKCardTerrain Terrain)
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

	inline FString SerializeNameArray(const TArray<FName>& Values)
	{
		TArray<FString> Strings;
		Strings.Reserve(Values.Num());
		for (const FName Value : Values)
		{
			Strings.Add(Value.ToString());
		}
		return FString::Join(Strings, TEXT(";"));
	}

	inline bool IsSafeRunId(const FString& RunId)
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

	inline FString BuildCaseCsvHeader(const bool bIncludeOrthogonalIdentity)
	{
		const FString Identity = bIncludeOrthogonalIdentity ? TEXT("dimension,variant,") : TEXT("");
		return TEXT("schema_version,") + Identity
			+ TEXT("cohort,quest_npc,equipment_set,equipment_quality,enhancement,chapter,node,seed_ordinal,seed,outcome,rounds,remaining_party_health,first_round_deaths,companion_template,companion_role,companion_primary_archetype,companion_birth_cards,companion_selected_cards,terrain,active_cards,automatic_resolutions,energy_spent,energy_gained,mana_spent,mana_gained,energy_unspent_at_phase_end,mana_unspent_at_phase_end,healing_generated,armor_generated,overkill_damage,overhealing,stranded_target_failures,maximum_queue_depth,maximum_hand_size,damage_by_source,damage_by_origin,healing_by_source,armor_by_source,status_produced,status_consumed,cards_seen_by_id,cards_played_by_id,damage_by_card_id,healing_by_card_id,armor_by_card_id,error\n");
	}

	inline FString BuildCaseCsvRow(
		const FGameXXKRouteBalanceCase& Case,
		const FGameXXKSimulationMetrics& Metrics,
		const FString& Outcome,
		const bool bIncludeOrthogonalIdentity)
	{
		const auto Integer = [](const int64 Value)
		{
			return FString::Printf(TEXT("%lld"), Value);
		};
		TArray<FString> Columns = {TEXT("3")};
		if (bIncludeOrthogonalIdentity)
		{
			Columns.Add(EscapeCsv(Case.DimensionId.ToString()));
			Columns.Add(EscapeCsv(Case.VariantId.ToString()));
		}
		Columns.Append({
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
			Integer(Metrics.Rounds),
			Integer(Metrics.RemainingPartyHealth),
			Integer(Metrics.FirstRoundDeaths),
			EscapeCsv(Metrics.CompanionTemplateId.ToString()),
			EscapeCsv(CompanionRoleLabel(Metrics.CompanionRole)),
			EscapeCsv(Metrics.CompanionPrimaryArchetypeId.ToString()),
			EscapeCsv(SerializeNameArray(Metrics.CompanionBirthCardIds)),
			EscapeCsv(SerializeNameArray(Metrics.CompanionSelectedCardIds)),
			EscapeCsv(TerrainLabel(Metrics.Terrain)),
			Integer(Metrics.ActivelyPlayedCards),
			Integer(Metrics.AutomaticResolutionCount),
			Integer(Metrics.EnergySpent),
			Integer(Metrics.EnergyGained),
			Integer(Metrics.ManaSpent),
			Integer(Metrics.ManaGained),
			Integer(Metrics.EnergyUnspentAtPhaseEnd),
			Integer(Metrics.ManaUnspentAtPhaseEnd),
			Integer(Metrics.HealingGenerated),
			Integer(Metrics.ArmorGenerated),
			Integer(Metrics.OverkillDamage),
			Integer(Metrics.Overhealing),
			Integer(Metrics.StrandedTargetFailures),
			Integer(Metrics.MaximumAutomaticQueueDepth),
			Integer(Metrics.MaximumHandSize),
			EscapeCsv(SerializeMetricMap(Metrics.DamageBySource)),
			EscapeCsv(SerializeMetricMap(Metrics.DamageByOrigin)),
			EscapeCsv(SerializeMetricMap(Metrics.HealingBySource)),
			EscapeCsv(SerializeMetricMap(Metrics.ArmorBySource)),
			EscapeCsv(SerializeMetricMap(Metrics.StatusProduced)),
			EscapeCsv(SerializeMetricMap(Metrics.StatusConsumed)),
			EscapeCsv(SerializeMetricMap(Metrics.CardsSeenById)),
			EscapeCsv(SerializeMetricMap(Metrics.CardsPlayedById)),
			EscapeCsv(SerializeMetricMap(Metrics.DamageByCardId)),
			EscapeCsv(SerializeMetricMap(Metrics.HealingByCardId)),
			EscapeCsv(SerializeMetricMap(Metrics.ArmorByCardId)),
			EscapeCsv(Metrics.FailureReason.IsNone() ? TEXT("") : Metrics.FailureReason.ToString())});
		return FString::Join(Columns, TEXT(",")) + TEXT("\n");
	}
}
