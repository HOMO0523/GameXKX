#include "MVP/GameXXKMVPPlayerController.h"

#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "MVP/GameXXKBattleScenePresenter.h"
#include "MVP/GameXXKBattleSceneUnitActor.h"
#include "MVP/GameXXKRouteEncounterSceneActor.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "PaperFlipbookComponent.h"
#include "Town/GameXXKHeroCharacter.h"
#include "Town/GameXXKTownPlayerPawn.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKMainMenuWidget.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"
#include "UI/GameXXKTownOverlayWidget.h"

namespace
{
	const FVector2D DefaultRouteMapViewportSize(1280.0f, 720.0f);
	const FName BattleSceneCameraTag(TEXT("GameXXK_BattleScene_Camera"));
	const FVector BattleSceneCameraFallbackLocation(-420.0f, 0.0f, 720.0f);
	const FRotator BattleSceneCameraFallbackRotation(-60.0f, 0.0f, 0.0f);
	constexpr float BattleSceneCameraFallbackFOV = 55.0f;

	FVector2D ResolveRouteMapViewportSize(const APlayerController* PlayerController)
	{
		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		if (PlayerController)
		{
			PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
		}

		if (ViewportWidth > 0 && ViewportHeight > 0)
		{
			return FVector2D(static_cast<float>(ViewportWidth), static_cast<float>(ViewportHeight));
		}
		return DefaultRouteMapViewportSize;
	}

	void ConfigureFullscreenRouteMapSlot(UWidget* RouteWidget)
	{
		UGameViewportSubsystem* ViewportSubsystem = UGameViewportSubsystem::Get();
		if (!ViewportSubsystem || !RouteWidget)
		{
			return;
		}

		FGameViewportWidgetSlot RouteSlot = ViewportSubsystem->GetWidgetSlot(RouteWidget);
		RouteSlot.Anchors = FAnchors(0.0f, 0.0f, 1.0f, 1.0f);
		RouteSlot.Offsets = FMargin(0.0f);
		RouteSlot.Alignment = FVector2D::ZeroVector;
		ViewportSubsystem->SetWidgetSlot(RouteWidget, RouteSlot);
	}

	void ConfigureBattleSceneCameraActor(ACameraActor* CameraActor)
	{
		if (!CameraActor)
		{
			return;
		}

		const FRotator CurrentRotation = CameraActor->GetActorRotation();
		if (CurrentRotation.Pitch < -65.0f || CurrentRotation.Pitch > -55.0f)
		{
			CameraActor->SetActorRotation(BattleSceneCameraFallbackRotation);
		}
		if (UCameraComponent* CameraComponent = CameraActor->GetCameraComponent())
		{
			CameraComponent->ProjectionMode = ECameraProjectionMode::Perspective;
			CameraComponent->FieldOfView = BattleSceneCameraFallbackFOV;
		}
	}
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

void AGameXXKMVPPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateBattleTargetingPointerFromMouse();
}

void AGameXXKMVPPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AGameXXKMVPPlayerController::HandleRouteMapPrimaryClick);
	}
}

bool AGameXXKMVPPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (Params.Key == EKeys::Escape && Params.Event == IE_Pressed)
	{
		EnsurePlayerFlowWidgets();
		if (BattleBoardWidget && BattleBoardWidget->CancelBattleTargeting())
		{
			return true;
		}
	}
	if (Params.Key == EKeys::I && Params.Event == IE_Pressed)
	{
		EnsurePlayerFlowWidgets();
		UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
		if (Subsystem && Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Town)
		{
			const bool bWasInventoryOpen = Subsystem->GetRuntimeState().TownPanelMode == EGameXXKTownPanelMode::Inventory;
			const bool bHandled = bWasInventoryOpen
				? Subsystem->CloseTownPanel()
				: Subsystem->OpenTownPanel(EGameXXKTownPanelMode::Inventory);
			if (bHandled)
			{
				RefreshPlayerFlowWidgets();
				return true;
			}
		}
	}
	if (Params.Key == EKeys::LeftMouseButton && Params.Event == IE_Released)
	{
		if (TryHandleBattleSceneLeftClick())
		{
			return true;
		}
		HandleRouteMapPrimaryClick();
	}
	if (Params.Key == EKeys::F && Params.Event == IE_Pressed && TryHandleRouteEncounterInteract())
	{
		return true;
	}
	if (Params.Key == EKeys::RightMouseButton && Params.Event == IE_Released && TryHandleBattleSceneRightClick())
	{
		return true;
	}
	return Super::InputKey(Params);
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

bool AGameXXKMVPPlayerController::OpenBattleCommandMenuForUnitForTest(AGameXXKBattleSceneUnitActor* UnitActor, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition)
{
	if (!UnitActor || !UnitActor->CanOpenPartyCommandMenu())
	{
		return false;
	}
	EnsurePlayerFlowWidgets();
	return BattleBoardWidget && BattleBoardWidget->OpenCommandMenuForPartyUnit(UnitActor->GetUnitIndex(), MenuScreenPosition, UnitScreenPosition);
}

bool AGameXXKMVPPlayerController::ToggleBattleCommandMenuForUnitForTest(AGameXXKBattleSceneUnitActor* UnitActor, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition)
{
	EnsurePlayerFlowWidgets();
	return ToggleBattleCommandMenuForUnit(UnitActor, MenuScreenPosition, UnitScreenPosition);
}

bool AGameXXKMVPPlayerController::ConfirmBattleTargetForUnitForTest(AGameXXKBattleSceneUnitActor* UnitActor)
{
	if (!UnitActor || !UnitActor->CanReceiveTargetedBattleAction())
	{
		return false;
	}
	EnsurePlayerFlowWidgets();
	return BattleBoardWidget && BattleBoardWidget->ConfirmTargetingEnemy(UnitActor->GetUnitIndex());
}

bool AGameXXKMVPPlayerController::CancelBattleTargetingForTest()
{
	EnsurePlayerFlowWidgets();
	return BattleBoardWidget && BattleBoardWidget->CancelBattleTargeting();
}

bool AGameXXKMVPPlayerController::UpdateBattleTargetingPointerForTest(FVector2D CursorScreenPosition)
{
	EnsurePlayerFlowWidgets();
	return UpdateBattleTargetingPointer(CursorScreenPosition);
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
	EnsureBattleScenePresenter();

	ApplyPlayerFlowInputMode();
}

void AGameXXKMVPPlayerController::ConfigureRouteMapWidgetViewport(UGameXXKOneGameRouteMapWidget* RouteWidget) const
{
	if (!RouteWidget)
	{
		return;
	}

	const FVector2D RouteMapViewportSize = ResolveRouteMapViewportSize(this);
	ConfigureFullscreenRouteMapSlot(RouteWidget);
	RouteWidget->SetRouteMapViewportGeometry(FVector2D::ZeroVector, RouteMapViewportSize);
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
	const EGameXXKScreen ActiveScreen = Subsystem ? Subsystem->GetRuntimeState().Screen : EGameXXKScreen::MainMenu;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	if (ActiveScreen == EGameXXKScreen::MainMenu && MainMenuWidget)
	{
		InputMode.SetWidgetToFocus(MainMenuWidget->TakeWidget());
	}
	else if (ActiveScreen == EGameXXKScreen::DungeonMap && RouteMapWidget)
	{
		InputMode.SetWidgetToFocus(RouteMapWidget->TakeWidget());
	}
	else if (ActiveScreen == EGameXXKScreen::Battle && BattleBoardWidget)
	{
		InputMode.SetWidgetToFocus(BattleBoardWidget->TakeWidget());
	}
	SetInputMode(InputMode);

	if (ActiveScreen == EGameXXKScreen::Town && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
	}
	else if ((ActiveScreen == EGameXXKScreen::RouteEvent
		|| ActiveScreen == EGameXXKScreen::RouteCamp
		|| ActiveScreen == EGameXXKScreen::RouteMerchant)
		&& FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
	}
}

void AGameXXKMVPPlayerController::HandleRouteMapPrimaryClick()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem
		|| Subsystem->GetRuntimeState().Screen != EGameXXKScreen::DungeonMap
		|| !RouteMapWidget
		|| RouteMapWidget->GetVisibility() != ESlateVisibility::Visible)
	{
		return;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	const FVector2D MousePosition(MouseX, MouseY);
	FVector2D SlateCursorPosition = MousePosition;
	const bool bHasSlateCursorPosition = FSlateApplication::IsInitialized();
	if (bHasSlateCursorPosition)
	{
		SlateCursorPosition = FSlateApplication::Get().GetCursorPos();
	}

	const TArray<FGameXXKOneGameRouteNodeVisualState> VisualStates = RouteMapWidget->GetRouteNodeVisualStatesForTest();
	for (int32 VisualIndex = VisualStates.Num() - 1; VisualIndex >= 0; --VisualIndex)
	{
		const FGameXXKOneGameRouteNodeVisualState& VisualState = VisualStates[VisualIndex];
		if (!VisualState.bEnabled || VisualState.NodeId == INDEX_NONE)
		{
			continue;
		}

		const FVector2D Min = VisualState.ViewportHitBoxPosition;
		const FVector2D Max = Min + VisualState.HitBoxSize;
		const bool bViewportHit = MousePosition.X >= Min.X
			&& MousePosition.Y >= Min.Y
			&& MousePosition.X <= Max.X
			&& MousePosition.Y <= Max.Y;

		const FVector2D ScreenMin = VisualState.ScreenHitBoxPosition;
		const FVector2D ScreenMax = ScreenMin + VisualState.HitBoxSize;
		const bool bScreenHit = bHasSlateCursorPosition
			&& SlateCursorPosition.X >= ScreenMin.X
			&& SlateCursorPosition.Y >= ScreenMin.Y
			&& SlateCursorPosition.X <= ScreenMax.X
			&& SlateCursorPosition.Y <= ScreenMax.Y;

		if (bViewportHit || bScreenHit)
		{
			RouteMapWidget->ExecuteRouteNodeById(VisualState.NodeId);
			return;
		}
	}
}

bool AGameXXKMVPPlayerController::TryHandleRouteEncounterInteract()
{
	AGameXXKRouteEncounterSceneActor* EncounterActor = FindRouteEncounterActorForActiveScreen();
	if (!EncounterActor)
	{
		return false;
	}

	return EncounterActor->ApplyDefaultInteraction(GetPawn());
}

AGameXXKRouteEncounterSceneActor* AGameXXKMVPPlayerController::FindRouteEncounterActorForActiveScreen() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const EGameXXKScreen ActiveScreen = Subsystem ? Subsystem->GetRuntimeState().Screen : EGameXXKScreen::MainMenu;
	if (ActiveScreen != EGameXXKScreen::RouteEvent
		&& ActiveScreen != EGameXXKScreen::RouteCamp
		&& ActiveScreen != EGameXXKScreen::RouteMerchant)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AGameXXKRouteEncounterSceneActor> It(World); It; ++It)
	{
		AGameXXKRouteEncounterSceneActor* Candidate = *It;
		if (Candidate && Candidate->MatchesRuntimeScreen(ActiveScreen))
		{
			return Candidate;
		}
	}
	return nullptr;
}

void AGameXXKMVPPlayerController::EnsureBattleScenePresenter()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bNeedsBattleScene = Subsystem
		&& Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Battle
		&& Subsystem->GetRuntimeState().bHasActiveBattle;
	UWorld* World = GetWorld();
	if (!bNeedsBattleScene || !World || !World->IsGameWorld())
	{
		BattleScenePresenter = nullptr;
		return;
	}

	if (!IsValid(BattleScenePresenter) || BattleScenePresenter->GetWorld() != World)
	{
		BattleScenePresenter = nullptr;
		for (TActorIterator<AGameXXKBattleScenePresenter> It(World); It; ++It)
		{
			BattleScenePresenter = *It;
			break;
		}
	}

	if (!BattleScenePresenter)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		BattleScenePresenter = World->SpawnActor<AGameXXKBattleScenePresenter>(
			AGameXXKBattleScenePresenter::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParameters);
	}

	if (BattleScenePresenter)
	{
		BattleScenePresenter->SetMVPSubsystemForTest(Subsystem);
		BattleScenePresenter->EnsureBattleScene();
	}
	ApplyBattleSceneCamera();
}

void AGameXXKMVPPlayerController::ApplyBattleSceneCamera()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	AActor* CameraActor = FindBattleSceneCameraActor();
	if (!CameraActor)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		ACameraActor* SpawnedCamera = World->SpawnActor<ACameraActor>(
			ACameraActor::StaticClass(),
			BattleSceneCameraFallbackLocation,
			BattleSceneCameraFallbackRotation,
			SpawnParameters);
		if (SpawnedCamera)
		{
			SpawnedCamera->Tags.AddUnique(BattleSceneCameraTag);
			if (UCameraComponent* CameraComponent = SpawnedCamera->GetCameraComponent())
			{
				CameraComponent->ProjectionMode = ECameraProjectionMode::Perspective;
				CameraComponent->FieldOfView = BattleSceneCameraFallbackFOV;
			}
			CameraActor = SpawnedCamera;
		}
	}

	if (ACameraActor* BattleCameraActor = Cast<ACameraActor>(CameraActor))
	{
		ConfigureBattleSceneCameraActor(BattleCameraActor);
	}

	if (CameraActor && GetViewTarget() != CameraActor)
	{
		SetViewTarget(CameraActor);
	}
}

bool AGameXXKMVPPlayerController::TryHandleBattleSceneRightClick()
{
	EnsurePlayerFlowWidgets();

	AGameXXKBattleSceneUnitActor* UnitActor = FindBattleSceneUnitUnderCursor(false);
	if (!UnitActor || !UnitActor->CanOpenPartyCommandMenu())
	{
		return BattleBoardWidget && BattleBoardWidget->CancelBattleTargeting();
	}

	float CursorX = 0.0f;
	float CursorY = 0.0f;
	if (!GetMousePosition(CursorX, CursorY))
	{
		return false;
	}
	FVector2D UnitScreenPosition(CursorX, CursorY);
	FVector UnitCommandWorldPosition = UnitActor->GetActorLocation();
	if (const UPaperFlipbookComponent* BattleVisual = UnitActor->GetBattleVisualComponent())
	{
		UnitCommandWorldPosition = BattleVisual->Bounds.Origin;
	}
	ProjectWorldLocationToScreen(UnitCommandWorldPosition, UnitScreenPosition, true);
	return ToggleBattleCommandMenuForUnit(UnitActor, FVector2D(CursorX, CursorY), UnitScreenPosition);
}

bool AGameXXKMVPPlayerController::TryHandleBattleSceneLeftClick()
{
	EnsurePlayerFlowWidgets();
	if (!BattleBoardWidget || !BattleBoardWidget->IsTargetingBattleActionForTest())
	{
		return false;
	}

	UpdateBattleTargetingPointerFromMouse();

	AGameXXKBattleSceneUnitActor* UnitActor = FindBattleSceneUnitUnderCursor(true);
	if (UnitActor && UnitActor->CanReceiveTargetedBattleAction())
	{
		return BattleBoardWidget->ConfirmTargetingEnemy(UnitActor->GetUnitIndex());
	}
	return BattleBoardWidget->KeepTargetingAfterEmptyClickForTest();
}

AGameXXKBattleSceneUnitActor* AGameXXKMVPPlayerController::FindBattleSceneUnitUnderCursor(bool bRequireEnemyTarget) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle)
	{
		return nullptr;
	}

	FHitResult HitResult;
	if (GetHitResultUnderCursor(ECC_Visibility, true, HitResult))
	{
		if (AGameXXKBattleSceneUnitActor* UnitActor = Cast<AGameXXKBattleSceneUnitActor>(HitResult.GetActor()))
		{
			if (bRequireEnemyTarget ? UnitActor->CanReceiveTargetedBattleAction() : UnitActor->CanOpenPartyCommandMenu())
			{
				return UnitActor;
			}
		}
	}
	return nullptr;
}

bool AGameXXKMVPPlayerController::ToggleBattleCommandMenuForUnit(AGameXXKBattleSceneUnitActor* UnitActor, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition)
{
	if (!UnitActor || !UnitActor->CanOpenPartyCommandMenu())
	{
		return false;
	}
	EnsurePlayerFlowWidgets();
	return BattleBoardWidget && BattleBoardWidget->ToggleCommandMenuForPartyUnit(UnitActor->GetUnitIndex(), MenuScreenPosition, UnitScreenPosition);
}

bool AGameXXKMVPPlayerController::UpdateBattleTargetingPointer(FVector2D CursorScreenPosition)
{
	if (!BattleBoardWidget || !BattleBoardWidget->IsTargetingBattleActionForTest())
	{
		return false;
	}
	BattleBoardWidget->UpdateTargetingPointer(CursorScreenPosition);
	return true;
}

bool AGameXXKMVPPlayerController::UpdateBattleTargetingPointerFromMouse()
{
	if (!BattleBoardWidget || !BattleBoardWidget->IsTargetingBattleActionForTest())
	{
		return false;
	}

	float CursorX = 0.0f;
	float CursorY = 0.0f;
	if (!GetMousePosition(CursorX, CursorY))
	{
		return false;
	}
	if (FSlateApplication::IsInitialized())
	{
		BattleBoardWidget->UpdateTargetingPointerFromSlateAbsolutePosition(FSlateApplication::Get().GetCursorPos());
		return true;
	}
	return UpdateBattleTargetingPointer(FVector2D(CursorX, CursorY));
}

AActor* AGameXXKMVPPlayerController::FindBattleSceneCameraActor() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	ACameraActor* FirstCamera = nullptr;
	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		ACameraActor* Candidate = *It;
		if (!Candidate)
		{
			continue;
		}
		if (!FirstCamera)
		{
			FirstCamera = Candidate;
		}
		if (Candidate->ActorHasTag(BattleSceneCameraTag))
		{
			return Candidate;
		}
#if WITH_EDITOR
		if (Candidate->GetActorLabel() == BattleSceneCameraTag.ToString())
		{
			return Candidate;
		}
#endif
	}
	return FirstCamera;
}
