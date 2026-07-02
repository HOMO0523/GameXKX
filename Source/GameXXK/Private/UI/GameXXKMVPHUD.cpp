#include "UI/GameXXKMVPHUD.h"

#include "Engine/Canvas.h"
#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKMVPCommandRouter.h"
#include "UI/GameXXKPlayableRootWidget.h"

AGameXXKMVPHUD::AGameXXKMVPHUD()
{
	PlayableRootWidgetClass = UGameXXKPlayableRootWidget::StaticClass();
}

void AGameXXKMVPHUD::BeginPlay()
{
	Super::BeginPlay();

	if (UGameXXKPlayableRootWidget* RootWidget = CreatePlayableRootWidget())
	{
		if (!RootWidget->IsInViewport())
		{
			RootWidget->AddToViewport();
		}
	}
}

UGameXXKMVPSubsystem* AGameXXKMVPHUD::ResolveMVPSubsystem() const
{
	if (OverrideSubsystem)
	{
		return OverrideSubsystem;
	}

	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UGameXXKMVPSubsystem>() : nullptr;
}

void AGameXXKMVPHUD::SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem)
{
	OverrideSubsystem = InSubsystem;
	if (PlayableRootWidget)
	{
		PlayableRootWidget->SetMVPSubsystem(InSubsystem);
		PlayableRootWidget->RefreshFromState();
	}
}

void AGameXXKMVPHUD::SetStartGameSlotForTest(const FString& SlotName, int32 UserIndex)
{
	StartGameSlotNameOverride = SlotName;
	StartGameUserIndexOverride = UserIndex;
	if (PlayableRootWidget)
	{
		PlayableRootWidget->SetStartGameSlotForTest(SlotName, UserIndex);
	}
}

UGameXXKPlayableRootWidget* AGameXXKMVPHUD::CreatePlayableRootWidgetForTest()
{
	return CreatePlayableRootWidget();
}

bool AGameXXKMVPHUD::HasPlayableRootWidget() const
{
	return PlayableRootWidget != nullptr;
}

UGameXXKPlayableRootWidget* AGameXXKMVPHUD::GetPlayableRootWidget() const
{
	return PlayableRootWidget;
}

UGameXXKPlayableRootWidget* AGameXXKMVPHUD::CreatePlayableRootWidget()
{
	if (PlayableRootWidget)
	{
		return PlayableRootWidget;
	}

	TSubclassOf<UGameXXKPlayableRootWidget> WidgetClass = PlayableRootWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UGameXXKPlayableRootWidget::StaticClass();
	}
	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		PlayableRootWidget = CreateWidget<UGameXXKPlayableRootWidget>(PlayerController, WidgetClass);
	}
	if (!PlayableRootWidget)
	{
		PlayableRootWidget = NewObject<UGameXXKPlayableRootWidget>(this, WidgetClass);
	}
	if (PlayableRootWidget)
	{
		PlayableRootWidget->SetMVPSubsystem(ResolveMVPSubsystem());
		PlayableRootWidget->SetStartGameSlotForTest(StartGameSlotNameOverride, StartGameUserIndexOverride);
		PlayableRootWidget->RefreshFromState();
	}
	return PlayableRootWidget;
}

TArray<FGameXXKMVPCommandDescriptor> AGameXXKMVPHUD::BuildVisibleCommands() const
{
	return GameXXKMVPCommandRouter::BuildVisibleCommands(ResolveMVPSubsystem());
}

bool AGameXXKMVPHUD::HandleDemoCommand(FName CommandName)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bSucceeded = GameXXKMVPCommandRouter::ExecuteVisibleCommand(Subsystem, CommandName, StartGameSlotNameOverride, StartGameUserIndexOverride, true);
	if (PlayableRootWidget)
	{
		PlayableRootWidget->RefreshFromState();
	}
	return bSucceeded;
}

FText AGameXXKMVPHUD::BuildStatusText() const
{
	return GameXXKMVPCommandRouter::BuildStatusText(ResolveMVPSubsystem());
}

void AGameXXKMVPHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	const float PanelX = 24.0f;
	float CursorY = 24.0f;
	const float PanelWidth = 360.0f;
	const float ButtonHeight = 34.0f;
	const float Gap = 8.0f;

	DrawRect(FLinearColor(0.02f, 0.025f, 0.03f, 0.86f), PanelX - 12.0f, CursorY - 12.0f, PanelWidth + 24.0f, 520.0f);
	DrawText(TEXT("GameXXK MVP Playable Shell"), FColor::White, PanelX, CursorY, nullptr, 1.15f);
	CursorY += 34.0f;
	DrawText(BuildStatusText().ToString(), FColor(210, 220, 225), PanelX, CursorY, nullptr, 0.72f);
	CursorY += 42.0f;

	const TArray<FGameXXKMVPCommandDescriptor> Commands = BuildVisibleCommands();
	for (int32 Index = 0; Index < Commands.Num(); ++Index)
	{
		const FGameXXKMVPCommandDescriptor& Command = Commands[Index];
		const FLinearColor Fill = Command.bEnabled ? FLinearColor(0.08f, 0.30f, 0.23f, 0.94f) : FLinearColor(0.16f, 0.16f, 0.16f, 0.82f);
		const FColor TextColor = Command.bEnabled ? FColor::White : FColor(150, 150, 150);
		DrawRect(Fill, PanelX, CursorY, PanelWidth, ButtonHeight);
		DrawText(Command.Label.ToString(), TextColor, PanelX + 12.0f, CursorY + 8.0f, nullptr, 0.86f);
		if (Command.bEnabled)
		{
			AddHitBox(FVector2D(PanelX, CursorY), FVector2D(PanelWidth, ButtonHeight), Command.CommandName, true, Index);
		}
		CursorY += ButtonHeight + Gap;
	}
}

void AGameXXKMVPHUD::NotifyHitBoxClick(FName BoxName)
{
	HandleDemoCommand(BoxName);
}
