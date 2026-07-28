#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"
#include "GameXXKCompanionTypes.h"
#include "GameXXKEnemyTypes.h"
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

/** A pending post-battle reward. It stays stable until the player picks, replaces a route card, or skips. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPendingRouteCardReward
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SourceNodeId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 ChoiceSeed = 0;

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

/** Stable provenance for a route-run card entry. */
UENUM(BlueprintType)
enum class EGameXXKRouteCardSourceKind : uint8
{
	Invalid = 0 UMETA(Hidden),
	HeroBase = 1,
	CompanionBase = 2,
	QuestNpcBase = 3,
	RouteReward = 4,
	Merchant = 5,
	RouteBase = 6
};

/** A stable, serializable card entry owned by one route run. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKRouteCardEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName EntryId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName CardId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardQuality CurrentQuality = EGameXXKCardQuality::Common;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKRouteCardSourceKind SourceKind = EGameXXKRouteCardSourceKind::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName OwnerUnitId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bTemporaryRouteCard = false;

	/** The sole authority for whether this stable entry occupies one of the route deck's 12 slots. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bConsumesRouteCapacity = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 AcquisitionOrdinal = INDEX_NONE;
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

	/** Explicit route provenance prevents a stale saved NPC selection from joining an unrelated route. */
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

	/** Only temporary route rewards live here; permanent hero/partner/NPC configuration remains elsewhere. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> RouteCardIds;

	/** Stable route-card entries; legacy RouteCardIds remains a transitional consumer input until migration/adapter integration. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKRouteCardEntry> RouteCardEntries;

	/** Dedicated stable-entry sequence, independent from the legacy reward-offer ordinal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 NextRouteCardEntryOrdinal = 0;

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
