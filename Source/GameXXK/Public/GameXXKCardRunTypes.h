#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"
#include "GameXXKCompanionTypes.h"
#include "GameXXKEnemyTypes.h"
#include "GameXXKPartyFormationTypes.h"
#include "GameXXKRelicTypes.h"
#include "GameXXKRouteMerchantTypes.h"
#include "GameXXKCardRunTypes.generated.h"

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKResolvedEnemyIntentEffect
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKEnemyIntentEffectType Type = EGameXXKEnemyIntentEffectType::DirectDamage;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> TargetUnitIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Magnitude = 0;

	/** Base magnitude before any saved source-status consumption. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 BaseMagnitude = 0;

	/** Source status and exact planned stack count consumed when this saved intent resolves. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardStatus ConsumedStatus = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ConsumedStacks = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 MagnitudePerConsumedStack = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bMagnitudePerConsumedStackUsesTargetMaxHealthPercent = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 HitCount = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardStatus Status = EGameXXKCardStatus::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 StatusStacks = 0;

	/** Source catalog target rule retained so a saved intent can validate a persistent target at execution time. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKEnemyIntentTargetRule TargetRule = EGameXXKEnemyIntentTargetRule::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bAssignsPersistentTarget = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bPhaseTwoFallbackToLowestHealth = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bClearsPersistentTargetAfterResolve = false;
};

/** A deterministic, serializable enemy action prepared for the current enemy phase. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardEnemyIntent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName CardId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FString CardDisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SourceUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SuggestedTargetUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SourceSlotNumber = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 TargetSlotNumber = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Damage = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardDamageKind Kind = EGameXXKCardDamageKind::SingleTargetAttack;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardStatusStack> OnHitStatuses;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName IntentDefinitionId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKEnemyIntentTargetRule TargetRule = EGameXXKEnemyIntentTargetRule::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKResolvedEnemyIntentEffect> Effects;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ResolutionOrder = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bCharging = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ChargeRounds = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FString PhaseLabel;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FString> TooltipLines;
};

/** Post-battle reward option kinds for the tiered three-choice (2026-08-14 redesign). */
UENUM(BlueprintType)
enum class EGameXXKBattleRewardKind : uint8
{
	None = 0 UMETA(Hidden),
	/** A hero or active-companion configured card; choosing upgrades its quality one step. */
	DeckCardUpgrade = 1,
	/** A boss-exclusive card entering the route deck (Boss battles only). */
	BossCard = 2,
	/** A relic grant. */
	Relic = 3,
	/** Permanent +1 shared-energy cap (Elite battles only). */
	EnergyCapBonus = 4,
	/** Permanent +1 draw-per-round (Elite battles only). */
	DrawBonus = 5
};

/** One typed post-battle three-choice option. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKBattleRewardOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKBattleRewardKind Kind = EGameXXKBattleRewardKind::None;

	/** DeckCardUpgrade / BossCard payload. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName CardId = NAME_None;

	/** Relic payload. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName RelicId = NAME_None;
};

/** A pending post-battle reward. It stays stable until the player picks, replaces a route card, or skips. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPendingRouteCardReward
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SourceNodeId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ChoiceSeed = 0;

	/** Tiered typed reward options; the legacy CardIds payload stays until reward generation switches over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKBattleRewardOption> Options;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> CardIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bRequiresRouteCardReplacement = false;
};

/** A pending non-combat route event that can optionally attach a task NPC for this route. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPendingRouteEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SourceNodeId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ChoiceSeed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName EventNpcId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName EncounterId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bCanRecruitPermanentCompanion = false;
};

/** Whether a validated acquisition can commit immediately (boss cards always commit once a slot is free). */
enum class EGameXXKRouteCardAcquisitionDecision : uint8
{
	CanCommit,
	RequiresReplacement
};

/** Pure acquisition summary shared by the boss-card reward preview. */
struct GAMEXXK_API FGameXXKRouteCardAcquisitionPreview
{
	EGameXXKRouteCardAcquisitionDecision Decision = EGameXXKRouteCardAcquisitionDecision::CanCommit;
	FName CardId = NAME_None;
};

UENUM(BlueprintType)
enum class EGameXXKRouteTerminalOutcome : uint8
{
	Cleared,
	Defeated,
	Abandoned
};

/** Durable terminal-route snapshot. The same ID may be applied at most once across save/reload. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKRouteSettlementReceipt
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGuid SettlementId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKRouteTerminalOutcome Outcome = EGameXXKRouteTerminalOutcome::Defeated;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SourceTravelMoney = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SourceCardAcquisitionCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 PermanentGoldAward = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 EnhancementStoneAward = 0;
};

/** Chapter-scoped idempotency receipt for one route node's travel-money award. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKRouteTravelMoneyReceipt
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Chapter = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 NodeId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Amount = 0;
};

/**
 * Route-local and permanent card-system state.  The legacy MVP battle arrays are a projection only;
 * this structure owns card zones, combat state, party configuration, deterministic offers, and seeds.
 */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardRunState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> HeroUnlockedCardIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> HeroSelectedCardIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKCompanionRosterState CompanionRoster;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKCompanionPartySelection PartySelection;

	/** Save-authoritative ordered 1P / 2P / 3P party references. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKOrderedPartyFormation OrderedFormation;

	/** Serialized v29-and-earlier route-provenance tombstone. Current runtime keeps this NAME_None. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName ActiveTemporaryQuestNpcId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bLoadoutLockedForRoute = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RouteRandomSeed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKRouteProgress RouteProgress;

	/** Currency earned and spent only within the active route; it converts at terminal settlement. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RouteTravelMoney = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bRouteEconomyInitialized = false;

	/** Chapter plus node is the stable key because node IDs restart in each chapter. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKRouteTravelMoneyReceipt> RewardedTravelMoneyNodes;

	/** Route-local merchant offers and an optional unresolved card-replacement purchase. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKRouteMerchantState RouteMerchant;

	/** A persisted receipt survives the save window between terminal snapshot and award application. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKRouteSettlementReceipt PendingSettlement;

	/** Idempotency key retained after route cleanup, so a replay cannot award permanent currency twice. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGuid LastAppliedRouteSettlementId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 NextRewardOrdinal = 0;

	/** Boss-exclusive cards owned by the player deck; at most MaxBossCardSlots entries (2026-08-14 redesign). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> BossCardSlots;

	/** The player deck accepts at most this many boss-exclusive cards. */
	static constexpr int32 MaxBossCardSlots = 3;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bHasActiveCardBattle = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ActiveBattleSourceNodeId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKCardBattleRuntime ActiveBattle;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKCardEnemyIntent> EnemyIntents;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 NextEnemyIntentIndex = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKPendingRouteCardReward PendingReward;

	/** Quality upgrades earned by choosing deck-card battle rewards; applied when battle decks assemble. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TMap<FName, EGameXXKCardQuality> UpgradedCardQualities;

	/** Permanent shared-energy cap bonus from Elite battle rewards. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 BonusSharedEnergyCap = 0;

	/** Permanent draw-per-round bonus from Elite battle rewards. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 BonusRoundDrawCount = 0;

	/** Set only after the player explicitly chooses or skips the current battle's saved reward offer. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bActiveBattleRewardResolved = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKPendingRouteEvent PendingEvent;

	/** Unlimited route-run relic inventory, newest first for the shared right-top HUD. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKRelicInstance> Relics;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 NextRelicAcquisitionOrdinal = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKPendingRelicOffer PendingRelicOffer;

	/** Attribute gains from positive events; applied only for the current route. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKRouteAttributeBonuses RouteAttributeBonuses;
};
