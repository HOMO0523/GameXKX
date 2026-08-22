#include "UI/GameXXKRouteMerchantWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SafeZone.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKCardText.h"
#include "GameXXKRelicCatalog.h"
#include "MVP/GameXXKMVPSubsystem.h"

namespace
{
	const FVector2D MerchantDesignResolution(1920.0f, 1080.0f);
	const FVector2D MerchantCardFrameSize(250.0f, 350.0f);
	const FVector2D MerchantRelicFrameSize(250.0f, 250.0f);
	constexpr float MerchantColumnFraction = 0.23f;
	constexpr float OffersColumnFraction = 0.77f;
	constexpr int32 MerchantCardSlotCount = 0;
	constexpr int32 MerchantRelicSlotCount = 4;
	constexpr int32 MerchantOfferSlotCount = MerchantCardSlotCount + MerchantRelicSlotCount;

	static constexpr const TCHAR* CardFrameTexturePath = TEXT("/Game/GameXXK/UI/Cards/Textures/T_CardFrame_PSD057.T_CardFrame_PSD057");
	static constexpr const TCHAR* PanelFrameTexturePath = TEXT("/Game/GameXXK/UI/Town/Textures/Backpack/T_TownBackpack_WindowFrame.T_TownBackpack_WindowFrame");
	static constexpr const TCHAR* ActionButtonTexturePath = TEXT("/Game/GameXXK/UI/Town/Textures/PSD/Controls/T_TownPsd_ButtonPrimary.T_TownPsd_ButtonPrimary");
	static constexpr const TCHAR* HeroPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Hero.T_CardPortrait_Hero");
	static constexpr const TCHAR* BladePortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_Blade.T_CardPortrait_Role_Blade");
	static constexpr const TCHAR* GuardPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_Guard.T_CardPortrait_Role_Guard");
	static constexpr const TCHAR* HealerPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_Healer.T_CardPortrait_Role_Healer");
	static constexpr const TCHAR* HunterPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_Hunter.T_CardPortrait_Role_Hunter");
	static constexpr const TCHAR* SorcererPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_Sorcerer.T_CardPortrait_Role_Sorcerer");
	static constexpr const TCHAR* FormationMasterPortraitTexturePath = TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_FormationMaster.T_CardPortrait_Role_FormationMaster");

	UTexture2D* LoadTexture(const TCHAR* Path)
	{
		return Path ? LoadObject<UTexture2D>(nullptr, Path) : nullptr;
	}

	FSlateBrush MakeTextureBrush(
		const TCHAR* Path,
		const FVector2D& ImageSize,
		const ESlateBrushDrawType::Type DrawAs = ESlateBrushDrawType::Image,
		const FMargin& Margin = FMargin(0.065f),
		const FLinearColor& Tint = FLinearColor::White)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(LoadTexture(Path));
		Brush.DrawAs = DrawAs;
		Brush.ImageSize = ImageSize;
		Brush.Margin = Margin;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	FButtonStyle MakeTextureButtonStyle(
		const TCHAR* Path,
		const FVector2D& ImageSize,
		const bool bBox,
		const FMargin& Margin = FMargin(0.065f))
	{
		const ESlateBrushDrawType::Type DrawAs = bBox ? ESlateBrushDrawType::Box : ESlateBrushDrawType::Image;
		const FSlateBrush Normal = MakeTextureBrush(Path, ImageSize, DrawAs, Margin);
		const FSlateBrush Hovered = MakeTextureBrush(Path, ImageSize, DrawAs, Margin, FLinearColor(1.06f, 1.04f, 0.96f, 1.0f));
		const FSlateBrush Pressed = MakeTextureBrush(Path, ImageSize, DrawAs, Margin, FLinearColor(0.82f, 0.78f, 0.68f, 1.0f));
		const FSlateBrush Disabled = MakeTextureBrush(Path, ImageSize, DrawAs, Margin, FLinearColor(0.42f, 0.42f, 0.42f, 0.78f));
		FButtonStyle Style;
		Style.SetNormal(Normal);
		Style.SetHovered(Hovered);
		Style.SetPressed(Pressed);
		Style.SetDisabled(Disabled);
		return Style;
	}

	UTextBlock* MakeText(
		UWidgetTree* WidgetTree,
		const FText& Text,
		const int32 FontSize,
		const FLinearColor& Color = FLinearColor(0.12f, 0.09f, 0.06f, 1.0f),
		const FName Name = NAME_None)
	{
		if (!WidgetTree)
		{
			return nullptr;
		}
		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		TextBlock->SetText(Text);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetAutoWrapText(true);
		TextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		return TextBlock;
	}

	void AddCanvasChild(
		UCanvasPanel* Canvas,
		UWidget* Child,
		const FVector2D& Position,
		const FVector2D& Size,
		const int32 ZOrder = 0)
	{
		if (!Canvas || !Child)
		{
			return;
		}
		if (UCanvasPanelSlot* ChildSlot = Canvas->AddChildToCanvas(Child))
		{
			ChildSlot->SetPosition(Position);
			ChildSlot->SetSize(Size);
			ChildSlot->SetZOrder(ZOrder);
		}
	}

	FString ResolveCardPortraitPath(const FGameXXKCardDefinition& Definition)
	{
		if (Definition.Owner == EGameXXKCardOwner::Hero)
		{
			return HeroPortraitTexturePath;
		}
		if (Definition.Owner != EGameXXKCardOwner::Profession)
		{
			return FString();
		}
		switch (Definition.Role)
		{
		case EGameXXKCharacterRole::Blade: return BladePortraitTexturePath;
		case EGameXXKCharacterRole::Guard: return GuardPortraitTexturePath;
		case EGameXXKCharacterRole::Healer: return HealerPortraitTexturePath;
		case EGameXXKCharacterRole::Hunter: return HunterPortraitTexturePath;
		case EGameXXKCharacterRole::Sorcerer: return SorcererPortraitTexturePath;
		case EGameXXKCharacterRole::FormationMaster: return FormationMasterPortraitTexturePath;
		default: return FString();
		}
	}

	FString OfferFallbackName(const EGameXXKRouteMerchantOfferKind Kind)
	{
		return Kind == EGameXXKRouteMerchantOfferKind::Relic ? TEXT("未知遗物") : TEXT("未知卡牌");
	}
}

void UGameXXKRouteMerchantOfferButton::Configure(
	UGameXXKRouteMerchantWidget* InOwner,
	const FName InOfferId,
	const bool bInPurchaseAction)
{
	Owner = InOwner;
	OfferId = InOfferId;
	bPurchaseAction = bInPurchaseAction;
	OnClicked.RemoveDynamic(this, &UGameXXKRouteMerchantOfferButton::HandleClicked);
	OnClicked.AddDynamic(this, &UGameXXKRouteMerchantOfferButton::HandleClicked);
}

void UGameXXKRouteMerchantOfferButton::HandleClicked()
{
	if (Owner && bPurchaseAction && !OfferId.IsNone())
	{
		Owner->PurchaseOffer(OfferId);
	}
}

void UGameXXKRouteMerchantReplacementButton::Configure(
	UGameXXKRouteMerchantWidget* InOwner,
	const FName InReplacementEntryId)
{
	Owner = InOwner;
	ReplacementEntryId = InReplacementEntryId;
	OnClicked.RemoveDynamic(this, &UGameXXKRouteMerchantReplacementButton::HandleClicked);
	OnClicked.AddDynamic(this, &UGameXXKRouteMerchantReplacementButton::HandleClicked);
}

void UGameXXKRouteMerchantReplacementButton::HandleClicked()
{
	if (Owner && !ReplacementEntryId.IsNone())
	{
		Owner->SelectReplacementEntry(ReplacementEntryId);
	}
}

TSharedRef<SWidget> UGameXXKRouteMerchantWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKRouteMerchantWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	RefreshFromState();
}

void UGameXXKRouteMerchantWidget::RefreshFromState()
{
	BuildProgrammaticLayout();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::RouteMerchant)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	FGameXXKRouteMerchantView View;
	FString ViewError;
	if (!Subsystem->GetRouteMerchantView(View, &ViewError))
	{
		LastActionError = ViewError;
		ApplyView(FGameXXKRouteMerchantView());
	}
	else
	{
		RestorePendingReplacementSelection(Subsystem, View);
		ApplyView(View);
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

bool UGameXXKRouteMerchantWidget::PurchaseOffer(const FName OfferId, const FName ReplacementEntryId)
{
	LastActionError.Reset();
	LastPurchaseResult = FGameXXKRouteMerchantPurchaseResult();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || OfferId.IsNone())
	{
		LastActionError = TEXT("商店购买缺少有效商品或运行状态。");
		UpdateLastActionErrorDisplay();
		return false;
	}

	const bool bPurchased = Subsystem->PurchaseRouteMerchant(OfferId, ReplacementEntryId, LastPurchaseResult);
	if (!bPurchased && !LastPurchaseResult.bRequiresReplacement)
	{
		LastActionError = LastPurchaseResult.FailureReason.IsEmpty()
			? TEXT("购买未能完成。")
			: LastPurchaseResult.FailureReason;
	}
	RefreshFromState();
	NotifyPlayerFlowStateChanged();
	return bPurchased || LastPurchaseResult.bRequiresReplacement;
}

bool UGameXXKRouteMerchantWidget::SelectReplacementEntry(const FName ReplacementEntryId)
{
	if (!LastPurchaseResult.bRequiresReplacement
		|| LastPurchaseResult.OfferId.IsNone()
		|| ReplacementEntryId.IsNone()
		|| !LastPurchaseResult.EligibleReplacementEntryIds.Contains(ReplacementEntryId))
	{
		LastActionError = TEXT("只能选择当前列表中的路线牌实例进行替换。");
		UpdateLastActionErrorDisplay();
		return false;
	}

	const FName PendingOfferId = LastPurchaseResult.OfferId;
	return PurchaseOffer(PendingOfferId, ReplacementEntryId);
}

bool UGameXXKRouteMerchantWidget::RefreshStock()
{
	LastActionError.Reset();
	LastPurchaseResult = FGameXXKRouteMerchantPurchaseResult();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Subsystem->RefreshRouteMerchant(&LastActionError))
	{
		if (LastActionError.IsEmpty())
		{
			LastActionError = TEXT("商店刷新未能完成。");
		}
		RefreshFromState();
		return false;
	}
	RefreshFromState();
	NotifyPlayerFlowStateChanged();
	return true;
}

bool UGameXXKRouteMerchantWidget::CancelPendingPurchase()
{
	LastActionError.Reset();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Subsystem->CancelPendingRouteMerchantPurchase(&LastActionError))
	{
		if (LastActionError.IsEmpty())
		{
			LastActionError = TEXT("没有可取消的待替换购买。");
		}
		RefreshFromState();
		return false;
	}
	LastPurchaseResult = FGameXXKRouteMerchantPurchaseResult();
	RefreshFromState();
	NotifyPlayerFlowStateChanged();
	return true;
}

bool UGameXXKRouteMerchantWidget::LeaveMerchant()
{
	LastActionError.Reset();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Subsystem->ResolveMerchantRouteNode())
	{
		LastActionError = TEXT("当前无法离开路线商店。");
		UpdateLastActionErrorDisplay();
		return false;
	}
	ClearTransientInteractionState();
	RefreshFromState();
	NotifyPlayerFlowStateChanged();
	return true;
}

void UGameXXKRouteMerchantWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("RouteMerchantWidgetTree"));
	}
	if (!WidgetTree || RootSafeArea)
	{
		return;
	}

	RootSafeArea = WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), TEXT("RouteMerchantSafeArea"));
	RootSafeArea->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = RootSafeArea;

	ResponsiveScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("RouteMerchantResponsiveScale"));
	ResponsiveScaleBox->SetStretch(EStretch::ScaleToFit);
	ResponsiveScaleBox->SetStretchDirection(EStretchDirection::Both);
	ResponsiveScaleBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RootSafeArea->AddChild(ResponsiveScaleBox);

	DesignSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RouteMerchantDesign1920x1080"));
	DesignSizeBox->SetWidthOverride(MerchantDesignResolution.X);
	DesignSizeBox->SetHeightOverride(MerchantDesignResolution.Y);
	DesignSizeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ResponsiveScaleBox->AddChild(DesignSizeBox);

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RouteMerchantRootCanvas"));
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	DesignSizeBox->AddChild(RootCanvas);

	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RouteMerchantBackdrop"));
	Backdrop->SetBrushColor(FLinearColor(0.055f, 0.047f, 0.035f, 0.96f));
	Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
	AddCanvasChild(RootCanvas, Backdrop, FVector2D::ZeroVector, MerchantDesignResolution, 0);

	const float MerchantColumnWidth = MerchantDesignResolution.X * MerchantColumnFraction;
	const float OffersColumnWidth = MerchantDesignResolution.X * OffersColumnFraction;
	UBorder* MerchantPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RouteMerchantLeftPanel"));
	MerchantPanel->SetBrush(MakeTextureBrush(PanelFrameTexturePath, FVector2D(MerchantColumnWidth - 46.0f, 982.0f), ESlateBrushDrawType::Box));
	MerchantPanel->SetBrushColor(FLinearColor::White);
	MerchantPanel->SetPadding(FMargin(36.0f, 44.0f));
	MerchantPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	AddCanvasChild(RootCanvas, MerchantPanel, FVector2D(24.0f, 48.0f), FVector2D(MerchantColumnWidth - 48.0f, 984.0f), 1);

	UVerticalBox* MerchantCopy = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RouteMerchantLeftCopy"));
	MerchantCopy->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	MerchantPanel->SetContent(MerchantCopy);
	UTextBlock* MerchantTitle = MakeText(WidgetTree, NSLOCTEXT("GameXXKRouteMerchant", "MerchantTitle", "山路行商"), 38, FLinearColor(0.12f, 0.08f, 0.04f, 1.0f));
	MerchantTitle->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* ChildSlot = MerchantCopy->AddChildToVerticalBox(MerchantTitle))
	{
		ChildSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 32.0f));
	}

	UBorder* MerchantPlaceholder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RouteMerchantPortraitPlaceholder"));
	MerchantPlaceholder->SetBrushColor(FLinearColor(0.76f, 0.67f, 0.49f, 0.34f));
	MerchantPlaceholder->SetPadding(FMargin(22.0f));
	MerchantPlaceholder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UTextBlock* PlaceholderCopy = MakeText(
		WidgetTree,
		NSLOCTEXT("GameXXKRouteMerchant", "MerchantPlaceholder", "行商形象占位\n\n山高路远，货随缘来。"),
		27,
		FLinearColor(0.24f, 0.16f, 0.08f, 1.0f));
	PlaceholderCopy->SetJustification(ETextJustify::Center);
	MerchantPlaceholder->SetVerticalAlignment(VAlign_Center);
	MerchantPlaceholder->SetContent(PlaceholderCopy);
	if (UVerticalBoxSlot* ChildSlot = MerchantCopy->AddChildToVerticalBox(MerchantPlaceholder))
	{
		ChildSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ChildSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 30.0f));
	}

	UTextBlock* MerchantExplanation = MakeText(
		WidgetTree,
		NSLOCTEXT("GameXXKRouteMerchant", "MerchantExplanation", "仅收本次路线的行旅钱。\n卡牌会进入临时路线牌组；遗物只在本次路线生效。\n货品售出不补，刷新会整批更换。"),
		21,
		FLinearColor(0.19f, 0.13f, 0.075f, 1.0f));
	MerchantExplanation->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* ChildSlot = MerchantCopy->AddChildToVerticalBox(MerchantExplanation))
	{
		ChildSlot->SetPadding(FMargin(6.0f, 12.0f));
	}

	LastActionErrorText = MakeText(
		WidgetTree,
		FText::GetEmpty(),
		18,
		FLinearColor(0.62f, 0.075f, 0.035f, 1.0f),
		TEXT("RouteMerchantLastActionError"));
	LastActionErrorText->SetJustification(ETextJustify::Center);
	LastActionErrorText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* ChildSlot = MerchantCopy->AddChildToVerticalBox(LastActionErrorText))
	{
		ChildSlot->SetPadding(FMargin(6.0f, 8.0f));
	}

	UCanvasPanel* OffersCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RouteMerchantOffersCanvas"));
	OffersCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	AddCanvasChild(RootCanvas, OffersCanvas, FVector2D(MerchantColumnWidth, 0.0f), FVector2D(OffersColumnWidth, MerchantDesignResolution.Y), 2);

	UTextBlock* Header = MakeText(WidgetTree, NSLOCTEXT("GameXXKRouteMerchant", "StockHeader", "途中货栈"), 34, FLinearColor(0.94f, 0.84f, 0.62f, 1.0f));
	AddCanvasChild(OffersCanvas, Header, FVector2D(58.0f, 24.0f), FVector2D(360.0f, 52.0f));
	RouteTravelMoneyText = MakeText(WidgetTree, FText::GetEmpty(), 30, FLinearColor(0.94f, 0.84f, 0.62f, 1.0f), TEXT("RouteMerchantTravelMoney"));
	RouteTravelMoneyText->SetJustification(ETextJustify::Right);
	AddCanvasChild(OffersCanvas, RouteTravelMoneyText, FVector2D(OffersColumnWidth - 430.0f, 24.0f), FVector2D(350.0f, 52.0f));

	CardOfferRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RouteMerchantCardRow"));
	CardOfferRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	AddCanvasChild(OffersCanvas, CardOfferRow, FVector2D(36.0f, 86.0f), FVector2D(OffersColumnWidth - 72.0f, 474.0f));
	for (int32 CardIndex = 0; CardIndex < MerchantCardSlotCount; ++CardIndex)
	{
		if (USizeBox* Cell = BuildOfferCell(EGameXXKRouteMerchantOfferKind::Card, CardIndex))
		{
			if (UHorizontalBoxSlot* ChildSlot = CardOfferRow->AddChildToHorizontalBox(Cell))
			{
				ChildSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				ChildSlot->SetHorizontalAlignment(HAlign_Center);
				ChildSlot->SetPadding(FMargin(12.0f, 0.0f));
			}
		}
	}

	RelicOfferRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RouteMerchantRelicRow"));
	RelicOfferRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	AddCanvasChild(OffersCanvas, RelicOfferRow, FVector2D(36.0f, 574.0f), FVector2D(OffersColumnWidth - 72.0f, 374.0f));
	for (int32 RelicIndex = 0; RelicIndex < MerchantRelicSlotCount; ++RelicIndex)
	{
		if (USizeBox* Cell = BuildOfferCell(EGameXXKRouteMerchantOfferKind::Relic, MerchantCardSlotCount + RelicIndex))
		{
			if (UHorizontalBoxSlot* ChildSlot = RelicOfferRow->AddChildToHorizontalBox(Cell))
			{
				ChildSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				ChildSlot->SetHorizontalAlignment(HAlign_Center);
				ChildSlot->SetPadding(FMargin(12.0f, 0.0f));
			}
		}
	}

	UHorizontalBox* BottomActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RouteMerchantBottomActions"));
	BottomActions->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	AddCanvasChild(OffersCanvas, BottomActions, FVector2D(OffersColumnWidth - 930.0f, 976.0f), FVector2D(850.0f, 64.0f), 5);

	auto AddActionButton = [this, BottomActions](UButton*& OutButton, UTextBlock*& OutLabel, const FName Name, const FText& InitialText)
	{
		OutButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		OutButton->SetStyle(MakeTextureButtonStyle(ActionButtonTexturePath, FVector2D(250.0f, 58.0f), true, FMargin(5.0f / 73.0f, 5.0f / 31.0f)));
		OutLabel = MakeText(WidgetTree, InitialText, 21, FLinearColor(0.16f, 0.10f, 0.045f, 1.0f));
		OutLabel->SetJustification(ETextJustify::Center);
		OutButton->SetContent(OutLabel);
		if (UHorizontalBoxSlot* ChildSlot = BottomActions->AddChildToHorizontalBox(OutButton))
		{
			ChildSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			ChildSlot->SetPadding(FMargin(10.0f, 2.0f));
		}
	};

	UButton* RefreshRaw = nullptr;
	UTextBlock* RefreshLabelRaw = nullptr;
	AddActionButton(RefreshRaw, RefreshLabelRaw, TEXT("RouteMerchantRefreshButton"), FText::GetEmpty());
	RefreshButton = RefreshRaw;
	RefreshButtonText = RefreshLabelRaw;
	RefreshButton->OnClicked.AddDynamic(this, &UGameXXKRouteMerchantWidget::HandleRefreshClicked);

	UButton* CancelRaw = nullptr;
	UTextBlock* CancelLabelRaw = nullptr;
	AddActionButton(CancelRaw, CancelLabelRaw, TEXT("RouteMerchantCancelButton"), NSLOCTEXT("GameXXKRouteMerchant", "CancelPending", "取消替换"));
	CancelButton = CancelRaw;
	CancelButtonText = CancelLabelRaw;
	CancelButton->OnClicked.AddDynamic(this, &UGameXXKRouteMerchantWidget::HandleCancelClicked);
	CancelButton->SetVisibility(ESlateVisibility::Collapsed);

	UButton* LeaveRaw = nullptr;
	UTextBlock* LeaveLabelRaw = nullptr;
	AddActionButton(LeaveRaw, LeaveLabelRaw, TEXT("RouteMerchantLeaveButton"), NSLOCTEXT("GameXXKRouteMerchant", "Leave", "离开"));
	LeaveButton = LeaveRaw;
	LeaveButtonText = LeaveLabelRaw;
	LeaveButton->OnClicked.AddDynamic(this, &UGameXXKRouteMerchantWidget::HandleLeaveClicked);

	ReplacementSelectionPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("RouteMerchantReplacementSelectionPanel"));
	ReplacementSelectionPanel->SetBrush(MakeTextureBrush(
		PanelFrameTexturePath,
		FVector2D(OffersColumnWidth - 180.0f, 790.0f),
		ESlateBrushDrawType::Box));
	ReplacementSelectionPanel->SetBrushColor(FLinearColor(0.94f, 0.88f, 0.72f, 0.99f));
	ReplacementSelectionPanel->SetPadding(FMargin(42.0f, 34.0f));
	ReplacementSelectionPanel->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvasChild(
		OffersCanvas,
		ReplacementSelectionPanel,
		FVector2D(90.0f, 104.0f),
		FVector2D(OffersColumnWidth - 180.0f, 790.0f),
		20);

	UVerticalBox* ReplacementStack = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("RouteMerchantReplacementSelectionStack"));
	ReplacementStack->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ReplacementSelectionPanel->SetContent(ReplacementStack);
	UTextBlock* ReplacementTitle = MakeText(
		WidgetTree,
		NSLOCTEXT("GameXXKRouteMerchant", "ReplacementTitle", "路线牌已满 · 选择一张替换"),
		32,
		FLinearColor(0.15f, 0.09f, 0.04f, 1.0f));
	ReplacementTitle->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* ChildSlot = ReplacementStack->AddChildToVerticalBox(ReplacementTitle))
	{
		ChildSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}
	UTextBlock* ReplacementExplanation = MakeText(
		WidgetTree,
		NSLOCTEXT("GameXXKRouteMerchant", "ReplacementExplanation", "选择后才会一次性扣除行旅钱并售出商品；取消不会扣款。"),
		19,
		FLinearColor(0.26f, 0.16f, 0.07f, 1.0f));
	ReplacementExplanation->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* ChildSlot = ReplacementStack->AddChildToVerticalBox(ReplacementExplanation))
	{
		ChildSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 22.0f));
	}
	ReplacementSelectionGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(
		UUniformGridPanel::StaticClass(),
		TEXT("RouteMerchantReplacementSelectionGrid"));
	ReplacementSelectionGrid->SetSlotPadding(FMargin(8.0f));
	ReplacementSelectionGrid->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UVerticalBoxSlot* ChildSlot = ReplacementStack->AddChildToVerticalBox(ReplacementSelectionGrid))
	{
		ChildSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ChildSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	RenderedOfferIds.SetNum(MerchantOfferSlotCount);
	OfferTooltips.SetNum(MerchantOfferSlotCount);
	OfferDisabledReasons.SetNum(MerchantOfferSlotCount);
	SetVisibility(ESlateVisibility::Collapsed);
}

USizeBox* UGameXXKRouteMerchantWidget::BuildOfferCell(
	const EGameXXKRouteMerchantOfferKind Kind,
	const int32 GlobalOfferIndex)
{
	if (!WidgetTree || GlobalOfferIndex < 0 || GlobalOfferIndex >= MerchantOfferSlotCount)
	{
		return nullptr;
	}
	const bool bCard = Kind == EGameXXKRouteMerchantOfferKind::Card;
	const FVector2D VisualSize = bCard ? MerchantCardFrameSize : MerchantRelicFrameSize;

	USizeBox* Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("RouteMerchantOfferCell%d"), GlobalOfferIndex));
	Cell->SetWidthOverride(410.0f);
	Cell->SetHeightOverride(bCard ? 470.0f : 370.0f);
	Cell->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("RouteMerchantOfferStack%d"), GlobalOfferIndex));
	Stack->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Cell->AddChild(Stack);

	USizeBox* VisualBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("RouteMerchantOfferVisualSize%d"), GlobalOfferIndex));
	VisualBox->SetWidthOverride(VisualSize.X);
	VisualBox->SetHeightOverride(VisualSize.Y);
	VisualBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UVerticalBoxSlot* ChildSlot = Stack->AddChildToVerticalBox(VisualBox))
	{
		ChildSlot->SetHorizontalAlignment(HAlign_Center);
	}

	UGameXXKRouteMerchantOfferButton* DisplayButton = WidgetTree->ConstructWidget<UGameXXKRouteMerchantOfferButton>(
		UGameXXKRouteMerchantOfferButton::StaticClass(),
		*FString::Printf(TEXT("RouteMerchantOfferDisplay%d"), GlobalOfferIndex));
	DisplayButton->SetStyle(bCard
		? MakeTextureButtonStyle(CardFrameTexturePath, VisualSize, false)
		: MakeTextureButtonStyle(PanelFrameTexturePath, VisualSize, true));
	DisplayButton->Configure(this, NAME_None, false);
	VisualBox->AddChild(DisplayButton);
	OfferDisplayButtons.Add(DisplayButton);

	UOverlay* Face = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *FString::Printf(TEXT("RouteMerchantOfferFace%d"), GlobalOfferIndex));
	Face->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	DisplayButton->SetContent(Face);

	UImage* Art = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("RouteMerchantOfferArt%d"), GlobalOfferIndex));
	Art->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* ChildSlot = Face->AddChildToOverlay(Art))
	{
		ChildSlot->SetHorizontalAlignment(HAlign_Fill);
		ChildSlot->SetVerticalAlignment(VAlign_Fill);
		ChildSlot->SetPadding(bCard ? FMargin(34.0f, 34.0f, 34.0f, 92.0f) : FMargin(38.0f));
	}
	OfferArtImages.Add(Art);

	UBorder* TitleBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("RouteMerchantOfferTitleBar%d"), GlobalOfferIndex));
	TitleBar->SetPadding(FMargin(7.0f, 5.0f));
	TitleBar->SetBrushColor(FLinearColor(0.70f, 0.62f, 0.46f, 0.94f));
	TitleBar->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UOverlaySlot* ChildSlot = Face->AddChildToOverlay(TitleBar))
	{
		ChildSlot->SetHorizontalAlignment(HAlign_Fill);
		ChildSlot->SetVerticalAlignment(VAlign_Bottom);
		ChildSlot->SetPadding(bCard ? FMargin(25.0f, 0.0f, 25.0f, 26.0f) : FMargin(18.0f));
	}
	OfferTitleBars.Add(TitleBar);
	UTextBlock* NameText = MakeText(WidgetTree, FText::GetEmpty(), bCard ? 18 : 20, FLinearColor(0.09f, 0.065f, 0.035f, 1.0f));
	NameText->SetJustification(ETextJustify::Center);
	TitleBar->SetContent(NameText);
	OfferNameTexts.Add(NameText);

	UTextBlock* PriceText = MakeText(WidgetTree, FText::GetEmpty(), 19, FLinearColor(0.94f, 0.84f, 0.62f, 1.0f));
	PriceText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* ChildSlot = Stack->AddChildToVerticalBox(PriceText))
	{
		ChildSlot->SetPadding(FMargin(0.0f, 5.0f, 0.0f, 0.0f));
	}
	OfferPriceTexts.Add(PriceText);

	UTextBlock* StatusText = MakeText(WidgetTree, FText::GetEmpty(), 15, FLinearColor(0.78f, 0.70f, 0.56f, 1.0f));
	StatusText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* ChildSlot = Stack->AddChildToVerticalBox(StatusText))
	{
		ChildSlot->SetPadding(FMargin(6.0f, 1.0f, 6.0f, 3.0f));
	}
	OfferStatusTexts.Add(StatusText);

	UGameXXKRouteMerchantOfferButton* PurchaseButton = WidgetTree->ConstructWidget<UGameXXKRouteMerchantOfferButton>(
		UGameXXKRouteMerchantOfferButton::StaticClass(),
		*FString::Printf(TEXT("RouteMerchantOfferBuy%d"), GlobalOfferIndex));
	PurchaseButton->SetStyle(MakeTextureButtonStyle(ActionButtonTexturePath, FVector2D(210.0f, 48.0f), true, FMargin(5.0f / 73.0f, 5.0f / 31.0f)));
	PurchaseButton->Configure(this, NAME_None, true);
	UTextBlock* PurchaseText = MakeText(WidgetTree, NSLOCTEXT("GameXXKRouteMerchant", "Buy", "购买"), 19, FLinearColor(0.16f, 0.10f, 0.045f, 1.0f));
	PurchaseText->SetJustification(ETextJustify::Center);
	PurchaseButton->SetContent(PurchaseText);
	if (UVerticalBoxSlot* ChildSlot = Stack->AddChildToVerticalBox(PurchaseButton))
	{
		ChildSlot->SetHorizontalAlignment(HAlign_Center);
		ChildSlot->SetPadding(FMargin(72.0f, 0.0f));
	}
	OfferPurchaseButtons.Add(PurchaseButton);
	OfferPurchaseTexts.Add(PurchaseText);
	return Cell;
}

void UGameXXKRouteMerchantWidget::ApplyView(const FGameXXKRouteMerchantView& View)
{
	CachedView = View;
	if (RouteTravelMoneyText)
	{
		RouteTravelMoneyText->SetText(FText::Format(
			NSLOCTEXT("GameXXKRouteMerchant", "TravelMoney", "行旅钱  {0}"),
			FText::AsNumber(View.RouteTravelMoney)));
	}
	if (RefreshButtonText)
	{
		RefreshButtonText->SetText(FText::Format(
			NSLOCTEXT("GameXXKRouteMerchant", "RefreshPrice", "刷新  {0}"),
			FText::AsNumber(View.RefreshCost)));
	}
	if (RefreshButton)
	{
		RefreshButton->SetIsEnabled(View.bRefreshEnabled);
		RefreshButton->SetToolTipText(View.bRefreshEnabled
			? NSLOCTEXT("GameXXKRouteMerchant", "RefreshTooltip", "更换全部六件货品。下一次刷新会按路线规则涨价。")
			: FText::FromString(View.RefreshDisabledReason));
	}
	if (CancelButton)
	{
		CancelButton->SetVisibility(View.bHasPendingReplacement ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		CancelButton->SetIsEnabled(View.bHasPendingReplacement);
	}
	if (LeaveButton)
	{
		LeaveButton->SetIsEnabled(View.bCanLeave);
		LeaveButton->SetToolTipText(NSLOCTEXT("GameXXKRouteMerchant", "LeaveTooltip", "离开并完成当前商人节点。待替换购买会先取消且不扣款。"));
	}

	for (int32 Index = 0; Index < MerchantCardSlotCount; ++Index)
	{
		ApplyOffer(Index, View.CardOffers.IsValidIndex(Index) ? &View.CardOffers[Index] : nullptr, EGameXXKRouteMerchantOfferKind::Card);
	}
	for (int32 Index = 0; Index < MerchantRelicSlotCount; ++Index)
	{
		ApplyOffer(
			MerchantCardSlotCount + Index,
			View.RelicOffers.IsValidIndex(Index) ? &View.RelicOffers[Index] : nullptr,
			EGameXXKRouteMerchantOfferKind::Relic);
	}
	ApplyReplacementSelection(View);
	UpdateLastActionErrorDisplay();
}

void UGameXXKRouteMerchantWidget::RestorePendingReplacementSelection(
	const UGameXXKMVPSubsystem* Subsystem,
	const FGameXXKRouteMerchantView& View)
{
	if (!View.bHasPendingReplacement
		|| (LastPurchaseResult.bRequiresReplacement
			&& !LastPurchaseResult.OfferId.IsNone()
			&& !LastPurchaseResult.EligibleReplacementEntryIds.IsEmpty()))
	{
		return;
	}
	if (!Subsystem)
	{
		return;
	}

	const FGameXXKPendingRouteMerchantPurchase& Pending =
		Subsystem->GetRuntimeState().CardRun.RouteMerchant.PendingPurchase;
	FGameXXKRouteMerchantPurchasePreview Preview;
	FString PreviewError;
	if (!Pending.bActive
		|| Pending.OfferId.IsNone()
		|| !Subsystem->PreviewRouteMerchantPurchase(Pending.OfferId, NAME_None, Preview, &PreviewError)
		|| !Preview.bRequiresReplacement
		|| Preview.EligibleReplacementEntryIds.IsEmpty())
	{
		if (LastActionError.IsEmpty())
		{
			LastActionError = PreviewError.IsEmpty()
				? TEXT("无法恢复待替换购买的候选路线牌。")
				: PreviewError;
		}
		return;
	}

	FGameXXKRouteMerchantPurchaseResult RecoveredResult;
	RecoveredResult.bRequiresReplacement = true;
	RecoveredResult.Offer = Preview.Offer;
	RecoveredResult.OfferId = Preview.Offer.OfferId;
	RecoveredResult.CardId = Preview.Offer.ContentId;
	RecoveredResult.BalanceBefore = Preview.BalanceBefore;
	RecoveredResult.BalanceAfter = Preview.BalanceAfter;
	RecoveredResult.Price = Preview.Price;
	RecoveredResult.MergeSurvivorEntryId = Preview.MergeSurvivorEntryId;
	RecoveredResult.ConsumedEntryIds = Preview.ConsumedEntryIds;
	RecoveredResult.FinalQuality = Preview.FinalQuality;
	RecoveredResult.TemporaryCountDelta = Preview.TemporaryCountDelta;
	RecoveredResult.CapacityDelta = Preview.CapacityDelta;
	RecoveredResult.EligibleReplacementEntryIds = Preview.EligibleReplacementEntryIds;
	RecoveredResult.Failure = Preview.Failure;
	RecoveredResult.FailureReason = Preview.FailureReason;
	LastPurchaseResult = MoveTemp(RecoveredResult);
}

void UGameXXKRouteMerchantWidget::ApplyReplacementSelection(const FGameXXKRouteMerchantView& View)
{
	RenderedReplacementEntryIds.Reset();
	ReplacementSelectionButtons.Reset();
	if (ReplacementSelectionGrid)
	{
		ReplacementSelectionGrid->ClearChildren();
	}

	const bool bShowReplacementSelection = View.bHasPendingReplacement
		&& LastPurchaseResult.bRequiresReplacement
		&& !LastPurchaseResult.OfferId.IsNone()
		&& !LastPurchaseResult.EligibleReplacementEntryIds.IsEmpty();
	if (ReplacementSelectionPanel)
	{
		ReplacementSelectionPanel->SetVisibility(
			bShowReplacementSelection
				? ESlateVisibility::SelfHitTestInvisible
				: ESlateVisibility::Collapsed);
	}
	if (!bShowReplacementSelection || !WidgetTree || !ReplacementSelectionGrid)
	{
		return;
	}

	for (const FName ReplacementEntryId : LastPurchaseResult.EligibleReplacementEntryIds)
	{
		if (ReplacementEntryId.IsNone() || RenderedReplacementEntryIds.Contains(ReplacementEntryId))
		{
			continue;
		}
		const int32 ChoiceIndex = RenderedReplacementEntryIds.Add(ReplacementEntryId);
		UGameXXKRouteMerchantReplacementButton* ChoiceButton = WidgetTree->ConstructWidget<UGameXXKRouteMerchantReplacementButton>(
			UGameXXKRouteMerchantReplacementButton::StaticClass(),
			*FString::Printf(TEXT("RouteMerchantReplacementChoice%d"), ChoiceIndex));
		ChoiceButton->SetStyle(MakeTextureButtonStyle(
			ActionButtonTexturePath,
			FVector2D(340.0f, 112.0f),
			true,
			FMargin(5.0f / 73.0f, 5.0f / 31.0f)));
		ChoiceButton->Configure(this, ReplacementEntryId);
		ChoiceButton->SetToolTipText(FText::FromString(FString::Printf(
			TEXT("替换路线牌实例：%s"),
			*ReplacementEntryId.ToString())));

		UTextBlock* ChoiceLabel = MakeText(
			WidgetTree,
			BuildReplacementEntryLabel(ReplacementEntryId),
			18,
			FLinearColor(0.16f, 0.10f, 0.045f, 1.0f));
		ChoiceLabel->SetJustification(ETextJustify::Center);
		ChoiceButton->SetContent(ChoiceLabel);
		if (UUniformGridSlot* ChoiceSlot = ReplacementSelectionGrid->AddChildToUniformGrid(
			ChoiceButton,
			ChoiceIndex / 3,
			ChoiceIndex % 3))
		{
			ChoiceSlot->SetHorizontalAlignment(HAlign_Fill);
			ChoiceSlot->SetVerticalAlignment(VAlign_Fill);
		}
		ReplacementSelectionButtons.Add(ChoiceButton);
	}
}

void UGameXXKRouteMerchantWidget::UpdateLastActionErrorDisplay()
{
	if (!LastActionErrorText)
	{
		return;
	}
	LastActionErrorText->SetText(FText::FromString(LastActionError));
	LastActionErrorText->SetVisibility(
		LastActionError.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
}

FText UGameXXKRouteMerchantWidget::BuildReplacementEntryLabel(const FName ReplacementEntryId) const
{
	return FText::FromString(ReplacementEntryId.ToString());
}

void UGameXXKRouteMerchantWidget::ApplyOffer(
	const int32 GlobalOfferIndex,
	const FGameXXKRouteMerchantOfferView* OfferView,
	const EGameXXKRouteMerchantOfferKind ExpectedKind)
{
	if (!OfferDisplayButtons.IsValidIndex(GlobalOfferIndex)
		|| !OfferPurchaseButtons.IsValidIndex(GlobalOfferIndex)
		|| !OfferNameTexts.IsValidIndex(GlobalOfferIndex)
		|| !OfferPriceTexts.IsValidIndex(GlobalOfferIndex)
		|| !OfferStatusTexts.IsValidIndex(GlobalOfferIndex)
		|| !OfferPurchaseTexts.IsValidIndex(GlobalOfferIndex))
	{
		return;
	}

	const FGameXXKRouteMerchantOffer* Offer = OfferView ? &OfferView->SavedOffer : nullptr;
	const bool bUnavailable = !Offer || Offer->bUnavailable || Offer->ContentId.IsNone();
	const FName OfferId = Offer ? Offer->OfferId : NAME_None;
	const FString DisabledReason = ResolveDisabledReason(OfferView);
	RenderedOfferIds[GlobalOfferIndex] = OfferId;
	OfferDisabledReasons[GlobalOfferIndex] = DisabledReason;

	UGameXXKRouteMerchantOfferButton* DisplayButton = OfferDisplayButtons[GlobalOfferIndex];
	UGameXXKRouteMerchantOfferButton* PurchaseButton = OfferPurchaseButtons[GlobalOfferIndex];
	DisplayButton->Configure(this, OfferId, false);
	DisplayButton->SetIsEnabled(true);
	PurchaseButton->Configure(this, OfferId, true);
	PurchaseButton->SetIsEnabled(OfferView && OfferView->bPurchaseEnabled);

	FString DisplayName = bUnavailable ? TEXT("暂无商品") : OfferFallbackName(ExpectedKind);
	FString ArtPath;
	UTexture2D* ArtTexture = nullptr;
	if (!bUnavailable && ExpectedKind == EGameXXKRouteMerchantOfferKind::Card)
	{
		if (const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(Offer->ContentId))
		{
			DisplayName = Definition->DisplayName.ToString();
			ArtPath = ResolveCardPortraitPath(*Definition);
			ArtTexture = ArtPath.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *ArtPath);
		}
	}
	else if (!bUnavailable && ExpectedKind == EGameXXKRouteMerchantOfferKind::Relic)
	{
		if (const FGameXXKRelicDefinition* Definition = FGameXXKRelicCatalog::FindDefinition(Offer->ContentId))
		{
			DisplayName = Definition->DisplayName.ToString();
			ArtTexture = Cast<UTexture2D>(Definition->IconTexturePath.TryLoad());
		}
	}

	OfferNameTexts[GlobalOfferIndex]->SetText(FText::FromString(DisplayName));
	OfferPriceTexts[GlobalOfferIndex]->SetText(bUnavailable
		? NSLOCTEXT("GameXXKRouteMerchant", "NoPrice", "价格  --")
		: FText::Format(NSLOCTEXT("GameXXKRouteMerchant", "OfferPrice", "价格  {0}"), FText::AsNumber(Offer->Price)));
	OfferStatusTexts[GlobalOfferIndex]->SetText(DisabledReason.IsEmpty()
		? NSLOCTEXT("GameXXKRouteMerchant", "Available", "可购买")
		: FText::FromString(DisabledReason));

	FText PurchaseLabel = NSLOCTEXT("GameXXKRouteMerchant", "Buy", "购买");
	if (bUnavailable)
	{
		PurchaseLabel = NSLOCTEXT("GameXXKRouteMerchant", "Unavailable", "不可用");
	}
	else if (Offer->bSold)
	{
		PurchaseLabel = NSLOCTEXT("GameXXKRouteMerchant", "Sold", "已售出");
	}
	else if (!OfferView->bAffordable)
	{
		PurchaseLabel = NSLOCTEXT("GameXXKRouteMerchant", "Insufficient", "行旅钱不足");
	}
	OfferPurchaseTexts[GlobalOfferIndex]->SetText(PurchaseLabel);

	if (OfferTitleBars.IsValidIndex(GlobalOfferIndex))
	{
		FLinearColor QualityColor = bUnavailable
			? FLinearColor(0.38f, 0.35f, 0.30f, 0.90f)
			: FGameXXKCardQualityRules::GetDisplayColor(Offer->Quality);
		QualityColor.A = 0.94f;
		OfferTitleBars[GlobalOfferIndex]->SetBrushColor(QualityColor);
	}
	if (OfferArtImages.IsValidIndex(GlobalOfferIndex))
	{
		UImage* Art = OfferArtImages[GlobalOfferIndex];
		if (ArtTexture)
		{
			Art->SetBrushFromTexture(ArtTexture, true);
			Art->SetColorAndOpacity(Offer && Offer->bSold ? FLinearColor(0.36f, 0.36f, 0.36f, 0.72f) : FLinearColor::White);
			Art->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Art->SetBrush(FSlateBrush());
			Art->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	const FText Tooltip = BuildOfferTooltip(OfferView, ExpectedKind, DisabledReason);
	OfferTooltips[GlobalOfferIndex] = Tooltip;
	DisplayButton->SetToolTipText(Tooltip);
	PurchaseButton->SetToolTipText(Tooltip);
}

FText UGameXXKRouteMerchantWidget::BuildOfferTooltip(
	const FGameXXKRouteMerchantOfferView* OfferView,
	const EGameXXKRouteMerchantOfferKind ExpectedKind,
	const FString& DisabledReason) const
{
	if (!OfferView || OfferView->SavedOffer.bUnavailable || OfferView->SavedOffer.ContentId.IsNone())
	{
		return FText::FromString(FString::Printf(
			TEXT("暂无商品\n%s"),
			DisabledReason.IsEmpty() ? TEXT("本格当前不可用。") : *DisabledReason));
	}

	const FGameXXKRouteMerchantOffer& Offer = OfferView->SavedOffer;
	if (ExpectedKind == EGameXXKRouteMerchantOfferKind::Card)
	{
		if (const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(Offer.ContentId))
		{
			FGameXXKCardTooltipContext Context;
			Context.InteractionResult = FString::Printf(TEXT("价格：%d 行旅钱"), Offer.Price);
			Context.UnavailableReason = DisabledReason;
			return FText::FromString(GameXXKCardText::DescribeTooltip(*Definition, Offer.Quality, nullptr, Context));
		}
	}
	else if (const FGameXXKRelicDefinition* Definition = FGameXXKRelicCatalog::FindDefinition(Offer.ContentId))
	{
		const FString Quality = FGameXXKCardQualityRules::GetDisplayName(Offer.Quality).ToString();
		return FText::FromString(FString::Printf(
			TEXT("%s · %s\n%s\n价格：%d 行旅钱%s%s"),
			*Definition->DisplayName.ToString(),
			*Quality,
			*Definition->Description.ToString(),
			Offer.Price,
			DisabledReason.IsEmpty() ? TEXT("") : TEXT("\n"),
			*DisabledReason));
	}

	return FText::FromString(FString::Printf(
		TEXT("%s\n价格：%d 行旅钱%s%s"),
		*Offer.ContentId.ToString(),
		Offer.Price,
		DisabledReason.IsEmpty() ? TEXT("") : TEXT("\n"),
		*DisabledReason));
}

FString UGameXXKRouteMerchantWidget::ResolveDisabledReason(const FGameXXKRouteMerchantOfferView* OfferView) const
{
	if (!OfferView)
	{
		return TEXT("本格当前不可用。");
	}
	if (!OfferView->DisabledReason.IsEmpty())
	{
		return OfferView->DisabledReason;
	}
	if (OfferView->SavedOffer.bUnavailable)
	{
		return TEXT("本格当前没有可售商品。");
	}
	if (OfferView->SavedOffer.bSold)
	{
		return TEXT("这件商品已经售出。");
	}
	if (!OfferView->bAffordable)
	{
		return TEXT("行旅钱不足。");
	}
	if (!OfferView->bPurchaseEnabled)
	{
		return TEXT("当前不能购买这件商品。");
	}
	return FString();
}

void UGameXXKRouteMerchantWidget::ClearTransientInteractionState()
{
	LastPurchaseResult = FGameXXKRouteMerchantPurchaseResult();
	LastActionError.Reset();
	for (UGameXXKRouteMerchantOfferButton* Button : OfferDisplayButtons)
	{
		if (Button)
		{
			Button->SetToolTipText(FText::GetEmpty());
		}
	}
	for (UGameXXKRouteMerchantOfferButton* Button : OfferPurchaseButtons)
	{
		if (Button)
		{
			Button->SetToolTipText(FText::GetEmpty());
		}
	}
	OfferTooltips.Init(FText::GetEmpty(), MerchantOfferSlotCount);
	RenderedReplacementEntryIds.Reset();
	ReplacementSelectionButtons.Reset();
	if (ReplacementSelectionGrid)
	{
		ReplacementSelectionGrid->ClearChildren();
	}
	if (ReplacementSelectionPanel)
	{
		ReplacementSelectionPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	UpdateLastActionErrorDisplay();
}

void UGameXXKRouteMerchantWidget::HandleRefreshClicked()
{
	RefreshStock();
}

void UGameXXKRouteMerchantWidget::HandleCancelClicked()
{
	CancelPendingPurchase();
}

void UGameXXKRouteMerchantWidget::HandleLeaveClicked()
{
	LeaveMerchant();
}

FVector2D UGameXXKRouteMerchantWidget::GetDesignResolutionForTest() const
{
	return MerchantDesignResolution;
}

float UGameXXKRouteMerchantWidget::GetMerchantColumnFractionForTest() const
{
	return MerchantColumnFraction;
}

float UGameXXKRouteMerchantWidget::GetOffersColumnFractionForTest() const
{
	return OffersColumnFraction;
}

FVector2D UGameXXKRouteMerchantWidget::GetCardFrameSizeForTest() const
{
	return MerchantCardFrameSize;
}

FVector2D UGameXXKRouteMerchantWidget::GetRelicFrameSizeForTest() const
{
	return MerchantRelicFrameSize;
}

FString UGameXXKRouteMerchantWidget::GetCardFrameResourcePathForTest() const
{
	return CardFrameTexturePath;
}

int32 UGameXXKRouteMerchantWidget::GetRenderedCardOfferCountForTest() const
{
	return CachedView.CardOffers.Num();
}

int32 UGameXXKRouteMerchantWidget::GetRenderedRelicOfferCountForTest() const
{
	return CachedView.RelicOffers.Num();
}

int32 UGameXXKRouteMerchantWidget::GetOfferTooltipCountForTest() const
{
	int32 Count = 0;
	for (const FText& Tooltip : OfferTooltips)
	{
		if (!Tooltip.IsEmpty())
		{
			++Count;
		}
	}
	return Count;
}

bool UGameXXKRouteMerchantWidget::HasOnlyButtonHitTargetsForTest() const
{
	if (!WidgetTree)
	{
		return false;
	}
	bool bOnlyButtonsHitTestable = true;
	WidgetTree->ForEachWidget([&bOnlyButtonsHitTestable](UWidget* Widget)
	{
		if (Widget && Widget->GetVisibility() == ESlateVisibility::Visible && !Widget->IsA<UButton>())
		{
			bOnlyButtonsHitTestable = false;
		}
	});
	return bOnlyButtonsHitTestable;
}

bool UGameXXKRouteMerchantWidget::IsOfferPurchaseEnabledForTest(const FName OfferId) const
{
	const int32 Index = RenderedOfferIds.IndexOfByKey(OfferId);
	return OfferPurchaseButtons.IsValidIndex(Index) && OfferPurchaseButtons[Index] && OfferPurchaseButtons[Index]->GetIsEnabled();
}

FString UGameXXKRouteMerchantWidget::GetOfferDisabledReasonForTest(const FName OfferId) const
{
	const int32 Index = RenderedOfferIds.IndexOfByKey(OfferId);
	return OfferDisabledReasons.IsValidIndex(Index) ? OfferDisabledReasons[Index] : FString();
}

FText UGameXXKRouteMerchantWidget::GetRouteTravelMoneyTextForTest() const
{
	return RouteTravelMoneyText ? RouteTravelMoneyText->GetText() : FText::GetEmpty();
}

FText UGameXXKRouteMerchantWidget::GetRefreshButtonTextForTest() const
{
	return RefreshButtonText ? RefreshButtonText->GetText() : FText::GetEmpty();
}

FText UGameXXKRouteMerchantWidget::GetLeaveButtonTextForTest() const
{
	return LeaveButtonText ? LeaveButtonText->GetText() : FText::GetEmpty();
}

FGameXXKRouteMerchantPurchaseResult UGameXXKRouteMerchantWidget::GetLastPurchaseResultForTest() const
{
	return LastPurchaseResult;
}

FString UGameXXKRouteMerchantWidget::GetLastActionErrorForTest() const
{
	return LastActionError;
}

FText UGameXXKRouteMerchantWidget::GetDisplayedLastActionErrorForTest() const
{
	return LastActionErrorText ? LastActionErrorText->GetText() : FText::GetEmpty();
}

TArray<FName> UGameXXKRouteMerchantWidget::GetEligibleReplacementEntryIdsForTest() const
{
	return RenderedReplacementEntryIds;
}

bool UGameXXKRouteMerchantWidget::IsReplacementSelectionVisibleForTest() const
{
	return ReplacementSelectionPanel
		&& ReplacementSelectionPanel->GetVisibility() != ESlateVisibility::Collapsed;
}
