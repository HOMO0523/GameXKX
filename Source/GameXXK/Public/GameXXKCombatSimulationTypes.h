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

	/** Continue an authored opening/current battle without rebuilding its hand or difficulty. */
	UPROPERTY(SaveGame)
	bool bResumeActiveBattle = false;
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

USTRUCT()
struct GAMEXXK_API FGameXXKSimulationUnitSnapshot
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FName UnitId = NAME_None;
	UPROPERTY(SaveGame) EGameXXKCardTargetSide Side = EGameXXKCardTargetSide::Invalid;
	UPROPERTY(SaveGame) int32 HP = 0;
	UPROPERTY(SaveGame) int32 Armor = 0;
	UPROPERTY(SaveGame) int32 Mana = 0;
	UPROPERTY(SaveGame) TArray<FGameXXKCardStatusStack> Statuses;
};

/** Source-of-truth audit for one successfully committed adapter action. */
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

	UPROPERTY(SaveGame) int32 EnergyBefore = 0;
	UPROPERTY(SaveGame) int32 EnergyAfter = 0;
	UPROPERTY(SaveGame) int32 EnergyPaid = 0;
	UPROPERTY(SaveGame) int32 ManaPaid = 0;
	UPROPERTY(SaveGame) int64 EffectiveDamage = 0;
	UPROPERTY(SaveGame) int64 DamageTaken = 0;
	UPROPERTY(SaveGame) int64 EffectiveHealing = 0;
	UPROPERTY(SaveGame) int64 GeneratedArmor = 0;
	UPROPERTY(SaveGame) TArray<FGameXXKSimulationUnitSnapshot> UnitsBefore;
	UPROPERTY(SaveGame) TArray<FGameXXKSimulationUnitSnapshot> UnitsAfter;
	UPROPERTY(SaveGame) TArray<FGameXXKCardDamageResult> DamagePackets;
	UPROPERTY(SaveGame) TArray<FGameXXKCardHealingResult> HealingPackets;
	UPROPERTY(SaveGame) TArray<FGameXXKCardArmorResult> ArmorPackets;
	UPROPERTY(SaveGame) TArray<FGameXXKCardStatusChangeResult> StatusChanges;
};

/** Raw metrics intentionally avoid balancing judgments; later tuning owns aggregation and targets. */
USTRUCT()
struct GAMEXXK_API FGameXXKSimulationMetrics
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	bool bVictory = false;

	/** The battle resolved as a no-progress stalemate defeat instead of a combat outcome. */
	UPROPERTY(SaveGame)
	bool bStalemateResolved = false;

	/** Deterministic permanent-partner identity captured before the battle starts. */
	UPROPERTY(SaveGame)
	FName CompanionTemplateId = NAME_None;

	UPROPERTY(SaveGame)
	EGameXXKCharacterRole CompanionRole = EGameXXKCharacterRole::Invalid;

	UPROPERTY(SaveGame)
	FName CompanionPrimaryArchetypeId = NAME_None;

	/** Six immutable birth cards reconstructed from Role + CardSeed. */
	UPROPERTY(SaveGame)
	TArray<FName> CompanionBirthCardIds;

	/** Five cards actually selected for this route. */
	UPROPERTY(SaveGame)
	TArray<FName> CompanionSelectedCardIds;

	UPROPERTY(SaveGame)
	EGameXXKCardTerrain Terrain = EGameXXKCardTerrain::Invalid;

	UPROPERTY(SaveGame)
	int32 Rounds = 0;

	/** Effective health loss from the same per-unit ledger used by Boss settlement. */
	UPROPERTY(SaveGame) int64 DamageDealt = 0;
	UPROPERTY(SaveGame) int64 DamageTaken = 0;
	/** Explicit audit: ledger outgoing damage minus all exported outgoing packets. */
	UPROPERTY(SaveGame) int64 DamageLedgerDifference = 0;

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

	/** Resources left when the policy explicitly ends a player phase, accumulated across rounds. */
	UPROPERTY(SaveGame)
	int64 EnergyUnspentAtPhaseEnd = 0;

	UPROPERTY(SaveGame)
	int64 ManaUnspentAtPhaseEnd = 0;

	UPROPERTY(SaveGame)
	int64 HealingGenerated = 0;

	UPROPERTY(SaveGame)
	int64 ArmorGenerated = 0;

	/** Player-card damage remaining after armor and target health were exhausted. */
	UPROPERTY(SaveGame)
	int64 OverkillDamage = 0;

	/** Requested player-card healing that could not become effective healing. */
	UPROPERTY(SaveGame)
	int64 Overhealing = 0;

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

	/** A card is seen once when its stable instance newly enters the hand. */
	UPROPERTY(SaveGame)
	TMap<FName, int64> CardsSeenById;

	/** Active player commits only; automatic replays never increment this map. */
	UPROPERTY(SaveGame)
	TMap<FName, int64> CardsPlayedById;

	/** Effective contribution of the active card transaction, keyed by its stable CardId. */
	UPROPERTY(SaveGame)
	TMap<FName, int64> DamageByCardId;

	UPROPERTY(SaveGame)
	TMap<FName, int64> HealingByCardId;

	UPROPERTY(SaveGame)
	TMap<FName, int64> ArmorByCardId;

	UPROPERTY(SaveGame)
	FName FailureReason = NAME_None;
};
