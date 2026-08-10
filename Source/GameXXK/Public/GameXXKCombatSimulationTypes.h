#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"
#include "GameXXKMVPRules.h"
#include "GameXXKCombatSimulationTypes.generated.h"

/** The simulation only exposes authored player policies; it never substitutes a second combat rule set. */
UENUM()
enum class EGameXXKSimulationPolicy : uint8
{
	Invalid = 0,
	Skilled = 1
};

/** One serializable real-rule battle request. InitialRuntimeState remains untouched by the runner. */
USTRUCT()
struct GAMEXXK_API FGameXXKSimulationScenario
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	int32 Seed = 0;

	UPROPERTY(SaveGame)
	FGameXXKRuntimeState InitialRuntimeState;

	UPROPERTY(SaveGame)
	EGameXXKNodeKind NodeKind = EGameXXKNodeKind::Battle;

	UPROPERTY(SaveGame)
	EGameXXKCardTerrain Terrain = EGameXXKCardTerrain::Plain;

	UPROPERTY(SaveGame)
	EGameXXKSimulationPolicy Policy = EGameXXKSimulationPolicy::Skilled;

	UPROPERTY(SaveGame)
	int32 MaxRounds = 100;

	UPROPERTY(SaveGame)
	int32 MaxDecisions = 2000;
};

/** A resolved policy choice using stable card and unit identifiers only. */
USTRUCT()
struct GAMEXXK_API FGameXXKSimulationDecision
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FName CardInstanceId = NAME_None;

	UPROPERTY(SaveGame)
	FName TargetUnitId = NAME_None;

	UPROPERTY(SaveGame)
	bool bEndPlayerPhase = false;
};

/** Compact source-of-truth audit for one successfully committed adapter action. */
USTRUCT()
struct GAMEXXK_API FGameXXKSimulationTraceEntry
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	int32 Round = 0;

	UPROPERTY(SaveGame)
	FName Action = NAME_None;

	UPROPERTY(SaveGame)
	FName SourceUnitId = NAME_None;

	UPROPERTY(SaveGame)
	FName CardOrIntentId = NAME_None;

	UPROPERTY(SaveGame)
	FName TargetUnitId = NAME_None;

	UPROPERTY(SaveGame)
	int32 HealthDelta = 0;

	UPROPERTY(SaveGame)
	int32 ManaDelta = 0;

	UPROPERTY(SaveGame)
	int32 ArmorDelta = 0;
};

/** Raw metrics intentionally avoid balancing judgments; later tuning owns aggregation and targets. */
USTRUCT()
struct GAMEXXK_API FGameXXKSimulationMetrics
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	bool bVictory = false;

	UPROPERTY(SaveGame)
	int32 Rounds = 0;

	UPROPERTY(SaveGame)
	int32 RemainingPartyHealth = 0;

	UPROPERTY(SaveGame)
	int32 FirstRoundDeaths = 0;

	UPROPERTY(SaveGame)
	int32 ActivelyPlayedCards = 0;

	/** Automatic card snapshots and task rewards, not individual damage packets. */
	UPROPERTY(SaveGame)
	int32 AutomaticResolutionCount = 0;

	UPROPERTY(SaveGame)
	int64 EnergySpent = 0;

	UPROPERTY(SaveGame)
	int64 EnergyGained = 0;

	UPROPERTY(SaveGame)
	int64 ManaSpent = 0;

	UPROPERTY(SaveGame)
	int64 ManaGained = 0;

	UPROPERTY(SaveGame)
	int64 HealingGenerated = 0;

	UPROPERTY(SaveGame)
	int64 ArmorGenerated = 0;

	/** Hand cards with affordable costs but no legal selectable target when the phase ends. */
	UPROPERTY(SaveGame)
	int32 StrandedTargetFailures = 0;

	UPROPERTY(SaveGame)
	int32 MaximumAutomaticQueueDepth = 0;

	UPROPERTY(SaveGame)
	int32 MaximumHandSize = 0;

	UPROPERTY(SaveGame)
	TMap<FName, int64> DamageBySource;

	UPROPERTY(SaveGame)
	TMap<FName, int64> DamageByOrigin;

	UPROPERTY(SaveGame)
	TMap<FName, int64> HealingBySource;

	UPROPERTY(SaveGame)
	TMap<FName, int64> ArmorBySource;

	UPROPERTY(SaveGame)
	TMap<FName, int64> StatusProduced;

	UPROPERTY(SaveGame)
	TMap<FName, int64> StatusConsumed;

	UPROPERTY(SaveGame)
	FName FailureReason = NAME_None;
};
