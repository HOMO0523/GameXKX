#pragma once

#include "CoreMinimal.h"
#include "GameXXKMetaShopTypes.h"

struct FGameXXKRuntimeState;

class GAMEXXK_API FGameXXKMetaShopRules final
{
public:
	static constexpr int32 EquipmentPackPrice = 100000;
	static constexpr int32 CompanionPackPrice = 500;
	static constexpr int32 MaxPurchaseQuantity = 10;
	static int32 EquipmentItemLevel(const FGameXXKRuntimeState& State);
	static bool PurchaseBatch(FGameXXKRuntimeState& State,EGameXXKMetaShopProductId ProductId,int32 Quantity,
		TArray<FGameXXKMetaShopPurchaseResult>& Results,FText& Message);

	static const TArray<FGameXXKMetaShopProductDefinition>& GetProducts();
	static const TArray<FGameXXKMetaShopProductDefinition>& GetDesktopProducts();
	static const FGameXXKMetaShopProductDefinition* FindProduct(EGameXXKMetaShopProductId ProductId);
	static EGameXXKEquipmentQuality QualityFromRoll(int32 RollOneToHundred);
	static bool PreviewPurchase(
		const FGameXXKRuntimeState& State,
		EGameXXKMetaShopProductId ProductId,
		FGameXXKMetaShopPurchasePreview& OutPreview);
	static bool Purchase(
		FGameXXKRuntimeState& InOutState,
		EGameXXKMetaShopProductId ProductId,
		FGameXXKMetaShopPurchaseResult& OutResult);
	static int32 DeriveSeed(const FGameXXKRuntimeState& State);
	static bool ValidateState(const FGameXXKRuntimeState& State, FString* OutError = nullptr);
};
