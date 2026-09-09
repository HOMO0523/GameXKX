#pragma once

#include "CoreMinimal.h"

struct FGameXXKRuntimeState;

/** Physical travel-money stacks shared by Backpack and Warehouse. */
class GAMEXXK_API FGameXXKTravelMoneyRules final
{
public:
	static constexpr int32 ChestDropQuantity = 10;
	static constexpr int32 ShopBundleQuantity = 10;
	static constexpr int32 ShopBundleGoldPrice = 100000;
	static constexpr int32 DismantleGoldPerUnit = 2500;
	static FName ItemId();
	static constexpr const TCHAR* IconPath = TEXT("/Game/GameXXK/UI/Items/Currency/T_Item_TravelMoney.T_Item_TravelMoney");
	static int64 GetBalance(const FGameXXKRuntimeState& State);
	static bool CanAfford(const FGameXXKRuntimeState& State, int32 Amount);
	static bool Spend(FGameXXKRuntimeState& State, int32 Amount, FString* OutError = nullptr);
};
