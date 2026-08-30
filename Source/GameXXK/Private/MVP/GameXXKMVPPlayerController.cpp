#include "MVP/GameXXKMVPPlayerController.h"

#include "Blueprint/GameViewportSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/InputComponent.h"
#include "Dialogue/GameXXKDialogueAsset.h"
#include "Dialogue/GameXXKDialogueCoordinator.h"
#include "Engine/Engine.h"
#include "Engine/GameEngine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "InputKeyEventArgs.h"
#include "Interaction/GameXXKInteractableComponent.h"
#include "Interaction/GameXXKInteractionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKLevelFlow.h"
#include "MVP/GameXXKRouteEncounterSceneActor.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Narrative/GameXXKNarrativeCoordinator.h"
#include "Narrative/GameXXKNarrativeSequenceAsset.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "Town/GameXXKHeroCharacter.h"
#include "Town/GameXXKTownNpcActor.h"
#include "Town/GameXXKTownNpcCharacter.h"
#include "Town/GameXXKTownPlayerPawn.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattleOverlayCoordinator.h"
#include "UI/GameXXKCompanionRosterWidget.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKDesktopHudSessionSubsystem.h"
#include "UI/GameXXKDialogueHistoryWidget.h"
#include "UI/GameXXKDialoguePanelWidget.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "UI/GameXXKMainMenuWidget.h"
#include "UI/GameXXKMetaShopWidget.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"
#include "UI/GameXXKQuestDialogWidget.h"
#include "UI/GameXXKRouteEncounterPanelWidget.h"
#include "UI/GameXXKRouteMerchantWidget.h"
#include "UI/GameXXKRelicBarWidget.h"
#include "UI/GameXXKSpeechBubbleWidget.h"
#include "UI/GameXXKTaskPanelWidget.h"
#include "UI/GameXXKTownHudWidget.h"
#include "UI/GameXXKTownOverlayWidget.h"
#include "UI/GameXXKWorldMapWidget.h"
#include "UObject/UObjectGlobals.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SWindow.h"

#if PLATFORM_WINDOWS
#include "IGameXXKDesktopOverlayModule.h"
#endif

namespace
{
	const FVector2D DefaultRouteMapViewportSize(1280.0f, 720.0f);

	class FGameXXKNarrativeUiCommandExecutor final : public IGameXXKNarrativeCommandExecutor
	{
	public:
		FGameXXKNarrativeUiCommandExecutor(
			const FName InCommandType,
			TFunction<bool()> InOpenAction,
			TFunction<void()> InCancelAction)
			: CommandType(InCommandType)
			, OpenAction(MoveTemp(InOpenAction))
			, CancelAction(MoveTemp(InCancelAction))
		{
		}

		virtual bool Supports(const FName InCommandType) const override
		{
			return InCommandType == CommandType;
		}

		virtual FGameXXKNarrativeCommandResult Execute(
			const FGameXXKNarrativeCommandDefinition& Command,
			FGameXXKRuntimeState& InOutCandidateState) override
		{
			(void)Command;
			(void)InOutCandidateState;
			FGameXXKNarrativeCommandResult Result;
			Result.Status = OpenAction && OpenAction()
				? EGameXXKNarrativeCommandStatus::Pending
				: EGameXXKNarrativeCommandStatus::Failed;
			if (Result.Status == EGameXXKNarrativeCommandStatus::Failed)
			{
				Result.Error = FString::Printf(TEXT("Failed to open narrative UI command: %s"), *CommandType.ToString());
			}
			return Result;
		}

		virtual void CancelPending() override
		{
			if (CancelAction) CancelAction();
		}

	private:
		FName CommandType;
		TFunction<bool()> OpenAction;
		TFunction<void()> CancelAction;
	};

	FString NarrativeAssetStem(const FName StableId)
	{
		FString Stem(TEXT("DA_"));
		for (const TCHAR Character : StableId.ToString())
		{
			Stem.AppendChar(FChar::IsAlnum(Character) ? Character : TEXT('_'));
		}
		return Stem;
	}

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

	bool IsGenericRouteEncounterScreen(const EGameXXKScreen Screen)
	{
		return Screen == EGameXXKScreen::RouteEvent
			|| Screen == EGameXXKScreen::RouteCamp;
	}

	bool IsSourceLessRouteEncounterPackageValid(const FString& CurrentPackageName, const EGameXXKScreen Screen)
	{
		return GameXXKLevelFlow::MapPackageMatches(CurrentPackageName, GameXXKLevelFlow::MapForScreen(Screen))
			|| GameXXKLevelFlow::IsDesktopTrainingHUDMapPackage(CurrentPackageName);
	}

	FString ResolveDesktopTrainingPerfProfile()
	{
		FString Profile;
		FParse::Value(FCommandLine::Get(), TEXT("GameXXKPerfProfile="), Profile);
		return Profile.ToLower();
	}
}

AGameXXKMVPPlayerController::AGameXXKMVPPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	MainMenuWidgetClass = UGameXXKMainMenuWidget::StaticClass();
	WorldMapWidgetClass = UGameXXKWorldMapWidget::StaticClass();
	TownOverlayWidgetClass = UGameXXKTownOverlayWidget::StaticClass();
	RouteMapWidgetClass = UGameXXKOneGameRouteMapWidget::StaticClass();
	BattleBoardWidgetClass = UGameXXKBattleBoardWidget::StaticClass();
	InventoryWindowWidgetClass = UGameXXKInventoryWindowWidget::StaticClass();
	MetaShopWidgetClass = UGameXXKMetaShopWidget::StaticClass();
	CompanionRosterWidgetClass = UGameXXKCompanionRosterWidget::StaticClass();
	QuestDialogWidgetClass = UGameXXKQuestDialogWidget::StaticClass();
	RouteEncounterPanelWidgetClass = UGameXXKRouteEncounterPanelWidget::StaticClass();
	RouteMerchantWidgetClass = UGameXXKRouteMerchantWidget::StaticClass();
	RelicBarWidgetClass = UGameXXKRelicBarWidget::StaticClass();
	TaskPanelWidgetClass = UGameXXKTaskPanelWidget::StaticClass();
	TownHudWidgetClass = UGameXXKTownHudWidget::StaticClass();
	DesktopTrainingWorkbenchWidgetClass = UGameXXKDesktopTrainingWorkbenchWidget::StaticClass();
}

void AGameXXKMVPPlayerController::BeginPlay()
{
	Super::BeginPlay();
	BindInteractionRequests(GetPawn());
	if (!PreLoadMapWithContextDelegateHandle.IsValid())
	{
		PreLoadMapWithContextDelegateHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(
			this,
			&AGameXXKMVPPlayerController::HandlePreLoadMapWithContext);
	}

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	SetTrackedInputMode(EGameXXKTrackedInputMode::GameAndUI);

	const FString DesktopTrainingPerfProfile = ResolveDesktopTrainingPerfProfile();
	const bool bPerfEmptyProfile = DesktopTrainingPerfProfile == TEXT("empty");
	const bool bIsDesktopTrainingHUDMap = ResolvePlayerFlowBootProfile()
		== EGameXXKPlayerFlowBootProfile::DesktopTrainingOnly;
	if (bIsDesktopTrainingHUDMap)
	{
		// The canonical 2D entry opts into the workbench automatically. The 3D
		// town path remains available only when it is loaded explicitly.
		bEnableDesktopTrainingWorkbench = !bPerfEmptyProfile;
		if (!bPerfEmptyProfile)
		{
			EnsureDesktopTrainingWidgets();
		}
	}
	else
	{
		// The 3D town presents the same Workbench directly in the game viewport.
		bEnableDesktopTrainingWorkbench = true;
		EnsurePlayerFlowWidgets();
	}

	RefreshPlayerFlowWidgets();
	if ((!bIsDesktopTrainingHUDMap || !bPerfEmptyProfile))
	{
		if (UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
			Subsystem && Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Town)
		{
			if (OpenDesktopTrainingWorkbench())
			{
				RestoreDesktopWorkbenchSessionAfterMapTravel();
			}
			if (bIsDesktopTrainingHUDMap)
			{
				ApplyDesktopTrainingPerfProfile(DesktopTrainingPerfProfile);
			}
		}
	}
}

void AGameXXKMVPPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindInteractionRequests();
	if (DialogueCoordinator)
	{
		DialogueCoordinator->PauseAndExit();
	}
	if (NarrativeCoordinator)
	{
		NarrativeCoordinator->PauseAndRelease();
	}
	SetNarrativeInputLocked(false);
	if (PreLoadMapWithContextDelegateHandle.IsValid())
	{
		FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadMapWithContextDelegateHandle);
		PreLoadMapWithContextDelegateHandle.Reset();
	}
	ExitBattleOverlay();
	SetDesktopWorkbenchTownPanelInputLock(false);
	if (DesktopTrainingWorkbenchWidget)
	{
		DesktopTrainingWorkbenchWidget->CloseWorkbench();
	}
	DestroyDesktopTrainingOverlayWindow();
	Super::EndPlay(EndPlayReason);
}

void AGameXXKMVPPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	BindInteractionRequests(InPawn);
}

void AGameXXKMVPPlayerController::OnUnPossess()
{
	UnbindInteractionRequests();
	Super::OnUnPossess();
}

void AGameXXKMVPPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (DialogueCoordinator && DialogueCoordinator->IsBlockingPresentation())
	{
		DialogueCoordinator->TickAuto(DeltaTime, nullptr);
		if (DialogueBubbleWidget)
		{
			DialogueBubbleWidget->UpdateAnchor(this);
		}
		RefreshNarrativeInputLock();
	}
	if (DesktopTrainingOverlayWindow.IsValid()
		&& bDesktopTrainingOverlayCompositionActive
		&& DesktopTrainingWorkbenchWidget
		&& DesktopTrainingWorkbenchWidget->IsWorkbenchVisibleForTest())
	{
		// The engine can show its primary game window once more after BeginPlay.
		// Keep that opaque renderer host hidden while the transparent desktop HUD owns presentation.
		SetDesktopTrainingGameViewportVisible(false);
	}
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
	if (HandleNarrativeInput(Params))
	{
		return true;
	}
	if (Params.Key == EKeys::Escape && Params.Event == IE_Pressed)
	{
		if (QuestDialogWidget && QuestDialogWidget->IsDialogOpen())
		{
			return CloseQuestDialog();
		}
		if (IsRouteEncounterPanelOpenForTest())
		{
			return CloseRouteEncounterPanel();
		}
		if (TaskPanelWidget && TaskPanelWidget->IsTaskPanelOpenForTest())
		{
			return CloseTaskPanel();
		}
		if (IsMetaShopOpenForTest())
		{
			return CloseMetaShopWindow();
		}
		if (InventoryWindowWidget && InventoryWindowWidget->IsWindowVisibleForTest())
		{
			return CloseInventoryWindow();
		}
		if (IsCompanionRosterOpenForTest())
		{
			return CloseCompanionRoster();
		}
		if (TownHudWidget && TownHudWidget->IsCompanionCodexOpenForTest())
		{
			return TownHudWidget->CloseCompanionCodex();
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
	if (IsRouteEncounterPanelOpenForTest()
		&& Params.Key == EKeys::F
		&& Params.Event == IE_Pressed)
	{
		// F only opens the paper panel. Visible choices are always mouse/touch
		// buttons, so a second F cannot silently resolve the encounter.
		return true;
	}
	if (IsRouteMerchantWidgetOpenForTest()
		&& Params.Key == EKeys::F
		&& Params.Event == IE_Pressed)
	{
		// The dedicated merchant HUD owns the route-merchant interaction. Do not
		// let legacy world interaction reopen the generic encounter panel.
		return true;
	}
	if (TaskPanelWidget && TaskPanelWidget->IsTaskPanelOpenForTest()
		&& (Params.Key == EKeys::F || Params.Key == EKeys::Q || Params.Key == EKeys::I))
	{
		return true;
	}
	if (Params.Key == EKeys::Tab && Params.Event == IE_Pressed && bEnableDesktopTrainingWorkbench)
	{
		UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
		if (Subsystem && Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Town)
		{
			if (OpenDesktopTrainingWorkbench())
			{
				DesktopTrainingWorkbenchWidget->HandleActionClicked(60);
				return true;
			}
		}
	}
	if (Params.Key == EKeys::Q && Params.Event == IE_Pressed)
	{
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
	if (Params.Key == EKeys::C && Params.Event == IE_Pressed)
	{
		UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
		if (Subsystem && Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Town)
		{
			const bool bHandled = IsCompanionRosterOpenForTest() ? CloseCompanionRoster() : OpenCompanionRoster();
			if (bHandled)
			{
				RefreshPlayerFlowWidgets();
				return true;
			}
		}
	}
	if (Params.Key == EKeys::LeftMouseButton && Params.Event == IE_Released)
	{
		HandleRouteMapPrimaryClick();
	}
	if (Params.Key == EKeys::F && Params.Event == IE_Pressed)
	{
		const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
		if (Subsystem && IsGenericRouteEncounterScreen(Subsystem->GetRuntimeState().Screen))
		{
			const bool bOpened = TryHandleRouteEncounterInteract();
			// In a live route world this controller owns F so an unfocused input
			// cannot fall through to the pawn component's nearby-actor fallback.
			// Headless automation still receives false and must use its explicit
			// panel seam instead.
			return bOpened || CanAddPlayerWidgetsToViewport();
		}
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

#if WITH_DEV_AUTOMATION_TESTS
void AGameXXKMVPPlayerController::SetDesktopTrainingBootProfileForTest(const bool bEnabled)
{
	OverrideBootProfileForTest = bEnabled
		? EGameXXKPlayerFlowBootProfile::DesktopTrainingOnly
		: EGameXXKPlayerFlowBootProfile::FullPlayerFlow;
	bEnableDesktopTrainingWorkbench = bEnabled;
}

FString AGameXXKMVPPlayerController::GetDesktopTrainingPerfProfileForTest() const
{
	return ResolveDesktopTrainingPerfProfile();
}

bool AGameXXKMVPPlayerController::EnsureDesktopTrainingWidgetsForTest()
{
	return EnsureDesktopTrainingWidgets();
}

bool AGameXXKMVPPlayerController::ApplyDesktopTrainingPerfProfileForTest(const FString& Profile)
{
	return ApplyDesktopTrainingPerfProfile(Profile);
}

bool AGameXXKMVPPlayerController::ShouldUseDesktopWindowForMapNameForTest(
	const FString& MapPackageName)
{
	return ShouldUseDesktopWindowForMapName(MapPackageName);
}

bool AGameXXKMVPPlayerController::ShouldBeginDesktopTownMapTravelForTest(
	const bool bAlreadyPending,
	const FName TargetMap)
{
	return !bAlreadyPending && !TargetMap.IsNone();
}
#endif

void AGameXXKMVPPlayerController::SetDesktopTrainingWorkbenchEnabledForTest(const bool bEnabled)
{
	bEnableDesktopTrainingWorkbench = bEnabled;
	if (bEnabled)
	{
		OpenDesktopTrainingWorkbench();
	}
	else
	{
		CloseDesktopTrainingWorkbench();
	}
}

UGameXXKDesktopTrainingWorkbenchWidget* AGameXXKMVPPlayerController::GetDesktopTrainingWorkbenchWidgetForTest() const
{
	return DesktopTrainingWorkbenchWidget;
}

TArray<UGameXXKDesktopTrainingWorkbenchWidget*> AGameXXKMVPPlayerController::GetAllDesktopTrainingWorkbenchWidgetsForTest() const
{
	TArray<UGameXXKDesktopTrainingWorkbenchWidget*> Result;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return Result;
	}
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UGameXXKDesktopTrainingWorkbenchWidget::StaticClass(), false);
	for (UUserWidget* Widget : Widgets)
	{
		if (UGameXXKDesktopTrainingWorkbenchWidget* Workbench = Cast<UGameXXKDesktopTrainingWorkbenchWidget>(Widget))
		{
			Result.Add(Workbench);
		}
	}
	return Result;
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

void AGameXXKMVPPlayerController::EnterBattleOverlay()
{
	if (!RouteMapWidget || !BattleBoardWidget)
	{
		return;
	}
	if (!BattleOverlayCoordinator)
	{
		BattleOverlayCoordinator = NewObject<UGameXXKBattleOverlayCoordinator>(this);
	}
	if (BattleOverlayCoordinator && !BattleOverlayCoordinator->IsActive())
	{
		const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
		UE_LOG(LogTemp, Warning, TEXT("[BattleOverlay] Enter screen=%d board=%s"),
			Subsystem ? static_cast<int32>(Subsystem->GetRuntimeState().Screen) : -1, *BattleBoardWidget->GetName());
		BattleOverlayCoordinator->Enter(*this, *RouteMapWidget, *BattleBoardWidget);
	}
}

void AGameXXKMVPPlayerController::ExitBattleOverlay()
{
	if (BattleOverlayCoordinator)
	{
		const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
		UE_LOG(LogTemp, Warning, TEXT("[BattleOverlay] Exit screen=%d active=%d"),
			Subsystem ? static_cast<int32>(Subsystem->GetRuntimeState().Screen) : -1,
			BattleOverlayCoordinator->IsActive() ? 1 : 0);
		BattleOverlayCoordinator->Exit(*this);
	}
}

bool AGameXXKMVPPlayerController::IsBattleOverlayActive() const
{
	return BattleOverlayCoordinator && BattleOverlayCoordinator->IsActive();
}

FGameXXKBattleOverlaySnapshot AGameXXKMVPPlayerController::CaptureBattleOverlaySnapshot(
	const UGameXXKOneGameRouteMapWidget& RouteWidget) const
{
	FGameXXKBattleOverlaySnapshot Snapshot;
	if (GetWorld())
	{
		Snapshot.bGamePaused = UGameplayStatics::IsGamePaused(this);
		Snapshot.bWorldRenderingEnabled = UGameplayStatics::GetEnableWorldRendering(this);
	}
	Snapshot.bShowMouseCursor = bShowMouseCursor;
	Snapshot.bEnableClickEvents = bEnableClickEvents;
	Snapshot.bEnableMouseOverEvents = bEnableMouseOverEvents;
	Snapshot.bMoveInputIgnored = IsMoveInputIgnored();
	Snapshot.bLookInputIgnored = IsLookInputIgnored();
	Snapshot.InputMode = TrackedInputMode;
	Snapshot.RouteVisibility = RouteWidget.GetVisibility();
	Snapshot.RouteScrollOffset = RouteWidget.GetCurrentScrollOffset();
	return Snapshot;
}

bool AGameXXKMVPPlayerController::ApplyBattleOverlayEntry(
	UGameXXKOneGameRouteMapWidget& RouteWidget,
	UGameXXKBattleBoardWidget& BattleWidget,
	const uint64 SessionToken)
{
	if (!BattleOverlayCoordinator || !BattleOverlayCoordinator->IsCurrentSession(SessionToken))
	{
		return false;
	}

	FlushPressedKeys();
	bBattleOverlayAcquiredFullTickWhenPaused = !bShouldPerformFullTickWhenPaused;
	if (bBattleOverlayAcquiredFullTickWhenPaused)
	{
		bShouldPerformFullTickWhenPaused = true;
	}
	bBattleOverlayAcquiredMoveInputIgnore = !IsMoveInputIgnored();
	if (bBattleOverlayAcquiredMoveInputIgnore)
	{
		SetIgnoreMoveInput(true);
	}
	bBattleOverlayAcquiredLookInputIgnore = !IsLookInputIgnored();
	if (bBattleOverlayAcquiredLookInputIgnore)
	{
		SetIgnoreLookInput(true);
	}
	if (GetWorld())
	{
		UGameplayStatics::SetGamePaused(this, true);
		UGameplayStatics::SetEnableWorldRendering(this, false);
	}
	RouteWidget.SetVisibility(ESlateVisibility::Collapsed);
	BattleWidget.SetVisibility(ESlateVisibility::Visible);
	BattleWidget.SetIsFocusable(true);
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	SetTrackedInputMode(EGameXXKTrackedInputMode::UIOnly, &BattleWidget);
	// Focus acquisition calls TakeWidget() and may perform the first Slate rebuild.
	// Start the visual session only after that rebuild so formation children are
	// added to a live design-stage canvas, never to a half-built constraint slot.
	if (!BattleWidget.BeginBattleVisualSession(SessionToken))
	{
		return false;
	}
	return BattleOverlayCoordinator->IsCurrentSession(SessionToken);
}

void AGameXXKMVPPlayerController::CancelBattleVisualLoads(const uint64 ClosingSessionToken)
{
	if (BattleBoardWidget)
	{
		BattleBoardWidget->CancelBattleVisualSession(ClosingSessionToken);
	}
}

void AGameXXKMVPPlayerController::RestoreBattleOverlaySnapshot(
	const FGameXXKBattleOverlaySnapshot& Snapshot,
	UGameXXKOneGameRouteMapWidget* RouteWidget,
	UGameXXKBattleBoardWidget* BattleWidget)
{
	if (BattleWidget)
	{
		BattleWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (RouteWidget)
	{
		RouteWidget->SetVisibility(Snapshot.RouteVisibility);
		RouteWidget->RestoreScrollOffset(Snapshot.RouteScrollOffset);
	}
	if (GetWorld())
	{
		UGameplayStatics::SetEnableWorldRendering(this, Snapshot.bWorldRenderingEnabled);
		UGameplayStatics::SetGamePaused(this, Snapshot.bGamePaused);
	}
	if (bBattleOverlayAcquiredFullTickWhenPaused)
	{
		bShouldPerformFullTickWhenPaused = false;
		bBattleOverlayAcquiredFullTickWhenPaused = false;
	}
	if (bBattleOverlayAcquiredMoveInputIgnore)
	{
		SetIgnoreMoveInput(false);
		bBattleOverlayAcquiredMoveInputIgnore = false;
	}
	if (bBattleOverlayAcquiredLookInputIgnore)
	{
		SetIgnoreLookInput(false);
		bBattleOverlayAcquiredLookInputIgnore = false;
	}
	bShowMouseCursor = Snapshot.bShowMouseCursor;
	bEnableClickEvents = Snapshot.bEnableClickEvents;
	bEnableMouseOverEvents = Snapshot.bEnableMouseOverEvents;
	SetTrackedInputMode(Snapshot.InputMode, RouteWidget);
}

UGameXXKMainMenuWidget* AGameXXKMVPPlayerController::GetMainMenuWidgetForTest() const
{
	return MainMenuWidget;
}

UGameXXKWorldMapWidget* AGameXXKMVPPlayerController::GetWorldMapWidgetForTest() const
{
	return WorldMapWidget;
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

UGameXXKBattleBoardWidget* AGameXXKMVPPlayerController::GetOrCreateBattleBoardWidget()
{
	return EnsureBattleBoardWidget();
}

UGameXXKInventoryWindowWidget* AGameXXKMVPPlayerController::GetInventoryWindowWidgetForTest() const
{
	return InventoryWindowWidget;
}

UGameXXKMetaShopWidget* AGameXXKMVPPlayerController::GetMetaShopWidgetForTest() const
{
	return MetaShopWidget;
}

UGameXXKCompanionRosterWidget* AGameXXKMVPPlayerController::GetCompanionRosterWidgetForTest() const
{
	return CompanionRosterWidget;
}

UGameXXKQuestDialogWidget* AGameXXKMVPPlayerController::GetQuestDialogWidgetForTest() const
{
	return QuestDialogWidget;
}

UGameXXKTaskPanelWidget* AGameXXKMVPPlayerController::GetTaskPanelWidgetForTest() const
{
	return TaskPanelWidget;
}

UGameXXKRouteEncounterPanelWidget* AGameXXKMVPPlayerController::GetRouteEncounterPanelWidgetForTest() const
{
	return RouteEncounterPanelWidget;
}

UGameXXKRouteMerchantWidget* AGameXXKMVPPlayerController::GetRouteMerchantWidgetForTest() const
{
	return RouteMerchantWidget;
}

bool AGameXXKMVPPlayerController::IsRouteMerchantWidgetOpenForTest() const
{
	return RouteMerchantWidget
		&& RouteMerchantWidget->GetVisibility() != ESlateVisibility::Collapsed
		&& RouteMerchantWidget->GetVisibility() != ESlateVisibility::Hidden;
}

bool AGameXXKMVPPlayerController::IsRouteMerchantInputLockedForTest() const
{
	return bRouteMerchantInputLocked;
}

UGameXXKTownHudWidget* AGameXXKMVPPlayerController::GetTownHudWidgetForTest() const
{
	return TownHudWidget;
}

bool AGameXXKMVPPlayerController::HasMainMenuWidgetInViewportForTest() const
{
	return MainMenuWidget && MainMenuWidget->IsInViewport();
}

bool AGameXXKMVPPlayerController::HasWorldMapWidgetInViewportForTest() const
{
	return WorldMapWidget && WorldMapWidget->IsInViewport();
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

bool AGameXXKMVPPlayerController::IsMetaShopOpenForTest() const
{
	return MetaShopWidget && MetaShopWidget->GetVisibility() != ESlateVisibility::Collapsed;
}

bool AGameXXKMVPPlayerController::IsMetaShopInputLockedForTest() const
{
	return bMetaShopInputLocked;
}

bool AGameXXKMVPPlayerController::IsInventoryWindowModalInputLocked() const
{
	return InventoryWindowWidget && InventoryWindowWidget->IsModalInputLockActiveForTest();
}

bool AGameXXKMVPPlayerController::IsCompanionRosterOpenForTest() const
{
	return CompanionRosterWidget && CompanionRosterWidget->GetVisibility() != ESlateVisibility::Collapsed;
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
		|| (IsNarrativeGameplayUiBlocked() && !bOpeningNarrativeUiCommand)
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
	CloseMetaShopWindow();

	PendingQuestNpc = QuestNpc;
	PendingQuestInstigator = InstigatorPawn;
	Dialog->OpenDialog();
	FlushPressedKeys();
	SetIgnoreMoveInput(true);
	ApplyPlayerFlowInputMode();
	return true;
}

bool AGameXXKMVPPlayerController::OpenTownNpcInteractionForNpc(AActor* TownNpc, APawn* InstigatorPawn)
{
	if (!TownNpc || !InstigatorPawn)
	{
		return false;
	}
	const UGameXXKInteractableComponent* Metadata =
		TownNpc->FindComponentByClass<UGameXXKInteractableComponent>();
	if (!Metadata || !Metadata->IsInteractionEnabled())
	{
		return false;
	}
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}
	UGameXXKNarrativeSequenceAsset* Sequence =
		ResolveNarrativeSequenceAsset(Metadata->GetNarrativeSequenceId());
	if (!Sequence)
	{
		return false;
	}
	const FGameXXKNarrativeSequenceSessionState& ExistingSession =
		Subsystem->GetRuntimeState().NarrativeSequenceSession;
	if (ExistingSession.bActive)
	{
		if (!CanStartNpcNarrativeInteraction(true)
			|| ExistingSession.SequenceId != Sequence->SequenceId
			|| (ActiveNarrativeInteractionActor.IsValid()
				&& ActiveNarrativeInteractionActor.Get() != TownNpc)
			|| (DialogueCoordinator && DialogueCoordinator->IsBlockingPresentation())
			|| !EnsureNarrativeInteractionRuntime())
		{
			return false;
		}
		if (NarrativeCoordinator->IsInputTokenHeld())
		{
			return false;
		}
		ActiveNarrativeInteractionActor = TownNpc;
		const bool bResumed = NarrativeCoordinator->ResumeSequence(*Sequence, nullptr);
		if (bResumed)
		{
			FlushPressedKeys();
			RefreshNarrativeInputLock();
			ApplyPlayerFlowInputMode();
		}
		return bResumed;
	}
	if (!CanStartNpcNarrativeInteraction() || !EnsureNarrativeInteractionRuntime())
	{
		return false;
	}
	FGameXXKNarrativeStartContext Context;
	Context.StoryId = TEXT("Story.NpcInteraction");
	Context.TaskId = Metadata->GetInteractionId();
	Context.StepId = TEXT("Step.Interact");
	Context.StageContractId = Sequence->StageContractId;
	Context.CharacterIdByRole.Add(TEXT("Npc"), Metadata->GetInteractionId());
	ActiveNarrativeInteractionActor = TownNpc;
	FString Error;
	const bool bStarted = NarrativeCoordinator->StartSequence(*Sequence, Context, &Error);
	if (bStarted)
	{
		FlushPressedKeys();
		RefreshNarrativeInputLock();
		ApplyPlayerFlowInputMode();
	}
	else
	{
		ActiveNarrativeInteractionActor.Reset();
	}
	return bStarted;
}

bool AGameXXKMVPPlayerController::OpenTaskOfferPanelForNpc(AActor* QuestNpc, APawn* InstigatorPawn)
{
	// A second modal request is rejected before it can replace the original
	// pending NPC/instigator pair.
	if (!QuestNpc || !InstigatorPawn
		|| (IsNarrativeGameplayUiBlocked() && !bOpeningNarrativeUiCommand)
		|| (QuestDialogWidget && QuestDialogWidget->IsDialogOpen())
		|| (TaskPanelWidget && TaskPanelWidget->IsTaskPanelOpenForTest()))
	{
		return false;
	}

	CloseMetaShopWindow();
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
	if (IsNarrativeGameplayUiBlocked() && !bOpeningNarrativeUiCommand)
	{
		return false;
	}
	CloseMetaShopWindow();
	CloseCompanionRoster();
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
	return false;
}

bool AGameXXKMVPPlayerController::OpenMetaShopWindow()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Town)
	{
		return false;
	}
	if (IsMetaShopOpenForTest())
	{
		return CloseMetaShopWindow();
	}
	if (IsNarrativeGameplayUiBlocked() && !bOpeningNarrativeUiCommand)
	{
		return false;
	}
	if (QuestDialogWidget && QuestDialogWidget->IsDialogOpen())
	{
		return false;
	}

	CloseInventoryWindow();
	CloseCompanionRoster();
	CloseTaskPanel();
	UGameXXKMetaShopWidget* Shop = EnsureMetaShopWidget();
	if (!Shop)
	{
		return false;
	}
	Subsystem->CloseTownPanel();
	Shop->SetMVPSubsystem(Subsystem);
	if (!Shop->OpenMetaShop())
	{
		return false;
	}

	FlushPressedKeys();
	SetIgnoreMoveInput(true);
	bMetaShopInputLocked = true;
	ApplyPlayerFlowInputMode();
	return true;
}

bool AGameXXKMVPPlayerController::CloseMetaShopWindow()
{
	const bool bWasOpen = IsMetaShopOpenForTest();
	if (!bWasOpen && !bMetaShopInputLocked)
	{
		return false;
	}
	if (bWasOpen)
	{
		MetaShopWidget->CloseMetaShop();
	}
	else
	{
		HandleMetaShopClosed();
	}
	return true;
}

void AGameXXKMVPPlayerController::HandleMetaShopClosed()
{
	if (bMetaShopInputLocked)
	{
		SetIgnoreMoveInput(false);
		bMetaShopInputLocked = false;
	}
	if (!bCancellingNarrativeUiCommand && NarrativeCoordinator)
	{
		NarrativeCoordinator->CompletePendingCommand(EGameXXKNarrativeCommandStatus::Completed, nullptr);
		RefreshNarrativeInputLock();
	}
	ApplyPlayerFlowInputMode();
}

void AGameXXKMVPPlayerController::HandleMetaShopCompanionReplacementRequested()
{
	CloseMetaShopWindow();
	OpenCompanionRoster();
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

bool AGameXXKMVPPlayerController::OpenCompanionRoster()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Town)
	{
		return false;
	}
	if (IsNarrativeGameplayUiBlocked() && !bOpeningNarrativeUiCommand)
	{
		return false;
	}
	if (QuestDialogWidget && QuestDialogWidget->IsDialogOpen())
	{
		return false;
	}

	CloseMetaShopWindow();
	CloseInventoryWindow();
	CloseTaskPanel();
	if (TownHudWidget)
	{
		TownHudWidget->CloseCompanionCodex();
	}
	UGameXXKCompanionRosterWidget* Roster = EnsureCompanionRosterWidget();
	if (!Roster)
	{
		return false;
	}
	Roster->SetMVPSubsystem(Subsystem);
	Roster->RefreshFromState();
	Roster->SetVisibility(ESlateVisibility::Visible);
	FlushPressedKeys();
	SetIgnoreMoveInput(true);
	ApplyPlayerFlowInputMode();
	return true;
}

bool AGameXXKMVPPlayerController::CloseCompanionRoster()
{
	if (!IsCompanionRosterOpenForTest())
	{
		return false;
	}
	CompanionRosterWidget->SetVisibility(ESlateVisibility::Collapsed);
	SetIgnoreMoveInput(false);
	ApplyPlayerFlowInputMode();
	return true;
}

bool AGameXXKMVPPlayerController::OpenTaskPanel()
{
	if (IsNarrativeGameplayUiBlocked() && !bOpeningNarrativeUiCommand)
	{
		return false;
	}
	if (QuestDialogWidget && QuestDialogWidget->IsDialogOpen())
	{
		return false;
	}
	CloseMetaShopWindow();
	CloseCompanionRoster();
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
	const bool bWasOpen = IsTaskPanelOpenForTest();
	const bool bClosed = TaskPanelWidget && TaskPanelWidget->CloseTaskPanel();
	if (bClosed && bWasOpen)
	{
		PendingQuestNpc.Reset();
		PendingQuestInstigator.Reset();
		SetIgnoreMoveInput(false);
		if (!bCancellingNarrativeUiCommand && NarrativeCoordinator)
		{
			NarrativeCoordinator->CompletePendingCommand(EGameXXKNarrativeCommandStatus::Completed, nullptr);
			RefreshNarrativeInputLock();
		}
		ApplyPlayerFlowInputMode();
	}
	return bClosed;
}

bool AGameXXKMVPPlayerController::IsTaskPanelOpenForTest() const
{
	return TaskPanelWidget && TaskPanelWidget->IsTaskPanelOpenForTest();
}

bool AGameXXKMVPPlayerController::OpenRouteEncounterPanel()
{
	if (CanAddPlayerWidgetsToViewport())
	{
		return false;
	}
	return OpenRouteEncounterPanelInternal(nullptr);
}

bool AGameXXKMVPPlayerController::OpenRouteEncounterPanelFromActor(AGameXXKRouteEncounterSceneActor* SourceActor)
{
	if (!SourceActor || SourceActor != GetFocusedRouteEncounterActor())
	{
		return false;
	}
	return OpenRouteEncounterPanelInternal(SourceActor);
}

bool AGameXXKMVPPlayerController::OpenRouteEncounterPanelInternal(AGameXXKRouteEncounterSceneActor* SourceActor)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !IsGenericRouteEncounterScreen(Subsystem->GetRuntimeState().Screen))
	{
		return false;
	}
	if (!SourceActor && CanAddPlayerWidgetsToViewport())
	{
		const UWorld* World = GetWorld();
		const FString CurrentPackageName = World && World->GetOutermost()
			? World->GetOutermost()->GetName()
			: FString();
		if (!IsSourceLessRouteEncounterPackageValid(CurrentPackageName, Subsystem->GetRuntimeState().Screen))
		{
			return false;
		}
	}
	if (IsRouteEncounterPanelOpenForTest())
	{
		if (ActiveRouteEncounterSourceActor.Get() == SourceActor && HasValidRouteEncounterContext())
		{
			return true;
		}
		ForceCloseRouteEncounterPanelLocal();
	}

	UGameXXKRouteEncounterPanelWidget* Panel = EnsureRouteEncounterPanelWidget();
	const bool bOpened = Panel && Panel->OpenEncounterPanel();
	if (bOpened)
	{
		ActiveRouteEncounterSourceActor = SourceActor;
		ActiveRouteEncounterScreen = Subsystem->GetRuntimeState().Screen;
		ActiveRouteEncounterNodeId = Subsystem->GetRuntimeState().PendingRouteNodeId;
		FlushPressedKeys();
		SetIgnoreMoveInput(true);
		ApplyPlayerFlowInputMode();
	}
	return bOpened;
}

bool AGameXXKMVPPlayerController::CloseRouteEncounterPanel()
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (Subsystem
		&& IsRouteEncounterPanelOpenForTest()
		&& HasValidRouteEncounterContext()
		&& !ActiveRouteEncounterSourceActor.IsValid()
		&& (Subsystem->GetRuntimeState().Screen == EGameXXKScreen::RouteEvent
			|| Subsystem->GetRuntimeState().Screen == EGameXXKScreen::RouteCamp))
	{
		return ReturnPendingRouteChoiceToMap();
	}

	return ForceCloseRouteEncounterPanelLocal();
}

void AGameXXKMVPPlayerController::SetDesktopWorkbenchTownPanelInputLock(
	const bool bLocked)
{
	if (bLocked)
	{
		if (!bOwnsDesktopWorkbenchTownMoveInputLock)
		{
			SetIgnoreMoveInput(true);
			bOwnsDesktopWorkbenchTownMoveInputLock = true;
		}
		if (!bOwnsDesktopWorkbenchTownLookInputLock)
		{
			SetIgnoreLookInput(true);
			bOwnsDesktopWorkbenchTownLookInputLock = true;
		}
		return;
	}
	if (bOwnsDesktopWorkbenchTownMoveInputLock)
	{
		SetIgnoreMoveInput(false);
		bOwnsDesktopWorkbenchTownMoveInputLock = false;
	}
	if (bOwnsDesktopWorkbenchTownLookInputLock)
	{
		SetIgnoreLookInput(false);
		bOwnsDesktopWorkbenchTownLookInputLock = false;
	}
}

bool AGameXXKMVPPlayerController::ReturnPendingRouteChoiceToMap()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || (IsRouteEncounterPanelOpenForTest() && !HasValidRouteEncounterContext()))
	{
		ForceCloseRouteEncounterPanelLocal();
		return false;
	}
	if (!Subsystem->ReturnPendingRouteChoiceToMap())
	{
		return false;
	}
	ForceCloseRouteEncounterPanelLocal();
	RefreshPlayerFlowWidgets();
	return true;
}

bool AGameXXKMVPPlayerController::TriggerRouteEncounterEscapeForTest()
{
	return InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Escape, IE_Pressed, 1.0f));
}

bool AGameXXKMVPPlayerController::IsRouteEncounterPanelOpenForTest() const
{
	return RouteEncounterPanelWidget && RouteEncounterPanelWidget->IsEncounterPanelOpenForTest();
}

AGameXXKRouteEncounterSceneActor* AGameXXKMVPPlayerController::GetRouteEncounterSourceActorForTest() const
{
	return ActiveRouteEncounterSourceActor.Get();
}

bool AGameXXKMVPPlayerController::ResolveRouteEncounterAction(const EGameXXKRouteEncounterAction Action)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !IsRouteEncounterPanelOpenForTest())
	{
		return false;
	}
	if (!HasValidRouteEncounterContext())
	{
		ForceCloseRouteEncounterPanelLocal();
		return false;
	}

	if (Action == EGameXXKRouteEncounterAction::ClosePanel)
	{
		return ReturnPendingRouteChoiceToMap();
	}

	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	if (!IsGenericRouteEncounterScreen(State.Screen))
	{
		return false;
	}
	// The panel has one immutable source context.  Preserve it before the route
	// rule changes screen/state, then notify only that exact actor after a real
	// visible choice succeeds.
	AGameXXKRouteEncounterSceneActor* EncounterActor = ActiveRouteEncounterSourceActor.Get();

	bool bResolved = false;
	switch (Action)
	{
	case EGameXXKRouteEncounterAction::SelectChoice0:
		bResolved = State.Screen == EGameXXKScreen::RouteEvent && Subsystem->ResolveRouteEncounterChoice(0);
		break;
	case EGameXXKRouteEncounterAction::SelectChoice1:
		bResolved = State.Screen == EGameXXKScreen::RouteEvent && Subsystem->ResolveRouteEncounterChoice(1);
		break;
	case EGameXXKRouteEncounterAction::SelectChoice2:
		bResolved = State.Screen == EGameXXKScreen::RouteEvent && Subsystem->ResolveRouteEncounterChoice(2);
		break;
	case EGameXXKRouteEncounterAction::AcceptTaskNpcSupport:
		bResolved = false;
		break;
	case EGameXXKRouteEncounterAction::TakeGold:
		bResolved = State.Screen == EGameXXKScreen::RouteEvent
			&& Subsystem->ResolveEventReward(true);
		break;
	case EGameXXKRouteEncounterAction::TakeHealingPowder:
		bResolved = State.Screen == EGameXXKScreen::RouteEvent
			&& Subsystem->ResolveEventReward(false);
		break;
	case EGameXXKRouteEncounterAction::CampTakeLifeSavingTalisman:
	case EGameXXKRouteEncounterAction::CampRest: // Legacy action keeps the new true=charm mapping.
		bResolved = State.Screen == EGameXXKScreen::RouteCamp
			&& Subsystem->ResolveCampReward(true);
		break;
	case EGameXXKRouteEncounterAction::CampTakeRouteMoney:
	case EGameXXKRouteEncounterAction::CampTakeHealingPowder: // Legacy action keeps the new false=money mapping.
		bResolved = State.Screen == EGameXXKScreen::RouteCamp
			&& Subsystem->ResolveCampReward(false);
		break;
	default:
		break;
	}

	if (!bResolved)
	{
		if (RouteEncounterPanelWidget)
		{
			RouteEncounterPanelWidget->RefreshFromState();
		}
		return false;
	}

	ForceCloseRouteEncounterPanelLocal();
	if (EncounterActor)
	{
		EncounterActor->RecordExplicitChoiceResolved(GetPawn());
	}
	const UWorld* const World = GetWorld();
	const FString CurrentPackageName = World && World->GetOutermost()
		? World->GetOutermost()->GetName()
		: FString();
	PrepareForRuntimeStateMapTravel(CurrentPackageName);
	GameXXKLevelFlow::OpenMapForRuntimeState(Subsystem);
	RefreshPlayerFlowWidgets();
	return true;
}

bool AGameXXKMVPPlayerController::ConfirmBattleTargetForUnitId(const FName UnitId)
{
	if (UnitId.IsNone())
	{
		return false;
	}
	EnsureBattleBoardWidget();
	if (!BattleBoardWidget || !BattleBoardWidget->IsCardTargetingActive())
	{
		return false;
	}
	return BattleBoardWidget->ConfirmTargetingUnit(UnitId);
}

bool AGameXXKMVPPlayerController::CancelBattleTargetingForTest()
{
	EnsureBattleBoardWidget();
	return BattleBoardWidget && BattleBoardWidget->CancelBattleTargeting();
}

bool AGameXXKMVPPlayerController::UpdateBattleTargetingPointerForTest(FVector2D CursorScreenPosition)
{
	EnsureBattleBoardWidget();
	return UpdateBattleTargetingPointer(CursorScreenPosition);
}

#if WITH_DEV_AUTOMATION_TESTS
void AGameXXKMVPPlayerController::SetTrackedInputModeForTest(const EGameXXKTrackedInputMode InputMode)
{
	SetTrackedInputMode(InputMode);
}

bool AGameXXKMVPPlayerController::PrepareForRuntimeStateMapTravelForTest(const FString& CurrentPackageName)
{
	return PrepareForRuntimeStateMapTravel(CurrentPackageName);
}

bool AGameXXKMVPPlayerController::CanAddPlayerWidgetsToViewportForTest() const
{
	return CanAddPlayerWidgetsToViewport();
}

TSharedRef<SWindow> AGameXXKMVPPlayerController::BuildDesktopTrainingOverlayWindowForTest(
	const FVector2D& HostPosition,
	const FVector2D& HostSize)
{
	return BuildDesktopTrainingOverlayWindow(
		HostPosition,
		HostSize,
		SNullWidget::NullWidget,
		true);
}

bool AGameXXKMVPPlayerController::ShouldHideDesktopTrainingGameViewportForTest(
	const bool bEditorMode,
	const bool bGameCommandLine)
{
	return ShouldHideDesktopTrainingGameViewport(bEditorMode, bGameCommandLine);
}

bool AGameXXKMVPPlayerController::ShouldHideViewportAfterOverlayAttachForTest(
	const bool bOverlayRequested,
	const bool bOverlayAttached)
{
	return bOverlayRequested && bOverlayAttached;
}

bool AGameXXKMVPPlayerController::ShouldAttemptDesktopOverlayAfterFailureForTest(
	const bool bOverlayFailedForSession)
{
	return !bOverlayFailedForSession;
}

bool AGameXXKMVPPlayerController::IsSourceLessRouteEncounterPackageValidForTest(const FString& CurrentPackageName) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem
		&& IsSourceLessRouteEncounterPackageValid(CurrentPackageName, Subsystem->GetRuntimeState().Screen);
}

void AGameXXKMVPPlayerController::SetBattleMousePositionOverrideForTest(const FVector2D InMousePosition)
{
	bUseBattleMousePositionOverrideForTest = true;
	BattleMousePositionOverrideForTest = InMousePosition;
}

void AGameXXKMVPPlayerController::ClearBattleMousePositionOverrideForTest()
{
	bUseBattleMousePositionOverrideForTest = false;
	BattleMousePositionOverrideForTest = FVector2D::ZeroVector;
}

void AGameXXKMVPPlayerController::SetShouldPerformFullTickWhenPausedForTest(const bool bEnabled)
{
	bShouldPerformFullTickWhenPaused = bEnabled;
}
#endif

UGameXXKMVPSubsystem* AGameXXKMVPPlayerController::ResolveMVPSubsystem() const
{
	if (OverrideSubsystem)
	{
		return OverrideSubsystem;
	}

	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UGameXXKMVPSubsystem>() : nullptr;
}

bool AGameXXKMVPPlayerController::EnsureNarrativeInteractionRuntime()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}
	if (!NarrativeCoordinator)
	{
		NarrativeCoordinator = NewObject<UGameXXKNarrativeCoordinator>(this, TEXT("NarrativeCoordinator"));
		const TWeakObjectPtr<AGameXXKMVPPlayerController> WeakController(this);
		NarrativeCoordinator->RegisterExecutor(
			TEXT("openShop"),
			MakeShared<FGameXXKNarrativeUiCommandExecutor>(
				TEXT("openShop"),
				[WeakController]()
				{
					AGameXXKMVPPlayerController* Controller = WeakController.Get();
					if (!Controller) return false;
					Controller->bOpeningNarrativeUiCommand = true;
					const bool bOpened = Controller->OpenMetaShopWindow();
					Controller->bOpeningNarrativeUiCommand = false;
					return bOpened;
				},
				[WeakController]()
				{
					if (AGameXXKMVPPlayerController* Controller = WeakController.Get())
					{
						Controller->bCancellingNarrativeUiCommand = true;
						Controller->CloseMetaShopWindow();
						Controller->bCancellingNarrativeUiCommand = false;
					}
				}));
		NarrativeCoordinator->RegisterExecutor(
			TEXT("openTaskOffer"),
			MakeShared<FGameXXKNarrativeUiCommandExecutor>(
				TEXT("openTaskOffer"),
				[WeakController]()
				{
					AGameXXKMVPPlayerController* Controller = WeakController.Get();
					if (!Controller) return false;
					Controller->bOpeningNarrativeUiCommand = true;
					const bool bOpened = Controller->OpenTaskOfferPanelForNpc(
						Controller->ActiveNarrativeInteractionActor.Get(),
						Controller->GetPawn());
					Controller->bOpeningNarrativeUiCommand = false;
					return bOpened;
				},
				[WeakController]()
				{
					if (AGameXXKMVPPlayerController* Controller = WeakController.Get())
					{
						Controller->bCancellingNarrativeUiCommand = true;
						Controller->CloseTaskPanel();
						Controller->bCancellingNarrativeUiCommand = false;
					}
				}));
	}
	if (!DialoguePanelWidget)
	{
		DialoguePanelWidget = CanAddPlayerWidgetsToViewport()
			? CreateWidget<UGameXXKDialoguePanelWidget>(this, UGameXXKDialoguePanelWidget::StaticClass())
			: NewObject<UGameXXKDialoguePanelWidget>(this, TEXT("DialoguePanelWidget"));
		if (DialoguePanelWidget && CanAddPlayerWidgetsToViewport()) DialoguePanelWidget->AddToViewport(220);
	}
	if (!DialogueBubbleWidget)
	{
		DialogueBubbleWidget = CanAddPlayerWidgetsToViewport()
			? CreateWidget<UGameXXKSpeechBubbleWidget>(this, UGameXXKSpeechBubbleWidget::StaticClass())
			: NewObject<UGameXXKSpeechBubbleWidget>(this, TEXT("DialogueBubbleWidget"));
		if (DialogueBubbleWidget && CanAddPlayerWidgetsToViewport()) DialogueBubbleWidget->AddToViewport(219);
	}
	if (!DialogueHistoryWidget)
	{
		DialogueHistoryWidget = CanAddPlayerWidgetsToViewport()
			? CreateWidget<UGameXXKDialogueHistoryWidget>(this, UGameXXKDialogueHistoryWidget::StaticClass())
			: NewObject<UGameXXKDialogueHistoryWidget>(this, TEXT("DialogueHistoryWidget"));
		if (DialogueHistoryWidget && CanAddPlayerWidgetsToViewport()) DialogueHistoryWidget->AddToViewport(221);
	}
	if (!DialogueCoordinator)
	{
		DialogueCoordinator = NewObject<UGameXXKDialogueCoordinator>(this, TEXT("DialogueCoordinator"));
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	NarrativeCoordinator->BindState(State, State.NarrativeSequenceSession);
	NarrativeCoordinator->SetDialogueStartDelegate(
		FGameXXKNarrativeDialogueStartRequest::CreateUObject(
			this,
			&AGameXXKMVPPlayerController::HandleNarrativeDialogueStart));
	DialogueCoordinator->Bind(
		State.DialogueSession,
		DialoguePanelWidget,
		DialogueBubbleWidget,
		DialogueHistoryWidget);
	DialogueCoordinator->SetBubbleAnchorResolver(
		FGameXXKDialogueBubbleAnchorResolver::CreateUObject(
			this,
			&AGameXXKMVPPlayerController::ResolveDialogueBubbleAnchor));
	return true;
}

bool AGameXXKMVPPlayerController::CanStartNpcNarrativeInteraction(
	const bool bAllowPausedSession) const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	return Subsystem
		&& Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Town
		&& (bAllowPausedSession
			|| (!Subsystem->GetRuntimeState().NarrativeSequenceSession.bActive
				&& !Subsystem->GetRuntimeState().DialogueSession.bActive))
		&& (!QuestDialogWidget || !QuestDialogWidget->IsDialogOpen())
		&& (!TaskPanelWidget || !TaskPanelWidget->IsTaskPanelOpenForTest())
		&& (!InventoryWindowWidget || !InventoryWindowWidget->IsWindowVisibleForTest())
		&& !IsMetaShopOpenForTest()
		&& !IsCompanionRosterOpenForTest()
		&& !IsBattleOverlayActive()
		&& !IsRouteEncounterPanelOpenForTest()
		&& !IsRouteMerchantWidgetOpenForTest();
}

UGameXXKNarrativeSequenceAsset* AGameXXKMVPPlayerController::ResolveNarrativeSequenceAsset(
	const FName SequenceId) const
{
	if (SequenceId.IsNone())
	{
		return nullptr;
	}
	const FString AssetName = NarrativeAssetStem(SequenceId);
	const FString ObjectPath = FString::Printf(
		TEXT("/Game/GameXXK/Narrative/Sequences/%s.%s"),
		*AssetName,
		*AssetName);
	UGameXXKNarrativeSequenceAsset* Asset =
		LoadObject<UGameXXKNarrativeSequenceAsset>(nullptr, *ObjectPath);
	return Asset && Asset->SequenceId == SequenceId ? Asset : nullptr;
}

UGameXXKDialogueAsset* AGameXXKMVPPlayerController::ResolveDialogueAsset(const FName DialogueId) const
{
	if (DialogueId.IsNone())
	{
		return nullptr;
	}
	const FString AssetName = NarrativeAssetStem(DialogueId);
	const FString ObjectPath = FString::Printf(
		TEXT("/Game/GameXXK/Narrative/Dialogues/%s.%s"),
		*AssetName,
		*AssetName);
	UGameXXKDialogueAsset* Asset = LoadObject<UGameXXKDialogueAsset>(nullptr, *ObjectPath);
	return Asset && Asset->DialogueId == DialogueId ? Asset : nullptr;
}

void AGameXXKMVPPlayerController::BindInteractionRequests(APawn* InPawn)
{
	UGameXXKInteractionComponent* Interaction =
		InPawn ? InPawn->FindComponentByClass<UGameXXKInteractionComponent>() : nullptr;
	if (BoundInteractionComponent.Get() == Interaction && InteractionRequestedHandle.IsValid())
	{
		return;
	}
	UnbindInteractionRequests();
	if (!Interaction)
	{
		return;
	}
	BoundInteractionComponent = Interaction;
	InteractionRequestedHandle = Interaction->OnInteractionRequested().AddUObject(
		this,
		&AGameXXKMVPPlayerController::HandleInteractionRequested);
}

void AGameXXKMVPPlayerController::UnbindInteractionRequests()
{
	if (UGameXXKInteractionComponent* Interaction = BoundInteractionComponent.Get();
		Interaction && InteractionRequestedHandle.IsValid())
	{
		Interaction->OnInteractionRequested().Remove(InteractionRequestedHandle);
	}
	BoundInteractionComponent.Reset();
	InteractionRequestedHandle.Reset();
}

void AGameXXKMVPPlayerController::HandleInteractionRequested(
	AActor* Actor,
	const FName InteractionId,
	const FName SequenceId)
{
	const UGameXXKInteractableComponent* Metadata =
		Actor ? Actor->FindComponentByClass<UGameXXKInteractableComponent>() : nullptr;
	if (!Metadata
		|| Metadata->GetInteractionId() != InteractionId
		|| Metadata->GetNarrativeSequenceId() != SequenceId)
	{
		return;
	}
	OpenTownNpcInteractionForNpc(Actor, GetPawn());
}

void AGameXXKMVPPlayerController::HandleNarrativeDialogueStart(
	const FName DialogueId,
	FGameXXKNarrativeDialogueCompleted Completion)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	UGameXXKDialogueAsset* Asset = ResolveDialogueAsset(DialogueId);
	if (!Subsystem || !DialogueCoordinator || !Asset)
	{
		if (Completion.IsBound()) Completion.Execute(TEXT("Outcome.DialogueUnavailable"));
		RefreshNarrativeInputLock();
		return;
	}
	const FGameXXKNarrativeSequenceSessionState& SequenceSession =
		Subsystem->GetRuntimeState().NarrativeSequenceSession;
	FGameXXKDialogueStartContext Context;
	Context.StoryId = SequenceSession.StoryId;
	Context.StoryVersion = SequenceSession.StoryVersion;
	Context.TaskId = SequenceSession.TaskId;
	Context.StepId = SequenceSession.StepId;
	Context.SequenceId = SequenceSession.SequenceId;
	Context.StageContractId = SequenceSession.StageContractId;
	const FGameXXKNarrativeDialogueCompleted CompletionCopy = Completion;
	FGameXXKDialogueFinished Finished = FGameXXKDialogueFinished::CreateLambda(
		[this, CompletionCopy](const FName CompletedDialogueId, const FName OutcomeId) mutable
		{
			(void)CompletedDialogueId;
			if (CompletionCopy.IsBound()) CompletionCopy.Execute(OutcomeId);
			RefreshNarrativeInputLock();
		});
	FString Error;
	const bool bStarted = Subsystem->GetRuntimeState().DialogueSession.bActive
		? DialogueCoordinator->ResumeDialogue(*Asset, MoveTemp(Finished), &Error)
		: DialogueCoordinator->StartDialogue(
			*Asset,
			Context,
			MoveTemp(Finished),
			&Error);
	if (!bStarted && Completion.IsBound())
	{
		Completion.Execute(TEXT("Outcome.DialogueUnavailable"));
	}
	RefreshNarrativeInputLock();
}

USceneComponent* AGameXXKMVPPlayerController::ResolveDialogueBubbleAnchor(const FName SpeakerId) const
{
	AActor* Actor = ActiveNarrativeInteractionActor.Get();
	const UGameXXKInteractableComponent* Metadata =
		Actor ? Actor->FindComponentByClass<UGameXXKInteractableComponent>() : nullptr;
	return Metadata && Metadata->GetInteractionId() == SpeakerId
		? Metadata->GetPromptAnchor()
		: nullptr;
}

bool AGameXXKMVPPlayerController::HandleNarrativeInput(const FInputKeyEventArgs& Params)
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bNarrativeActive = Subsystem
		&& (Subsystem->GetRuntimeState().NarrativeSequenceSession.bActive
			|| Subsystem->GetRuntimeState().DialogueSession.bActive);
	if (!bNarrativeActive)
	{
		return false;
	}
	if (Params.Event != IE_Pressed)
	{
		return Params.Key == EKeys::F || Params.Key == EKeys::I || Params.Key == EKeys::Q;
	}
	if (Params.Key == EKeys::Escape)
	{
		if (DialogueCoordinator) DialogueCoordinator->PauseAndExit();
		if (NarrativeCoordinator) NarrativeCoordinator->PauseAndRelease();
		SetNarrativeInputLocked(false);
		ApplyPlayerFlowInputMode();
		return true;
	}
	if (!DialogueCoordinator || !DialogueCoordinator->IsBlockingPresentation())
	{
		return Params.Key == EKeys::F || Params.Key == EKeys::I || Params.Key == EKeys::Q || Params.Key == EKeys::Tab;
	}
	bool bHandled = false;
	if (Params.Key == EKeys::SpaceBar || Params.Key == EKeys::Enter || Params.Key == EKeys::LeftMouseButton)
	{
		bHandled = DialogueCoordinator->Advance(nullptr);
	}
	else if (Params.Key == EKeys::LeftControl || Params.Key == EKeys::RightControl)
	{
		bHandled = DialogueCoordinator->SkipSeenCurrentNode(nullptr);
	}
	else
	{
		int32 OptionIndex = INDEX_NONE;
		if (Params.Key == EKeys::One) OptionIndex = 0;
		else if (Params.Key == EKeys::Two) OptionIndex = 1;
		else if (Params.Key == EKeys::Three) OptionIndex = 2;
		else if (Params.Key == EKeys::Four) OptionIndex = 3;
		const FGameXXKDialogueOutput& Output = DialogueCoordinator->GetCurrentOutputForTest();
		if (Output.Options.IsValidIndex(OptionIndex))
		{
			bHandled = DialogueCoordinator->ChooseOption(Output.Options[OptionIndex].OptionId, nullptr);
		}
	}
	RefreshNarrativeInputLock();
	ApplyPlayerFlowInputMode();
	return bHandled || Params.Key == EKeys::F || Params.Key == EKeys::I || Params.Key == EKeys::Q || Params.Key == EKeys::Tab;
}

void AGameXXKMVPPlayerController::RefreshNarrativeInputLock()
{
	const bool bLocked = (NarrativeCoordinator && NarrativeCoordinator->IsInputTokenHeld())
		|| (DialogueCoordinator && DialogueCoordinator->IsBlockingPresentation());
	SetNarrativeInputLocked(bLocked);
	if (!bLocked)
	{
		ActiveNarrativeInteractionActor.Reset();
	}
}

void AGameXXKMVPPlayerController::SetNarrativeInputLocked(const bool bLocked)
{
	if (bLocked && !bNarrativeMoveInputLocked)
	{
		SetIgnoreMoveInput(true);
		bNarrativeMoveInputLocked = true;
	}
	else if (!bLocked && bNarrativeMoveInputLocked)
	{
		SetIgnoreMoveInput(false);
		bNarrativeMoveInputLocked = false;
	}
	if (bLocked && !bNarrativeLookInputLocked)
	{
		SetIgnoreLookInput(true);
		bNarrativeLookInputLocked = true;
	}
	else if (!bLocked && bNarrativeLookInputLocked)
	{
		SetIgnoreLookInput(false);
		bNarrativeLookInputLocked = false;
	}
}

bool AGameXXKMVPPlayerController::IsNarrativeGameplayUiBlocked() const
{
	return (NarrativeCoordinator && NarrativeCoordinator->IsInputTokenHeld())
		|| (DialogueCoordinator && DialogueCoordinator->IsBlockingPresentation());
}

EGameXXKPlayerFlowBootProfile AGameXXKMVPPlayerController::ResolvePlayerFlowBootProfile() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (OverrideBootProfileForTest.IsSet())
	{
		return OverrideBootProfileForTest.GetValue();
	}
#endif

	const UWorld* World = GetWorld();
	const FString PackageName = World && World->GetOutermost()
		? World->GetOutermost()->GetName()
		: FString();
	return GameXXKLevelFlow::IsDesktopTrainingHUDMapPackage(PackageName)
		? EGameXXKPlayerFlowBootProfile::DesktopTrainingOnly
		: EGameXXKPlayerFlowBootProfile::FullPlayerFlow;
}

bool AGameXXKMVPPlayerController::EnsureDesktopTrainingWidgets()
{
	bEnableDesktopTrainingWorkbench = true;
	return EnsureDesktopTrainingWorkbenchWidget() != nullptr;
}

bool AGameXXKMVPPlayerController::ApplyDesktopTrainingPerfProfile(const FString& Profile)
{
	const FString NormalizedProfile = Profile.ToLower();
	if (NormalizedProfile.IsEmpty() || NormalizedProfile == TEXT("empty"))
	{
		return true;
	}
	if (!DesktopTrainingWorkbenchWidget)
	{
		return false;
	}

	DesktopTrainingWorkbenchWidget->OpenBackpack();
	if (NormalizedProfile == TEXT("travel"))
	{
		const FName StageId(TEXT("Training.Normal.1-1"));
		if (!DesktopTrainingWorkbenchWidget->SelectStageForTest(StageId))
		{
			return false;
		}
		DesktopTrainingWorkbenchWidget->ClickTravelForTest();
		const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
		return Subsystem
			&& Subsystem->GetTrainingTravelRuntimeCopy().StageId == StageId
			&& Subsystem->GetTrainingTravelRuntimeCopy().Phase != EGameXXKTrainingTravelPhase::Idle;
	}
	if (NormalizedProfile == TEXT("challenge"))
	{
		UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
		if (!Subsystem
			|| !Subsystem->AcceptQuest()
			|| !Subsystem->OpenDungeonFromTownExit()
			|| !Subsystem->SelectDungeonNode(EGameXXKNodeKind::Start)
			|| !Subsystem->SelectDungeonNode(EGameXXKNodeKind::Battle))
		{
			return false;
		}
		CloseDesktopTrainingWorkbench();
		if (!EnsurePlayerFlowWidgets())
		{
			return false;
		}
		RefreshPlayerFlowWidgets();
		return BattleBoardWidget
			&& BattleBoardWidget->IsBattleBoardVisible()
			&& Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle;
	}
	return false;
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

	if (!WorldMapWidget)
	{
		EnsureWorldMapWidget();
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
	if (bEnableDesktopTrainingWorkbench)
	{
		EnsureDesktopTrainingWorkbenchWidget();
	}

	EnsureRouteMapWidget();
	EnsureBattleBoardWidget();

	if (!InventoryWindowWidget)
	{
		EnsureInventoryWindowWidget();
	}
	if (!MetaShopWidget)
	{
		EnsureMetaShopWidget();
	}

	if (!CompanionRosterWidget)
	{
		EnsureCompanionRosterWidget();
	}

	if (!QuestDialogWidget)
	{
		EnsureQuestDialogWidget();
	}
	if (!RouteEncounterPanelWidget)
	{
		EnsureRouteEncounterPanelWidget();
	}
	if (!RouteMerchantWidget)
	{
		EnsureRouteMerchantWidget();
	}
	if (!RelicBarWidget)
	{
		EnsureRelicBarWidget();
	}

	if (!TaskPanelWidget)
	{
		EnsureTaskPanelWidget();
	}

	RefreshPlayerFlowWidgets();
	return MainMenuWidget && WorldMapWidget && TownOverlayWidget && TownHudWidget && RouteMapWidget && BattleBoardWidget && InventoryWindowWidget && MetaShopWidget && CompanionRosterWidget && QuestDialogWidget && RouteEncounterPanelWidget && RouteMerchantWidget && RelicBarWidget && TaskPanelWidget;
}

UGameXXKOneGameRouteMapWidget* AGameXXKMVPPlayerController::EnsureRouteMapWidget()
{
	const bool bCanAddToViewport = CanAddPlayerWidgetsToViewport();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
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
		RouteMapWidget->SetIsFocusable(true);
		RouteMapWidget->SetMVPSubsystem(Subsystem);
		ConfigureRouteMapWidgetViewport(RouteMapWidget);
		if (bCanAddToViewport && !RouteMapWidget->IsInViewport())
		{
			RouteMapWidget->AddToViewport(40);
		}
	}
	return RouteMapWidget;
}

UGameXXKBattleBoardWidget* AGameXXKMVPPlayerController::EnsureBattleBoardWidget()
{
	const bool bCanAddToViewport = CanAddPlayerWidgetsToViewport();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
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
	return BattleBoardWidget;
}

UGameXXKWorldMapWidget* AGameXXKMVPPlayerController::EnsureWorldMapWidget()
{
	const bool bCanAddToViewport = CanAddPlayerWidgetsToViewport();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!WorldMapWidget)
	{
		TSubclassOf<UGameXXKWorldMapWidget> WidgetClass = WorldMapWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UGameXXKWorldMapWidget::StaticClass();
		}
		WorldMapWidget = bCanAddToViewport ? CreateWidget<UGameXXKWorldMapWidget>(this, WidgetClass) : nullptr;
		if (!WorldMapWidget)
		{
			WorldMapWidget = NewObject<UGameXXKWorldMapWidget>(this, WidgetClass);
		}
	}
	if (WorldMapWidget)
	{
		WorldMapWidget->SetMVPSubsystem(Subsystem);
		ConfigureFullscreenRouteMapSlot(WorldMapWidget);
		if (bCanAddToViewport && !WorldMapWidget->IsInViewport())
		{
			WorldMapWidget->AddToViewport(45);
			ConfigureFullscreenRouteMapSlot(WorldMapWidget);
		}
	}
	return WorldMapWidget;
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

UGameXXKMetaShopWidget* AGameXXKMVPPlayerController::EnsureMetaShopWidget()
{
	const bool bCanAddToViewport = CanAddPlayerWidgetsToViewport();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	bool bCreatedShop = false;
	if (!MetaShopWidget)
	{
		TSubclassOf<UGameXXKMetaShopWidget> WidgetClass = MetaShopWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UGameXXKMetaShopWidget::StaticClass();
		}
		MetaShopWidget = bCanAddToViewport ? CreateWidget<UGameXXKMetaShopWidget>(this, WidgetClass) : nullptr;
		if (!MetaShopWidget)
		{
			MetaShopWidget = NewObject<UGameXXKMetaShopWidget>(this, WidgetClass);
		}
		bCreatedShop = MetaShopWidget != nullptr;
	}
	if (MetaShopWidget)
	{
		MetaShopWidget->SetMVPSubsystem(Subsystem);
		MetaShopWidget->SetCloseRequestedDelegate(FSimpleDelegate::CreateUObject(this, &AGameXXKMVPPlayerController::HandleMetaShopClosed));
		MetaShopWidget->SetCompanionReplacementRequestedDelegate(FSimpleDelegate::CreateUObject(this, &AGameXXKMVPPlayerController::HandleMetaShopCompanionReplacementRequested));
		if (bCreatedShop)
		{
			MetaShopWidget->TakeWidget();
			MetaShopWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		ConfigureFullscreenInventoryWindowSlot(MetaShopWidget);
		if (bCanAddToViewport && !MetaShopWidget->IsInViewport())
		{
			MetaShopWidget->AddToViewport(155);
			ConfigureFullscreenInventoryWindowSlot(MetaShopWidget);
		}
	}
	return MetaShopWidget;
}

UGameXXKCompanionRosterWidget* AGameXXKMVPPlayerController::EnsureCompanionRosterWidget()
{
	const bool bCanAddToViewport = CanAddPlayerWidgetsToViewport();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	bool bCreatedRoster = false;
	if (!CompanionRosterWidget)
	{
		TSubclassOf<UGameXXKCompanionRosterWidget> WidgetClass = CompanionRosterWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UGameXXKCompanionRosterWidget::StaticClass();
		}
		CompanionRosterWidget = bCanAddToViewport ? CreateWidget<UGameXXKCompanionRosterWidget>(this, WidgetClass) : nullptr;
		if (!CompanionRosterWidget)
		{
			CompanionRosterWidget = NewObject<UGameXXKCompanionRosterWidget>(this, WidgetClass);
		}
		bCreatedRoster = CompanionRosterWidget != nullptr;
	}
	if (CompanionRosterWidget)
	{
		CompanionRosterWidget->SetMVPSubsystem(Subsystem);
		CompanionRosterWidget->RefreshFromState();
		if (bCreatedRoster)
		{
			CompanionRosterWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		ConfigureFullscreenInventoryWindowSlot(CompanionRosterWidget);
		if (bCanAddToViewport && !CompanionRosterWidget->IsInViewport())
		{
			CompanionRosterWidget->AddToViewport(150);
			ConfigureFullscreenInventoryWindowSlot(CompanionRosterWidget);
		}
	}
	return CompanionRosterWidget;
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

UGameXXKRouteEncounterPanelWidget* AGameXXKMVPPlayerController::EnsureRouteEncounterPanelWidget()
{
	const bool bCanAddToViewport = CanAddPlayerWidgetsToViewport();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	bool bCreatedRouteEncounterPanel = false;
	if (!RouteEncounterPanelWidget)
	{
		TSubclassOf<UGameXXKRouteEncounterPanelWidget> WidgetClass = RouteEncounterPanelWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UGameXXKRouteEncounterPanelWidget::StaticClass();
		}
		RouteEncounterPanelWidget = bCanAddToViewport ? CreateWidget<UGameXXKRouteEncounterPanelWidget>(this, WidgetClass) : nullptr;
		if (!RouteEncounterPanelWidget)
		{
			RouteEncounterPanelWidget = NewObject<UGameXXKRouteEncounterPanelWidget>(this, WidgetClass);
		}
		bCreatedRouteEncounterPanel = RouteEncounterPanelWidget != nullptr;
	}
	if (RouteEncounterPanelWidget)
	{
		RouteEncounterPanelWidget->SetIsFocusable(true);
		RouteEncounterPanelWidget->SetMVPSubsystem(Subsystem);
		if (bCreatedRouteEncounterPanel)
		{
			RouteEncounterPanelWidget->CloseEncounterPanel();
		}
		ConfigureFullscreenTaskPanelSlot(RouteEncounterPanelWidget);
		if (bCanAddToViewport && !RouteEncounterPanelWidget->IsInViewport())
		{
			RouteEncounterPanelWidget->AddToViewport(180);
			ConfigureFullscreenTaskPanelSlot(RouteEncounterPanelWidget);
		}
	}
	return RouteEncounterPanelWidget;
}

UGameXXKRouteMerchantWidget* AGameXXKMVPPlayerController::EnsureRouteMerchantWidget()
{
	const bool bCanAddToViewport = CanAddPlayerWidgetsToViewport();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	bool bCreatedRouteMerchant = false;
	if (!RouteMerchantWidget)
	{
		TSubclassOf<UGameXXKRouteMerchantWidget> WidgetClass = RouteMerchantWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UGameXXKRouteMerchantWidget::StaticClass();
		}
		RouteMerchantWidget = bCanAddToViewport
			? CreateWidget<UGameXXKRouteMerchantWidget>(this, WidgetClass)
			: nullptr;
		if (!RouteMerchantWidget)
		{
			RouteMerchantWidget = NewObject<UGameXXKRouteMerchantWidget>(this, WidgetClass);
		}
		bCreatedRouteMerchant = RouteMerchantWidget != nullptr;
	}
	if (RouteMerchantWidget)
	{
		RouteMerchantWidget->SetMVPSubsystem(Subsystem);
		RouteMerchantWidget->SetIsFocusable(true);
		if (bCreatedRouteMerchant)
		{
			RouteMerchantWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		ConfigureFullscreenTaskPanelSlot(RouteMerchantWidget);
		if (bCanAddToViewport && !RouteMerchantWidget->IsInViewport())
		{
			RouteMerchantWidget->AddToViewport(185);
			ConfigureFullscreenTaskPanelSlot(RouteMerchantWidget);
		}
	}
	return RouteMerchantWidget;
}

UGameXXKRelicBarWidget* AGameXXKMVPPlayerController::EnsureRelicBarWidget()
{
	const bool bCanAddToViewport = CanAddPlayerWidgetsToViewport();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!RelicBarWidget)
	{
		TSubclassOf<UGameXXKRelicBarWidget> WidgetClass = RelicBarWidgetClass;
		if (!WidgetClass) WidgetClass = UGameXXKRelicBarWidget::StaticClass();
		RelicBarWidget = bCanAddToViewport ? CreateWidget<UGameXXKRelicBarWidget>(this, WidgetClass) : nullptr;
		if (!RelicBarWidget) RelicBarWidget = NewObject<UGameXXKRelicBarWidget>(this, WidgetClass);
	}
	if (RelicBarWidget)
	{
		RelicBarWidget->SetMVPSubsystem(Subsystem);
		RelicBarWidget->PrepareForEmbedding();
		RelicBarWidget->RefreshFromState();
		ConfigureFullscreenTaskPanelSlot(RelicBarWidget);
		if (bCanAddToViewport && !RelicBarWidget->IsInViewport())
		{
			RelicBarWidget->AddToViewport(170);
			ConfigureFullscreenTaskPanelSlot(RelicBarWidget);
		}
	}
	return RelicBarWidget;
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

TSharedRef<SWindow> AGameXXKMVPPlayerController::BuildDesktopTrainingOverlayWindow(
	const FVector2D& WindowPosition,
	const FVector2D& WindowSize,
	const TSharedRef<SWidget>& Content,
	const bool bRequestComposition)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Type(EWindowType::Normal)
		.Style(&FWindowStyle::GetBorderless())
		.Title(FText::FromString(TEXT("GameXXKDesktopOverlay")))
		.AutoCenter(EAutoCenter::None)
		.ScreenPosition(WindowPosition)
		.ClientSize(WindowSize)
		.AdjustInitialSizeAndPositionForDPIScale(false)
		.SupportsTransparency(
			bRequestComposition
				? EWindowTransparency::PerPixel
				: EWindowTransparency::None)
		.SizingRule(ESizingRule::FixedSize)
		.IsPopupWindow(false)
		.IsTopmostWindow(true)
		.FocusWhenFirstShown(false)
		.ActivationPolicy(EWindowActivationPolicy::Always)
		.UseOSWindowBorder(false)
		.HasCloseButton(false)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.CreateTitleBar(false)
		.SaneWindowPlacement(false)
		.LayoutBorder(FMargin(0.0f))
		.bManualManageDPI(true)
		[
			Content
		];
	Window->SetAcceptsInput(true);
	return Window;
}

bool AGameXXKMVPPlayerController::ShouldUseDesktopTrainingOverlayWindow() const
{
#if PLATFORM_WINDOWS
	const UWorld* World = GetWorld();
	const FString MapPackageName = World && World->GetOutermost()
		? World->GetOutermost()->GetName()
		: FString();
	return CanAddPlayerWidgetsToViewport()
		&& FSlateApplication::IsInitialized()
		&& !GIsAutomationTesting
		&& ShouldAttemptDesktopOverlayAfterFailureForTest(
			bDesktopTrainingOverlayFailedForSession)
		&& IGameXXKDesktopOverlayModule::Get().IsRuntimeSupported()
		&& ShouldUseDesktopWindowForMapName(MapPackageName);
#else
	return false;
#endif
}

bool AGameXXKMVPPlayerController::ShouldUseDesktopWindowForMapName(
	const FString& MapPackageName)
{
	return GameXXKLevelFlow::IsDesktopTrainingHUDMapPackage(MapPackageName);
}

bool AGameXXKMVPPlayerController::ShouldRefreshExistingDesktopOverlayAttachment(
	const bool bOverlayWindowValid,
	const bool bCompositionActive,
	const bool bWorkbenchValid)
{
	return bOverlayWindowValid && bCompositionActive && bWorkbenchValid;
}

bool AGameXXKMVPPlayerController::EnsureDesktopTrainingOverlayWindow()
{
	if (DesktopTrainingOverlayWindow.IsValid())
	{
		if (!ShouldRefreshExistingDesktopOverlayAttachment(
			true,
			bDesktopTrainingOverlayCompositionActive,
			DesktopTrainingWorkbenchWidget != nullptr))
		{
			return false;
		}
#if PLATFORM_WINDOWS
		const TSharedPtr<FGenericWindow> NativeWindow =
			DesktopTrainingOverlayWindow->GetNativeWindow();
		void* NativeWindowHandle = NativeWindow.IsValid()
			? NativeWindow->GetOSWindowHandle()
			: nullptr;
		if (!DesktopTrainingWorkbenchWidget->AttachDesktopNativeWindowForPresentation(
				NativeWindowHandle))
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("Desktop overlay reuse failed to restore native workbench attachment: handle=%p"),
				NativeWindowHandle);
			return false;
		}
#endif
		return bDesktopTrainingOverlayCompositionActive;
	}
	if (!ShouldUseDesktopTrainingOverlayWindow() || !DesktopTrainingWorkbenchWidget)
	{
		return false;
	}

	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetInitialDisplayMetrics(DisplayMetrics);
	FPlatformRect WorkArea = DisplayMetrics.PrimaryDisplayWorkAreaRect;
	if (WorkArea.Right <= WorkArea.Left || WorkArea.Bottom <= WorkArea.Top)
	{
		return false;
	}
	const FVector2D HostPosition(
		static_cast<float>(WorkArea.Left),
		static_cast<float>(WorkArea.Top));
	const FVector2D HostSize(
		static_cast<float>(FMath::Max(1, WorkArea.Right - WorkArea.Left)),
		static_cast<float>(FMath::Max(1, WorkArea.Bottom - WorkArea.Top)));
	DesktopTrainingWorkbenchWidget->InitializeDesktopPresentationHostSize(HostSize);
	const FVector2D WindowPosition =
		HostPosition + DesktopTrainingWorkbenchWidget->GetDesktopWindowTopLeftForHost();
	const FVector2D WindowSize = DesktopTrainingWorkbenchWidget->GetDesktopWindowSizeForHost();
	DesktopTrainingOverlayWindow = BuildDesktopTrainingOverlayWindow(
		WindowPosition,
		WindowSize,
		DesktopTrainingWorkbenchWidget->TakeWidget(),
		true);
#if PLATFORM_WINDOWS
	IGameXXKDesktopOverlayModule& Overlay = IGameXXKDesktopOverlayModule::Get();
	FSlateApplication::Get().AddWindow(DesktopTrainingOverlayWindow.ToSharedRef(), false);
	const TSharedPtr<FGenericWindow> NativeWindow =
		DesktopTrainingOverlayWindow->GetNativeWindow();
	void* NativeWindowHandle = NativeWindow.IsValid()
		? NativeWindow->GetOSWindowHandle()
		: nullptr;
	const bool bNativeRegionAttached =
		DesktopTrainingWorkbenchWidget->AttachDesktopNativeWindowForPresentation(
			NativeWindowHandle);
	bool bCompositionAttached = false;
	if (bNativeRegionAttached)
	{
		Overlay.BeginOverlayWindowCreation();
		DesktopTrainingOverlayWindow->ShowWindow();
		bCompositionAttached = Overlay.EndOverlayWindowCreation(NativeWindowHandle);
	}
	bDesktopTrainingOverlayCompositionActive =
		bCompositionAttached && bNativeRegionAttached;
	UE_LOG(
		LogTemp,
		Log,
		TEXT("Desktop overlay attach result: composition=%s native_region=%s handle=%p"),
		bCompositionAttached ? TEXT("true") : TEXT("false"),
		bNativeRegionAttached ? TEXT("true") : TEXT("false"),
		NativeWindowHandle);
	if (!bDesktopTrainingOverlayCompositionActive)
	{
		bDesktopTrainingOverlayFailedForSession = true;
		DesktopTrainingOverlayWindow->HideWindow();
		DesktopTrainingWorkbenchWidget->DetachDesktopNativeWindowForPresentation();
		Overlay.ReleaseOverlayWindow(NativeWindowHandle);
		DesktopTrainingOverlayWindow->SetContent(SNullWidget::NullWidget);
		FSlateApplication::Get().RequestDestroyWindow(
			DesktopTrainingOverlayWindow.ToSharedRef());
		DesktopTrainingOverlayWindow.Reset();
		return false;
	}
	return true;
#else
	return false;
#endif
}

void AGameXXKMVPPlayerController::SetDesktopTrainingGameViewportVisible(const bool bVisible)
{
	const bool bGameCommandLine = FParse::Param(FCommandLine::Get(), TEXT("game"));
	if (!ShouldHideDesktopTrainingGameViewport(GIsEditor, bGameCommandLine))
	{
		return;
	}
	TSharedPtr<SWindow> GameViewportWindow = DesktopTrainingGameViewportWindow.Pin();
	if (GEngine && GEngine->GameViewport && FSlateApplication::IsInitialized())
	{
		if (!GameViewportWindow.IsValid())
		{
			const TSharedPtr<SViewport> GameViewportWidget =
				GEngine->GameViewport->GetGameViewportWidget();
			if (GameViewportWidget.IsValid())
			{
				GameViewportWindow = FSlateApplication::Get().FindWidgetWindow(
					GameViewportWidget.ToSharedRef());
			}
		}
	}
	if (!GameViewportWindow.IsValid())
	{
		const UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		GameViewportWindow = GameEngine ? GameEngine->GameViewportWindow.Pin() : nullptr;
	}
	if (!GameViewportWindow.IsValid()
		|| GameViewportWindow == DesktopTrainingOverlayWindow)
	{
		return;
	}
	DesktopTrainingGameViewportWindow = GameViewportWindow;
	if (bVisible)
	{
		if (!GameViewportWindow->IsVisible())
		{
			GameViewportWindow->ShowWindow();
		}
		bDesktopTrainingGameViewportHidden = false;
	}
	else
	{
		if (GameViewportWindow->IsVisible())
		{
			GameViewportWindow->HideWindow();
		}
		bDesktopTrainingGameViewportHidden = true;
	}
}

bool AGameXXKMVPPlayerController::ShouldHideDesktopTrainingGameViewport(
	const bool bEditorMode,
	const bool bGameCommandLine)
{
	return !bEditorMode || bGameCommandLine;
}

void AGameXXKMVPPlayerController::ShowDesktopTrainingOverlayWindow()
{
	if (!EnsureDesktopTrainingOverlayWindow())
	{
		return;
	}
	if (bDesktopTrainingOverlayCompositionActive)
	{
		DesktopTrainingOverlayWindow->ShowWindow();
		SetDesktopTrainingGameViewportVisible(false);
	}
}

void AGameXXKMVPPlayerController::HideDesktopTrainingOverlayWindow()
{
	if (DesktopTrainingOverlayWindow.IsValid())
	{
		DesktopTrainingOverlayWindow->HideWindow();
	}
	if (bDesktopTrainingGameViewportHidden)
	{
		SetDesktopTrainingGameViewportVisible(true);
	}
}

void AGameXXKMVPPlayerController::DestroyDesktopTrainingOverlayWindow()
{
	HideDesktopTrainingOverlayWindow();
	if (DesktopTrainingOverlayWindow.IsValid() && FSlateApplication::IsInitialized())
	{
#if PLATFORM_WINDOWS
		const TSharedPtr<FGenericWindow> NativeWindow =
			DesktopTrainingOverlayWindow->GetNativeWindow();
		void* NativeWindowHandle = NativeWindow.IsValid()
			? NativeWindow->GetOSWindowHandle()
			: nullptr;
		if (IGameXXKDesktopOverlayModule::IsAvailable())
		{
			IGameXXKDesktopOverlayModule::Get().ReleaseOverlayWindow(NativeWindowHandle);
		}
#endif
		DesktopTrainingOverlayWindow->SetContent(SNullWidget::NullWidget);
		FSlateApplication::Get().RequestDestroyWindow(DesktopTrainingOverlayWindow.ToSharedRef());
	}
	DesktopTrainingOverlayWindow.Reset();
	DesktopTrainingGameViewportWindow.Reset();
	bDesktopTrainingOverlayCompositionActive = false;
}

UGameXXKDesktopTrainingWorkbenchWidget* AGameXXKMVPPlayerController::EnsureDesktopTrainingWorkbenchWidget()
{
	const bool bCanAddToViewport = CanAddPlayerWidgetsToViewport();
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!DesktopTrainingWorkbenchWidget)
	{
		TSubclassOf<UGameXXKDesktopTrainingWorkbenchWidget> WidgetClass = DesktopTrainingWorkbenchWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = UGameXXKDesktopTrainingWorkbenchWidget::StaticClass();
		}
		DesktopTrainingWorkbenchWidget = bCanAddToViewport
			? CreateWidget<UGameXXKDesktopTrainingWorkbenchWidget>(this, WidgetClass)
			: nullptr;
		if (!DesktopTrainingWorkbenchWidget)
		{
			DesktopTrainingWorkbenchWidget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>(this, WidgetClass);
		}
	}
	if (DesktopTrainingWorkbenchWidget)
	{
		DesktopTrainingWorkbenchWidget->SetMVPSubsystem(Subsystem);
		const bool bDesktopWindowPresentation = ResolvePlayerFlowBootProfile()
			== EGameXXKPlayerFlowBootProfile::DesktopTrainingOnly;
		DesktopTrainingWorkbenchWidget->SetPresentationMode(
			bDesktopWindowPresentation
				? EGameXXKDesktopHudPresentationMode::DesktopWindow
				: EGameXXKDesktopHudPresentationMode::TownViewport);
		bool bUsesDesktopOverlay = false;
		if (ShouldUseDesktopTrainingOverlayWindow())
		{
			if (DesktopTrainingWorkbenchWidget->IsInViewport())
			{
				DesktopTrainingWorkbenchWidget->RemoveFromParent();
			}
			bUsesDesktopOverlay = EnsureDesktopTrainingOverlayWindow();
		}
		if (!bUsesDesktopOverlay)
		{
			if (DesktopTrainingOverlayWindow.IsValid())
			{
				DestroyDesktopTrainingOverlayWindow();
			}
			FVector2D ViewportSize = FVector2D::ZeroVector;
			if (GEngine && GEngine->GameViewport)
			{
				GEngine->GameViewport->GetViewportSize(ViewportSize);
			}
			if (ViewportSize.X > 1.0f && ViewportSize.Y > 1.0f)
			{
				DesktopTrainingWorkbenchWidget->InitializeDesktopPresentationHostSize(ViewportSize);
			}
			ConfigureFullscreenTaskPanelSlot(DesktopTrainingWorkbenchWidget);
			if (bCanAddToViewport && !DesktopTrainingWorkbenchWidget->IsInViewport())
			{
				DesktopTrainingWorkbenchWidget->AddToViewport(200);
				ConfigureFullscreenTaskPanelSlot(DesktopTrainingWorkbenchWidget);
			}
		}
	}
	return DesktopTrainingWorkbenchWidget;
}

void AGameXXKMVPPlayerController::RestoreDesktopWorkbenchSessionAfterMapTravel()
{
	UGameInstance* GameInstance = GetGameInstance();
	UGameXXKDesktopHudSessionSubsystem* Session = GameInstance
		? GameInstance->GetSubsystem<UGameXXKDesktopHudSessionSubsystem>()
		: nullptr;
	if (!Session || !DesktopTrainingWorkbenchWidget)
	{
		return;
	}
	FGameXXKDesktopWorkbenchSessionState State;
	if (Session->ConsumeAfterMapTravel(State))
	{
		DesktopTrainingWorkbenchWidget->RestoreSessionStateAfterMapTravel(State);
	}
	bDesktopTownMapTravelPending = false;
}

bool AGameXXKMVPPlayerController::RequestDesktopTownToggleFromWorkbench()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !DesktopTrainingWorkbenchWidget)
	{
		return false;
	}
	const FString CurrentPackageName = World->GetOutermost()
		? World->GetOutermost()->GetName()
		: FString();
	const FName TargetMap =
		GameXXKLevelFlow::TownToggleTargetForMapPackage(CurrentPackageName);
	if (bDesktopTownMapTravelPending || TargetMap.IsNone()
		|| !FPackageName::DoesPackageExist(TargetMap.ToString()))
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UGameXXKDesktopHudSessionSubsystem* Session = GameInstance
		? GameInstance->GetSubsystem<UGameXXKDesktopHudSessionSubsystem>()
		: nullptr;
	if (!Session)
	{
		return false;
	}
	const FGameXXKDesktopWorkbenchSessionState State =
		DesktopTrainingWorkbenchWidget->CaptureSessionStateForMapTravel();
	if (!State.bValid)
	{
		return false;
	}

	Session->StoreForMapTravel(State);
	bDesktopTownMapTravelPending = true;
	DesktopTrainingWorkbenchWidget->SetTownMapTravelPending(true);
	DesktopTrainingWorkbenchWidget->CloseWorkbench();
	HideDesktopTrainingOverlayWindow();
	UGameplayStatics::OpenLevel(World, TargetMap);
	return true;
}

bool AGameXXKMVPPlayerController::OpenDesktopTrainingWorkbench()
{
	if (!bEnableDesktopTrainingWorkbench)
	{
		return false;
	}
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Town)
	{
		return false;
	}
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = EnsureDesktopTrainingWorkbenchWidget();
	if (!Widget)
	{
		return false;
	}
	if (TownHudWidget)
	{
		TownHudWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	const bool bOpened = Widget->IsWorkbenchVisibleForTest() || Widget->OpenWorkbench();
	if (bOpened)
	{
		ShowDesktopTrainingOverlayWindow();
	}
	return bOpened;
}

bool AGameXXKMVPPlayerController::CloseDesktopTrainingWorkbench()
{
	if (!DesktopTrainingWorkbenchWidget)
	{
		return false;
	}
	const bool bClosed = DesktopTrainingWorkbenchWidget->CloseWorkbench();
	HideDesktopTrainingOverlayWindow();
	if (TownHudWidget && ResolveMVPSubsystem() && ResolveMVPSubsystem()->GetRuntimeState().Screen == EGameXXKScreen::Town)
	{
		TownHudWidget->SetVisibility(ESlateVisibility::Visible);
	}
	return bClosed;
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
	if (Subsystem
		&& ActiveScreen == EGameXXKScreen::Town
		&& Subsystem->GetRuntimeState().TownPanelMode != EGameXXKTownPanelMode::None)
	{
		// Inventory and Trade were legacy town-overlay panels. Persisted values
		// must not reopen either retired panel when a town save is restored.
		Subsystem->CloseTownPanel();
	}
	const bool bExitedBattleOverlay = ActiveScreen != EGameXXKScreen::Battle && IsBattleOverlayActive();
	if (bExitedBattleOverlay)
	{
		ExitBattleOverlay();
	}
	if (InventoryWindowWidget
		&& ActiveScreen != EGameXXKScreen::Town
		&& (InventoryWindowWidget->IsWindowVisibleForTest()
			|| InventoryWindowWidget->GetWindowModeForTest() != EGameXXKInventoryWindowMode::None))
	{
		// A town inventory cannot survive into route/battle screens. Go through
		// the controller close path so it clears trade state and restores focus.
		CloseInventoryWindow();
	}
	if (MetaShopWidget && ActiveScreen != EGameXXKScreen::Town
		&& (IsMetaShopOpenForTest() || bMetaShopInputLocked))
	{
		CloseMetaShopWindow();
	}
	if (CompanionRosterWidget && ActiveScreen != EGameXXKScreen::Town && IsCompanionRosterOpenForTest())
	{
		// Permanent roster configuration is town-only. Do not let a full-screen
		// backpack cover the route, event, or card battle after a level transition.
		CloseCompanionRoster();
	}
	if (QuestDialogWidget && ActiveScreen != EGameXXKScreen::Town && IsQuestDialogOpenForTest())
	{
		CloseQuestDialog();
	}
	if (TaskPanelWidget && ActiveScreen != EGameXXKScreen::Town && IsTaskPanelOpenForTest())
	{
		CloseTaskPanel();
	}
	if (RouteEncounterPanelWidget && !IsGenericRouteEncounterScreen(ActiveScreen) && IsRouteEncounterPanelOpenForTest())
	{
		// The modal belongs only to a pending route encounter. Explicit choices
		// close it before state changes; this also covers external transitions.
		ForceCloseRouteEncounterPanelLocal();
	}
	if (ActiveScreen != EGameXXKScreen::RouteMerchant && bRouteMerchantInputLocked)
	{
		SetIgnoreMoveInput(false);
		bRouteMerchantInputLocked = false;
	}
	if (ActiveScreen == EGameXXKScreen::DungeonMap && (!RouteMapWidget))
	{
		// The HUD-only desktop boot profile creates only the workbench while it is
		// in Town. The Challenge route map is the second lazy promotion: it needs
		// the shared route owner without the BattleBoard yet.
		EnsureRouteMapWidget();
	}
	if (IsGenericRouteEncounterScreen(ActiveScreen)
		&& (!RouteMapWidget || !RouteEncounterPanelWidget))
	{
		// Event and Camp nodes on the generated Challenge map use the same
		// source-less pure-HUD choice panel as the legacy route. Promote it
		// lazily on the desktop profile so the node actually opens a panel.
		EnsureRouteMapWidget();
		EnsureRouteEncounterPanelWidget();
	}
	if (ActiveScreen == EGameXXKScreen::RouteMerchant
		&& (!RouteMapWidget || !RouteMerchantWidget))
	{
		// Merchant nodes are hosted by their own full-screen widget; the desktop
		// profile must promote it just like the DungeonMap and Battle surfaces.
		EnsureRouteMapWidget();
		EnsureRouteMerchantWidget();
	}
	if (ActiveScreen == EGameXXKScreen::Battle && (!RouteMapWidget || !BattleBoardWidget))
	{
		// The HUD-only boot profile intentionally creates only the workbench while
		// it is in Town. A direct Training challenge is the one transition that
		// must promote the lazy shell to the shared playable Battle surface.
		EnsureRouteMapWidget();
		EnsureBattleBoardWidget();
	}
	if (ActiveScreen == EGameXXKScreen::Battle)
	{
		// Close every off-screen owner of an input-ignore increment first, but
		// still capture the route before its Battle-state refresh collapses it.
		EnterBattleOverlay();
	}
	if (MainMenuWidget)
	{
		MainMenuWidget->SetMVPSubsystem(Subsystem);
		MainMenuWidget->RefreshFromState();
	}
	if (WorldMapWidget)
	{
		WorldMapWidget->SetMVPSubsystem(Subsystem);
		ConfigureFullscreenRouteMapSlot(WorldMapWidget);
		WorldMapWidget->RefreshFromState();
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
	if (bEnableDesktopTrainingWorkbench)
	{
		UGameXXKDesktopTrainingWorkbenchWidget* Workbench = EnsureDesktopTrainingWorkbenchWidget();
		if (Workbench && ActiveScreen == EGameXXKScreen::Town)
		{
			Workbench->SetMVPSubsystem(Subsystem);
			// Return-to-Town restores the pure-2D shell only when a non-Town screen
			// actually closed it. The flag is cleared before opening so a rebuild
			// that re-enters this refresh can never reopen the workbench again.
			if (bDesktopWorkbenchClosedForFlow
				&& !Workbench->IsWorkbenchVisibleForTest())
			{
				bDesktopWorkbenchClosedForFlow = false;
				OpenDesktopTrainingWorkbench();
			}
		}
		else if (Workbench && ActiveScreen != EGameXXKScreen::Town)
		{
			// The workbench is a Town-owned shell. Close it before entering route or
			// battle screens so it cannot retain input or cover gameplay. Record
			// ownership even when the originating Workbench action already collapsed
			// itself before notifying us; Town must still restore the idle strip.
			bDesktopWorkbenchClosedForFlow = true;
			Workbench->CloseWorkbench();
			HideDesktopTrainingOverlayWindow();
		}
		if (TownHudWidget && Workbench && Workbench->IsWorkbenchVisibleForTest())
		{
			TownHudWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		else if (TownHudWidget && ActiveScreen == EGameXXKScreen::Town)
		{
			TownHudWidget->SetVisibility(ESlateVisibility::Visible);
		}
	}
	else if (DesktopTrainingWorkbenchWidget)
	{
		DesktopTrainingWorkbenchWidget->CloseWorkbench();
		HideDesktopTrainingOverlayWindow();
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
	}
	if (MetaShopWidget)
	{
		MetaShopWidget->SetMVPSubsystem(Subsystem);
		if (IsMetaShopOpenForTest())
		{
			MetaShopWidget->RefreshFromState();
		}
	}
	if (CompanionRosterWidget)
	{
		CompanionRosterWidget->SetMVPSubsystem(Subsystem);
		CompanionRosterWidget->RefreshFromState();
		if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Town)
		{
			CompanionRosterWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	if (QuestDialogWidget)
	{
		QuestDialogWidget->SetMVPSubsystem(Subsystem);
	}
	if (RouteEncounterPanelWidget)
	{
		RouteEncounterPanelWidget->SetMVPSubsystem(Subsystem);
		RouteEncounterPanelWidget->RefreshFromState();
	}
	if (RouteMerchantWidget)
	{
		RouteMerchantWidget->SetMVPSubsystem(Subsystem);
		RouteMerchantWidget->RefreshFromState();
	}
	if (RelicBarWidget)
	{
		RelicBarWidget->SetMVPSubsystem(Subsystem);
		RelicBarWidget->RefreshFromState();
	}
	if (!Subsystem || !IsGenericRouteEncounterScreen(Subsystem->GetRuntimeState().Screen))
	{
		ForceCloseRouteEncounterPanelLocal();
	}
	else if (IsGenericRouteEncounterScreen(Subsystem->GetRuntimeState().Screen)
		&& !IsRouteEncounterPanelOpenForTest())
	{
		// Event and camp nodes use this shared pure-HUD choice panel. On the
		// desktop training map there is no scene actor, so the source-less
		// variant opens exactly like a route-map event in full flow.
		// RouteMerchant is deliberately hosted by RouteMerchantWidget instead.
		OpenRouteEncounterPanelInternal(nullptr);
	}
	else if (IsRouteEncounterPanelOpenForTest() && !HasValidRouteEncounterContext())
	{
		// A route transition or a destroyed/replaced source actor invalidates the
		// modal context.  Do not leave a stale panel that could resolve a new node.
		ForceCloseRouteEncounterPanelLocal();
	}

	const bool bRouteMerchantOpen = ActiveScreen == EGameXXKScreen::RouteMerchant
		&& IsRouteMerchantWidgetOpenForTest();
	if (bRouteMerchantOpen && !bRouteMerchantInputLocked)
	{
		FlushPressedKeys();
		SetIgnoreMoveInput(true);
		bRouteMerchantInputLocked = true;
	}
	else if (!bRouteMerchantOpen && bRouteMerchantInputLocked)
	{
		SetIgnoreMoveInput(false);
		bRouteMerchantInputLocked = false;
	}
	if (TaskPanelWidget)
	{
		TaskPanelWidget->SetMVPSubsystem(Subsystem);
		TaskPanelWidget->RefreshFromState();
	}
	if (!bExitedBattleOverlay)
	{
		ApplyPlayerFlowInputMode();
	}
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
	return World && World->IsGameWorld() && IsLocalPlayerController() && Player;
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
	if (IsBattleOverlayActive() && BattleBoardWidget)
	{
		// ApplyBattleOverlayEntry already focused the board before starting its
		// visual session. Re-taking the same widget here is redundant and can
		// tear down a headless Slate resource between refreshes.
		if (TrackedInputMode != EGameXXKTrackedInputMode::UIOnly)
		{
			SetTrackedInputMode(EGameXXKTrackedInputMode::UIOnly, BattleBoardWidget);
		}
		return;
	}

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const EGameXXKScreen ActiveScreen = Subsystem ? Subsystem->GetRuntimeState().Screen : EGameXXKScreen::MainMenu;

	UWidget* WidgetToFocus = nullptr;
	if (ActiveScreen == EGameXXKScreen::MainMenu && MainMenuWidget)
	{
		WidgetToFocus = MainMenuWidget;
	}
	else if (ActiveScreen == EGameXXKScreen::WorldMap && WorldMapWidget)
	{
		WidgetToFocus = WorldMapWidget;
	}
	else if (ActiveScreen == EGameXXKScreen::Town && QuestDialogWidget && QuestDialogWidget->IsDialogOpen())
	{
		WidgetToFocus = QuestDialogWidget;
	}
	else if (ActiveScreen == EGameXXKScreen::Town && TaskPanelWidget && TaskPanelWidget->IsTaskPanelOpenForTest())
	{
		WidgetToFocus = TaskPanelWidget;
	}
	else if (ActiveScreen == EGameXXKScreen::Town && IsMetaShopOpenForTest())
	{
		WidgetToFocus = MetaShopWidget;
	}
	else if (ActiveScreen == EGameXXKScreen::Town && IsCompanionRosterOpenForTest())
	{
		WidgetToFocus = CompanionRosterWidget;
	}
	else if (ActiveScreen == EGameXXKScreen::RouteMerchant && IsRouteMerchantWidgetOpenForTest())
	{
		WidgetToFocus = RouteMerchantWidget;
	}
	else if (IsGenericRouteEncounterScreen(ActiveScreen) && IsRouteEncounterPanelOpenForTest())
	{
		WidgetToFocus = RouteEncounterPanelWidget;
	}
	else if (ActiveScreen == EGameXXKScreen::DungeonMap && RouteMapWidget)
	{
		WidgetToFocus = RouteMapWidget;
	}
	else if (ActiveScreen == EGameXXKScreen::Battle && BattleBoardWidget)
	{
		WidgetToFocus = BattleBoardWidget;
	}
	SetTrackedInputMode(EGameXXKTrackedInputMode::GameAndUI, WidgetToFocus);

	if (ActiveScreen == EGameXXKScreen::Town
		&& (!QuestDialogWidget || !QuestDialogWidget->IsDialogOpen())
		&& (!TaskPanelWidget || !TaskPanelWidget->IsTaskPanelOpenForTest())
		&& !IsMetaShopOpenForTest()
		&& !IsCompanionRosterOpenForTest()
		&& FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
	}
	else if ((ActiveScreen == EGameXXKScreen::RouteEvent
		|| ActiveScreen == EGameXXKScreen::RouteCamp)
		&& !IsRouteEncounterPanelOpenForTest()
		&& FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
	}
	else if (ActiveScreen == EGameXXKScreen::RouteMerchant
		&& !IsRouteMerchantWidgetOpenForTest()
		&& FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
	}
}

void AGameXXKMVPPlayerController::SetTrackedInputMode(
	const EGameXXKTrackedInputMode InputMode,
	UWidget* WidgetToFocus)
{
	const UUserWidget* const UserWidgetToFocus = Cast<UUserWidget>(WidgetToFocus);
	const bool bCanFocusWidget = WidgetToFocus
		&& (!UserWidgetToFocus || UserWidgetToFocus->IsFocusable());
	if (GetWorld())
	{
		switch (InputMode)
		{
		case EGameXXKTrackedInputMode::GameOnly:
		{
			FInputModeGameOnly Mode;
			SetInputMode(Mode);
			break;
		}
		case EGameXXKTrackedInputMode::UIOnly:
		{
			FInputModeUIOnly Mode;
			if (bCanFocusWidget)
			{
				Mode.SetWidgetToFocus(WidgetToFocus->TakeWidget());
			}
			SetInputMode(Mode);
			break;
		}
		case EGameXXKTrackedInputMode::GameAndUI:
		default:
		{
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(false);
			if (bCanFocusWidget)
			{
				Mode.SetWidgetToFocus(WidgetToFocus->TakeWidget());
			}
			SetInputMode(Mode);
			break;
		}
		}
	}
	TrackedInputMode = InputMode;
}

bool AGameXXKMVPPlayerController::PrepareForRuntimeStateMapTravel(const FString& CurrentPackageName)
{
	const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !GameXXKLevelFlow::RequiresMapLoadForRuntimeState(CurrentPackageName, Subsystem->GetRuntimeState()))
	{
		return false;
	}
	ExitBattleOverlay();
	return true;
}

void AGameXXKMVPPlayerController::HandlePreLoadMapWithContext(
	const FWorldContext& WorldContext,
	const FString& MapName)
{
	(void)MapName;
	if (WorldContext.World() == GetWorld())
	{
		ExitBattleOverlay();
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
	AGameXXKRouteEncounterSceneActor* SourceActor = GetFocusedRouteEncounterActor();
	return SourceActor && SourceActor->ApplyDefaultInteraction(GetPawn());
}

AGameXXKRouteEncounterSceneActor* AGameXXKMVPPlayerController::GetFocusedRouteEncounterActor() const
{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const EGameXXKScreen ActiveScreen = Subsystem ? Subsystem->GetRuntimeState().Screen : EGameXXKScreen::MainMenu;
	if (!IsGenericRouteEncounterScreen(ActiveScreen))
	{
		return nullptr;
	}

	APawn* ControlledPawn = GetPawn();
	UGameXXKInteractionComponent* Interaction = ControlledPawn ? ControlledPawn->FindComponentByClass<UGameXXKInteractionComponent>() : nullptr;
	AGameXXKRouteEncounterSceneActor* SourceActor = Interaction
		? Cast<AGameXXKRouteEncounterSceneActor>(Interaction->GetFocusedActor())
		: nullptr;
	if (!SourceActor || SourceActor->GetWorld() != GetWorld() || !SourceActor->MatchesRuntimeScreen(ActiveScreen))
	{
		return nullptr;
	}
	return SourceActor;
}


bool AGameXXKMVPPlayerController::HasValidRouteEncounterContext() const

{
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || !IsGenericRouteEncounterScreen(Subsystem->GetRuntimeState().Screen))
	{
		return false;
	}

	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	if (ActiveRouteEncounterScreen != State.Screen
		|| ActiveRouteEncounterNodeId != State.PendingRouteNodeId)
	{
		return false;
	}
	if (!ActiveRouteEncounterSourceActor.IsValid())
	{
		if (!CanAddPlayerWidgetsToViewport())
		{
			return true;
		}

		// Non-combat route scenes are source-less HUD choices. Their world map is
		// only presentation, so the screen-to-map contract is the full context.
		const UWorld* World = GetWorld();
		const FString CurrentPackageName = World && World->GetOutermost() ? World->GetOutermost()->GetName() : FString();
		return IsSourceLessRouteEncounterPackageValid(CurrentPackageName, State.Screen);
	}

	AGameXXKRouteEncounterSceneActor* SourceActor = ActiveRouteEncounterSourceActor.Get();
	return SourceActor
		&& SourceActor->GetWorld() == GetWorld()
		&& SourceActor->MatchesRuntimeScreen(State.Screen)
		&& ActiveRouteEncounterScreen == State.Screen
		&& ActiveRouteEncounterNodeId == State.PendingRouteNodeId;
}

bool AGameXXKMVPPlayerController::ForceCloseRouteEncounterPanelLocal()
{
	const bool bOwnedContext = IsRouteEncounterPanelOpenForTest()
		|| ActiveRouteEncounterSourceActor.IsValid()
		|| ActiveRouteEncounterNodeId != INDEX_NONE;
	const bool bClosed = RouteEncounterPanelWidget && RouteEncounterPanelWidget->CloseEncounterPanel();
	ClearRouteEncounterContext();
	if (bOwnedContext)
	{
		SetIgnoreMoveInput(false);
		ApplyPlayerFlowInputMode();
	}
	return bClosed;
}

void AGameXXKMVPPlayerController::ClearRouteEncounterContext()
{
	ActiveRouteEncounterSourceActor.Reset();
	ActiveRouteEncounterScreen = EGameXXKScreen::MainMenu;
	ActiveRouteEncounterNodeId = INDEX_NONE;
}

bool AGameXXKMVPPlayerController::UpdateBattleTargetingPointer(FVector2D CursorScreenPosition)
{
	if (!BattleBoardWidget
		|| (!BattleBoardWidget->IsTargetingBattleActionForTest() && !BattleBoardWidget->IsCardTargetingActive()))
	{
		return false;
	}
	BattleBoardWidget->UpdateTargetingPointer(CursorScreenPosition);
	return true;
}

bool AGameXXKMVPPlayerController::UpdateBattleTargetingPointerFromMouse()
{
	if (!BattleBoardWidget
		|| (!BattleBoardWidget->IsTargetingBattleActionForTest() && !BattleBoardWidget->IsCardTargetingActive()))
	{
		return false;
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (bUseBattleMousePositionOverrideForTest)
	{
		return UpdateBattleTargetingPointer(BattleMousePositionOverrideForTest);
	}
#endif

	float CursorX = 0.0f;
	float CursorY = 0.0f;
	if (!GetMousePosition(CursorX, CursorY))
	{
		return false;
	}
	double ScaledCursorX = 0.0;
	double ScaledCursorY = 0.0;
	if (UWidgetLayoutLibrary::GetMousePositionScaledByDPI(this, ScaledCursorX, ScaledCursorY))
	{
		// Viewport-local UMG coordinates do not contain the floating PIE window's
		// desktop origin. Convert them through the Board's 1920x1080 safe stage so
		// a new-editor-window session cannot add its top-left offset to the arrow.
		BattleBoardWidget->UpdateTargetingPointerFromViewportLocalPosition(
			FVector2D(ScaledCursorX, ScaledCursorY));
		return true;
	}
	if (FSlateApplication::IsInitialized())
	{
		BattleBoardWidget->UpdateTargetingPointerFromSlateAbsolutePosition(FSlateApplication::Get().GetCursorPos());
		return true;
	}
	BattleBoardWidget->UpdateTargetingPointerFromViewportLocalPosition(FVector2D(CursorX, CursorY));
	return true;
}
