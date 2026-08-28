#include "UI/GameXXKGuideOverlayWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

namespace GameXXKGuideOverlayWidgetPrivate
{
	void Place(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D Position, const FVector2D Size, const int32 ZOrder)
	{
		if (UCanvasPanelSlot* Slot = Canvas ? Canvas->AddChildToCanvas(Widget) : nullptr)
		{
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
			Slot->SetZOrder(ZOrder);
		}
	}
}

TSharedRef<SWidget> UGameXXKGuideOverlayWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKGuideOverlayWidget::NativeDestruct()
{
	if (DestroyedDelegate.IsBound())
	{
		DestroyedDelegate.Execute();
	}
	Super::NativeDestruct();
}

void UGameXXKGuideOverlayWidget::PresentGuide(
	const FGameXXKGuideOutput& Output,
	const FSlateRect& TargetRect)
{
	BuildProgrammaticLayout();
	CurrentOutput = Output;
	CurrentTargetRect = TargetRect;
	bGuideVisible = Output.bActive;
	if (!bGuideVisible || !RootCanvas)
	{
		DismissGuide();
		return;
	}

	// Input filtering is semantic and tokenized by the coordinator. The visual
	// overlay itself never steals the allowed target's pointer hit.
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (DimMask)
	{
		DimMask->SetVisibility(Output.InputPolicy == EGameXXKGuideInputPolicy::Forced
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	}
	FVector2D TargetPosition(TargetRect.Left, TargetRect.Top);
	FVector2D TargetMaximum(TargetRect.Right, TargetRect.Bottom);
	const FGeometry& HostGeometry = GetCachedGeometry();
	if (HostGeometry.GetLocalSize().X > 0.0f && HostGeometry.GetLocalSize().Y > 0.0f)
	{
		TargetPosition = HostGeometry.AbsoluteToLocal(TargetPosition);
		TargetMaximum = HostGeometry.AbsoluteToLocal(TargetMaximum);
	}
	const FVector2D TargetSize(
		FMath::Max(1.0f, TargetMaximum.X - TargetPosition.X),
		FMath::Max(1.0f, TargetMaximum.Y - TargetPosition.Y));
	if (UCanvasPanelSlot* CanvasSlot = TargetHighlight ? Cast<UCanvasPanelSlot>(TargetHighlight->Slot) : nullptr)
	{
		CanvasSlot->SetPosition(TargetPosition - FVector2D(6.0f));
		CanvasSlot->SetSize(TargetSize + FVector2D(12.0f));
	}
	if (UCanvasPanelSlot* CanvasSlot = ArrowText ? Cast<UCanvasPanelSlot>(ArrowText->Slot) : nullptr)
	{
		CanvasSlot->SetPosition(FVector2D(TargetPosition.X + TargetSize.X * 0.5f - 20.0f, TargetPosition.Y - 52.0f));
	}
	if (UCanvasPanelSlot* CanvasSlot = GuideTextPanel ? Cast<UCanvasPanelSlot>(GuideTextPanel->Slot) : nullptr)
	{
		const float PanelY = TargetPosition.Y + TargetSize.Y + 14.0f;
		CanvasSlot->SetPosition(FVector2D(FMath::Max(8.0f, TargetPosition.X - 100.0f), PanelY));
	}
	if (GuideText)
	{
		GuideText->SetText(Output.Text);
	}
}

void UGameXXKGuideOverlayWidget::DismissGuide()
{
	bGuideVisible = false;
	CurrentOutput = FGameXXKGuideOutput();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UGameXXKGuideOverlayWidget::SetDestroyedDelegate(FGameXXKGuideOverlayDestroyed InDelegate)
{
	DestroyedDelegate = MoveTemp(InDelegate);
}

bool UGameXXKGuideOverlayWidget::IsGuideVisibleForTest() const
{
	return bGuideVisible;
}

bool UGameXXKGuideOverlayWidget::IsBlockingInputForTest() const
{
	return bGuideVisible && CurrentOutput.InputPolicy == EGameXXKGuideInputPolicy::Forced;
}

FSlateRect UGameXXKGuideOverlayWidget::GetTargetRectForTest() const
{
	return CurrentTargetRect;
}

FText UGameXXKGuideOverlayWidget::GetGuideTextForTest() const
{
	return CurrentOutput.Text;
}

void UGameXXKGuideOverlayWidget::BuildProgrammaticLayout()
{
	using namespace GameXXKGuideOverlayWidgetPrivate;
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GuideOverlayRoot"));
	WidgetTree->RootWidget = RootCanvas;

	DimMask = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GuideDimMask"));
	DimMask->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.56f));
	Place(RootCanvas, DimMask, FVector2D::ZeroVector, FVector2D(1920.0f, 1080.0f), 0);

	TargetHighlight = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GuideTargetHighlight"));
	TargetHighlight->SetBrushColor(FLinearColor(0.18f, 0.72f, 0.92f, 0.26f));
	Place(RootCanvas, TargetHighlight, FVector2D::ZeroVector, FVector2D(1.0f), 1);

	ArrowText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GuideArrow"));
	ArrowText->SetText(FText::FromString(TEXT("↓")));
	ArrowText->SetColorAndOpacity(FSlateColor(FLinearColor(0.16f, 0.62f, 0.78f, 1.0f)));
	FSlateFontInfo ArrowFont = ArrowText->GetFont();
	ArrowFont.Size = 38;
	ArrowText->SetFont(ArrowFont);
	Place(RootCanvas, ArrowText, FVector2D::ZeroVector, FVector2D(40.0f, 48.0f), 2);

	GuideTextPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GuideTextPanel"));
	GuideTextPanel->SetPadding(FMargin(14.0f, 9.0f));
	GuideTextPanel->SetBrushColor(FLinearColor(0.86f, 0.80f, 0.67f, 0.96f));
	GuideText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GuideText"));
	GuideText->SetAutoWrapText(true);
	GuideText->SetColorAndOpacity(FSlateColor(FLinearColor(0.12f, 0.09f, 0.05f, 1.0f)));
	FSlateFontInfo TextFont = GuideText->GetFont();
	TextFont.Size = 18;
	GuideText->SetFont(TextFont);
	GuideTextPanel->SetContent(GuideText);
	Place(RootCanvas, GuideTextPanel, FVector2D::ZeroVector, FVector2D(320.0f, 76.0f), 3);
	DismissGuide();
}
