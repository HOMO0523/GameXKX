#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Layout/SlateRect.h"

#include "GameXXKBattleGuideBubbleWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UTextBlock;

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKBattleGuideBubbleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void PresentBubble(
		const FText& Text,
		bool bShowContinueHint,
		const FSlateRect& AnchorRect,
		FVector2D HostSize,
		bool bPreferAbove);
	void DismissBubble();

	static FSlateRect ResolveBubbleRect(
		const FSlateRect& AnchorRect,
		FVector2D HostSize,
		bool bPreferAbove);

	bool IsBubbleVisible() const { return bBubbleVisible; }
	bool IsContinueHintVisible() const;
	FSlateRect GetFinalLocalRect() const { return FinalLocalRect; }
	FString GetPaperTexturePath() const;

private:
	void BuildProgrammaticLayout();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PaperFrame;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BodyText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ContinueHintText;

	FSlateRect FinalLocalRect;
	bool bBubbleVisible = false;
};
