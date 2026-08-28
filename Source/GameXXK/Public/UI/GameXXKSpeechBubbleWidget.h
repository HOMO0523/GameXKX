#pragma once

#include "CoreMinimal.h"
#include "Dialogue/GameXXKDialogueTypes.h"
#include "Blueprint/UserWidget.h"

#include "GameXXKSpeechBubbleWidget.generated.h"

class APlayerController;
class UBorder;
class UCanvasPanel;
class USceneComponent;
class UTextBlock;

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKSpeechBubbleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	bool PresentBubble(const FGameXXKDialoguePresentationView& View, USceneComponent* Anchor);
	bool UpdateAnchor(APlayerController* Controller);
	void ClearBubble();

	static FVector2D ClampToViewportForTest(
		FVector2D ProjectedPosition,
		FVector2D ViewportSize,
		FVector2D BubbleSize);
	bool IsBubbleVisibleForTest() const;
	bool IsBubbleHitTestInvisibleForTest() const;
	int32 GetBubbleCountForTest() const;
	FText GetBodyTextForTest() const;
	int32 GetMaximumLineCountForTest() const;

private:
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
};
