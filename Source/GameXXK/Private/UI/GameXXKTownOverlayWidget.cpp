#include "UI/GameXXKTownOverlayWidget.h"

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
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKMVPCommandRouter.h"

namespace
{
	static const FName EnterDungeonCommand(TEXT("EnterDungeon"));
	static const FName SaveSlotOneCommand(TEXT("SaveSlot1"));
	static const FName OpenInventoryCommand(TEXT("OpenInventory"));
	static const FName OpenCharacterPanelCommand(TEXT("OpenCharacterPanel"));
	static const FName OpenTradeCommand(TEXT("OpenTrade"));
	static const FName CloseTownPanelCommand(TEXT("CloseTownPanel"));
	const FVector2D TownPanelPosition(28.0f, 28.0f);
	const FVector2D TownPanelSize(360.0f, 260.0f);
	const FVector2D TownDetailPanelPosition(412.0f, 28.0f);
	const FVector2D TownDetailPanelSize(760.0f, 540.0f);
	const FString InventorySlotTexturePath(TEXT("/Game/GameXXK/UI/Inventory/Textures/T_InkBackpackSlot.T_InkBackpackSlot"));
	const FString ItemIconTextureRoot(TEXT("/Game/GameXXK/UI/Inventory/Textures/"));
	constexpr int32 InventorySlotCount = 20;
	constexpr float BackpackSlotSize = 72.0f;
	constexpr float BackpackIconSize = 46.0f;

	static FSlateBrush BuildTextureBrush(UTexture2D* Texture, const FVector2D& ImageSize, const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = ImageSize;
		Brush.DrawAs = Texture ? ESlateBrushDrawType::Image : ESlateBrushDrawType::Box;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	static bool IsCommandEnabled(const TArray<FGameXXKMVPCommandDescriptor>& Commands, FName CommandName)
	{
		for (const FGameXXKMVPCommandDescriptor& Command : Commands)
		{
			if (Command.CommandName == CommandName)
			{
				return Command.bEnabled;
			}
		}
		return false;
	}
}

TSharedRef<SWidget> UGameXXKTownOverlayWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKTownOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	RefreshFromState();
}

void UGameXXKTownOverlayWidget::RefreshFromState()
{
	BuildProgrammaticLayout();

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bShouldShow = Subsystem && Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Town;
	SetVisibility(bShouldShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	SetIsEnabled(bShouldShow);

	CachedStatusText = GameXXKMVPCommandRouter::BuildStatusText(Subsystem);
	ConfigureProgrammaticLayout();
}

bool UGameXXKTownOverlayWidget::EnterRouteMap()
{
	return ExecuteTownCommand(EnterDungeonCommand);
}

bool UGameXXKTownOverlayWidget::SaveToSlotOne()
{
	return ExecuteTownCommand(SaveSlotOneCommand);
}

bool UGameXXKTownOverlayWidget::ExecuteTownCommandForTest(FName CommandName)
{
	return ExecuteTownCommand(CommandName);
}

TArray<FGameXXKMVPCommandDescriptor> UGameXXKTownOverlayWidget::BuildTownActionsForTest() const
{
	TArray<FGameXXKMVPCommandDescriptor> Commands = GameXXKMVPCommandRouter::BuildVisibleCommands(ResolveMVPSubsystem());
	Commands.RemoveAll([](const FGameXXKMVPCommandDescriptor& Command)
	{
		return Command.CommandName == EnterDungeonCommand;
	});
	return Commands;
}

bool UGameXXKTownOverlayWidget::HasTownActionForTest(FName CommandName, bool bRequireEnabled) const
{
	const TArray<FGameXXKMVPCommandDescriptor> Commands = BuildTownActionsForTest();
	for (const FGameXXKMVPCommandDescriptor& Command : Commands)
	{
		if (Command.CommandName == CommandName && (!bRequireEnabled || Command.bEnabled))
		{
			return true;
		}
	}
	return false;
}

bool UGameXXKTownOverlayWidget::IsTownOverlayVisible() const
{
	return GetVisibility() == ESlateVisibility::Visible || GetVisibility() == ESlateVisibility::SelfHitTestInvisible;
}

FText UGameXXKTownOverlayWidget::GetStatusTextForTest() const
{
	return CachedStatusText;
}

EGameXXKTownPanelMode UGameXXKTownOverlayWidget::GetActiveTownPanelForTest() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem ? Subsystem->GetRuntimeState().TownPanelMode : EGameXXKTownPanelMode::None;
}

int32 UGameXXKTownOverlayWidget::GetInventorySlotCountForTest() const
{
	return InventorySlotCount;
}

FString UGameXXKTownOverlayWidget::GetInventorySlotResourcePathForTest() const
{
	return InventorySlotTexturePath;
}

int32 UGameXXKTownOverlayWidget::GetShopStockSlotCountForTest() const
{
	return CurrentShopStockSlotItemIds.Num();
}

FString UGameXXKTownOverlayWidget::GetShopStockSlotResourcePathForTest() const
{
	return InventorySlotTexturePath;
}

FName UGameXXKTownOverlayWidget::GetShopStockSlotItemIdForTest(int32 SlotIndex) const
{
	return CurrentShopStockSlotItemIds.IsValidIndex(SlotIndex) ? CurrentShopStockSlotItemIds[SlotIndex] : NAME_None;
}

FName UGameXXKTownOverlayWidget::GetPlayerBackpackSlotItemIdForTest(int32 SlotIndex) const
{
	return CurrentPlayerBackpackSlotItemIds.IsValidIndex(SlotIndex) ? CurrentPlayerBackpackSlotItemIds[SlotIndex] : NAME_None;
}

FString UGameXXKTownOverlayWidget::GetItemIconResourcePathForTest(FName ItemId) const
{
	return ResolveItemIconTexturePath(ItemId);
}

bool UGameXXKTownOverlayWidget::SelectShopStockSlotForTest(int32 SlotIndex)
{
	return SelectShopStockSlot(SlotIndex);
}

bool UGameXXKTownOverlayWidget::IsPurchaseConfirmationVisibleForTest() const
{
	return !PendingPurchaseItemId.IsNone()
		&& PurchaseConfirmationPanel
		&& PurchaseConfirmationPanel->GetVisibility() != ESlateVisibility::Collapsed;
}

FName UGameXXKTownOverlayWidget::GetPendingPurchaseItemIdForTest() const
{
	return PendingPurchaseItemId;
}

bool UGameXXKTownOverlayWidget::ConfirmPendingPurchaseForTest()
{
	return ConfirmPendingPurchase();
}

void UGameXXKTownOverlayWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("TownOverlayWidgetTree"));
	}
	if (!WidgetTree || RootCanvas)
	{
		return;
	}
	if (RootCanvas && WidgetTree->RootWidget == RootCanvas)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GameXXKTownOverlayRoot"));
	WidgetTree->RootWidget = RootCanvas;

	PanelBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TownOverlayPanel"));
	if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(PanelBox))
	{
		PanelSlot->SetPosition(TownPanelPosition);
		PanelSlot->SetSize(TownPanelSize);
	}

	AddTextBlock(PanelBox, NSLOCTEXT("GameXXKTownOverlay", "Title", "Qingshan Town"));
	StatusTextBlock = AddTextBlock(PanelBox, FText::GetEmpty());

	InventoryButton = AddCommandButton(PanelBox, NSLOCTEXT("GameXXKTownOverlay", "OpenInventory", "背包"));
	if (InventoryButton)
	{
		InventoryButton->OnClicked.AddDynamic(this, &UGameXXKTownOverlayWidget::HandleInventoryClicked);
	}
	CharacterButton = AddCommandButton(PanelBox, NSLOCTEXT("GameXXKTownOverlay", "OpenCharacter", "角色"));
	if (CharacterButton)
	{
		CharacterButton->OnClicked.AddDynamic(this, &UGameXXKTownOverlayWidget::HandleCharacterClicked);
	}
	TradeButton = AddCommandButton(PanelBox, NSLOCTEXT("GameXXKTownOverlay", "OpenTrade", "商店"));
	if (TradeButton)
	{
		TradeButton->OnClicked.AddDynamic(this, &UGameXXKTownOverlayWidget::HandleTradeClicked);
	}
	ClosePanelButton = AddCommandButton(PanelBox, NSLOCTEXT("GameXXKTownOverlay", "ClosePanel", "关闭面板"));
	if (ClosePanelButton)
	{
		ClosePanelButton->OnClicked.AddDynamic(this, &UGameXXKTownOverlayWidget::HandleClosePanelClicked);
	}

	SaveButton = AddCommandButton(PanelBox, NSLOCTEXT("GameXXKTownOverlay", "SaveSlotOne", "Save Slot 1"));
	if (SaveButton)
	{
		SaveButton->OnClicked.AddDynamic(this, &UGameXXKTownOverlayWidget::HandleSaveClicked);
	}

	ActivePanelBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TownOverlayActivePanel"));
	if (UCanvasPanelSlot* ActivePanelSlot = RootCanvas->AddChildToCanvas(ActivePanelBox))
	{
		ActivePanelSlot->SetPosition(TownDetailPanelPosition);
		ActivePanelSlot->SetSize(TownDetailPanelSize);
	}
	ActivePanelTitleBlock = AddTextBlock(ActivePanelBox, FText::GetEmpty());

	ActivePanelBackplate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TownInventoryBackplate"));
	ActivePanelBackplate->SetPadding(FMargin(18.0f, 16.0f));
	ActivePanelBackplate->SetBrushColor(FLinearColor(0.10f, 0.085f, 0.065f, 0.86f));
	if (UVerticalBoxSlot* BackplateSlot = ActivePanelBox->AddChildToVerticalBox(ActivePanelBackplate))
	{
		BackplateSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
		BackplateSlot->SetHorizontalAlignment(HAlign_Fill);
		BackplateSlot->SetVerticalAlignment(VAlign_Fill);
	}

	ActivePanelColumns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TownUnifiedInventoryColumns"));
	ActivePanelBackplate->AddChild(ActivePanelColumns);

	EquipmentPanelBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TownEquipmentPanel"));
	if (UHorizontalBoxSlot* EquipmentColumnSlot = ActivePanelColumns->AddChildToHorizontalBox(EquipmentPanelBox))
	{
		EquipmentColumnSlot->SetPadding(FMargin(0.0f, 0.0f, 18.0f, 0.0f));
		EquipmentColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		EquipmentColumnSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	UTextBlock* EquipmentHeader = AddTextBlock(EquipmentPanelBox, NSLOCTEXT("GameXXKTownOverlay", "EquipmentHeader", "装备"));
	if (EquipmentHeader)
	{
		FSlateFontInfo EquipmentHeaderFont = EquipmentHeader->GetFont();
		EquipmentHeaderFont.Size = 18;
		EquipmentHeaderFont.TypefaceFontName = TEXT("Bold");
		EquipmentHeader->SetFont(EquipmentHeaderFont);
	}

	auto AddEquipmentSlot = [this](const FName SlotName, const FText& FallbackText) -> UTextBlock*
	{
		USizeBox* SlotSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), SlotName);
		SlotSizeBox->SetWidthOverride(210.0f);
		SlotSizeBox->SetHeightOverride(64.0f);

		UBorder* SlotBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		SlotBorder->SetPadding(FMargin(10.0f, 8.0f));
		SlotBorder->SetBrushColor(FLinearColor(0.18f, 0.14f, 0.10f, 0.92f));
		SlotSizeBox->AddChild(SlotBorder);

		UTextBlock* SlotTextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		SlotTextBlock->SetText(FallbackText);
		SlotTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f, 0.88f, 0.74f, 1.0f)));
		SlotTextBlock->SetAutoWrapText(true);
		SlotBorder->AddChild(SlotTextBlock);

		if (UVerticalBoxSlot* EquipmentSlot = EquipmentPanelBox->AddChildToVerticalBox(SlotSizeBox))
		{
			EquipmentSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
			EquipmentSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		return SlotTextBlock;
	};
	WeaponSlotTextBlock = AddEquipmentSlot(TEXT("TownEquipmentSlot_Weapon"), NSLOCTEXT("GameXXKTownOverlay", "WeaponSlotEmpty", "武器\n未装备"));
	ArmorSlotTextBlock = AddEquipmentSlot(TEXT("TownEquipmentSlot_Armor"), NSLOCTEXT("GameXXKTownOverlay", "ArmorSlotEmpty", "防具\n未装备"));
	AccessorySlotTextBlock = AddEquipmentSlot(TEXT("TownEquipmentSlot_Accessory"), NSLOCTEXT("GameXXKTownOverlay", "AccessorySlotEmpty", "饰品\n未装备"));
	ActivePanelBodyBlock = AddTextBlock(EquipmentPanelBox, FText::GetEmpty());
	if (ActivePanelBodyBlock)
	{
		ActivePanelBodyBlock->SetAutoWrapText(true);
	}

	UVerticalBox* BackpackPanelBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TownBackpackPanel"));
	if (UHorizontalBoxSlot* BackpackColumnSlot = ActivePanelColumns->AddChildToHorizontalBox(BackpackPanelBox))
	{
		BackpackColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		BackpackColumnSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	ShopStockPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TownShopStockPanel"));
	BackpackPanelBox->AddChildToVerticalBox(ShopStockPanel);

	UTextBlock* ShopHeader = AddTextBlock(ShopStockPanel, NSLOCTEXT("GameXXKTownOverlay", "ShopStockHeader", "商人货架"));
	if (ShopHeader)
	{
		FSlateFontInfo ShopHeaderFont = ShopHeader->GetFont();
		ShopHeaderFont.Size = 17;
		ShopHeaderFont.TypefaceFontName = TEXT("Bold");
		ShopHeader->SetFont(ShopHeaderFont);
	}
	ShopStockGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("TownShopStockGrid"));
	ShopStockPanel->AddChildToVerticalBox(ShopStockGrid);

	PurchaseConfirmationPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TownPurchaseConfirmationPanel"));
	PurchaseConfirmationPanel->SetPadding(FMargin(10.0f, 8.0f));
	PurchaseConfirmationPanel->SetBrushColor(FLinearColor(0.16f, 0.12f, 0.08f, 0.92f));
	PurchaseConfirmationPanel->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBox* PurchaseConfirmationBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TownPurchaseConfirmationBox"));
	PurchaseConfirmationPanel->AddChild(PurchaseConfirmationBox);
	PurchaseConfirmationTextBlock = AddTextBlock(PurchaseConfirmationBox, FText::GetEmpty());
	ConfirmPurchaseButton = AddCommandButton(PurchaseConfirmationBox, NSLOCTEXT("GameXXKTownOverlay", "ConfirmPurchase", "购买"));
	if (ConfirmPurchaseButton)
	{
		ConfirmPurchaseButton->OnClicked.AddDynamic(this, &UGameXXKTownOverlayWidget::HandleConfirmPurchaseClicked);
	}
	CancelPurchaseButton = AddCommandButton(PurchaseConfirmationBox, NSLOCTEXT("GameXXKTownOverlay", "CancelPurchase", "取消"));
	if (CancelPurchaseButton)
	{
		CancelPurchaseButton->OnClicked.AddDynamic(this, &UGameXXKTownOverlayWidget::HandleCancelPurchaseClicked);
	}
	if (UVerticalBoxSlot* PurchaseSlot = ShopStockPanel->AddChildToVerticalBox(PurchaseConfirmationPanel))
	{
		PurchaseSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
		PurchaseSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	UTextBlock* BackpackHeader = AddTextBlock(BackpackPanelBox, NSLOCTEXT("GameXXKTownOverlay", "BackpackHeader", "背包"));
	if (BackpackHeader)
	{
		FSlateFontInfo BackpackHeaderFont = BackpackHeader->GetFont();
		BackpackHeaderFont.Size = 18;
		BackpackHeaderFont.TypefaceFontName = TEXT("Bold");
		BackpackHeader->SetFont(BackpackHeaderFont);
	}

	InventoryGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("TownSharedInventoryGrid"));
	BackpackPanelBox->AddChildToVerticalBox(InventoryGrid);
	InventorySlotLabels.Reset();
	InventorySlotIcons.Reset();
	for (int32 SlotIndex = 0; SlotIndex < InventorySlotCount; ++SlotIndex)
	{
		UTextBlock* SlotLabel = nullptr;
		UImage* SlotIcon = nullptr;
		UButton* SlotButton = nullptr;
		USizeBox* SlotSizeBox = BuildItemSlotWidget(FName(*FString::Printf(TEXT("TownInventorySlotSize_%02d"), SlotIndex)), SlotLabel, SlotIcon, SlotButton);
		if (UUniformGridSlot* GridSlot = InventoryGrid->AddChildToUniformGrid(SlotSizeBox, SlotIndex / 5, SlotIndex % 5))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}
		InventorySlotLabels.Add(SlotLabel);
		InventorySlotIcons.Add(SlotIcon);
	}
}

void UGameXXKTownOverlayWidget::ConfigureProgrammaticLayout()
{
	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(CachedStatusText);
	}

	const TArray<FGameXXKMVPCommandDescriptor> Commands = BuildTownActionsForTest();
	if (InventoryButton)
	{
		InventoryButton->SetIsEnabled(IsCommandEnabled(Commands, OpenInventoryCommand));
	}
	if (CharacterButton)
	{
		CharacterButton->SetIsEnabled(IsCommandEnabled(Commands, OpenCharacterPanelCommand));
	}
	if (TradeButton)
	{
		TradeButton->SetIsEnabled(IsCommandEnabled(Commands, OpenTradeCommand));
	}
	if (ClosePanelButton)
	{
		ClosePanelButton->SetIsEnabled(IsCommandEnabled(Commands, CloseTownPanelCommand));
	}
	if (SaveButton)
	{
		SaveButton->SetIsEnabled(IsCommandEnabled(Commands, SaveSlotOneCommand));
	}
	if (EnterRouteButton)
	{
		EnterRouteButton->SetIsEnabled(IsCommandEnabled(Commands, EnterDungeonCommand));
	}

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const EGameXXKTownPanelMode ActiveMode = State ? State->TownPanelMode : EGameXXKTownPanelMode::None;
	if (ActivePanelBox)
	{
		ActivePanelBox->SetVisibility(ActiveMode == EGameXXKTownPanelMode::None ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (ActivePanelBackplate)
	{
		ActivePanelBackplate->SetVisibility(ActiveMode == EGameXXKTownPanelMode::None ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (InventoryGrid)
	{
		InventoryGrid->SetVisibility(ActiveMode == EGameXXKTownPanelMode::None ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (ShopStockPanel)
	{
		ShopStockPanel->SetVisibility(ActiveMode == EGameXXKTownPanelMode::Trade ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ActiveMode != EGameXXKTownPanelMode::Trade)
	{
		PendingPurchaseItemId = NAME_None;
	}
	if (PurchaseConfirmationPanel)
	{
		PurchaseConfirmationPanel->SetVisibility(!PendingPurchaseItemId.IsNone() && ActiveMode == EGameXXKTownPanelMode::Trade ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (!ActivePanelTitleBlock || !ActivePanelBodyBlock || !State)
	{
		return;
	}

	auto ResolveItemDisplayName = [](FName ItemId) -> FString
	{
		if (ItemId.IsNone())
		{
			return TEXT("未装备");
		}
		bool bFound = false;
		const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(ItemId, bFound);
		return bFound ? Def.DisplayName.ToString() : ItemId.ToString();
	};

	if (WeaponSlotTextBlock)
	{
		WeaponSlotTextBlock->SetText(FText::FromString(FString::Printf(TEXT("武器\n%s"), *ResolveItemDisplayName(State->EquippedWeapon))));
	}
	if (ArmorSlotTextBlock)
	{
		ArmorSlotTextBlock->SetText(FText::FromString(FString::Printf(TEXT("防具\n%s"), *ResolveItemDisplayName(State->EquippedArmor))));
	}
	if (AccessorySlotTextBlock)
	{
		AccessorySlotTextBlock->SetText(FText::FromString(FString::Printf(TEXT("饰品\n%s"), *ResolveItemDisplayName(State->EquippedAccessory))));
	}

	const TArray<FGameXXKTownItemSlotView> PlayerBackpackSlots = BuildPlayerBackpackSlotViews(*State);
	CurrentPlayerBackpackSlotItemIds.Reset();
	for (int32 SlotIndex = 0; SlotIndex < InventorySlotLabels.Num(); ++SlotIndex)
	{
		UTextBlock* SlotLabel = InventorySlotLabels[SlotIndex].Get();
		UImage* SlotIcon = InventorySlotIcons.IsValidIndex(SlotIndex) ? InventorySlotIcons[SlotIndex].Get() : nullptr;
		const FGameXXKTownItemSlotView SlotView = PlayerBackpackSlots.IsValidIndex(SlotIndex) ? PlayerBackpackSlots[SlotIndex] : FGameXXKTownItemSlotView{};
		CurrentPlayerBackpackSlotItemIds.Add(SlotView.ItemId);
		ApplyItemSlotVisual(SlotLabel, SlotIcon, SlotView);
	}

	if (ActiveMode == EGameXXKTownPanelMode::Inventory)
	{
		ActivePanelTitleBlock->SetText(NSLOCTEXT("GameXXKTownOverlay", "InventoryTitle", "背包"));
		TArray<FString> ItemLines;
		for (const TPair<FName, int32>& Entry : State->Inventory)
		{
			if (Entry.Value > 0)
			{
				bool bFound = false;
				const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(Entry.Key, bFound);
				ItemLines.Add(FString::Printf(TEXT("%s x%d"), bFound ? *Def.DisplayName.ToString() : *Entry.Key.ToString(), Entry.Value));
			}
		}
		ItemLines.Sort();
		ActivePanelBodyBlock->SetText(FText::FromString(ItemLines.Num() > 0 ? FString::Join(ItemLines, TEXT("\n")) : TEXT("空")));
	}
	else if (ActiveMode == EGameXXKTownPanelMode::Character)
	{
		ActivePanelTitleBlock->SetText(NSLOCTEXT("GameXXKTownOverlay", "CharacterTitle", "角色"));
		ActivePanelBodyBlock->SetText(FText::FromString(FString::Printf(
			TEXT("Lv.%d\nHP %d/%d\nMP %d/%d\n攻 %d  防 %d  速 %d\n武器 %s\n防具 %s\n饰品 %s"),
			State->PlayerLevel,
			State->PlayerHP,
			State->PlayerMaxHP,
			State->PlayerMP,
			State->PlayerMaxMP,
			State->PlayerAttack,
			State->PlayerDefense,
			State->PlayerSpeed,
			*State->EquippedWeapon.ToString(),
			*State->EquippedArmor.ToString(),
			*State->EquippedAccessory.ToString())));
	}
	else if (ActiveMode == EGameXXKTownPanelMode::Trade)
	{
		ActivePanelTitleBlock->SetText(NSLOCTEXT("GameXXKTownOverlay", "TradeTitle", "商店"));
		const TArray<FGameXXKTownItemSlotView> ShopStockSlots = BuildShopStockSlotViews();
		CurrentShopStockSlotItemIds.Reset();
		if (ShopStockGrid)
		{
			ShopStockGrid->ClearChildren();
			ShopStockSlotLabels.Reset();
			ShopStockSlotIcons.Reset();
			ShopStockSlotButtons.Reset();
			for (int32 SlotIndex = 0; SlotIndex < ShopStockSlots.Num(); ++SlotIndex)
			{
				UTextBlock* SlotLabel = nullptr;
				UImage* SlotIcon = nullptr;
				UButton* SlotButton = nullptr;
				USizeBox* SlotSizeBox = BuildItemSlotWidget(FName(*FString::Printf(TEXT("TownShopStockSlotSize_%02d"), SlotIndex)), SlotLabel, SlotIcon, SlotButton);
				if (UUniformGridSlot* GridSlot = SlotSizeBox ? ShopStockGrid->AddChildToUniformGrid(SlotSizeBox, SlotIndex / 4, SlotIndex % 4) : nullptr)
				{
					GridSlot->SetHorizontalAlignment(HAlign_Center);
					GridSlot->SetVerticalAlignment(VAlign_Center);
				}
				if (SlotButton)
				{
					switch (SlotIndex)
					{
					case 0:
						SlotButton->OnClicked.AddDynamic(this, &UGameXXKTownOverlayWidget::HandleShopStockSlot0Clicked);
						break;
					case 1:
						SlotButton->OnClicked.AddDynamic(this, &UGameXXKTownOverlayWidget::HandleShopStockSlot1Clicked);
						break;
					case 2:
						SlotButton->OnClicked.AddDynamic(this, &UGameXXKTownOverlayWidget::HandleShopStockSlot2Clicked);
						break;
					case 3:
						SlotButton->OnClicked.AddDynamic(this, &UGameXXKTownOverlayWidget::HandleShopStockSlot3Clicked);
						break;
					case 4:
						SlotButton->OnClicked.AddDynamic(this, &UGameXXKTownOverlayWidget::HandleShopStockSlot4Clicked);
						break;
					case 5:
						SlotButton->OnClicked.AddDynamic(this, &UGameXXKTownOverlayWidget::HandleShopStockSlot5Clicked);
						break;
					case 6:
						SlotButton->OnClicked.AddDynamic(this, &UGameXXKTownOverlayWidget::HandleShopStockSlot6Clicked);
						break;
					case 7:
						SlotButton->OnClicked.AddDynamic(this, &UGameXXKTownOverlayWidget::HandleShopStockSlot7Clicked);
						break;
					default:
						break;
					}
					ShopStockSlotButtons.Add(SlotButton);
				}
				ApplyItemSlotVisual(SlotLabel, SlotIcon, ShopStockSlots[SlotIndex]);
				ShopStockSlotLabels.Add(SlotLabel);
				ShopStockSlotIcons.Add(SlotIcon);
				CurrentShopStockSlotItemIds.Add(ShopStockSlots[SlotIndex].ItemId);
			}
		}
		TArray<FString> ShopLines;
		for (const FGameXXKTownItemSlotView& SlotView : ShopStockSlots)
		{
			bool bFound = false;
			const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(SlotView.ItemId, bFound);
			if (bFound)
			{
				ShopLines.Add(FString::Printf(TEXT("%s  买%d/卖%d"), *Def.DisplayName.ToString(), Def.BuyPrice, Def.SellPrice));
			}
		}
		ActivePanelBodyBlock->SetText(FText::FromString(FString::Join(ShopLines, TEXT("\n"))));
		if (!PendingPurchaseItemId.IsNone() && PurchaseConfirmationTextBlock)
		{
			bool bFound = false;
			const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(PendingPurchaseItemId, bFound);
			PurchaseConfirmationTextBlock->SetText(FText::FromString(bFound
				? FString::Printf(TEXT("确认购买 %s？价格 %d 金"), *Def.DisplayName.ToString(), Def.BuyPrice)
				: FString::Printf(TEXT("确认购买 %s？"), *PendingPurchaseItemId.ToString())));
		}
	}
}

bool UGameXXKTownOverlayWidget::ExecuteTownCommand(FName CommandName)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}

	const bool bExecuted = GameXXKMVPCommandRouter::ExecuteVisibleCommand(Subsystem, CommandName);
	if (!bExecuted || !NotifyPlayerFlowStateChanged())
	{
		RefreshFromState();
	}
	if (bExecuted && CommandName == EnterDungeonCommand)
	{
		OnRouteMapEntered();
	}
	return bExecuted;
}

UTextBlock* UGameXXKTownOverlayWidget::AddTextBlock(UVerticalBox* Parent, const FText& Text) const
{
	if (!WidgetTree || !Parent)
	{
		return nullptr;
	}

	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TextBlock->SetText(Text);
	TextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Parent->AddChildToVerticalBox(TextBlock);
	return TextBlock;
}

UButton* UGameXXKTownOverlayWidget::AddCommandButton(UVerticalBox* Parent, const FText& Label) const
{
	if (!WidgetTree || !Parent)
	{
		return nullptr;
	}

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetBackgroundColor(FLinearColor(0.08f, 0.30f, 0.23f, 0.96f));
	UTextBlock* ButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	ButtonLabel->SetText(Label);
	ButtonLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Button->AddChild(ButtonLabel);
	Parent->AddChildToVerticalBox(Button);
	return Button;
}

USizeBox* UGameXXKTownOverlayWidget::BuildItemSlotWidget(const FName& SlotName, UTextBlock*& OutSlotLabel, UImage*& OutSlotIcon, UButton*& OutSlotButton) const
{
	OutSlotLabel = nullptr;
	OutSlotIcon = nullptr;
	OutSlotButton = nullptr;
	if (!WidgetTree)
	{
		return nullptr;
	}

	USizeBox* SlotSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), SlotName);
	SlotSizeBox->SetWidthOverride(BackpackSlotSize);
	SlotSizeBox->SetHeightOverride(BackpackSlotSize);

	UTexture2D* InventorySlotTexture = LoadObject<UTexture2D>(nullptr, *InventorySlotTexturePath);
	UButton* SlotButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	OutSlotButton = SlotButton;
	SlotButton->SetBackgroundColor(InventorySlotTexture ? FLinearColor::White : FLinearColor(0.20f, 0.17f, 0.12f, 0.82f));
	if (InventorySlotTexture)
	{
		const FVector2D SlotImageSize(BackpackSlotSize, BackpackSlotSize);
		FButtonStyle SlotStyle;
		SlotStyle.SetNormal(BuildTextureBrush(InventorySlotTexture, SlotImageSize, FLinearColor(1.0f, 1.0f, 1.0f, 0.94f)));
		SlotStyle.SetHovered(BuildTextureBrush(InventorySlotTexture, SlotImageSize, FLinearColor(1.0f, 0.94f, 0.78f, 1.0f)));
		SlotStyle.SetPressed(BuildTextureBrush(InventorySlotTexture, SlotImageSize, FLinearColor(0.86f, 0.78f, 0.62f, 1.0f)));
		SlotStyle.SetDisabled(BuildTextureBrush(InventorySlotTexture, SlotImageSize, FLinearColor(0.45f, 0.43f, 0.38f, 0.55f)));
		SlotStyle.SetNormalPadding(FMargin(8.0f));
		SlotStyle.SetPressedPadding(FMargin(9.0f, 10.0f, 7.0f, 6.0f));
		SlotButton->SetStyle(SlotStyle);
	}

	UOverlay* SlotOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	SlotButton->AddChild(SlotOverlay);

	OutSlotIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	OutSlotIcon->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* IconOverlaySlot = SlotOverlay->AddChildToOverlay(OutSlotIcon))
	{
		IconOverlaySlot->SetHorizontalAlignment(HAlign_Center);
		IconOverlaySlot->SetVerticalAlignment(VAlign_Center);
		IconOverlaySlot->SetPadding(FMargin(6.0f, 4.0f, 6.0f, 12.0f));
	}

	OutSlotLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	OutSlotLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.86f, 0.74f, 0.75f)));
	OutSlotLabel->SetJustification(ETextJustify::Center);
	OutSlotLabel->SetAutoWrapText(true);
	FSlateFontInfo SlotFont = OutSlotLabel->GetFont();
	SlotFont.Size = 11;
	OutSlotLabel->SetFont(SlotFont);
	if (UOverlaySlot* LabelOverlaySlot = SlotOverlay->AddChildToOverlay(OutSlotLabel))
	{
		LabelOverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		LabelOverlaySlot->SetVerticalAlignment(VAlign_Bottom);
		LabelOverlaySlot->SetPadding(FMargin(7.0f, 0.0f, 7.0f, 8.0f));
	}
	SlotSizeBox->AddChild(SlotButton);
	return SlotSizeBox;
}

void UGameXXKTownOverlayWidget::ApplyItemSlotVisual(UTextBlock* SlotLabel, UImage* SlotIcon, const FGameXXKTownItemSlotView& SlotView) const
{
	if (SlotLabel)
	{
		SlotLabel->SetText(SlotView.Label);
		SlotLabel->SetColorAndOpacity(FSlateColor(SlotView.ItemId.IsNone()
			? FLinearColor(0.92f, 0.86f, 0.74f, 0.35f)
			: FLinearColor(0.98f, 0.91f, 0.76f, 1.0f)));
	}

	if (!SlotIcon)
	{
		return;
	}

	UTexture2D* IconTexture = LoadItemIconTexture(SlotView.ItemId);
	if (!IconTexture)
	{
		SlotIcon->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SlotIcon->SetBrushFromTexture(IconTexture, true);
	FSlateBrush IconBrush = SlotIcon->GetBrush();
	IconBrush.ImageSize = FVector2D(BackpackIconSize, BackpackIconSize);
	IconBrush.TintColor = FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, SlotView.ItemId.IsNone() ? 0.0f : 1.0f));
	SlotIcon->SetBrush(IconBrush);
	SlotIcon->SetVisibility(SlotView.ItemId.IsNone() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

FString UGameXXKTownOverlayWidget::ResolveItemIconTexturePath(FName ItemId) const
{
	if (ItemId == UGameXXKMVPRules::ItemHealingPowder())
	{
		return ItemIconTextureRoot + TEXT("T_ItemHealingPowder.T_ItemHealingPowder");
	}
	if (ItemId == FName(TEXT("Item.LingzhiPowder")))
	{
		return ItemIconTextureRoot + TEXT("T_ItemLingzhiPowder.T_ItemLingzhiPowder");
	}
	if (ItemId == FName(TEXT("Item.QingxinTea")))
	{
		return ItemIconTextureRoot + TEXT("T_ItemQingxinTea.T_ItemQingxinTea");
	}
	if (ItemId == FName(TEXT("Item.CraneSachet")))
	{
		return ItemIconTextureRoot + TEXT("T_ItemCraneSachet.T_ItemCraneSachet");
	}
	if (ItemId == UGameXXKMVPRules::ItemIronSword())
	{
		return ItemIconTextureRoot + TEXT("T_ItemQingfengShortSword.T_ItemQingfengShortSword");
	}
	if (ItemId == UGameXXKMVPRules::ItemClothArmor())
	{
		return ItemIconTextureRoot + TEXT("T_ItemBambooLightArmor.T_ItemBambooLightArmor");
	}
	if (ItemId == FName(TEXT("Item.CranePatternTalisman")))
	{
		return ItemIconTextureRoot + TEXT("T_ItemCranePatternTalisman.T_ItemCranePatternTalisman");
	}
	if (ItemId == FName(TEXT("Item.InkstonePendant")))
	{
		return ItemIconTextureRoot + TEXT("T_ItemInkstonePendant.T_ItemInkstonePendant");
	}
	if (ItemId == UGameXXKMVPRules::ItemWoodenSword())
	{
		return ItemIconTextureRoot + TEXT("T_ItemWoodenSword.T_ItemWoodenSword");
	}
	if (ItemId == UGameXXKMVPRules::ItemStarterClothArmor())
	{
		return ItemIconTextureRoot + TEXT("T_ItemStarterClothArmor.T_ItemStarterClothArmor");
	}
	if (ItemId == UGameXXKMVPRules::ItemClothTalisman())
	{
		return ItemIconTextureRoot + TEXT("T_ItemClothTalisman.T_ItemClothTalisman");
	}
	return FString();
}

UTexture2D* UGameXXKTownOverlayWidget::LoadItemIconTexture(FName ItemId) const
{
	const FString IconTexturePath = ResolveItemIconTexturePath(ItemId);
	return IconTexturePath.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *IconTexturePath);
}

TArray<FGameXXKTownItemSlotView> UGameXXKTownOverlayWidget::BuildPlayerBackpackSlotViews(const FGameXXKRuntimeState& State) const
{
	TArray<TPair<FName, int32>> InventoryEntries;
	for (const TPair<FName, int32>& Entry : State.Inventory)
	{
		if (Entry.Value > 0)
		{
			InventoryEntries.Add(Entry);
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

	TArray<FGameXXKTownItemSlotView> Slots;
	for (const TPair<FName, int32>& Entry : InventoryEntries)
	{
		bool bFound = false;
		const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(Entry.Key, bFound);
		FGameXXKTownItemSlotView SlotView;
		SlotView.Source = EGameXXKTownItemSlotSource::PlayerInventory;
		SlotView.ItemId = Entry.Key;
		SlotView.Quantity = Entry.Value;
		SlotView.Label = FText::FromString(FString::Printf(TEXT("x%d"), Entry.Value));
		Slots.Add(SlotView);
	}
	return Slots;
}

TArray<FGameXXKTownItemSlotView> UGameXXKTownOverlayWidget::BuildShopStockSlotViews() const
{
	TArray<FGameXXKTownItemSlotView> Slots;
	for (FName ItemId : UGameXXKMVPRules::GetShopItemIds())
	{
		bool bFound = false;
		const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(ItemId, bFound);
		if (!bFound)
		{
			continue;
		}
		FGameXXKTownItemSlotView SlotView;
		SlotView.Source = EGameXXKTownItemSlotSource::ShopStock;
		SlotView.ItemId = ItemId;
		SlotView.Quantity = 1;
		SlotView.Label = FText::FromString(FString::Printf(TEXT("%d金"), Def.BuyPrice));
		Slots.Add(SlotView);
	}
	return Slots;
}

bool UGameXXKTownOverlayWidget::SelectShopStockSlot(int32 SlotIndex)
{
	if (!CurrentShopStockSlotItemIds.IsValidIndex(SlotIndex))
	{
		return false;
	}

	const FName ItemId = CurrentShopStockSlotItemIds[SlotIndex];
	if (ItemId.IsNone())
	{
		return false;
	}

	PendingPurchaseItemId = ItemId;
	ConfigureProgrammaticLayout();
	return true;
}

bool UGameXXKTownOverlayWidget::ConfirmPendingPurchase()
{
	if (PendingPurchaseItemId.IsNone())
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}

	const FName ItemId = PendingPurchaseItemId;
	if (!Subsystem->BuyItem(ItemId, 1))
	{
		ConfigureProgrammaticLayout();
		return false;
	}

	PendingPurchaseItemId = NAME_None;
	if (!NotifyPlayerFlowStateChanged())
	{
		RefreshFromState();
	}
	return true;
}

void UGameXXKTownOverlayWidget::HandleSaveClicked()
{
	SaveToSlotOne();
}

void UGameXXKTownOverlayWidget::HandleEnterRouteClicked()
{
	EnterRouteMap();
}

void UGameXXKTownOverlayWidget::HandleInventoryClicked()
{
	ExecuteTownCommand(OpenInventoryCommand);
}

void UGameXXKTownOverlayWidget::HandleCharacterClicked()
{
	ExecuteTownCommand(OpenCharacterPanelCommand);
}

void UGameXXKTownOverlayWidget::HandleTradeClicked()
{
	ExecuteTownCommand(OpenTradeCommand);
}

void UGameXXKTownOverlayWidget::HandleClosePanelClicked()
{
	ExecuteTownCommand(CloseTownPanelCommand);
}

void UGameXXKTownOverlayWidget::HandleConfirmPurchaseClicked()
{
	ConfirmPendingPurchase();
}

void UGameXXKTownOverlayWidget::HandleCancelPurchaseClicked()
{
	PendingPurchaseItemId = NAME_None;
	ConfigureProgrammaticLayout();
}

void UGameXXKTownOverlayWidget::HandleShopStockSlot0Clicked()
{
	SelectShopStockSlot(0);
}

void UGameXXKTownOverlayWidget::HandleShopStockSlot1Clicked()
{
	SelectShopStockSlot(1);
}

void UGameXXKTownOverlayWidget::HandleShopStockSlot2Clicked()
{
	SelectShopStockSlot(2);
}

void UGameXXKTownOverlayWidget::HandleShopStockSlot3Clicked()
{
	SelectShopStockSlot(3);
}

void UGameXXKTownOverlayWidget::HandleShopStockSlot4Clicked()
{
	SelectShopStockSlot(4);
}

void UGameXXKTownOverlayWidget::HandleShopStockSlot5Clicked()
{
	SelectShopStockSlot(5);
}

void UGameXXKTownOverlayWidget::HandleShopStockSlot6Clicked()
{
	SelectShopStockSlot(6);
}

void UGameXXKTownOverlayWidget::HandleShopStockSlot7Clicked()
{
	SelectShopStockSlot(7);
}
