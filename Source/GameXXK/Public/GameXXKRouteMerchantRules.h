#pragma once

#include "CoreMinimal.h"
#include "GameXXKRouteMerchantTypes.h"

struct FGameXXKRuntimeState;

/** Pure candidate-copy authority for route-merchant stock, refresh, preview, and purchase. */
class GAMEXXK_API FGameXXKRouteMerchantRules final
{
public:
	struct FDeployedCardCandidate
	{
		FName OwnerMemberId = NAME_None;
		FName CardId = NAME_None;
		EGameXXKCardQuality CurrentQuality = EGameXXKCardQuality::Invalid;
	};

	static constexpr int32 CardSlotCount = 4;
	static constexpr int32 RelicSlotCount = 4;
	static constexpr int32 TotalSlotCount = CardSlotCount + RelicSlotCount;

	/** Cost for the next refresh at the supplied persisted refresh count. Zero means invalid. */
	static int32 GetRefreshCost(int32 RefreshCount);

	static bool BuildEffectiveDeployedCardPool(
		const FGameXXKRuntimeState& State,
		TArray<FDeployedCardCandidate>& OutCandidates,
		FString* OutError = nullptr);

	/** Opens the active pending merchant. First open persists stock; same-node reopen is byte-stable. */
	static bool EnsureStock(FGameXXKRuntimeState& InOutState, FString* OutError = nullptr);

	/** Pure validation for a persisted snapshot, independent of the currently open screen or pending node. */
	static bool ValidateSavedStock(const FGameXXKRuntimeState& State, FString* OutError = nullptr);

	/** Pure projection of the already-persisted active merchant snapshot. */
	static bool GetView(
		FGameXXKRuntimeState& InOutState,
		FGameXXKRouteMerchantView& OutView,
		FString* OutError = nullptr);

	/** Const projection for callers that already normalized stock on entry. */
	static bool GetView(
		const FGameXXKRuntimeState& State,
		FGameXXKRouteMerchantView& OutView,
		FString* OutError = nullptr);

	/** Rerolls only unsold slots and debits ordinary gold after full generation succeeds. */
	static bool Refresh(FGameXXKRuntimeState& InOutState, FString* OutError = nullptr);

	/** Pure simulation. ReplacementEntryId is retained for API compatibility and must be None. */
	static bool PreviewPurchase(
		const FGameXXKRuntimeState& State,
		FName OfferId,
		FName ReplacementEntryId,
		FGameXXKRouteMerchantPurchasePreview& OutPreview,
		FString* OutError = nullptr);

	/** Atomic carried-card quality upgrade; never creates a replacement transaction. */
	static bool Purchase(
		FGameXXKRuntimeState& InOutState,
		FName OfferId,
		FName ReplacementEntryId,
		FGameXXKRouteMerchantPurchaseResult& OutResult);

	/** Clears legacy pending-replacement metadata without changing currency, offers, cards, or relics. */
	static bool CancelPendingPurchase(FGameXXKRuntimeState& InOutState, FString* OutError = nullptr);
};
