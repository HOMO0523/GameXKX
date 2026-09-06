#include "UI/GameXXKRouteMerchantWidget.h"
#include "UI/GameXXKInRunUiStyle.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
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
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKCardText.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKRelicCatalog.h"
#include "Guide/GameXXKGuideTargetRegistry.h"
#include "Framework/Application/SlateApplication.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKCardTooltipWidget.h"

namespace
{
	const FVector2D MerchantDesignResolution(1920.0f, 1080.0f);
	const FVector2D MerchantCardFrameSize(206.0f, 285.0f);
	const FVector2D MerchantRelicFrameSize(300.0f, 104.0f);
	constexpr float MerchantColumnFraction = 0.0f;
	constexpr float OffersColumnFraction = 1.0f;
	constexpr int32 MerchantCardSlotCount = 4;
	constexpr int32 MerchantRelicSlotCount = 4;
	constexpr int32 MerchantOfferSlotCount = MerchantCardSlotCount + MerchantRelicSlotCount;

	static constexpr const TCHAR* CardFrameTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CardFrame.T_MasterV2_CardFrame");
	static constexpr const TCHAR* RelicFrameTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot.T_MasterV2_ItemSlot");
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
		FSlateFontInfo Font = FGameXXKInRunUiStyle::Font(FontSize, true, FontSize >= 20);
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
		if (Definition.Owner == EGameXXKCardOwner::QuestNpc)
		{
			const FName NpcId = Definition.NpcId.IsNone() ? Definition.OwnerId : Definition.NpcId;
			if (FGameXXKCompanionCatalog::FindQuestNpcDefinition(NpcId))
			{
				const FString Suffix = NpcId.ToString().RightChop(4);
				return FString::Printf(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_%s.T_CardPortrait_Npc_%s"),*Suffix,*Suffix);
			}
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

	FString OfferFallbackName()
	{
		return TEXT("未知卡牌");
	}

	FText QuestNpcFriendlyName(const FName NpcId)
	{
		if (NpcId == TEXT("Npc.TusiChief")) return NSLOCTEXT("GameXXKRouteMerchant", "NpcTusiChief", "土司首领");
		if (NpcId == TEXT("Npc.SongJinBao")) return NSLOCTEXT("GameXXKRouteMerchant", "NpcSongJinBao", "宋金宝");
		if (NpcId == TEXT("Npc.YueBai")) return NSLOCTEXT("GameXXKRouteMerchant", "NpcYueBai", "月白");
		if (NpcId == TEXT("Npc.ZhouGuangZu")) return NSLOCTEXT("GameXXKRouteMerchant", "NpcZhouGuangZu", "周光祖");
		if (NpcId == TEXT("Npc.JinGui")) return NSLOCTEXT("GameXXKRouteMerchant", "NpcJinGui", "金贵");
		if (NpcId == TEXT("Npc.QiongMeiEr")) return NSLOCTEXT("GameXXKRouteMerchant", "NpcQiongMeiEr", "琼梅儿");
		return NSLOCTEXT("GameXXKRouteMerchant", "QuestNpcFallback", "同行角色");
	}

	bool ContainsChineseText(const FString& Text)
	{
		for (const TCHAR Character : Text)
		{
			const uint32 CodePoint = static_cast<uint32>(Character);
			if ((CodePoint >= 0x3400U && CodePoint <= 0x4DBFU)
				|| (CodePoint >= 0x4E00U && CodePoint <= 0x9FFFU))
			{
				return true;
			}
		}
		return false;
	}

	FString LocalizeMerchantRuleError(const FString& RawError, const TCHAR* Fallback)
	{
		if (RawError.IsEmpty())
		{
			return Fallback;
		}
		if (ContainsChineseText(RawError))
		{
			return RawError;
		}
		auto Contains = [&RawError](const TCHAR* Pattern)
		{
			return RawError.Contains(Pattern, ESearchCase::IgnoreCase);
		};
		if (Contains(TEXT("no longer carries")) || Contains(TEXT("card no longer carried")))
		{
			return TEXT("持牌角色已不再携带这张卡牌，请刷新商店。");
		}
		if (Contains(TEXT("no longer deployed")) || Contains(TEXT("owner no longer deployed")))
		{
			return TEXT("持牌角色已不在当前队伍中，请刷新商店。");
		}
		if (Contains(TEXT("quality changed")) || Contains(TEXT("stale card quality")))
		{
			return TEXT("卡牌品质已经变化，请刷新商店。");
		}
		if (Contains(TEXT("reached Epic")) || Contains(TEXT("maximum quality")) || Contains(TEXT("max quality")))
		{
			return TEXT("这张卡牌已经达到最高品质。");
		}
		if (Contains(TEXT("already sold")))
		{
			return TEXT("这个强化名额已售出。");
		}
		if (Contains(TEXT("ordinary gold")) || Contains(TEXT("not enough gold")))
		{
			return TEXT("金币不足，无法完成本次操作。");
		}
		if (Contains(TEXT("No upgradable")) || Contains(TEXT("No unsold"))
			|| Contains(TEXT("available to refresh")) || Contains(TEXT("no refresh")))
		{
			return TEXT("当前没有可强化或刷新的卡牌。");
		}
		if (Contains(TEXT("stale or unknown")) || Contains(TEXT("changed before commit")))
		{
			return TEXT("商品状态已经变化，请刷新商店后重试。");
		}
		if (Contains(TEXT("active locked generated route"))
			|| Contains(TEXT("pending route node"))
			|| Contains(TEXT("not a generated merchant node"))
			|| Contains(TEXT("invalid route context")))
		{
			return TEXT("当前不在有效的路线商店中。");
		}
		if (Contains(TEXT("saved merchant")) || Contains(TEXT("merchant stock"))
			|| Contains(TEXT("persisted metadata")) || Contains(TEXT("pending merchant purchase")))
		{
			return TEXT("商店数据已经变化，请重新进入商店。");
		}
		return Fallback;
	}

	FString LocalizePurchaseFailure(const FGameXXKRouteMerchantPurchaseResult& Result)
	{
		switch (Result.Failure)
		{
		case EGameXXKRouteMerchantPurchaseFailure::InvalidRouteContext:
			return TEXT("当前不在有效的路线商店中。");
		case EGameXXKRouteMerchantPurchaseFailure::InvalidMerchantStock:
		case EGameXXKRouteMerchantPurchaseFailure::PendingPurchaseConflict:
			return TEXT("商店数据已经变化，请重新进入商店。");
		case EGameXXKRouteMerchantPurchaseFailure::StaleOfferId:
			return TEXT("商品状态已经变化，请刷新商店后重试。");
		case EGameXXKRouteMerchantPurchaseFailure::OfferUnavailable:
			return TEXT("这个卡位当前没有可强化卡牌。");
		case EGameXXKRouteMerchantPurchaseFailure::OfferAlreadySold:
			return TEXT("这个强化名额已售出。");
		case EGameXXKRouteMerchantPurchaseFailure::InsufficientTravelMoney:
		case EGameXXKRouteMerchantPurchaseFailure::InsufficientOrdinaryGold:
			return TEXT("金币不足，无法购买本次强化。");
		case EGameXXKRouteMerchantPurchaseFailure::InvalidCardDefinition:
			return TEXT("卡牌资料暂不可用，请刷新商店。");
		case EGameXXKRouteMerchantPurchaseFailure::InvalidActiveCompanion:
		case EGameXXKRouteMerchantPurchaseFailure::OwnerNoLongerDeployed:
			return TEXT("持牌角色已不在当前队伍中，请刷新商店。");
		case EGameXXKRouteMerchantPurchaseFailure::CardNoLongerCarried:
			return TEXT("持牌角色已不再携带这张卡牌，请刷新商店。");
		case EGameXXKRouteMerchantPurchaseFailure::StaleCardQuality:
			return TEXT("卡牌品质已经变化，请刷新商店。");
		case EGameXXKRouteMerchantPurchaseFailure::CardAlreadyMaxQuality:
			return TEXT("这张卡牌已经达到最高品质。");
		case EGameXXKRouteMerchantPurchaseFailure::ArithmeticOverflow:
			return TEXT("金币计算异常，本次购买未扣款。");
		case EGameXXKRouteMerchantPurchaseFailure::DuplicateRelic:
		case EGameXXKRouteMerchantPurchaseFailure::InvalidRouteCardOrdinal:
		case EGameXXKRouteMerchantPurchaseFailure::DeckAcquisitionRejected:
		case EGameXXKRouteMerchantPurchaseFailure::InvalidReplacementEntryId:
		case EGameXXKRouteMerchantPurchaseFailure::RelicAcquisitionRejected:
			return TEXT("旧版商店操作已经失效，请刷新商店。");
		case EGameXXKRouteMerchantPurchaseFailure::None:
		default:
			return LocalizeMerchantRuleError(Result.FailureReason, TEXT("购买未能完成，请稍后重试。"));
		}
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

TSharedRef<SWidget> UGameXXKRouteMerchantWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	RegisterGuideTargets();
	return Super::RebuildWidget();
}

void UGameXXKRouteMerchantWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	RefreshFromState();
}

void UGameXXKRouteMerchantWidget::NativeDestruct()
{
	FGameXXKGuideTargetRegistry& Registry = FGameXXKGuideTargetRegistry::Get();
	Registry.UnregisterTarget(TEXT("Route.Merchant.CardRow"), CardOfferRow);
	Registry.UnregisterTarget(TEXT("Route.Merchant.RelicRow"), RelicOfferRow);
	Registry.UnregisterTarget(TEXT("Route.Merchant.Leave"), LeaveButton);
	Super::NativeDestruct();
}

void UGameXXKRouteMerchantWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const bool bShiftExpanded = UGameXXKCardTooltipWidget::IsPhysicalShiftDown();
	const bool bControlDown = UGameXXKCardTooltipWidget::IsPhysicalControlDown();
	const bool bEscapeDown = UGameXXKCardTooltipWidget::IsPhysicalEscapeDown();
	const bool bWindowActive = UGameXXKCardTooltipWidget::IsOwnerWindowActive(this);
	bCardTooltipShiftExpanded = bShiftExpanded;
	for (int32 Index = 0; Index < OfferCardTooltipWidgets.Num(); ++Index)
	{
		if (UGameXXKCardTooltipWidget* Tooltip = OfferCardTooltipWidgets[Index])
		{
			const UButton* Display = OfferDisplayButtons.IsValidIndex(Index) ? OfferDisplayButtons[Index].Get() : nullptr;
			const UButton* Buy = OfferPurchaseButtons.IsValidIndex(Index) ? OfferPurchaseButtons[Index].Get() : nullptr;
			const bool bHovered = (Display && Display->GetToolTip() == Tooltip && Display->IsHovered())
				|| (Buy && Buy->GetToolTip() == Tooltip && Buy->IsHovered());
			Tooltip->UpdateInspectionFromOwner(bWindowActive && bHovered, bShiftExpanded, bControlDown, bEscapeDown);
		}
	}
}

void UGameXXKRouteMerchantWidget::RefreshFromState()
{
	BuildProgrammaticLayout();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::RouteMerchant)
	{
		bGuideMerchantOpenedEmitted = false;
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	FGameXXKRouteMerchantView View;
	FString ViewError;
	if (!Subsystem->GetRouteMerchantView(View, &ViewError))
	{
		LastActionError = LocalizeMerchantRuleError(ViewError, TEXT("商店数据暂时无法读取。"));
		ApplyView(FGameXXKRouteMerchantView());
	}
	else
	{
		ApplyView(View);
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RegisterGuideTargets();
	if (!bGuideMerchantOpenedEmitted)
	{
		bGuideMerchantOpenedEmitted = true;
		FGameXXKGuideTargetRegistry::Get().EmitEvent(TEXT("Event.Merchant.Opened"));
	}
}

bool UGameXXKRouteMerchantWidget::PurchaseOffer(const FName OfferId)
{
	const int32 OfferIndex = RenderedOfferIds.IndexOfByKey(OfferId);
	const FName GuideActionId = OfferIndex >= 0 && OfferIndex < MerchantCardSlotCount
		? FName(TEXT("Action.Merchant.PurchaseCard"))
		: FName(TEXT("Action.Merchant.PurchaseRelic"));
	if (!FGameXXKGuideTargetRegistry::Get().IsActionAllowed(GuideActionId))
	{
		return false;
	}
	LastActionError.Reset();
	LastPurchaseResult = FGameXXKRouteMerchantPurchaseResult();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || OfferId.IsNone())
	{
		LastActionError = TEXT("商店购买缺少有效商品或运行状态。");
		UpdateLastActionErrorDisplay();
		return false;
	}

	const bool bPurchased = Subsystem->PurchaseRouteMerchant(OfferId, NAME_None, LastPurchaseResult);
	if (!bPurchased)
	{
		LastActionError = LocalizePurchaseFailure(LastPurchaseResult);
	}
	else
	{
		FGameXXKGuideTargetRegistry::Get().EmitEvent(
			OfferIndex >= 0 && OfferIndex < MerchantCardSlotCount
				? TEXT("Event.Merchant.CardPurchased")
				: TEXT("Event.Merchant.RelicPurchased"));
	}
	RefreshFromState();
	NotifyPlayerFlowStateChanged();
	return bPurchased;
}

bool UGameXXKRouteMerchantWidget::RefreshStock()
{
	LastActionError.Reset();
	LastPurchaseResult = FGameXXKRouteMerchantPurchaseResult();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	FString RefreshError;
	if (!Subsystem || !Subsystem->RefreshRouteMerchant(&RefreshError))
	{
		LastActionError = LocalizeMerchantRuleError(RefreshError, TEXT("商店刷新未能完成。"));
		RefreshFromState();
		NotifyPlayerFlowStateChanged();
		return false;
	}
	RefreshFromState();
	NotifyPlayerFlowStateChanged();
	return true;
}

bool UGameXXKRouteMerchantWidget::LeaveMerchant()
{
	if (!FGameXXKGuideTargetRegistry::Get().IsActionAllowed(TEXT("Action.Merchant.Leave")))
	{
		return false;
	}
	LastActionError.Reset();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Subsystem->ResolveMerchantRouteNode())
	{
		LastActionError = TEXT("当前无法离开路线商店。");
		UpdateLastActionErrorDisplay();
		return false;
	}
	ClearTransientInteractionState();
	FGameXXKGuideTargetRegistry::Get().EmitEvent(TEXT("Event.Merchant.Left"));
	RefreshFromState();
	NotifyPlayerFlowStateChanged();
	return true;
}

void UGameXXKRouteMerchantWidget::RegisterGuideTargets()
{
	FGameXXKGuideTargetRegistry& Registry = FGameXXKGuideTargetRegistry::Get();
	if (CardOfferRow)
	{
		Registry.RegisterWidgetTarget(TEXT("Route.Merchant.CardRow"), CardOfferRow);
	}
	if (RelicOfferRow)
	{
		Registry.RegisterWidgetTarget(TEXT("Route.Merchant.RelicRow"), RelicOfferRow);
	}
	if (LeaveButton)
	{
		Registry.RegisterWidgetTarget(TEXT("Route.Merchant.Leave"), LeaveButton);
	}
}

void UGameXXKRouteMerchantWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("RouteMerchantWidgetTree"));
	if (!WidgetTree || RootSafeArea) return;
	RootSafeArea = WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), TEXT("RouteMerchantSafeArea"));
	RootSafeArea->SetVisibility(ESlateVisibility::SelfHitTestInvisible); WidgetTree->RootWidget = RootSafeArea;
	ResponsiveScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("RouteMerchantResponsiveScale"));
	ResponsiveScaleBox->SetStretch(EStretch::ScaleToFit); ResponsiveScaleBox->SetStretchDirection(EStretchDirection::Both);
	ResponsiveScaleBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UOverlay* ViewportLayers = WidgetTree->ConstructWidget<UOverlay>();
	ViewportLayers->SetVisibility(ESlateVisibility::SelfHitTestInvisible); RootSafeArea->AddChild(ViewportLayers);
	auto* FullBackdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("RouteMerchantBackdrop"));
	FullBackdrop->SetBrushColor(FLinearColor(.022f,.027f,.021f,.97f)); FullBackdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (auto* BackdropSlot = ViewportLayers->AddChildToOverlay(FullBackdrop)) { BackdropSlot->SetHorizontalAlignment(HAlign_Fill); BackdropSlot->SetVerticalAlignment(VAlign_Fill); }
	if (auto* ScaleSlot = ViewportLayers->AddChildToOverlay(ResponsiveScaleBox)) { ScaleSlot->SetHorizontalAlignment(HAlign_Fill); ScaleSlot->SetVerticalAlignment(VAlign_Fill); }
	DesignSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RouteMerchantDesign1920x1080"));
	DesignSizeBox->SetWidthOverride(1920); DesignSizeBox->SetHeightOverride(1080);
	DesignSizeBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible); ResponsiveScaleBox->AddChild(DesignSizeBox);
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RouteMerchantRootCanvas"));
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible); DesignSizeBox->AddChild(RootCanvas);

	auto* Paper = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RouteMerchantPaper"));
	Paper->SetBrush(FGameXXKInRunUiStyle::Paper({1736, 980})); Paper->SetBrushColor(FLinearColor::White);
	Paper->SetVisibility(ESlateVisibility::HitTestInvisible); AddCanvasChild(RootCanvas, Paper, {92, 48}, {1736, 980}, 1);
	auto* Title = MakeText(WidgetTree, FText::FromString(TEXT("山路行商")), 44, FGameXXKInRunUiStyle::Ink(), TEXT("RouteMerchantTitle"));
	AddCanvasChild(RootCanvas, Title, {163, 91}, {990, 76}, 2);
	OrdinaryGoldText = MakeText(WidgetTree, FText::GetEmpty(), 29, FGameXXKInRunUiStyle::Ink(), TEXT("RouteMerchantOrdinaryGold"));
	OrdinaryGoldText->SetJustification(ETextJustify::Right); AddCanvasChild(RootCanvas, OrdinaryGoldText, {1390, 117}, {350, 50}, 2);
	auto* CardHeader = MakeText(WidgetTree, FText::FromString(TEXT("卡牌强化")), 23, FGameXXKInRunUiStyle::Ink(), TEXT("RouteMerchantCardRowHeader"));
	AddCanvasChild(RootCanvas, CardHeader, {161, 193}, {620, 41}, 2);
	CardOfferRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RouteMerchantCardRow"));
	CardOfferRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible); AddCanvasChild(RootCanvas, CardOfferRow, {145, 234}, {1630, 432}, 2);
	for (int32 I = 0; I < MerchantCardSlotCount; ++I)
	{
		if (auto* Cell = BuildOfferCell(EGameXXKRouteMerchantOfferKind::Card, I))
		{
			auto* LayoutSlot = CardOfferRow->AddChildToHorizontalBox(Cell); LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LayoutSlot->SetHorizontalAlignment(HAlign_Center); LayoutSlot->SetPadding(FMargin(8, 0));
		}
	}
	AddCanvasChild(RootCanvas, MakeText(WidgetTree, FText::FromString(TEXT("随身遗物")), 25, FGameXXKInRunUiStyle::Ink(), TEXT("RouteMerchantRelicRowHeader")), {161, 671}, {700, 38}, 2);
	RelicOfferRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RouteMerchantRelicRow"));
	RelicOfferRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible); AddCanvasChild(RootCanvas, RelicOfferRow, {145, 719}, {1630, 215}, 2);
	for (int32 I = 0; I < MerchantRelicSlotCount; ++I)
	{
		if (auto* Cell = BuildOfferCell(EGameXXKRouteMerchantOfferKind::Relic, MerchantCardSlotCount + I))
		{
			auto* LayoutSlot = RelicOfferRow->AddChildToHorizontalBox(Cell); LayoutSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			LayoutSlot->SetHorizontalAlignment(HAlign_Center); LayoutSlot->SetPadding(FMargin(8, 0));
		}
	}
	LastActionErrorText = MakeText(WidgetTree, FText::GetEmpty(), 18, FGameXXKInRunUiStyle::Vermilion(), TEXT("RouteMerchantLastActionError"));
	LastActionErrorText->SetVisibility(ESlateVisibility::Collapsed); AddCanvasChild(RootCanvas, LastActionErrorText, {164, 956}, {886, 52}, 3);
	RefreshButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RouteMerchantRefreshButton"));
	RefreshButton->SetStyle(FGameXXKInRunUiStyle::Action({316, 59}, false));
	RefreshButtonText = MakeText(WidgetTree, FText::GetEmpty(), 22, FGameXXKInRunUiStyle::Ink(), TEXT("RouteMerchantRefreshLabel"));
	RefreshButtonText->SetAutoWrapText(false); RefreshButtonText->SetJustification(ETextJustify::Center);
	RefreshButton->SetContent(RefreshButtonText); RefreshButton->OnClicked.AddDynamic(this, &UGameXXKRouteMerchantWidget::HandleRefreshClicked);
	AddCanvasChild(RootCanvas, RefreshButton, {1114, 952}, {316, 59}, 3);
	LeaveButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RouteMerchantLeaveButton"));
	LeaveButton->SetStyle(FGameXXKInRunUiStyle::Action({280, 59}));
	LeaveButtonText = MakeText(WidgetTree, FText::FromString(TEXT("离开商店")), 23, FLinearColor::White, TEXT("RouteMerchantLeaveLabel"));
	LeaveButtonText->SetAutoWrapText(false); LeaveButtonText->SetJustification(ETextJustify::Center);
	LeaveButton->SetContent(LeaveButtonText); LeaveButton->OnClicked.AddDynamic(this, &UGameXXKRouteMerchantWidget::HandleLeaveClicked);
	AddCanvasChild(RootCanvas, LeaveButton, {1460, 952}, {280, 59}, 3);
	RenderedOfferIds.SetNum(MerchantOfferSlotCount); OfferTooltips.SetNum(MerchantOfferSlotCount); OfferDisabledReasons.SetNum(MerchantOfferSlotCount);
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
	Cell->SetWidthOverride(338.0f);
	Cell->SetHeightOverride(bCard ? 432.0f : 215.0f);
	Cell->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *FString::Printf(TEXT("RouteMerchantOfferStack%d"), GlobalOfferIndex));
	Stack->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Cell->AddChild(Stack);

	UTextBlock* OwnerText = MakeText(
		WidgetTree,
		FText::GetEmpty(),
		18,
		FGameXXKInRunUiStyle::Ink(),
		*FString::Printf(TEXT("RouteMerchantOfferOwner%d"), GlobalOfferIndex));
	OwnerText->SetJustification(ETextJustify::Center);
	OwnerText->SetAutoWrapText(false);
	if (!bCard) OwnerText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* ChildSlot = Stack->AddChildToVerticalBox(OwnerText))
	{
		ChildSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}
	OfferOwnerTexts.Add(OwnerText);

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
		: FGameXXKInRunUiStyle::Choice(VisualSize));
	DisplayButton->Configure(this, NAME_None, false);
	VisualBox->AddChild(DisplayButton);
	OfferDisplayButtons.Add(DisplayButton);

	UOverlay* Face = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *FString::Printf(TEXT("RouteMerchantOfferFace%d"), GlobalOfferIndex));
	Face->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	DisplayButton->SetContent(Face);
	if (auto* FaceSlot = Cast<UButtonSlot>(Face->Slot)) { FaceSlot->SetHorizontalAlignment(HAlign_Fill); FaceSlot->SetVerticalAlignment(VAlign_Fill); FaceSlot->SetPadding(FMargin(0)); }

	UImage* Art = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("RouteMerchantOfferArt%d"), GlobalOfferIndex));
	Art->SetVisibility(ESlateVisibility::Collapsed);
	UScaleBox* ArtScale = WidgetTree->ConstructWidget<UScaleBox>();
	ArtScale->SetStretch(EStretch::ScaleToFit);
	ArtScale->SetVisibility(ESlateVisibility::HitTestInvisible);
	ArtScale->SetContent(Art);
	if (UOverlaySlot* ChildSlot = Face->AddChildToOverlay(ArtScale))
	{
		ChildSlot->SetHorizontalAlignment(HAlign_Fill);
		ChildSlot->SetVerticalAlignment(VAlign_Fill);
		ChildSlot->SetPadding(bCard ? FMargin(8.0f, 48.0f, 8.0f, 9.0f) : FMargin(10.0f, 10.0f, 196.0f, 10.0f));
	}
	OfferArtImages.Add(Art);

	UTextBlock* ArtUnavailableText = MakeText(
		WidgetTree,
		FText::GetEmpty(),
		14,
		FLinearColor(0.30f, 0.24f, 0.15f, 1.0f),
		*FString::Printf(TEXT("RouteMerchantOfferArtUnavailable%d"), GlobalOfferIndex));
	ArtUnavailableText->SetJustification(ETextJustify::Center);
	ArtUnavailableText->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* ChildSlot = Face->AddChildToOverlay(ArtUnavailableText))
	{
		ChildSlot->SetHorizontalAlignment(HAlign_Fill);
		ChildSlot->SetVerticalAlignment(VAlign_Center);
		ChildSlot->SetPadding(FMargin(30.0f));
	}
	OfferArtUnavailableTexts.Add(ArtUnavailableText);

	UBorder* TitleBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *FString::Printf(TEXT("RouteMerchantOfferTitleBar%d"), GlobalOfferIndex));
	TitleBar->SetPadding(FMargin(0.0f));
	FSlateBrush NoNameBacking; NoNameBacking.DrawAs = ESlateBrushDrawType::NoDrawType; TitleBar->SetBrush(NoNameBacking);
	TitleBar->SetBrushColor(FLinearColor(0.70f, 0.62f, 0.46f, 0.94f));
	TitleBar->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UOverlaySlot* ChildSlot = Face->AddChildToOverlay(TitleBar))
	{
		ChildSlot->SetHorizontalAlignment(HAlign_Fill);
		ChildSlot->SetVerticalAlignment(bCard ? VAlign_Top : VAlign_Center);
		ChildSlot->SetPadding(bCard ? FMargin(8.0f, 14.0f, 8.0f, 0.0f) : FMargin(110.0f, 0.0f, 14.0f, 0.0f));
	}
	OfferTitleBars.Add(TitleBar);
	UTextBlock* NameText = MakeText(
		WidgetTree,
		FText::GetEmpty(),
		(bCard ? 22 : 20),
		FGameXXKInRunUiStyle::Ink(),
		*FString::Printf(TEXT("RouteMerchantOfferName%d"), GlobalOfferIndex));
	NameText->SetJustification(ETextJustify::Center);
	NameText->SetAutoWrapText(true);
	TitleBar->SetContent(NameText);
	OfferNameTexts.Add(NameText);
	if (bCard)
	{
		auto* Cost = MakeText(WidgetTree,FText::GetEmpty(),22,FGameXXKInRunUiStyle::Ink(),*FString::Printf(TEXT("RouteMerchantCardCost%d"),GlobalOfferIndex));
		Cost->SetAutoWrapText(false);
		if (auto* CostSlot = Face->AddChildToOverlay(Cost)) { CostSlot->SetHorizontalAlignment(HAlign_Left); CostSlot->SetVerticalAlignment(VAlign_Top); CostSlot->SetPadding(FMargin(13,78,0,0)); }
	}

	UTextBlock* QualityText = MakeText(
		WidgetTree,
		FText::GetEmpty(),
		16,
		FGameXXKInRunUiStyle::MutedInk(),
		*FString::Printf(TEXT("RouteMerchantOfferQuality%d"), GlobalOfferIndex));
	QualityText->SetJustification(ETextJustify::Center);
	QualityText->SetAutoWrapText(false);
	if (UVerticalBoxSlot* ChildSlot = Stack->AddChildToVerticalBox(QualityText))
	{
		ChildSlot->SetPadding(FMargin(4.0f, 5.0f, 4.0f, 2.0f));
	}
	OfferQualityTexts.Add(QualityText);

	UTextBlock* EffectText = MakeText(
		WidgetTree,
		FText::GetEmpty(),
		11,
		FLinearColor(0.80f, 0.73f, 0.60f, 1.0f),
		*FString::Printf(TEXT("RouteMerchantOfferEffect%d"), GlobalOfferIndex));
	EffectText->SetJustification(ETextJustify::Center);
	// Detailed effects live in the full tooltip. Keeping this reflected text
	// collapsed prevents two compact rows from turning into dense copy walls.
	EffectText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* ChildSlot = Stack->AddChildToVerticalBox(EffectText))
	{
		ChildSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ChildSlot->SetPadding(FMargin(10.0f, 1.0f, 10.0f, 3.0f));
	}
	OfferEffectTexts.Add(EffectText);

	UTextBlock* PriceText = MakeText(
		WidgetTree,
		FText::GetEmpty(),
		20,
		FGameXXKInRunUiStyle::Ink(),
		*FString::Printf(TEXT("RouteMerchantOfferPrice%d"), GlobalOfferIndex));
	PriceText->SetJustification(ETextJustify::Center);
	PriceText->SetAutoWrapText(false);
	if (UVerticalBoxSlot* ChildSlot = Stack->AddChildToVerticalBox(PriceText))
	{
		ChildSlot->SetPadding(FMargin(0.0f, 5.0f, 0.0f, 0.0f));
	}
	OfferPriceTexts.Add(PriceText);

	UTextBlock* StatusText = MakeText(
		WidgetTree,
		FText::GetEmpty(),
		10,
		FLinearColor(0.78f, 0.70f, 0.56f, 1.0f),
		*FString::Printf(TEXT("RouteMerchantOfferStatus%d"), GlobalOfferIndex));
	StatusText->SetJustification(ETextJustify::Center);
	StatusText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* ChildSlot = Stack->AddChildToVerticalBox(StatusText))
	{
		ChildSlot->SetPadding(FMargin(6.0f, 1.0f, 6.0f, 3.0f));
	}
	OfferStatusTexts.Add(StatusText);

	UGameXXKRouteMerchantOfferButton* PurchaseButton = WidgetTree->ConstructWidget<UGameXXKRouteMerchantOfferButton>(
		UGameXXKRouteMerchantOfferButton::StaticClass(),
		*FString::Printf(TEXT("RouteMerchantOfferBuy%d"), GlobalOfferIndex));
	PurchaseButton->SetStyle(FGameXXKInRunUiStyle::Action(FVector2D(208.0f, 46.0f)));
	PurchaseButton->Configure(this, NAME_None, true);
	UTextBlock* PurchaseText = MakeText(
		WidgetTree,
		NSLOCTEXT("GameXXKRouteMerchant", "Buy", "强化"),
		20,
		FLinearColor::White,
		*FString::Printf(TEXT("RouteMerchantOfferBuyLabel%d"), GlobalOfferIndex));
	PurchaseText->SetJustification(ETextJustify::Center);
	PurchaseText->SetAutoWrapText(false);
	FSlateFontInfo PurchaseFont = PurchaseText->GetFont();
	PurchaseFont.TypefaceFontName = TEXT("Bold");
	PurchaseText->SetFont(PurchaseFont);
	PurchaseButton->SetContent(PurchaseText);
	USizeBox* PurchaseSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		*FString::Printf(TEXT("RouteMerchantOfferBuySize%d"), GlobalOfferIndex));
	PurchaseSize->SetWidthOverride(208.0f);
	PurchaseSize->SetHeightOverride(46.0f);
	PurchaseSize->AddChild(PurchaseButton);
	if (UVerticalBoxSlot* ChildSlot = Stack->AddChildToVerticalBox(PurchaseSize))
	{
		ChildSlot->SetHorizontalAlignment(HAlign_Center);
		ChildSlot->SetPadding(FMargin(0.0f));
	}
	OfferPurchaseButtons.Add(PurchaseButton);
	OfferPurchaseTexts.Add(PurchaseText);
	if (OfferCardTooltipWidgets.Num() < MerchantOfferSlotCount)
	{
		OfferCardTooltipWidgets.SetNum(MerchantOfferSlotCount);
	}
	UGameXXKCardTooltipWidget* CardTooltip = WidgetTree->ConstructWidget<UGameXXKCardTooltipWidget>(
		UGameXXKCardTooltipWidget::StaticClass(),
		*FString::Printf(TEXT("RouteMerchantCardTooltip%d"), GlobalOfferIndex));
	OfferCardTooltipWidgets[GlobalOfferIndex] = CardTooltip;
	return Cell;
}

void UGameXXKRouteMerchantWidget::ApplyView(const FGameXXKRouteMerchantView& View)
{
	if (OrdinaryGoldText)
	{
		OrdinaryGoldText->SetText(FText::Format(
			NSLOCTEXT("GameXXKRouteMerchant", "OrdinaryGold", "金币：{0}"),
			FText::AsNumber(View.PlayerGold)));
	}
	if (RefreshButtonText)
	{
		RefreshButtonText->SetText(FText::Format(
			NSLOCTEXT("GameXXKRouteMerchant", "RefreshPrice", "刷新 {0} 金币"),
			FText::AsNumber(View.RefreshCost)));
	}
	if (RefreshButton)
	{
		RefreshButton->SetIsEnabled(View.bRefreshEnabled);
		RefreshButton->SetToolTipText(View.bRefreshEnabled
			? NSLOCTEXT("GameXXKRouteMerchant", "RefreshTooltip", "重抽两排未购买商品；已售槽保留，下一次刷新费用会提高。")
			: FText::FromString(LocalizeMerchantRuleError(
				View.RefreshDisabledReason,
				TEXT("当前不能刷新商店。"))));
	}
	if (LeaveButton)
	{
		LeaveButton->SetIsEnabled(View.bCanLeave);
		LeaveButton->SetToolTipText(NSLOCTEXT("GameXXKRouteMerchant", "LeaveTooltip", "离开商店、完成当前商人节点并返回路线图。"));
	}

	for (int32 Index = 0; Index < MerchantCardSlotCount; ++Index)
	{
		ApplyOffer(Index, View.CardOffers.IsValidIndex(Index) ? &View.CardOffers[Index] : nullptr, EGameXXKRouteMerchantOfferKind::Card);
	}
	for (int32 RelicIndex = 0; RelicIndex < MerchantRelicSlotCount; ++RelicIndex)
	{
		const int32 GlobalIndex = MerchantCardSlotCount + RelicIndex;
		ApplyOffer(
			GlobalIndex,
			View.RelicOffers.IsValidIndex(RelicIndex) ? &View.RelicOffers[RelicIndex] : nullptr,
			EGameXXKRouteMerchantOfferKind::Relic);
	}
	UpdateLastActionErrorDisplay();
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

void UGameXXKRouteMerchantWidget::ApplyOffer(
	const int32 GlobalOfferIndex,
	const FGameXXKRouteMerchantOfferView* OfferView,
	const EGameXXKRouteMerchantOfferKind ExpectedKind)
{
	if (!OfferDisplayButtons.IsValidIndex(GlobalOfferIndex)
		|| !OfferPurchaseButtons.IsValidIndex(GlobalOfferIndex)
		|| !OfferNameTexts.IsValidIndex(GlobalOfferIndex)
		|| !OfferOwnerTexts.IsValidIndex(GlobalOfferIndex)
		|| !OfferQualityTexts.IsValidIndex(GlobalOfferIndex)
		|| !OfferEffectTexts.IsValidIndex(GlobalOfferIndex)
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

	const bool bCard = ExpectedKind == EGameXXKRouteMerchantOfferKind::Card;
	FString DisplayName = bUnavailable
		? (bCard ? TEXT("没有可强化卡牌") : TEXT("没有可购买遗物"))
		: (bCard ? OfferFallbackName() : TEXT("未知遗物"));
	FString ArtPath;
	UTexture2D* ArtTexture = nullptr;
	const FGameXXKCardDefinition* CardDefinition = nullptr;
	const FGameXXKRelicDefinition* RelicDefinition = nullptr;
	if (!bUnavailable && bCard)
	{
		CardDefinition = FGameXXKCardCatalog::FindCardDefinition(Offer->ContentId);
		if (CardDefinition)
		{
			DisplayName = CardDefinition->DisplayName.ToString();
			ArtPath = ResolveCardPortraitPath(*CardDefinition);
			ArtTexture = ArtPath.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *ArtPath);
		}
	}
	else if (!bUnavailable)
	{
		RelicDefinition = FGameXXKRelicCatalog::FindDefinition(Offer->ContentId);
		if (RelicDefinition)
		{
			DisplayName = RelicDefinition->DisplayName.ToString();
			ArtPath = RelicDefinition->IconTexturePath.ToString();
			ArtTexture = ArtPath.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *ArtPath);
		}
	}

	OfferNameTexts[GlobalOfferIndex]->SetText(FText::FromString(DisplayName));
	if (auto* Cost = Cast<UTextBlock>(WidgetTree->FindWidget(*FString::Printf(TEXT("RouteMerchantCardCost%d"),GlobalOfferIndex))))
		Cost->SetText(CardDefinition ? FText::FromString(FString::Printf(TEXT("%d气\n%d内"),CardDefinition->EnergyCost,CardDefinition->ManaCost)) : FText::GetEmpty());
	OfferOwnerTexts[GlobalOfferIndex]->SetText(bCard
		? (bUnavailable
			? NSLOCTEXT("GameXXKRouteMerchant", "NoOwnerCompact", "暂无持牌角色")
			: FText::Format(
				NSLOCTEXT("GameXXKRouteMerchant", "OwnerCompact", "{0}"),
				ResolveOwnerLabel(Offer->OwnerMemberId)))
		: NSLOCTEXT("GameXXKRouteMerchant", "RelicOwner", "遗物"));
	OfferQualityTexts[GlobalOfferIndex]->SetText(bUnavailable
		? (bCard
			? NSLOCTEXT("GameXXKRouteMerchant", "NoQualityUpgrade", "-- → --")
			: NSLOCTEXT("GameXXKRouteMerchant", "NoRelicQuality", "--"))
		: (bCard
			? FText::Format(
				NSLOCTEXT("GameXXKRouteMerchant", "QualityUpgrade", "{0} → {1}"),
				FGameXXKCardQualityRules::GetDisplayName(Offer->Quality),
				FGameXXKCardQualityRules::GetDisplayName(Offer->NextQuality))
			: FGameXXKCardQualityRules::GetDisplayName(Offer->Quality)));
	FString EffectPreview;
	if (bUnavailable)
	{
		EffectPreview = bCard
			? TEXT("当前没有更多可强化的携带卡牌")
			: TEXT("当前没有更多可购买遗物");
	}
	else if (CardDefinition)
	{
		EffectPreview = GameXXKCardText::DescribeEffects(*CardDefinition, Offer->NextQuality);
		EffectPreview.ReplaceInline(TEXT("\n"), TEXT(" · "));
		if (EffectPreview.Len() > 64)
		{
			EffectPreview = EffectPreview.Left(61) + TEXT("...");
		}
	}
	else if (RelicDefinition)
	{
		EffectPreview = RelicDefinition->Description.ToString();
		EffectPreview.ReplaceInline(TEXT("\n"), TEXT(" · "));
		if (EffectPreview.Len() > 64)
		{
			EffectPreview = EffectPreview.Left(61) + TEXT("...");
		}
	}
	else
	{
		EffectPreview = bCard ? TEXT("强化效果资料暂不可用") : TEXT("遗物资料暂不可用");
	}
	OfferEffectTexts[GlobalOfferIndex]->SetText(FText::FromString(
		bUnavailable || !bCard ? EffectPreview : FString::Printf(TEXT("强化后：%s"), *EffectPreview)));
	OfferPriceTexts[GlobalOfferIndex]->SetText(bUnavailable
		? NSLOCTEXT("GameXXKRouteMerchant", "NoPrice", "金币 --")
		: FText::Format(NSLOCTEXT("GameXXKRouteMerchant", "OfferPrice", "金币 {0}"), FText::AsNumber(Offer->Price)));
	OfferStatusTexts[GlobalOfferIndex]->SetText(DisabledReason.IsEmpty()
		? NSLOCTEXT("GameXXKRouteMerchant", "Available", "可购买")
		: FText::FromString(DisabledReason));

	FText PurchaseLabel = bCard
		? NSLOCTEXT("GameXXKRouteMerchant", "Buy", "强化")
		: NSLOCTEXT("GameXXKRouteMerchant", "BuyRelic", "购买");
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
		PurchaseLabel = NSLOCTEXT("GameXXKRouteMerchant", "Insufficient", "金币不足");
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
	if (OfferArtUnavailableTexts.IsValidIndex(GlobalOfferIndex))
	{
		UTextBlock* MissingArt = OfferArtUnavailableTexts[GlobalOfferIndex];
		MissingArt->SetText(bUnavailable
			? (bCard
				? NSLOCTEXT("GameXXKRouteMerchant", "UnavailableArt", "没有可强化卡牌")
				: NSLOCTEXT("GameXXKRouteMerchant", "UnavailableRelicArt", "没有可购买遗物"))
			: (bCard
				? NSLOCTEXT("GameXXKRouteMerchant", "MissingArt", "卡面暂缺")
				: NSLOCTEXT("GameXXKRouteMerchant", "MissingRelicArt", "遗物图标暂缺")));
		MissingArt->SetVisibility(ArtTexture
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
	}

	const FText Tooltip = BuildOfferTooltip(OfferView, ExpectedKind, DisabledReason);
	UGameXXKCardTooltipWidget* CardTooltip = OfferCardTooltipWidgets.IsValidIndex(GlobalOfferIndex)
		? OfferCardTooltipWidgets[GlobalOfferIndex].Get()
		: nullptr;
	if (!bUnavailable && bCard && CardDefinition && CardTooltip && Offer)
	{
		FGameXXKCardTooltipContext Context;
		Context.InteractionResult = FString::Printf(
			TEXT("强化：%s → %s · %d金币"),
			*FGameXXKCardQualityRules::GetDisplayName(Offer->Quality).ToString(),
			*FGameXXKCardQualityRules::GetDisplayName(Offer->NextQuality).ToString(),
			Offer->Price);
		Context.UnavailableReason = DisabledReason;
		FGameXXKCardPlayPreview ReferencePreview;
		const UGameXXKMVPSubsystem* TooltipSubsystem = ResolveMVPSubsystem();
		const FGameXXKCardPlayPreview* Preview = TooltipSubsystem
			&& FGameXXKCardBattleAdapter::BuildReferenceCardPlayPreview(
				TooltipSubsystem->GetRuntimeState(),
				FGameXXKEquipmentRules::HeroCharacterId(),
				CardDefinition->Id,
				Offer->NextQuality,
				ReferencePreview)
			? &ReferencePreview
			: nullptr;
		CardTooltip->ConfigureCard(
			*CardDefinition,
			Offer->NextQuality,
			Preview,
			Context);
		DisplayButton->SetToolTipText(FText::GetEmpty());
		PurchaseButton->SetToolTipText(FText::GetEmpty());
		DisplayButton->SetToolTip(CardTooltip);
		PurchaseButton->SetToolTip(CardTooltip);
		OfferTooltips[GlobalOfferIndex] = FText::FromString(CardTooltip->GetDisplayedTextForTest());
	}
	else
	{
		OfferTooltips[GlobalOfferIndex] = Tooltip;
		DisplayButton->SetToolTipText(Tooltip);
		PurchaseButton->SetToolTipText(Tooltip);
	}
}

FText UGameXXKRouteMerchantWidget::BuildOfferTooltip(
	const FGameXXKRouteMerchantOfferView* OfferView,
	const EGameXXKRouteMerchantOfferKind ExpectedKind,
	const FString& DisabledReason) const
{
	if (!OfferView || OfferView->SavedOffer.bUnavailable || OfferView->SavedOffer.ContentId.IsNone())
	{
		const TCHAR* EmptyLabel = ExpectedKind == EGameXXKRouteMerchantOfferKind::Card
			? TEXT("没有可强化卡牌")
			: TEXT("没有可购买遗物");
		return FText::FromString(FString::Printf(
			TEXT("%s\n%s"),
			EmptyLabel,
			DisabledReason.IsEmpty() ? TEXT("本格当前不可用。") : *DisabledReason));
	}

	const FGameXXKRouteMerchantOffer& Offer = OfferView->SavedOffer;
	if (ExpectedKind == EGameXXKRouteMerchantOfferKind::Card)
	{
		if (const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(Offer.ContentId))
		{
			FGameXXKCardTooltipContext Context;
			Context.InteractionResult = FString::Printf(TEXT("价格：%d 金币"), Offer.Price);
			Context.UnavailableReason = DisabledReason;
			return FText::FromString(FString::Printf(
				TEXT("%s\n强化至%s：\n%s"),
				*GameXXKCardText::DescribeTooltip(*Definition, Offer.Quality, nullptr, Context),
				*FGameXXKCardQualityRules::GetDisplayName(Offer.NextQuality).ToString(),
				*GameXXKCardText::DescribeEffects(*Definition, Offer.NextQuality)));
		}
	}
	else if (const FGameXXKRelicDefinition* Definition =
		FGameXXKRelicCatalog::FindDefinition(Offer.ContentId))
	{
		return FText::FromString(FString::Printf(
			TEXT("%s\n%s\n品质：%s\n价格：%d 金币%s%s"),
			*Definition->DisplayName.ToString(),
			*Definition->Description.ToString(),
			*FGameXXKCardQualityRules::GetDisplayName(Offer.Quality).ToString(),
			Offer.Price,
			DisabledReason.IsEmpty() ? TEXT("") : TEXT("\n"),
			*DisabledReason));
	}

	const TCHAR* UnknownLabel = ExpectedKind == EGameXXKRouteMerchantOfferKind::Card
		? TEXT("未知卡牌")
		: TEXT("未知遗物");
	return FText::FromString(FString::Printf(
		TEXT("%s\n价格：%d 金币%s%s"),
		UnknownLabel,
		Offer.Price,
		DisabledReason.IsEmpty() ? TEXT("") : TEXT("\n"),
		*DisabledReason));
}

FText UGameXXKRouteMerchantWidget::ResolveOwnerLabel(const FName OwnerMemberId) const
{
	if (OwnerMemberId == TEXT("Player"))
	{
		return NSLOCTEXT("GameXXKRouteMerchant", "HeroOwner", "主角");
	}
	if (FGameXXKCompanionCatalog::FindQuestNpcDefinition(OwnerMemberId))
	{
		return QuestNpcFriendlyName(OwnerMemberId);
	}
	if (const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem())
	{
		FGameXXKPermanentCompanion Companion;
		if (Subsystem->TryGetPermanentCompanionView(OwnerMemberId, Companion))
		{
			const FString DisplayName = FGameXXKCompanionRules::GetCompanionDisplayName(
				Companion.Role,
				Companion.NameSeed);
			if (!DisplayName.IsEmpty())
			{
				return FText::FromString(DisplayName);
			}
		}
	}
	return NSLOCTEXT("GameXXKRouteMerchant", "FriendlyOwnerFallback", "队伍成员");
}

FString UGameXXKRouteMerchantWidget::ResolveDisabledReason(const FGameXXKRouteMerchantOfferView* OfferView) const
{
	if (!OfferView)
	{
		return TEXT("本格当前不可用。");
	}
	if (!OfferView->DisabledReason.IsEmpty())
	{
		return LocalizeMerchantRuleError(
			OfferView->DisabledReason,
			TEXT("当前不能强化这张卡牌。"));
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
		return TEXT("金币不足。");
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
	UpdateLastActionErrorDisplay();
}

void UGameXXKRouteMerchantWidget::HandleRefreshClicked()
{
	RefreshStock();
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

FString UGameXXKRouteMerchantWidget::GetCardFrameResourcePathForTest() const
{
	return CardFrameTexturePath;
}

int32 UGameXXKRouteMerchantWidget::GetRenderedCardOfferCountForTest() const
{
	return CardOfferRow ? CardOfferRow->GetChildrenCount() : 0;
}

int32 UGameXXKRouteMerchantWidget::GetRenderedRelicOfferCountForTest() const
{
	return RelicOfferRow ? RelicOfferRow->GetChildrenCount() : 0;
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

FText UGameXXKRouteMerchantWidget::GetOrdinaryGoldTextForTest() const
{
	return OrdinaryGoldText ? OrdinaryGoldText->GetText() : FText::GetEmpty();
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
