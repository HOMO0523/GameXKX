#pragma once

#include "CoreMinimal.h"
#include "GameXXKEnemyTypes.h"
#include "GameXXKEquipmentSetCatalog.h"
#include "GameXXKTrainingSettlementTypes.h"
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
	Epic = 3 UMETA(DisplayName = "史诗")
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

/** Why a card definition is being resolved. Only ActivePlay owns hand/payment/listener side effects. */
UENUM(BlueprintType)
enum class EGameXXKCardResolutionOrigin : uint8
{
	Invalid = 0 UMETA(Hidden),
	ActivePlay = 1,
	AutomaticReplay = 2,
	MageTaskReplay = 3,
	HeavyArrow = 4,
	Reaction = 5,
	TerrainListener = 6,
	TaskReward = 7,
	TaskNpcTaskReplay = 8,
	PartnerSorcererTaskReplay = 9,
	Equipment = 10
};

/** Declarative post-base behavior for protagonist Hunter cards. */
UENUM(BlueprintType)
enum class EGameXXKHeavyArrowKind : uint8
{
	None = 0,
	ExtraAttackPerCharge = 1,
	ToxicExplosionPerCharge = 2,
	AddPrimaryAttackPercentPerCharge = 3
};

/** Unit whose Charge is locked and consumed by a Heavy Arrow payload. */
UENUM(BlueprintType)
enum class EGameXXKHeavyArrowChargeSource : uint8
{
	CardOwner = 0,
	HighestAttackAlly = 1
};

/** When Charge is snapshotted. Hunter attacks lock before their base effects; support NPCs may grant it first. */
UENUM(BlueprintType)
enum class EGameXXKHeavyArrowLockTiming : uint8
{
	BeforeBaseEffects = 0,
	AfterBaseEffects = 1
};

/** Reward executed after the complete four-card protagonist Sorcerer task replay. */
UENUM(BlueprintType)
enum class EGameXXKHeroSpellTaskReward : uint8
{
	None = 0,
	Fire = 1,
	Ice = 2,
	Lightning = 3,
	Universal = 4
};

/** Sequencing family for one permanent Sorcerer-partner card. */
UENUM(BlueprintType)
enum class EGameXXKSorcererCardFamily : uint8
{
	None = 0,
	Core = 1,
	Fire = 2,
	Ice = 3,
	Lightning = 4,
	Universal = 5
};

/** Locked five-card task branch for a permanent Sorcerer partner. */
UENUM(BlueprintType)
enum class EGameXXKSorcererTaskBranch : uint8
{
	None = 0,
	Normal = 1,
	Fire = 2,
	Ice = 3,
	Lightning = 4
};

/** Data-selected base/sequence behavior for permanent Sorcerer-partner cards. */
UENUM(BlueprintType)
enum class EGameXXKSorcererSequenceRule : uint8
{
	None = 0,
	CoreSearch = 1,
	CoreManaEcho = 2,
	FireLamp = 3,
	FireSpread = 4,
	FireBurst = 5,
	FireSearch = 6,
	IceCurrentManaRestore = 7,
	IceMaxMana = 8,
	IceArmorDouble = 9,
	IceSearch = 10,
	LightningMark = 11,
	LightningSearch = 12,
	LightningMarkHits = 13,
	LightningStorm = 14,
	UniversalScalingAttack = 15,
	UniversalDraw = 16,
	UniversalPartyArmor = 17,
	UniversalSearch = 18
};

/** Starter reward selected by Sorcerer card data after a five-card replay. */
UENUM(BlueprintType)
enum class EGameXXKSorcererRewardRule : uint8
{
	None = 0,
	CoreSearch = 1,
	CoreManaEcho = 2,
	FireLamp = 3,
	FireSpread = 4,
	FireBurst = 5,
	FireSearch = 6,
	IceCurrentManaRestore = 7,
	IceMaxMana = 8,
	IceArmorDouble = 9,
	IceSearch = 10,
	LightningMark = 11,
	LightningSearch = 12,
	LightningMarkHits = 13,
	LightningStorm = 14,
	UniversalScalingAttack = 15,
	UniversalDraw = 16,
	UniversalPartyArmor = 17,
	UniversalSearch = 18
};

/** Data-selected special handling for the base effects of permanent Blade partner cards. */
UENUM(BlueprintType)
enum class EGameXXKBladeBaseRule : uint8
{
	None = 0,
	HealFromTriggeredBleed = 1,
	PreserveTriggeredBleed = 2,
	ConsumeVulnerabilityForExtraAttacks = 3,
	RefundCostsAndDrawOnKill = 4,
	OpenBladeExtraAttack = 5,
	OpenBladeResidualStyle = 6
};

/** The first-active-card payload contributed by a permanent Blade partner card. */
UENUM(BlueprintType)
enum class EGameXXKBladeChargeRule : uint8
{
	None = 0,
	ReplayNextActiveBase = 1,
	CopyNextActiveToHand = 2,
	ReturnNextActiveToHandOnce = 3,
	ReplayNextActiveNextRound = 4,
	RestoreNextActiveOwnerState = 5,
	DuplicateNextSingleTargetOrDraw = 6,
	MakeNextActiveEnergyFree = 7,
	MakeNextActiveManaFree = 8,
	RefundNextActiveCosts = 9,
	CountNextActiveTwice = 10,
	CopyNextActiveNextRound = 11,
	RetainNextActiveNextRound = 12,
	PreserveFinishCandidate = 13,
	RetainRemainingHand = 14,
	LightLoad = 15,
	DrawTwoAfterNextActive = 16,
	DrawSameOwnerAfterNextActive = 17,
	DrawOtherOwnerAfterNextActive = 18
};

/** The end-of-player-phase payload contributed by the final permanent Blade partner card. */
UENUM(BlueprintType)
enum class EGameXXKBladeFinishRule : uint8
{
	None = 0,
	ReturnFirstActiveNextRound = 1,
	MarkAndPrepareTwoCounters = 2,
	PreserveFirstTwoBleedTriggers = 3,
	DrawOnFirstThreeBleedTriggers = 4,
	HealBladeBleedCapTwelve = 5,
	ReturnFirstActiveAgainstBleeding = 6,
	FreezeVulnerabilityAndReplay = 7,
	CopyFirstStatusConsumer = 8,
	RefundFirstHighCostAndDrawTwo = 9,
	CopyFirstKill = 10,
	MarkAndReregisterCounterVolley = 11,
	FirstTwoDodgesFree = 12,
	TransferMarkBeforeCounter = 13,
	FirstCounterVolleyHitsAll = 14,
	StoreChargeAsNativeStyle = 15
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
	Counter = 26,
	Block = 27
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
	PlayedCard = 10,
	HighestArmorAlly = 11,
	HighestAttackAlly = 12,
	PriorityEnemy = 13,
	/** Every living unit on the manually selected unit's side. */
	SelectedTargetSide = 14
};

/** Unit whose attributes supply an effect's requested amount. */
UENUM(BlueprintType)
enum class EGameXXKCardEffectSource : uint8
{
	Invalid = 0 UMETA(Hidden),
	CardOwner = 1,
	SelectedTarget = 2,
	HighestArmorAlly = 3,
	HighestAttackAlly = 4
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
	RedirectSingleTargetEnemyAttacks = 28,
	RegisterReaction = 29,
	LoseHealthNonlethal = 30,
	Cleanse = 31,
	TriggerHighestDamageOverTime = 32,
	ResolveToxicExplosion = 33,
	HealOrReverseWithMedicine = 34,
	GainMedicineFromPartyHealthLoss = 35,
	DamagePercentAttackPlusArmor = 36,
	DamageAllPercentAttackPerConsumedArmor = 37,
	TriggerTerrainBenefit = 38,
	GainArmorFromCurrentManaPercent = 39,
	GainManaOverflowToArmor = 40,
	SearchUnfinishedHeroTaskCard = 41,
	TriggerStatus = 42,
	LightningPerTargetStatusSnapshot = 43,
	ReplayTriggeredCardBase = 44,
	ReplaySourceCardBase = 45,
	SearchUnfinishedTaskNpcCard = 46,
	ModifyManaCost = 47,
	WidenNextActiveSingleTarget = 48,
	PreserveNextReactionUse = 49,
	RetainArmorNextRound = 50,
	CleanseFriendlyDamageOverTime = 51,
	/** Heals an ally or applies equal unmitigated health loss to an enemy without consuming Medicine. */
	HealOrReverseFlat = 52,
	/** Switches the battle to TerrainOverride; choosing the current terrain is a valid no-op. */
	ChangeTerrain = 53,
	/** Deals Attack-percent damage plus an additional percentage for each live stack of Status on that target. */
	DamagePercentAttackPerTargetStatus = 54,
	/** Raises the target's maximum Mana without changing its current Mana. */
	IncreaseMaxMana = 55
};

/** Declares when and how an authored primary magnitude is resolved. */
UENUM(BlueprintType)
enum class EGameXXKCardMagnitudePolicy : uint8
{
	Unscaled = 0,
	ContinuousQuality = 1,
	ExplicitByQuality = 2,
	DotCoefficient = 3,
	PrintedCostArmor = 4,
	DefensePercent = 5,
	MedicineCoefficient = 6,
	DefenseIgnoreCoefficient = 7,
	/** GainManaOverflowToArmor: SecondaryMagnitude is a percentage of current Mana, rounded up. */
	CurrentManaPercentRecovery = 8,
	/** AddArmor: a percentage of the recipient's existing Armor, with no quality/level scaling. */
	CurrentArmorPercent = 9,
	/** AddArmor: copies the resolved integer named by ResultRef, with no further scaling. */
	PriorEffectResult = 10
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
	OwnerHasDamageOverTime = 8,
	TargetIsAlly = 9,
	TargetIsEnemy = 10
};

/** Persistent formula installed by the first successful active play of one permanent Healer card. */
UENUM(BlueprintType)
enum class EGameXXKHealerFormulaKind : uint8
{
	None = 0,
	AnyHealthChangeMedicine = 1,
	HighEnergyAndSixMedicine = 2,
	FirstHealingMedicine = 3,
	ThreeCleansedDotMedicine = 4,
	LowHealthCrossMedicine = 5,
	ThreeEffectiveHealsDraw = 6,
	BleedRemovedPartyArmor = 7,
	LargeHealingArmorOrVulnerability = 8,
	LowHealthCrossAgility = 9,
	ThreeUnitHealthChangeDrawMana = 10,
	PoisonDamageMedicine = 11,
	BleedPoisonMark = 12,
	GroupPoisonMedicineDraw = 13,
	DualDotExplosionMedicine = 14,
	TwoBleedPacketsMedicine = 15,
	GroupDirectDamageEnergy = 16,
	PoisonedVulnerabilityMedicineDraw = 17,
	TripleDotExplosionMomentumDraw = 18,
	/** First actual HP loss per distinct ally each round grants Medicine, at most three allies. */
	HeroFirstPartyHealthLossMedicine = 19,
	/** First Hero healing/reversal action each round that spends at least six Medicine draws one. */
	HeroSixMedicineHealDraw = 20,
	/** A two-or-more-type Toxic Explosion grants Medicine twice per round at most. */
	HeroDualDotExplosionMedicine = 21,
	/** First action each round that effectively heals at least two allies grants shared Energy. */
	HeroGroupHealEnergy = 22
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
	OnSingleTargetEnemyAttack = 6,
	BeforeNextActiveCard = 7,
	AfterNextActiveCard = 8,
	NextPlayerRoundStart = 9,
	BeforeFirstActiveCardNextPlayerRound = 10,
	AfterFirstActiveCardNextPlayerRound = 11,
	FirstActiveAttackAgainstStatusNextPlayerRound = 12,
	AfterEachActiveCard = 13,
	BeforeNextTerrainBenefit = 14
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

/** Declarative Blade sequence identity. Runtime resolves these enums without CardId branches. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKBladeSequenceRule
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKBladeBaseRule BaseRule = EGameXXKBladeBaseRule::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKBladeChargeRule ChargeRule = EGameXXKBladeChargeRule::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKBladeFinishRule FinishRule = EGameXXKBladeFinishRule::None;
};

/** Declarative identity for one permanent Sorcerer-partner card. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKSorcererCardRule
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKSorcererCardFamily Family = EGameXXKSorcererCardFamily::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKSorcererSequenceRule SequenceRule = EGameXXKSorcererSequenceRule::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKSorcererRewardRule RewardRule = EGameXXKSorcererRewardRule::None;
};

/** Data-only Heavy Arrow payload; runtime behavior is selected by Kind rather than CardId. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKHeavyArrowRule
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKHeavyArrowKind Kind = EGameXXKHeavyArrowKind::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MagnitudePerCharge = 0;

	/** Quality handling for MagnitudePerCharge; discrete Heavy Arrow fields stay unscaled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardMagnitudePolicy MagnitudePolicy = EGameXXKCardMagnitudePolicy::Unscaled;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 RareMagnitudePerCharge = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 EpicMagnitudePerCharge = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 DrawPerCharge = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MinimumChargeForEnergy = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 EnergyGain = 0;

	/** Protagonist cards use CardOwner; task-NPC support cards may empower the highest-Attack ally. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKHeavyArrowChargeSource ChargeSource = EGameXXKHeavyArrowChargeSource::CardOwner;

	/** Mana restored to the locked Charge owner for each consumed stack. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 ManaPerCharge = 0;

	/** Extra primary-packet scaling used when the main payload is an attack or Toxic Explosion. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 AdditionalPrimaryAttackPercentPerCharge = 0;

	/** Raw coefficient, quality/level generated once per locked Charge, with no DOT cap. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 IgnoreDefensePerCharge = 0;

	/** Additional live Bleed resolutions after the base hit for each locked Charge stack. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 TriggeredBleedResolutionsPerCharge = 0;

	/** Optional post-hit status granted once per locked Charge stack. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardStatus BonusStatus = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 BonusStatusStacksPerCharge = 0;

	/** One status grant per complete interval of locked Charge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 BonusStatusChargeInterval = 1;

	/** Zero means no additional cap. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MaxBonusStatusStacks = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardEffectTarget BonusStatusTarget = EGameXXKCardEffectTarget::Invalid;

	/** Percentage-point increase to a TargetHealthBelowPercent attack attachment per Charge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 HealthThresholdPointsPerCharge = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKHeavyArrowLockTiming LockTiming = EGameXXKHeavyArrowLockTiming::BeforeBaseEffects;
};

/** Data-only permanent-Hunter sequencing and one-shot setup payloads. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKHunterCardRule
{
	GENERATED_BODY()

	/** Added to the primary attack for every active card already played this round. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PrimaryAttackPercentPerPriorActiveCard = 0;

	/** Completed intervals are floor(prior active cards / interval). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 PriorActiveCardInterval = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 DrawPerCompletedInterval = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardStatus StatusPerCompletedInterval = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 StatusStacksPerCompletedInterval = 0;

	/** Accumulated Defense ignore consumed by this owner's next Heavy Arrow card. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 NextHeavyArrowIgnoreDefense = 0;

	/** Accumulated Charge granted by this owner's next successful perfect Agility dodge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 ChargeOnNextPerfectDodge = 0;
};

/** Data-only permanent-Healer cost and formula identity. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKHealerCardRule
{
	GENERATED_BODY()

	/** Added while this CardId's formula has not yet been installed this battle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 UnopenedFormulaEnergySurcharge = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKHealerFormulaKind FormulaKind = EGameXXKHealerFormulaKind::None;
};

/** One formula already opened by a successful permanent-Healer active play. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKHealerFormulaRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName OwnerUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceCardId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKHealerFormulaKind Kind = EGameXXKHealerFormulaKind::None;

	/** Older saves lack this field and use the catalog's base quality. New formulas lock opening quality. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardQuality SourceQuality = EGameXXKCardQuality::Invalid;

	/** Generic cumulative counter used by the selected formula kind. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Progress = 0;

	/** Generic per-phase counter used by formulas with separate player/enemy thresholds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PhaseProgress = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bProgressFromEnemyPhase = false;

	/** Zero means the formula has not spent its once-per-player-round budget. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 LastTriggeredRound = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SecondaryLastTriggeredRound = 0;

	/** Per-target once-per-round formulas store their current budget round here. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 UnitBudgetRound = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> TriggeredUnitIdsThisRound;
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardEffectConditionType Type = EGameXXKCardEffectConditionType::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardStatus Status = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 MinimumStatusStacks = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 MinimumArmor = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	float HealthPercentThreshold = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardTerrain Terrain = EGameXXKCardTerrain::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardTerrain AlternateTerrain = EGameXXKCardTerrain::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bConsumeStatus = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 MaxConsumedStatusStacks = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bScaleMagnitudeByConsumedStacks = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bConsumeOwnerArmor = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 MaxConsumedArmor = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bNegate = false;
};

/** Persistent data used by a future battle resolver to register delayed or reactive effects. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardBattleModifier
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardBattleModifierTrigger Trigger = EGameXXKCardBattleModifierTrigger::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardEffectType EffectType = EGameXXKCardEffectType::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardEffectTarget Target = EGameXXKCardEffectTarget::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardModifierRecipientScope RecipientScope = EGameXXKCardModifierRecipientScope::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardEffectTarget RecipientTarget = EGameXXKCardEffectTarget::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCharacterRole RequiredTriggeredRole = EGameXXKCharacterRole::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName RequiredTriggeredOwnerId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardModifierExpiry Expiry = EGameXXKCardModifierExpiry::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardTriggeredAttackTargetScope TriggeredAttackTargetScope = EGameXXKCardTriggeredAttackTargetScope::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardStatus Status = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Magnitude = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardMagnitudePolicy MagnitudePolicy = EGameXXKCardMagnitudePolicy::Unscaled;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RareMagnitude = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 EpicMagnitude = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RemainingTriggers = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 MinimumResult = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bPersistent = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bActivePlayOnly = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bExcludeSourceUnit = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bPreserveTriggeredStatus = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
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
	EGameXXKCardEffectSource Source = EGameXXKCardEffectSource::CardOwner;

	/** Legacy save metadata only. Healing coefficients always use the full current quality multiplier. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardQuality CoefficientReferenceQuality = EGameXXKCardQuality::Common;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Magnitude = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardMagnitudePolicy MagnitudePolicy = EGameXXKCardMagnitudePolicy::Unscaled;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RareMagnitude = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 EpicMagnitude = INDEX_NONE;

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

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardTerrain TerrainOverride = EGameXXKCardTerrain::Invalid;

	/** Names an integer result produced by this effect for later effects in the same resolution. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName ResultGroupId = NAME_None;

	/** References a prior integer result without relying on effect-array position. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName ResultRef = NAME_None;

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

	/** Optional final authored Mana costs for Rare and Epic. Both must be present or both absent. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 RareManaCost = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 EpicManaCost = INDEX_NONE;

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

	/** One or more equally selectable birth-build tags for non-core permanent-partner cards. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FName> ProfessionArchetypeIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIdentityLocked = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCharacterRole LinkedRole = EGameXXKCharacterRole::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 HeroUnlockLevel = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bExhaustOnPlay = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FGameXXKCardEffect> ChargeEffects;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FGameXXKCardEffect> FinishEffects;

	/** Permanent Blade partner rules; each stage is selected by data rather than CardId. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameXXKBladeSequenceRule BladeSequence;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameXXKHeavyArrowRule HeavyArrow;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameXXKHunterCardRule HunterRule;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameXXKHealerCardRule HealerRule;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FGameXXKSorcererCardRule SorcererRule;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKHeroSpellTaskReward SpellTaskReward = EGameXXKHeroSpellTaskReward::None;

	/** Reward resolved only after this named task NPC's carried three-card task completes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FGameXXKCardEffect> TaskNpcRewardEffects;
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
	InsightChooseToHand = 3,
	HeroTaskSearchChooseToHand = 4
};

/** A logical card zone. Pending-choice candidates are references/views, not a second owning zone. */
UENUM(BlueprintType)
enum class EGameXXKCardZone : uint8
{
	Invalid = 0 UMETA(Hidden),
	DrawPile = 1,
	Hand = 2,
	DiscardPile = 3,
	ExhaustPile = 4,
	PendingAutomaticHand = 5
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

	/** Generated copies remain real deck instances but expire at their owning player-round boundary. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bTemporary = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 EnergyCostOverride = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ManaCostOverride = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ExpireAfterPlayerRound = 0;
};

/** Persisted data for a choice that blocks normal deck mutations until it is resolved or cancelled. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPendingCardChoice
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardPendingChoiceKind Kind = EGameXXKCardPendingChoiceKind::Invalid;

	/** An already-open legacy Hero search may release a saved replay after its old generic requirements retire. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bLegacyHeroTaskSearch = false;

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
	TArray<FGameXXKCardInstance> ExhaustPile;

	/** Stable overflow zone for rules-owned automatic additions while the twenty-card hand is full. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardInstance> PendingAutomaticHandCards;

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

	/**
	 * Runtime-only identity of the synchronously resolving active card. It remains in its normal
	 * discard/exhaust/hand zone for explicit Blade mechanics, but generic draws must not recycle it
	 * until the outer card transaction has finished.
	 */
	UPROPERTY(Transient)
	FName ResolvingCardInstanceId = NAME_None;
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 SettlementHealthLost = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 SettlementHealingReceived = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 SettlementArmorGenerated = 0;

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

	/** Armor is authoritative for card resolution and has no gameplay cap. */
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
	DamageOverTime = 5,
	/** Card-authored numeric damage: bypasses Attack/Defense, but keeps direct-status, Armor, and level rules. */
	FixedDamage = 6
};

/** Identifies why a damage packet exists without changing its mitigation policy. */
UENUM(BlueprintType)
enum class EGameXXKCardDamageCause : uint8
{
	Invalid = 0 UMETA(Hidden),
	DirectAttack = 1,
	Counter = 2,
	Bleed = 3,
	Poison = 4,
	Burn = 5,
	Rot = 6,
	ToxicExplosionBleed = 7,
	ToxicExplosionPoison = 8,
	ToxicExplosionBurn = 9,
	SelfLoss = 10,
	Environment = 11,
	Block = 12,
	Medicine = 13,
	Relic = 14,
	FixedDamage = 15,
	ToxicExplosionRot = 16
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardResolutionOrigin ResolutionOrigin = EGameXXKCardResolutionOrigin::Invalid;

	/** Runtime-only card effect index used to project card-generation values into tooltip rows. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 SourceEffectIndex = INDEX_NONE;

	/** Deterministic 0..99 roll supplied by runtime direct-attack entrypoints. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 AgilityRollPercent = 0;

	/** Fixed defense points ignored after the resolved target (including a guardian) is known. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 IgnoredDefense = 0;

	/** Optional packet-start Momentum snapshot used when this card packet consumes live Momentum before damage. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MomentumStacksOverride = INDEX_NONE;

	/** Optional explicit Vulnerability consumption limit; INDEX_NONE keeps the global consume-all rule. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 VulnerabilityStacksToConsumeOverride = INDEX_NONE;

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

	/** Runtime-only card effect index; INDEX_NONE for reactions, terrain, enemy, and environmental packets. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 SourceEffectIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bTriggeredEnemyPhase = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 EnemyPhaseBefore = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 EnemyPhaseAfter = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 EnemyPhaseHealing = 0;

	/** True only when the permanent talent critical roll amplified this direct party hit. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bTalentCriticalHit = false;

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

	/** Damage after Defense and direct-status amplification, immediately before level difference. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 DamageBeforeLevelDifference = 0;

	/** Damage after the clamped source/target level difference and before Armor. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 DamageAfterLevelDifference = 0;

	/** Vulnerability stacks on the resolved receiver immediately before this hit. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 VulnerabilityStacksBeforeHit = 0;

	/** Vulnerability stacks removed by this hit after applying any explicit card override. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 VulnerabilityStacksConsumed = 0;

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

	/** Resolved target armor immediately before this successful damage attempt mutates combat state. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 TargetArmorBefore = 0;

	/** Resolved target armor immediately after this packet, before later effects may grant new armor. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 TargetArmorAfter = 0;

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

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 AgilityRollPercent = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 AgilityStacksConsumed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bPerfectAgilityDodge = false;

	/** Mitigation policy that produced this packet; never inferred from target mode or card name. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardDamageKind Kind = EGameXXKCardDamageKind::Invalid;

	/** Semantic source of this packet, independent of the mitigation path used to resolve it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardDamageCause Cause = EGameXXKCardDamageCause::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKCardResolutionOrigin ResolutionOrigin = EGameXXKCardResolutionOrigin::Invalid;

	/** Matching DoT stacks captured before this status packet was queued. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 StatusStacksBefore = 0;

	/** Flat Rot contribution added to this real Bleed, Poison, or Burn packet. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 RotDamageBonus = 0;

	/** Matching DoT stacks consumed after this packet; zero for a preserved explosion. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 StatusStacksConsumed = 0;
};

/** Actual status mutation produced inside one committed card transaction. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardStatusChangeResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName TargetUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardStatus Status = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 AppliedStacks = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RemovedStacks = 0;
};

/** Actual positive healing produced inside one committed card transaction. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardHealingResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName TargetUnitId = NAME_None;

	/** Runtime-only originating card effect index. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 SourceEffectIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RequestedHealing = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 EffectiveHealing = 0;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardArmorResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName TargetUnitId = NAME_None;

	/** Runtime-only originating card effect index. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 SourceEffectIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RequestedArmor = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 EffectiveArmor = 0;
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

/** Persisted semantic source kind for a card battle, independent of generated-route provenance. */
UENUM(BlueprintType)
enum class EGameXXKCardBattleNodeKind : uint8
{
	Invalid = 0 UMETA(Hidden),
	Battle = 1,
	Elite = 2,
	Boss = 3
};

/** Stable card/owner/target data sufficient to replay base effects after the live instance moves. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKResolvedCardSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName CardId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardQuality Quality = EGameXXKCardQuality::Common;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName OwnerUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> OriginalTargetUnitIds;

	/** Actual Mana paid by the active play; automatic resolutions never overwrite it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PaidManaCost = 0;

	/** One-based first-play position inside a permanent Sorcerer partner task, or zero otherwise. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SorcererSequencePosition = 0;

	/** Family of the immediately preceding recorded Sorcerer card, or None at position one. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKSorcererCardFamily PreviousSorcererFamily = EGameXXKSorcererCardFamily::None;

	/** Branch locked for replay; a Universal starter may remain None until its second distinct card. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKSorcererTaskBranch SorcererTaskBranch = EGameXXKSorcererTaskBranch::None;
};

/** One ordinary Blade Charge waiting for the next successful active card this player round. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKBladeChargeRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKBladeChargeRule Rule = EGameXXKBladeChargeRule::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceCardId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardQuality SourceQuality = EGameXXKCardQuality::Common;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceOwnerUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 CreatedRound = 0;
};

/** One next-active Blade Charge payload deferred to the following player round. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKBladeDelayedCardRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKBladeChargeRule Rule = EGameXXKBladeChargeRule::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceCardId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardQuality SourceQuality = EGameXXKCardQuality::Common;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceOwnerUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKResolvedCardSnapshot RecordedCard;

	/** Exact original instance used when a delayed Charge must copy or return the physical card. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKCardInstance RecordedInstance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 TriggerPlayerRound = 0;
};

/** One Sheathed-style Charge saved for a later active card without occupying the ordinary Charge slot. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKBladeStyleRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKBladeChargeRule Rule = EGameXXKBladeChargeRule::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceCardId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardQuality SourceQuality = EGameXXKCardQuality::Common;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceOwnerUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 TriggerPlayerRound = 0;

	/** Residual styles target the next active this round and may never create another residual style. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bResidual = false;
};

/** One Blade Finish waiting for its next-player-round active-card window. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKBladeFinishRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKBladeFinishRule Rule = EGameXXKBladeFinishRule::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceCardId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardQuality SourceQuality = EGameXXKCardQuality::Common;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceOwnerUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 TriggerPlayerRound = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RemainingTriggers = 0;

	/** Exact enemy whose existing Vulnerability is protected by Duan Yue until the next player round. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName ProtectedTargetUnitId = NAME_None;

	/** Minimum pre-existing Vulnerability restored after the protected enemy phase. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ProtectedStatusStacks = 0;

	/** Prevents a multi-hit enemy card from preparing this Finish once per packet. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bTriggeredForCurrentEnemyCard = false;
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKResolvedCardSnapshot SourceCardSnapshot;
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
	int32 SecondaryMagnitude = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, SaveGame)
	EGameXXKEquipmentMagnitudeUnit Unit = EGameXXKEquipmentMagnitudeUnit::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, SaveGame)
	int32 MaxTriggersPerRound = 0;
};

/** One wearer-owned PoJun copy of a successful Blade finisher's Charge. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPoJunStoredStyleRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKBladeChargeRule Rule = EGameXXKBladeChargeRule::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceCardId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardQuality SourceQuality = EGameXXKCardQuality::Common;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceOwnerUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 TriggerPlayerRound = 0;
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

	/** Four-piece style slot. It is local to this exact wearer descriptor. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKPoJunStoredStyleRuntime PendingPoJunStyle;

	/** Six-piece progress exists only in the player round that consumed this wearer's Charge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PoJunChargeProgressRound = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bPoJunChargeConsumedThisRound = false;

	/** Six-piece replay is consumed by the first successful active card in this player round. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PendingPoJunReplayPlayerRound = 0;
};

/** One independently consumable Counter or Block source registered for an enemy-card boundary. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKReactionRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName ReactionId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardStatus Status = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName RecipientUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName GrantedByUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceCardInstanceId = NAME_None;

	/** One card-effect registration may grant multiple layers; only one layer from that batch fires per enemy card. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RegistrationBatchOrdinal = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RemainingTriggers = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ExpireBeforePlayerRound = 0;
};

/** Serializable continuation for automatic base-card replays interrupted by an existing card choice. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKAutomaticResolutionQueue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bActive = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardResolutionOrigin Origin = EGameXXKCardResolutionOrigin::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKResolvedCardSnapshot> PendingCards;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 NextCardIndex = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKHeroSpellTaskReward PendingReward = EGameXXKHeroSpellTaskReward::None;

	/** Permanent Sorcerer-partner starter reward, mutually exclusive with PendingReward. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKSorcererRewardRule PendingSorcererReward = EGameXXKSorcererRewardRule::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName RewardOwnerUnitId = NAME_None;
};

/** Deferred Shanhe reward that waits for the active card's choices and automatic replays to finish. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPendingEquipmentRewardRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName DrawEffectId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName ManaEffectId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 TriggerRound = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 DrawCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 OtherAllyMana = 0;
};

/** Persisted progress and first-play ordering for the protagonist four-card Sorcerer task. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKHeroSpellTaskRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bActive = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> LockedHeroCardIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> CompletedHeroCardIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKResolvedCardSnapshot> FirstPlayOrder;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKHeroSpellTaskReward StarterReward = EGameXXKHeroSpellTaskReward::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName StarterOwnerUnitId = NAME_None;
};

/** Persisted three-card task progress for one named task NPC. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTaskNpcSpellTaskRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bActive = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName OwnerUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> LockedCardIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> CompletedCardIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKResolvedCardSnapshot> FirstPlayOrder;
};

/** Persisted five-card task progress and per-battle auto-hand history for one Sorcerer partner. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKSorcererPartnerTaskRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bActive = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName OwnerUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> LockedCardIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> CompletedCardIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKResolvedCardSnapshot> FirstPlayOrder;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKSorcererRewardRule StarterReward = EGameXXKSorcererRewardRule::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKSorcererTaskBranch LockedBranch = EGameXXKSorcererTaskBranch::None;

	/** Each Universal card may be moved automatically at most once per battle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> AutoHandedUniversalCardIds;
};

/** Saved enemy-card identity lock. Forecast refreshes values without rerolling this identity. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEnemyIntentLock
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName IntentId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PhaseNumber = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RoundNumber = 0;
};

/** Complete pure state of an in-progress card battle. It is deliberately independent from widget and scene indexes. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardBattleRuntime
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKBattleSessionStats SessionStats;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardBattlePhase Phase = EGameXXKCardBattlePhase::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardBattleNodeKind SourceNodeKind = EGameXXKCardBattleNodeKind::Invalid;

	/** Exact semantic encounter identity; never inferred from a truncated node hash. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceEncounterId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardTerrain Terrain = EGameXXKCardTerrain::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RoundNumber = 0;

	/** Highest living party combat level captured once when this battle starts. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 TeamMaxLevelSnapshot = 1;

	/** Normal/Hard/Hell enemy direct-damage multiplier stored as 100/125/150. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 EnemyDifficultyDamagePercent = 100;

	/** Explicit value/status/deck difficulty; must agree with EnemyDifficultyDamagePercent. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKEnemyDifficulty EnemyDifficulty = EGameXXKEnemyDifficulty::Normal;

	/** Transient guard used by copied tooltip simulations so gameplay audit logs stay truthful. */
	UPROPERTY(Transient)
	bool bSuppressEquipmentTriggerAudit = false;

	/** Accumulated enemy denial consumed by the next player-round energy refill. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PendingNextRoundEnergyPenalty = 0;

	/** Exact protagonist eight-card equipment snapshot; never inferred from the materialized battle deck. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> EquippedHeroCardIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ActiveCardsPlayedThisRound = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKResolvedCardSnapshot LastActiveCard;

	/** Automatic resolutions never create or consume this active-play-only sequence payload. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKBladeChargeRuntime PendingBladeCharge;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKBladeDelayedCardRuntime PendingBladeDelayedCard;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKBladeFinishRuntime PendingBladeFinish;

	/** A Sheathed finisher's Charge, reserved for the first active card of its trigger round. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKBladeStyleRuntime PendingBladeNativeStyle;

	/** Heng Yun may copy one consumed native style here for exactly the next active card. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKBladeStyleRuntime PendingBladeResidualStyle;

	/** Non-temporary cards that Yi Shi's consumed Charge may keep through this player-round boundary. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> BladeRetainedHandCardInstanceIds;

	/** Owner-keyed one-shot setup created by permanent Hunter cards such as Eagle Eye. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TMap<FName, int32> PendingHunterHeavyArrowIgnoreDefense;

	/** Owner-keyed one-shot setup consumed only by a successful perfect Agility dodge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TMap<FName, int32> PendingHunterPerfectDodgeCharge;

	/** Open formulas are keyed by their stable owner plus CardId; duplicate instances share one formula. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKHealerFormulaRuntime> HealerFormulas;

	/** Cumulative Medicine gain remainder; every complete six grants Momentum once. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TMap<FName, int32> MedicineGainRemainderByOwner;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bTerrainChangedThisRound = false;

	/** Run-scoped Elite reward bonuses, copied in at battle start. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 BonusSharedEnergyCap = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 BonusRoundDrawCount = 0;

	/** Independent deterministic stream for combat rolls such as Agility. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 CombatRandomState = 0;

	/** Permanent talent snapshot captured once at battle start. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 TalentFinalDamagePercent = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 TalentCriticalChancePercent = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 TalentCriticalDamagePercent = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKBattleDeckState Deck;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardCombatUnit> Units;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TMap<FName, FGameXXKEnemyBattleState> EnemyStates;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKEnemyIntentLock> LockedEnemyIntents;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardGuardLinkRuntime> GuardLinks;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardBattleModifierRuntime> Modifiers;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKReactionRuntime> Reactions;

	/** Team-wide one-shot protection for the next independently stored Counter or Block source. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PendingPreservedPartyReactionUses = 0;

	/** Party units whose armor skips exactly the next party phase-start decay point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> RetainArmorAtNextPartyPhaseUnitIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 NextReactionOrdinal = 0;

	/** Monotonic source for generated temporary card identities. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 NextGeneratedCardOrdinal = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKAutomaticResolutionQueue AutomaticResolutionQueue;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKHeroSpellTaskRuntime HeroSpellTask;

	/** Prevents more than one completed protagonist Sorcerer task in a player round. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 HeroSpellTaskLastCompletedRound = 0;

	/** Owner-scoped permanent Sorcerer five-card tasks; inactive entries retain only battle history. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKSorcererPartnerTaskRuntime> SorcererPartnerTasks;

	/** Independent named-NPC spell tasks; one active task per NPC owner. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKTaskNpcSpellTaskRuntime> TaskNpcSpellTasks;

	/** Equipment snapshots are materialized at battle start; card rules must never recalculate loadouts. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKEquipmentBattleEffectRuntime> EquipmentEffects;

	/** Ordered equipment rewards waiting for the current choice/replay chain to finish. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKPendingEquipmentRewardRuntime> PendingEquipmentRewards;

	/** Triggered draws that must wait until the current blocking card choice and replay queue finish. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PendingTriggeredDrawCount = 0;

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

	/** Battle projection of the owned one-use life-saving talisman; initialized once at battle start. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bLifeSavingTalismanArmed = false;

	/** Candidate-only catalog-consumption request raised by the first protected party health-loss packet. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bLifeSavingTalismanConsumptionPending = false;

	/** Catalog-authored party-healing percentage projected with the armed or pending life-saving talisman. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 LifeSavingTalismanHealingPercent = 0;
};

/** Semantic category for one card-owned generated value before target mitigation. */
UENUM(BlueprintType)
enum class EGameXXKCardDisplayValueKind : uint8
{
	Invalid = 0 UMETA(Hidden),
	AttackDamage = 1,
	FixedDamage = 2,
	DamageOverTime = 3,
	Healing = 4,
	Armor = 5,
	ManaRecovery = 6
};

/** Pure card-generation value paired with an optional target outcome from a copied runtime. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardResolvedDisplayValue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 EffectIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKCardDisplayValueKind Kind = EGameXXKCardDisplayValueKind::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FName SourceUnitId = NAME_None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FName TargetUnitId = NAME_None;

	/** Manual target whose branch produced this value; empty for automatic/no-target cards. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FName OriginalSelectedTargetUnitId = NAME_None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKCardStatus Status = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 BaseMagnitude = 0;

	/** Card-owned number shown in compact copy, before target mitigation/capacity. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 ResolvedMagnitude = 0;

	/** Optional copied-target result; card text never substitutes this for the generated value. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 ActualMagnitude = 0;

	/** Quality and level folded into one percentage; attacks store their final attack percentage. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 AmplificationPercent = 100;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 ReservoirCap = 0;

	/** Portion of a generated Mana recovery that exceeded the current cap before Armor conversion. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 OverflowMagnitude = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 HitCount = 1;
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

	/** Runtime-only identity of the unused Shanhe four-piece discount selected by the pure preview. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FName AppliedShanHeFourPieceEffectId = NAME_None;

	/** Non-mutating card-generation values resolved from the actual battle owner/source stats. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FGameXXKCardResolvedDisplayValue> ResolvedDisplayValues;

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
	EGameXXKCardResolutionOrigin ResolutionOrigin = EGameXXKCardResolutionOrigin::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> TargetUnitIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardDamageResult> DamageResults;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardStatusChangeResult> StatusChanges;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardHealingResult> HealingResults;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardArmorResult> ArmorResults;

	/** One entry per real Toxic Explosion operation, containing its distinct resolved DOT-type count. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<int32> ToxicExplosionDistinctDotTypeCounts;

	/** Charge locked and consumed by this Heavy Arrow action. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 HeavyArrowChargeConsumed = 0;

	/** Extra direct-attack packets appended by the Heavy Arrow rule. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 HeavyArrowExtraAttackCount = 0;

	/** Extra Toxic Explosion resolutions appended by the Heavy Arrow rule. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 HeavyArrowToxicExplosionCount = 0;

	/** Percentage points merged into the Heavy Arrow's primary attack. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 HeavyArrowPrimaryBonusPercent = 0;

	/** Number of automatic card snapshots/rewards completed as part of this active-card transaction. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 AutomaticResolutionCount = 0;

	/** Largest saved automatic queue observed before this active-card transaction resumed it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 MaximumAutomaticQueueDepth = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bOpenedPendingChoice = false;
};
