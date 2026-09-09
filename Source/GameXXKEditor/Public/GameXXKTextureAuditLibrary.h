#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameXXKTextureAuditLibrary.generated.h"
class UTexture2D;

UCLASS()
class GAMEXXKEDITOR_API UGameXXKTextureAuditLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Editor-only verification of decoded source pixels and the completed platform texture. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="GameXXK|Editor Automation")
	static FString InspectTexture(UTexture2D* Texture, bool bFinishCompilation = true);
};
