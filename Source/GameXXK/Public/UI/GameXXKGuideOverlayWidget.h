#pragma once

#include "CoreMinimal.h"
#include "Guide/GameXXKGuideRules.h"
#include "Blueprint/UserWidget.h"

#include "GameXXKGuideOverlayWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UTextBlock;

DECLARE_DELEGATE(FGameXXKGuideOverlayDestroyed);

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKGuideOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

	void PresentGuide(const FGameXXKGuideOutput& Output, const FSlateRect& TargetRect);
	void DismissGuide();
	void SetDestroyedDelegate(FGameXXKGuideOverlayDestroyed InDelegate);

	bool IsGuideVisibleForTest() const;
	bool IsBlockingInputForTest() const;
	FSlateRect GetTargetRectForTest() const;
	FText GetGuideTextForTest() const;

private:
	void BuildProgrammaticLayout();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> DimMask;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> TargetHighlight;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ArrowText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> GuideTextPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GuideText;

	FGameXXKGuideOutput CurrentOutput;
	FSlateRect CurrentTargetRect;
	FGameXXKGuideOverlayDestroyed DestroyedDelegate;
	bool bGuideVisible = false;
};
