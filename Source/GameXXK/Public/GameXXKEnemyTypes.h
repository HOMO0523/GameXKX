#pragma once

#include "CoreMinimal.h"

#include "GameXXKEnemyTypes.generated.h"

UENUM(BlueprintType)
enum class EGameXXKEnemyTier : uint8
{
	Normal = 0,
	Elite = 1,
	Boss = 2
};

UENUM(BlueprintType)
enum class EGameXXKEnemyIntentTargetRule : uint8
{
	None = 0,
	Self = 1,
	LowestHealthParty = 2,
	RandomLivingParty = 3,
	AllLivingParty = 4,
	AllEnemyAllies = 5,
	LowestHealthEnemyAlly = 6,
	MarkedParty = 7,
	PreyTarget = 8
};

UENUM(BlueprintType)
enum class EGameXXKEnemyIntentEffectType : uint8
{
	DirectDamage = 0,
	AddArmor = 1,
	Heal = 2,
	ApplyStatus = 3,
	ConsumeSharedQi = 4,
	ModifyAttack = 5,
	ModifyDefense = 6,
	ModifySpeed = 7,
	RemovePositiveStatus = 8,
	IncreaseNextCardEnergy = 9,
	SetCounter = 10,
	SetCharge = 11
};

UENUM(BlueprintType)
enum class EGameXXKEnemyPassiveTrigger : uint8
{
	BattleStart = 0,
	RoundStart = 1,
	FirstDirectDamageReceived = 2,
	DirectDamageReceived = 3,
	FirstStatusReceivedThisRound = 4,
	DirectAttackReceived = 5,
	TargetDefeated = 6
};

UENUM(BlueprintType)
enum class EGameXXKEnemyPassiveId : uint8
{
	None = 0,
	IronfeatherFirstHit = 1,
	BluehornArmorRetention = 2,
	MoneyRatWealth = 3,
	PorcupineCounter = 4,
	GraymaneMarkedHunt = 5,
	RedtuskRage = 6,
	BlackBearThickHide = 7,
	WhiteApeStatusGuard = 8,
	DeerHealCooldown = 9,
	TigerPredator = 10
};

UENUM(BlueprintType)
enum class EGameXXKEnemyPhaseId : uint8
{
	None = 0,
	MoneyRatMadHoard = 1,
	BlackBearEnraged = 2,
	TigerDread = 3
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEnemyBattleState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName DefinitionId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 IntentCursor = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bPhaseTwo = false;

	/** One-way baseline stat adjustment committed when a catalog boss first enters phase two. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bPhaseStatModifiersApplied = false;

	/** Saved portion of the phase-two attack adjustment, distinct from temporary intent buffs. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PhaseAttackModifier = 0;

	/** Saved portion of the phase-two defense adjustment. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PhaseDefenseModifier = 0;

	/** Stable target selected by a catalog persistent-target effect, never a transient widget index. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName PersistentTargetUnitId = NAME_None;

	/** Serialized EGameXXKCardStatus value projected onto PersistentTargetUnitId while it remains valid.
	 *  Stored as a byte here to keep the foundational enemy header independent from GameXXKCardTypes.
	 *  Value 1 is the stable serialized value of EGameXXKCardStatus::None. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	uint8 PersistentTargetStatus = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName PendingChargedIntentId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ChargeRoundsRemaining = 0;

	/** Direct-damage targets locked when a catalog charge action starts. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> PendingChargeTargetUnitIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 HealingCooldownRounds = 0;

	/** Prevents a newly started healing cooldown from decrementing at its originating enemy-phase boundary. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bHealingCooldownStartedThisEnemyPhase = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 InitiativeBonus = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bFirstHitPassiveAvailable = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bFirstStatusPassiveAvailable = true;

	/** Attack temporarily added by catalog intent effects during the current enemy phase. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 TemporaryAttackModifier = 0;

	/** Total temporary speed currently applied by catalog intent effects. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 TemporarySpeedModifier = 0;

	/** Portion of TemporarySpeedModifier that expires when the current enemy phase completes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SpeedModifierExpiringAfterCurrentEnemyPhase = 0;

	/** Speed granted during the current enemy phase; it is promoted for the following enemy phase. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PendingNextEnemyPhaseSpeedModifier = 0;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKRouteProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SchemaVersion = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RootSeed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<int32> ChapterSeeds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 CurrentChapter = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RouteCombatLevel = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ActualRouteCardAcquisitionCount = 0;
};
