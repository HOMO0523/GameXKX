#include "UI/GameXXKTownOverlayWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKMVPCommandRouter.h"

namespace
{
	static const FName EnterDungeonCommand(TEXT("EnterDungeon"));
	static const FName SaveSlotOneCommand(TEXT("SaveSlot1"));
	const FVector2D TownPanelPosition(28.0f, 28.0f);
	const FVector2D TownPanelSize(360.0f, 170.0f);

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

	SaveButton = AddCommandButton(PanelBox, NSLOCTEXT("GameXXKTownOverlay", "SaveSlotOne", "Save Slot 1"));
	if (SaveButton)
	{
		SaveButton->OnClicked.AddDynamic(this, &UGameXXKTownOverlayWidget::HandleSaveClicked);
	}
}

void UGameXXKTownOverlayWidget::ConfigureProgrammaticLayout()
{
	if (StatusTextBlock)
	{
		StatusTextBlock->SetText(CachedStatusText);
	}

	const TArray<FGameXXKMVPCommandDescriptor> Commands = BuildTownActionsForTest();
	if (SaveButton)
	{
		SaveButton->SetIsEnabled(IsCommandEnabled(Commands, SaveSlotOneCommand));
	}
	if (EnterRouteButton)
	{
		EnterRouteButton->SetIsEnabled(IsCommandEnabled(Commands, EnterDungeonCommand));
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
