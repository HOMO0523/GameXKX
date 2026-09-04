#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"
#include "GameXXKEnemyCatalog.generated.h"

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEnemyIntentEffectDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKEnemyIntentEffectType Type = EGameXXKEnemyIntentEffectType::DirectDamage;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKEnemyIntentTargetRule Target = EGameXXKEnemyIntentTargetRule::LowestHealthParty;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 FlatMagnitude = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 AttackPercent = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 HitCount = 1;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKCardStatus Status = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 StatusStacks = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FGameXXKEnemyDifficultyInt AttackPercentByDifficulty;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FGameXXKEnemyDifficultyInt StatusAmountByDifficulty;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FGameXXKEnemyDifficultyInt DefensePercentByDifficulty;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FGameXXKEnemyDifficultyInt ResourceAmountByDifficulty;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKCardStatus ConsumedStatus = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 MaxConsumedStacks = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 MagnitudePerConsumedStack = 0;

	/** When true, each consumed stack contributes this percentage of the resolved target's maximum health. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bMagnitudePerConsumedStackUsesTargetMaxHealthPercent = false;

	/** Optional source status read at forecast time without consuming it. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKCardStatus SourceStatusForFlatMagnitude = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 FlatMagnitudePerSourceStatusStack = 0;

	/** Stores this resolved status target on the source enemy instead of recomputing from live health later. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bAssignsPersistentTarget = false;

	/** If the stored target is defeated, a phase-two source may reacquire the lowest-health living party unit. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bPhaseTwoFallbackToLowestHealth = false;

	/** A non-phase-two source clears its stored target after this direct effect resolves. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bClearsPersistentTargetAfterResolve = false;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEnemyIntentDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName Id = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FGameXXKEnemyIntentEffectDefinition> Effects;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 ChargeRounds = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Weight = 1;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bPhaseTwoOnly = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bRequiresSourceBelowHalf = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKCardStatus RequiredTargetStatus = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 CooldownRounds = 0;

	/** Multiplier applied only to this intent's direct-damage effects after its source enters phase two. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 PhaseTwoDirectDamagePercent = 100;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEnemyPhaseDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 PhaseNumber = 1;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FGameXXKEnemyIntentDefinition> Intents;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 ArmorRetentionPercent = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 FirstStatusGuardDefensePercent = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 HealMissingHealthPercentOnBleedingPrey = 0;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEnemyComputedStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 MaxHP = 1;

	UPROPERTY(BlueprintReadOnly)
	int32 Attack = 1;

	UPROPERTY(BlueprintReadOnly)
	int32 Defense = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 Speed = 1;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEnemyDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName Id = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Chapter = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKEnemyTier Tier = EGameXXKEnemyTier::Normal;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 BaseHP = 1;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float HPPerLevel = 0.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 BaseAttack = 1;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float AttackPerLevel = 0.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 BaseDefense = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float DefensePerLevel = 0.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 Speed = 1;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FGameXXKEnemyIntentDefinition> Intents;

	/** Authoritative phase decks. Converted definitions leave legacy Intents empty. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FGameXXKEnemyPhaseDefinition> Phases;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKEnemyPassiveId PassiveId = EGameXXKEnemyPassiveId::None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKEnemyPhaseId PhaseId = EGameXXKEnemyPhaseId::None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 PhaseThresholdPercent = 0;

	/** Optional self status granted once at the start of every player round before enemy intents are forecast. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKCardStatus RoundStartStatus = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 RoundStartStatusStacks = 0;

	/** Replaces RoundStartStatusStacks after this enemy has entered its saved second phase when positive. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 PhaseTwoRoundStartStatusStacks = 0;

	/** Multiplier applied to catalog direct-damage effect magnitudes while a saved second phase is active. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 PhaseTwoDirectDamagePercent = 100;

	/** Multipliers applied once to this enemy's baseline combat stats when it enters the saved second phase. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 PhaseTwoAttackPercent = 100;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 PhaseTwoDefensePercent = 100;

	/** Direct-damage intent ids that gain exactly one additional hit while this enemy is in phase two. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FName> PhaseTwoAdditionalHitIntentIds;

	/** When this enemy actually damages a target carrying this status, heal this percentage of its current missing HP. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKCardStatus HealOnDamagingTargetStatus = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 HealMissingHealthPercentOnDamagingTargetStatus = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName CodexId = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FSoftObjectPath PortraitSoftPath;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FSoftObjectPath BattleVisualSoftPath;
};

class GAMEXXK_API FGameXXKEnemyCatalog final
{
public:
	static const TArray<FGameXXKEnemyDefinition>& GetAllDefinitions();
	static const FGameXXKEnemyDefinition* Find(FName DefinitionId);
	static TArray<FName> GetPool(int32 Chapter, EGameXXKEnemyTier Tier);
	static int32 ResolveTotalPhases(EGameXXKEnemyTier Tier, EGameXXKEnemyDifficulty Difficulty);
	static const FGameXXKEnemyPhaseDefinition* GetPhaseDefinition(
		const FGameXXKEnemyDefinition& Definition,
		int32 PhaseNumber);
	static const TArray<FGameXXKEnemyIntentDefinition>* GetPhaseIntents(
		const FGameXXKEnemyDefinition& Definition,
		int32 PhaseNumber);
	static bool Validate(FString* OutError = nullptr);
	static FGameXXKEnemyComputedStats ComputeStats(FName DefinitionId, int32 CombatLevel);
};
