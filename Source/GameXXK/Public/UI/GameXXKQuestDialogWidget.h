#pragma once

#include "CoreMinimal.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKQuestDialogWidget.generated.h"

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKQuestDialogWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Quest")
	bool AcceptQuest();

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|Quest")
	void OnQuestAccepted();
};
