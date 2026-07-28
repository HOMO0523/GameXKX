#include "GameXXKCardBattleAdapter.h"
#include "GameXXKMVPRules.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKBattleSceneUnitActor.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "PaperFlipbookComponent.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattleUnitHudWidget.h"

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

	AGameXXKBattleSceneUnitActor* MakeSceneActorForStableUnitId(
		UGameXXKMVPSubsystem* Subsystem,
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const int32 DeliberatelyWrongIndex)
	{
		if (!Subsystem)
		{
			return nullptr;
		}

		const TArray<FGameXXKBattleRuntimeUnit>& Units = Side == EGameXXKCardTargetSide::Enemy
			? Subsystem->GetRuntimeState().ActiveBattleEnemies
			: Subsystem->GetRuntimeState().ActiveBattleParty;
		const FGameXXKBattleRuntimeUnit* Unit = Units.FindByPredicate([UnitId](const FGameXXKBattleRuntimeUnit& Candidate)
		{
			return Candidate.Id == UnitId;
		});
		if (!Unit)
		{
			return nullptr;
		}

		AGameXXKBattleSceneUnitActor* Actor = NewObject<AGameXXKBattleSceneUnitActor>();
		Actor->ConfigureFromRuntimeUnit(Side == EGameXXKCardTargetSide::Enemy, DeliberatelyWrongIndex, *Unit);
		return Actor;
	}

	AGameXXKBattleSceneUnitActor* SpawnSceneActorForBridge(
		UWorld* World,
		const FGameXXKBattleRuntimeUnit& Unit,
		const bool bEnemy,
		const int32 UnitIndex)
	{
		if (!World)
		{
			return nullptr;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGameXXKBattleSceneUnitActor* Actor = World->SpawnActor<AGameXXKBattleSceneUnitActor>(
			AGameXXKBattleSceneUnitActor::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParameters);
		if (Actor)
		{
			Actor->ConfigureFromRuntimeUnit(bEnemy, UnitIndex, Unit);
		}
		return Actor;
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
	AGameXXKBattleSceneUnitActor* IllegalEnemyActor = MakeSceneActorForStableUnitId(
		Subsystem,
		EnemyUnitId,
		EGameXXKCardTargetSide::Enemy,
		73);
	TestNotNull(TEXT("bridge test creates an illegal enemy scene actor"), IllegalEnemyActor);
	TestFalse(TEXT("illegal scene actor target does not spend the selected friendly card"), Controller->ConfirmBattleTargetForUnitForTest(IllegalEnemyActor));
	TestTrue(TEXT("illegal scene actor target keeps the card targeting state"), Board->IsCardTargetingActive());
	TestFalse(TEXT("blank scene click does not cancel card targeting"), Controller->ConfirmBattleTargetForUnitForTest(nullptr));
	TestTrue(TEXT("blank scene click keeps the card targeting state"), Board->IsCardTargetingActive());

	AGameXXKBattleSceneUnitActor* PartyTargetActor = MakeSceneActorForStableUnitId(
		Subsystem,
		PartyTargetUnitId,
		EGameXXKCardTargetSide::Party,
		77);
	TestNotNull(TEXT("bridge test creates a friendly stable-ID scene actor"), PartyTargetActor);
	TestEqual(TEXT("friendly actor carries the card runtime stable unit id"), PartyTargetActor ? PartyTargetActor->GetUnitId() : NAME_None, PartyTargetUnitId);
	TestTrue(TEXT("controller commits a friendly card target by UnitId, not its scene index"), Controller->ConfirmBattleTargetForUnitForTest(PartyTargetActor));
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
	AGameXXKBattleSceneUnitActor* EnemyTargetActor = MakeSceneActorForStableUnitId(
		Subsystem,
		EnemyTargetUnitId,
		EGameXXKCardTargetSide::Enemy,
		91);
	TestNotNull(TEXT("bridge test creates an enemy stable-ID scene actor"), EnemyTargetActor);
	TestEqual(TEXT("enemy actor carries the card runtime stable unit id"), EnemyTargetActor ? EnemyTargetActor->GetUnitId() : NAME_None, EnemyTargetUnitId);
	TestTrue(TEXT("controller commits an enemy card target by UnitId, not its scene index"), Controller->ConfirmBattleTargetForUnitForTest(EnemyTargetActor));
	TestFalse(TEXT("a committed enemy target exits card targeting"), Board && Board->IsCardTargetingActive());

	UWorld* const SceneWorld = GWorld;
	TestNotNull(TEXT("automation supplies a world for the scene-to-board bridge"), SceneWorld);
	if (!SceneWorld)
	{
		return false;
	}

	UGameInstance* const SceneGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const SceneSubsystem = NewObject<UGameXXKMVPSubsystem>(SceneGameInstance);
	FName SceneCardInstanceId;
	FName ScenePartyTargetUnitId;
	FixtureError.Reset();
	TestTrue(FString::Printf(TEXT("scene bridge fixture finds a friendly-target card: %s"), *FixtureError),
		BuildManualTargetFixture(SceneSubsystem, EGameXXKCardTargetSide::Party, SceneCardInstanceId, ScenePartyTargetUnitId, FixtureError));
	if (SceneCardInstanceId.IsNone() || ScenePartyTargetUnitId.IsNone())
	{
		return false;
	}

	FActorSpawnParameters ControllerSpawnParameters;
	ControllerSpawnParameters.ObjectFlags |= RF_Transient;
	ControllerSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGameXXKMVPPlayerController* const SceneController = SceneWorld->SpawnActor<AGameXXKMVPPlayerController>(
		AGameXXKMVPPlayerController::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		ControllerSpawnParameters);
	TestNotNull(TEXT("scene bridge test spawns a controller with a live world"), SceneController);
	if (!SceneController)
	{
		return false;
	}

	TArray<AGameXXKBattleSceneUnitActor*> SpawnedSceneActors;
	TMap<FName, AGameXXKBattleSceneUnitActor*> SceneActorsByUnitId;
	for (int32 PartyIndex = 0; PartyIndex < SceneSubsystem->GetRuntimeState().ActiveBattleParty.Num(); ++PartyIndex)
	{
		AGameXXKBattleSceneUnitActor* Actor = SpawnSceneActorForBridge(
			SceneWorld,
			SceneSubsystem->GetRuntimeState().ActiveBattleParty[PartyIndex],
			false,
			PartyIndex + 77);
		TestNotNull(TEXT("scene bridge spawns a party unit actor"), Actor);
		if (Actor)
		{
			SpawnedSceneActors.Add(Actor);
			SceneActorsByUnitId.Add(Actor->GetUnitId(), Actor);
		}
	}
	for (int32 EnemyIndex = 0; EnemyIndex < SceneSubsystem->GetRuntimeState().ActiveBattleEnemies.Num(); ++EnemyIndex)
	{
		AGameXXKBattleSceneUnitActor* Actor = SpawnSceneActorForBridge(
			SceneWorld,
			SceneSubsystem->GetRuntimeState().ActiveBattleEnemies[EnemyIndex],
			true,
			EnemyIndex);
		TestNotNull(TEXT("scene bridge spawns an enemy unit actor"), Actor);
		if (Actor)
		{
			SpawnedSceneActors.Add(Actor);
			SceneActorsByUnitId.Add(Actor->GetUnitId(), Actor);
		}
	}

	SceneController->SetMVPSubsystemForTest(SceneSubsystem);
	TestTrue(TEXT("scene bridge controller creates a board"), SceneController->EnsurePlayerFlowWidgetsForTest());
	FActorSpawnParameters CameraSpawnParameters;
	CameraSpawnParameters.ObjectFlags |= RF_Transient;
	CameraSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* const SceneBattleCamera = SceneWorld->SpawnActor<ACameraActor>(
		ACameraActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, CameraSpawnParameters);
	TestNotNull(TEXT("the bridge test creates a transient battle camera"), SceneBattleCamera);
	if (UCameraComponent* const MutableSceneBattleCameraComponent = SceneBattleCamera ? SceneBattleCamera->GetCameraComponent() : nullptr)
	{
		SceneBattleCamera->SetActorLocation(FVector(1234.0f, -567.0f, 890.0f));
		SceneBattleCamera->SetActorRotation(FRotator(-41.0f, 117.0f, 3.0f));
		MutableSceneBattleCameraComponent->FieldOfView = 47.5f;
		MutableSceneBattleCameraComponent->AspectRatio = 1.0f;
		MutableSceneBattleCameraComponent->bConstrainAspectRatio = false;
	}
	const FVector OriginalCameraLocation = SceneBattleCamera ? SceneBattleCamera->GetActorLocation() : FVector::ZeroVector;
	const FRotator OriginalCameraRotation = SceneBattleCamera ? SceneBattleCamera->GetActorRotation() : FRotator::ZeroRotator;
	const float OriginalCameraFov = SceneBattleCamera && SceneBattleCamera->GetCameraComponent()
		? SceneBattleCamera->GetCameraComponent()->FieldOfView
		: 0.0f;
	SceneController->ConfigureBattleSceneCameraForTest(SceneBattleCamera);
	const UCameraComponent* const SceneBattleCameraComponent = SceneBattleCamera ? SceneBattleCamera->GetCameraComponent() : nullptr;
	TestNotNull(TEXT("the battle scene camera exposes a camera component"), SceneBattleCameraComponent);
	TestEqual(TEXT("configuring the HUD aspect never moves an existing battle camera"),
		SceneBattleCamera ? SceneBattleCamera->GetActorLocation() : FVector::ZeroVector,
		OriginalCameraLocation);
	TestTrue(TEXT("configuring the HUD aspect never rotates an existing battle camera"),
		SceneBattleCamera && SceneBattleCamera->GetActorRotation().Equals(OriginalCameraRotation, 0.001f));
	TestTrue(TEXT("configuring the HUD aspect preserves an existing battle camera FOV"),
		SceneBattleCameraComponent && FMath::IsNearlyEqual(SceneBattleCameraComponent->FieldOfView, OriginalCameraFov, 0.001f));
	TestTrue(TEXT("the battle camera constrains its world composition to the fixed 16:9 HUD stage"),
		SceneBattleCameraComponent && SceneBattleCameraComponent->bConstrainAspectRatio);
	TestTrue(TEXT("the battle camera uses the same 16:9 aspect ratio as the fixed HUD stage"),
		SceneBattleCameraComponent && FMath::IsNearlyEqual(SceneBattleCameraComponent->AspectRatio, 16.0f / 9.0f, 0.001f));
	UGameXXKBattleBoardWidget* const SceneBoard = SceneController->GetBattleBoardWidgetForTest();
	TestNotNull(TEXT("scene bridge controller exposes the board"), SceneBoard);
	const FName SceneEnemyUnitId = SceneSubsystem->GetRuntimeState().ActiveBattleEnemies[0].Id;
	AGameXXKBattleSceneUnitActor* const ScenePartyTargetActor = SceneActorsByUnitId.FindRef(ScenePartyTargetUnitId);
	AGameXXKBattleSceneUnitActor* const SceneEnemyActor = SceneActorsByUnitId.FindRef(SceneEnemyUnitId);
	TestNotNull(TEXT("scene bridge finds the selected friendly target actor"), ScenePartyTargetActor);
	TestNotNull(TEXT("scene bridge finds the illegal enemy actor"), SceneEnemyActor);
	if (SceneBoard && ScenePartyTargetActor && SceneEnemyActor)
	{
		SceneController->InitInputSystem();
		SceneController->SetBattleWorldProjectionOverrideForTest(true);
		SceneController->PlayerTick(0.0f);
		const FVector CenterWorldPosition = ScenePartyTargetActor->GetBattleVisualComponent()->Bounds.Origin;
		const FVector2D ExpectedCenterProjection(CenterWorldPosition.X + CenterWorldPosition.Z, CenterWorldPosition.Y);
		TestTrue(TEXT("PlayerTick registers the actor visual-center arrow projection"), SceneBoard->HasBattleUnitScreenPositionForTest(ScenePartyTargetUnitId));
		TestEqual(TEXT("PlayerTick arrow center follows the visual bounds origin"), SceneBoard->GetBattleUnitScreenPositionForTest(ScenePartyTargetUnitId), ExpectedCenterProjection);
		TestFalse(TEXT("PlayerTick no longer registers actor-foot projections for fixed resource HUDs"), SceneBoard->HasProjectedUnitHudScreenPositionForTest(ScenePartyTargetUnitId));
		UGameXXKBattleUnitHudWidget* const ScenePartyHud = SceneBoard->GetProjectedUnitHudForTest(ScenePartyTargetUnitId);
		TestNotNull(TEXT("the fixed resource HUD exists without an actor-foot projection"), ScenePartyHud);
		TestEqual(TEXT("the fixed resource HUD stays visible without an actor-foot projection"),
			ScenePartyHud ? ScenePartyHud->GetVisibility() : ESlateVisibility::Collapsed,
			ESlateVisibility::SelfHitTestInvisible);
		SceneController->PlayerTick(0.0f);
		TestTrue(TEXT("a following PlayerTick re-registers the center projection after Board clear"), SceneBoard->HasBattleUnitScreenPositionForTest(ScenePartyTargetUnitId));
		TestFalse(TEXT("a following PlayerTick still leaves fixed HUD placement independent of actor feet"), SceneBoard->HasProjectedUnitHudScreenPositionForTest(ScenePartyTargetUnitId));

		TestTrue(TEXT("scene bridge card enters manual targeting"), SceneBoard->ClickCardInHand(SceneCardInstanceId));
		SceneController->SetBattleSceneCursorHitOverrideForTest(nullptr);
		TestTrue(TEXT("simulated empty physical left-click is consumed by the controller"),
			SceneController->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 1.0f)));
		TestTrue(TEXT("simulated empty physical left-click keeps card targeting active"), SceneBoard->IsCardTargetingActive());
		SceneController->ClearBattleSceneCursorHitOverrideForTest();
		const FVector2D PointerPosition(812.0f, 468.0f);
		TestTrue(TEXT("controller forwards arrow pointer movement while card targeting is active"), SceneController->UpdateBattleTargetingPointerForTest(PointerPosition));
		TestEqual(TEXT("card arrow pointer follows the controller cursor update"), SceneBoard->GetTargetingPointerPositionForTest(), PointerPosition);
		SceneController->RefreshPlayerFlowWidgetsForTest();
		TestTrue(TEXT("board exposes the selected friendly unit as a legal card target"), SceneBoard->IsTargetUnitHighlighted(ScenePartyTargetUnitId));
		TestFalse(TEXT("board rejects the enemy for the friendly-target card"), SceneBoard->IsTargetUnitHighlighted(SceneEnemyUnitId));
		TestTrue(TEXT("controller bridge applies legal-target outline to the scene actor"), ScenePartyTargetActor->IsCardTargetHighlighted());
		TestFalse(TEXT("controller bridge clears the illegal-target outline from the scene actor"), SceneEnemyActor->IsCardTargetHighlighted());
		TestEqual(TEXT("physical-click target keeps the stable UnitId despite a wrong scene index"), ScenePartyTargetActor->GetUnitId(), ScenePartyTargetUnitId);
		SceneController->SetBattleSceneCursorHitOverrideForTest(ScenePartyTargetActor);
		TestTrue(TEXT("simulated physical unit-click is consumed by the controller"),
			SceneController->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::LeftMouseButton, IE_Released, 1.0f)));
		TestFalse(TEXT("simulated physical unit-click submits the friendly scene actor by stable UnitId"), SceneBoard->IsCardTargetingActive());
		SceneController->ClearBattleSceneCursorHitOverrideForTest();
	}

	for (AGameXXKBattleSceneUnitActor* Actor : SpawnedSceneActors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}
	if (SceneBattleCamera)
	{
		SceneBattleCamera->Destroy();
	}
	SceneController->Destroy();
	return true;
}

#endif
