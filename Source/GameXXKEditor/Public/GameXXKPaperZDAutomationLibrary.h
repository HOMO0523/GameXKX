#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameXXKPaperZDAutomationLibrary.generated.h"

UCLASS()
class GAMEXXKEDITOR_API UGameXXKPaperZDAutomationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "GameXXK|Editor Automation")
	static FString EnsureHeroPaperZDAssets();
};
