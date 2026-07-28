#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"

struct FGameXXKRuntimeState;

class GAMEXXK_API FGameXXKRelicRules final
{
public:
	static bool AcquireRelic(FGameXXKRuntimeState& InOutState, FName RelicId, FString* OutError = nullptr);
	static bool CreateRelicOffer(FGameXXKRuntimeState& InOutState, int32 SourceNodeId, int32 ChoiceSeed, TArray<FName>& OutRelicIds, FString* OutError = nullptr);
	static bool ChoosePendingRelic(FGameXXKRuntimeState& InOutState, FName RelicId, FString* OutError = nullptr);
	static void ClearRouteRelics(FGameXXKRuntimeState& InOutState);

	static void ApplyBattleStart(FGameXXKRuntimeState& InOutState);
	static void ApplyPlayerRoundStart(FGameXXKRuntimeState& InOutState);
	static void ApplyPlayerRoundEnd(FGameXXKRuntimeState& InOutState);
	static void ApplyCardPlayed(FGameXXKRuntimeState& InOutState, FName OwnerUnitId, const TArray<FGameXXKCardDamageResult>& DamageResults);
	static void ApplyDamageTaken(FGameXXKRuntimeState& InOutState, const TArray<FGameXXKCardDamageResult>& DamageResults);

	/** Pure checked sum of route-travel-money relic effects for one completed node. */
	static bool CalculateRouteNodeTravelMoneyBonus(
		const FGameXXKRuntimeState& State,
		int32& OutBonus,
		FString* OutError = nullptr);

	/** Applies only non-currency RouteNodeCompleted effects after a new economy receipt is recorded. */
	static void ApplyRouteNodeCompletedNonCurrency(FGameXXKRuntimeState& InOutState);
};
