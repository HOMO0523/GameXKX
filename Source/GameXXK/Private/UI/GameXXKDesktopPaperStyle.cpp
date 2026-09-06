#include "UI/GameXXKDesktopPaperStyle.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Engine/Texture2D.h"

namespace GameXXKDesktopPaperStyle
{
	FSlateBrush MakeBrush(const FVector2D& Size)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(LoadObject<UTexture2D>(nullptr, TexturePath));
		Brush.ImageSize = Size;
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = FMargin(SliceMargin);
		Brush.TintColor = FSlateColor(FLinearColor::White);
		return Brush;
	}

	float GetBackpackScale(const FVector2D& HostSize)
	{
		return FMath::Min(HostSize.X / BackpackReferenceSize.X, HostSize.Y / BackpackReferenceSize.Y);
	}

	FMargin GetBackpackOutsets(const FVector2D& HostSize)
	{
		const float Scale = GetBackpackScale(HostSize);
		const FVector2D Origin = (HostSize - BackpackReferenceSize * Scale) * 0.5f
			+ (BackpackWidgetOffset + BackpackPaperPosition) * Scale;
		const FVector2D End = Origin + BackpackPaperSize * Scale;
		return FMargin(-Origin.X, -Origin.Y, End.X - HostSize.X, End.Y - HostSize.Y);
	}

	UBorder* MakePanel(UWidgetTree* Tree, const FName Name, const FVector2D& BackpackHostSize,
		const FLinearColor& FallbackColor, const bool bUseBackpackEdges)
	{
		UBorder* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Panel->SetPadding(FMargin(0));
		Panel->SetClipping(EWidgetClipping::Inherit);
		const FSlateBrush Brush = MakeBrush(BackpackPaperSize);
		if (!Brush.GetResourceObject())
		{
			Panel->SetBrushColor(FallbackColor);
			return Panel;
		}
		FSlateBrush Invisible = Brush;
		Invisible.DrawAs = ESlateBrushDrawType::NoDrawType;
		Panel->SetBrush(Invisible);
		UCanvasPanel* Canvas = Tree->ConstructWidget<UCanvasPanel>();
		Canvas->SetClipping(EWidgetClipping::Inherit);
		Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
		Panel->SetContent(Canvas);

		// Slate derives Box corners from the source texture. Scale only the
		// background at the backpack's fit, then fill its inverse allotted size.
		UScaleBox* ScaleBox = Tree->ConstructWidget<UScaleBox>();
		ScaleBox->SetStretch(EStretch::UserSpecified);
		ScaleBox->SetUserSpecifiedScale(GetBackpackScale(BackpackHostSize));
		ScaleBox->SetClipping(EWidgetClipping::Inherit);
		UCanvasPanelSlot* SurfaceSlot = Canvas->AddChildToCanvas(ScaleBox);
		SurfaceSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		const FMargin Outsets = bUseBackpackEdges ? GetBackpackOutsets(BackpackHostSize) : FMargin(0);
		SurfaceSlot->SetOffsets(FMargin(-Outsets.Left, -Outsets.Top, -Outsets.Right, -Outsets.Bottom));
		UBorder* Paper = Tree->ConstructWidget<UBorder>();
		Paper->SetBrush(Brush);
		Paper->SetBrushColor(FLinearColor::White);
		Paper->SetPadding(FMargin(0));
		Paper->SetClipping(EWidgetClipping::Inherit);
		ScaleBox->SetContent(Paper);
		if (UScaleBoxSlot* Slot = Cast<UScaleBoxSlot>(Paper->Slot))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		return Panel;
	}

	void SetPanelContent(UBorder* Panel, UWidget* Content, const FMargin& Padding)
	{
		UWidget* Paper = Panel->GetContent();
		Panel->ClearChildren();
		UWidgetTree* Tree = Panel->GetTypedOuter<UWidgetTree>();
		UOverlay* Layers = Tree->ConstructWidget<UOverlay>();
		if (Paper)
		{
			UOverlaySlot* Slot = Layers->AddChildToOverlay(Paper);
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		UOverlaySlot* ContentSlot = Layers->AddChildToOverlay(Content);
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
		ContentSlot->SetPadding(Padding);
		Panel->SetPadding(FMargin(0));
		Panel->SetContent(Layers);
	}
}
