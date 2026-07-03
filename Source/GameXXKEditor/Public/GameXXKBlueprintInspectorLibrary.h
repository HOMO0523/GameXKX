#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameXXKBlueprintInspectorLibrary.generated.h"

UCLASS()
class GAMEXXKEDITOR_API UGameXXKBlueprintInspectorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "GameXXK|Editor Automation")
	static FString InspectAssetToJson(const FString& AssetPath);
};
