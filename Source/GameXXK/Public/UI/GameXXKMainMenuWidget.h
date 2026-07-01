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

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|MainMenu")
	void OnStartGameSucceeded();
};
