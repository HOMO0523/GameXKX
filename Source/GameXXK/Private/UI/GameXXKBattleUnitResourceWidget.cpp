#include "UI/GameXXKBattleUnitResourceWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

namespace
{
	const FString HealthFrameTexturePath(TEXT("/Game/GameXXK/UI/Town/Textures/PSD/Character/T_TownPsd_CharacterHealthFrame.T_TownPsd_CharacterHealthFrame"));
	const FString QiFrameTexturePath(TEXT("/Game/GameXXK/UI/Town/Textures/PSD/Character/T_TownPsd_CharacterManaFrame.T_TownPsd_CharacterManaFrame"));
	const FLinearColor PaperFallbackColor(0.86f, 0.79f, 0.62f, 1.0f);
	const FLinearColor HealthFillColor(0.62f, 0.25f, 0.22f, 1.0f);
	const FLinearColor QiFillColor(0.24f, 0.43f, 0.56f, 1.0f);

	FSlateBrush MakeFrameBrush(const FString& TexturePath, const FVector2D& ImageSize)
	{
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *TexturePath))
		{
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.ImageSize = ImageSize;
			Brush.TintColor = FSlateColor(FLinearColor::White);
			Brush.SetResourceObject(Texture);
			return Brush;
		}

		FSlateColorBrush PaperFallbackBrush(PaperFallbackColor);
		PaperFallbackBrush.ImageSize = ImageSize;
		return PaperFallbackBrush;
	}

	FProgressBarStyle MakeResourceBarStyle(const FString& FrameTexturePath, const FLinearColor& FillColor)
	{
		const FVector2D BarSize(168.0f, 18.0f);
		FProgressBarStyle Style;
		Style.SetBackgroundImage(MakeFrameBrush(FrameTexturePath, BarSize));
		FSlateColorBrush FillBrush(FillColor);
		FillBrush.ImageSize = BarSize;
		Style.SetFillImage(FillBrush);
		return Style;
	}

	void ConfigureReadableText(UTextBlock* TextBlock, const int32 FontSize)
	{
		if (!TextBlock)
		{
			return;
		}

		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		TextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.18f, 0.12f, 0.07f, 1.0f)));
	}

	float GetSafePercent(const int32 CurrentValue, const int32 MaximumValue)
	{
		return FMath::Clamp(
			static_cast<float>(CurrentValue) / static_cast<float>(FMath::Max(1, MaximumValue)),
			0.0f,
			1.0f);
	}
}

void UGameXXKBattleUnitResourceWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
	RefreshDisplay();
}

void UGameXXKBattleUnitResourceWidget::SetUnitResources(
	const FString& InSlotLabel,
	const FText& InDisplayName,
	const int32 InCurrentHP,
	const int32 InMaxHP,
	const int32 InCurrentMana,
	const int32 InMaxMana,
	const bool bInShowQi)
{
	SlotLabel = InSlotLabel;
	DisplayName = InDisplayName;
	CurrentHP = InCurrentHP;
	MaxHP = InMaxHP;
	CurrentMana = InCurrentMana;
	MaxMana = InMaxMana;
	bShowQi = bInShowQi;
	RefreshDisplay();
}

bool UGameXXKBattleUnitResourceWidget::PrepareForScreenSpaceEmbedding()
{
	Initialize();
	EnsureWidgetTree();
	RefreshDisplay();
	return RootBox && WidgetTree && WidgetTree->RootWidget == RootBox;
}

bool UGameXXKBattleUnitResourceWidget::HasRuntimeWidgetTreeForTest() const
{
	return RootBox && WidgetTree && WidgetTree->RootWidget == RootBox;
}

FString UGameXXKBattleUnitResourceWidget::GetHealthDisplayTextForTest() const
{
	return HealthText ? HealthText->GetText().ToString() : FString();
}

FString UGameXXKBattleUnitResourceWidget::GetQiDisplayTextForTest() const
{
	return QiText ? QiText->GetText().ToString() : FString();
}

float UGameXXKBattleUnitResourceWidget::GetHealthPercentForTest() const
{
	return HealthBar ? HealthBar->GetPercent() : 0.0f;
}

float UGameXXKBattleUnitResourceWidget::GetQiPercentForTest() const
{
	return QiBar ? QiBar->GetPercent() : 0.0f;
}

bool UGameXXKBattleUnitResourceWidget::IsQiRowVisibleForTest() const
{
	return QiRow && QiRow->GetVisibility() == ESlateVisibility::HitTestInvisible;
}

bool UGameXXKBattleUnitResourceWidget::AreContentWidgetsHitTestTransparentForTest() const
{
	return IdentityText
		&& IdentityText->GetVisibility() == ESlateVisibility::HitTestInvisible
		&& HealthRow
		&& HealthRow->GetVisibility() == ESlateVisibility::HitTestInvisible
		&& QiRow
		&& (QiRow->GetVisibility() == ESlateVisibility::HitTestInvisible || QiRow->GetVisibility() == ESlateVisibility::Collapsed);
}

ESlateVisibility UGameXXKBattleUnitResourceWidget::GetRootHitTestVisibilityForTest()
{
	return ESlateVisibility::SelfHitTestInvisible;
}

void UGameXXKBattleUnitResourceWidget::EnsureWidgetTree()
{
	if (RootBox || !WidgetTree)
	{
		return;
	}

	RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BattleUnitResourceRoot"));
	WidgetTree->RootWidget = RootBox;
	RootBox->SetVisibility(GetRootHitTestVisibilityForTest());

	IdentityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("IdentityText"));
	ConfigureReadableText(IdentityText, 13);
	IdentityText->SetJustification(ETextJustify::Center);
	IdentityText->SetVisibility(ESlateVisibility::HitTestInvisible);
	RootBox->AddChildToVerticalBox(IdentityText);

	HealthRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HealthRow"));
	HealthRow->SetVisibility(ESlateVisibility::HitTestInvisible);
	HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthText"));
	ConfigureReadableText(HealthText, 12);
	HealthRow->AddChildToHorizontalBox(HealthText);
	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
	HealthBar->SetWidgetStyle(MakeResourceBarStyle(HealthFrameTexturePath, HealthFillColor));
	HealthBar->SetBarFillType(EProgressBarFillType::LeftToRight);
	if (UHorizontalBoxSlot* HealthBarSlot = HealthRow->AddChildToHorizontalBox(HealthBar))
	{
		HealthBarSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
		HealthBarSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* HealthRowSlot = RootBox->AddChildToVerticalBox(HealthRow))
	{
		HealthRowSlot->SetPadding(FMargin(4.0f, 1.0f));
	}

	QiRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("QiRow"));
	QiRow->SetVisibility(ESlateVisibility::HitTestInvisible);
	QiText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QiText"));
	ConfigureReadableText(QiText, 12);
	QiRow->AddChildToHorizontalBox(QiText);
	QiBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("QiBar"));
	QiBar->SetWidgetStyle(MakeResourceBarStyle(QiFrameTexturePath, QiFillColor));
	QiBar->SetBarFillType(EProgressBarFillType::LeftToRight);
	if (UHorizontalBoxSlot* QiBarSlot = QiRow->AddChildToHorizontalBox(QiBar))
	{
		QiBarSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
		QiBarSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* QiRowSlot = RootBox->AddChildToVerticalBox(QiRow))
	{
		QiRowSlot->SetPadding(FMargin(4.0f, 1.0f));
	}
}

void UGameXXKBattleUnitResourceWidget::RefreshDisplay()
{
	if (!RootBox)
	{
		return;
	}

	if (IdentityText)
	{
		const FString Name = DisplayName.IsEmpty() ? TEXT("Unknown") : DisplayName.ToString();
		IdentityText->SetText(FText::FromString(FString::Printf(TEXT("%s · %s"), *SlotLabel, *Name)));
	}
	if (HealthText)
	{
		HealthText->SetText(FText::FromString(FString::Printf(TEXT("气血 %d / %d"), CurrentHP, MaxHP)));
	}
	if (HealthBar)
	{
		HealthBar->SetPercent(GetSafePercent(CurrentHP, MaxHP));
	}
	if (QiText)
	{
		QiText->SetText(FText::FromString(FString::Printf(TEXT("气力 %d / %d"), CurrentMana, MaxMana)));
	}
	if (QiBar)
	{
		QiBar->SetPercent(GetSafePercent(CurrentMana, MaxMana));
	}
	if (QiRow)
	{
		QiRow->SetVisibility(bShowQi ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
