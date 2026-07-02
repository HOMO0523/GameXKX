#include "UI/GameXXKPlayableRootWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"

namespace GameXXKPlayableRootCommands
{
	static const FName StartGame(TEXT("StartGame"));
	static const FName SelectQingshan(TEXT("SelectQingshan"));
	static const FName SelectTanjiang(TEXT("SelectTanjiang"));

	static void AddCommand(TArray<FGameXXKMVPCommandDescriptor>& Commands, FName Name, const TCHAR* Label, bool bEnabled)
	{
		Commands.Emplace(Name, FText::FromString(Label), bEnabled);
	}
}

void UGameXXKPlayableRootWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	RefreshFromState();
}

EGameXXKScreen UGameXXKPlayableRootWidget::GetCurrentScreen() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem ? Subsystem->GetRuntimeState().Screen : EGameXXKScreen::MainMenu;
}

TArray<FGameXXKMVPCommandDescriptor> UGameXXKPlayableRootWidget::BuildVisibleCommands() const
{
	TArray<FGameXXKMVPCommandDescriptor> Commands;
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return Commands;
	}

	using namespace GameXXKPlayableRootCommands;
	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	switch (State.Screen)
	{
	case EGameXXKScreen::MainMenu:
		AddCommand(Commands, StartGame, TEXT("Start / Continue"), true);
		break;
	case EGameXXKScreen::WorldMap:
		AddCommand(Commands, SelectQingshan, TEXT("Qingshan Town"), State.UnlockedRegions.Contains(UGameXXKMVPRules::RegionQingshan()));
		AddCommand(Commands, SelectTanjiang, TEXT("Tanjiang Town"), State.UnlockedRegions.Contains(UGameXXKMVPRules::RegionTanjiang()));
		break;
	default:
		break;
	}

	return Commands;
}

bool UGameXXKPlayableRootWidget::HasVisibleCommand(FName CommandName, bool bExpectedEnabled) const
{
	const TArray<FGameXXKMVPCommandDescriptor> Commands = BuildVisibleCommands();
	const FGameXXKMVPCommandDescriptor* MatchingCommand = Commands.FindByPredicate([CommandName](const FGameXXKMVPCommandDescriptor& Command)
	{
		return Command.CommandName == CommandName;
	});
	return MatchingCommand && MatchingCommand->bEnabled == bExpectedEnabled;
}

bool UGameXXKPlayableRootWidget::ExecuteVisibleCommand(FName CommandName)
{
	const TArray<FGameXXKMVPCommandDescriptor> Commands = BuildVisibleCommands();
	const FGameXXKMVPCommandDescriptor* MatchingCommand = Commands.FindByPredicate([CommandName](const FGameXXKMVPCommandDescriptor& Command)
	{
		return Command.CommandName == CommandName;
	});
	if (!MatchingCommand || !MatchingCommand->bEnabled)
	{
		return false;
	}

	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}

	using namespace GameXXKPlayableRootCommands;
	bool bSucceeded = false;
	if (CommandName == StartGame)
	{
		bSucceeded = StartGameSlotNameOverride.IsEmpty()
			? Subsystem->StartGame()
			: Subsystem->StartGameFromSlot(StartGameSlotNameOverride, StartGameUserIndexOverride);
	}
	else if (CommandName == SelectQingshan)
	{
		bSucceeded = Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan());
	}
	else if (CommandName == SelectTanjiang)
	{
		bSucceeded = Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionTanjiang());
	}

	RefreshFromState();
	return bSucceeded;
}

void UGameXXKPlayableRootWidget::SetStartGameSlotForTest(const FString& SlotName, int32 UserIndex)
{
	StartGameSlotNameOverride = SlotName;
	StartGameUserIndexOverride = UserIndex;
}

void UGameXXKPlayableRootWidget::RefreshFromState()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (StatusText)
	{
		const EGameXXKScreen Screen = Subsystem ? Subsystem->GetRuntimeState().Screen : EGameXXKScreen::MainMenu;
		const FString ScreenLabel = Screen == EGameXXKScreen::MainMenu ? TEXT("Main Menu")
			: Screen == EGameXXKScreen::WorldMap ? TEXT("World Map")
			: Screen == EGameXXKScreen::Town ? TEXT("Town")
			: TEXT("GameXXK MVP");
		StatusText->SetText(FText::FromString(ScreenLabel));
	}

	using namespace GameXXKPlayableRootCommands;
	ConfigureCommandButton(StartButton, StartButtonLabel, StartGame, FText::FromString(TEXT("Start / Continue")));
	ConfigureCommandButton(QingshanButton, QingshanButtonLabel, SelectQingshan, FText::FromString(TEXT("Qingshan Town")));
	ConfigureCommandButton(TanjiangButton, TanjiangButtonLabel, SelectTanjiang, FText::FromString(TEXT("Tanjiang Town")));
}

void UGameXXKPlayableRootWidget::HandleStartClicked()
{
	ExecuteVisibleCommand(GameXXKPlayableRootCommands::StartGame);
}

void UGameXXKPlayableRootWidget::HandleQingshanClicked()
{
	ExecuteVisibleCommand(GameXXKPlayableRootCommands::SelectQingshan);
}

void UGameXXKPlayableRootWidget::HandleTanjiangClicked()
{
	ExecuteVisibleCommand(GameXXKPlayableRootCommands::SelectTanjiang);
}

void UGameXXKPlayableRootWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree || RootBox || WidgetTree->RootWidget)
	{
		return;
	}

	RootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GameXXKPlayableRoot"));
	WidgetTree->RootWidget = RootBox;

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	RootBox->AddChildToVerticalBox(StatusText);

	StartButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StartButton"));
	StartButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StartButtonLabel"));
	StartButton->AddChild(StartButtonLabel);
	StartButton->OnClicked.AddDynamic(this, &UGameXXKPlayableRootWidget::HandleStartClicked);
	RootBox->AddChildToVerticalBox(StartButton);

	QingshanButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("QingshanButton"));
	QingshanButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QingshanButtonLabel"));
	QingshanButton->AddChild(QingshanButtonLabel);
	QingshanButton->OnClicked.AddDynamic(this, &UGameXXKPlayableRootWidget::HandleQingshanClicked);
	RootBox->AddChildToVerticalBox(QingshanButton);

	TanjiangButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TanjiangButton"));
	TanjiangButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TanjiangButtonLabel"));
	TanjiangButton->AddChild(TanjiangButtonLabel);
	TanjiangButton->OnClicked.AddDynamic(this, &UGameXXKPlayableRootWidget::HandleTanjiangClicked);
	RootBox->AddChildToVerticalBox(TanjiangButton);
}

void UGameXXKPlayableRootWidget::ConfigureCommandButton(UButton* Button, UTextBlock* Label, FName CommandName, const FText& CommandLabel)
{
	if (!Button)
	{
		return;
	}

	const TArray<FGameXXKMVPCommandDescriptor> Commands = BuildVisibleCommands();
	const FGameXXKMVPCommandDescriptor* MatchingCommand = Commands.FindByPredicate([CommandName](const FGameXXKMVPCommandDescriptor& Command)
	{
		return Command.CommandName == CommandName;
	});

	Button->SetVisibility(MatchingCommand ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Button->SetIsEnabled(MatchingCommand && MatchingCommand->bEnabled);
	if (Label)
	{
		Label->SetText(MatchingCommand ? MatchingCommand->Label : CommandLabel);
	}
}
