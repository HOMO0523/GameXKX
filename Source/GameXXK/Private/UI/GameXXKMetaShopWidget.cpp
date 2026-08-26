#include "UI/GameXXKMetaShopWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "GameXXKAffixCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentRules.h"
#include "MVP/GameXXKMVPSubsystem.h"

namespace
{
	constexpr const TCHAR* PaperFrameTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_PanelLarge.T_MasterV2_PanelLarge");
	constexpr const TCHAR* CloseInkTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CloseInk.T_MasterV2_CloseInk");
	constexpr const TCHAR* ItemSlotTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot.T_MasterV2_ItemSlot");
	constexpr const TCHAR* IngotTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_Ingot.T_MasterV2_Ingot");
	constexpr const TCHAR* SelectionInkTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ButtonPurchase.T_MasterV2_ButtonPurchase");
	constexpr const TCHAR* PurchaseButtonTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ButtonPurchase.T_MasterV2_ButtonPurchase");
	const FVector2D CloseButtonSize(74.0f, 74.0f);
	const FVector2D ProductCardSize(170.0f, 170.0f);
	const FVector2D ProductIconSize(120.0f, 120.0f);
	const FVector2D SelectionInkSize(194.0f, 66.0f);
	const FVector2D SelectionInkOffset(-12.0f, -24.0f);
	const FVector2D PurchaseButtonSize(210.0f, 72.0f);

	FSlateBrush MakeTextureBrush(const TCHAR* Path, const FVector2D& ImageSize)
	{
		FSlateBrush Brush;
		UTexture2D* Texture = Path ? LoadObject<UTexture2D>(nullptr, Path) : nullptr;
		Brush.SetResourceObject(Texture);
		Brush.DrawAs = Texture ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
		Brush.ImageSize = ImageSize;
		Brush.TintColor = FSlateColor(FLinearColor::White);
		return Brush;
	}

	FButtonStyle MakeTextureButtonStyle(const TCHAR* Path, const FVector2D& ImageSize)
	{
		FButtonStyle Style;
		Style.SetNormal(MakeTextureBrush(Path, ImageSize));
		Style.SetHovered(MakeTextureBrush(Path, ImageSize));
		Style.SetPressed(MakeTextureBrush(Path, ImageSize));
		Style.SetDisabled(MakeTextureBrush(Path, ImageSize));
		return Style;
	}

	FButtonStyle MakeBoxTextureButtonStyle(const TCHAR* Path, const FVector2D& ImageSize, const FMargin& Margin)
	{
		FButtonStyle Style = MakeTextureButtonStyle(Path, ImageSize);
		FSlateBrush Brush = Style.Normal;
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = Margin;
		Style.SetNormal(Brush);
		Style.SetHovered(Brush);
		Style.SetPressed(Brush);
		Style.SetDisabled(Brush);
		return Style;
	}

	FString ShopCardPortraitPath(const EGameXXKCharacterRole Role)
	{
		const TCHAR* RoleName = nullptr;
		switch (Role)
		{
		case EGameXXKCharacterRole::Blade: RoleName = TEXT("Blade"); break;
		case EGameXXKCharacterRole::Guard: RoleName = TEXT("Guard"); break;
		case EGameXXKCharacterRole::Healer: RoleName = TEXT("Healer"); break;
		case EGameXXKCharacterRole::Hunter: RoleName = TEXT("Hunter"); break;
		case EGameXXKCharacterRole::Sorcerer: RoleName = TEXT("Sorcerer"); break;
		case EGameXXKCharacterRole::FormationMaster: RoleName = TEXT("FormationMaster"); break;
		default: return FString();
		}
		return FString::Printf(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_%s.T_CardPortrait_Role_%s"), RoleName, RoleName);
	}

	FText ShopEquipmentQualityText(const EGameXXKEquipmentQuality Quality)
	{
		return FGameXXKEquipmentQualityRules::GetDisplayName(Quality);
	}

	FText ShopRoleDisplayName(const EGameXXKCharacterRole Role)
	{
		switch (Role)
		{
		case EGameXXKCharacterRole::Blade: return NSLOCTEXT("GameXXKMetaShop", "RoleBlade", "刀客");
		case EGameXXKCharacterRole::Guard: return NSLOCTEXT("GameXXKMetaShop", "RoleGuard", "护卫");
		case EGameXXKCharacterRole::Healer: return NSLOCTEXT("GameXXKMetaShop", "RoleHealer", "医师");
		case EGameXXKCharacterRole::Hunter: return NSLOCTEXT("GameXXKMetaShop", "RoleHunter", "猎手");
		case EGameXXKCharacterRole::Sorcerer: return NSLOCTEXT("GameXXKMetaShop", "RoleSorcerer", "术士");
		case EGameXXKCharacterRole::FormationMaster: return NSLOCTEXT("GameXXKMetaShop", "RoleFormationMaster", "阵师");
		default: return NSLOCTEXT("GameXXKMetaShop", "RoleUnknown", "未知职业");
		}
	}

	FText ShopEquipmentSlotText(const EGameXXKEquipmentSlot Slot)
	{
		switch (Slot)
		{
		case EGameXXKEquipmentSlot::Weapon: return NSLOCTEXT("GameXXKMetaShop", "SlotWeapon", "武器");
		case EGameXXKEquipmentSlot::Head: return NSLOCTEXT("GameXXKMetaShop", "SlotHead", "头部");
		case EGameXXKEquipmentSlot::Armor: return NSLOCTEXT("GameXXKMetaShop", "SlotArmor", "衣甲");
		case EGameXXKEquipmentSlot::Belt: return NSLOCTEXT("GameXXKMetaShop", "SlotBelt", "腰带");
		case EGameXXKEquipmentSlot::Shoes: return NSLOCTEXT("GameXXKMetaShop", "SlotShoes", "鞋");
		case EGameXXKEquipmentSlot::Accessory: return NSLOCTEXT("GameXXKMetaShop", "SlotAccessory", "饰品");
		default: return FText::GetEmpty();
		}
	}

	FText ShopEquipmentInstanceDetail(
		const UGameXXKMVPSubsystem* Subsystem,
		const FGameXXKEquipmentInstance& Instance,
		const FGameXXKEquipmentDefinition& Definition)
	{
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("部位：%s"), *ShopEquipmentSlotText(Definition.Slot).ToString()));
		Lines.Add(FString::Printf(TEXT("装备等级 %d"), Instance.ItemLevel));
		Lines.Add(FString::Printf(TEXT("品质：%s"), *ShopEquipmentQualityText(Instance.Quality).ToString()));
		Lines.Add(FString::Printf(TEXT("强化 +%d"), Instance.EnhancementLevel));
		for (const FGameXXKEquipmentAffixRoll& Roll : Instance.RolledAffixes)
		{
			const FGameXXKAffixDefinition* Affix = FGameXXKAffixCatalog::FindDefinition(Roll.AffixId);
			if (Affix)
			{
				if (Roll.Unit == EGameXXKEquipmentMagnitudeUnit::BasisPoints)
				{
					Lines.Add(FString::Printf(TEXT("%s +%.2f%%"), *Affix->DisplayName.ToString(), Roll.Magnitude / 100.0));
				}
				else
				{
					Lines.Add(FString::Printf(TEXT("%s +%d"), *Affix->DisplayName.ToString(), Roll.Magnitude));
				}
			}
		}
		FGameXXKEquipmentTooltipSnapshot Snapshot;
		if (Subsystem && Subsystem->GetEquipmentTooltipSnapshot(
			Instance.InstanceId,
			FGameXXKEquipmentRules::HeroCharacterId(),
			Snapshot))
		{
			if (Snapshot.ItemCurrentStats.Attack != 0) { Lines.Add(FString::Printf(TEXT("攻击 %+d"), Snapshot.ItemCurrentStats.Attack)); }
			if (Snapshot.ItemCurrentStats.Defense != 0) { Lines.Add(FString::Printf(TEXT("防御 %+d"), Snapshot.ItemCurrentStats.Defense)); }
			if (Snapshot.ItemCurrentStats.MaxHealth != 0) { Lines.Add(FString::Printf(TEXT("气血 %+d"), Snapshot.ItemCurrentStats.MaxHealth)); }
			if (Snapshot.ItemCurrentStats.MaxMana != 0) { Lines.Add(FString::Printf(TEXT("真气 %+d"), Snapshot.ItemCurrentStats.MaxMana)); }
			if (Snapshot.ItemCurrentStats.Speed != 0) { Lines.Add(FString::Printf(TEXT("身法 %+d"), Snapshot.ItemCurrentStats.Speed)); }
		}
		return FText::FromString(FString::Join(Lines, TEXT("\n")));
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const FName Name, const FText& Text, const int32 Size)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Block->SetText(Text);
		// No auto wrap by default so narrow boxes never stack Chinese vertically;
		// description blocks opt back in explicitly.
		Block->SetAutoWrapText(false);
		Block->SetJustification(ETextJustify::Center);
		Block->SetColorAndOpacity(FSlateColor(FLinearColor(0.12f, 0.08f, 0.04f, 1.0f)));
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		return Block;
	}

	void AddCanvas(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D Position, const FVector2D Size, const int32 ZOrder)
	{
		if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget))
		{
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
			Slot->SetZOrder(ZOrder);
		}
	}

	FSlateBrush TextureBrush(const TCHAR* Path)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(LoadObject<UTexture2D>(nullptr, Path));
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.Margin = FMargin(0.08f);
		Brush.TintColor = FSlateColor(FLinearColor::White);
		return Brush;
	}
	UBorder* BuildResultTooltip(UWidgetTree* WidgetTree, const FText& Text)
	{
		if (!WidgetTree)
		{
			return nullptr;
		}
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Frame->SetBrush(TextureBrush(ItemSlotTexturePath));
		Frame->SetPadding(FMargin(16.0f, 12.0f));
		UTextBlock* Block = MakeText(WidgetTree, NAME_None, Text, 13);
		Block->SetJustification(ETextJustify::Left);
		Block->SetAutoWrapText(true);
		Frame->SetContent(Block);
		return Frame;
	}

}

void UGameXXKMetaShopProductButton::Configure(
	UGameXXKMetaShopWidget* InOwner,
	const EGameXXKMetaShopProductId InProductId)
{
	Owner = InOwner;
	ProductId = InProductId;
	OnClicked.RemoveDynamic(this, &UGameXXKMetaShopProductButton::HandleClicked);
	OnClicked.AddDynamic(this, &UGameXXKMetaShopProductButton::HandleClicked);
}

void UGameXXKMetaShopProductButton::HandleClicked()
{
	if (Owner)
	{
		Owner->SelectProduct(ProductId);
	}
}

TSharedRef<SWidget> UGameXXKMetaShopWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKMetaShopWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	RefreshFromState();
}

void UGameXXKMetaShopWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MetaShopRoot"));
	WidgetTree->RootWidget = RootCanvas;

	// Page 07 paper window at absolute Master V1 screen coordinates.
	UBorder* PaperFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MetaShopPaperFrame"));
	PaperFrame->SetBrush(TextureBrush(PaperFrameTexturePath));
	AddCanvas(RootCanvas, PaperFrame, FVector2D(311.0f, 173.0f), FVector2D(1450.0f, 849.0f), 1);

	// Content is placed at Master V1 screen coordinates, so the content canvas
	// must live on the root at (0,0) — a sibling of the paper window, not a child.
	FrameCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MetaShopFrameCanvas"));
	AddCanvas(RootCanvas, FrameCanvas, FVector2D::ZeroVector, FVector2D(1920.0f, 1080.0f), 2);

	// Page 07 title.
	UTextBlock* TitleText = MakeText(WidgetTree, TEXT("MetaShopTitleText"), NSLOCTEXT("GameXXKMetaShop", "Title", "商店"), 28);
	TitleText->SetJustification(ETextJustify::Left);
	TitleText->SetAutoWrapText(false);
	AddCanvas(FrameCanvas, TitleText, FVector2D(390.0f, 211.0f), FVector2D(86.0f, 43.0f), 0);

	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MetaShopCloseButton"));
	CloseButton->SetStyle(MakeTextureButtonStyle(CloseInkTexturePath, CloseButtonSize));
	CloseButton->SetBackgroundColor(FLinearColor::White);
	CloseButton->OnClicked.AddDynamic(this, &UGameXXKMetaShopWidget::HandleCloseClicked);
	AddCanvas(FrameCanvas, CloseButton, FVector2D(1652.0f, 201.0f), CloseButtonSize, 0);

	// Page 07 product cards: four on the top row, three on the bottom row.
	const FVector2D ProductCardPositions[7] = {
		FVector2D(410.0f, 300.0f), FVector2D(630.0f, 300.0f), FVector2D(850.0f, 300.0f), FVector2D(1070.0f, 300.0f),
		FVector2D(520.0f, 610.0f), FVector2D(740.0f, 610.0f), FVector2D(960.0f, 610.0f)};
	for (int32 Index = 0; Index < 7; ++Index)
	{
		UGameXXKMetaShopProductButton* Card = WidgetTree->ConstructWidget<UGameXXKMetaShopProductButton>(
			UGameXXKMetaShopProductButton::StaticClass(),
			FName(*FString::Printf(TEXT("MetaShopProductCard_%d"), Index)));
		Card->SetStyle(MakeBoxTextureButtonStyle(ItemSlotTexturePath, ProductCardSize, FMargin(0.08f)));
		Card->SetBackgroundColor(FLinearColor::White);
		UOverlay* CardOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UImage* Image = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			FName(*FString::Printf(TEXT("MetaShopProductImage_%d"), Index)));
		Image->SetDesiredSizeOverride(ProductIconSize);
		if (UOverlaySlot* ImageSlot = CardOverlay->AddChildToOverlay(Image))
		{
			ImageSlot->SetHorizontalAlignment(HAlign_Center);
			ImageSlot->SetVerticalAlignment(VAlign_Center);
		}
		Card->AddChild(CardOverlay);
		AddCanvas(FrameCanvas, Card, ProductCardPositions[Index], ProductCardSize, 0);

		UTextBlock* Name = MakeText(WidgetTree, FName(*FString::Printf(TEXT("MetaShopProductName_%d"), Index)), FText::GetEmpty(), 16);
		Name->SetJustification(ETextJustify::Center);
		Name->SetAutoWrapText(false);
		AddCanvas(FrameCanvas, Name, FVector2D(ProductCardPositions[Index].X, ProductCardPositions[Index].Y + 182.0f), FVector2D(170.0f, 21.0f), 0);

		UImage* IngotIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*FString::Printf(TEXT("MetaShopProductIngot_%d"), Index)));
		IngotIcon->SetBrush(MakeTextureBrush(IngotTexturePath, FVector2D(18.0f, 20.0f)));
		AddCanvas(FrameCanvas, IngotIcon, ProductCardPositions[Index] + FVector2D(55.0f, 218.0f), FVector2D(18.0f, 20.0f), 0);

		UTextBlock* Price = MakeText(WidgetTree, FName(*FString::Printf(TEXT("MetaShopProductPrice_%d"), Index)), FText::GetEmpty(), 14);
		Price->SetJustification(ETextJustify::Left);
		Price->SetAutoWrapText(false);
		AddCanvas(FrameCanvas, Price, ProductCardPositions[Index] + FVector2D(75.0f, 216.0f), FVector2D(70.0f, 20.0f), 0);

		ProductButtons.Add(Card);
		ProductImages.Add(Image);
		ProductNameTexts.Add(Name);
		ProductPriceTexts.Add(Price);
	}

	// Page 07 selection ink above the chosen product card.
	ProductSelectionInk = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MetaShopProductSelectionInk"));
	ProductSelectionInk->SetBrush(MakeTextureBrush(SelectionInkTexturePath, SelectionInkSize));
	ProductSelectionInk->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvas(FrameCanvas, ProductSelectionInk, ProductCardPositions[0] + SelectionInkOffset, SelectionInkSize, 0);

	// Page 07 detail area: slot, icon, description texts, price row, purchase button.
	UImage* DetailSlot = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MetaShopDetailSlot"));
	DetailSlot->SetBrush(MakeTextureBrush(ItemSlotTexturePath, FVector2D(220.0f, 220.0f)));
	AddCanvas(FrameCanvas, DetailSlot, FVector2D(1370.0f, 330.0f), FVector2D(220.0f, 220.0f), 0);

	DetailIconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MetaShopDetailIcon"));
	DetailIconImage->SetDesiredSizeOverride(FVector2D(72.0f, 132.0f));
	AddCanvas(FrameCanvas, DetailIconImage, FVector2D(1444.0f, 374.0f), FVector2D(72.0f, 132.0f), 0);

	DetailNameText = MakeText(WidgetTree, TEXT("MetaShopDetailName"), FText::GetEmpty(), 20);
	DetailNameText->SetJustification(ETextJustify::Left);
	DetailNameText->SetAutoWrapText(false);
	AddCanvas(FrameCanvas, DetailNameText, FVector2D(1305.0f, 585.0f), FVector2D(260.0f, 24.0f), 0);

	DetailDescriptionText = MakeText(WidgetTree, TEXT("MetaShopDetailDescription"), FText::GetEmpty(), 14);
	DetailDescriptionText->SetJustification(ETextJustify::Left);
	DetailDescriptionText->SetAutoWrapText(true);
	AddCanvas(FrameCanvas, DetailDescriptionText, FVector2D(1305.0f, 615.0f), FVector2D(260.0f, 110.0f), 0);

	DisabledReasonText = MakeText(WidgetTree, TEXT("MetaShopDisabledReason"), FText::GetEmpty(), 13);
	DisabledReasonText->SetJustification(ETextJustify::Left);
	AddCanvas(FrameCanvas, DisabledReasonText, FVector2D(1305.0f, 745.0f), FVector2D(260.0f, 32.0f), 0);

	PurchaseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MetaShopPurchaseButton"));
	PurchaseButton->SetStyle(MakeBoxTextureButtonStyle(PurchaseButtonTexturePath, PurchaseButtonSize, FMargin(0.08f)));
	PurchaseButton->SetBackgroundColor(FLinearColor::White);
	PurchaseButton->OnClicked.AddDynamic(this, &UGameXXKMetaShopWidget::HandlePurchaseClicked);
	UTextBlock* PurchaseText = MakeText(WidgetTree, TEXT("MetaShopPurchaseText"), NSLOCTEXT("GameXXKMetaShop", "Purchase", "购买"), 24);
	PurchaseText->SetJustification(ETextJustify::Center);
	PurchaseText->SetAutoWrapText(false);
	PurchaseButton->AddChild(PurchaseText);
	AddCanvas(FrameCanvas, PurchaseButton, FVector2D(1375.0f, 870.0f), PurchaseButtonSize, 0);

	ConfirmOverlay = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MetaShopConfirmOverlay"));
	ConfirmOverlay->SetBrush(TextureBrush(PaperFrameTexturePath));
	ConfirmOverlay->SetPadding(FMargin(35.0f));
	UVerticalBox* ConfirmBody = WidgetTree->ConstructWidget<UVerticalBox>();
	ConfirmBody->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("MetaShopConfirmPrompt"), NSLOCTEXT("GameXXKMetaShop", "ConfirmPrompt", "确认购买所选商品？"), 32));
	UHorizontalBox* ConfirmActions = WidgetTree->ConstructWidget<UHorizontalBox>();
	UButton* ConfirmButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MetaShopConfirmButton"));
	ConfirmButton->SetStyle(MakeBoxTextureButtonStyle(PurchaseButtonTexturePath, FVector2D(150.0f, 48.0f), FMargin(0.08f)));
	ConfirmButton->SetBackgroundColor(FLinearColor::White);
	ConfirmButton->OnClicked.AddDynamic(this, &UGameXXKMetaShopWidget::HandleConfirmClicked);
	ConfirmButton->AddChild(MakeText(WidgetTree, TEXT("MetaShopConfirmText"), NSLOCTEXT("GameXXKMetaShop", "Confirm", "确认"), 27));
	UButton* CancelButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MetaShopCancelButton"));
	CancelButton->SetStyle(MakeBoxTextureButtonStyle(PurchaseButtonTexturePath, FVector2D(150.0f, 48.0f), FMargin(0.08f)));
	CancelButton->SetBackgroundColor(FLinearColor::White);
	CancelButton->OnClicked.AddDynamic(this, &UGameXXKMetaShopWidget::HandleCancelClicked);
	CancelButton->AddChild(MakeText(WidgetTree, TEXT("MetaShopCancelText"), NSLOCTEXT("GameXXKMetaShop", "Cancel", "取消"), 27));
	ConfirmActions->AddChildToHorizontalBox(ConfirmButton);
	ConfirmActions->AddChildToHorizontalBox(CancelButton);
	if (UVerticalBoxSlot* ActionsSlot = ConfirmBody->AddChildToVerticalBox(ConfirmActions))
	{
		ActionsSlot->SetHorizontalAlignment(HAlign_Center);
		ActionsSlot->SetPadding(FMargin(0.0f, 24.0f, 0.0f, 0.0f));
	}
	ConfirmOverlay->SetContent(ConfirmBody);
	ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvas(RootCanvas, ConfirmOverlay, FVector2D(600.0f, 350.0f), FVector2D(720.0f, 330.0f), 10);

	ResultPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MetaShopResultPanel"));
	ResultPanel->SetBrush(TextureBrush(PaperFrameTexturePath));
	ResultPanel->SetPadding(FMargin(28.0f));
	UVerticalBox* ResultBody = WidgetTree->ConstructWidget<UVerticalBox>();
	// A backpack-slot frame hosts the purchased item's art; hovering it shows
	// the generated equipment's stats (or the recruited partner's card face).
	ResultSlotFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MetaShopResultSlotFrame"));
	ResultSlotFrame->SetBrush(TextureBrush(ItemSlotTexturePath));
	ResultSlotFrame->SetPadding(FMargin(6.0f));
	ResultImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MetaShopResultImage"));
	ResultImage->SetDesiredSizeOverride(FVector2D(68.0f, 68.0f));
	ResultImage->SetVisibility(ESlateVisibility::Collapsed);
	ResultSlotFrame->SetContent(ResultImage);
	if (UVerticalBoxSlot* ImageSlot = ResultBody->AddChildToVerticalBox(ResultSlotFrame))
	{
		ImageSlot->SetHorizontalAlignment(HAlign_Center);
		ImageSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}
	ResultText = MakeText(WidgetTree, TEXT("MetaShopResultText"), FText::GetEmpty(), 17);
	ResultText->SetAutoWrapText(true);
	ResultBody->AddChildToVerticalBox(ResultText);
	ResultConfirmButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MetaShopResultConfirmButton"));
	ResultConfirmButton->SetStyle(MakeBoxTextureButtonStyle(PurchaseButtonTexturePath, FVector2D(130.0f, 40.0f), FMargin(0.08f)));
	ResultConfirmButton->SetBackgroundColor(FLinearColor::White);
	ResultConfirmButton->OnClicked.AddDynamic(this, &UGameXXKMetaShopWidget::HandleResultConfirmClicked);
	ResultConfirmButton->AddChild(MakeText(WidgetTree, TEXT("MetaShopResultConfirmText"), NSLOCTEXT("GameXXKMetaShop", "ResultConfirm", "确定"), 20));
	if (UVerticalBoxSlot* ConfirmSlot = ResultBody->AddChildToVerticalBox(ResultConfirmButton))
	{
		ConfirmSlot->SetHorizontalAlignment(HAlign_Center);
		ConfirmSlot->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 0.0f));
	}
	ResultPanel->SetContent(ResultBody);
	ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
	AddCanvas(RootCanvas, ResultPanel, FVector2D(620.0f, 350.0f), FVector2D(680.0f, 360.0f), 11);
}


void UGameXXKMetaShopWidget::RefreshFromState()
{
	BuildProgrammaticLayout();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Town || !bIsOpen)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	CurrentProducts = Subsystem->GetMetaShopProducts();
	ApplyProducts(CurrentProducts);
	if (GoldText)
	{
		GoldText->SetText(FText::FromString(FString::Printf(TEXT("元宝：%d"), Subsystem->GetRuntimeState().PlayerGold)));
	}
	if (SelectedProductId == EGameXXKMetaShopProductId::Invalid && !CurrentProducts.IsEmpty())
	{
		SelectedProductId = CurrentProducts[0].ProductId;
	}
	UpdateSelectedProduct();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UGameXXKMetaShopWidget::ApplyProducts(const TArray<FGameXXKMetaShopProductDefinition>& Products)
{
	for (int32 Index = 0; Index < ProductButtons.Num(); ++Index)
	{
		const bool bValid = Products.IsValidIndex(Index);
		ProductButtons[Index]->SetVisibility(bValid ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bValid)
		{
			continue;
		}
		const FGameXXKMetaShopProductDefinition& Product = Products[Index];
		ProductButtons[Index]->Configure(this, Product.ProductId);
		ProductNameTexts[Index]->SetText(Product.DisplayName);
		ProductPriceTexts[Index]->SetText(FText::FromString(FString::Printf(TEXT("%d"), Product.Price)));
		if (UTexture2D* Texture = Cast<UTexture2D>(Product.IconSoftPath.TryLoad()))
		{
			ProductImages[Index]->SetBrushFromTexture(Texture, true);
		}
	}
}

bool UGameXXKMetaShopWidget::SelectProduct(const EGameXXKMetaShopProductId ProductId)
{
	if (!CurrentProducts.ContainsByPredicate([ProductId](const FGameXXKMetaShopProductDefinition& Product)
		{ return Product.ProductId == ProductId; }))
	{
		return false;
	}
	SelectedProductId = ProductId;
	UpdateSelectedProduct();
	return true;
}

void UGameXXKMetaShopWidget::UpdateSelectedProduct()
{
	const FGameXXKMetaShopProductDefinition* Product = CurrentProducts.FindByPredicate(
		[this](const FGameXXKMetaShopProductDefinition& Candidate)
		{ return Candidate.ProductId == SelectedProductId; });
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Product || !Subsystem)
	{
		DisabledReason = FText::FromString(TEXT("商品不可用。"));
		PurchaseButton->SetIsEnabled(false);
		return;
	}
	DetailNameText->SetText(Product->DisplayName);
	DetailDescriptionText->SetText(BuildProductDescription(*Product));
	if (DetailIconImage)
	{
		if (UTexture2D* Texture = Cast<UTexture2D>(Product->IconSoftPath.TryLoad()))
		{
			// Aspect-fit the source texture inside the 72x132 detail slot instead of
			// stretching it (square art forced into the tall slot looked squished).
			const float TexW = static_cast<float>(Texture->GetSizeX());
			const float TexH = static_cast<float>(Texture->GetSizeY());
			const float Scale = FMath::Min(72.0f / TexW, 132.0f / TexH);
			DetailIconImage->SetDesiredSizeOverride(FVector2D(TexW * Scale, TexH * Scale));
			DetailIconImage->SetBrushFromTexture(Texture, true);
			DetailIconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			DetailIconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (Subsystem->PreviewMetaShopPurchase(SelectedProductId, SelectedPreview))
	{
		DisabledReason = FText::GetEmpty();
	}
	else
	{
		DisabledReason = SelectedPreview.Message;
	}
	DisabledReasonText->SetText(DisabledReason);
	PurchaseButton->SetIsEnabled(SelectedPreview.bAvailable);
	// Page 07 selection ink follows the chosen product card.
	if (ProductSelectionInk)
	{
		const int32 SelectedIndex = CurrentProducts.IndexOfByPredicate(
			[this](const FGameXXKMetaShopProductDefinition& Candidate)
			{ return Candidate.ProductId == SelectedProductId; });
		const FVector2D CardPositions[7] = {
			FVector2D(410.0f, 300.0f), FVector2D(630.0f, 300.0f), FVector2D(850.0f, 300.0f), FVector2D(1070.0f, 300.0f),
			FVector2D(520.0f, 610.0f), FVector2D(740.0f, 610.0f), FVector2D(960.0f, 610.0f)};
		const bool bSelected = SelectedIndex >= 0 && SelectedIndex < 7;
		ProductSelectionInk->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bSelected)
		{
			if (UCanvasPanelSlot* InkSlot = Cast<UCanvasPanelSlot>(ProductSelectionInk->Slot))
			{
				InkSlot->SetPosition(CardPositions[SelectedIndex] + SelectionInkOffset);
			}
		}
	}
}

FText UGameXXKMetaShopWidget::BuildProductDescription(const FGameXXKMetaShopProductDefinition& Product) const
{
	if (Product.Kind == EGameXXKMetaShopProductKind::CompanionPack)
	{
		return FText::FromString(TEXT("获得一名永久伙伴。伙伴上限 12 人；满员时进入替换流程，取消候选不退还金币。"));
	}
	return FText::FromString(TEXT("随机获得该套装的武器、头部、护甲、腰带、鞋子或饰品之一。\n普通 70% / 稀有 25% / 珍稀 5%"));
}

bool UGameXXKMetaShopWidget::RequestPurchase()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !Subsystem->PreviewMetaShopPurchase(SelectedProductId, SelectedPreview))
	{
		DisabledReason = SelectedPreview.Message;
		DisabledReasonText->SetText(DisabledReason);
		return false;
	}
	ConfirmOverlay->SetVisibility(ESlateVisibility::Visible);
	ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
	return true;
}

bool UGameXXKMetaShopWidget::ConfirmPurchase()
{
	if (!ConfirmOverlay || ConfirmOverlay->GetVisibility() == ESlateVisibility::Collapsed)
	{
		return false;
	}
	ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	LastPurchaseResult = FGameXXKMetaShopPurchaseResult();
	if (!Subsystem || !Subsystem->PurchaseMetaShopProduct(SelectedProductId, LastPurchaseResult))
	{
		DisabledReason = LastPurchaseResult.Message;
		DisabledReasonText->SetText(DisabledReason);
		ResultText->SetText(BuildPurchaseResultText());
		ResultPanel->SetVisibility(ESlateVisibility::Visible);
		return false;
	}
	RefreshFromState();
	// The result panel shows the purchased item's art: the companion card face
	// for a partner, the equipment icon for a pack.
	if (ResultImage)
	{
		UTexture2D* Texture = nullptr;
		if (!LastPurchaseResult.GeneratedEquipmentId.IsNone())
		{
			const UGameXXKMVPSubsystem* ResolveSub = ResolveMVPSubsystem();
			const FGameXXKEquipmentInstance* Instance = ResolveSub
				? FGameXXKEquipmentRules::FindInstance(ResolveSub->GetRuntimeState().EquipmentCollection, LastPurchaseResult.GeneratedEquipmentId)
				: nullptr;
			const FGameXXKEquipmentDefinition* Definition = Instance
				? FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId)
				: nullptr;
			if (Definition && !Definition->IconSoftPath.IsNull())
			{
				Texture = Cast<UTexture2D>(Definition->IconSoftPath.TryLoad());
			}
		}
		else if (!LastPurchaseResult.CompanionResult.Companion.InstanceId.IsNone())
		{
			Texture = LoadObject<UTexture2D>(nullptr, *ShopCardPortraitPath(LastPurchaseResult.CompanionResult.Companion.Role));
		}
		if (Texture)
		{
			ResultImage->SetBrushFromTexture(Texture, true);
			ResultImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ResultImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		// Hovering the slot shows the generated item's stats, or the partner's name.
		if (ResultImage && WidgetTree)
		{
			if (!LastPurchaseResult.GeneratedEquipmentId.IsNone())
			{
				const UGameXXKMVPSubsystem* ResolveSub = ResolveMVPSubsystem();
				const FGameXXKEquipmentInstance* Instance = ResolveSub
					? FGameXXKEquipmentRules::FindInstance(Subsystem->GetRuntimeState().EquipmentCollection, LastPurchaseResult.GeneratedEquipmentId)
					: nullptr;
				const FGameXXKEquipmentDefinition* Definition = Instance
					? FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId)
					: nullptr;
				if (Instance && Definition)
				{
					ResultSlotFrame->SetToolTip(BuildResultTooltip(WidgetTree,
						FText::FromString(FString::Printf(TEXT("%s\n%s"),
							*Definition->DisplayName.ToString(),
							*ShopEquipmentInstanceDetail(Subsystem, *Instance, *Definition).ToString()))));
				}
			}
			else if (!LastPurchaseResult.CompanionResult.Companion.InstanceId.IsNone())
			{
				const FGameXXKPermanentCompanion& Companion = LastPurchaseResult.CompanionResult.Companion;
				ResultSlotFrame->SetToolTip(BuildResultTooltip(WidgetTree,
					FText::FromString(FString::Printf(TEXT("%s（%s）"),
						*FGameXXKCompanionRules::GetCompanionDisplayName(Companion.Role, Companion.NameSeed),
						*ShopRoleDisplayName(Companion.Role).ToString()))));
			}
		}
	}
	ResultText->SetText(BuildPurchaseResultText());
	ResultPanel->SetVisibility(ESlateVisibility::Visible);
	if (LastPurchaseResult.CompanionResult.Outcome == EGameXXKCompanionRecruitOutcome::PendingReplacement)
	{
		CompanionReplacementRequestedDelegate.ExecuteIfBound();
	}
	NotifyPlayerFlowStateChanged();
	return true;
}

FText UGameXXKMetaShopWidget::BuildPurchaseResultText() const
{
	if (!LastPurchaseResult.bPurchased)
	{
		return LastPurchaseResult.Message.IsEmpty() ? FText::FromString(TEXT("购买失败。")) : LastPurchaseResult.Message;
	}
	if (!LastPurchaseResult.GeneratedEquipmentId.IsNone())
	{
		const UGameXXKMVPSubsystem* ResolveSub = ResolveMVPSubsystem();
		const FGameXXKEquipmentInstance* Instance = ResolveSub
			? FGameXXKEquipmentRules::FindInstance(ResolveSub->GetRuntimeState().EquipmentCollection, LastPurchaseResult.GeneratedEquipmentId)
			: nullptr;
		const FGameXXKEquipmentDefinition* Definition = Instance
			? FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId)
			: nullptr;
		if (Definition)
		{
			return FText::FromString(FString::Printf(
				TEXT("获得装备：%s\n等级 %d，品质：%s"),
				*Definition->DisplayName.ToString(),
				Instance->ItemLevel,
				*ShopEquipmentQualityText(Instance->Quality).ToString()));
		}
		return FText::FromString(TEXT("获得装备一件。"));
	}
	if (!LastPurchaseResult.CompanionResult.Companion.InstanceId.IsNone())
	{
		const FGameXXKPermanentCompanion& Companion = LastPurchaseResult.CompanionResult.Companion;
		return FText::FromString(FString::Printf(
			TEXT("获得伙伴：%s（%s）"),
			*FGameXXKCompanionRules::GetCompanionDisplayName(Companion.Role, Companion.NameSeed),
			*ShopRoleDisplayName(Companion.Role).ToString()));
	}
	return FText::FromString(TEXT("购买成功。"));
}

bool UGameXXKMetaShopWidget::CancelPurchase()
{
	if (!ConfirmOverlay || ConfirmOverlay->GetVisibility() == ESlateVisibility::Collapsed)
	{
		return false;
	}
	ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
	return true;
}

void UGameXXKMetaShopWidget::CloseMetaShop()
{
	bIsOpen = false;
	ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
	ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
	SetVisibility(ESlateVisibility::Collapsed);
	CloseRequestedDelegate.ExecuteIfBound();
}

void UGameXXKMetaShopWidget::SetCloseRequestedDelegate(FSimpleDelegate InDelegate)
{
	CloseRequestedDelegate = MoveTemp(InDelegate);
}

void UGameXXKMetaShopWidget::SetCompanionReplacementRequestedDelegate(FSimpleDelegate InDelegate)
{
	CompanionReplacementRequestedDelegate = MoveTemp(InDelegate);
}

void UGameXXKMetaShopWidget::HandlePurchaseClicked() { RequestPurchase(); }
void UGameXXKMetaShopWidget::HandleConfirmClicked() { ConfirmPurchase(); }
void UGameXXKMetaShopWidget::HandleCancelClicked() { CancelPurchase(); }

void UGameXXKMetaShopWidget::HandleResultConfirmClicked()
{
	if (ResultPanel)
	{
		ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}
void UGameXXKMetaShopWidget::HandleCloseClicked() { CloseMetaShop(); }

bool UGameXXKMetaShopWidget::OpenMetaShop()
{
	BuildProgrammaticLayout();
	ConfirmOverlay->SetVisibility(ESlateVisibility::Collapsed);
	ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
	LastPurchaseResult = FGameXXKMetaShopPurchaseResult();
	bIsOpen = true;
	RefreshFromState();
	return GetVisibility() != ESlateVisibility::Collapsed && CurrentProducts.Num() == 7;
}

bool UGameXXKMetaShopWidget::OpenMetaShopForTest() { return OpenMetaShop(); }

bool UGameXXKMetaShopWidget::SelectProductForTest(const EGameXXKMetaShopProductId ProductId) { return SelectProduct(ProductId); }
bool UGameXXKMetaShopWidget::RequestPurchaseForTest() { return RequestPurchase(); }
bool UGameXXKMetaShopWidget::ConfirmPurchaseForTest() { return ConfirmPurchase(); }
bool UGameXXKMetaShopWidget::CancelPurchaseForTest() { return CancelPurchase(); }
int32 UGameXXKMetaShopWidget::GetProductCardCountForTest() const { return ProductButtons.Num(); }
FText UGameXXKMetaShopWidget::GetDisabledReasonForTest() const { return DisabledReason; }
FGameXXKMetaShopPurchaseResult UGameXXKMetaShopWidget::GetLastPurchaseResultForTest() const { return LastPurchaseResult; }
