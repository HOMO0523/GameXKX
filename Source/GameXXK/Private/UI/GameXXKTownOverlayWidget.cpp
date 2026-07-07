#include "UI/GameXXKTownOverlayWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
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
	const FVector2D TownDetailPanelSize(440.0f, 420.0f);
	const FString InventorySlotTexturePath(TEXT("/Game/GameXXK/UI/Inventory/Textures/T_InkBackpackSlot.T_InkBackpackSlot"));
	constexpr int32 InventorySlotCount = 20;

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
	ActivePanelBodyBlock = AddTextBlock(ActivePanelBox, FText::GetEmpty());
	InventoryGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("TownInventoryGrid"));
	ActivePanelBox->AddChildToVerticalBox(InventoryGrid);
	UTexture2D* InventorySlotTexture = LoadObject<UTexture2D>(nullptr, *InventorySlotTexturePath);
	for (int32 SlotIndex = 0; SlotIndex < InventorySlotCount; ++SlotIndex)
	{
		UButton* SlotButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		SlotButton->SetBackgroundColor(InventorySlotTexture ? FLinearColor::White : FLinearColor(0.20f, 0.17f, 0.12f, 0.82f));
		if (InventorySlotTexture)
		{
			const FVector2D SlotImageSize(64.0f, 64.0f);
			FButtonStyle SlotStyle;
			SlotStyle.SetNormal(BuildTextureBrush(InventorySlotTexture, SlotImageSize, FLinearColor(1.0f, 1.0f, 1.0f, 0.94f)));
			SlotStyle.SetHovered(BuildTextureBrush(InventorySlotTexture, SlotImageSize, FLinearColor(1.0f, 0.94f, 0.78f, 1.0f)));
			SlotStyle.SetPressed(BuildTextureBrush(InventorySlotTexture, SlotImageSize, FLinearColor(0.86f, 0.78f, 0.62f, 1.0f)));
			SlotStyle.SetDisabled(BuildTextureBrush(InventorySlotTexture, SlotImageSize, FLinearColor(0.45f, 0.43f, 0.38f, 0.55f)));
			SlotStyle.SetNormalPadding(FMargin(8.0f));
			SlotStyle.SetPressedPadding(FMargin(9.0f, 10.0f, 7.0f, 6.0f));
			SlotButton->SetStyle(SlotStyle);
		}
		UTextBlock* SlotLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		SlotLabel->SetText(FText::AsNumber(SlotIndex + 1));
		SlotLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.86f, 0.74f, 0.75f)));
		SlotButton->AddChild(SlotLabel);
		InventoryGrid->AddChildToUniformGrid(SlotButton, SlotIndex / 5, SlotIndex % 5);
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
	if (InventoryGrid)
	{
		InventoryGrid->SetVisibility(ActiveMode == EGameXXKTownPanelMode::Inventory ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!ActivePanelTitleBlock || !ActivePanelBodyBlock || !State)
	{
		return;
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
		TArray<FString> ShopLines;
		for (FName ItemId : UGameXXKMVPRules::GetShopItemIds())
		{
			bool bFound = false;
			const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(ItemId, bFound);
			if (bFound)
			{
				ShopLines.Add(FString::Printf(TEXT("%s  买%d/卖%d"), *Def.DisplayName.ToString(), Def.BuyPrice, Def.SellPrice));
			}
		}
		ActivePanelBodyBlock->SetText(FText::FromString(FString::Join(ShopLines, TEXT("\n"))));
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
