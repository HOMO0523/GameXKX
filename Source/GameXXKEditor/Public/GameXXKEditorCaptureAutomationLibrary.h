#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameXXKEditorCaptureAutomationLibrary.generated.h"

class UUserWidget;

UCLASS()
class GAMEXXKEDITOR_API UGameXXKEditorCaptureAutomationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "GameXXK|Editor Automation")
	static FString PrepareLevelViewportForCapture();

	/**
	 * Renders the desktop HUD off screen as a normal frame, root-background-only frame,
	 * and foreground-only RGBA frame. This creates no native or Slate window.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "GameXXK|Editor Automation")
	static FString CaptureDesktopHudLayerAudit(const FString& OutputDirectory);

    /** Native-resolution render of an existing live game widget, never a generated fixture. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "GameXXK|Editor Automation")
    static bool CaptureLiveGameWidget(UUserWidget* Widget, const FString& Filename, int32 Width=1920, int32 Height=1080);
};
