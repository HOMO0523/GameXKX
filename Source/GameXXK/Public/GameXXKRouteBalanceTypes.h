#pragma once

#include "CoreMinimal.h"
#include "GameXXKCombatSimulationTypes.h"
#include "GameXXKMVPRules.h"

/** One immutable player/NPC/equipment fixture used by the route-balance matrix. */
struct GAMEXXK_API FGameXXKRouteBalanceCohort
{
	FName CohortId = NAME_None;
	FName QuestNpcId = NAME_None;
	FName EquipmentSetId = NAME_None;
	FName EquipmentQualityId = NAME_None;
	int32 EnhancementLevel = 0;
};

/** One fixed-seed battle request expanded from the locked full matrix. */
struct GAMEXXK_API FGameXXKRouteBalanceCase
{
	FName CohortId = NAME_None;
	FName QuestNpcId = NAME_None;
	FName EquipmentSetId = NAME_None;
	FName EquipmentQualityId = NAME_None;
	EGameXXKNodeKind NodeKind = EGameXXKNodeKind::Battle;
	int32 EnhancementLevel = 0;
	int32 Chapter = 0;
	int32 RouteLevel = 0;
	int32 SeedOrdinal = INDEX_NONE;
	int32 Seed = 0;
};

/** A temporary, simulation-only percentage scale applied to one encounter bucket. */
struct GAMEXXK_API FGameXXKRouteBalanceStatScale
{
	int32 MaxHPPercent = 100;
	int32 AttackPercent = 100;
	int32 DefensePercent = 100;
};

/**
 * A diagnostic projection profile. It is intentionally passed explicitly to the simulator and
 * never persists into the enemy catalog, route state, save data, or runtime encounter rules.
 */
struct GAMEXXK_API FGameXXKRouteBalanceCalibrationProfile
{
	FName ProfileId = NAME_None;
	TMap<int32, FGameXXKRouteBalanceStatScale> EncounterScales;

	static int32 MakeEncounterKey(const int32 Chapter, const EGameXXKNodeKind NodeKind)
	{
		return Chapter * 16 + static_cast<int32>(NodeKind);
	}
};

/** The exact enemy combat values projected before one balance fixture begins. */
struct GAMEXXK_API FGameXXKRouteBalanceInitialEnemy
{
	FName DefinitionId = NAME_None;
	int32 BattleSlotNumber = INDEX_NONE;
	int32 CombatLevel = 1;
	int32 MaxHP = 0;
	int32 Attack = 0;
	int32 Defense = 0;
};

/** The outcome of one fixed route-balance fixture resolved by the existing battle simulator. */
struct GAMEXXK_API FGameXXKRouteBalanceCaseResult
{
	FGameXXKRouteBalanceCase Case;
	FGameXXKSimulationMetrics Metrics;
	TArray<FGameXXKRouteBalanceInitialEnemy> InitialEnemies;
};

/** One chapter and node-kind win-rate bucket in a completed diagnostic report. */
struct GAMEXXK_API FGameXXKRouteBalanceAggregate
{
	int32 Chapter = 0;
	EGameXXKNodeKind NodeKind = EGameXXKNodeKind::Battle;
	int32 CaseCount = 0;
	int32 VictoryCount = 0;
	double VictoryRate = 0.0;
};

/** In-memory diagnostic output. It deliberately contains no tuning recommendation or write authority. */
struct GAMEXXK_API FGameXXKRouteBalanceReport
{
	TArray<FGameXXKRouteBalanceCaseResult> Results;
	TArray<FGameXXKRouteBalanceAggregate> Aggregates;
	double ElapsedSeconds = 0.0;
};

/** The source-independent in-memory form of route-balance-matrix-v1.json. */
struct GAMEXXK_API FGameXXKRouteBalanceMatrix
{
	int32 SchemaVersion = 0;
	int32 SeedCount = 0;
	TArray<EGameXXKNodeKind> NodeKinds;
	TArray<FGameXXKRouteBalanceCohort> Cohorts;
	TMap<int32, int32> RouteLevelsByChapter;
};
