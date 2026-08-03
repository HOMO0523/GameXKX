#include "GameXXKCardBattleAdapter.h"
#include "GameXXKMVPRules.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Input/Events.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKBattleSceneUnitActor.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKBattleRuntimeUnit MakeBridgeEnemy()
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = TEXT("MoneyRat");
		Unit.DisplayName = FText::FromString(TEXT("钱鼠"));
		Unit.HP = 240;
		Unit.MaxHP = 240;
		Unit.Attack = 8;
		Unit.Defense = 0;
		Unit.Speed = 8;
		Unit.bEnemy = true;
		return Unit;
	}

	bool BuildManualTargetFixture(
		UGameXXKMVPSubsystem* Subsystem,
		const EGameXXKCardTargetSide DesiredTargetSide,
		FName& OutCardInstanceId,
		FName& OutTargetUnitId,
		FString& OutError)
	{
		OutCardInstanceId = NAME_None;
		OutTargetUnitId = NAME_None;
		OutError.Reset();
		if (!Subsystem)
		{
			OutError = TEXT("The bridge test subsystem is missing.");
			return false;
		}

		for (int32 Seed = 1; Seed <= 256; ++Seed)
		{
			FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
			State = UGameXXKMVPRules::CreateNewGame();
			State.Screen = EGameXXKScreen::Battle;
			State.bHasActiveBattle = true;
			State.ActiveBattleNodeId = 17;
			State.ActiveBattleEnemies = {MakeBridgeEnemy()};

			FString Error;
			if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error)
				|| !FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, Seed, &Error))
			{
				OutError = Error;
				return false;
			}

			for (const FGameXXKCardInstance& CardInstance : State.CardRun.ActiveBattle.Deck.Hand)
			{
				FGameXXKCardPlayPreview Preview;
				if (!FGameXXKCardBattleAdapter::BuildCardPlayPreview(State, CardInstance.InstanceId, Preview, &Error)
					|| !Preview.bCanPlay
					|| !Preview.TargetRequest.bRequiresManualSelection)
				{
					continue;
				}

				const FGameXXKCardTargetCandidateView* Candidate = Preview.TargetRequest.CandidateViews.FindByPredicate([DesiredTargetSide](const FGameXXKCardTargetCandidateView& View)
				{
					return View.bCanSelect && View.Side == DesiredTargetSide;
				});
				if (Candidate)
				{
					OutCardInstanceId = CardInstance.InstanceId;
					OutTargetUnitId = Candidate->UnitId;
					return true;
				}
			}
		}

		OutError = TEXT("No affordable manual card with the requested target side was found in deterministic opening hands.");
		return false;
	}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleInputBridgeTest,
	"GameXXK.Integration.CardBattle.ControllerInputBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleInputBridgeTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	Controller->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("controller creates the battle board for stable-ID bridge coverage"), Controller->EnsurePlayerFlowWidgetsForTest());

	FName PartyCardInstanceId;
	FName PartyTargetUnitId;
	FString FixtureError;
	TestTrue(FString::Printf(TEXT("fixture finds a manual friendly-target card: %s"), *FixtureError),
		BuildManualTargetFixture(Subsystem, EGameXXKCardTargetSide::Party, PartyCardInstanceId, PartyTargetUnitId, FixtureError));
	if (PartyCardInstanceId.IsNone() || PartyTargetUnitId.IsNone())
	{
		return false;
	}

	Controller->RefreshPlayerFlowWidgetsForTest();
	UGameXXKBattleBoardWidget* Board = Controller->GetBattleBoardWidgetForTest();
	TestNotNull(TEXT("controller exposes its battle board"), Board);
	if (!Board)
	{
		return false;
	}
	const FGameXXKCardInstance* PartyCardInstance = Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand.FindByPredicate([PartyCardInstanceId](const FGameXXKCardInstance& Candidate)
	{
		return Candidate.InstanceId == PartyCardInstanceId;
	});
	TestNotNull(TEXT("the selected friendly card retains its stable owner unit id"), PartyCardInstance);
	if (!PartyCardInstance || PartyCardInstance->OwnerUnitId.IsNone())
	{
		return false;
	}

	const FVector2D InitialOwnerProjection(486.0f, 338.0f);
	const FVector2D MovedOwnerProjection(672.0f, 414.0f);
	Board->RegisterBattleUnitScreenPosition(PartyCardInstance->OwnerUnitId, InitialOwnerProjection);
	TestTrue(TEXT("friendly-target card enters card targeting"), Board->ClickCardInHand(PartyCardInstanceId));
	TestTrue(TEXT("friendly-target card stays in card targeting mode"), Board->IsCardTargetingActive());
	TestEqual(TEXT("card arrow starts at the owner projection registered before selection"), Board->GetTargetingSourcePositionForTest(), InitialOwnerProjection);
	Board->RegisterBattleUnitScreenPosition(PartyCardInstance->OwnerUnitId, MovedOwnerProjection);
	TestEqual(TEXT("a refreshed owner projection moves the active card arrow source"), Board->GetTargetingSourcePositionForTest(), MovedOwnerProjection);

	const FName EnemyUnitId = Subsystem->GetRuntimeState().ActiveBattleEnemies[0].Id;
	TestFalse(TEXT("illegal enemy UnitId does not spend the selected friendly card"), Controller->ConfirmBattleTargetForUnitId(EnemyUnitId));
	TestTrue(TEXT("illegal stable-ID target keeps the card targeting state"), Board->IsCardTargetingActive());
	TestFalse(TEXT("empty stable UnitId does not cancel card targeting"), Controller->ConfirmBattleTargetForUnitId(NAME_None));
	TestTrue(TEXT("blank scene click keeps the card targeting state"), Board->IsCardTargetingActive());

	TestTrue(TEXT("controller commits a friendly card target directly by stable UnitId"), Controller->ConfirmBattleTargetForUnitId(PartyTargetUnitId));
	TestFalse(TEXT("a committed friendly target exits card targeting"), Board->IsCardTargetingActive());

	FName EnemyCardInstanceId;
	FName EnemyTargetUnitId;
	FixtureError.Reset();
	TestTrue(FString::Printf(TEXT("fixture finds a manual enemy-target card: %s"), *FixtureError),
		BuildManualTargetFixture(Subsystem, EGameXXKCardTargetSide::Enemy, EnemyCardInstanceId, EnemyTargetUnitId, FixtureError));
	if (EnemyCardInstanceId.IsNone() || EnemyTargetUnitId.IsNone())
	{
		return false;
	}

	Controller->RefreshPlayerFlowWidgetsForTest();
	Board = Controller->GetBattleBoardWidgetForTest();
	TestTrue(TEXT("enemy-target card enters card targeting"), Board && Board->ClickCardInHand(EnemyCardInstanceId));
	TestTrue(TEXT("controller commits an enemy card target directly by stable UnitId"), Controller->ConfirmBattleTargetForUnitId(EnemyTargetUnitId));
	TestFalse(TEXT("a committed enemy target exits card targeting"), Board && Board->IsCardTargetingActive());

	const FName SceneWorldName = MakeUniqueObjectName(
		nullptr,
		UWorld::StaticClass(),
		TEXT("GameXXKCardBattleInputBridgeWorld"),
		EUniqueObjectNameOptions::GloballyUnique);
	UWorld* const SceneWorld = UWorld::CreateWorld(EWorldType::Game, false, SceneWorldName, GetTransientPackage());
	TestNotNull(TEXT("input bridge creates an isolated game world"), SceneWorld);
	if (!SceneWorld)
	{
		return false;
	}
	FWorldContext& SceneWorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	SceneWorld->AddToRoot();
	SceneWorldContext.SetCurrentWorld(SceneWorld);
	SceneWorld->InitializeActorsForPlay(FURL());

	UGameInstance* const SceneGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const SceneSubsystem = NewObject<UGameXXKMVPSubsystem>(SceneGameInstance);
	FName SceneCardInstanceId;
	FName ScenePartyTargetUnitId;
	FixtureError.Reset();
	TestTrue(FString::Printf(TEXT("HUD bridge fixture finds a friendly-target card: %s"), *FixtureError),
		BuildManualTargetFixture(SceneSubsystem, EGameXXKCardTargetSide::Party, SceneCardInstanceId, ScenePartyTargetUnitId, FixtureError));
	if (SceneCardInstanceId.IsNone() || ScenePartyTargetUnitId.IsNone())
	{
		SceneWorld->DestroyWorld(false);
		GEngine->DestroyWorldContext(SceneWorld);
		SceneWorld->RemoveFromRoot();
		return false;
	}
	SceneSubsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::DungeonMap;

	FActorSpawnParameters TransientSpawnParameters;
	TransientSpawnParameters.ObjectFlags |= RF_Transient;
	TransientSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGameXXKMVPPlayerController* const SceneController = SceneWorld->SpawnActor<AGameXXKMVPPlayerController>(
		AGameXXKMVPPlayerController::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, TransientSpawnParameters);
	ACameraActor* const ExistingViewCamera = SceneWorld->SpawnActor<ACameraActor>(
		ACameraActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, TransientSpawnParameters);
	TestNotNull(TEXT("HUD bridge spawns a controller in the isolated game world"), SceneController);
	TestNotNull(TEXT("HUD bridge spawns one pre-existing view camera"), ExistingViewCamera);
	if (!SceneController || !ExistingViewCamera)
	{
		SceneWorld->DestroyWorld(false);
		GEngine->DestroyWorldContext(SceneWorld);
		SceneWorld->RemoveFromRoot();
		return false;
	}

	ExistingViewCamera->SetActorLocation(FVector(1234.0f, -567.0f, 890.0f));
	ExistingViewCamera->SetActorRotation(FRotator(-41.0f, 117.0f, 3.0f));
	UCameraComponent* const ExistingViewCameraComponent = ExistingViewCamera->GetCameraComponent();
	TestNotNull(TEXT("the pre-existing view camera has a camera component"), ExistingViewCameraComponent);
	if (!ExistingViewCameraComponent)
	{
		SceneWorld->DestroyWorld(false);
		GEngine->DestroyWorldContext(SceneWorld);
		SceneWorld->RemoveFromRoot();
		return false;
	}
	ExistingViewCameraComponent->FieldOfView = 47.5f;
	ExistingViewCameraComponent->AspectRatio = 1.31f;
	ExistingViewCameraComponent->bConstrainAspectRatio = false;
	SceneController->SetViewTarget(ExistingViewCamera);
	SceneController->SetMVPSubsystemForTest(SceneSubsystem);
	TestTrue(TEXT("HUD bridge controller creates its player-flow widgets"), SceneController->EnsurePlayerFlowWidgetsForTest());
	UGameXXKBattleBoardWidget* const SceneBoard = SceneController->GetBattleBoardWidgetForTest();
	UGameXXKOneGameRouteMapWidget* const RouteWidgetBeforeBattle = SceneController->GetRouteMapWidgetForTest();
	TestNotNull(TEXT("HUD bridge controller exposes the battle board"), SceneBoard);
	TestNotNull(TEXT("HUD bridge controller exposes the route widget"), RouteWidgetBeforeBattle);
	if (!SceneBoard || !RouteWidgetBeforeBattle)
	{
		SceneWorld->DestroyWorld(false);
		GEngine->DestroyWorldContext(SceneWorld);
		SceneWorld->RemoveFromRoot();
		return false;
	}
	if (!SceneController->HasActorBegunPlay())
	{
		SceneController->DispatchBeginPlay();
	}
	ULocalPlayer* const SceneLocalPlayer = NewObject<ULocalPlayer>(GEngine);
	SceneController->SetPlayer(SceneLocalPlayer);
	TestTrue(TEXT("BeginPlay binds the real pre-load map delegate"),
		FCoreUObjectDelegates::PreLoadMapWithContext.IsBoundToObject(SceneController));

	// An off-screen town modal owns its own move-ignore increment. Battle entry
	// must close that modal before taking the overlay snapshot, then acquire a
	// fresh overlay increment which is released exactly once on ordinary exit.
	SceneSubsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Town;
	SceneController->RefreshPlayerFlowWidgetsForTest();
	TestTrue(TEXT("town companion modal opens before the synthetic battle transition"), SceneController->OpenCompanionRoster());
	TestTrue(TEXT("town companion modal owns a move-ignore increment"), SceneController->IsMoveInputIgnored());
	SceneSubsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Battle;
	SceneController->RefreshPlayerFlowWidgetsForTest();
	TestFalse(TEXT("battle entry closes the stale town companion modal"), SceneController->IsCompanionRosterOpenForTest());
	TestTrue(TEXT("battle overlay remains move-ignored after stale modal cleanup"), SceneController->IsMoveInputIgnored());
	SceneSubsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::DungeonMap;
	SceneController->RefreshPlayerFlowWidgetsForTest();
	TestFalse(TEXT("modal-to-battle fixture exits the overlay"), SceneController->IsBattleOverlayActive());
	TestFalse(TEXT("modal-to-battle exit leaks no move-ignore increment"), SceneController->IsMoveInputIgnored());

	AGameXXKBattleSceneUnitActor* const LegacySceneUnit = SceneWorld->SpawnActor<AGameXXKBattleSceneUnitActor>(
		AGameXXKBattleSceneUnitActor::StaticClass(), FVector(25.0f, 47.0f, 80.0f), FRotator::ZeroRotator, TransientSpawnParameters);
	TestNotNull(TEXT("dormant-bridge fixture pre-places one legacy scene unit"), LegacySceneUnit);
	if (!LegacySceneUnit)
	{
		SceneController->EndPlay(EEndPlayReason::Destroyed);
		SceneWorld->DestroyWorld(false);
		GEngine->DestroyWorldContext(SceneWorld);
		SceneWorld->RemoveFromRoot();
		return false;
	}
	LegacySceneUnit->SetMVPSubsystemForTest(SceneSubsystem);
	SceneController->SetBattleWorldProjectionOverrideForTest(true);
	const auto ResolveCardOwnerUnitId = [SceneSubsystem](const FName CardInstanceId) -> FName
	{
		const FGameXXKCardInstance* const Card = SceneSubsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand.FindByPredicate(
			[CardInstanceId](const FGameXXKCardInstance& Candidate)
			{
				return Candidate.InstanceId == CardInstanceId;
			});
		return Card ? Card->OwnerUnitId : NAME_None;
	};
	const auto ConfigureLegacyPartyTarget = [SceneSubsystem, LegacySceneUnit](const FName UnitId) -> bool
	{
		const TArray<FGameXXKBattleRuntimeUnit>& Party = SceneSubsystem->GetRuntimeState().ActiveBattleParty;
		const int32 UnitIndex = Party.IndexOfByPredicate([UnitId](const FGameXXKBattleRuntimeUnit& Unit)
		{
			return Unit.Id == UnitId;
		});
		if (!Party.IsValidIndex(UnitIndex))
		{
			return false;
		}
		LegacySceneUnit->ConfigureFromRuntimeUnit(false, UnitIndex, Party[UnitIndex], UnitIndex + 1);
		return LegacySceneUnit->GetUnitId() == UnitId;
	};
	const FVector2D HudOwnedPosition(901.0f, 509.0f);
	const FName SceneCardOwnerUnitId = ResolveCardOwnerUnitId(SceneCardInstanceId);
	TestFalse(TEXT("dormant-bridge fixture resolves the current card owner"), SceneCardOwnerUnitId.IsNone());
	TestTrue(TEXT("dormant-bridge fixture configures the legacy actor as the legal target"),
		ConfigureLegacyPartyTarget(ScenePartyTargetUnitId));
	const FVector LegacyActorLocation = LegacySceneUnit->GetActorLocation();
	SceneBoard->RegisterBattleUnitScreenPosition(SceneCardOwnerUnitId, HudOwnedPosition);
	LegacySceneUnit->SetCardTargetHighlight(true);

	AWorldSettings* const SceneWorldSettings = SceneWorld->GetWorldSettings();
	APlayerState* const OriginalPauser = SceneWorldSettings ? SceneWorldSettings->GetPauserPlayerState() : nullptr;
	APlayerState* const TestPauser = SceneWorld->SpawnActor<APlayerState>(
		APlayerState::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, TransientSpawnParameters);
	if (SceneWorldSettings)
	{
		SceneWorldSettings->SetPauserPlayerState(TestPauser);
	}
	UGameplayStatics::SetEnableWorldRendering(SceneWorld, false);
	TestTrue(TEXT("non-default fixture begins paused"), UGameplayStatics::IsGamePaused(SceneWorld));
	TestFalse(TEXT("non-default fixture begins with world rendering disabled"), UGameplayStatics::GetEnableWorldRendering(SceneWorld));
	RouteWidgetBeforeBattle->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RouteWidgetBeforeBattle->RestoreScrollOffset(123.0f);
	const float RouteScrollBeforeBattle = RouteWidgetBeforeBattle->GetCurrentScrollOffset();
	SceneController->bShowMouseCursor = false;
	SceneController->bEnableClickEvents = false;
	SceneController->bEnableMouseOverEvents = false;
	SceneController->SetShouldPerformFullTickWhenPausedForTest(false);
	SceneController->ResetIgnoreMoveInput();
	SceneController->ResetIgnoreLookInput();
	SceneController->SetIgnoreMoveInput(true);
	SceneController->SetIgnoreLookInput(true);
	SceneController->SetTrackedInputModeForTest(EGameXXKTrackedInputMode::GameOnly);

	const FString WorldPackageBeforeBattle = SceneWorld->GetOutermost()->GetName();
	AActor* const ViewTargetBeforeBattle = SceneController->GetViewTarget();
	const FVector CameraLocationBeforeBattle = ExistingViewCamera->GetActorLocation();
	const FRotator CameraRotationBeforeBattle = ExistingViewCamera->GetActorRotation();
	const float CameraFovBeforeBattle = ExistingViewCameraComponent->FieldOfView;
	const float CameraAspectBeforeBattle = ExistingViewCameraComponent->AspectRatio;
	const bool bCameraConstrainedBeforeBattle = ExistingViewCameraComponent->bConstrainAspectRatio;

	SceneSubsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Battle;
	SceneController->RefreshPlayerFlowWidgetsForTest();
	TestTrue(TEXT("battle overlay is active in the isolated game world"), SceneController->IsBattleOverlayActive());
	TestTrue(TEXT("battle overlay retains the same route widget"), SceneController->GetRouteMapWidgetForTest() == RouteWidgetBeforeBattle);
	TestEqual(TEXT("battle overlay keeps the same world package"), SceneWorld->GetOutermost()->GetName(), WorldPackageBeforeBattle);
	TestTrue(TEXT("battle overlay keeps the active view target"), SceneController->GetViewTarget() == ViewTargetBeforeBattle);
	TestEqual(TEXT("battle overlay keeps camera location"), ExistingViewCamera->GetActorLocation(), CameraLocationBeforeBattle);
	TestTrue(TEXT("battle overlay keeps camera rotation"), ExistingViewCamera->GetActorRotation().Equals(CameraRotationBeforeBattle, 0.001f));
	TestTrue(TEXT("battle overlay keeps camera FOV"), FMath::IsNearlyEqual(ExistingViewCameraComponent->FieldOfView, CameraFovBeforeBattle, 0.001f));
	TestTrue(TEXT("battle overlay keeps camera aspect"), FMath::IsNearlyEqual(ExistingViewCameraComponent->AspectRatio, CameraAspectBeforeBattle, 0.001f));
	TestEqual(TEXT("battle overlay keeps camera aspect constraint"), ExistingViewCameraComponent->bConstrainAspectRatio, bCameraConstrainedBeforeBattle);
	TestTrue(TEXT("entry preserves an already-paused world"), UGameplayStatics::IsGamePaused(SceneWorld));
	TestFalse(TEXT("entry keeps world rendering disabled"), UGameplayStatics::GetEnableWorldRendering(SceneWorld));
	TestEqual(TEXT("entry tracks UI-only input"), SceneController->GetTrackedInputModeForTest(), EGameXXKTrackedInputMode::UIOnly);
	TestTrue(TEXT("entry enables the mouse cursor"), SceneController->bShowMouseCursor);
	TestTrue(TEXT("entry enables click events"), SceneController->bEnableClickEvents);
	TestTrue(TEXT("entry enables mouse-over events"), SceneController->bEnableMouseOverEvents);
	TestTrue(TEXT("entry enables full controller ticks while the overlay is paused"), SceneController->ShouldPerformFullTickWhenPaused());
	TestEqual(TEXT("overlay entry leaves HUD-owned unit positions untouched"),
		SceneBoard->GetBattleUnitScreenPositionForTest(SceneCardOwnerUnitId), HudOwnedPosition);
	TestTrue(TEXT("overlay entry leaves a pre-existing legacy highlight untouched"), LegacySceneUnit->IsCardTargetHighlighted());
	SceneBoard->RegisterBattleUnitScreenPosition(SceneCardOwnerUnitId, HudOwnedPosition);
	LegacySceneUnit->SetCardTargetHighlight(false);
	const FVector2D IdlePointerBeforeTick = SceneBoard->GetTargetingPointerPositionForTest();
	SceneController->SetBattleMousePositionOverrideForTest(FVector2D(711.0f, 333.0f));
	SceneWorld->Tick(LEVELTICK_All, 1.0f / 60.0f);
	TestEqual(TEXT("PlayerTick leaves the HUD pointer untouched outside targeting"),
		SceneBoard->GetTargetingPointerPositionForTest(), IdlePointerBeforeTick);
	TestEqual(TEXT("idle PlayerTick leaves the legacy actor location untouched"),
		LegacySceneUnit->GetActorLocation(), LegacyActorLocation);
	TestTrue(TEXT("HUD bridge card enters manual targeting without a scene actor"), SceneBoard->ClickCardInHand(SceneCardInstanceId));
	TestFalse(TEXT("HUD fallback provides a nonzero targeting origin"), SceneBoard->GetTargetingSourcePositionForTest().IsNearlyZero());
	const FVector2D PointerPosition(812.0f, 468.0f);
	SceneController->SetBattleMousePositionOverrideForTest(PointerPosition);
	SceneWorld->Tick(LEVELTICK_All, 1.0f / 60.0f);
	TestEqual(TEXT("runtime PlayerTick follows the controlled mouse during card targeting"),
		SceneBoard->GetTargetingPointerPositionForTest(), PointerPosition);
	TestEqual(TEXT("PlayerTick leaves HUD-owned unit positions untouched"),
		SceneBoard->GetBattleUnitScreenPositionForTest(SceneCardOwnerUnitId), HudOwnedPosition);
	TestEqual(TEXT("card-targeting PlayerTick leaves the legacy actor location untouched"),
		LegacySceneUnit->GetActorLocation(), LegacyActorLocation);
	TestFalse(TEXT("PlayerTick never applies targeting highlight to a legacy actor"), LegacySceneUnit->IsCardTargetHighlighted());
	SceneController->ClearBattleMousePositionOverrideForTest();
	SceneBoard->RegisterBattleUnitScreenPosition(SceneCardOwnerUnitId, HudOwnedPosition);
	LegacySceneUnit->SetCardTargetHighlight(false);
	SceneController->RefreshPlayerFlowWidgetsForTest();
	TestEqual(TEXT("player-flow refresh leaves HUD-owned unit positions untouched"),
		SceneBoard->GetBattleUnitScreenPositionForTest(SceneCardOwnerUnitId), HudOwnedPosition);
	TestFalse(TEXT("player-flow refresh never applies targeting highlight to a legacy actor"), LegacySceneUnit->IsCardTargetHighlighted());
	SceneBoard->RegisterBattleUnitScreenPosition(SceneCardOwnerUnitId, HudOwnedPosition);
	LegacySceneUnit->SetCardTargetHighlight(true);
	TestTrue(TEXT("stable UnitId bridge commits the HUD-selected target"), SceneController->ConfirmBattleTargetForUnitId(ScenePartyTargetUnitId));
	TestEqual(TEXT("stable UnitId confirmation leaves HUD-owned positions untouched"),
		SceneBoard->GetBattleUnitScreenPositionForTest(SceneCardOwnerUnitId), HudOwnedPosition);
	TestTrue(TEXT("stable UnitId confirmation never mutates a legacy actor highlight"), LegacySceneUnit->IsCardTargetHighlighted());

	FName PhysicalCardInstanceId;
	FName PhysicalTargetUnitId;
	FixtureError.Reset();
	TestTrue(FString::Printf(TEXT("dormant-bridge fixture rebuilds a card for physical-click authority: %s"), *FixtureError),
		BuildManualTargetFixture(
			SceneSubsystem,
			EGameXXKCardTargetSide::Party,
			PhysicalCardInstanceId,
			PhysicalTargetUnitId,
			FixtureError));
	SceneController->RefreshPlayerFlowWidgetsForTest();
	TestTrue(TEXT("physical-click fixture configures the legacy scene actor"), ConfigureLegacyPartyTarget(PhysicalTargetUnitId));
	TestTrue(TEXT("physical-click fixture enters manual card targeting"), SceneBoard->ClickCardInHand(PhysicalCardInstanceId));
	SceneController->SetBattleSceneCursorHitOverrideForTest(LegacySceneUnit);
	SceneController->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 1.0f));
	TestTrue(TEXT("legacy scene-unit clicks never take card-targeting authority"), SceneBoard->IsCardTargetingActive());
	SceneController->ClearBattleSceneCursorHitOverrideForTest();
	SceneBoard->CancelBattleTargeting();

	FName EscapeCardInstanceId;
	FName EscapeTargetUnitId;
	FixtureError.Reset();
	TestTrue(FString::Printf(TEXT("dormant-bridge fixture rebuilds a card for Escape cancellation: %s"), *FixtureError),
		BuildManualTargetFixture(
			SceneSubsystem,
			EGameXXKCardTargetSide::Party,
			EscapeCardInstanceId,
			EscapeTargetUnitId,
			FixtureError));
	SceneController->RefreshPlayerFlowWidgetsForTest();
	TestTrue(TEXT("Escape fixture configures the legacy scene actor"), ConfigureLegacyPartyTarget(EscapeTargetUnitId));
	const FName EscapeCardOwnerUnitId = ResolveCardOwnerUnitId(EscapeCardInstanceId);
	SceneBoard->RegisterBattleUnitScreenPosition(EscapeCardOwnerUnitId, HudOwnedPosition);
	TestTrue(TEXT("Escape fixture enters manual card targeting"), SceneBoard->ClickCardInHand(EscapeCardInstanceId));
	LegacySceneUnit->SetCardTargetHighlight(true);
	TestTrue(TEXT("UI-only overlay configures the fullscreen board as focusable"), SceneBoard->IsFocusable());
	const FKeyEvent EscapeKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
	const FReply EscapeReply = SceneBoard->TakeWidget()->OnKeyDown(FGeometry(), EscapeKeyEvent);
	TestTrue(TEXT("fullscreen board handles Escape through its Slate key path"), EscapeReply.IsEventHandled());
	TestFalse(TEXT("Escape exits card targeting"), SceneBoard->IsCardTargetingActive());
	TestEqual(TEXT("Escape cancellation leaves HUD-owned positions untouched"),
		SceneBoard->GetBattleUnitScreenPositionForTest(EscapeCardOwnerUnitId), HudOwnedPosition);
	TestTrue(TEXT("Escape cancellation never mutates a legacy actor highlight"), LegacySceneUnit->IsCardTargetHighlighted());

	FGameXXKRuntimeState& LegacyActionState = SceneSubsystem->GetMutableRuntimeState();
	LegacyActionState.CardRun.bHasActiveCardBattle = false;
	SceneBoard->RefreshFromState();
	TestTrue(TEXT("legacy action fixture opens a party command menu"),
		SceneBoard->OpenCommandMenuForPartyUnit(0, FVector2D(960.0f, 360.0f), FVector2D(960.0f, 360.0f)));
	TestTrue(TEXT("legacy action fixture enters basic-attack targeting"), SceneBoard->ExecuteBasicAttackAction());
	TestTrue(TEXT("legacy action fixture is targeting an action"), SceneBoard->IsTargetingBattleActionForTest());
	const FVector2D ActionPointerPosition(744.0f, 402.0f);
	SceneController->SetBattleMousePositionOverrideForTest(ActionPointerPosition);
	SceneWorld->Tick(LEVELTICK_All, 1.0f / 60.0f);
	TestEqual(TEXT("runtime PlayerTick follows the controlled mouse during action targeting"),
		SceneBoard->GetTargetingPointerPositionForTest(), ActionPointerPosition);
	TestEqual(TEXT("action-targeting PlayerTick leaves the legacy actor location untouched"),
		LegacySceneUnit->GetActorLocation(), LegacyActorLocation);
	TestTrue(TEXT("action-targeting PlayerTick leaves the legacy actor highlight untouched"), LegacySceneUnit->IsCardTargetHighlighted());
	TestTrue(TEXT("action cancellation returns to the command menu"), SceneBoard->CancelBattleTargeting());
	TestTrue(TEXT("command-menu cancellation returns to idle"), SceneBoard->CancelBattleTargeting());
	const FVector2D IdlePointerAfterAction = SceneBoard->GetTargetingPointerPositionForTest();
	SceneController->SetBattleMousePositionOverrideForTest(FVector2D(601.0f, 287.0f));
	SceneWorld->Tick(LEVELTICK_All, 1.0f / 60.0f);
	TestEqual(TEXT("PlayerTick again leaves the HUD pointer untouched after action targeting"),
		SceneBoard->GetTargetingPointerPositionForTest(), IdlePointerAfterAction);
	TestEqual(TEXT("post-action idle PlayerTick leaves the legacy actor location untouched"),
		LegacySceneUnit->GetActorLocation(), LegacyActorLocation);
	TestTrue(TEXT("post-action idle PlayerTick leaves the legacy actor highlight untouched"), LegacySceneUnit->IsCardTargetHighlighted());
	SceneController->ClearBattleMousePositionOverrideForTest();
	LegacyActionState.CardRun.bHasActiveCardBattle = true;
	SceneBoard->RefreshFromState();

	SceneSubsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::DungeonMap;
	SceneController->RefreshPlayerFlowWidgetsForTest();
	TestFalse(TEXT("ordinary battle exit deactivates the overlay"), SceneController->IsBattleOverlayActive());
	TestEqual(TEXT("dungeon-map refresh becomes authoritative after overlay restoration"), RouteWidgetBeforeBattle->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("ordinary exit restores route scroll exactly"), RouteWidgetBeforeBattle->GetCurrentScrollOffset(), RouteScrollBeforeBattle);
	TestFalse(TEXT("ordinary exit restores hidden cursor"), SceneController->bShowMouseCursor);
	TestFalse(TEXT("ordinary exit restores disabled click events"), SceneController->bEnableClickEvents);
	TestFalse(TEXT("ordinary exit restores disabled mouse-over events"), SceneController->bEnableMouseOverEvents);
	TestFalse(TEXT("ordinary exit releases the overlay-owned full paused tick"), SceneController->ShouldPerformFullTickWhenPaused());
	TestEqual(TEXT("ordinary exit restores GameOnly input"), SceneController->GetTrackedInputModeForTest(), EGameXXKTrackedInputMode::GameOnly);
	TestTrue(TEXT("ordinary exit preserves pre-existing move ignore"), SceneController->IsMoveInputIgnored());
	TestTrue(TEXT("ordinary exit preserves pre-existing look ignore"), SceneController->IsLookInputIgnored());
	SceneController->SetIgnoreMoveInput(false);
	SceneController->SetIgnoreLookInput(false);
	TestFalse(TEXT("overlay did not stack move ignore"), SceneController->IsMoveInputIgnored());
	TestFalse(TEXT("overlay did not stack look ignore"), SceneController->IsLookInputIgnored());

	RouteWidgetBeforeBattle->SetVisibility(ESlateVisibility::Hidden);
	RouteWidgetBeforeBattle->RestoreScrollOffset(77.0f);
	SceneSubsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Battle;
	SceneController->RefreshPlayerFlowWidgetsForTest();
	TestTrue(TEXT("pre-travel fixture re-enters the overlay"), SceneController->IsBattleOverlayActive());
	TestTrue(TEXT("pre-travel entry reacquires full paused ticking"), SceneController->ShouldPerformFullTickWhenPaused());
	SceneSubsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Town;
	const FName ForeignWorldName = MakeUniqueObjectName(
		nullptr,
		UWorld::StaticClass(),
		TEXT("GameXXKForeignPreLoadWorld"),
		EUniqueObjectNameOptions::GloballyUnique);
	UWorld* const ForeignWorld = UWorld::CreateWorld(EWorldType::PIE, false, ForeignWorldName, GetTransientPackage());
	TestNotNull(TEXT("pre-load isolation creates a distinct foreign world"), ForeignWorld);
	if (!ForeignWorld)
	{
		SceneController->EndPlay(EEndPlayReason::Destroyed);
		if (SceneWorldSettings)
		{
			SceneWorldSettings->SetPauserPlayerState(OriginalPauser);
		}
		SceneWorld->DestroyWorld(false);
		GEngine->DestroyWorldContext(SceneWorld);
		SceneWorld->RemoveFromRoot();
		return false;
	}
	TestTrue(TEXT("pre-load isolation uses a world distinct from the battle overlay"), ForeignWorld != SceneWorld);
	ForeignWorld->AddToRoot();
	FWorldContext& ForeignWorldContext = GEngine->CreateNewWorldContext(EWorldType::PIE);
	ForeignWorldContext.PIEInstance = 1;
	ForeignWorldContext.SetCurrentWorld(ForeignWorld);
	TestTrue(TEXT("pre-load isolation uses the registered foreign world context"), ForeignWorldContext.World() == ForeignWorld);
	FCoreUObjectDelegates::PreLoadMapWithContext.Broadcast(ForeignWorldContext, TEXT("/Game/GameXXK/Maps/L_QingshanInn"));
	TestTrue(TEXT("pre-load broadcast for another world leaves this overlay active"), SceneController->IsBattleOverlayActive());
	ForeignWorld->DestroyWorld(false);
	GEngine->DestroyWorldContext(ForeignWorld);
	ForeignWorld->RemoveFromRoot();
	FWorldContext MatchingPreLoadWorldContext;
	MatchingPreLoadWorldContext.WorldType = EWorldType::PIE;
	MatchingPreLoadWorldContext.PIEInstance = 1;
	MatchingPreLoadWorldContext.SetCurrentWorld(SceneWorld);
	FCoreUObjectDelegates::PreLoadMapWithContext.Broadcast(MatchingPreLoadWorldContext, TEXT("/Game/GameXXK/Maps/L_QingshanInn"));
	TestFalse(TEXT("matching real pre-load broadcast exits the overlay"), SceneController->IsBattleOverlayActive());
	TestFalse(TEXT("matching real pre-load cleanup releases full paused ticking"), SceneController->ShouldPerformFullTickWhenPaused());
	TestEqual(TEXT("real pre-load cleanup restores route visibility"), RouteWidgetBeforeBattle->GetVisibility(), ESlateVisibility::Hidden);
	TestEqual(TEXT("real pre-load cleanup restores route scroll"), RouteWidgetBeforeBattle->GetCurrentScrollOffset(), 77.0f);

	SceneSubsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::DungeonMap;
	SceneController->RefreshPlayerFlowWidgetsForTest();
	RouteWidgetBeforeBattle->SetVisibility(ESlateVisibility::HitTestInvisible);
	RouteWidgetBeforeBattle->RestoreScrollOffset(91.0f);
	SceneController->bShowMouseCursor = false;
	SceneController->SetShouldPerformFullTickWhenPausedForTest(true);
	SceneController->SetTrackedInputModeForTest(EGameXXKTrackedInputMode::GameOnly);
	SceneSubsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Battle;
	SceneController->RefreshPlayerFlowWidgetsForTest();
	TestTrue(TEXT("EndPlay fixture re-enters the overlay"), SceneController->IsBattleOverlayActive());
	TestTrue(TEXT("overlay entry preserves a pre-existing full paused tick"), SceneController->ShouldPerformFullTickWhenPaused());
	SceneController->EndPlay(EEndPlayReason::Destroyed);
	TestFalse(TEXT("EndPlay removes the real pre-load map delegate"),
		FCoreUObjectDelegates::PreLoadMapWithContext.IsBoundToObject(SceneController));
	TestFalse(TEXT("EndPlay cleanup exits the overlay"), SceneController->IsBattleOverlayActive());
	TestEqual(TEXT("EndPlay cleanup restores route visibility"), RouteWidgetBeforeBattle->GetVisibility(), ESlateVisibility::HitTestInvisible);
	TestEqual(TEXT("EndPlay cleanup restores route scroll"), RouteWidgetBeforeBattle->GetCurrentScrollOffset(), 91.0f);
	TestFalse(TEXT("EndPlay cleanup restores cursor state"), SceneController->bShowMouseCursor);
	TestTrue(TEXT("EndPlay cleanup preserves a pre-existing full paused tick"), SceneController->ShouldPerformFullTickWhenPaused());
	TestEqual(TEXT("EndPlay cleanup restores tracked input"), SceneController->GetTrackedInputModeForTest(), EGameXXKTrackedInputMode::GameOnly);
	TestTrue(TEXT("EndPlay cleanup preserves the initial paused state"), UGameplayStatics::IsGamePaused(SceneWorld));
	TestFalse(TEXT("EndPlay cleanup preserves disabled world rendering"), UGameplayStatics::GetEnableWorldRendering(SceneWorld));

	if (SceneWorldSettings)
	{
		SceneWorldSettings->SetPauserPlayerState(OriginalPauser);
	}
	SceneWorld->DestroyWorld(false);
	GEngine->DestroyWorldContext(SceneWorld);
	SceneWorld->RemoveFromRoot();
	return true;
}

#endif
