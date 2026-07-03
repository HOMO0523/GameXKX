#include "UI/GameXXKPlayableRootWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKMVPCommandRouter.h"

namespace GameXXKPlayableRootCommands
{
	static constexpr int32 MaxCommandButtonCount = 12;
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
	return GameXXKMVPCommandRouter::BuildVisibleCommands(ResolveMVPSubsystem(), StartGameSlotNameOverride, StartGameUserIndexOverride);
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

	const bool bSucceeded = GameXXKMVPCommandRouter::ExecuteVisibleCommand(Subsystem, CommandName, StartGameSlotNameOverride, StartGameUserIndexOverride);

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
		StatusText->SetText(GameXXKMVPCommandRouter::BuildStatusText(Subsystem));
	}

	const TArray<FGameXXKMVPCommandDescriptor> Commands = BuildVisibleCommands();
	for (int32 ButtonIndex = 0; ButtonIndex < CommandButtons.Num(); ++ButtonIndex)
	{
		const FGameXXKMVPCommandDescriptor* Command = Commands.IsValidIndex(ButtonIndex) ? &Commands[ButtonIndex] : nullptr;
		ConfigureCommandButton(ButtonIndex, Command);
	}
}

void UGameXXKPlayableRootWidget::HandleCommandButton0Clicked()
{
	ExecuteCommandButtonAtIndex(0);
}

void UGameXXKPlayableRootWidget::HandleCommandButton1Clicked()
{
	ExecuteCommandButtonAtIndex(1);
}

void UGameXXKPlayableRootWidget::HandleCommandButton2Clicked()
{
	ExecuteCommandButtonAtIndex(2);
}

void UGameXXKPlayableRootWidget::HandleCommandButton3Clicked()
{
	ExecuteCommandButtonAtIndex(3);
}

void UGameXXKPlayableRootWidget::HandleCommandButton4Clicked()
{
	ExecuteCommandButtonAtIndex(4);
}

void UGameXXKPlayableRootWidget::HandleCommandButton5Clicked()
{
	ExecuteCommandButtonAtIndex(5);
}

void UGameXXKPlayableRootWidget::HandleCommandButton6Clicked()
{
	ExecuteCommandButtonAtIndex(6);
}

void UGameXXKPlayableRootWidget::HandleCommandButton7Clicked()
{
	ExecuteCommandButtonAtIndex(7);
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

	CommandButtons.Reset();
	CommandButtonLabels.Reset();
	CommandButtonNames.Reset();
	for (int32 ButtonIndex = 0; ButtonIndex < GameXXKPlayableRootCommands::MaxCommandButtonCount; ++ButtonIndex)
	{
		UButton* CommandButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("CommandButton%d"), ButtonIndex));
		UTextBlock* CommandButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("CommandButtonLabel%d"), ButtonIndex));
		CommandButton->AddChild(CommandButtonLabel);
		BindCommandButton(CommandButton, ButtonIndex);
		RootBox->AddChildToVerticalBox(CommandButton);

		CommandButtons.Add(CommandButton);
		CommandButtonLabels.Add(CommandButtonLabel);
		CommandButtonNames.Add(NAME_None);
	}
}

void UGameXXKPlayableRootWidget::ConfigureCommandButton(int32 ButtonIndex, const FGameXXKMVPCommandDescriptor* Command)
{
	if (!CommandButtons.IsValidIndex(ButtonIndex))
	{
		return;
	}

	UButton* Button = CommandButtons[ButtonIndex];
	UTextBlock* Label = CommandButtonLabels.IsValidIndex(ButtonIndex) ? CommandButtonLabels[ButtonIndex].Get() : nullptr;
	if (!Button)
	{
		return;
	}

	Button->SetVisibility(Command ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Button->SetIsEnabled(Command && Command->bEnabled);
	if (CommandButtonNames.IsValidIndex(ButtonIndex))
	{
		CommandButtonNames[ButtonIndex] = Command ? Command->CommandName : NAME_None;
	}
	if (Label)
	{
		Label->SetText(Command ? Command->Label : FText::GetEmpty());
	}
}

void UGameXXKPlayableRootWidget::ExecuteCommandButtonAtIndex(int32 ButtonIndex)
{
	if (!CommandButtonNames.IsValidIndex(ButtonIndex) || CommandButtonNames[ButtonIndex].IsNone())
	{
		return;
	}
	ExecuteVisibleCommand(CommandButtonNames[ButtonIndex]);
}

void UGameXXKPlayableRootWidget::BindCommandButton(UButton* Button, int32 ButtonIndex)
{
	if (!Button)
	{
		return;
	}

	switch (ButtonIndex)
	{
	case 0:
		Button->OnClicked.AddDynamic(this, &UGameXXKPlayableRootWidget::HandleCommandButton0Clicked);
		break;
	case 1:
		Button->OnClicked.AddDynamic(this, &UGameXXKPlayableRootWidget::HandleCommandButton1Clicked);
		break;
	case 2:
		Button->OnClicked.AddDynamic(this, &UGameXXKPlayableRootWidget::HandleCommandButton2Clicked);
		break;
	case 3:
		Button->OnClicked.AddDynamic(this, &UGameXXKPlayableRootWidget::HandleCommandButton3Clicked);
		break;
	case 4:
		Button->OnClicked.AddDynamic(this, &UGameXXKPlayableRootWidget::HandleCommandButton4Clicked);
		break;
	case 5:
		Button->OnClicked.AddDynamic(this, &UGameXXKPlayableRootWidget::HandleCommandButton5Clicked);
		break;
	case 6:
		Button->OnClicked.AddDynamic(this, &UGameXXKPlayableRootWidget::HandleCommandButton6Clicked);
		break;
	case 7:
		Button->OnClicked.AddDynamic(this, &UGameXXKPlayableRootWidget::HandleCommandButton7Clicked);
		break;
	default:
		break;
	}
}
