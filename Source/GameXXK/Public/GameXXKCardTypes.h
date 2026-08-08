#pragma once

#include "CoreMinimal.h"
#include "GameXXKEnemyTypes.h"
#include "GameXXKEquipmentSetCatalog.h"
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

/** Independent card/relic quality contract. Do not conflate this with acquisition-source rarity. */
UENUM(BlueprintType)
enum class EGameXXKCardQuality : uint8
{
	Invalid = 0 UMETA(Hidden),
	Common = 1 UMETA(DisplayName = "普通"),
	Rare = 2 UMETA(DisplayName = "稀有"),
	Epic = 3 UMETA(DisplayName = "珍稀")
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
	RedirectSingleTargetEnemyAttack = 18,
	/** A one-round terrain-bonus doubling window; expires before the next player phase. */
	TerrainBonusDoubleThisRound = 19,
	Medicine = 20,
	Weak = 21,
	Wealth = 22,
	Rage = 23,
	Prey = 24,
	Charge = 25,
	Counter = 26
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
	EGameXXKCardQuality BaseQuality = EGameXXKCardQuality::Common;

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

/** Serialized choice states for deck operations. Invalid is deliberately the default. */
UENUM(BlueprintType)
enum class EGameXXKCardPendingChoiceKind : uint8
{
	Invalid = 0 UMETA(Hidden),
	None = 1,
	ForcedDiscard = 2,
	InsightChooseToHand = 3
};

/** A logical card zone. Pending-choice candidates are references/views, not a second owning zone. */
UENUM(BlueprintType)
enum class EGameXXKCardZone : uint8
{
	Invalid = 0 UMETA(Hidden),
	DrawPile = 1,
	Hand = 2,
	DiscardPile = 3
};

/** One materialized battle card. InstanceId, rather than CardId, is the unique runtime identity. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName InstanceId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName CardId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardQuality CurrentQuality = EGameXXKCardQuality::Common;

	/** Existing battle-unit identity; rules never substitute a UI index for this value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName OwnerUnitId = NAME_None;

	/** Stable run-deck entry from which this battle instance was materialized. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceEntryId = NAME_None;

	/** Stable acquisition order within the source deck. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 AcquisitionOrdinal = INDEX_NONE;
};

/** Persisted data for a choice that blocks normal deck mutations until it is resolved or cancelled. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPendingCardChoice
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardPendingChoiceKind Kind = EGameXXKCardPendingChoiceKind::Invalid;

	/** Candidate copies are views; the owning instance remains in Hand or DrawPile until confirmation. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardInstance> Candidates;

	/** Generic number of selections required by the active choice. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RequiredCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RequiredDiscardCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RequiredHandPickCount = 0;

	/** Insight candidates in logical top-to-bottom order. DrawPile.Last() is the logical top. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> InsightTopOrder;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName InsightPickedInstanceId = NAME_None;

	/** Requested remaining insight order, also logical top-to-bottom. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> InsightReorderedInstanceIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bCanCancel = false;

	/** Insight cancellation only clears the choice; it never rewinds a card previously moved to discard. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bCancelPreservesDrawTop = true;
};

/**
 * Serializable deck runtime state. DrawPile.Last() is the logical top: draws Pop() from the end,
 * and insight order is always expressed from that top toward index zero.
 */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKBattleDeckState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 InitialRandomSeed = 0;

	/** Persisted state of the rules-local deterministic PRNG, not a UI or engine global random source. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 CurrentRandomState = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardInstance> DrawPile;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardInstance> Hand;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardInstance> DiscardPile;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKPendingCardChoice PendingChoice;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SharedEnergy = 3;

	/** Normal round-start refill target. Card effects may grow Hand to the rules-owned hard capacity. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 HandLimit = 5;

	/** Stable non-zone ledger used to prove that every initialized instance remains in exactly one zone. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> ActiveInstanceIds;
};

/** Logical battle side used by pure target selection. This is never a widget index. */
UENUM(BlueprintType)
enum class EGameXXKCardTargetSide : uint8
{
	Invalid = 0 UMETA(Hidden),
	Party = 1,
	Enemy = 2
};

/** Why a stable unit is visible but unavailable to a target-selection UI. */
UENUM(BlueprintType)
enum class EGameXXKCardTargetDisabledReason : uint8
{
	Invalid = 0 UMETA(Hidden),
	None = 1,
	WrongSide = 2,
	NotLiving = 3,
	OwnerExcluded = 4,
	NotSource = 5,
	RequiredStatusMissing = 6,
	ForbiddenStatusPresent = 7,
	HealthBelowMinimum = 8,
	HealthAboveMaximum = 9,
	TerrainMismatch = 10,
	InvalidHealth = 11
};

/** One status stack view supplied by the battle layer for targeting. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardStatusStack
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardStatus Status = EGameXXKCardStatus::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Stacks = 0;
};

/** Stable battle-unit facts needed to calculate card target candidates. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardTargetUnit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName UnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardTargetSide Side = EGameXXKCardTargetSide::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bLiving = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 HP = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 MaxHP = 0;

	/** Persistent battle slot/order, rather than a temporary UI list position. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 StableSortOrder = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardStatusStack> Statuses;
};

/** UI-safe target candidate view. Invalid candidates remain visible with an explicit reason. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardTargetCandidateView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName UnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardTargetSide Side = EGameXXKCardTargetSide::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bCanSelect = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardTargetDisabledReason DisabledReason = EGameXXKCardTargetDisabledReason::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bAutoLocked = false;
};

/** Pure target-selection result passed to a future battle UI or resolver. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardTargetRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName CardInstanceId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardTargetMode EffectiveMode = EGameXXKCardTargetMode::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardTargetPresentation Presentation = EGameXXKCardTargetPresentation::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bRequiresManualSelection = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bRequiresRandomResolution = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardTargetCandidateView> CandidateViews;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> AutomaticTargetUnitIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FString FailureReason;
};

/**
 * Pure combat-unit state used by card rules before the legacy MVP battle facade is connected.
 * UnitId and StableSortOrder are gameplay identities; neither may be derived from a widget index.
 */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardCombatUnit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName UnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardTargetSide Side = EGameXXKCardTargetSide::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCharacterRole Role = EGameXXKCharacterRole::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bLiving = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 HP = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 MaxHP = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Mana = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 MaxMana = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Attack = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Defense = 0;

	/** Persistent turn-order stat projected once from the permanent character loadout. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Speed = 1;

	/** Armor is authoritative for card resolution and capped at 99. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Armor = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 StableSortOrder = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName EnemyDefinitionId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 BattleSlotNumber = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 CombatLevel = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardStatusStack> Statuses;
};

/** Selects the only valid mitigation policy for one resolved damage packet. */
UENUM(BlueprintType)
enum class EGameXXKCardDamageKind : uint8
{
	Invalid = 0 UMETA(Hidden),
	/** One directed attack: guard, agility, defense, vulnerability, armor, then health. */
	SingleTargetAttack = 1,
	/** One attack packet against one member of a group: agility, defense, vulnerability, armor, then health; never guard. */
	GroupAttack = 2,
	/** Intentional self-health loss: health only, with no guard, agility, defense, or vulnerability. */
	SelfHealthLoss = 3,
	/** Global or map-source health loss: health only, with no guard, agility, defense, or vulnerability. */
	EnvironmentalHealthLoss = 4,
	/** End-phase status health loss. The dedicated DoT resolver owns this case. */
	DamageOverTime = 5
};

/** Source and policy metadata for one atomic damage packet. It is never inferred from a UI widget. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardDamageContext
{
	GENERATED_BODY()

	/** Required and living for direct attacks and self loss; empty for environmental health loss. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName SourceUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardDamageKind Kind = EGameXXKCardDamageKind::Invalid;

	/** Fixed defense points ignored after the resolved target (including a guardian) is known. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 IgnoredDefense = 0;

	/** Optional packet-start Momentum snapshot used when this card packet consumes live Momentum before damage. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MomentumStacksOverride = INDEX_NONE;

	/** Statuses that belong to this direct hit and therefore are cancelled when agility avoids it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FGameXXKCardStatusStack> OnHitStatuses;
};

/** A unit-specific guard redirect. Guard is never represented by an unbound status stack. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardGuardLinkRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName GuardianUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName ProtectedUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Stacks = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardGuardRedirectPolicy RedirectPolicy = EGameXXKCardGuardRedirectPolicy::Invalid;
};

/** A UI-safe audit of one direct-damage attempt. It carries stable IDs, never array positions. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardDamageResult
{
	GENERATED_BODY()

	/** Stable source identity captured from the explicit damage context for audit and follow-up effects. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName SourceUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName OriginalTargetUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName ResolvedTargetUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 RequestedDamage = 0;

	/** Requested direct damage before Momentum is added. Zero for non-direct packets. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 BaseRequestedDamage = 0;

	/** Flat direct-damage contribution from the packet's Momentum snapshot. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MomentumDamageBonus = 0;

	/** Direct damage after Weak and before defense. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 DamageAfterWeak = 0;

	/** Amount removed by Weak from the Momentum-inclusive direct damage. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 WeakDamageReduction = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 DamageAfterDefense = 0;

	/** Compatibility field containing all direct-hit status amplification, including Mark. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 DamageAfterVulnerability = 0;

	/** Mark stacks on the final resolved receiver immediately before this hit. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MarkStacksBeforeHit = 0;

	/** Fixed additive Mark percentage applied to this hit, or zero. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MarkDamageBonusPercent = 0;

	/** Mark stacks removed by this hit; currently zero or one. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MarkStacksConsumed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 ArmorAbsorbed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 HealthDamage = 0;

	/** Resolved target health immediately before this successful damage attempt mutates combat state. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 TargetHealthBefore = 0;

	/** Resolved target health at the common successful tail, including unchanged avoid/armor-only outcomes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 TargetHealthAfter = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bRedirected = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bAvoidedByAgility = false;
};

/** Explicit serializable phase for the card-driven battle runtime. */
UENUM(BlueprintType)
enum class EGameXXKCardBattlePhase : uint8
{
	Invalid = 0 UMETA(Hidden),
	Player = 1,
	Enemy = 2,
	Victory = 3,
	Defeat = 4
};

/** One persisted delayed-effect instance. Recipient IDs are resolved when the source card is played. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardBattleModifierRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName ModifierId = NAME_None;

	/** Optional exact hand-card binding for a modifier that must never affect a different instance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName RequiredPlayedCardInstanceId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceCardInstanceId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName OriginalSelectedTargetUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> RecipientUnitIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKCardBattleModifier Definition;
};

/** Declarative permanent-character equipment effect. Battle runtime materializes these at combat start. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEquipmentActiveEffect
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, SaveGame)
	FName EffectId = NAME_None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, SaveGame)
	FName SourceCharacterId = NAME_None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, SaveGame)
	EGameXXKEquipmentSet Set = EGameXXKEquipmentSet::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, SaveGame)
	int32 RequiredPieces = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, SaveGame)
	EGameXXKEquipmentSetBonusScope Scope = EGameXXKEquipmentSetBonusScope::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, SaveGame)
	EGameXXKEquipmentSetBonusHook Hook = EGameXXKEquipmentSetBonusHook::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, SaveGame)
	EGameXXKEquipmentModifierKind ModifierKind = EGameXXKEquipmentModifierKind::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, SaveGame)
	int32 Magnitude = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, SaveGame)
	EGameXXKEquipmentMagnitudeUnit Unit = EGameXXKEquipmentMagnitudeUnit::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, SaveGame)
	int32 MaxTriggersPerRound = 0;
};

/** One persistent equipment descriptor attached exactly once to its permanent party source. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEquipmentBattleEffectRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKEquipmentActiveEffect ActiveEffect;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceCharacterId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 CurrentRoundTriggerCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 LastTriggerRound = 0;
};

/** Complete pure state of an in-progress card battle. It is deliberately independent from widget and scene indexes. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardBattleRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardBattlePhase Phase = EGameXXKCardBattlePhase::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardTerrain Terrain = EGameXXKCardTerrain::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RoundNumber = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKBattleDeckState Deck;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardCombatUnit> Units;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TMap<FName, FGameXXKEnemyBattleState> EnemyStates;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardGuardLinkRuntime> GuardLinks;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardBattleModifierRuntime> Modifiers;

	/** Equipment snapshots are materialized at battle start; card rules must never recalculate loadouts. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKEquipmentBattleEffectRuntime> EquipmentEffects;

	/** Monotonic per-battle source for stable modifier IDs. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 NextModifierOrdinal = 0;

	/** Extra future enemy intentions currently disclosed by cards or task-NPC passives. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RevealedEnemyIntentCount = 0;

	/** End-of-round effects that augment the next player phase's normal three shared energy. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PendingNextRoundEnergyBonus = 0;

	/** One enemy-phase request to surcharge one deterministic playable instance after the next hand refresh. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PendingNextPlayerHandEnergySurcharge = 0;

	/** Stable enemy source retained until the pending next-hand surcharge is materialized or expires. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName PendingNextPlayerHandEnergySurchargeSourceUnitId = NAME_None;
};

/** Read-only card-check result consumed by the hand UI before it enters the arrow-targeting state. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardPlayPreview
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName CardInstanceId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName CardId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName OwnerUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 EffectiveEnergyCost = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 EffectiveManaCost = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bCanPlay = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKCardTargetRequest TargetRequest;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FString FailureReason;
};

/** Stable audit result of one committed card play. It contains IDs and numerical packets, never UI indices. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardPlayResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName CardInstanceId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName CardId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName OwnerUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> TargetUnitIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardDamageResult> DamageResults;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bOpenedPendingChoice = false;
};
