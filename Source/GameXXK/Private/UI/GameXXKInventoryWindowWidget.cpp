#include "UI/GameXXKInventoryWindowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"

namespace
{
	const FVector2D InventoryWindowPosition(260.0f, 78.0f);
	const FVector2D InventoryWindowSize(940.0f, 600.0f);
	const FName WeaponSlotId(TEXT("Weapon"));
	const FName ArmorSlotId(TEXT("Armor"));
	const FName AccessorySlotId(TEXT("Accessory"));

	UTextBlock* MakeText(UWidgetTree* WidgetTree, const FText& Text, int32 FontSize = 16)
	{
		if (!WidgetTree)
		{
			return nullptr;
		}
		UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		TextBlock->SetText(Text);
		TextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.96f, 0.90f, 0.76f, 1.0f)));
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = FontSize;
		TextBlock->SetFont(Font);
		return TextBlock;
	}

	UButton* MakeTextButton(UWidgetTree* WidgetTree, const FText& Text)
	{
		if (!WidgetTree)
		{
			return nullptr;
		}
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetBackgroundColor(FLinearColor(0.14f, 0.12f, 0.09f, 0.94f));
		UTextBlock* Label = MakeText(WidgetTree, Text, 15);
		if (Label)
		{
			Label->SetJustification(ETextJustify::Center);
			Button->AddChild(Label);
		}
		return Button;
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
	SelectedSlotSource = ESelectedSlotSource::None;
	SelectedItemId = NAME_None;
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

bool UGameXXKInventoryWindowWidget::IsModalInputLockActiveForTest() const
{
	return WindowMode == EGameXXKInventoryWindowMode::MerchantTrade;
}

bool UGameXXKInventoryWindowWidget::SelectPlayerBackpackItemForTest(FName ItemId)
{
	return SelectPlayerBackpackItem(ItemId);
}

bool UGameXXKInventoryWindowWidget::ExecuteSelectedPrimaryActionForTest()
{
	if (SelectedItemId.IsNone() || SelectedSlotSource != ESelectedSlotSource::PlayerBackpack)
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}

	bool bFound = false;
	const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(SelectedItemId, bFound);
	if (!bFound)
	{
		return false;
	}

	bool bExecuted = false;
	if (Def.Kind == EGameXXKItemKind::Consumable)
	{
		bExecuted = Subsystem->UseItem(SelectedItemId);
	}
	else
	{
		bExecuted = Subsystem->EquipItem(SelectedItemId);
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

	WindowFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryWindowFrame"));
	WindowFrame->SetBrushColor(FLinearColor(0.20f, 0.16f, 0.10f, 0.94f));
	WindowFrame->SetPadding(FMargin(18.0f, 14.0f));
	if (UCanvasPanelSlot* FrameSlot = RootCanvas->AddChildToCanvas(WindowFrame))
	{
		FrameSlot->SetPosition(InventoryWindowPosition);
		FrameSlot->SetSize(InventoryWindowSize);
	}

	UVerticalBox* FrameBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryWindowFrameBox"));
	WindowFrame->AddChild(FrameBox);
	TitleTextBlock = MakeText(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "TitleInventory", "行囊"), 22);
	if (TitleTextBlock)
	{
		FrameBox->AddChildToVerticalBox(TitleTextBlock);
	}
	GoldTextBlock = MakeText(WidgetTree, FText::GetEmpty(), 15);
	if (GoldTextBlock)
	{
		FrameBox->AddChildToVerticalBox(GoldTextBlock);
	}

	CloseButton = MakeTextButton(WidgetTree, FText::FromString(TEXT("X")));
	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UGameXXKInventoryWindowWidget::HandleCloseClicked);
		if (UCanvasPanelSlot* CloseSlot = RootCanvas->AddChildToCanvas(CloseButton))
		{
			CloseSlot->SetPosition(InventoryWindowPosition + FVector2D(InventoryWindowSize.X - 58.0f, 18.0f));
			CloseSlot->SetSize(FVector2D(44.0f, 44.0f));
		}
	}

	ConfirmationDialogFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryConfirmationDialogFrame"));
	ConfirmationDialogFrame->SetBrushColor(FLinearColor(0.12f, 0.09f, 0.06f, 0.98f));
	ConfirmationDialogFrame->SetPadding(FMargin(16.0f));
	if (UCanvasPanelSlot* DialogSlot = RootCanvas->AddChildToCanvas(ConfirmationDialogFrame))
	{
		DialogSlot->SetPosition(InventoryWindowPosition + FVector2D(210.0f, 176.0f));
		DialogSlot->SetSize(FVector2D(520.0f, 220.0f));
	}

	UVerticalBox* DialogBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryConfirmationDialogBox"));
	ConfirmationDialogFrame->AddChild(DialogBox);
	DialogBox->AddChildToVerticalBox(MakeText(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "ConfirmPrompt", "确认操作？"), 18));
	ConfirmationConfirmButton = MakeTextButton(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "Confirm", "确认"));
	if (ConfirmationConfirmButton)
	{
		ConfirmationConfirmButton->OnClicked.AddDynamic(this, &UGameXXKInventoryWindowWidget::HandleConfirmClicked);
		DialogBox->AddChildToVerticalBox(ConfirmationConfirmButton);
	}
	ConfirmationCancelButton = MakeTextButton(WidgetTree, NSLOCTEXT("GameXXKInventoryWindow", "Cancel", "取消"));
	if (ConfirmationCancelButton)
	{
		ConfirmationCancelButton->OnClicked.AddDynamic(this, &UGameXXKInventoryWindowWidget::HandleCancelClicked);
		DialogBox->AddChildToVerticalBox(ConfirmationCancelButton);
	}

	RefreshProgrammaticLayout();
}

void UGameXXKInventoryWindowWidget::RefreshProgrammaticLayout()
{
	BuildProgrammaticLayout();
	const bool bWindowVisible = WindowMode != EGameXXKInventoryWindowMode::None;
	SetVisibility(bWindowVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (WindowFrame)
	{
		WindowFrame->SetVisibility(bWindowVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (CloseButton)
	{
		CloseButton->SetVisibility(bWindowVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ConfirmationDialogFrame)
	{
		ConfirmationDialogFrame->SetVisibility(PendingConfirmationAction == EConfirmationAction::None ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
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
	SelectedSlotSource = ESelectedSlotSource::PlayerBackpack;
	SelectedItemId = ItemId;
	return true;
}

bool UGameXXKInventoryWindowWidget::SelectMerchantStockSlot(int32 SlotIndex)
{
	const TArray<FName> ShopItems = UGameXXKMVPRules::GetShopItemIds();
	if (!ShopItems.IsValidIndex(SlotIndex))
	{
		return false;
	}
	SelectedSlotSource = ESelectedSlotSource::MerchantStock;
	SelectedItemId = ShopItems[SlotIndex];
	return true;
}

bool UGameXXKInventoryWindowWidget::RequestBuyForSelectedItem()
{
	if (WindowMode != EGameXXKInventoryWindowMode::MerchantTrade || SelectedSlotSource != ESelectedSlotSource::MerchantStock || SelectedItemId.IsNone())
	{
		return false;
	}
	PendingConfirmationAction = EConfirmationAction::Buy;
	PendingConfirmationItemId = SelectedItemId;
	PendingConfirmationQuantity = 1;
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
	if (ConfirmationDialogFrame)
	{
		ConfirmationDialogFrame->SetVisibility(ESlateVisibility::Collapsed);
	}
	return true;
}

void UGameXXKInventoryWindowWidget::HandleCloseClicked()
{
	CloseInventoryWindow();
}

void UGameXXKInventoryWindowWidget::HandleConfirmClicked()
{
	ConfirmDialog();
}

void UGameXXKInventoryWindowWidget::HandleCancelClicked()
{
	CancelDialog();
}
