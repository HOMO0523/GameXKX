#include "UI/GameXXKBattlePartyQiWidget.h"
#include "UI/GameXXKInRunUiStyle.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const FSoftObjectPath PartyQiSoulIconTexturePath(TEXT("/Game/GameXXK/UI/Battle/PartyQi/T_BattlePartyQi_SoulOrb.T_BattlePartyQi_SoulOrb"));
	const FVector2D PartyQiIconSize(140.0f, 140.0f);
	const FLinearColor SoulFallbackColor(0.26f, 0.31f, 0.30f, 0.84f);
	const FLinearColor PartyQiInkColor(0.12f, 0.10f, 0.075f, 1.0f);

	FSlateBrush MakeSoulIconBrush()
	{
		if (UTexture2D* Texture = Cast<UTexture2D>(PartyQiSoulIconTexturePath.TryLoad()))
		{
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.ImageSize = PartyQiIconSize;
			Brush.TintColor = FSlateColor(FLinearColor::White);
			Brush.SetResourceObject(Texture);
			return Brush;
		}

		FSlateColorBrush FallbackBrush(SoulFallbackColor);
		FallbackBrush.ImageSize = PartyQiIconSize;
		return FallbackBrush;
	}

	void ConfigureInkText(UTextBlock* TextBlock, const int32 FontSize)
	{
		if (!TextBlock)
		{
			return;
		}

		FSlateFontInfo Font = FGameXXKInRunUiStyle::Font(FontSize,true);
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		TextBlock->SetColorAndOpacity(FSlateColor(PartyQiInkColor));
		TextBlock->SetJustification(ETextJustify::Center);
		TextBlock->SetShadowOffset(FVector2D(1.0f, 1.0f));
		TextBlock->SetShadowColorAndOpacity(FLinearColor(0.93f, 0.89f, 0.78f, 0.58f));
		TextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UGameXXKBattlePartyQiWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
	RefreshDisplay();
}

void UGameXXKBattlePartyQiWidget::SetSharedQi(const int32 InSharedQi)
{
	SharedQi = FMath::Max(0, InSharedQi);
	RefreshDisplay();
}

bool UGameXXKBattlePartyQiWidget::PrepareForBoardEmbedding()
{
	Initialize();
	EnsureWidgetTree();
	RefreshDisplay();
	return RootBox && WidgetTree && WidgetTree->RootWidget == RootBox;
}

bool UGameXXKBattlePartyQiWidget::HasRuntimeWidgetTreeForTest() const
{
	return RootBox && WidgetTree && WidgetTree->RootWidget == RootBox;
}

int32 UGameXXKBattlePartyQiWidget::GetSharedQiForTest() const
{
	return SharedQi;
}

FString UGameXXKBattlePartyQiWidget::GetDisplayTextForTest() const
{
	return QiText ? QiText->GetText().ToString() : FString();
}

FString UGameXXKBattlePartyQiWidget::GetSubtitleTextForTest() const
{
	return FString();
}

bool UGameXXKBattlePartyQiWidget::AreContentWidgetsHitTestTransparentForTest() const
{
	return RootBox
		&& RootBox->GetVisibility() == ESlateVisibility::SelfHitTestInvisible
		&& IconOverlay
		&& IconOverlay->GetVisibility() == ESlateVisibility::HitTestInvisible
		&& SoulIcon
		&& SoulIcon->GetVisibility() == ESlateVisibility::HitTestInvisible
		&& QiText
		&& QiText->GetVisibility() == ESlateVisibility::HitTestInvisible;
}

FString UGameXXKBattlePartyQiWidget::GetPaperFrameResourcePathForTest() const
{
	return PartyQiSoulIconTexturePath.ToString();
}

FLinearColor UGameXXKBattlePartyQiWidget::GetQiInkColorForTest() const
{
	return PartyQiInkColor;
}

void UGameXXKBattlePartyQiWidget::EnsureWidgetTree()
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (RootBox || !WidgetTree)
	{
		return;
	}

	RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BattlePartyQiRoot"));
	RootBox->SetWidthOverride(PartyQiIconSize.X);
	RootBox->SetHeightOverride(PartyQiIconSize.Y);
	RootBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = RootBox;

	IconOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BattlePartyQiOverlay"));
	IconOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	RootBox->SetContent(IconOverlay);

	SoulIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BattlePartyQiSoulIcon"));
	SoulIcon->SetBrush(MakeSoulIconBrush());
	SoulIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* SoulIconSlot = IconOverlay->AddChildToOverlay(SoulIcon))
	{
		SoulIconSlot->SetHorizontalAlignment(HAlign_Fill);
		SoulIconSlot->SetVerticalAlignment(VAlign_Fill);
	}

	QiText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BattlePartyQiText"));
	ConfigureInkText(QiText, 56);
	if (UOverlaySlot* QiSlot = IconOverlay->AddChildToOverlay(QiText))
	{
		QiSlot->SetHorizontalAlignment(HAlign_Center);
		QiSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UGameXXKBattlePartyQiWidget::RefreshDisplay()
{
	if (QiText)
	{
		QiText->SetText(FText::FromString(FString::FromInt(SharedQi)));
	}
}
