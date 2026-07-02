#pragma once

#include "CoreMinimal.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKTradeWidget.generated.h"

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKTradeWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Trade")
	bool BuyItemById(FName ItemId, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Trade")
	bool SellItemById(FName ItemId, int32 Quantity);

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|Trade")
	void OnTradeChanged();
};
