#pragma once

#include "CoreMinimal.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKInventoryWidget.generated.h"

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKInventoryWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "GameXXK|Inventory")
	int32 GetItemCount(FName ItemId) const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Inventory")
	bool UseHealingItem();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Inventory")
	bool EquipItemById(FName ItemId);

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|Inventory")
	void OnInventoryChanged();
};
