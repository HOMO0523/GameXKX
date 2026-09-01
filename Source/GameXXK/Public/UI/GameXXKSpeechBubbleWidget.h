#pragma once

#include "CoreMinimal.h"
#include "Dialogue/GameXXKDialogueTypes.h"
#include "Blueprint/UserWidget.h"

#include "GameXXKSpeechBubbleWidget.generated.h"

class APlayerController;
class UBorder;
class UCanvasPanel;
class UPrimitiveComponent;
class USceneComponent;
class UTextBlock;

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKSpeechBubbleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	bool PresentBubble(const FGameXXKDialoguePresentationView& View, USceneComponent* Anchor);
	bool PresentBubbleAtVisualTop(
		const FGameXXKDialoguePresentationView& View,
		UPrimitiveComponent* VisualAnchor);
	bool UpdateAnchor(APlayerController* Controller);
	void ClearBubble();

	static FVector2D ClampToViewportForTest(
		FVector2D ProjectedPosition,
		FVector2D ViewportSize,
		FVector2D BubbleSize);
	static FVector VisualBoundsTopForTest(FVector BoundsOrigin, FVector BoundsExtent);
	bool IsBubbleVisibleForTest() const;
	bool UsesVisualBoundsTopForTest() const { return bUseVisualBoundsTop; }
	bool IsBubbleHitTestInvisibleForTest() const;
	int32 GetBubbleCountForTest() const;
	FText GetBodyTextForTest() const;
	int32 GetMaximumLineCountForTest() const;

private:
	bool PresentBubbleInternal(
		const FGameXXKDialoguePresentationView& View,
		USceneComponent* Anchor,
		bool bInUseVisualBoundsTop);
	void BuildProgrammaticLayout();
	static FVector2D ClampToViewport(
		FVector2D ProjectedPosition,
		FVector2D ViewportSize,
		FVector2D BubbleSize);

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> BubbleFrame;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BodyText;

	TWeakObjectPtr<USceneComponent> AnchorComponent;
	FGameXXKDialoguePresentationView CurrentView;
	FString LastPresentationError;
	bool bBubbleVisible = false;
	bool bUseVisualBoundsTop = false;
};
