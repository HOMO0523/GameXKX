#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"

struct FGameXXKRuntimeState;

class GAMEXXK_API FGameXXKRelicRules final
{
public:
	/** Stable catalog ID for the unique Camp reward. */
	static FName LifeSavingTalismanId();
	static bool OwnsLifeSavingTalisman(const FGameXXKRuntimeState& State);

	static bool AcquireRelic(FGameXXKRuntimeState& InOutState, FName RelicId, FString* OutError = nullptr);
	static bool CreateRelicOffer(FGameXXKRuntimeState& InOutState, int32 SourceNodeId, int32 ChoiceSeed, TArray<FName>& OutRelicIds, FString* OutError = nullptr);
	static bool ChoosePendingRelic(FGameXXKRuntimeState& InOutState, FName RelicId, FString* OutError = nullptr);
	static void ClearRouteRelics(FGameXXKRuntimeState& InOutState);

	static bool ApplyBattleStart(FGameXXKRuntimeState& InOutState, FGameXXKCardPlayResult* Output = nullptr, FString* OutError = nullptr);
	static bool ApplyPlayerRoundStart(FGameXXKRuntimeState& InOutState, FGameXXKCardPlayResult* Output = nullptr, FString* OutError = nullptr);
	static bool ApplyPlayerRoundEnd(FGameXXKRuntimeState& InOutState, FGameXXKCardPlayResult* Output = nullptr, FString* OutError = nullptr);
	static bool ApplyCardPlayed(
		FGameXXKRuntimeState& InOutState,
		FName OwnerUnitId,
		const TArray<FGameXXKCardDamageResult>& PrimaryDamageResults,
		FGameXXKCardPlayResult& InOutCardPlayResult,
		FString* OutError = nullptr);
	static bool ApplyDamageTaken(FGameXXKRuntimeState& InOutState, const TArray<FGameXXKCardDamageResult>& DamageResults,
		FGameXXKCardPlayResult* Output = nullptr, FString* OutError = nullptr);

	/** Pure checked sum of route-travel-money relic effects for one completed node. */
	static bool CalculateRouteNodeTravelMoneyBonus(
		const FGameXXKRuntimeState& State,
		int32& OutBonus,
		FString* OutError = nullptr);

	/** Applies only non-currency RouteNodeCompleted effects after a new economy receipt is recorded. */
	static void ApplyRouteNodeCompletedNonCurrency(FGameXXKRuntimeState& InOutState);
};
