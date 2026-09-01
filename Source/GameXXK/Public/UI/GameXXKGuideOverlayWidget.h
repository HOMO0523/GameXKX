#pragma once

#include "CoreMinimal.h"
#include "Guide/GameXXKGuideRules.h"
#include "Blueprint/UserWidget.h"

#include "GameXXKGuideOverlayWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UTextBlock;
class UGameXXKBattleGuideBubbleWidget;

DECLARE_DELEGATE(FGameXXKGuideOverlayDestroyed);

/** Paint-only sibling kept below the paper bubble in the overlay canvas. */
UCLASS()
class GAMEXXK_API UGameXXKGuideSpotlightWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	void PresentSpotlight(
		const FGameXXKGuideOutput& Output,
		const TArray<FSlateRect>& LocalTargetRects);
	void DismissSpotlight();

private:
	void BuildProgrammaticLayout();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	FGameXXKGuideOutput CurrentOutput;
	TArray<FSlateRect> CurrentTargetRects;
	bool bSpotlightVisible = false;
};

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKGuideOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

	void PresentGuide(const FGameXXKGuideOutput& Output, const FSlateRect& TargetRect);
	void PresentGuide(
		const FGameXXKGuideOutput& Output,
		const TArray<FSlateRect>& LocalTargetRects,
		const TOptional<FSlateRect>& LocalBubbleAnchorRect);
	void DismissGuide();
	void SetDestroyedDelegate(FGameXXKGuideOverlayDestroyed InDelegate);
	static TArray<FSlateRect> BuildDimRegions(
		FVector2D HostSize,
		const TArray<FSlateRect>& Cutouts,
		float Padding = 6.0f);

	bool IsGuideVisibleForTest() const;
	bool IsBlockingInputForTest() const;
	FSlateRect GetTargetRectForTest() const;
	const TArray<FSlateRect>& GetTargetRectsForTest() const { return CurrentTargetRects; }
	FText GetGuideTextForTest() const;
	UGameXXKBattleGuideBubbleWidget* GetBattleBubbleForTest() const
	{
		return GuideBubble;
	}

private:
	void BuildProgrammaticLayout();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKGuideSpotlightWidget> GuideSpotlight;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKBattleGuideBubbleWidget> GuideBubble;

	FGameXXKGuideOutput CurrentOutput;
	TArray<FSlateRect> CurrentTargetRects;
	TOptional<FSlateRect> CurrentBubbleAnchorRect;
	FGameXXKGuideOverlayDestroyed DestroyedDelegate;
	bool bGuideVisible = false;
};
