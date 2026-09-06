#include "UI/GameXXKBattleStatusIconWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Math/Box2D.h"
#include "Styling/SlateBrush.h"

namespace
{
	const FLinearColor PaperColor(0.96f, 0.90f, 0.76f, 1.0f);
	const FLinearColor InkColor(0.13f, 0.09f, 0.05f, 1.0f);
	const FLinearColor StackSealColor(0.32f, 0.08f, 0.05f, 1.0f);

	FSlateBrush MakeRoundedBrush(
		const FLinearColor& FillColor,
		const FLinearColor& OutlineColor,
		const float OutlineWidth,
		const float CornerRadius)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FillColor);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(CornerRadius, FSlateColor(OutlineColor), OutlineWidth);
		return Brush;
	}

	void ConfigureCenteredText(UTextBlock* const TextBlock, const int32 FontSize, const FLinearColor& Color)
	{
		if (!TextBlock)
		{
			return;
		}

		TextBlock->SetJustification(ETextJustify::Center);
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	FString FormatStack(const int32 Stacks)
	{
		return Stacks > 99 ? TEXT("99+") : FString::FromInt(FMath::Max(1, Stacks));
	}

	FSlateBrush MakeCroppedIconBrush(UTexture2D* const Texture, const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.TintColor = FSlateColor(Tint);
		Brush.SetResourceObject(Texture);
		Brush.SetUVRegion(FBox2f(FVector2f(0.22f, 0.20f), FVector2f(0.80f, 0.80f)));
		return Brush;
	}
}

void UGameXXKBattleStatusIconWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
	RefreshDisplay();
}

FReply UGameXXKBattleStatusIconWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	return MakeMouseButtonDownReply();
}

void UGameXXKBattleStatusIconWidget::SetBadgeModel(
	const FGameXXKBattleStatusBadgeModel& InBadgeModel,
	const bool bInOverflow)
{
	CachedBadgeModel = InBadgeModel;
	bIsOverflowBadge = bInOverflow;
	RefreshDisplay();
}

bool UGameXXKBattleStatusIconWidget::PrepareForScreenSpaceEmbedding()
{
	Initialize();
	EnsureWidgetTree();
	RefreshDisplay();
	return RootBox && WidgetTree && WidgetTree->RootWidget == RootBox;
}

bool UGameXXKBattleStatusIconWidget::HasRuntimeWidgetTreeForTest() const
{
	return RootBox && WidgetTree && WidgetTree->RootWidget == RootBox;
}

FName UGameXXKBattleStatusIconWidget::GetIconIdForTest() const
{
	return CachedBadgeModel.Style.IconId;
}

FString UGameXXKBattleStatusIconWidget::GetDisplayedStackForTest() const
{
	return StackText ? StackText->GetText().ToString() : FString();
}

FString UGameXXKBattleStatusIconWidget::FormatStackForTest(const int32 Stacks)
{
	return FormatStack(Stacks);
}

float UGameXXKBattleStatusIconWidget::GetIconOverscanPaddingForTest()
{
	return -3.0f;
}

ESlateVisibility UGameXXKBattleStatusIconWidget::GetHitTargetVisibilityForTest()
{
	return ESlateVisibility::Visible;
}

ESlateVisibility UGameXXKBattleStatusIconWidget::GetTooltipVisibilityForTest()
{
	return ESlateVisibility::HitTestInvisible;
}

FReply UGameXXKBattleStatusIconWidget::GetMouseButtonDownReplyForTest() const
{
	return MakeMouseButtonDownReply();
}

bool UGameXXKBattleStatusIconWidget::DoesMouseDownPassThroughForTest() const
{
	return !GetMouseButtonDownReplyForTest().IsEventHandled();
}

FReply UGameXXKBattleStatusIconWidget::MakeMouseButtonDownReply()
{
	return FReply::Unhandled();
}

void UGameXXKBattleStatusIconWidget::EnsureWidgetTree()
{
	// A normal unit-HUD badge must stay input-transparent, but callers may
	// deliberately hide a prepared badge before NativeConstruct runs.  The
	// retired cinematic badge follows exactly that lifecycle: Board creation
	// prepares it, marks it Hidden, then NativeConstruct re-enters this method.
	// Do not resurrect Hidden/Collapsed badges into an empty "? x1" overlay.
	if (GetVisibility() != ESlateVisibility::Hidden
		&& GetVisibility() != ESlateVisibility::Collapsed)
	{
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	if (RootBox || !WidgetTree)
	{
		return;
	}

	RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BattleStatusIconRoot"));
	WidgetTree->RootWidget = RootBox;
	// Keep the native 44px slot so six badges plus the overflow badge still fit
	// inside the fixed 320px actor HUD. The source art is enlarged by UV crop,
	// not by widening the row and pushing the status rail into the next unit.
	RootBox->SetWidthOverride(44.0f);
	RootBox->SetHeightOverride(44.0f);
	RootBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	HitTarget = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BattleStatusIconHitTarget"));
	HitTarget->SetBrush(MakeRoundedBrush(InkColor, InkColor, 1.0f, 6.0f));
	HitTarget->SetPadding(FMargin(1.0f));
	HitTarget->SetVisibility(GetHitTargetVisibilityForTest());
	RootBox->SetContent(HitTarget);

	UBorder* const BadgePaper = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BattleStatusIconPaper"));
	BadgePaper->SetBrush(MakeRoundedBrush(PaperColor, InkColor, 1.0f, 5.0f));
	BadgePaper->SetPadding(FMargin(0.0f));
	BadgePaper->SetVisibility(ESlateVisibility::HitTestInvisible);
	HitTarget->SetContent(BadgePaper);

	UOverlay* const BadgeOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BattleStatusIconOverlay"));
	BadgeOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	BadgeOverlay->SetClipping(EWidgetClipping::ClipToBounds);
	BadgePaper->SetContent(BadgeOverlay);

	UImage* const IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BattleStatusIconImage"));
	IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* const IconSlot = BadgeOverlay->AddChildToOverlay(IconImage))
	{
		IconSlot->SetHorizontalAlignment(HAlign_Fill);
		IconSlot->SetVerticalAlignment(VAlign_Fill);
		IconSlot->SetPadding(FMargin(GetIconOverscanPaddingForTest()));
	}

	UTextBlock* const GlyphText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BattleStatusIconGlyph"));
	ConfigureCenteredText(GlyphText, 19, InkColor);
	GlyphText->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* const GlyphSlot = BadgeOverlay->AddChildToOverlay(GlyphText))
	{
		GlyphSlot->SetHorizontalAlignment(HAlign_Center);
		GlyphSlot->SetVerticalAlignment(VAlign_Center);
	}

	UBorder* const StackSeal = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BattleStatusIconStackSeal"));
	StackSeal->SetBrush(MakeRoundedBrush(StackSealColor, InkColor, 1.0f, 7.0f));
	StackSeal->SetPadding(FMargin(2.0f, 0.0f));
	StackSeal->SetVisibility(ESlateVisibility::HitTestInvisible);
	StackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BattleStatusIconStackText"));
	ConfigureCenteredText(StackText, 10, FLinearColor::White);
	StackSeal->SetContent(StackText);
	if (UOverlaySlot* const StackSlot = BadgeOverlay->AddChildToOverlay(StackSeal))
	{
		StackSlot->SetHorizontalAlignment(HAlign_Right);
		StackSlot->SetVerticalAlignment(VAlign_Top);
	}

	UBorder* const TooltipPaper = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BattleStatusIconTooltipPaper"));
	TooltipPaper->SetBrush(MakeRoundedBrush(PaperColor, InkColor, 1.0f, 6.0f));
	TooltipPaper->SetPadding(FMargin(8.0f, 6.0f));
	TooltipPaper->SetVisibility(GetTooltipVisibilityForTest());
	UTextBlock* const TooltipText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BattleStatusIconTooltipText"));
	ConfigureCenteredText(TooltipText, 13, InkColor);
	TooltipText->SetAutoWrapText(true);
	TooltipPaper->SetContent(TooltipText);
	HitTarget->SetToolTip(TooltipPaper);
}

void UGameXXKBattleStatusIconWidget::RefreshDisplay()
{
	if (!RootBox)
	{
		return;
	}

	if (StackText)
	{
		StackText->SetText(FText::FromString(
			bIsOverflowBadge ? FString::Printf(TEXT("+%d"), FMath::Max(0, CachedBadgeModel.Stacks)) : FormatStack(CachedBadgeModel.Stacks)));
	}

	UImage* const IconImage = Cast<UImage>(GetWidgetFromName(TEXT("BattleStatusIconImage")));
	UTextBlock* const GlyphText = Cast<UTextBlock>(GetWidgetFromName(TEXT("BattleStatusIconGlyph")));
	bool bUseFallbackGlyph = bIsOverflowBadge;
	if (!bUseFallbackGlyph && CachedBadgeModel.Style.TexturePath.IsValid())
	{
		if (UTexture2D* const Texture = Cast<UTexture2D>(CachedBadgeModel.Style.TexturePath.TryLoad()))
		{
			if (IconImage)
			{
				IconImage->SetBrush(MakeCroppedIconBrush(Texture, CachedBadgeModel.Style.Tint));
				IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			bUseFallbackGlyph = false;
		}
		else
		{
			bUseFallbackGlyph = true;
		}
	}
	else if (!bIsOverflowBadge)
	{
		bUseFallbackGlyph = true;
	}

	if (IconImage && bUseFallbackGlyph)
	{
		IconImage->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (GlyphText)
	{
		const FString Glyph = bIsOverflowBadge
			? TEXT("+")
			: (CachedBadgeModel.Style.FallbackGlyph.IsEmpty() ? TEXT("?") : CachedBadgeModel.Style.FallbackGlyph);
		GlyphText->SetText(FText::FromString(Glyph));
		GlyphText->SetColorAndOpacity(FSlateColor(CachedBadgeModel.Style.Tint));
		GlyphText->SetVisibility(bUseFallbackGlyph ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (UBorder* const TooltipPaper = HitTarget ? Cast<UBorder>(HitTarget->GetToolTip()) : nullptr)
	{
		if (UTextBlock* const TooltipText = Cast<UTextBlock>(TooltipPaper->GetContent()))
		{
			TooltipText->SetText(FText::FromString(CachedBadgeModel.Tooltip));
		}
	}
}
