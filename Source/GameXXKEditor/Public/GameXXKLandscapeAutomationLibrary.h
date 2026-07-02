#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameXXKLandscapeAutomationLibrary.generated.h"

UCLASS()
class GAMEXXKEDITOR_API UGameXXKLandscapeAutomationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "GameXXK|Editor Automation")
	static FString EnsureQingshanTownLandscape();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "GameXXK|Editor Automation")
	static FString AuditQingshanTownLandscape();
};
