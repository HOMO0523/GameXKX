#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.generated.h"

/** Card-data enum values are serialized contract values. Append new values; never renumber existing values. */

/** How a card obtains the unit(s) that its effects address. Invalid is intentionally the default. */
UENUM(BlueprintType)
enum class EGameXXKCardTargetMode : uint8
{
	Invalid = 0 UMETA(Hidden),
	None = 1,
	Self = 2,
	SingleEnemy = 3,
	SingleAlly = 4,
	OtherAlly = 5,
	AllEnemies = 6,
	AllAllies = 7,
	AllOtherAllies = 8,
	RandomEnemy = 9,
	LowestHealthAlly = 10,
	LowestHealthOtherAlly = 11,
	AnyLivingUnit = 12
};

/** Presentation intent for future battle UI; it does not perform targeting. */
UENUM(BlueprintType)
enum class EGameXXKCardTargetPresentation : uint8
{
	Invalid = 0 UMETA(Hidden),
	NoSelection = 1,
	Self = 2,
	PlayerSelectsUnit = 3,
	AutomaticUnit = 4,
	Group = 5
};

UENUM(BlueprintType)
enum class EGameXXKCardOwner : uint8
{
	Invalid = 0 UMETA(Hidden),
	Hero = 1,
	Profession = 2,
	QuestNpc = 3,
	Route = 4
};

UENUM(BlueprintType)
enum class EGameXXKCardRarity : uint8
{
	Invalid = 0 UMETA(Hidden),
	Permanent = 1,
	Common = 2,
	Rare = 3,
	Boss = 4
};

UENUM(BlueprintType)
enum class EGameXXKCharacterRole : uint8
{
	Invalid = 0 UMETA(Hidden),
	Hero = 1,
	Blade = 2,
	Guard = 3,
	Healer = 4,
	Hunter = 5,
	Sorcerer = 6,
	FormationMaster = 7,
	QuestNpc = 8,
	Route = 9
};

/** Lifecycle state reserved for deck, hand, discard, and battle integrations. */
UENUM(BlueprintType)
enum class EGameXXKCardState : uint8
{
	Invalid = 0 UMETA(Hidden),
	InDeck = 1,
	InHand = 2,
	Played = 3,
	Discarded = 4,
	Exhausted = 5
};

UENUM(BlueprintType)
enum class EGameXXKCardUnitState : uint8
{
	Invalid = 0 UMETA(Hidden),
	Living = 1,
	Defeated = 2,
	Any = 3
};

/** Battle statuses used by card effects and data-only conditions. */
UENUM(BlueprintType)
enum class EGameXXKCardStatus : uint8
{
	Invalid = 0 UMETA(Hidden),
	None = 1,
	Momentum = 2,
	Agility = 3,
	Vulnerability = 4,
	Bleed = 5,
	Poison = 6,
	Burn = 7,
	Mark = 8,
	Guard = 9,
	DamageOverTime = 10,
	CannotReceiveVulnerability = 11,
	NextAttackBonus = 12,
	NextAttackAppliesVulnerability = 13,
	NextHealingBonus = 14,
	TerrainBonusDouble = 15,
	NextTerrainCardFree = 16,
	NextTerrainCardEnergyReduction = 17,
	RedirectSingleTargetEnemyAttack = 18
};

UENUM(BlueprintType)
enum class EGameXXKCardTerrain : uint8
{
	Invalid = 0 UMETA(Hidden),
	Plain = 1,
	Cliff = 2,
	Forest = 3,
	WaterShore = 4,
	Ferry = 5,
	Village = 6,
	Cave = 7
};

/** The entity addressed by an individual effect once target selection has resolved. */
UENUM(BlueprintType)
enum class EGameXXKCardEffectTarget : uint8
{
	Invalid = 0 UMETA(Hidden),
	CardOwner = 1,
	SelectedTarget = 2,
	AllEnemies = 3,
	AllAllies = 4,
	AllOtherAllies = 5,
	EachLivingAlly = 6,
	LowestHealthAlly = 7,
	LowestHealthOtherAlly = 8,
	Attacker = 9,
	PlayedCard = 10
};

/** Declarative effect operations. No operation is selected by CardId at runtime. */
UENUM(BlueprintType)
enum class EGameXXKCardEffectType : uint8
{
	Invalid = 0 UMETA(Hidden),
	DamagePercentAttack = 1,
	DamageFlat = 2,
	LoseHealth = 3,
	Heal = 4,
	AddArmor = 5,
	GainMana = 6,
	GainEnergy = 7,
	GainManaPerConsumedStatus = 8,
	DrawCards = 9,
	ApplyStatus = 10,
	RemoveStatus = 11,
	RemoveAnyDamageOverTime = 12,
	Insight = 13,
	DiscoverCards = 14,
	ReorderCards = 15,
	DiscardCards = 16,
	IgnoreDefense = 17,
	BonusDamagePercent = 18,
	BonusDamagePercentPerConsumedStatus = 19,
	BonusDamagePercentPerConsumedArmor = 20,
	EachLivingAllyAttackSelectedTarget = 21,
	ApplyGuardLink = 22,
	ApplyBattleModifier = 23,
	ModifyHealingPercent = 24,
	ModifyEnergyCost = 25,
	RevealEnemyIntent = 26,
	DoubleTerrainBonus = 27,
	RedirectSingleTargetEnemyAttacks = 28
};

/** Optional, soft gate for an effect. It may also describe status consumption. */
UENUM(BlueprintType)
enum class EGameXXKCardEffectConditionType : uint8
{
	None = 0,
	TargetHasStatus = 1,
	TargetHasAnyDamageOverTime = 2,
	OwnerHasStatus = 3,
	OwnerArmorAtLeast = 4,
	OwnerHealthBelowPercent = 5,
	TargetHealthBelowPercent = 6,
	TerrainIsAny = 7,
	OwnerHasDamageOverTime = 8
};

UENUM(BlueprintType)
enum class EGameXXKCardBattleModifierTrigger : uint8
{
	Invalid = 0 UMETA(Hidden),
	FirstDirectDamageReceivedThisRound = 1,
	OnCardPlayed = 2,
	OnNextAttack = 3,
	OnNextHealing = 4,
	EndOfRound = 5,
	OnSingleTargetEnemyAttack = 6
};

/** How a guard link redirects a qualifying attack. */
UENUM(BlueprintType)
enum class EGameXXKCardGuardRedirectPolicy : uint8
{
	Invalid = 0 UMETA(Hidden),
	RedirectNextSingleTargetDirectAttackToGuardian = 1
};

/** Recipient scope for a delayed battle modifier. */
UENUM(BlueprintType)
enum class EGameXXKCardModifierRecipientScope : uint8
{
	Invalid = 0 UMETA(Hidden),
	CardOwner = 1,
	SelectedTarget = 2,
	AllAllies = 3,
	AllOtherAllies = 4,
	SharedDeck = 5
};

/** Explicit lifetime for a delayed battle modifier. */
UENUM(BlueprintType)
enum class EGameXXKCardModifierExpiry : uint8
{
	Invalid = 0 UMETA(Hidden),
	AfterTriggerCount = 1,
	EndOfCurrentRound = 2,
	EndOfCurrentRoundOrTriggerCount = 3
};

/** Which attack target a triggered attack modifier permits. */
UENUM(BlueprintType)
enum class EGameXXKCardTriggeredAttackTargetScope : uint8
{
	Invalid = 0 UMETA(Hidden),
	AnyTarget = 1,
	RecipientTarget = 2,
	OriginalSelectedTarget = 3
};

/** Condition that switches a card's targeting mode without a card-id branch. */
UENUM(BlueprintType)
enum class EGameXXKCardTargetModeOverrideConditionType : uint8
{
	Invalid = 0 UMETA(Hidden),
	TerrainIsAny = 1,
	OwnerHasStatus = 2,
	TargetHasStatus = 3
};

/** Declarative target-mode switch used by terrain- and status-sensitive cards. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardTargetModeOverride
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardTargetModeOverrideConditionType ConditionType = EGameXXKCardTargetModeOverrideConditionType::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardStatus Status = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MinimumStatusStacks = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardTerrain Terrain = EGameXXKCardTerrain::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardTerrain AlternateTerrain = EGameXXKCardTerrain::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardTargetMode Mode = EGameXXKCardTargetMode::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardTargetPresentation Presentation = EGameXXKCardTargetPresentation::Invalid;
};

/** Hard target filters. A default-constructed target specification is deliberately unusable. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardTargetSpec
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardTargetMode Mode = EGameXXKCardTargetMode::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardTargetPresentation Presentation = EGameXXKCardTargetPresentation::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardUnitState RequiredUnitState = EGameXXKCardUnitState::Living;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardStatus RequiredStatus = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 RequiredStatusMinimumStacks = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardStatus ForbiddenStatus = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float MinimumHealthPercent = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float MaximumHealthPercent = 100.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardTerrain RequiredTerrain = EGameXXKCardTerrain::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardTerrain AlternateRequiredTerrain = EGameXXKCardTerrain::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bRequireDifferentFromOwner = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FGameXXKCardTargetModeOverride> ModeOverrides;
};

/** Soft effect condition; consumption flags let a resolver execute stack-based cards without CardId checks. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardEffectCondition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardEffectConditionType Type = EGameXXKCardEffectConditionType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardStatus Status = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MinimumStatusStacks = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MinimumArmor = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float HealthPercentThreshold = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardTerrain Terrain = EGameXXKCardTerrain::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardTerrain AlternateTerrain = EGameXXKCardTerrain::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bConsumeStatus = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MaxConsumedStatusStacks = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bScaleMagnitudeByConsumedStacks = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bConsumeOwnerArmor = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MaxConsumedArmor = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bNegate = false;
};

/** Persistent data used by a future battle resolver to register delayed or reactive effects. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardBattleModifier
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardBattleModifierTrigger Trigger = EGameXXKCardBattleModifierTrigger::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardEffectType EffectType = EGameXXKCardEffectType::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardEffectTarget Target = EGameXXKCardEffectTarget::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardModifierRecipientScope RecipientScope = EGameXXKCardModifierRecipientScope::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardEffectTarget RecipientTarget = EGameXXKCardEffectTarget::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCharacterRole RequiredTriggeredRole = EGameXXKCharacterRole::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName RequiredTriggeredOwnerId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardModifierExpiry Expiry = EGameXXKCardModifierExpiry::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardTriggeredAttackTargetScope TriggeredAttackTargetScope = EGameXXKCardTriggeredAttackTargetScope::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardStatus Status = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Magnitude = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 RemainingTriggers = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MinimumResult = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bPersistent = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameXXKCardEffectCondition Condition;
};

/** One data-only redirect binding. The resolver applies its policy without CardId checks. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardGuardLink
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardEffectTarget Guardian = EGameXXKCardEffectTarget::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardEffectTarget ProtectedUnit = EGameXXKCardEffectTarget::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Stacks = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardGuardRedirectPolicy RedirectPolicy = EGameXXKCardGuardRedirectPolicy::Invalid;
};

/** One atomic card effect. Multi-hit cards use HitCount rather than a CardId-specific path. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardEffect
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardEffectType Type = EGameXXKCardEffectType::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardEffectTarget Target = EGameXXKCardEffectTarget::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Magnitude = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 SecondaryMagnitude = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 HitCount = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardStatus Status = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameXXKCardEffectCondition Condition;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameXXKCardBattleModifier Modifier;

	/** Names the stack-consumption result produced by this effect. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName ConsumptionGroupId = NAME_None;

	/** References a previous consumed-stack result within the same card resolution. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName ConsumedStackResultRef = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameXXKCardGuardLink GuardLink;
};

/** Immutable catalog entry copied to future battle/deck runtime state as needed. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName Id = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText DisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardOwner Owner = EGameXXKCardOwner::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardRarity Rarity = EGameXXKCardRarity::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCharacterRole Role = EGameXXKCharacterRole::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName OwnerId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName NpcId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 EnergyCost = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 ManaCost = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameXXKCardTargetSpec TargetSpec;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FGameXXKCardEffect> Effects;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName VisualArtKey = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName FrameKey = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName AcquisitionKey = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bCoreProfessionCard = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIdentityLocked = false;
};

/** Visual recipe only. Keys are stable semantic names, not asset paths or UI text. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardVisualDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName CardId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName ArtKey = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName FrameKey = NAME_None;

	/** Stable subject identity used to lock hero and NPC portrait sources. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName IdentitySubjectKey = NAME_None;

	/** Source portrait or illustration recipe; independent from per-card art treatment. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName SourceArtKey = NAME_None;

	/** Decorative frame/overlay recipe applied independently of the source image. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName OverlayKey = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIdentityLocked = false;
};
