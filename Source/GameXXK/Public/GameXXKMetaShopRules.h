#pragma once

#include "CoreMinimal.h"
#include "GameXXKMetaShopTypes.h"

struct FGameXXKRuntimeState;

class GAMEXXK_API FGameXXKMetaShopRules final
{
public:
	static constexpr int32 EquipmentPackPrice = 100;
	static constexpr int32 CompanionPackPrice = 500;

	static const TArray<FGameXXKMetaShopProductDefinition>& GetProducts();
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
