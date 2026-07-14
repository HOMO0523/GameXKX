#include "MVP/GameXXKMVPPlayerController.h"

#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKBattleScenePresenter.h"
#include "MVP/GameXXKBattleSceneUnitActor.h"
#include "MVP/GameXXKRouteEncounterSceneActor.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "PaperFlipbookComponent.h"
#include "Town/GameXXKHeroCharacter.h"
#include "Town/GameXXKTownNpcActor.h"
#include "Town/GameXXKTownNpcCharacter.h"
#include "Town/GameXXKTownPlayerPawn.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "UI/GameXXKMainMenuWidget.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"
#include "UI/GameXXKQuestDialogWidget.h"
#include "UI/GameXXKTaskPanelWidget.h"
#include "UI/GameXXKTownHudWidget.h"
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

	void ConfigureFullscreenInventoryWindowSlot(UWidget* InventoryWidget)
	{
		UGameViewportSubsystem* ViewportSubsystem = UGameViewportSubsystem::Get();
		if (!ViewportSubsystem || !InventoryWidget)
		{
			return;
		}

		FGameViewportWidgetSlot InventorySlot = ViewportSubsystem->GetWidgetSlot(InventoryWidget);
		InventorySlot.Anchors = FAnchors(0.0f, 0.0f, 1.0f, 1.0f);
		InventorySlot.Offsets = FMargin(0.0f);
		InventorySlot.Alignment = FVector2D::ZeroVector;
		ViewportSubsystem->SetWidgetSlot(InventoryWidget, InventorySlot);
	}

	void ConfigureFullscreenTaskPanelSlot(UWidget* TaskPanelWidget)
	{
		UGameViewportSubsystem* ViewportSubsystem = UGameViewportSubsystem::Get();
		if (!ViewportSubsystem || !TaskPanelWidget)
		{
			return;
		}

		FGameViewportWidgetSlot TaskSlot = ViewportSubsystem->GetWidgetSlot(TaskPanelWidget);
		TaskSlot.Anchors = FAnchors(0.0f, 0.0f, 1.0f, 1.0f);
		TaskSlot.Offsets = FMargin(0.0f);
		TaskSlot.Alignment = FVector2D::ZeroVector;
		ViewportSubsystem->SetWidgetSlot(TaskPanelWidget, TaskSlot);
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
	InventoryWindowWidgetClass = UGameXXKInventoryWindowWidget::StaticClass();
	QuestDialogWidgetClass = UGameXXKQuestDialogWidget::StaticClass();
	TaskPanelWidgetClass = UGameXXKTaskPanelWidget::StaticClass();
	TownHudWidgetClass = UGameXXKTownHudWidget::StaticClass();
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
	// Route-map pointer-up handling is centralized in InputKey so a physical
	// release cannot be dispatched once by the raw binding and once again by
	// the controller override.
}

bool AGameXXKMVPPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	if (Params.Key == EKeys::Escape && Params.Event == IE_Pressed)
	{
		EnsurePlayerFlowWidgets();
		if (QuestDialogWidget && QuestDialogWidget->IsDialogOpen())
		{
			return CloseQuestDialog();
		}
		if (TaskPanelWidget && TaskPanelWidget->IsTaskPanelOpenForTest())
		{
			return CloseTaskPanel();
		}
		if (InventoryWindowWidget && InventoryWindowWidget->IsWindowVisibleForTest())
		{
			return CloseInventoryWindow();
		}
		if (BattleBoardWidget && BattleBoardWidget->CancelBattleTargeting())
		{
			return true;
		}
	}
	if (QuestDialogWidget && QuestDialogWidget->IsDialogOpen())
	{
		return Super::InputKey(Params);
	}
	if (TaskPanelWidget && TaskPanelWidget->IsTaskPanelOpenForTest()
		&& (Params.Key == EKeys::F || Params.Key == EKeys::Q || Params.Key == EKeys::I))
	{
		return true;
	}
	if (Params.Key == EKeys::Q && Params.Event == IE_Pressed)
	{
		EnsurePlayerFlowWidgets();
		UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
		if (Subsystem && Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Town)
		{
			// While a task modal is open Q is intentionally consumed above; Escape/back
			// is the close action. Q only opens the accepted-task panel from gameplay.
			const bool bHandled = OpenTaskPanel();
			if (bHandled)
			{
				RefreshPlayerFlowWidgets();
				return true;
			}
		}
	}
	if (Params.Key == EKeys::I && Params.Event == IE_Pressed)
	{
		EnsurePlayerFlowWidgets();
		UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
		if (Subsystem && Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Town)
		{
			const bool bWasInventoryOpen = InventoryWindowWidget
				&& InventoryWindowWidget->GetWindowModeForTest() == EGameXXKInventoryWindowMode::FreeInventory;
			const bool bHandled = bWasInventoryOpen ? CloseInventoryWindow() : OpenFreeInventoryWindow();
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

UGameXXKInventoryWindowWidget* AGameXXKMVPPlayerController::GetInventoryWindowWidgetForTest() const
{
	return InventoryWindowWidget;
}

UGameXXKQuestDialogWidget* AGameXXKMVPPlayerController::GetQuestDialogWidgetForTest() const
{
	return QuestDialogWidget;
}

UGameXXKTaskPanelWidget* AGameXXKMVPPlayerController::GetTaskPanelWidgetForTest() const
{
	return TaskPanelWidget;
}

UGameXXKTownHudWidget* AGameXXKMVPPlayerController::GetTownHudWidgetForTest() const
{
	return TownHudWidget;
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

bool AGameXXKMVPPlayerController::IsInventoryWindowModalInputLockedForTest() const
{
	return IsInventoryWindowModalInputLocked();
}

bool AGameXXKMVPPlayerController::IsInventoryWindowModalInputLocked() const
{
	return InventoryWindowWidget && InventoryWindowWidget->IsModalInputLockActiveForTest();
}

bool AGameXXKMVPPlayerController::IsQuestDialogOpenForTest() const
{
	return QuestDialogWidget && QuestDialogWidget->IsDialogOpen();
}

bool AGameXXKMVPPlayerController::IsQuestDialogModalInputLockedForTest() const
{
	return IsQuestDialogOpenForTest();
}

bool AGameXXKMVPPlayerController::OpenQuestDialogPreviewForTest()
{
	UGameXXKQuestDialogWidget* Dialog = EnsureQuestDialogWidget();
	if (!Dialog)
	{
		return false;
	}

	PendingQuestNpc.Reset();
	PendingQuestInstigator.Reset();
	Dialog->OpenDialog();
	SetIgnoreMoveInput(true);
	ApplyPlayerFlowInputMode();
	return true;
}

bool AGameXXKMVPPlayerController::OpenQuestDialogForNpc(AActor* QuestNpc, APawn* InstigatorPawn)
{
	// Do not replace the active task-offer modal or overwrite its pending NPC.
	if (!QuestNpc || !InstigatorPawn
		|| (QuestDialogWidget && QuestDialogWidget->IsDialogOpen())
		|| (TaskPanelWidget && TaskPanelWidget->IsTaskPanelOpenForTest()))
	{
		return false;
	}
	UGameXXKQuestDialogWidget* Dialog = EnsureQuestDialogWidget();
	if (!Dialog)
	{
		return false;
	}

	PendingQuestNpc = QuestNpc;
	PendingQuestInstigator = InstigatorPawn;
	Dialog->OpenDialog();
	FlushPressedKeys();
	SetIgnoreMoveInput(true);
	ApplyPlayerFlowInputMode();
	return true;
}

bool AGameXXKMVPPlayerController::OpenTaskOfferPanelForNpc(AActor* QuestNpc, APawn* InstigatorPawn)
{
	// A second modal request is rejected before it can replace the original
	// pending NPC/instigator pair.
	if (!QuestNpc || !InstigatorPawn
		|| (QuestDialogWidget && QuestDialogWidget->IsDialogOpen())
		|| (TaskPanelWidget && TaskPanelWidget->IsTaskPanelOpenForTest()))
	{
		return false;
	}

	CloseInventoryWindow();
	UGameXXKTaskPanelWidget* TaskPanel = EnsureTaskPanelWidget();
	if (!TaskPanel || !TaskPanel->OpenTaskOfferPanel())
	{
		return false;
	}

	PendingQuestNpc = QuestNpc;
	PendingQuestInstigator = InstigatorPawn;
	FlushPressedKeys();
	SetIgnoreMoveInput(true);
	ApplyPlayerFlowInputMode();
	return true;
}

bool AGameXXKMVPPlayerController::AcceptQuestDialog()
{
	if (!QuestDialogWidget || !QuestDialogWidget->IsDialogOpen())
	{
		return false;
	}

	const bool bAccepted = ConfirmPendingQuestNpc(UGameXXKMVPRules::TaskQingshanMain());
	if (bAccepted)
	{
		QuestDialogWidget->OnQuestAccepted();
	}

	CloseQuestDialog();
	RefreshPlayerFlowWidgets();
	return bAccepted;
}

bool AGameXXKMVPPlayerController::AcceptTaskOfferById(FName TaskId)
{
	if (TaskId.IsNone()
		|| !TaskPanelWidget
		|| !TaskPanelWidget->IsTaskPanelOpenForTest()
		|| !TaskPanelWidget->IsShowingTaskOffersForTest()
		|| !TaskPanelWidget->HasVisibleTaskOffer(TaskId))
	{
		return false;
	}

	const bool bAccepted = ConfirmPendingQuestNpc(TaskId);
	if (!bAccepted)
	{
		return false;
	}

	PendingQuestNpc.Reset();
	PendingQuestInstigator.Reset();
	RefreshPlayerFlowWidgets();
	return true;
}

bool AGameXXKMVPPlayerController::AcceptTaskOffer()
{
	// A task click must explicitly carry the row's id. Accepting the pending NPC
	// without that identity would silently turn any row into the first main quest.
	return false;
}

bool AGameXXKMVPPlayerController::CloseQuestDialog()
{
	if (!QuestDialogWidget || !QuestDialogWidget->CloseDialog())
	{
		return false;
	}

	PendingQuestNpc.Reset();
	PendingQuestInstigator.Reset();
	SetIgnoreMoveInput(false);
	ApplyPlayerFlowInputMode();
	return true;
}

bool AGameXXKMVPPlayerController::OpenFreeInventoryWindow()
{
	CloseTaskPanel();
	UGameXXKInventoryWindowWidget* InventoryWindow = EnsureInventoryWindowWidget();
	const bool bOpened = InventoryWindow && InventoryWindow->OpenFreeInventory();
	if (bOpened)
	{
		ApplyPlayerFlowInputMode();
	}
	return bOpened;
}

bool AGameXXKMVPPlayerController::OpenMerchantTradeWindow()
{
	if (InventoryWindowWidget
		&& InventoryWindowWidget->GetWindowModeForTest() == EGameXXKInventoryWindowMode::MerchantTrade
		&& InventoryWindowWidget->IsWindowVisibleForTest())
	{
		return CloseInventoryWindow();
	}

	CloseTaskPanel();
	UGameXXKInventoryWindowWidget* InventoryWindow = EnsureInventoryWindowWidget();
	const bool bOpened = InventoryWindow && InventoryWindow->OpenMerchantTrade();
	if (bOpened)
	{
		if (UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem())
		{
			Subsystem->CloseTownPanel();
		}
		ApplyPlayerFlowInputMode();
	}
	return bOpened;
}

bool AGameXXKMVPPlayerController::CloseInventoryWindow()
{
	const bool bWasMerchantTrade = InventoryWindowWidget
		&& InventoryWindowWidget->GetWindowModeForTest() == EGameXXKInventoryWindowMode::MerchantTrade;
	const bool bClosed = InventoryWindowWidget && InventoryWindowWidget->CloseInventoryWindow();
	if (bClosed)
	{
		if (bWasMerchantTrade)
		{
			if (UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem())
			{
				Subsystem->CloseTownPanel();
			}
		}
		ApplyPlayerFlowInputMode();
	}
	return bClosed;
}

bool AGameXXKMVPPlayerController::OpenTaskPanel()
{
	if (QuestDialogWidget && QuestDialogWidget->IsDialogOpen())
	{
		return false;
	}
	CloseInventoryWindow();
	PendingQuestNpc.Reset();
	PendingQuestInstigator.Reset();
	UGameXXKTaskPanelWidget* TaskPanel = EnsureTaskPanelWidget();
	const bool bOpened = TaskPanel && TaskPanel->OpenTaskPanel();
	if (bOpened)
	{
		FlushPressedKeys();
		SetIgnoreMoveInput(true);
		ApplyPlayerFlowInputMode();
	}
	return bOpened;
}

bool AGameXXKMVPPlayerController::CloseTaskPanel()
{
	const bool bClosed = TaskPanelWidget && TaskPanelWidget->CloseTaskPanel();
	if (bClosed)
	{
		PendingQuestNpc.Reset();
		PendingQuestInstigator.Reset();
		SetIgnoreMoveInput(false);
		ApplyPlayerFlowInputMode();
	}
	return bClosed;
}

bool AGameXXKMVPPlayerController::IsTaskPanelOpenForTest() const
{
	return TaskPanelWidget && TaskPanelWidget->IsTaskPanelOpenForTest();
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

	if (!TownHudWidget)
	{
		EnsureTownHudWidget();
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

	if (!InventoryWindowWidget)
	{
		EnsureInventoryWindowWidget();
	}

	if (!QuestDialogWidget)
	{
		EnsureQuestDialogWidget();
	}

	if (!TaskPanelWidget)
	{
		EnsureTaskPanelWidget();
	}

	RefreshPlayerFlowWidgets();
	return MainMenuWidget && TownOverlayWidget && TownHudWidget && RouteMapWidget && BattleBoardWidget && InventoryWindowWidget && QuestDialogWidget && TaskPanelWidget;
}

UGameXXKInventoryWindowWidget* AGameXXKMVPPlayerController::EnsureInventoryWindowWidget()
{
	const bool bCanAddToViewport = CanAddPlayerWidgetsToViewport();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	bool bCreatedInventoryWindow = false;
	if (!InventoryWindowWidget)
	{
		TSubclassOf<UGameXXKInventoryWindowWidget> WidgetClass = InventoryWindowWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UGameXXKInventoryWindowWidget::StaticClass();
		}
		InventoryWindowWidget = bCanAddToViewport ? CreateWidget<UGameXXKInventoryWindowWidget>(this, WidgetClass) : nullptr;
		if (!InventoryWindowWidget)
		{
			InventoryWindowWidget = NewObject<UGameXXKInventoryWindowWidget>(this, WidgetClass);
		}
		bCreatedInventoryWindow = InventoryWindowWidget != nullptr;
	}
	if (InventoryWindowWidget)
	{
		InventoryWindowWidget->SetMVPSubsystem(Subsystem);
		if (bCreatedInventoryWindow)
		{
			InventoryWindowWidget->CloseInventoryWindow();
		}
		ConfigureFullscreenInventoryWindowSlot(InventoryWindowWidget);
		if (bCanAddToViewport && !InventoryWindowWidget->IsInViewport())
		{
			InventoryWindowWidget->AddToViewport(120);
			ConfigureFullscreenInventoryWindowSlot(InventoryWindowWidget);
		}
	}
	return InventoryWindowWidget;
}

UGameXXKQuestDialogWidget* AGameXXKMVPPlayerController::EnsureQuestDialogWidget()
{
	const bool bCanAddToViewport = CanAddPlayerWidgetsToViewport();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	bool bCreatedQuestDialog = false;
	if (!QuestDialogWidget)
	{
		TSubclassOf<UGameXXKQuestDialogWidget> WidgetClass = QuestDialogWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UGameXXKQuestDialogWidget::StaticClass();
		}
		QuestDialogWidget = bCanAddToViewport ? CreateWidget<UGameXXKQuestDialogWidget>(this, WidgetClass) : nullptr;
		if (!QuestDialogWidget)
		{
			QuestDialogWidget = NewObject<UGameXXKQuestDialogWidget>(this, WidgetClass);
		}
		bCreatedQuestDialog = QuestDialogWidget != nullptr;
	}
	if (QuestDialogWidget)
	{
		QuestDialogWidget->SetMVPSubsystem(Subsystem);
		if (bCreatedQuestDialog)
		{
			QuestDialogWidget->CloseDialog();
		}
		if (bCanAddToViewport && !QuestDialogWidget->IsInViewport())
		{
			QuestDialogWidget->AddToViewport(160);
		}
	}
	return QuestDialogWidget;
}

UGameXXKTaskPanelWidget* AGameXXKMVPPlayerController::EnsureTaskPanelWidget()
{
	const bool bCanAddToViewport = CanAddPlayerWidgetsToViewport();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	bool bCreatedTaskPanel = false;
	if (!TaskPanelWidget)
	{
		TSubclassOf<UGameXXKTaskPanelWidget> WidgetClass = TaskPanelWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UGameXXKTaskPanelWidget::StaticClass();
		}
		TaskPanelWidget = bCanAddToViewport ? CreateWidget<UGameXXKTaskPanelWidget>(this, WidgetClass) : nullptr;
		if (!TaskPanelWidget)
		{
			TaskPanelWidget = NewObject<UGameXXKTaskPanelWidget>(this, WidgetClass);
		}
		bCreatedTaskPanel = TaskPanelWidget != nullptr;
	}
	if (TaskPanelWidget)
	{
		TaskPanelWidget->SetMVPSubsystem(Subsystem);
		if (bCreatedTaskPanel)
		{
			TaskPanelWidget->CloseTaskPanel();
		}
		ConfigureFullscreenTaskPanelSlot(TaskPanelWidget);
		if (bCanAddToViewport && !TaskPanelWidget->IsInViewport())
		{
			TaskPanelWidget->AddToViewport(140);
			ConfigureFullscreenTaskPanelSlot(TaskPanelWidget);
		}
	}
	return TaskPanelWidget;
}

UGameXXKTownHudWidget* AGameXXKMVPPlayerController::EnsureTownHudWidget()
{
	const bool bCanAddToViewport = CanAddPlayerWidgetsToViewport();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!TownHudWidget)
	{
		TSubclassOf<UGameXXKTownHudWidget> WidgetClass = TownHudWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UGameXXKTownHudWidget::StaticClass();
		}
		TownHudWidget = bCanAddToViewport ? CreateWidget<UGameXXKTownHudWidget>(this, WidgetClass) : nullptr;
		if (!TownHudWidget)
		{
			TownHudWidget = NewObject<UGameXXKTownHudWidget>(this, WidgetClass);
		}
	}
	if (TownHudWidget)
	{
		TownHudWidget->SetMVPSubsystem(Subsystem);
		ConfigureFullscreenTaskPanelSlot(TownHudWidget);
		if (bCanAddToViewport && !TownHudWidget->IsInViewport())
		{
			TownHudWidget->AddToViewport(35);
			ConfigureFullscreenTaskPanelSlot(TownHudWidget);
		}
	}
	return TownHudWidget;
}

bool AGameXXKMVPPlayerController::ConfirmPendingQuestNpc(FName TaskId)
{
	// The current quest NPC owns the Qingshan main offer. Future NPC/task pairs
	// can extend this mapping without accidentally accepting an unrelated offer.
	if (TaskId != UGameXXKMVPRules::TaskQingshanMain())
	{
		return false;
	}

	APawn* InstigatorPawn = PendingQuestInstigator.Get();
	if (AGameXXKTownNpcCharacter* CharacterNpc = Cast<AGameXXKTownNpcCharacter>(PendingQuestNpc.Get()))
	{
		return CharacterNpc->ConfirmQuestDialogInteraction(InstigatorPawn);
	}
	if (AGameXXKTownNpcActor* ActorNpc = Cast<AGameXXKTownNpcActor>(PendingQuestNpc.Get()))
	{
		return ActorNpc->ConfirmQuestDialogInteraction(InstigatorPawn);
	}
	return false;
}

void AGameXXKMVPPlayerController::RefreshPlayerFlowWidgets()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const EGameXXKScreen ActiveScreen = Subsystem ? Subsystem->GetRuntimeState().Screen : EGameXXKScreen::MainMenu;
	if (InventoryWindowWidget
		&& ActiveScreen != EGameXXKScreen::Town
		&& (InventoryWindowWidget->IsWindowVisibleForTest()
			|| InventoryWindowWidget->GetWindowModeForTest() != EGameXXKInventoryWindowMode::None))
	{
		// A town inventory cannot survive into route/battle screens. Go through
		// the controller close path so it clears trade state and restores focus.
		CloseInventoryWindow();
	}
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
	if (TownHudWidget)
	{
		TownHudWidget->SetMVPSubsystem(Subsystem);
		TownHudWidget->RefreshFromState();
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
	if (InventoryWindowWidget)
	{
		InventoryWindowWidget->SetMVPSubsystem(Subsystem);
		if (Subsystem && Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Town && !InventoryWindowWidget->IsWindowVisibleForTest())
		{
			switch (Subsystem->GetRuntimeState().TownPanelMode)
			{
			case EGameXXKTownPanelMode::Inventory:
				InventoryWindowWidget->OpenFreeInventory();
				break;
			case EGameXXKTownPanelMode::Trade:
				InventoryWindowWidget->OpenMerchantTrade();
				break;
			default:
				break;
			}
		}
	}
	if (QuestDialogWidget)
	{
		QuestDialogWidget->SetMVPSubsystem(Subsystem);
	}
	if (TaskPanelWidget)
	{
		TaskPanelWidget->SetMVPSubsystem(Subsystem);
		TaskPanelWidget->RefreshFromState();
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
	else if (ActiveScreen == EGameXXKScreen::Town && QuestDialogWidget && QuestDialogWidget->IsDialogOpen())
	{
		InputMode.SetWidgetToFocus(QuestDialogWidget->TakeWidget());
	}
	else if (ActiveScreen == EGameXXKScreen::Town && TaskPanelWidget && TaskPanelWidget->IsTaskPanelOpenForTest())
	{
		InputMode.SetWidgetToFocus(TaskPanelWidget->TakeWidget());
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

	if (ActiveScreen == EGameXXKScreen::Town
		&& (!QuestDialogWidget || !QuestDialogWidget->IsDialogOpen())
		&& (!TaskPanelWidget || !TaskPanelWidget->IsTaskPanelOpenForTest())
		&& FSlateApplication::IsInitialized())
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
	FVector2D CursorWidgetPosition(CursorX, CursorY);
	double ScaledCursorX = 0.0;
	double ScaledCursorY = 0.0;
	if (UWidgetLayoutLibrary::GetMousePositionScaledByDPI(this, ScaledCursorX, ScaledCursorY))
	{
		CursorWidgetPosition = FVector2D(ScaledCursorX, ScaledCursorY);
	}
	FVector2D UnitWidgetPosition = CursorWidgetPosition;
	FVector UnitCommandWorldPosition = UnitActor->GetActorLocation();
	if (const UPaperFlipbookComponent* BattleVisual = UnitActor->GetBattleVisualComponent())
	{
		UnitCommandWorldPosition = BattleVisual->Bounds.Origin;
	}
	UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(this, UnitCommandWorldPosition, UnitWidgetPosition, true);
	return ToggleBattleCommandMenuForUnit(UnitActor, CursorWidgetPosition, UnitWidgetPosition);
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
	double ScaledCursorX = 0.0;
	double ScaledCursorY = 0.0;
	if (UWidgetLayoutLibrary::GetMousePositionScaledByDPI(this, ScaledCursorX, ScaledCursorY))
	{
		return UpdateBattleTargetingPointer(FVector2D(ScaledCursorX, ScaledCursorY));
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
