#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"
#include "GameXXKRouteMerchantTypes.generated.h"

/** The persisted kind of a single route-merchant offer. */
UENUM(BlueprintType)
enum class EGameXXKRouteMerchantOfferKind : uint8
{
	Invalid = 0 UMETA(Hidden),
	Card,
	Relic
};

/** Stable, UI-facing purchase failure categories. Values are append-only save/UI contracts. */
UENUM(BlueprintType)
enum class EGameXXKRouteMerchantPurchaseFailure : uint8
{
	None = 0,
	InvalidRouteContext,
	InvalidMerchantStock,
	PendingPurchaseConflict,
	StaleOfferId,
	OfferUnavailable,
	OfferAlreadySold,
	InsufficientTravelMoney,
	InvalidCardDefinition,
	InvalidActiveCompanion,
	DuplicateRelic,
	InvalidRouteCardOrdinal,
	DeckAcquisitionRejected,
	InvalidReplacementEntryId,
	RelicAcquisitionRejected,
	ArithmeticOverflow
};

/** A deterministic offer held by one visited route merchant. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKRouteMerchantOffer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName OfferId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKRouteMerchantOfferKind Kind = EGameXXKRouteMerchantOfferKind::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName ContentId = NAME_None;

	/** Catalog base quality captured with the stock so reopening never rerolls classification. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCardQuality Quality = EGameXXKCardQuality::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Price = 0;

	/** Explicit stable empty-slot representation used when every legal item was collected. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bUnavailable = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bSold = false;
};

/** A purchase paused while the player chooses a stable route-card EntryId to replace. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPendingRouteMerchantPurchase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bActive = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName OfferId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName CardId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Price = 0;
};

/** Route-local merchant snapshot. OfferSeed is the retained wire name for StockSeed. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKRouteMerchantState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SourceNodeId = INDEX_NONE;

	/** Stable stock seed; legacy field name retained for source/save compatibility. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 OfferSeed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RefreshCount = 0;

	/** Exactly three card slots followed by exactly three relic slots for a nonempty snapshot. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKRouteMerchantOffer> Offers;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKPendingRouteMerchantPurchase PendingPurchase;
};

/** Public read-model for one merchant offer. This type is intentionally not persisted. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKRouteMerchantOfferView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FGameXXKRouteMerchantOffer SavedOffer;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bAffordable = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bPurchaseEnabled = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FString DisabledReason;
};

/** Public read-model for the route merchant. This type is intentionally not persisted. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKRouteMerchantView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 RouteTravelMoney = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FGameXXKRouteMerchantOfferView> CardOffers;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FGameXXKRouteMerchantOfferView> RelicOffers;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 RefreshCost = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bRefreshAffordable = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bRefreshEnabled = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FString RefreshDisabledReason;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bHasPendingReplacement = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bCanLeave = true;
};

/** Pure card/relic purchase simulation. No preview call mutates the runtime. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKRouteMerchantPurchasePreview
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bCanPurchase = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bRequiresReplacement = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FGameXXKRouteMerchantOffer Offer;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 BalanceBefore = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 BalanceAfter = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 Price = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FName MergeSurvivorEntryId = NAME_None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FName> ConsumedEntryIds;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKCardQuality FinalQuality = EGameXXKCardQuality::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 TemporaryCountDelta = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 CapacityDelta = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FName> EligibleReplacementEntryIds;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKRouteMerchantPurchaseFailure Failure = EGameXXKRouteMerchantPurchaseFailure::None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FString FailureReason;
};

/** Atomic purchase result. It repeats preview facts so UI never has to inspect mutable runtime arrays. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKRouteMerchantPurchaseResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bPurchased = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bRequiresReplacement = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FGameXXKRouteMerchantOffer Offer;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FName OfferId = NAME_None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FName CardId = NAME_None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 BalanceBefore = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 BalanceAfter = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 Price = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FName MergeSurvivorEntryId = NAME_None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FName> ConsumedEntryIds;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKCardQuality FinalQuality = EGameXXKCardQuality::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 TemporaryCountDelta = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 CapacityDelta = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FName ReplacementEntryId = NAME_None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FName> EligibleReplacementEntryIds;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKRouteMerchantPurchaseFailure Failure = EGameXXKRouteMerchantPurchaseFailure::None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FString FailureReason;
};
