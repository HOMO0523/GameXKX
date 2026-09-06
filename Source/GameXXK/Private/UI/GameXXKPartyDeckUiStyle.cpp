#include "UI/GameXXKPartyDeckUiStyle.h"

#include "Components/ScrollBox.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

namespace
{
	static constexpr const TCHAR* PaperTrackTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/Scrollbars/T_PartyDeck_ScrollPaperTrack_GeneratedV1.T_PartyDeck_ScrollPaperTrack_GeneratedV1");
	static constexpr const TCHAR* InkThumbTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/Scrollbars/T_PartyDeck_ScrollInkThumb_GeneratedV1.T_PartyDeck_ScrollInkThumb_GeneratedV1");

	FSlateBrush MakeScrollBrush(UTexture2D* Texture, const FVector2D& Size, const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = Size;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}
}

void FGameXXKPartyDeckUiStyle::ApplyPaperInkScrollBar(UScrollBox* ScrollBox)
{
	if (!ScrollBox)
	{
		return;
	}

	UTexture2D* PaperTrack = LoadObject<UTexture2D>(nullptr, PaperTrackTexturePath);
	UTexture2D* InkThumb = LoadObject<UTexture2D>(nullptr, InkThumbTexturePath);
	if (!PaperTrack || !InkThumb)
	{
		return;
	}

	const FSlateBrush PaperBrush = MakeScrollBrush(PaperTrack, FVector2D(18.0f, 128.0f), FLinearColor(0.882f, 0.827f, 0.722f, 1.0f));
	FScrollBarStyle ScrollStyle;
	ScrollStyle
		.SetVerticalBackgroundImage(PaperBrush)
		.SetVerticalTopSlotImage(PaperBrush)
		.SetVerticalBottomSlotImage(PaperBrush)
		.SetNormalThumbImage(MakeScrollBrush(InkThumb, FVector2D(18.0f, 62.0f), FLinearColor(0.161f, 0.145f, 0.133f, 0.96f)))
		.SetHoveredThumbImage(MakeScrollBrush(InkThumb, FVector2D(18.0f, 62.0f), FLinearColor(0.251f, 0.224f, 0.196f, 1.0f)))
		.SetDraggedThumbImage(MakeScrollBrush(InkThumb, FVector2D(18.0f, 62.0f), FLinearColor(0.102f, 0.090f, 0.078f, 1.0f)))
		.SetThickness(18.0f);
	ScrollBox->SetWidgetBarStyle(ScrollStyle);
	ScrollBox->SetScrollbarThickness(FVector2D(18.0f, 18.0f));
	ScrollBox->SetScrollbarPadding(FMargin(3.0f, 2.0f, 0.0f, 2.0f));
	ScrollBox->SetAlwaysShowScrollbar(true);
}

FString FGameXXKPartyDeckUiStyle::GetPaperTrackResourcePath()
{
	return PaperTrackTexturePath;
}

FString FGameXXKPartyDeckUiStyle::GetInkThumbResourcePath()
{
	return InkThumbTexturePath;
}

FString FGameXXKPartyDeckUiStyle::GetBackpackInkThumbResourcePath()
{
	return TEXT("/Game/GameXXK/UI/MasterV2/Approved/inventory_scrollbar_Button.inventory_scrollbar_Button");
}

void FGameXXKPartyDeckUiStyle::ApplyBackpackInkScrollBar(UScrollBox* ScrollBox, const float Thickness, const float MinimumThumbLength)
{
	if (!ScrollBox) return;
	UTexture2D* Ink = LoadObject<UTexture2D>(nullptr, *GetBackpackInkThumbResourcePath());
	if (!Ink) return;
	const FVector2D ThumbSize = ScrollBox->GetOrientation() == Orient_Horizontal
		? FVector2D(MinimumThumbLength, Thickness) : FVector2D(Thickness, MinimumThumbLength);
	FSlateBrush Invisible;
	Invisible.DrawAs = ESlateBrushDrawType::NoDrawType;
	FScrollBarStyle Style;
	Style.SetHorizontalBackgroundImage(Invisible).SetHorizontalTopSlotImage(Invisible).SetHorizontalBottomSlotImage(Invisible)
		.SetVerticalBackgroundImage(Invisible).SetVerticalTopSlotImage(Invisible).SetVerticalBottomSlotImage(Invisible)
		.SetNormalThumbImage(MakeScrollBrush(Ink, ThumbSize, FLinearColor::White))
		.SetHoveredThumbImage(MakeScrollBrush(Ink, ThumbSize, FLinearColor(1.12f, 1.08f, 1.0f)))
		.SetDraggedThumbImage(MakeScrollBrush(Ink, ThumbSize, FLinearColor(0.75f, 0.72f, 0.68f)))
		.SetThickness(Thickness);
	ScrollBox->SetWidgetBarStyle(Style);
	ScrollBox->SetScrollbarThickness(ThumbSize);
	ScrollBox->SetScrollbarPadding(FMargin(2.0f));
	ScrollBox->SetAlwaysShowScrollbar(false);
	ScrollBox->SetScrollBarVisibility(ESlateVisibility::Visible);
}
