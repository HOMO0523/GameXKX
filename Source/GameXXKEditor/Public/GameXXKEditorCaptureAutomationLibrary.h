#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameXXKEditorCaptureAutomationLibrary.generated.h"

UCLASS()
class GAMEXXKEDITOR_API UGameXXKEditorCaptureAutomationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "GameXXK|Editor Automation")
	static FString PrepareLevelViewportForCapture();
};
