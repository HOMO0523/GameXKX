#include "UI/GameXXKBattleGuideBubbleWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

namespace GameXXKBattleGuideBubblePrivate
{
	const FVector2D BubbleSize(420.0f, 132.0f);
	constexpr float SafeMargin = 16.0f;
	constexpr float AnchorGap = 18.0f;
	const FVector2D PaperSourceSize(100.0f, 101.0f);
	const TCHAR* PaperTexturePath =
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot.T_MasterV2_ItemSlot");

	bool Fits(const FVector2D Position, const FVector2D HostSize)
	{
		return Position.X >= SafeMargin
			&& Position.Y >= SafeMargin
			&& Position.X + BubbleSize.X <= HostSize.X - SafeMargin
			&& Position.Y + BubbleSize.Y <= HostSize.Y - SafeMargin;
	}

	bool FitsVertically(const FVector2D Position, const FVector2D HostSize)
	{
		return Position.Y >= SafeMargin
			&& Position.Y + BubbleSize.Y <= HostSize.Y - SafeMargin;
	}

	FVector2D ClampPosition(FVector2D Position, const FVector2D HostSize)
	{
		const float MaximumX = FMath::Max(
			SafeMargin,
			HostSize.X - BubbleSize.X - SafeMargin);
		const float MaximumY = FMath::Max(
			SafeMargin,
			HostSize.Y - BubbleSize.Y - SafeMargin);
		Position.X = FMath::Clamp(Position.X, SafeMargin, MaximumX);
		Position.Y = FMath::Clamp(Position.Y, SafeMargin, MaximumY);
		return Position;
	}
}

TSharedRef<SWidget> UGameXXKBattleGuideBubbleWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKBattleGuideBubbleWidget::PresentBubble(
	const FText& Text,
	const bool bShowContinueHint,
	const FSlateRect& AnchorRect,
	const FVector2D HostSize,
	const bool bPreferAbove)
{
	BuildProgrammaticLayout();
	if (!PaperFrame || Text.IsEmpty())
	{
		DismissBubble();
		return;
	}
	FinalLocalRect = ResolveBubbleRect(AnchorRect, HostSize, bPreferAbove);
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PaperFrame->Slot))
	{
		CanvasSlot->SetPosition(FVector2D(FinalLocalRect.Left, FinalLocalRect.Top));
		CanvasSlot->SetSize(FVector2D(
			FinalLocalRect.Right - FinalLocalRect.Left,
			FinalLocalRect.Bottom - FinalLocalRect.Top));
	}
	if (BodyText)
	{
		BodyText->SetText(Text);
	}
	if (ContinueHintText)
	{
		ContinueHintText->SetVisibility(
			bShowContinueHint
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
	bBubbleVisible = true;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UGameXXKBattleGuideBubbleWidget::DismissBubble()
{
	bBubbleVisible = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

FSlateRect UGameXXKBattleGuideBubbleWidget::ResolveBubbleRect(
	const FSlateRect& AnchorRect,
	const FVector2D HostSize,
	const bool bPreferAbove)
{
	using namespace GameXXKBattleGuideBubblePrivate;
	const float AnchorCenterX = (AnchorRect.Left + AnchorRect.Right) * 0.5f;
	const float AnchorCenterY = (AnchorRect.Top + AnchorRect.Bottom) * 0.5f;
	const FVector2D Right(
		AnchorRect.Right + AnchorGap,
		AnchorCenterY - BubbleSize.Y * 0.5f);
	const FVector2D Left(
		AnchorRect.Left - AnchorGap - BubbleSize.X,
		AnchorCenterY - BubbleSize.Y * 0.5f);
	const FVector2D Above(
		AnchorCenterX - BubbleSize.X * 0.5f,
		AnchorRect.Top - AnchorGap - BubbleSize.Y);
	const FVector2D Below(
		AnchorCenterX - BubbleSize.X * 0.5f,
		AnchorRect.Bottom + AnchorGap);
	const FVector2D Preferred = bPreferAbove ? Above : Below;
	FVector2D Position = Preferred;
	if (!FitsVertically(Preferred, HostSize))
	{
		const TArray<FVector2D> Fallbacks = bPreferAbove
			? TArray<FVector2D>{Right, Left, Below}
			: TArray<FVector2D>{Above, Right, Left};
		for (const FVector2D Candidate : Fallbacks)
		{
			if (Fits(Candidate, HostSize))
			{
				Position = Candidate;
				break;
			}
		}
	}
	Position = ClampPosition(Position, HostSize);
	return FSlateRect(
		Position.X,
		Position.Y,
		Position.X + BubbleSize.X,
		Position.Y + BubbleSize.Y);
}

bool UGameXXKBattleGuideBubbleWidget::IsContinueHintVisible() const
{
	return ContinueHintText
		&& ContinueHintText->GetVisibility() != ESlateVisibility::Collapsed
		&& ContinueHintText->GetVisibility() != ESlateVisibility::Hidden;
}

FString UGameXXKBattleGuideBubbleWidget::GetPaperTexturePath() const
{
	return GameXXKBattleGuideBubblePrivate::PaperTexturePath;
}

void UGameXXKBattleGuideBubbleWidget::BuildProgrammaticLayout()
{
	using namespace GameXXKBattleGuideBubblePrivate;
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("BattleGuideBubbleRoot"));
	WidgetTree->RootWidget = RootCanvas;
	PaperFrame = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("BattleGuideBubblePaper"));
	PaperFrame->SetPadding(FMargin(34.0f, 24.0f, 34.0f, 18.0f));
	if (UTexture2D* PaperTexture = LoadObject<UTexture2D>(nullptr, PaperTexturePath))
	{
		FSlateBrush PaperBrush;
		PaperBrush.SetResourceObject(PaperTexture);
		PaperBrush.DrawAs = ESlateBrushDrawType::Box;
		PaperBrush.Margin = FMargin(0.065f);
		PaperBrush.ImageSize = PaperSourceSize;
		PaperFrame->SetBrush(PaperBrush);
	}
	PaperFrame->SetBrushColor(FLinearColor::White);
	PaperFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(PaperFrame))
	{
		CanvasSlot->SetPosition(FVector2D::ZeroVector);
		CanvasSlot->SetSize(BubbleSize);
	}
	UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("BattleGuideBubbleBody"));
	PaperFrame->SetContent(Body);
	BodyText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("BattleGuideBubbleText"));
	BodyText->SetAutoWrapText(true);
	BodyText->SetWrapTextAt(352.0f);
	BodyText->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.13f, 0.085f, 0.04f, 1.0f)));
	FSlateFontInfo BodyFont = BodyText->GetFont();
	BodyFont.Size = 19;
	BodyText->SetFont(BodyFont);
	if (UVerticalBoxSlot* BodySlot = Body->AddChildToVerticalBox(BodyText))
	{
		BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	ContinueHintText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("BattleGuideBubbleContinueHint"));
	ContinueHintText->SetText(FText::FromString(TEXT("空格继续")));
	ContinueHintText->SetJustification(ETextJustify::Right);
	ContinueHintText->SetColorAndOpacity(
		FSlateColor(FLinearColor(0.28f, 0.19f, 0.09f, 0.86f)));
	FSlateFontInfo HintFont = ContinueHintText->GetFont();
	HintFont.Size = 14;
	ContinueHintText->SetFont(HintFont);
	if (UVerticalBoxSlot* HintSlot = Body->AddChildToVerticalBox(ContinueHintText))
	{
		HintSlot->SetHorizontalAlignment(HAlign_Fill);
		HintSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
	}
	DismissBubble();
}
