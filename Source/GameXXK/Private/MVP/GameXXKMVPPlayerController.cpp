#include "MVP/GameXXKMVPPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Town/GameXXKHeroCharacter.h"
#include "Town/GameXXKTownPlayerPawn.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKMainMenuWidget.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"
#include "UI/GameXXKTownOverlayWidget.h"

namespace
{
	const FVector2D RouteMapViewportPosition(420.0f, 124.0f);
	const FVector2D RouteMapViewportSize(560.0f, 520.0f);
}

AGameXXKMVPPlayerController::AGameXXKMVPPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	MainMenuWidgetClass = UGameXXKMainMenuWidget::StaticClass();
	TownOverlayWidgetClass = UGameXXKTownOverlayWidget::StaticClass();
	RouteMapWidgetClass = UGameXXKOneGameRouteMapWidget::StaticClass();
	BattleBoardWidgetClass = UGameXXKBattleBoardWidget::StaticClass();
}

void AGameXXKMVPPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

	EnsurePlayerFlowWidgets();
	RefreshPlayerFlowWidgets();
}

void AGameXXKMVPPlayerController::FlushPressedKeys()
{
	Super::FlushPressedKeys();

	if (AGameXXKTownPlayerPawn* TownPawn = Cast<AGameXXKTownPlayerPawn>(GetPawn()))
	{
		TownPawn->ResetTownMovementInput();
	}
	else if (AGameXXKHeroCharacter* HeroCharacter = Cast<AGameXXKHeroCharacter>(GetPawn()))
	{
		HeroCharacter->ResetTownMovementInput();
	}
}

void AGameXXKMVPPlayerController::SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem)
{
	OverrideSubsystem = InSubsystem;
	RefreshPlayerFlowWidgets();
}

bool AGameXXKMVPPlayerController::EnsurePlayerFlowWidgetsForTest()
{
	return EnsurePlayerFlowWidgets();
}

void AGameXXKMVPPlayerController::RefreshPlayerFlowWidgetsForTest()
{
	RefreshPlayerFlowWidgetsFromState();
}

void AGameXXKMVPPlayerController::RefreshPlayerFlowWidgetsFromState()
{
	RefreshPlayerFlowWidgets();
}

UGameXXKMainMenuWidget* AGameXXKMVPPlayerController::GetMainMenuWidgetForTest() const
{
	return MainMenuWidget;
}

UGameXXKTownOverlayWidget* AGameXXKMVPPlayerController::GetTownOverlayWidgetForTest() const
{
	return TownOverlayWidget;
}

UGameXXKOneGameRouteMapWidget* AGameXXKMVPPlayerController::GetRouteMapWidgetForTest() const
{
	return RouteMapWidget;
}

UGameXXKBattleBoardWidget* AGameXXKMVPPlayerController::GetBattleBoardWidgetForTest() const
{
	return BattleBoardWidget;
}

bool AGameXXKMVPPlayerController::HasMainMenuWidgetInViewportForTest() const
{
	return MainMenuWidget && MainMenuWidget->IsInViewport();
}

bool AGameXXKMVPPlayerController::HasTownOverlayWidgetInViewportForTest() const
{
	return TownOverlayWidget && TownOverlayWidget->IsInViewport();
}

bool AGameXXKMVPPlayerController::HasRouteMapWidgetInViewportForTest() const
{
	return RouteMapWidget && RouteMapWidget->IsInViewport();
}

bool AGameXXKMVPPlayerController::HasBattleBoardWidgetInViewportForTest() const
{
	return BattleBoardWidget && BattleBoardWidget->IsInViewport();
}

UGameXXKMVPSubsystem* AGameXXKMVPPlayerController::ResolveMVPSubsystem() const
{
	if (OverrideSubsystem)
	{
		return OverrideSubsystem;
	}

	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UGameXXKMVPSubsystem>() : nullptr;
}

bool AGameXXKMVPPlayerController::EnsurePlayerFlowWidgets()
{
	const bool bCanAddToViewport = CanAddPlayerWidgetsToViewport();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();

	if (!MainMenuWidget)
	{
		TSubclassOf<UGameXXKMainMenuWidget> WidgetClass = MainMenuWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UGameXXKMainMenuWidget::StaticClass();
		}
		MainMenuWidget = bCanAddToViewport ? CreateWidget<UGameXXKMainMenuWidget>(this, WidgetClass) : nullptr;
		if (!MainMenuWidget)
		{
			MainMenuWidget = NewObject<UGameXXKMainMenuWidget>(this, WidgetClass);
		}
	}
	if (MainMenuWidget)
	{
		MainMenuWidget->SetMVPSubsystem(Subsystem);
		if (bCanAddToViewport && !MainMenuWidget->IsInViewport())
		{
			MainMenuWidget->AddToViewport(100);
		}
	}

	if (!TownOverlayWidget)
	{
		TSubclassOf<UGameXXKTownOverlayWidget> WidgetClass = TownOverlayWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UGameXXKTownOverlayWidget::StaticClass();
		}
		TownOverlayWidget = bCanAddToViewport ? CreateWidget<UGameXXKTownOverlayWidget>(this, WidgetClass) : nullptr;
		if (!TownOverlayWidget)
		{
			TownOverlayWidget = NewObject<UGameXXKTownOverlayWidget>(this, WidgetClass);
		}
	}
	if (TownOverlayWidget)
	{
		TownOverlayWidget->SetMVPSubsystem(Subsystem);
		if (bCanAddToViewport && !TownOverlayWidget->IsInViewport())
		{
			TownOverlayWidget->AddToViewport(30);
		}
	}

	if (!RouteMapWidget)
	{
		TSubclassOf<UGameXXKOneGameRouteMapWidget> WidgetClass = RouteMapWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UGameXXKOneGameRouteMapWidget::StaticClass();
		}
		RouteMapWidget = bCanAddToViewport ? CreateWidget<UGameXXKOneGameRouteMapWidget>(this, WidgetClass) : nullptr;
		if (!RouteMapWidget)
		{
			RouteMapWidget = NewObject<UGameXXKOneGameRouteMapWidget>(this, WidgetClass);
		}
	}
	if (RouteMapWidget)
	{
		RouteMapWidget->SetMVPSubsystem(Subsystem);
		ConfigureRouteMapWidgetViewport(RouteMapWidget);
		if (bCanAddToViewport && !RouteMapWidget->IsInViewport())
		{
			RouteMapWidget->AddToViewport(40);
		}
	}

	if (!BattleBoardWidget)
	{
		TSubclassOf<UGameXXKBattleBoardWidget> WidgetClass = BattleBoardWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UGameXXKBattleBoardWidget::StaticClass();
		}
		BattleBoardWidget = bCanAddToViewport ? CreateWidget<UGameXXKBattleBoardWidget>(this, WidgetClass) : nullptr;
		if (!BattleBoardWidget)
		{
			BattleBoardWidget = NewObject<UGameXXKBattleBoardWidget>(this, WidgetClass);
		}
	}
	if (BattleBoardWidget)
	{
		BattleBoardWidget->SetMVPSubsystem(Subsystem);
		if (bCanAddToViewport && !BattleBoardWidget->IsInViewport())
		{
			BattleBoardWidget->AddToViewport(50);
		}
	}

	RefreshPlayerFlowWidgets();
	return MainMenuWidget && TownOverlayWidget && RouteMapWidget && BattleBoardWidget;
}

void AGameXXKMVPPlayerController::RefreshPlayerFlowWidgets()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (MainMenuWidget)
	{
		MainMenuWidget->SetMVPSubsystem(Subsystem);
		MainMenuWidget->RefreshFromState();
	}
	if (TownOverlayWidget)
	{
		TownOverlayWidget->SetMVPSubsystem(Subsystem);
		TownOverlayWidget->RefreshFromState();
	}
	if (RouteMapWidget)
	{
		RouteMapWidget->SetMVPSubsystem(Subsystem);
		ConfigureRouteMapWidgetViewport(RouteMapWidget);
		RouteMapWidget->RefreshFromState();
	}
	if (BattleBoardWidget)
	{
		BattleBoardWidget->SetMVPSubsystem(Subsystem);
		BattleBoardWidget->RefreshFromState();
	}

	ApplyPlayerFlowInputMode();
}

void AGameXXKMVPPlayerController::ConfigureRouteMapWidgetViewport(UGameXXKOneGameRouteMapWidget* RouteWidget) const
{
	if (!RouteWidget)
	{
		return;
	}

	RouteWidget->SetAlignmentInViewport(FVector2D::ZeroVector);
	RouteWidget->SetPositionInViewport(RouteMapViewportPosition, false);
	RouteWidget->SetDesiredSizeInViewport(RouteMapViewportSize);
	RouteWidget->SetRouteMapViewportGeometry(RouteMapViewportPosition, RouteMapViewportSize);
}

bool AGameXXKMVPPlayerController::CanAddPlayerWidgetsToViewport() const
{
	const UWorld* World = GetWorld();
	return World && World->IsGameWorld();
}

void AGameXXKMVPPlayerController::ApplyPlayerFlowInputMode()
{
	if (!CanAddPlayerWidgetsToViewport())
	{
		return;
	}

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bMainMenuActive = Subsystem && Subsystem->GetRuntimeState().Screen == EGameXXKScreen::MainMenu;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	if (bMainMenuActive && MainMenuWidget)
	{
		InputMode.SetWidgetToFocus(MainMenuWidget->TakeWidget());
	}
	SetInputMode(InputMode);

	if (!bMainMenuActive && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
	}
}
