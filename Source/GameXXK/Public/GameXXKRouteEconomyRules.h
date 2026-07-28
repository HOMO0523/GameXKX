#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardRunTypes.h"

/** Pure, atomic rules for route-local travel money and chapter-scoped node receipts. */
class GAMEXXK_API FGameXXKRouteEconomyRules final
{
public:
	static bool InitializeRoute(
		FGameXXKCardRunState& CardRun,
		int32 StartingBalance = 60,
		FString* OutError = nullptr);

	static bool AwardNodeOnce(
		FGameXXKCardRunState& CardRun,
		int32 Chapter,
		int32 NodeId,
		int32 Amount,
		bool& OutAwarded,
		FString* OutError = nullptr);

	static bool CanAfford(const FGameXXKCardRunState& CardRun, int32 Amount);

	static bool Spend(
		FGameXXKCardRunState& CardRun,
		int32 Amount,
		FString* OutError = nullptr);

	static void ClearRouteEconomy(FGameXXKCardRunState& CardRun);
};
