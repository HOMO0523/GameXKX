#include "UI/GameXXKGuideOverlayWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"
#include "UI/GameXXKBattleGuideBubbleWidget.h"

namespace GameXXKGuideOverlayWidgetPrivate
{
	constexpr float DimAlpha = 0.56f;
	constexpr float OutlineThickness = 3.0f;
	const FVector2D DefaultHostSize(1920.0f, 1080.0f);

	void Place(
		UCanvasPanel* Canvas,
		UWidget* Widget,
		const FVector2D Position,
		const FVector2D Size,
		const int32 ZOrder)
	{
		if (UCanvasPanelSlot* Slot = Canvas ? Canvas->AddChildToCanvas(Widget) : nullptr)
		{
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
			Slot->SetZOrder(ZOrder);
		}
	}

	TArray<FSlateRect> ClampCutouts(
		const FVector2D HostSize,
		const TArray<FSlateRect>& Cutouts,
		const float Padding)
	{
		TArray<FSlateRect> Result;
		if (HostSize.X <= 0.0f || HostSize.Y <= 0.0f)
		{
			return Result;
		}
		for (const FSlateRect& Cutout : Cutouts)
		{
			const FSlateRect Candidate(
				FMath::Clamp(Cutout.Left - Padding, 0.0f, HostSize.X),
				FMath::Clamp(Cutout.Top - Padding, 0.0f, HostSize.Y),
				FMath::Clamp(Cutout.Right + Padding, 0.0f, HostSize.X),
				FMath::Clamp(Cutout.Bottom + Padding, 0.0f, HostSize.Y));
			if (Candidate.Right > Candidate.Left
				&& Candidate.Bottom > Candidate.Top)
			{
				Result.Add(Candidate);
			}
		}
		return Result;
	}

	void PaintRect(
		FSlateWindowElementList& OutDrawElements,
		const FGeometry& Geometry,
		const int32 Layer,
		const FSlateRect& Rect,
		const FSlateBrush& Brush)
	{
		const FVector2D Size(Rect.Right - Rect.Left, Rect.Bottom - Rect.Top);
		if (Size.X <= 0.0f || Size.Y <= 0.0f)
		{
			return;
		}
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			Layer,
			Geometry.ToPaintGeometry(
				Size,
				FSlateLayoutTransform(FVector2D(Rect.Left, Rect.Top))),
			&Brush,
			ESlateDrawEffect::None,
			FLinearColor::White);
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

int32 UGameXXKGuideOverlayWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const bool bParentEnabled) const
{
	using namespace GameXXKGuideOverlayWidgetPrivate;
	int32 ChildLayer = LayerId;
	if (bGuideVisible && !CurrentTargetRects.IsEmpty())
	{
		const FVector2D HostSize = AllottedGeometry.GetLocalSize();
		const TArray<FSlateRect> PaddedCutouts =
			ClampCutouts(HostSize, CurrentTargetRects, 6.0f);
		if (CurrentOutput.InputPolicy == EGameXXKGuideInputPolicy::Forced)
		{
			FSlateBrush DimBrush;
			DimBrush.DrawAs = ESlateBrushDrawType::Box;
			DimBrush.TintColor = FSlateColor(
				FLinearColor(0.0f, 0.0f, 0.0f, DimAlpha));
			for (const FSlateRect& Region : BuildDimRegions(HostSize, CurrentTargetRects, 6.0f))
			{
				PaintRect(OutDrawElements, AllottedGeometry, ChildLayer, Region, DimBrush);
			}
			++ChildLayer;
		}

		FSlateBrush OutlineBrush;
		OutlineBrush.DrawAs = ESlateBrushDrawType::Box;
		OutlineBrush.TintColor = FSlateColor(
			FLinearColor(0.16f, 0.62f, 0.78f, 0.96f));
		for (const FSlateRect& Cutout : PaddedCutouts)
		{
			PaintRect(OutDrawElements, AllottedGeometry, ChildLayer,
				FSlateRect(Cutout.Left, Cutout.Top, Cutout.Right, Cutout.Top + OutlineThickness), OutlineBrush);
			PaintRect(OutDrawElements, AllottedGeometry, ChildLayer,
				FSlateRect(Cutout.Left, Cutout.Bottom - OutlineThickness, Cutout.Right, Cutout.Bottom), OutlineBrush);
			PaintRect(OutDrawElements, AllottedGeometry, ChildLayer,
				FSlateRect(Cutout.Left, Cutout.Top, Cutout.Left + OutlineThickness, Cutout.Bottom), OutlineBrush);
			PaintRect(OutDrawElements, AllottedGeometry, ChildLayer,
				FSlateRect(Cutout.Right - OutlineThickness, Cutout.Top, Cutout.Right, Cutout.Bottom), OutlineBrush);
		}
		++ChildLayer;
	}
	return Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		ChildLayer,
		InWidgetStyle,
		bParentEnabled);
}

void UGameXXKGuideOverlayWidget::PresentGuide(
	const FGameXXKGuideOutput& Output,
	const FSlateRect& TargetRect)
{
	PresentGuide(Output, TArray<FSlateRect>{TargetRect}, TOptional<FSlateRect>());
}

void UGameXXKGuideOverlayWidget::PresentGuide(
	const FGameXXKGuideOutput& Output,
	const TArray<FSlateRect>& LocalTargetRects,
	const TOptional<FSlateRect>& LocalBubbleAnchorRect)
{
	BuildProgrammaticLayout();
	CurrentOutput = Output;
	CurrentTargetRects = LocalTargetRects;
	CurrentBubbleAnchorRect = LocalBubbleAnchorRect;
	bGuideVisible = Output.bActive && !CurrentTargetRects.IsEmpty();
	if (!bGuideVisible || !RootCanvas || !GuideBubble)
	{
		DismissGuide();
		return;
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
	FVector2D HostSize = GetCachedGeometry().GetLocalSize();
	if (HostSize.X <= 0.0f || HostSize.Y <= 0.0f)
	{
		HostSize = GameXXKGuideOverlayWidgetPrivate::DefaultHostSize;
	}
	if (UCanvasPanelSlot* BubbleSlot = Cast<UCanvasPanelSlot>(GuideBubble->Slot))
	{
		BubbleSlot->SetPosition(FVector2D::ZeroVector);
		BubbleSlot->SetSize(HostSize);
	}
	const bool bHasExplicitBubbleAnchor = CurrentBubbleAnchorRect.IsSet();
	const FSlateRect BubbleAnchor = bHasExplicitBubbleAnchor
		? CurrentBubbleAnchorRect.GetValue()
		: CurrentTargetRects[0];
	const bool bShowContinueHint =
		Output.AllowedActionIds.Contains(TEXT("Action.Guide.Continue"));
	GuideBubble->PresentBubble(
		Output.Text,
		bShowContinueHint,
		BubbleAnchor,
		HostSize,
		!bHasExplicitBubbleAnchor);
}

void UGameXXKGuideOverlayWidget::DismissGuide()
{
	bGuideVisible = false;
	CurrentOutput = FGameXXKGuideOutput();
	CurrentTargetRects.Reset();
	CurrentBubbleAnchorRect.Reset();
	if (GuideBubble)
	{
		GuideBubble->DismissBubble();
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void UGameXXKGuideOverlayWidget::SetDestroyedDelegate(
	FGameXXKGuideOverlayDestroyed InDelegate)
{
	DestroyedDelegate = MoveTemp(InDelegate);
}

TArray<FSlateRect> UGameXXKGuideOverlayWidget::BuildDimRegions(
	const FVector2D HostSize,
	const TArray<FSlateRect>& Cutouts,
	const float Padding)
{
	using namespace GameXXKGuideOverlayWidgetPrivate;
	TArray<FSlateRect> Regions;
	const TArray<FSlateRect> ClampedCutouts =
		ClampCutouts(HostSize, Cutouts, FMath::Max(0.0f, Padding));
	if (HostSize.X <= 0.0f || HostSize.Y <= 0.0f)
	{
		return Regions;
	}
	if (ClampedCutouts.IsEmpty())
	{
		Regions.Add(FSlateRect(0.0f, 0.0f, HostSize.X, HostSize.Y));
		return Regions;
	}

	TArray<float> YBoundaries = {
		0.0f,
		static_cast<float>(HostSize.Y)};
	for (const FSlateRect& Cutout : ClampedCutouts)
	{
		YBoundaries.Add(Cutout.Top);
		YBoundaries.Add(Cutout.Bottom);
	}
	YBoundaries.Sort();
	for (int32 Index = YBoundaries.Num() - 1; Index > 0; --Index)
	{
		if (FMath::IsNearlyEqual(YBoundaries[Index], YBoundaries[Index - 1]))
		{
			YBoundaries.RemoveAt(Index);
		}
	}

	for (int32 BandIndex = 0; BandIndex + 1 < YBoundaries.Num(); ++BandIndex)
	{
		const float Top = YBoundaries[BandIndex];
		const float Bottom = YBoundaries[BandIndex + 1];
		if (Bottom <= Top)
		{
			continue;
		}
		TArray<FVector2D> Intervals;
		for (const FSlateRect& Cutout : ClampedCutouts)
		{
			if (Cutout.Top < Bottom && Cutout.Bottom > Top)
			{
				Intervals.Add(FVector2D(Cutout.Left, Cutout.Right));
			}
		}
		Intervals.Sort([](const FVector2D& A, const FVector2D& B)
		{
			return A.X < B.X
				|| (FMath::IsNearlyEqual(A.X, B.X) && A.Y < B.Y);
		});
		float Cursor = 0.0f;
		for (const FVector2D Interval : Intervals)
		{
			if (Interval.X > Cursor)
			{
				Regions.Add(FSlateRect(Cursor, Top, Interval.X, Bottom));
			}
			Cursor = FMath::Max(Cursor, Interval.Y);
		}
		if (Cursor < HostSize.X)
		{
			Regions.Add(FSlateRect(Cursor, Top, HostSize.X, Bottom));
		}
	}
	return Regions;
}

bool UGameXXKGuideOverlayWidget::IsGuideVisibleForTest() const
{
	return bGuideVisible;
}

bool UGameXXKGuideOverlayWidget::IsBlockingInputForTest() const
{
	return bGuideVisible
		&& CurrentOutput.InputPolicy == EGameXXKGuideInputPolicy::Forced;
}

FSlateRect UGameXXKGuideOverlayWidget::GetTargetRectForTest() const
{
	return CurrentTargetRects.IsEmpty()
		? FSlateRect()
		: CurrentTargetRects[0];
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
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("GuideOverlayRoot"));
	RootCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = RootCanvas;
	GuideBubble = WidgetTree->ConstructWidget<UGameXXKBattleGuideBubbleWidget>(
		UGameXXKBattleGuideBubbleWidget::StaticClass(),
		TEXT("GuideBattleBubble"));
	Place(RootCanvas, GuideBubble, FVector2D::ZeroVector, DefaultHostSize, 3);
	DismissGuide();
}
