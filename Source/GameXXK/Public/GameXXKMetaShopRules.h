#pragma once

#include "CoreMinimal.h"
#include "GameXXKMetaShopTypes.h"

class GAMEXXK_API FGameXXKMetaShopRules final
{
public:
	static constexpr int32 EquipmentPackPrice = 100;
	static constexpr int32 CompanionPackPrice = 500;

	static const TArray<FGameXXKMetaShopProductDefinition>& GetProducts();
	static const FGameXXKMetaShopProductDefinition* FindProduct(EGameXXKMetaShopProductId ProductId);
};
