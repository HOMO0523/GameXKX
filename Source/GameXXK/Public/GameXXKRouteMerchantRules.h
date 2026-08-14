#pragma once

#include "CoreMinimal.h"
#include "GameXXKRouteMerchantTypes.h"

struct FGameXXKRuntimeState;

/** Pure candidate-copy authority for route-merchant stock, refresh, preview, and purchase. */
class GAMEXXK_API FGameXXKRouteMerchantRules final
{
public:
	static constexpr int32 CardSlotCount = 0;
	static constexpr int32 RelicSlotCount = 4;
	static constexpr int32 TotalSlotCount = CardSlotCount + RelicSlotCount;

	/** Cost for the next refresh at the supplied persisted refresh count. Zero means invalid. */
	static int32 GetRefreshCost(int32 RefreshCount);

	/** Opens the active pending merchant. First open persists stock; same-node reopen is byte-stable. */
	static bool EnsureStock(FGameXXKRuntimeState& InOutState, FString* OutError = nullptr);

	/** Pure validation for a persisted snapshot, independent of the currently open screen or pending node. */
	static bool ValidateSavedStock(const FGameXXKRuntimeState& State, FString* OutError = nullptr);

	/** Pure projection of the already-persisted active merchant snapshot. */
	static bool GetView(
		const FGameXXKRuntimeState& State,
		FGameXXKRouteMerchantView& OutView,
		FString* OutError = nullptr);

	/** Atomically replaces all six slots and debits route-only money after full generation succeeds. */
	static bool Refresh(FGameXXKRuntimeState& InOutState, FString* OutError = nullptr);

	/** Pure simulation. ReplacementEntryId must be a stable entry id, never a CardId. */
	static bool PreviewPurchase(
		const FGameXXKRuntimeState& State,
		FName OfferId,
		FName ReplacementEntryId,
		FGameXXKRouteMerchantPurchasePreview& OutPreview,
		FString* OutError = nullptr);

	/** Atomic commit. A missing required replacement records only pending metadata and never debits. */
	static bool Purchase(
		FGameXXKRuntimeState& InOutState,
		FName OfferId,
		FName ReplacementEntryId,
		FGameXXKRouteMerchantPurchaseResult& OutResult);

	/** Clears only the pending replacement transaction; currency, offers, deck, and relics are unchanged. */
	static bool CancelPendingPurchase(FGameXXKRuntimeState& InOutState, FString* OutError = nullptr);
};
