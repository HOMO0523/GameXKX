#include "UI/GameXXKMainMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "GameXXKMVPRules.h"
#include "Kismet/GameplayStatics.h"
#include "MVP/GameXXKSaveGame.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/GameXXKMVPCommandRouter.h"

namespace
{
	static const FName MainMenuQingshanTownMap(TEXT("/Game/GameXXK/Maps/L_QingshanInn"));
	static const FName ResolveEventGoldCommand(TEXT("ResolveEventGold"));
	static const FName ResolveCampHealCommand(TEXT("ResolveCampHeal"));
	static const FName BuyHealingPowderCommand(TEXT("BuyHealingPowder"));
	static const FName SellHealingPowderCommand(TEXT("SellHealingPowder"));
	static const FName UseHealingPowderCommand(TEXT("UseHealingPowder"));
	static const FName CompleteMerchantNodeCommand(TEXT("CompleteMerchantNode"));
	static constexpr const TCHAR* MainMenuCoverTexturePath = TEXT("/Game/GameXXK/UI/MainMenu/Textures/T_MainMenuCover.T_MainMenuCover");
	static constexpr const TCHAR* InkButtonTexturePath = TEXT("/Game/GameXXK/UI/MainMenu/Textures/T_InkButtonBase.T_InkButtonBase");

	static FSlateBrush BuildTextureBrush(UTexture2D* Texture, const FVector2D& ImageSize, const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = ImageSize;
		Brush.DrawAs = Texture ? ESlateBrushDrawType::Image : ESlateBrushDrawType::Box;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	static FText BuildSlotLabel(int32 SlotIndex, const FText& ScreenLabel, int32 PlayerLevel)
	{
		return FText::Format(
			NSLOCTEXT("GameXXKMainMenu", "OccupiedSlotLabel", "Slot {0}: {1} - Level {2}"),
			FText::AsNumber(SlotIndex + 1),
			ScreenLabel,
			FText::AsNumber(PlayerLevel));
	}
}

TSharedRef<SWidget> UGameXXKMainMenuWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildProgrammaticLayout();
	RefreshFromState();
}

bool UGameXXKMainMenuWidget::StartGame()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bStarted = Subsystem
		&& Subsystem->StartGame()
		&& Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan());
	if (bStarted)
	{
		RequestPlayableMapForRuntimeState();
		SetMenuLayer(EGameXXKMainMenuLayer::Landing);
		OnStartGameSucceeded();
		RefreshFromState();
	}
	return bStarted;
}

bool UGameXXKMainMenuWidget::StartGameFromSlot(FString SlotName, int32 UserIndex)
{
	return ContinueGameFromSlot(SlotName, UserIndex);
}

bool UGameXXKMainMenuWidget::ContinueGameFromSlot(FString SlotName, int32 UserIndex)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bStarted = Subsystem && Subsystem->ContinueGameFromSlot(SlotName, UserIndex);
	if (bStarted)
	{
		RequestPlayableMapForRuntimeState();
		SetMenuLayer(EGameXXKMainMenuLayer::Landing);
		OnStartGameSucceeded();
		RefreshFromState();
	}
	return bStarted;
}

bool UGameXXKMainMenuWidget::DoesSaveGameExist(FString SlotName, int32 UserIndex) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem && Subsystem->DoesSaveGameExist(SlotName, UserIndex);
}

bool UGameXXKMainMenuWidget::DeleteSaveGame(FString SlotName, int32 UserIndex)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bDeleted = Subsystem && Subsystem->DeleteSaveGame(SlotName, UserIndex);
	if (bDeleted)
	{
		RefreshFromState();
	}
	return bDeleted;
}

bool UGameXXKMainMenuWidget::ContinueFromSlotIndex(int32 SlotIndex)
{
	if (!IsValidManualSlotIndex(SlotIndex))
	{
		return false;
	}

	const FString SlotName = GetManualSlotNameChecked(SlotIndex);
	if (!DoesSaveGameExist(SlotName, SaveSlotUserIndex))
	{
		return false;
	}

	return ContinueGameFromSlot(SlotName, SaveSlotUserIndex);
}

bool UGameXXKMainMenuWidget::OpenContinueModal()
{
	SetMenuLayer(EGameXXKMainMenuLayer::ContinueModal);
	RefreshFromState();
	return true;
}

bool UGameXXKMainMenuWidget::RequestDeleteSlot(int32 SlotIndex)
{
	if (!IsValidManualSlotIndex(SlotIndex))
	{
		return false;
	}

	const FString SlotName = GetManualSlotNameChecked(SlotIndex);
	if (!DoesSaveGameExist(SlotName, SaveSlotUserIndex))
	{
		return false;
	}

	PendingDeleteSlotIndex = SlotIndex;
	SetMenuLayer(EGameXXKMainMenuLayer::DeleteConfirmModal);
	RefreshFromState();
	return true;
}

bool UGameXXKMainMenuWidget::ConfirmDeleteSlot()
{
	if (!IsValidManualSlotIndex(PendingDeleteSlotIndex))
	{
		return false;
	}

	const FString SlotName = GetManualSlotNameChecked(PendingDeleteSlotIndex);
	const bool bDeleted = DeleteSaveGame(SlotName, SaveSlotUserIndex);
	if (bDeleted)
	{
		PendingDeleteSlotIndex = INDEX_NONE;
		SetMenuLayer(EGameXXKMainMenuLayer::ContinueModal);
		RefreshFromState();
	}
	return bDeleted;
}

bool UGameXXKMainMenuWidget::CancelDeleteSlot()
{
	PendingDeleteSlotIndex = INDEX_NONE;
	SetMenuLayer(EGameXXKMainMenuLayer::ContinueModal);
	RefreshFromState();
	return true;
}

bool UGameXXKMainMenuWidget::OpenOptionsModal()
{
	SetMenuLayer(EGameXXKMainMenuLayer::OptionsModal);
	RefreshFromState();
	return true;
}

bool UGameXXKMainMenuWidget::OpenQuitUnavailableModal()
{
	SetMenuLayer(EGameXXKMainMenuLayer::QuitUnavailableModal);
	RefreshFromState();
	return true;
}

bool UGameXXKMainMenuWidget::CloseActiveModal()
{
	if (MenuLayer == EGameXXKMainMenuLayer::DeleteConfirmModal)
	{
		SetMenuLayer(EGameXXKMainMenuLayer::ContinueModal);
	}
	else
	{
		PendingDeleteSlotIndex = INDEX_NONE;
		SetMenuLayer(EGameXXKMainMenuLayer::Landing);
	}

	RefreshFromState();
	return true;
}

void UGameXXKMainMenuWidget::RefreshFromState()
{
	RefreshProgrammaticLayout();
}

TArray<FGameXXKMVPCommandDescriptor> UGameXXKMainMenuWidget::BuildLandingActionsForTest() const
{
	TArray<FGameXXKMVPCommandDescriptor> Actions;
	Actions.Reserve(4);
	Actions.Emplace(FName(TEXT("NewGame")), NSLOCTEXT("GameXXKMainMenu", "NewGameAction", "开始游戏"), true);
	Actions.Emplace(FName(TEXT("OpenContinue")), NSLOCTEXT("GameXXKMainMenu", "ContinueAction", "加载存档"), true);
	Actions.Emplace(FName(TEXT("OpenOptions")), NSLOCTEXT("GameXXKMainMenu", "OptionsAction", "设置游戏"), true);
	Actions.Emplace(FName(TEXT("OpenQuit")), NSLOCTEXT("GameXXKMainMenu", "QuitAction", "退出"), true);
	return Actions;
}

bool UGameXXKMainMenuWidget::HasLandingActionForTest(FName CommandName, bool bRequireEnabled) const
{
	const TArray<FGameXXKMVPCommandDescriptor> Actions = BuildLandingActionsForTest();
	for (const FGameXXKMVPCommandDescriptor& Action : Actions)
	{
		if (Action.CommandName == CommandName && (!bRequireEnabled || Action.bEnabled))
		{
			return true;
		}
	}

	return false;
}

TArray<FGameXXKMVPCommandDescriptor> UGameXXKMainMenuWidget::BuildEncounterActionsForTest() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return {};
	}

	const EGameXXKScreen Screen = Subsystem->GetRuntimeState().Screen;
	if (Screen != EGameXXKScreen::RouteEvent && Screen != EGameXXKScreen::RouteCamp && Screen != EGameXXKScreen::RouteMerchant)
	{
		return {};
	}

	return GameXXKMVPCommandRouter::BuildVisibleCommands(Subsystem);
}

bool UGameXXKMainMenuWidget::HasEncounterActionForTest(FName CommandName, bool bRequireEnabled) const
{
	const TArray<FGameXXKMVPCommandDescriptor> Actions = BuildEncounterActionsForTest();
	for (const FGameXXKMVPCommandDescriptor& Action : Actions)
	{
		if (Action.CommandName == CommandName && (!bRequireEnabled || Action.bEnabled))
		{
			return true;
		}
	}
	return false;
}

bool UGameXXKMainMenuWidget::ExecuteEncounterActionForTest(FName CommandName)
{
	return ExecuteEncounterCommand(CommandName);
}

TArray<FGameXXKMainMenuSaveSlotRow> UGameXXKMainMenuWidget::BuildSaveSlotRowsForTest() const
{
	TArray<FGameXXKMainMenuSaveSlotRow> Rows;
	const int32 SlotCount = UGameXXKMVPSubsystem::GetManualSaveSlotCount();
	Rows.Reserve(SlotCount);
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		Rows.Add(BuildSaveSlotRow(SlotIndex));
	}
	return Rows;
}

EGameXXKMainMenuLayer UGameXXKMainMenuWidget::GetMenuLayerForTest() const
{
	return MenuLayer;
}

int32 UGameXXKMainMenuWidget::GetPendingDeleteSlotIndexForTest() const
{
	return PendingDeleteSlotIndex;
}

void UGameXXKMainMenuWidget::SetSaveSlotUserIndexForTest(int32 UserIndex)
{
	SaveSlotUserIndex = UserIndex;
	RefreshFromState();
}

FName UGameXXKMainMenuWidget::GetLastRequestedTownMapForTest() const
{
	return LastRequestedTownMap;
}

FGameXXKMainMenuSaveSlotRow UGameXXKMainMenuWidget::BuildSaveSlotRow(int32 SlotIndex) const
{
	FGameXXKMainMenuSaveSlotRow Row;
	Row.SlotIndex = SlotIndex;
	Row.SlotName = IsValidManualSlotIndex(SlotIndex) ? GetManualSlotNameChecked(SlotIndex) : FString();
	Row.bOccupied = !Row.SlotName.IsEmpty() && UGameplayStatics::DoesSaveGameExist(Row.SlotName, SaveSlotUserIndex);
	Row.bCanLoad = Row.bOccupied;
	Row.bCanDelete = Row.bOccupied;

	if (!Row.bOccupied)
	{
		Row.Label = FText::Format(
			NSLOCTEXT("GameXXKMainMenu", "EmptySlotLabel", "Slot {0}: Empty"),
			FText::AsNumber(SlotIndex + 1));
		return Row;
	}

	const UGameXXKSaveGame* SaveGame = Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromSlot(Row.SlotName, SaveSlotUserIndex));
	if (!SaveGame)
	{
		Row.Label = FText::Format(
			NSLOCTEXT("GameXXKMainMenu", "UnreadableSlotLabel", "Slot {0}: Empty"),
			FText::AsNumber(SlotIndex + 1));
		Row.bOccupied = false;
		Row.bCanLoad = false;
		Row.bCanDelete = false;
		return Row;
	}

	const FGameXXKRuntimeState RestoredState = UGameXXKMVPRules::RestoreFromSaveState(SaveGame->SaveState);
	Row.Label = BuildSlotLabel(SlotIndex, BuildScreenLabel(RestoredState), RestoredState.PlayerLevel);
	return Row;
}

FText UGameXXKMainMenuWidget::BuildScreenLabel(const FGameXXKRuntimeState& State) const
{
	if (State.Screen == EGameXXKScreen::Town && State.CurrentRegion == UGameXXKMVPRules::RegionQingshan())
	{
		return NSLOCTEXT("GameXXKMainMenu", "QingshanTownScreen", "Qingshan Town");
	}

	if (State.Screen == EGameXXKScreen::DungeonMap)
	{
		return NSLOCTEXT("GameXXKMainMenu", "RouteMapScreen", "Route Map");
	}

	if (State.Screen == EGameXXKScreen::WorldMap)
	{
		return NSLOCTEXT("GameXXKMainMenu", "WorldMapScreen", "World Map");
	}

	if (State.Screen == EGameXXKScreen::Battle)
	{
		return NSLOCTEXT("GameXXKMainMenu", "BattleScreen", "Battle");
	}

	if (State.Screen == EGameXXKScreen::RouteEvent)
	{
		return NSLOCTEXT("GameXXKMainMenu", "RouteEventScreen", "Event");
	}

	if (State.Screen == EGameXXKScreen::RouteCamp)
	{
		return NSLOCTEXT("GameXXKMainMenu", "RouteCampScreen", "Camp");
	}

	if (State.Screen == EGameXXKScreen::RouteMerchant)
	{
		return NSLOCTEXT("GameXXKMainMenu", "RouteMerchantScreen", "Merchant");
	}

	if (State.Screen == EGameXXKScreen::Town)
	{
		return NSLOCTEXT("GameXXKMainMenu", "TownScreen", "Town");
	}

	return NSLOCTEXT("GameXXKMainMenu", "UnknownScreen", "Unknown");
}

void UGameXXKMainMenuWidget::RequestPlayableMapForRuntimeState()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return;
	}

	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	if (State.Screen == EGameXXKScreen::Town && State.CurrentRegion == UGameXXKMVPRules::RegionQingshan())
	{
		RequestOpenTownMap(MainMenuQingshanTownMap);
	}
}

void UGameXXKMainMenuWidget::RequestOpenTownMap(FName MapName)
{
	LastRequestedTownMap = MapName;

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	UGameplayStatics::OpenLevel(World, MapName);
}

void UGameXXKMainMenuWidget::SetMenuLayer(EGameXXKMainMenuLayer NewLayer)
{
	MenuLayer = NewLayer;
	if (NewLayer != EGameXXKMainMenuLayer::DeleteConfirmModal)
	{
		PendingDeleteSlotIndex = INDEX_NONE;
	}
}

bool UGameXXKMainMenuWidget::IsValidManualSlotIndex(int32 SlotIndex) const
{
	return SlotIndex >= 0 && SlotIndex < UGameXXKMVPSubsystem::GetManualSaveSlotCount();
}

FString UGameXXKMainMenuWidget::GetManualSlotNameChecked(int32 SlotIndex) const
{
	check(IsValidManualSlotIndex(SlotIndex));
	return UGameXXKMVPSubsystem::GetManualSaveSlotName(SlotIndex);
}

bool UGameXXKMainMenuWidget::IsEncounterScreen() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}

	const EGameXXKScreen Screen = Subsystem->GetRuntimeState().Screen;
	return Screen == EGameXXKScreen::RouteEvent
		|| Screen == EGameXXKScreen::RouteCamp
		|| Screen == EGameXXKScreen::RouteMerchant;
}

bool UGameXXKMainMenuWidget::ExecuteEncounterCommand(FName CommandName)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !IsEncounterScreen())
	{
		return false;
	}

	const bool bExecuted = GameXXKMVPCommandRouter::ExecuteVisibleCommand(Subsystem, CommandName);
	if (bExecuted)
	{
		if (!NotifyPlayerFlowStateChanged())
		{
			RefreshFromState();
		}
	}
	return bExecuted;
}

void UGameXXKMainMenuWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("MainMenuWidgetTree"));
	}
	if (!WidgetTree)
	{
		return;
	}
	if (RootOverlay && WidgetTree->RootWidget == RootOverlay)
	{
		return;
	}

	RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MainMenuRoot"));
	WidgetTree->RootWidget = RootOverlay;

	LoadedMainMenuCoverTexture = LoadObject<UTexture2D>(nullptr, MainMenuCoverTexturePath);
	LoadedInkButtonTexture = LoadObject<UTexture2D>(nullptr, InkButtonTexturePath);

	CoverImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("GameXXKMainMenuCover"));
	if (CoverImage)
	{
		if (LoadedMainMenuCoverTexture)
		{
			CoverImage->SetBrushFromTexture(LoadedMainMenuCoverTexture, true);
		}
		CoverImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* CoverSlot = RootOverlay->AddChildToOverlay(CoverImage))
		{
			CoverSlot->SetHorizontalAlignment(HAlign_Fill);
			CoverSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	LandingBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LandingBox"));
	if (UOverlaySlot* LandingSlot = RootOverlay->AddChildToOverlay(LandingBox))
	{
		LandingSlot->SetHorizontalAlignment(HAlign_Left);
		LandingSlot->SetVerticalAlignment(VAlign_Center);
		LandingSlot->SetPadding(FMargin(128.0f, 0.0f, 0.0f, 0.0f));
	}

	EncounterBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EncounterBox"));
	RootOverlay->AddChildToOverlay(EncounterBox);

	UButton* NewGameButton = AddMenuButton(
		LandingBox,
		NSLOCTEXT("GameXXKMainMenu", "NewGameButton", "开始游戏"),
		FName(TEXT("MainMenuStartButton")),
		FName(TEXT("MainMenuStartButtonLabel")));
	NewGameButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleStartGameClicked);

	UButton* ContinueButton = AddMenuButton(
		LandingBox,
		NSLOCTEXT("GameXXKMainMenu", "ContinueButton", "加载存档"),
		FName(TEXT("MainMenuContinueButton")),
		FName(TEXT("MainMenuContinueButtonLabel")));
	ContinueButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleOpenContinueClicked);

	UButton* OptionsButton = AddMenuButton(
		LandingBox,
		NSLOCTEXT("GameXXKMainMenu", "OptionsButton", "设置游戏"),
		FName(TEXT("MainMenuOptionsButton")),
		FName(TEXT("MainMenuOptionsButtonLabel")));
	OptionsButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleOpenOptionsClicked);

	UButton* QuitButton = AddMenuButton(
		LandingBox,
		NSLOCTEXT("GameXXKMainMenu", "QuitButton", "退出"),
		FName(TEXT("MainMenuQuitButton")),
		FName(TEXT("MainMenuQuitButtonLabel")));
	QuitButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleOpenQuitClicked);

	ModalBackdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ModalBackdrop"));
	RootOverlay->AddChildToOverlay(ModalBackdrop);

	ModalBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ModalBox"));
	ModalBackdrop->SetContent(ModalBox);
}

void UGameXXKMainMenuWidget::RefreshProgrammaticLayout()
{
	bool bShouldShowMenu = true;
	bool bShouldShowEncounter = false;
	if (const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem())
	{
		const EGameXXKScreen Screen = Subsystem->GetRuntimeState().Screen;
		bShouldShowMenu = Screen == EGameXXKScreen::MainMenu;
	}
	SetVisibility((bShouldShowMenu || bShouldShowEncounter) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	SetIsEnabled(bShouldShowMenu || bShouldShowEncounter);

	if (!LandingBox || !EncounterBox || !ModalBackdrop || !ModalBox)
	{
		return;
	}

	LandingBox->SetVisibility(bShouldShowMenu ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	EncounterBox->SetVisibility(bShouldShowEncounter ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	EncounterBox->ClearChildren();

	if (bShouldShowEncounter)
	{
		const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
		if (Subsystem)
		{
			AddTextBlock(EncounterBox, BuildScreenLabel(Subsystem->GetRuntimeState()));
			for (const FGameXXKMVPCommandDescriptor& Command : BuildEncounterActionsForTest())
			{
				UButton* CommandButton = AddMenuButton(EncounterBox, Command.Label);
				CommandButton->SetIsEnabled(Command.bEnabled);
				if (Command.CommandName == ResolveEventGoldCommand)
				{
					CommandButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleResolveEventGoldClicked);
				}
				else if (Command.CommandName == ResolveCampHealCommand)
				{
					CommandButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleResolveCampHealClicked);
				}
				else if (Command.CommandName == BuyHealingPowderCommand)
				{
					CommandButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleBuyHealingPowderClicked);
				}
				else if (Command.CommandName == SellHealingPowderCommand)
				{
					CommandButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleSellHealingPowderClicked);
				}
				else if (Command.CommandName == UseHealingPowderCommand)
				{
					CommandButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleUseHealingPowderClicked);
				}
				else if (Command.CommandName == CompleteMerchantNodeCommand)
				{
					CommandButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleCompleteMerchantNodeClicked);
				}
			}
		}
		ModalBackdrop->SetVisibility(ESlateVisibility::Collapsed);
		ModalBox->ClearChildren();
		return;
	}

	const bool bShowingModal = MenuLayer != EGameXXKMainMenuLayer::Landing;
	ModalBackdrop->SetVisibility(bShowingModal ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	ModalBox->ClearChildren();

	if (!bShowingModal)
	{
		return;
	}

	if (MenuLayer == EGameXXKMainMenuLayer::ContinueModal)
	{
		AddTextBlock(ModalBox, NSLOCTEXT("GameXXKMainMenu", "ContinueTitle", "Continue"));
		for (const FGameXXKMainMenuSaveSlotRow& Row : BuildSaveSlotRowsForTest())
		{
			AddSaveSlotRow(ModalBox, Row);
		}
		AddCloseButton(ModalBox);
		return;
	}

	if (MenuLayer == EGameXXKMainMenuLayer::DeleteConfirmModal)
	{
		const FGameXXKMainMenuSaveSlotRow Row = BuildSaveSlotRow(PendingDeleteSlotIndex);
		AddTextBlock(ModalBox, FText::Format(
			NSLOCTEXT("GameXXKMainMenu", "DeleteConfirmTitle", "Delete {0}?"),
			Row.Label));

		UButton* DeleteButton = AddMenuButton(ModalBox, NSLOCTEXT("GameXXKMainMenu", "DeleteButton", "Delete"));
		DeleteButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleConfirmDeleteClicked);

		UButton* CancelButton = AddMenuButton(ModalBox, NSLOCTEXT("GameXXKMainMenu", "CancelButton", "Cancel"));
		CancelButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleCancelDeleteClicked);
		return;
	}

	if (MenuLayer == EGameXXKMainMenuLayer::OptionsModal)
	{
		AddTextBlock(ModalBox, NSLOCTEXT("GameXXKMainMenu", "OptionsUnavailable", "Options are unavailable in this build."));
		AddCloseButton(ModalBox);
		return;
	}

	AddTextBlock(ModalBox, NSLOCTEXT("GameXXKMainMenu", "QuitUnavailable", "Quit is unavailable in this build."));
	AddCloseButton(ModalBox);
}

UTextBlock* UGameXXKMainMenuWidget::AddTextBlock(UVerticalBox* Parent, const FText& Text) const
{
	UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TextBlock->SetText(Text);
	TextBlock->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Parent->AddChildToVerticalBox(TextBlock);
	return TextBlock;
}

UButton* UGameXXKMainMenuWidget::AddMenuButton(UVerticalBox* Parent, const FText& Label, FName ButtonName, FName LabelName) const
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonName);
	Button->SetBackgroundColor(FLinearColor::White);
	if (LoadedInkButtonTexture)
	{
		const FVector2D ButtonImageSize(320.0f, 86.0f);
		FButtonStyle ButtonStyle;
		ButtonStyle.SetNormal(BuildTextureBrush(LoadedInkButtonTexture, ButtonImageSize, FLinearColor(1.0f, 1.0f, 1.0f, 0.92f)));
		ButtonStyle.SetHovered(BuildTextureBrush(LoadedInkButtonTexture, ButtonImageSize, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));
		ButtonStyle.SetPressed(BuildTextureBrush(LoadedInkButtonTexture, ButtonImageSize, FLinearColor(0.82f, 0.90f, 0.86f, 0.96f)));
		ButtonStyle.SetDisabled(BuildTextureBrush(LoadedInkButtonTexture, ButtonImageSize, FLinearColor(0.48f, 0.52f, 0.50f, 0.55f)));
		ButtonStyle.SetNormalPadding(FMargin(48.0f, 14.0f, 48.0f, 14.0f));
		ButtonStyle.SetPressedPadding(FMargin(48.0f, 16.0f, 48.0f, 12.0f));
		Button->SetStyle(ButtonStyle);
	}
	else
	{
		Button->SetBackgroundColor(FLinearColor(0.04f, 0.12f, 0.10f, 0.88f));
	}

	UTextBlock* ButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), LabelName);
	ButtonLabel->SetText(Label);
	ButtonLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ButtonLabel->SetJustification(ETextJustify::Center);
	ButtonLabel->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f));
	ButtonLabel->SetShadowOffset(FVector2D(1.0f, 1.0f));
	FSlateFontInfo LabelFont = ButtonLabel->GetFont();
	LabelFont.Size = 28;
	LabelFont.TypefaceFontName = TEXT("Bold");
	ButtonLabel->SetFont(LabelFont);
	Button->AddChild(ButtonLabel);

	USizeBox* ButtonFrame = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ButtonFrame->SetWidthOverride(320.0f);
	ButtonFrame->SetHeightOverride(76.0f);
	ButtonFrame->AddChild(Button);
	if (UVerticalBoxSlot* ButtonSlot = Parent->AddChildToVerticalBox(ButtonFrame))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Left);
		ButtonSlot->SetVerticalAlignment(VAlign_Center);
		ButtonSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 8.0f));
	}
	return Button;
}

void UGameXXKMainMenuWidget::AddSaveSlotRow(UVerticalBox* Parent, const FGameXXKMainMenuSaveSlotRow& Row)
{
	UHorizontalBox* RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Parent->AddChildToVerticalBox(RowBox);

	UTextBlock* RowLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	RowLabel->SetText(Row.Label);
	RowLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	RowBox->AddChildToHorizontalBox(RowLabel);

	UButton* LoadButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	LoadButton->SetBackgroundColor(FLinearColor(0.08f, 0.30f, 0.23f, 0.96f));
	LoadButton->SetIsEnabled(Row.bCanLoad);
	UTextBlock* LoadLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LoadLabel->SetText(NSLOCTEXT("GameXXKMainMenu", "LoadButton", "Load"));
	LoadLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	LoadButton->AddChild(LoadLabel);
	switch (Row.SlotIndex)
	{
	case 0:
		LoadButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleLoadSlot0Clicked);
		break;
	case 1:
		LoadButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleLoadSlot1Clicked);
		break;
	case 2:
		LoadButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleLoadSlot2Clicked);
		break;
	case 3:
		LoadButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleLoadSlot3Clicked);
		break;
	case 4:
		LoadButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleLoadSlot4Clicked);
		break;
	default:
		break;
	}
	RowBox->AddChildToHorizontalBox(LoadButton);

	UButton* DeleteButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	DeleteButton->SetBackgroundColor(FLinearColor(0.28f, 0.10f, 0.10f, 0.96f));
	DeleteButton->SetIsEnabled(Row.bCanDelete);
	UTextBlock* DeleteLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	DeleteLabel->SetText(NSLOCTEXT("GameXXKMainMenu", "DeleteSlotButton", "Delete"));
	DeleteLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	DeleteButton->AddChild(DeleteLabel);
	switch (Row.SlotIndex)
	{
	case 0:
		DeleteButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleDeleteSlot0Clicked);
		break;
	case 1:
		DeleteButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleDeleteSlot1Clicked);
		break;
	case 2:
		DeleteButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleDeleteSlot2Clicked);
		break;
	case 3:
		DeleteButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleDeleteSlot3Clicked);
		break;
	case 4:
		DeleteButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleDeleteSlot4Clicked);
		break;
	default:
		break;
	}
	RowBox->AddChildToHorizontalBox(DeleteButton);
}

void UGameXXKMainMenuWidget::AddCloseButton(UVerticalBox* Parent)
{
	UButton* CloseButton = AddMenuButton(Parent, NSLOCTEXT("GameXXKMainMenu", "CloseButton", "Close"));
	CloseButton->OnClicked.AddDynamic(this, &UGameXXKMainMenuWidget::HandleCloseModalClicked);
}

void UGameXXKMainMenuWidget::HandleStartGameClicked()
{
	StartGame();
}

void UGameXXKMainMenuWidget::HandleOpenContinueClicked()
{
	OpenContinueModal();
}

void UGameXXKMainMenuWidget::HandleOpenOptionsClicked()
{
	OpenOptionsModal();
}

void UGameXXKMainMenuWidget::HandleOpenQuitClicked()
{
	OpenQuitUnavailableModal();
}

void UGameXXKMainMenuWidget::HandleConfirmDeleteClicked()
{
	ConfirmDeleteSlot();
}

void UGameXXKMainMenuWidget::HandleCancelDeleteClicked()
{
	CancelDeleteSlot();
}

void UGameXXKMainMenuWidget::HandleCloseModalClicked()
{
	CloseActiveModal();
}

void UGameXXKMainMenuWidget::HandleSaveSlotClicked(int32 SlotIndex)
{
	ContinueFromSlotIndex(SlotIndex);
}

void UGameXXKMainMenuWidget::HandleDeleteSlotClicked(int32 SlotIndex)
{
	RequestDeleteSlot(SlotIndex);
}

void UGameXXKMainMenuWidget::HandleLoadSlot0Clicked()
{
	HandleSaveSlotClicked(0);
}

void UGameXXKMainMenuWidget::HandleLoadSlot1Clicked()
{
	HandleSaveSlotClicked(1);
}

void UGameXXKMainMenuWidget::HandleLoadSlot2Clicked()
{
	HandleSaveSlotClicked(2);
}

void UGameXXKMainMenuWidget::HandleLoadSlot3Clicked()
{
	HandleSaveSlotClicked(3);
}

void UGameXXKMainMenuWidget::HandleLoadSlot4Clicked()
{
	HandleSaveSlotClicked(4);
}

void UGameXXKMainMenuWidget::HandleDeleteSlot0Clicked()
{
	HandleDeleteSlotClicked(0);
}

void UGameXXKMainMenuWidget::HandleDeleteSlot1Clicked()
{
	HandleDeleteSlotClicked(1);
}

void UGameXXKMainMenuWidget::HandleDeleteSlot2Clicked()
{
	HandleDeleteSlotClicked(2);
}

void UGameXXKMainMenuWidget::HandleDeleteSlot3Clicked()
{
	HandleDeleteSlotClicked(3);
}

void UGameXXKMainMenuWidget::HandleDeleteSlot4Clicked()
{
	HandleDeleteSlotClicked(4);
}

void UGameXXKMainMenuWidget::HandleResolveEventGoldClicked()
{
	ExecuteEncounterCommand(ResolveEventGoldCommand);
}

void UGameXXKMainMenuWidget::HandleResolveCampHealClicked()
{
	ExecuteEncounterCommand(ResolveCampHealCommand);
}

void UGameXXKMainMenuWidget::HandleBuyHealingPowderClicked()
{
	ExecuteEncounterCommand(BuyHealingPowderCommand);
}

void UGameXXKMainMenuWidget::HandleSellHealingPowderClicked()
{
	ExecuteEncounterCommand(SellHealingPowderCommand);
}

void UGameXXKMainMenuWidget::HandleUseHealingPowderClicked()
{
	ExecuteEncounterCommand(UseHealingPowderCommand);
}

void UGameXXKMainMenuWidget::HandleCompleteMerchantNodeClicked()
{
	ExecuteEncounterCommand(CompleteMerchantNodeCommand);
}
