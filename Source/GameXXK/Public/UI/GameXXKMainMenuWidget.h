#pragma once

#include "CoreMinimal.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKMainMenuWidget.generated.h"

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKMainMenuWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool StartGame();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|MainMenu")
	bool StartGameFromSlot(FString SlotName, int32 UserIndex = 0);

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|MainMenu")
	void OnStartGameSucceeded();
};
