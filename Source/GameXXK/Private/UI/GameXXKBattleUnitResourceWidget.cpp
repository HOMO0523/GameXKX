#include "UI/GameXXKBattleUnitResourceWidget.h"
#include "UI/GameXXKLocalization.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "UI/GameXXKInkResourceBarStyle.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Styling/SlateBrush.h"
#include "Brushes/SlateColorBrush.h"

namespace
{
	const FString HealthTrackTexturePath(TEXT("/Game/GameXXK/UI/Battle/ResourceBars/InkV3/T_InkResourceBarMaster.T_InkResourceBarMaster"));
	const FString HealthFullTexturePath(TEXT("/Game/GameXXK/UI/Battle/ResourceBars/InkV3/T_InkResourceBarMaster.T_InkResourceBarMaster"));
	const FString ManaTrackTexturePath(TEXT("/Game/GameXXK/UI/Battle/ResourceBars/InkV3/T_InkResourceBarMaster.T_InkResourceBarMaster"));
	const FString ManaFullTexturePath(TEXT("/Game/GameXXK/UI/Battle/ResourceBars/InkV3/T_InkResourceBarMaster.T_InkResourceBarMaster"));
	const FString ResourceMaskMaterialPath(TEXT("/Game/GameXXK/UI/Battle/ResourceBars/InkV3/M_InkResourceBar.M_InkResourceBar"));
	// The same authored silhouette is used at both battle and travel HUD sizes.
	const FVector2D ResourceBarLogicalSize(252.0f, 28.0f);

	FSlateBrush MakeResourceBrush(const FString& TexturePath)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = ResourceBarLogicalSize;
		Brush.TintColor = FSlateColor(FLinearColor::White);
		if (UTexture2D* const Texture = LoadObject<UTexture2D>(nullptr, *TexturePath))
		{
			Brush.SetResourceObject(Texture);
		}
		return Brush;
	}

	FProgressBarStyle MakeResourceBarStyle(const FString& TrackPath, const FString& FullPath)
	{
		FProgressBarStyle Style;
		Style.SetBackgroundImage(MakeResourceBrush(TrackPath));
		Style.SetFillImage(MakeResourceBrush(FullPath));
		Style.SetMarqueeImage(FSlateBrush());
		return Style;
	}

	UMaterialInstanceDynamic* CreateResourceMaskMaterial(UObject* Outer, const bool bMana = false)
	{
		return GameXXKInkResourceBarStyle::Create(Outer,ResourceBarLogicalSize,bMana);
	}

	void RefreshResourceMask(
		UImage* const Image,
		UMaterialInstanceDynamic* const Material,
		const float Percent)
	{
		if (!Image)
		{
			return;
		}

		if (!Material)
		{
			Image->SetBrush(FSlateBrush());
			return;
		}
		GameXXKInkResourceBarStyle::Update(Material,Percent);
		Image->SetBrushFromMaterial(Material);
	}

	void ConfigureReadableText(UTextBlock* TextBlock, const int32 FontSize)
	{
		if (!TextBlock)
		{
			return;
		}

		FSlateFontInfo Font = FGameXXKInRunUiStyle::Font(FontSize,true);
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		TextBlock->SetColorAndOpacity(FSlateColor(FGameXXKInRunUiStyle::Ink()));
		TextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	float GetSafePercent(const int32 CurrentValue, const int32 MaximumValue)
	{
		if (MaximumValue <= 0)
		{
			return 0.0f;
		}

		return FMath::Clamp(
			static_cast<float>(CurrentValue) / static_cast<float>(MaximumValue),
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

void UGameXXKBattleUnitResourceWidget::SetUnitVitals(
	const FString& InSlotLabel,
	const FText& InDisplayName,
	const int32 InCurrentHP,
	const int32 InMaxHP,
	const int32 InCurrentMana,
	const int32 InMaxMana,
	const bool bInShowMana)
{
	SlotLabel = InSlotLabel;
	DisplayName = InDisplayName;
	CurrentHP = InCurrentHP;
	MaxHP = InMaxHP;
	CurrentMana = InCurrentMana;
	MaxMana = InMaxMana;
	bShowMana = bInShowMana;
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

UWidget* UGameXXKBattleUnitResourceWidget::GetHealthRowForGuide() const
{
	return HealthRow.Get();
}

UWidget* UGameXXKBattleUnitResourceWidget::GetManaRowForGuide() const
{
	return ManaRow.Get();
}

FString UGameXXKBattleUnitResourceWidget::GetHealthDisplayTextForTest() const
{
	return HealthText ? HealthText->GetText().ToString() : FString();
}

FString UGameXXKBattleUnitResourceWidget::GetManaDisplayTextForTest() const
{
	return ManaText ? ManaText->GetText().ToString() : FString();
}

float UGameXXKBattleUnitResourceWidget::GetHealthPercentForTest() const
{
	return HealthPercent;
}

float UGameXXKBattleUnitResourceWidget::GetManaPercentForTest() const
{
	return ManaPercent;
}

bool UGameXXKBattleUnitResourceWidget::IsHealthFillLeftToRightForTest() const
{
	return HealthProgressBar && HealthProgressBar->GetBarFillType() == EProgressBarFillType::LeftToRight;
}

bool UGameXXKBattleUnitResourceWidget::IsManaFillLeftToRightForTest() const
{
	return ManaProgressBar && ManaProgressBar->GetBarFillType() == EProgressBarFillType::LeftToRight;
}

FString UGameXXKBattleUnitResourceWidget::GetHealthTrackResourcePathForTest() const
{
	return HealthTrackTexturePath;
}

FString UGameXXKBattleUnitResourceWidget::GetHealthFullResourcePathForTest() const
{
	return HealthFullTexturePath;
}

FString UGameXXKBattleUnitResourceWidget::GetManaTrackResourcePathForTest() const
{
	return ManaTrackTexturePath;
}

FString UGameXXKBattleUnitResourceWidget::GetManaFullResourcePathForTest() const
{
	return ManaFullTexturePath;
}

FString UGameXXKBattleUnitResourceWidget::GetResourceMaskMaterialPathForTest() const
{
	return ResourceMaskMaterialPath;
}

bool UGameXXKBattleUnitResourceWidget::IsManaRowVisibleForTest() const
{
	return ManaRow && ManaRow->GetVisibility() == ESlateVisibility::SelfHitTestInvisible;
}

ESlateVisibility UGameXXKBattleUnitResourceWidget::GetManaRowVisibilityForTest() const
{
	return ManaRow ? ManaRow->GetVisibility() : ESlateVisibility::Collapsed;
}

bool UGameXXKBattleUnitResourceWidget::AreContentWidgetsHitTestTransparentForTest() const
{
	return IdentityText
		&& IdentityText->GetVisibility() == ESlateVisibility::HitTestInvisible
		&& HealthRow
		&& HealthRow->GetVisibility() == ESlateVisibility::HitTestInvisible
		&& ManaRow
		&& (ManaRow->GetVisibility() == ESlateVisibility::SelfHitTestInvisible || ManaRow->GetVisibility() == ESlateVisibility::Collapsed);
}

ESlateVisibility UGameXXKBattleUnitResourceWidget::GetRootHitTestVisibilityForTest()
{
	return ESlateVisibility::SelfHitTestInvisible;
}

void UGameXXKBattleUnitResourceWidget::EnsureWidgetTree()
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (RootBox || !WidgetTree)
	{
		return;
	}

	RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BattleUnitResourceRoot"));
	WidgetTree->RootWidget = RootBox;
	RootBox->SetVisibility(GetRootHitTestVisibilityForTest());

	IdentityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("IdentityText"));
	ConfigureReadableText(IdentityText, 18);
	IdentityText->SetFont(FGameXXKInRunUiStyle::Font(18,true));
	IdentityText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* const IdentitySlot = RootBox->AddChildToVerticalBox(IdentityText))
	{
		IdentitySlot->SetHorizontalAlignment(HAlign_Center);
		IdentitySlot->SetPadding(FMargin(2.0f, 0.0f, 2.0f, 1.0f));
	}

	HealthRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HealthRow"));
	HealthRow->SetVisibility(ESlateVisibility::HitTestInvisible);
	UVerticalBox* const HealthContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HealthContentBox"));
	HealthContentBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthText"));
	ConfigureReadableText(HealthText, 18);
	HealthText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* const HealthTextSlot = HealthContentBox->AddChildToVerticalBox(HealthText))
	{
		HealthTextSlot->SetHorizontalAlignment(HAlign_Center);
		HealthTextSlot->SetPadding(FMargin(2.0f, 0.0f));
	}
	HealthBarSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HealthBarSizeBox"));
	HealthBarSizeBox->SetWidthOverride(ResourceBarLogicalSize.X);
	HealthBarSizeBox->SetHeightOverride(ResourceBarLogicalSize.Y);
	HealthProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthProgressBar"));
	HealthProgressBar->SetWidgetStyle(MakeResourceBarStyle(HealthTrackTexturePath, HealthFullTexturePath));
	HealthProgressBar->SetBarFillType(EProgressBarFillType::LeftToRight);
	HealthProgressBar->SetPercent(HealthPercent);
	HealthBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("HealthBarLegacy"));
	HealthTrackTexture = LoadObject<UTexture2D>(nullptr, *HealthTrackTexturePath);
	HealthFullTexture = LoadObject<UTexture2D>(nullptr, *HealthFullTexturePath);
	HealthMaskMaterial = CreateResourceMaskMaterial(this);
	// Render the shared ink rail through its authored UI material. A native
	// UProgressBar scales the full texture itself and exposes its transparent
	// source margins, which is why the fill appeared centered instead of being
	// consumed from the left edge.  Keep the progress bar as a test seam, but
	// use the mask image for the actual HUD.
	HealthText->RemoveFromParent();
	UOverlay* HealthOverlay=WidgetTree->ConstructWidget<UOverlay>();
	HealthOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	auto* HealthImageSlot=HealthOverlay->AddChildToOverlay(HealthBar);HealthImageSlot->SetHorizontalAlignment(HAlign_Fill);HealthImageSlot->SetVerticalAlignment(VAlign_Fill);
	auto* HealthNumberSlot=HealthOverlay->AddChildToOverlay(HealthText);HealthNumberSlot->SetHorizontalAlignment(HAlign_Center);HealthNumberSlot->SetVerticalAlignment(VAlign_Center);
	FSlateFontInfo HealthFont=HealthText->GetFont();HealthFont.OutlineSettings.OutlineSize=1;HealthFont.OutlineSettings.OutlineColor=FLinearColor(0.06f,0.04f,0.02f,1);HealthText->SetFont(HealthFont);HealthText->SetColorAndOpacity(FSlateColor(FLinearColor(1,0.97f,0.88f,1)));
	HealthBarSizeBox->SetContent(HealthOverlay);
	if (UVerticalBoxSlot* const HealthBarSlot = HealthContentBox->AddChildToVerticalBox(HealthBarSizeBox))
	{
		HealthBarSlot->SetHorizontalAlignment(HAlign_Center);
	}
	if (UHorizontalBoxSlot* const HealthContentSlot = HealthRow->AddChildToHorizontalBox(HealthContentBox))
	{
		HealthContentSlot->SetHorizontalAlignment(HAlign_Center);
		HealthContentSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* HealthRowSlot = RootBox->AddChildToVerticalBox(HealthRow))
	{
		HealthRowSlot->SetHorizontalAlignment(HAlign_Center);
		HealthRowSlot->SetPadding(FMargin(2.0f, 0.0f, 2.0f, 1.0f));
	}

	ManaRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ManaRow"));
	ManaRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UVerticalBox* const ManaContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ManaContentBox"));
	ManaContentBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	ManaText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ManaText"));
	ConfigureReadableText(ManaText, 18);
	ManaText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* const ManaTextSlot = ManaContentBox->AddChildToVerticalBox(ManaText))
	{
		ManaTextSlot->SetHorizontalAlignment(HAlign_Center);
		ManaTextSlot->SetPadding(FMargin(2.0f, 0.0f));
	}
	ManaBarSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ManaBarSizeBox"));
	ManaBarSizeBox->SetWidthOverride(ResourceBarLogicalSize.X);
	ManaBarSizeBox->SetHeightOverride(ResourceBarLogicalSize.Y);
	ManaProgressBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ManaProgressBar"));
	ManaProgressBar->SetWidgetStyle(MakeResourceBarStyle(ManaTrackTexturePath, ManaFullTexturePath));
	ManaProgressBar->SetBarFillType(EProgressBarFillType::LeftToRight);
	ManaProgressBar->SetPercent(ManaPercent);
	ManaBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ManaBarLegacy"));
	ManaTrackTexture = LoadObject<UTexture2D>(nullptr, *ManaTrackTexturePath);
	ManaFullTexture = LoadObject<UTexture2D>(nullptr, *ManaFullTexturePath);
	ManaMaskMaterial = CreateResourceMaskMaterial(this,true);
	ManaText->RemoveFromParent();
	UOverlay* ManaOverlay=WidgetTree->ConstructWidget<UOverlay>();
	ManaOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	auto* ManaImageSlot=ManaOverlay->AddChildToOverlay(ManaBar);ManaImageSlot->SetHorizontalAlignment(HAlign_Fill);ManaImageSlot->SetVerticalAlignment(VAlign_Fill);
	auto* ManaNumberSlot=ManaOverlay->AddChildToOverlay(ManaText);ManaNumberSlot->SetHorizontalAlignment(HAlign_Center);ManaNumberSlot->SetVerticalAlignment(VAlign_Center);
	FSlateFontInfo ManaFont=ManaText->GetFont();ManaFont.OutlineSettings.OutlineSize=1;ManaFont.OutlineSettings.OutlineColor=FLinearColor(0.06f,0.04f,0.02f,1);ManaText->SetFont(ManaFont);ManaText->SetColorAndOpacity(FSlateColor(FLinearColor(1,0.97f,0.88f,1)));
	ManaBarSizeBox->SetContent(ManaOverlay);
	if (UVerticalBoxSlot* const ManaBarSlot = ManaContentBox->AddChildToVerticalBox(ManaBarSizeBox))
	{
		ManaBarSlot->SetHorizontalAlignment(HAlign_Center);
	}
	if (UHorizontalBoxSlot* const ManaContentSlot = ManaRow->AddChildToHorizontalBox(ManaContentBox))
	{
		ManaContentSlot->SetHorizontalAlignment(HAlign_Center);
		ManaContentSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* ManaRowSlot = RootBox->AddChildToVerticalBox(ManaRow))
	{
		ManaRowSlot->SetHorizontalAlignment(HAlign_Center);
		ManaRowSlot->SetPadding(FMargin(2.0f, 0.0f, 2.0f, 1.0f));
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
		const FText Name = DisplayName.IsEmpty() ? GameXXKLocalization::Source(TEXT("未知")) : GameXXKLocalization::Localize(DisplayName);
		IdentityText->SetText(FText::Format(NSLOCTEXT("GameXXKBattle", "UnitIdentity", "{0} · {1}"),
			GameXXKLocalization::Source(SlotLabel), Name));
	}
	if (HealthText)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[HpText] outer=%s setHP=%d healthText=%s"), *GetPathName(), CurrentHP, *HealthText->GetPathName());
		HealthText->SetText(GameXXKLocalization::Source(FString::Printf(TEXT("气血 %d / %d"), CurrentHP, MaxHP)));
	}
	HealthPercent = GetSafePercent(CurrentHP, MaxHP);
	if (HealthProgressBar)
	{
		HealthProgressBar->SetPercent(HealthPercent);
	}
	RefreshResourceMask(HealthBar, HealthMaskMaterial, HealthPercent);
	if (ManaText)
	{
		ManaText->SetText(GameXXKLocalization::Source(FString::Printf(TEXT("内力 %d / %d"), CurrentMana, MaxMana)));
	}
	ManaPercent = GetSafePercent(CurrentMana, MaxMana);
	if (ManaProgressBar)
	{
		ManaProgressBar->SetPercent(ManaPercent);
	}
	RefreshResourceMask(ManaBar, ManaMaskMaterial, ManaPercent);
	if (ManaRow)
	{
		ManaRow->SetVisibility(bShowMana ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}
