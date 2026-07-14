#include "UI/GameXXKInventoryWindowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"

namespace
{
	const FVector2D InventoryWindowSize(1024.0f, 640.0f);
	const FVector2D CloseButtonSize(44.0f, 44.0f);
	const FVector2D BackpackSlotSize(72.0f, 72.0f);
	const FVector2D BackpackIconSize(46.0f, 46.0f);
	const FVector2D EquipmentSlotSize(220.0f, 86.0f);
	const FVector2D ActionButtonSize(206.0f, 44.0f);
	const int32 BackpackSlotCount = 20;
	const int32 BackpackColumns = 5;
	const FName WeaponSlotId(TEXT("Weapon"));
	const FName ArmorSlotId(TEXT("Armor"));
	const FName AccessorySlotId(TEXT("Accessory"));

	const FString TextureRoot(TEXT("/Game/GameXXK/UI/Inventory/Textures/"));
	const FString WindowFrameTexturePath(TextureRoot + TEXT("T_InventoryWindowFrame.T_InventoryWindowFrame"));
	const FString ConfirmationDialogTexturePath(TextureRoot + TEXT("T_InventoryConfirmationDialog.T_InventoryConfirmationDialog"));
	const FString CloseButtonTexturePath(TextureRoot + TEXT("T_InventoryCloseButton.T_InventoryCloseButton"));
	const FString BackpackSlotTexturePath(TextureRoot + TEXT("T_InkBackpackSlot.T_InkBackpackSlot"));
	const FString EquipmentSlotTexturePath(TextureRoot + TEXT("T_InventoryEquipmentSlots.T_InventoryEquipmentSlots"));
	const FString ActionButtonTexturePath(TextureRoot + TEXT("T_InventoryActionButtons.T_InventoryActionButtons"));
	const FString SlotStatesTexturePath(TextureRoot + TEXT("T_InventorySlotStates.T_InventorySlotStates"));

	UTexture2D* LoadTexture(const FString& Path)
	{
		return Path.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *Path);
	}

	FSlateBrush MakeTextureBrush(const FString& Path, const FVector2D& ImageSize, const FLinearColor& Tint = FLinearColor::White)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(LoadTexture(Path));
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.ImageSize = ImageSize;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	FButtonStyle MakeTextureButtonStyle(const FString& Path, const FVector2D& ImageSize, const FLinearColor& Tint = FLinearColor::White)
	{
		const FSlateBrush NormalBrush = MakeTextureBrush(Path, ImageSize, Tint);
		FButtonStyle Style;
		Style.SetNormal(NormalBrush);
		Style.SetHovered(MakeTextureBrush(Path, ImageSize, Tint * FLinearColor(1.08f, 1.08f, 1.08f, 1.0f)));
		Style.SetPressed(MakeTextureBrush(Path, ImageSize, Tint * FLinearColor(0.82f, 0.82f, 0.82f, 1.0f)));
		Style.SetDisabled(MakeTextureBrush(Path, ImageSize, FLinearColor(0.45f, 0.45f, 0.45f, 0.75f)));
		return Style;
	}

	UTextBlock* MakeText(UWidgetTree* WidgetTree, const FText& Text, int32 FontSize = 16, const FLinearColor& Color = FLinearColor(0.12f, 0.09f, 0.06f, 1.0f))
	{
		if (!WidgetTree)
		{
			return nullptr;
		}
		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TextBlock->SetText(Text);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetAutoWrapText(true);
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		return TextBlock;
	}

	UButton* MakeActionButton(UWidgetTree* WidgetTree, const FText& Text, UTextBlock*& OutText)
	{
		if (!WidgetTree)
		{
			return nullptr;
		}
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetStyle(MakeTextureButtonStyle(ActionButtonTexturePath, ActionButtonSize));
		Button->SetBackgroundColor(FLinearColor::White);
		OutText = MakeText(WidgetTree, Text, 16, FLinearColor::White);
		if (OutText)
		{
			OutText->SetJustification(ETextJustify::Center);
			Button->AddChild(OutText);
		}
		return Button;
	}

	void AddCanvasChild(UCanvasPanel* Canvas, UWidget* Child, const FVector2D& Position, const FVector2D& Size, const FAnchors& Anchors = FAnchors(0.0f, 0.0f), const FVector2D& Alignment = FVector2D::ZeroVector)
	{
		if (!Canvas || !Child)
		{
			return;
		}
		if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Child))
		{
			Slot->SetAnchors(Anchors);
			Slot->SetAlignment(Alignment);
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
		}
	}

	FString ResolveItemIconTexturePath(FName ItemId)
	{
		if (ItemId == UGameXXKMVPRules::ItemHealingPowder())
		{
			return TextureRoot + TEXT("T_ItemHealingPowder.T_ItemHealingPowder");
		}
		if (ItemId == FName(TEXT("Item.LingzhiPowder")))
		{
			return TextureRoot + TEXT("T_ItemLingzhiPowder.T_ItemLingzhiPowder");
		}
		if (ItemId == FName(TEXT("Item.QingxinTea")))
		{
			return TextureRoot + TEXT("T_ItemQingxinTea.T_ItemQingxinTea");
		}
		if (ItemId == FName(TEXT("Item.CraneSachet")))
		{
			return TextureRoot + TEXT("T_ItemCraneSachet.T_ItemCraneSachet");
		}
		if (ItemId == UGameXXKMVPRules::ItemIronSword())
		{
			return TextureRoot + TEXT("T_ItemQingfengShortSword.T_ItemQingfengShortSword");
		}
		if (ItemId == UGameXXKMVPRules::ItemClothArmor())
		{
			return TextureRoot + TEXT("T_ItemBambooLightArmor.T_ItemBambooLightArmor");
		}
		if (ItemId == FName(TEXT("Item.CranePatternTalisman")))
		{
			return TextureRoot + TEXT("T_ItemCranePatternTalisman.T_ItemCranePatternTalisman");
		}
		if (ItemId == FName(TEXT("Item.InkstonePendant")))
		{
			return TextureRoot + TEXT("T_ItemInkstonePendant.T_ItemInkstonePendant");
		}
		if (ItemId == UGameXXKMVPRules::ItemWoodenSword())
		{
			return TextureRoot + TEXT("T_ItemWoodenSword.T_ItemWoodenSword");
		}
		if (ItemId == UGameXXKMVPRules::ItemStarterClothArmor())
		{
			return TextureRoot + TEXT("T_ItemStarterClothArmor.T_ItemStarterClothArmor");
		}
		if (ItemId == UGameXXKMVPRules::ItemClothTalisman())
		{
			return TextureRoot + TEXT("T_ItemClothTalisman.T_ItemClothTalisman");
		}
		return FString();
	}

	FText ItemKindText(EGameXXKItemKind Kind)
	{
		switch (Kind)
		{
		case EGameXXKItemKind::Consumable:
			return NSLOCTEXT("GameXXKInventoryWindow", "KindConsumable", "消耗");
		case EGameXXKItemKind::Weapon:
			return NSLOCTEXT("GameXXKInventoryWindow", "KindWeapon", "武器");
		case EGameXXKItemKind::Armor:
			return NSLOCTEXT("GameXXKInventoryWindow", "KindArmor", "防具");
		case EGameXXKItemKind::Accessory:
			return NSLOCTEXT("GameXXKInventoryWindow", "KindAccessory", "饰品");
		default:
			return NSLOCTEXT("GameXXKInventoryWindow", "KindUnknown", "物品");
		}
	}

	FString ItemStatsText(const FGameXXKItemDef& Def)
	{
		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("类型：%s"), *ItemKindText(Def.Kind).ToString()));
		if (Def.HealAmount > 0)
		{
			Lines.Add(FString::Printf(TEXT("恢复 HP +%d"), Def.HealAmount));
		}
		if (Def.MPHealAmount > 0)
		{
			Lines.Add(FString::Printf(TEXT("恢复 MP +%d"), Def.MPHealAmount));
		}
		if (Def.AttackBonus > 0)
		{
			Lines.Add(FString::Printf(TEXT("攻击 +%d"), Def.AttackBonus));
		}
		if (Def.DefenseBonus > 0)
		{
			Lines.Add(FString::Printf(TEXT("防御 +%d"), Def.DefenseBonus));
		}
		if (Def.MaxHPBonus > 0)
		{
			Lines.Add(FString::Printf(TEXT("生命上限 +%d"), Def.MaxHPBonus));
		}
		if (Def.MaxMPBonus > 0)
		{
			Lines.Add(FString::Printf(TEXT("真气上限 +%d"), Def.MaxMPBonus));
		}
		Lines.Add(FString::Printf(TEXT("买入 %d金 / 卖出 %d金"), Def.BuyPrice, Def.SellPrice));
		return FString::Join(Lines, TEXT("\n"));
	}

	FName SlotForItemKind(EGameXXKItemKind Kind)
	{
		if (Kind == EGameXXKItemKind::Weapon)
		{
			return WeaponSlotId;
		}
		if (Kind == EGameXXKItemKind::Armor)
		{
			return ArmorSlotId;
		}
		if (Kind == EGameXXKItemKind::Accessory)
		{
			return AccessorySlotId;
		}
		return NAME_None;
	}
}

void UGameXXKInventorySlotButton::Configure(UGameXXKInventoryWindowWidget* InOwner, EGameXXKInventorySlotSource InSource, int32 InSlotIndex, FName InEquipmentSlotId)
{
	Owner = InOwner;
	Source = InSource;
	SlotIndex = InSlotIndex;
	EquipmentSlotId = InEquipmentSlotId;
	OnClicked.Clear();
	OnClicked.AddDynamic(this, &UGameXXKInventorySlotButton::HandleClicked);
}

void UGameXXKInventorySlotButton::HandleClicked()
{
	if (Owner)
	{
		Owner->HandleConfiguredSlotClicked(Source, SlotIndex, EquipmentSlotId);
	}
}

bool UGameXXKInventoryWindowWidget::OpenFreeInventory()
{
	return OpenInventoryWindow(EGameXXKInventoryWindowMode::FreeInventory);
}

bool UGameXXKInventoryWindowWidget::OpenMerchantTrade()
{
	return OpenInventoryWindow(EGameXXKInventoryWindowMode::MerchantTrade);
}

bool UGameXXKInventoryWindowWidget::CloseInventoryWindow()
{
	CancelDialog();
	WindowMode = EGameXXKInventoryWindowMode::None;
	SelectedSlotSource = EGameXXKInventorySlotSource::None;
	SelectedItemId = NAME_None;
	SelectedSlotIndex = INDEX_NONE;
	SelectedEquipmentSlotId = NAME_None;
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKInventoryWindowWidget::OpenFreeInventoryForTest()
{
	return OpenFreeInventory();
}

bool UGameXXKInventoryWindowWidget::OpenMerchantTradeForTest()
{
	return OpenMerchantTrade();
}

EGameXXKInventoryWindowMode UGameXXKInventoryWindowWidget::GetWindowModeForTest() const
{
	return WindowMode;
}

bool UGameXXKInventoryWindowWidget::HasWindowFrameForTest() const
{
	return WindowFrame != nullptr;
}

bool UGameXXKInventoryWindowWidget::HasCloseButtonForTest() const
{
	return CloseButton != nullptr;
}

bool UGameXXKInventoryWindowWidget::IsWindowVisibleForTest() const
{
	return GetVisibility() != ESlateVisibility::Collapsed && GetVisibility() != ESlateVisibility::Hidden;
}

bool UGameXXKInventoryWindowWidget::IsModalInputLockActiveForTest() const
{
	return WindowMode == EGameXXKInventoryWindowMode::MerchantTrade && IsWindowVisibleForTest();
}

FString UGameXXKInventoryWindowWidget::GetWindowFrameResourcePathForTest() const
{
	return WindowFrameTexturePath;
}

FString UGameXXKInventoryWindowWidget::GetCloseButtonResourcePathForTest() const
{
	return CloseButtonTexturePath;
}

int32 UGameXXKInventoryWindowWidget::GetBackpackSlotCountForTest() const
{
	return BackpackSlotButtons.Num();
}

FString UGameXXKInventoryWindowWidget::GetBackpackSlotResourcePathForTest() const
{
	return BackpackSlotTexturePath;
}

FString UGameXXKInventoryWindowWidget::GetBackpackSlotIconResourcePathForTest(int32 SlotIndex) const
{
	return CurrentBackpackSlotIconPaths.IsValidIndex(SlotIndex) ? CurrentBackpackSlotIconPaths[SlotIndex] : FString();
}

int32 UGameXXKInventoryWindowWidget::GetEquipmentSlotCountForTest() const
{
	return EquipmentSlotButtons.Num();
}

FString UGameXXKInventoryWindowWidget::GetEquipmentSlotResourcePathForTest() const
{
	return EquipmentSlotTexturePath;
}

int32 UGameXXKInventoryWindowWidget::GetMerchantStockSlotCountForTest() const
{
	return MerchantStockSlotButtons.Num();
}

FString UGameXXKInventoryWindowWidget::GetMerchantStockSlotResourcePathForTest() const
{
	return BackpackSlotTexturePath;
}

FText UGameXXKInventoryWindowWidget::GetSelectedPrimaryActionTextForTest() const
{
	return CurrentPrimaryActionText;
}

bool UGameXXKInventoryWindowWidget::SelectPlayerBackpackItemForTest(FName ItemId)
{
	return SelectPlayerBackpackItem(ItemId);
}

bool UGameXXKInventoryWindowWidget::ExecuteSelectedPrimaryActionForTest()
{
	if (SelectedItemId.IsNone())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}

	bool bExecuted = false;
	if (SelectedSlotSource == EGameXXKInventorySlotSource::Equipment)
	{
		bExecuted = Subsystem->UnequipItem(SelectedItemId);
	}
	else if (SelectedSlotSource == EGameXXKInventorySlotSource::PlayerBackpack)
	{
		bool bFound = false;
		const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(SelectedItemId, bFound);
		if (!bFound)
		{
			return false;
		}
		if (Def.Kind == EGameXXKItemKind::Consumable)
		{
			bExecuted = Subsystem->UseItem(SelectedItemId);
		}
		else
		{
			bExecuted = Subsystem->EquipItem(SelectedItemId);
		}
	}

	if (bExecuted)
	{
		RefreshProgrammaticLayout();
	}
	return bExecuted;
}

FName UGameXXKInventoryWindowWidget::GetEquippedItemForSlotForTest(FName SlotId) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return NAME_None;
	}
	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	if (SlotId == WeaponSlotId)
	{
		return State.EquippedWeapon;
	}
	if (SlotId == ArmorSlotId)
	{
		return State.EquippedArmor;
	}
	if (SlotId == AccessorySlotId)
	{
		return State.EquippedAccessory;
	}
	return NAME_None;
}

bool UGameXXKInventoryWindowWidget::SelectMerchantStockSlotForTest(int32 SlotIndex)
{
	return SelectMerchantStockSlot(SlotIndex);
}

bool UGameXXKInventoryWindowWidget::RequestSelectedBuyForTest()
{
	return RequestBuyForSelectedItem();
}

bool UGameXXKInventoryWindowWidget::ConfirmDialogForTest()
{
	return ConfirmDialog();
}

bool UGameXXKInventoryWindowWidget::CancelDialogForTest()
{
	return CancelDialog();
}

bool UGameXXKInventoryWindowWidget::IsConfirmationDialogVisibleForTest() const
{
	return ConfirmationDialogFrame && ConfirmationDialogFrame->GetVisibility() != ESlateVisibility::Collapsed;
}

bool UGameXXKInventoryWindowWidget::HasConfirmationConfirmButtonForTest() const
{
	return ConfirmationConfirmButton != nullptr;
}

bool UGameXXKInventoryWindowWidget::HasConfirmationCancelButtonForTest() const
{
	return ConfirmationCancelButton != nullptr;
}

void UGameXXKInventoryWindowWidget::HandleConfiguredSlotClicked(EGameXXKInventorySlotSource Source, int32 SlotIndex, FName EquipmentSlotId)
{
	if (PendingConfirmationAction != EConfirmationAction::None)
	{
		return;
	}

	bool bSelected = false;
	switch (Source)
	{
	case EGameXXKInventorySlotSource::PlayerBackpack:
		bSelected = SelectPlayerBackpackSlot(SlotIndex);
		break;
	case EGameXXKInventorySlotSource::MerchantStock:
		bSelected = SelectMerchantStockSlot(SlotIndex);
		break;
	case EGameXXKInventorySlotSource::Equipment:
		bSelected = SelectEquipmentSlot(EquipmentSlotId);
		break;
	default:
		break;
	}

	if (bSelected)
	{
		RefreshProgrammaticLayout();
	}
}

void UGameXXKInventoryWindowWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("InventoryWindowWidgetTree"));
	}
	if (!WidgetTree || RootCanvas)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InventoryWindowRoot"));
	WidgetTree->RootWidget = RootCanvas;

	ModalBackdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryModalBackdrop"));
	ModalBackdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.30f));
	AddCanvasChild(RootCanvas, ModalBackdrop, FVector2D::ZeroVector, FVector2D::ZeroVector, FAnchors(0.0f, 0.0f, 1.0f, 1.0f));

	WindowFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryWindowFrame"));
	WindowFrame->SetBrush(MakeTextureBrush(WindowFrameTexturePath, InventoryWindowSize));
	WindowFrame->SetBrushColor(FLinearColor::White);
	WindowFrame->SetPadding(FMargin(42.0f, 38.0f, 42.0f, 38.0f));
	AddCanvasChild(RootCanvas, WindowFrame, FVector2D::ZeroVector, InventoryWindowSize, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f));

	FrameCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("InventoryWindowFrameCanvas"));
	WindowFrame->AddChild(FrameCanvas);

	TitleTextBlock = MakeText(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "TitleInventory", "行囊"), 26, FLinearColor(0.08f, 0.06f, 0.04f, 1.0f));
	AddCanvasChild(FrameCanvas, TitleTextBlock, FVector2D(22.0f, 0.0f), FVector2D(180.0f, 52.0f));

	GoldTextBlock = MakeText(WidgetTree, FText::GetEmpty(), 16);
	AddCanvasChild(FrameCanvas, GoldTextBlock, FVector2D(206.0f, 8.0f), FVector2D(190.0f, 34.0f));

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("InventoryCloseButton"));
	CloseButton->SetStyle(MakeTextureButtonStyle(CloseButtonTexturePath, CloseButtonSize));
	CloseButton->SetBackgroundColor(FLinearColor::White);
	CloseButton->OnClicked.AddDynamic(this, &UGameXXKInventoryWindowWidget::HandleCloseClicked);
	AddCanvasChild(FrameCanvas, CloseButton, FVector2D(894.0f, 0.0f), CloseButtonSize);

	LeftRailFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryLeftRailFrame"));
	LeftRailFrame->SetBrushColor(FLinearColor(0.93f, 0.86f, 0.70f, 0.48f));
	LeftRailFrame->SetPadding(FMargin(12.0f));
	AddCanvasChild(FrameCanvas, LeftRailFrame, FVector2D(0.0f, 78.0f), FVector2D(220.0f, 470.0f));

	UOverlay* LeftRailOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("InventoryLeftRailOverlay"));
	LeftRailFrame->AddChild(LeftRailOverlay);

	EquipmentPanelBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryEquipmentPanel"));
	LeftRailOverlay->AddChildToOverlay(EquipmentPanelBox);

	MerchantStockGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("InventoryMerchantStockGrid"));
	LeftRailOverlay->AddChildToOverlay(MerchantStockGrid);

	UBorder* BackpackFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryBackpackFrame"));
	BackpackFrame->SetBrushColor(FLinearColor(0.95f, 0.89f, 0.74f, 0.42f));
	BackpackFrame->SetPadding(FMargin(14.0f, 16.0f));
	AddCanvasChild(FrameCanvas, BackpackFrame, FVector2D(242.0f, 78.0f), FVector2D(430.0f, 470.0f));

	BackpackGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("InventoryBackpackGrid"));
	BackpackFrame->AddChild(BackpackGrid);

	DetailPanelFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryDetailPanel"));
	DetailPanelFrame->SetBrushColor(FLinearColor(0.92f, 0.84f, 0.67f, 0.52f));
	DetailPanelFrame->SetPadding(FMargin(14.0f));
	AddCanvasChild(FrameCanvas, DetailPanelFrame, FVector2D(694.0f, 78.0f), FVector2D(246.0f, 470.0f));

	UVerticalBox* DetailBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryDetailBox"));
	DetailPanelFrame->AddChild(DetailBox);
	SelectedNameTextBlock = MakeText(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "NoSelectionTitle", "选择物品"), 20, FLinearColor(0.08f, 0.06f, 0.04f, 1.0f));
	DetailBox->AddChildToVerticalBox(SelectedNameTextBlock);
	SelectedDetailTextBlock = MakeText(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "NoSelectionDetail", "从背包、商店或装备槽中选择。"), 15);
	if (UVerticalBoxSlot* DetailTextSlot = DetailBox->AddChildToVerticalBox(SelectedDetailTextBlock))
	{
		DetailTextSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 12.0f));
	}
	UTextBlock* RawPrimaryActionText = nullptr;
	PrimaryActionButton = MakeActionButton(WidgetTree, FText::GetEmpty(), RawPrimaryActionText);
	PrimaryActionTextBlock = RawPrimaryActionText;
	PrimaryActionButton->OnClicked.AddDynamic(this, &UGameXXKInventoryWindowWidget::HandlePrimaryActionClicked);
	if (UVerticalBoxSlot* PrimarySlot = DetailBox->AddChildToVerticalBox(PrimaryActionButton))
	{
		PrimarySlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	}
	UTextBlock* RawSecondaryActionText = nullptr;
	SecondaryActionButton = MakeActionButton(WidgetTree, FText::GetEmpty(), RawSecondaryActionText);
	SecondaryActionTextBlock = RawSecondaryActionText;
	SecondaryActionButton->OnClicked.AddDynamic(this, &UGameXXKInventoryWindowWidget::HandleSecondaryActionClicked);
	if (UVerticalBoxSlot* SecondarySlot = DetailBox->AddChildToVerticalBox(SecondaryActionButton))
	{
		SecondarySlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	}

	for (int32 SlotIndex = 0; SlotIndex < BackpackSlotCount; ++SlotIndex)
	{
		UGameXXKInventorySlotButton* SlotButton = WidgetTree->ConstructWidget<UGameXXKInventorySlotButton>(UGameXXKInventorySlotButton::StaticClass(), *FString::Printf(TEXT("InventoryBackpackSlot_%02d"), SlotIndex));
		SlotButton->Configure(this, EGameXXKInventorySlotSource::PlayerBackpack, SlotIndex);
		SlotButton->SetStyle(MakeTextureButtonStyle(BackpackSlotTexturePath, BackpackSlotSize));
		SlotButton->SetBackgroundColor(FLinearColor::White);

		UOverlay* SlotOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UImage* SlotIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		SlotIcon->SetVisibility(ESlateVisibility::Collapsed);
		SlotOverlay->AddChildToOverlay(SlotIcon);

		UTextBlock* SlotLabel = MakeText(WidgetTree, FText::GetEmpty(), 12, FLinearColor::White);
		SlotLabel->SetJustification(ETextJustify::Right);
		if (UOverlaySlot* LabelSlot = SlotOverlay->AddChildToOverlay(SlotLabel))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Right);
			LabelSlot->SetVerticalAlignment(VAlign_Bottom);
			LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 5.0f, 4.0f));
		}

		UImage* SelectedOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		SelectedOverlay->SetBrush(MakeTextureBrush(SlotStatesTexturePath, BackpackSlotSize, FLinearColor(1.0f, 0.94f, 0.65f, 0.72f)));
		SelectedOverlay->SetVisibility(ESlateVisibility::Collapsed);
		SlotOverlay->AddChildToOverlay(SelectedOverlay);

		SlotButton->AddChild(SlotOverlay);
		USizeBox* SlotSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SlotSizeBox->SetWidthOverride(BackpackSlotSize.X);
		SlotSizeBox->SetHeightOverride(BackpackSlotSize.Y);
		SlotSizeBox->AddChild(SlotButton);
		if (UUniformGridSlot* GridSlot = BackpackGrid->AddChildToUniformGrid(SlotSizeBox, SlotIndex / BackpackColumns, SlotIndex % BackpackColumns))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}
		BackpackSlotButtons.Add(SlotButton);
		BackpackSlotIcons.Add(SlotIcon);
		BackpackSlotLabels.Add(SlotLabel);
		BackpackSelectedOverlays.Add(SelectedOverlay);
	}

	const TArray<FName> ShopItems = UGameXXKMVPRules::GetShopItemIds();
	for (int32 SlotIndex = 0; SlotIndex < ShopItems.Num(); ++SlotIndex)
	{
		UGameXXKInventorySlotButton* SlotButton = WidgetTree->ConstructWidget<UGameXXKInventorySlotButton>(UGameXXKInventorySlotButton::StaticClass(), *FString::Printf(TEXT("InventoryMerchantStockSlot_%02d"), SlotIndex));
		SlotButton->Configure(this, EGameXXKInventorySlotSource::MerchantStock, SlotIndex);
		SlotButton->SetStyle(MakeTextureButtonStyle(BackpackSlotTexturePath, BackpackSlotSize));
		SlotButton->SetBackgroundColor(FLinearColor::White);

		UOverlay* SlotOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UImage* SlotIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		SlotOverlay->AddChildToOverlay(SlotIcon);
		UTextBlock* SlotLabel = MakeText(WidgetTree, FText::GetEmpty(), 11, FLinearColor::White);
		SlotLabel->SetJustification(ETextJustify::Right);
		if (UOverlaySlot* LabelSlot = SlotOverlay->AddChildToOverlay(SlotLabel))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Right);
			LabelSlot->SetVerticalAlignment(VAlign_Bottom);
			LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 4.0f));
		}
		UImage* SelectedOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		SelectedOverlay->SetBrush(MakeTextureBrush(SlotStatesTexturePath, BackpackSlotSize, FLinearColor(1.0f, 0.94f, 0.65f, 0.72f)));
		SelectedOverlay->SetVisibility(ESlateVisibility::Collapsed);
		SlotOverlay->AddChildToOverlay(SelectedOverlay);
		SlotButton->AddChild(SlotOverlay);

		USizeBox* SlotSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SlotSizeBox->SetWidthOverride(BackpackSlotSize.X);
		SlotSizeBox->SetHeightOverride(BackpackSlotSize.Y);
		SlotSizeBox->AddChild(SlotButton);
		if (UUniformGridSlot* GridSlot = MerchantStockGrid->AddChildToUniformGrid(SlotSizeBox, SlotIndex / 2, SlotIndex % 2))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}
		MerchantStockSlotButtons.Add(SlotButton);
		MerchantStockSlotIcons.Add(SlotIcon);
		MerchantStockSlotLabels.Add(SlotLabel);
		MerchantStockSelectedOverlays.Add(SelectedOverlay);
	}

	const TArray<TPair<FName, FText>> EquipmentSlots = {
		TPair<FName, FText>(WeaponSlotId, NSLOCTEXT("GameXXKInventoryWindow", "WeaponSlot", "武器")),
		TPair<FName, FText>(ArmorSlotId, NSLOCTEXT("GameXXKInventoryWindow", "ArmorSlot", "防具")),
		TPair<FName, FText>(AccessorySlotId, NSLOCTEXT("GameXXKInventoryWindow", "AccessorySlot", "饰品")),
	};
	for (int32 SlotIndex = 0; SlotIndex < EquipmentSlots.Num(); ++SlotIndex)
	{
		const TPair<FName, FText>& SlotDef = EquipmentSlots[SlotIndex];
		UGameXXKInventorySlotButton* SlotButton = WidgetTree->ConstructWidget<UGameXXKInventorySlotButton>(UGameXXKInventorySlotButton::StaticClass(), *FString::Printf(TEXT("InventoryEquipmentSlot_%s"), *SlotDef.Key.ToString()));
		SlotButton->Configure(this, EGameXXKInventorySlotSource::Equipment, SlotIndex, SlotDef.Key);
		SlotButton->SetStyle(MakeTextureButtonStyle(EquipmentSlotTexturePath, EquipmentSlotSize));
		SlotButton->SetBackgroundColor(FLinearColor::White);

		UOverlay* SlotOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		UImage* SlotIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		SlotIcon->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* IconSlot = SlotOverlay->AddChildToOverlay(SlotIcon))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Left);
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));
		}
		UTextBlock* SlotLabel = MakeText(WidgetTree, SlotDef.Value, 14, FLinearColor(0.10f, 0.08f, 0.05f, 1.0f));
		if (UOverlaySlot* LabelSlot = SlotOverlay->AddChildToOverlay(SlotLabel))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Fill);
			LabelSlot->SetVerticalAlignment(VAlign_Center);
			LabelSlot->SetPadding(FMargin(68.0f, 0.0f, 10.0f, 0.0f));
		}
		UImage* SelectedOverlay = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		SelectedOverlay->SetBrush(MakeTextureBrush(SlotStatesTexturePath, EquipmentSlotSize, FLinearColor(1.0f, 0.94f, 0.65f, 0.72f)));
		SelectedOverlay->SetVisibility(ESlateVisibility::Collapsed);
		SlotOverlay->AddChildToOverlay(SelectedOverlay);
		SlotButton->AddChild(SlotOverlay);

		USizeBox* SlotSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		SlotSizeBox->SetWidthOverride(EquipmentSlotSize.X);
		SlotSizeBox->SetHeightOverride(EquipmentSlotSize.Y);
		SlotSizeBox->AddChild(SlotButton);
		if (UVerticalBoxSlot* EquipmentSlot = EquipmentPanelBox->AddChildToVerticalBox(SlotSizeBox))
		{
			EquipmentSlot->SetPadding(FMargin(0.0f, SlotIndex == 0 ? 0.0f : 12.0f, 0.0f, 0.0f));
			EquipmentSlot->SetHorizontalAlignment(HAlign_Center);
		}
		EquipmentSlotButtons.Add(SlotButton);
		EquipmentSlotIcons.Add(SlotIcon);
		EquipmentSlotLabels.Add(SlotLabel);
		EquipmentSelectedOverlays.Add(SelectedOverlay);
	}

	ConfirmationDialogFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryConfirmationDialogFrame"));
	ConfirmationDialogFrame->SetBrush(MakeTextureBrush(ConfirmationDialogTexturePath, FVector2D(520.0f, 260.0f)));
	ConfirmationDialogFrame->SetBrushColor(FLinearColor::White);
	ConfirmationDialogFrame->SetPadding(FMargin(34.0f, 30.0f));
	AddCanvasChild(RootCanvas, ConfirmationDialogFrame, FVector2D::ZeroVector, FVector2D(520.0f, 260.0f), FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f));

	UVerticalBox* DialogBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryConfirmationDialogBox"));
	ConfirmationDialogFrame->AddChild(DialogBox);
	ConfirmationPromptTextBlock = MakeText(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "ConfirmPrompt", "确认操作？"), 18, FLinearColor(0.08f, 0.06f, 0.04f, 1.0f));
	DialogBox->AddChildToVerticalBox(ConfirmationPromptTextBlock);
	ConfirmationSummaryTextBlock = MakeText(WidgetTree, FText::GetEmpty(), 15);
	if (UVerticalBoxSlot* SummarySlot = DialogBox->AddChildToVerticalBox(ConfirmationSummaryTextBlock))
	{
		SummarySlot->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 14.0f));
	}
	UHorizontalBox* DialogButtons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	DialogBox->AddChildToVerticalBox(DialogButtons);
	UTextBlock* ConfirmText = nullptr;
	ConfirmationConfirmButton = MakeActionButton(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "Confirm", "确认"), ConfirmText);
	ConfirmationConfirmButton->OnClicked.AddDynamic(this, &UGameXXKInventoryWindowWidget::HandleConfirmClicked);
	if (UHorizontalBoxSlot* ConfirmSlot = DialogButtons->AddChildToHorizontalBox(ConfirmationConfirmButton))
	{
		ConfirmSlot->SetPadding(FMargin(0.0f, 0.0f, 18.0f, 0.0f));
		ConfirmSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	UTextBlock* CancelText = nullptr;
	ConfirmationCancelButton = MakeActionButton(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "Cancel", "取消"), CancelText);
	ConfirmationCancelButton->OnClicked.AddDynamic(this, &UGameXXKInventoryWindowWidget::HandleCancelClicked);
	if (UHorizontalBoxSlot* CancelSlot = DialogButtons->AddChildToHorizontalBox(ConfirmationCancelButton))
	{
		CancelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	RefreshProgrammaticLayout();
}

void UGameXXKInventoryWindowWidget::RefreshProgrammaticLayout()
{
	BuildProgrammaticLayout();
	const bool bWindowVisible = WindowMode != EGameXXKInventoryWindowMode::None;
	SetVisibility(bWindowVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (ModalBackdrop)
	{
		ModalBackdrop->SetVisibility(WindowMode == EGameXXKInventoryWindowMode::MerchantTrade ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (WindowFrame)
	{
		WindowFrame->SetVisibility(bWindowVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (CloseButton)
	{
		CloseButton->SetVisibility(bWindowVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (TitleTextBlock)
	{
		TitleTextBlock->SetText(WindowMode == EGameXXKInventoryWindowMode::MerchantTrade
			? NSLOCTEXT("GameXXKInventoryWindow", "TitleTrade", "商铺")
			: NSLOCTEXT("GameXXKInventoryWindow", "TitleInventory", "行囊"));
	}
	if (GoldTextBlock)
	{
		const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
		GoldTextBlock->SetText(FText::FromString(Subsystem
			? FString::Printf(TEXT("金 %d"), Subsystem->GetRuntimeState().PlayerGold)
			: TEXT("金 0")));
	}

	if (EquipmentPanelBox)
	{
		EquipmentPanelBox->SetVisibility(WindowMode == EGameXXKInventoryWindowMode::MerchantTrade ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (MerchantStockGrid)
	{
		MerchantStockGrid->SetVisibility(WindowMode == EGameXXKInventoryWindowMode::MerchantTrade ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	RefreshBackpackSlots();
	RefreshMerchantStockSlots();
	RefreshEquipmentSlots();
	RefreshDetailPanel();
	RefreshConfirmationDialog();
}

void UGameXXKInventoryWindowWidget::RefreshBackpackSlots()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	TArray<TPair<FName, int32>> InventoryEntries;
	if (Subsystem)
	{
		for (const TPair<FName, int32>& Entry : Subsystem->GetRuntimeState().Inventory)
		{
			if (Entry.Value > 0)
			{
				InventoryEntries.Add(Entry);
			}
		}
	}
	const TArray<FName> KnownItemIds = UGameXXKMVPRules::GetKnownItemIds();
	InventoryEntries.Sort([&KnownItemIds](const TPair<FName, int32>& A, const TPair<FName, int32>& B)
	{
		const int32 AIndex = KnownItemIds.IndexOfByKey(A.Key);
		const int32 BIndex = KnownItemIds.IndexOfByKey(B.Key);
		if (AIndex != INDEX_NONE || BIndex != INDEX_NONE)
		{
			if (AIndex == INDEX_NONE)
			{
				return false;
			}
			if (BIndex == INDEX_NONE)
			{
				return true;
			}
			return AIndex < BIndex;
		}
		return A.Key.ToString() < B.Key.ToString();
	});

	CurrentBackpackSlotItemIds.Reset();
	CurrentBackpackSlotIconPaths.Reset();
	for (int32 SlotIndex = 0; SlotIndex < BackpackSlotButtons.Num(); ++SlotIndex)
	{
		const bool bHasItem = InventoryEntries.IsValidIndex(SlotIndex);
		const FName ItemId = bHasItem ? InventoryEntries[SlotIndex].Key : NAME_None;
		const int32 Quantity = bHasItem ? InventoryEntries[SlotIndex].Value : 0;
		CurrentBackpackSlotItemIds.Add(ItemId);
		const FString IconPath = ResolveItemIconTexturePath(ItemId);
		CurrentBackpackSlotIconPaths.Add(IconPath);

		if (UGameXXKInventorySlotButton* SlotButton = BackpackSlotButtons[SlotIndex])
		{
			SlotButton->SetIsEnabled(bHasItem);
		}
		if (UTextBlock* Label = BackpackSlotLabels.IsValidIndex(SlotIndex) ? BackpackSlotLabels[SlotIndex].Get() : nullptr)
		{
			Label->SetText(bHasItem ? FText::FromString(FString::Printf(TEXT("x%d"), Quantity)) : FText::GetEmpty());
		}
		if (UImage* Icon = BackpackSlotIcons.IsValidIndex(SlotIndex) ? BackpackSlotIcons[SlotIndex].Get() : nullptr)
		{
			if (UTexture2D* Texture = LoadTexture(IconPath))
			{
				Icon->SetBrushFromTexture(Texture, true);
				FSlateBrush Brush = Icon->GetBrush();
				Brush.ImageSize = BackpackIconSize;
				Icon->SetBrush(Brush);
				Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				Icon->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		if (UImage* SelectedOverlay = BackpackSelectedOverlays.IsValidIndex(SlotIndex) ? BackpackSelectedOverlays[SlotIndex].Get() : nullptr)
		{
			SelectedOverlay->SetVisibility(SelectedSlotSource == EGameXXKInventorySlotSource::PlayerBackpack && SelectedSlotIndex == SlotIndex ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}

void UGameXXKInventoryWindowWidget::RefreshMerchantStockSlots()
{
	const TArray<FName> ShopItems = UGameXXKMVPRules::GetShopItemIds();
	CurrentMerchantStockSlotItemIds = ShopItems;
	for (int32 SlotIndex = 0; SlotIndex < MerchantStockSlotButtons.Num(); ++SlotIndex)
	{
		const bool bHasItem = ShopItems.IsValidIndex(SlotIndex);
		const FName ItemId = bHasItem ? ShopItems[SlotIndex] : NAME_None;
		bool bFound = false;
		const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(ItemId, bFound);
		if (UGameXXKInventorySlotButton* SlotButton = MerchantStockSlotButtons[SlotIndex])
		{
			SlotButton->SetIsEnabled(bHasItem && bFound);
		}
		if (UTextBlock* Label = MerchantStockSlotLabels.IsValidIndex(SlotIndex) ? MerchantStockSlotLabels[SlotIndex].Get() : nullptr)
		{
			Label->SetText(bFound ? FText::FromString(FString::Printf(TEXT("%d金"), Def.BuyPrice)) : FText::GetEmpty());
		}
		if (UImage* Icon = MerchantStockSlotIcons.IsValidIndex(SlotIndex) ? MerchantStockSlotIcons[SlotIndex].Get() : nullptr)
		{
			if (UTexture2D* Texture = LoadTexture(ResolveItemIconTexturePath(ItemId)))
			{
				Icon->SetBrushFromTexture(Texture, true);
				FSlateBrush Brush = Icon->GetBrush();
				Brush.ImageSize = BackpackIconSize;
				Icon->SetBrush(Brush);
				Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				Icon->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		if (UImage* SelectedOverlay = MerchantStockSelectedOverlays.IsValidIndex(SlotIndex) ? MerchantStockSelectedOverlays[SlotIndex].Get() : nullptr)
		{
			SelectedOverlay->SetVisibility(SelectedSlotSource == EGameXXKInventorySlotSource::MerchantStock && SelectedSlotIndex == SlotIndex ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}

void UGameXXKInventoryWindowWidget::RefreshEquipmentSlots()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	CurrentEquipmentSlotItemIds.Reset();
	const FName EquippedIds[3] = {
		Subsystem ? Subsystem->GetRuntimeState().EquippedWeapon : NAME_None,
		Subsystem ? Subsystem->GetRuntimeState().EquippedArmor : NAME_None,
		Subsystem ? Subsystem->GetRuntimeState().EquippedAccessory : NAME_None,
	};
	const FName SlotIds[3] = { WeaponSlotId, ArmorSlotId, AccessorySlotId };
	const FText EmptyLabels[3] = {
		NSLOCTEXT("GameXXKInventoryWindow", "WeaponEmpty", "武器\n空"),
		NSLOCTEXT("GameXXKInventoryWindow", "ArmorEmpty", "防具\n空"),
		NSLOCTEXT("GameXXKInventoryWindow", "AccessoryEmpty", "饰品\n空"),
	};
	for (int32 SlotIndex = 0; SlotIndex < EquipmentSlotButtons.Num(); ++SlotIndex)
	{
		const FName ItemId = EquippedIds[SlotIndex];
		CurrentEquipmentSlotItemIds.Add(ItemId);
		bool bFound = false;
		const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(ItemId, bFound);
		if (UTextBlock* Label = EquipmentSlotLabels.IsValidIndex(SlotIndex) ? EquipmentSlotLabels[SlotIndex].Get() : nullptr)
		{
			Label->SetText(bFound ? FText::Format(NSLOCTEXT("GameXXKInventoryWindow", "EquipmentFilled", "{0}\n{1}"), EmptyLabels[SlotIndex].ToString().StartsWith(TEXT("武器")) ? FText::FromString(TEXT("武器")) : EmptyLabels[SlotIndex].ToString().StartsWith(TEXT("防具")) ? FText::FromString(TEXT("防具")) : FText::FromString(TEXT("饰品")), Def.DisplayName) : EmptyLabels[SlotIndex]);
		}
		if (UImage* Icon = EquipmentSlotIcons.IsValidIndex(SlotIndex) ? EquipmentSlotIcons[SlotIndex].Get() : nullptr)
		{
			if (UTexture2D* Texture = LoadTexture(ResolveItemIconTexturePath(ItemId)))
			{
				Icon->SetBrushFromTexture(Texture, true);
				FSlateBrush Brush = Icon->GetBrush();
				Brush.ImageSize = FVector2D(42.0f, 42.0f);
				Icon->SetBrush(Brush);
				Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				Icon->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		if (UImage* SelectedOverlay = EquipmentSelectedOverlays.IsValidIndex(SlotIndex) ? EquipmentSelectedOverlays[SlotIndex].Get() : nullptr)
		{
			SelectedOverlay->SetVisibility(SelectedSlotSource == EGameXXKInventorySlotSource::Equipment && SelectedEquipmentSlotId == SlotIds[SlotIndex] ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
}

void UGameXXKInventoryWindowWidget::RefreshDetailPanel()
{
	CurrentPrimaryActionText = FText::GetEmpty();
	CurrentSecondaryActionText = FText::GetEmpty();

	bool bFound = false;
	const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(SelectedItemId, bFound);
	if (!bFound)
	{
		if (SelectedNameTextBlock)
		{
			SelectedNameTextBlock->SetText(NSLOCTEXT("GameXXKInventoryWindow", "NoSelectionTitle", "选择物品"));
		}
		if (SelectedDetailTextBlock)
		{
			SelectedDetailTextBlock->SetText(NSLOCTEXT("GameXXKInventoryWindow", "NoSelectionDetail", "从背包、商店或装备槽中选择。"));
		}
		if (PrimaryActionButton)
		{
			PrimaryActionButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (SecondaryActionButton)
		{
			SecondaryActionButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (SelectedNameTextBlock)
	{
		SelectedNameTextBlock->SetText(Def.DisplayName);
	}
	if (SelectedDetailTextBlock)
	{
		SelectedDetailTextBlock->SetText(FText::FromString(ItemStatsText(Def)));
	}

	if (SelectedSlotSource == EGameXXKInventorySlotSource::PlayerBackpack)
	{
		CurrentPrimaryActionText = Def.Kind == EGameXXKItemKind::Consumable
			? NSLOCTEXT("GameXXKInventoryWindow", "UseAction", "使用")
			: NSLOCTEXT("GameXXKInventoryWindow", "EquipAction", "装备");
		if (WindowMode == EGameXXKInventoryWindowMode::MerchantTrade)
		{
			CurrentSecondaryActionText = NSLOCTEXT("GameXXKInventoryWindow", "SellAction", "出售");
		}
	}
	else if (SelectedSlotSource == EGameXXKInventorySlotSource::MerchantStock)
	{
		CurrentPrimaryActionText = NSLOCTEXT("GameXXKInventoryWindow", "BuyAction", "购买");
	}
	else if (SelectedSlotSource == EGameXXKInventorySlotSource::Equipment)
	{
		CurrentPrimaryActionText = NSLOCTEXT("GameXXKInventoryWindow", "UnequipAction", "卸下");
	}

	if (PrimaryActionButton && PrimaryActionTextBlock)
	{
		PrimaryActionTextBlock->SetText(CurrentPrimaryActionText);
		PrimaryActionButton->SetVisibility(CurrentPrimaryActionText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (SecondaryActionButton && SecondaryActionTextBlock)
	{
		SecondaryActionTextBlock->SetText(CurrentSecondaryActionText);
		SecondaryActionButton->SetVisibility(CurrentSecondaryActionText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}

void UGameXXKInventoryWindowWidget::RefreshConfirmationDialog()
{
	if (!ConfirmationDialogFrame)
	{
		return;
	}
	const bool bVisible = PendingConfirmationAction != EConfirmationAction::None && !PendingConfirmationItemId.IsNone();
	ConfirmationDialogFrame->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (!bVisible)
	{
		return;
	}

	bool bFound = false;
	const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(PendingConfirmationItemId, bFound);
	const FText ItemName = bFound ? Def.DisplayName : FText::FromName(PendingConfirmationItemId);
	if (ConfirmationPromptTextBlock)
	{
		ConfirmationPromptTextBlock->SetText(PendingConfirmationAction == EConfirmationAction::Buy
			? FText::Format(NSLOCTEXT("GameXXKInventoryWindow", "BuyConfirmPrompt", "购买 {0}？"), ItemName)
			: FText::Format(NSLOCTEXT("GameXXKInventoryWindow", "SellConfirmPrompt", "卖出 {0}？"), ItemName));
	}
	if (ConfirmationSummaryTextBlock)
	{
		ConfirmationSummaryTextBlock->SetText(FText::FromString(FString::Printf(TEXT("数量 x%d    金 %d"), FMath::Max(1, PendingConfirmationQuantity), PendingConfirmationPrice)));
	}
}

bool UGameXXKInventoryWindowWidget::OpenInventoryWindow(EGameXXKInventoryWindowMode InMode)
{
	if (InMode == EGameXXKInventoryWindowMode::None)
	{
		return false;
	}
	WindowMode = InMode;
	PendingConfirmationAction = EConfirmationAction::None;
	PendingConfirmationItemId = NAME_None;
	PendingConfirmationQuantity = 0;
	PendingConfirmationPrice = 0;
	BuildProgrammaticLayout();
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKInventoryWindowWidget::SelectPlayerBackpackItem(FName ItemId)
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || ItemId.IsNone() || UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), ItemId) <= 0)
	{
		return false;
	}
	SelectedSlotSource = EGameXXKInventorySlotSource::PlayerBackpack;
	SelectedItemId = ItemId;
	SelectedSlotIndex = CurrentBackpackSlotItemIds.IndexOfByKey(ItemId);
	SelectedEquipmentSlotId = NAME_None;
	RefreshDetailPanel();
	return true;
}

bool UGameXXKInventoryWindowWidget::SelectPlayerBackpackSlot(int32 SlotIndex)
{
	if (!CurrentBackpackSlotItemIds.IsValidIndex(SlotIndex) || CurrentBackpackSlotItemIds[SlotIndex].IsNone())
	{
		return false;
	}
	SelectedSlotSource = EGameXXKInventorySlotSource::PlayerBackpack;
	SelectedItemId = CurrentBackpackSlotItemIds[SlotIndex];
	SelectedSlotIndex = SlotIndex;
	SelectedEquipmentSlotId = NAME_None;
	return true;
}

bool UGameXXKInventoryWindowWidget::SelectMerchantStockSlot(int32 SlotIndex)
{
	const TArray<FName> ShopItems = UGameXXKMVPRules::GetShopItemIds();
	if (!ShopItems.IsValidIndex(SlotIndex))
	{
		return false;
	}
	SelectedSlotSource = EGameXXKInventorySlotSource::MerchantStock;
	SelectedItemId = ShopItems[SlotIndex];
	SelectedSlotIndex = SlotIndex;
	SelectedEquipmentSlotId = NAME_None;
	RefreshDetailPanel();
	return true;
}

bool UGameXXKInventoryWindowWidget::SelectEquipmentSlot(FName SlotId)
{
	const FName ItemId = GetEquippedItemForSlotForTest(SlotId);
	if (ItemId.IsNone())
	{
		return false;
	}
	SelectedSlotSource = EGameXXKInventorySlotSource::Equipment;
	SelectedItemId = ItemId;
	SelectedSlotIndex = INDEX_NONE;
	SelectedEquipmentSlotId = SlotId;
	return true;
}

bool UGameXXKInventoryWindowWidget::RequestBuyForSelectedItem()
{
	if (WindowMode != EGameXXKInventoryWindowMode::MerchantTrade || SelectedSlotSource != EGameXXKInventorySlotSource::MerchantStock || SelectedItemId.IsNone())
	{
		return false;
	}
	bool bFound = false;
	const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(SelectedItemId, bFound);
	if (!bFound)
	{
		return false;
	}
	PendingConfirmationAction = EConfirmationAction::Buy;
	PendingConfirmationItemId = SelectedItemId;
	PendingConfirmationQuantity = 1;
	PendingConfirmationPrice = Def.BuyPrice;
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKInventoryWindowWidget::RequestSellForSelectedItem()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (WindowMode != EGameXXKInventoryWindowMode::MerchantTrade
		|| SelectedSlotSource != EGameXXKInventorySlotSource::PlayerBackpack
		|| SelectedItemId.IsNone()
		|| !Subsystem
		|| UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), SelectedItemId) <= 0)
	{
		return false;
	}
	bool bFound = false;
	const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(SelectedItemId, bFound);
	if (!bFound)
	{
		return false;
	}
	PendingConfirmationAction = EConfirmationAction::Sell;
	PendingConfirmationItemId = SelectedItemId;
	PendingConfirmationQuantity = 1;
	PendingConfirmationPrice = Def.SellPrice;
	RefreshProgrammaticLayout();
	return true;
}

bool UGameXXKInventoryWindowWidget::ConfirmDialog()
{
	if (PendingConfirmationAction == EConfirmationAction::None || PendingConfirmationItemId.IsNone())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}

	bool bExecuted = false;
	if (PendingConfirmationAction == EConfirmationAction::Buy)
	{
		bExecuted = Subsystem->BuyItem(PendingConfirmationItemId, FMath::Max(1, PendingConfirmationQuantity));
	}
	else if (PendingConfirmationAction == EConfirmationAction::Sell)
	{
		bExecuted = Subsystem->SellItem(PendingConfirmationItemId, FMath::Max(1, PendingConfirmationQuantity));
	}

	if (bExecuted)
	{
		CancelDialog();
		RefreshProgrammaticLayout();
	}
	return bExecuted;
}

bool UGameXXKInventoryWindowWidget::CancelDialog()
{
	PendingConfirmationAction = EConfirmationAction::None;
	PendingConfirmationItemId = NAME_None;
	PendingConfirmationQuantity = 0;
	PendingConfirmationPrice = 0;
	if (ConfirmationDialogFrame)
	{
		ConfirmationDialogFrame->SetVisibility(ESlateVisibility::Collapsed);
	}
	return true;
}

void UGameXXKInventoryWindowWidget::HandleCloseClicked()
{
	if (AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController())
	{
		PlayerController->CloseInventoryWindow();
		return;
	}
	CloseInventoryWindow();
}

void UGameXXKInventoryWindowWidget::HandlePrimaryActionClicked()
{
	if (SelectedSlotSource == EGameXXKInventorySlotSource::MerchantStock)
	{
		RequestBuyForSelectedItem();
		return;
	}
	ExecuteSelectedPrimaryActionForTest();
}

void UGameXXKInventoryWindowWidget::HandleSecondaryActionClicked()
{
	RequestSellForSelectedItem();
}

void UGameXXKInventoryWindowWidget::HandleConfirmClicked()
{
	ConfirmDialog();
}

void UGameXXKInventoryWindowWidget::HandleCancelClicked()
{
	CancelDialog();
}
