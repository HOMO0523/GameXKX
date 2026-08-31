#include "Guide/GameXXKTutorial01GuideHost.h"

#include "Guide/GameXXKGuideCoordinator.h"
#include "Guide/GameXXKGuideTargetRegistry.h"
#include "UI/GameXXKGuideOverlayWidget.h"

namespace GameXXKTutorial01GuideHostPrivate
{
	const FName BattleOpenedEvent(TEXT("Event.Battle.Opened"));
	const FName ContinueAction(TEXT("Action.Guide.Continue"));
	const FName ContinueEvent(TEXT("Event.Tutorial01.Continue"));
	const FName EndTurnResolvedEvent(TEXT("Event.Battle.EndTurnResolved"));
	const FName PlayerTurnReadyEvent(TEXT("Event.Tutorial01.PlayerTurnReady"));
	const FName EndTurnStep(TEXT("Guide.Battle.Tutorial01.EndTurn"));
	constexpr float PlayerTurnWatchdogSeconds = 15.0f;
}

void UGameXXKTutorial01GuideHost::BeginDestroy()
{
	Cancel(TEXT("Tutorial guide host destroyed."));
	Super::BeginDestroy();
}

void UGameXXKTutorial01GuideHost::Bind(
	FGameXXKGuideProgress& InProgress,
	FGameXXKGuideTargetRegistry& InRegistry,
	UGameXXKGuideOverlayWidget& InOverlay,
	UGameXXKGuideAsset& InAsset,
	FGameXXKTutorial01GuideFailed OnFailed)
{
	Cancel();
	Progress = &InProgress;
	Registry = &InRegistry;
	Overlay = &InOverlay;
	Asset = &InAsset;
	FailedDelegate = MoveTemp(OnFailed);
	Coordinator = NewObject<UGameXXKGuideCoordinator>(this);
	Coordinator->Bind(InProgress, InRegistry, &InOverlay);
	Coordinator->SetFaultDelegate(
		FGameXXKGuideCoordinatorFault::CreateUObject(
			this,
			&UGameXXKTutorial01GuideHost::HandleCoordinatorFault));
}

bool UGameXXKTutorial01GuideHost::Start()
{
	using namespace GameXXKTutorial01GuideHostPrivate;
	if (!Progress || !Registry || !Coordinator || !Overlay.IsValid() || !Asset.IsValid()
		|| Progress->Preference != EGameXXKGuidePreference::NewPlayer)
	{
		return false;
	}
	bFailed = false;
	bSuspendedForEnemyTurn = false;
	PlayerTurnWatchdogElapsed = 0.0f;
	if (!GuideEventHandle.IsValid())
	{
		GuideEventHandle = Registry->OnGuideEvent().AddUObject(
			this,
			&UGameXXKTutorial01GuideHost::HandleGuideEvent);
	}
	FString Error;
	if (!Coordinator->StartGuide(*Asset.Get(), BattleOpenedEvent, &Error))
	{
		if (!bFailed && !Error.IsEmpty())
		{
			Fail(Error);
		}
		return false;
	}
	bStarted = true;
	ApplyActionGate();
	return true;
}

bool UGameXXKTutorial01GuideHost::HandleContinue()
{
	using namespace GameXXKTutorial01GuideHostPrivate;
	if (!bStarted || bSuspendedForEnemyTurn || !Registry
		|| !Registry->IsActionAllowed(ContinueAction))
	{
		return false;
	}
	return Registry->EmitEvent(ContinueEvent);
}

void UGameXXKTutorial01GuideHost::HandleGuideEvent(const FName EventId)
{
	using namespace GameXXKTutorial01GuideHostPrivate;
	if (!bStarted || bFailed || !Progress || !Coordinator)
	{
		return;
	}
	if (EventId == EndTurnResolvedEvent
		&& Progress->ActiveGuideStepId == EndTurnStep)
	{
		bSuspendedForEnemyTurn = true;
		PlayerTurnWatchdogElapsed = 0.0f;
		Coordinator->SuspendPresentation();
		ClearActionGate();
		return;
	}
	if (bSuspendedForEnemyTurn && EventId != PlayerTurnReadyEvent)
	{
		return;
	}

	FString Error;
	const bool bHandled = Coordinator->HandleEvent(EventId, &Error);
	if (!bHandled)
	{
		if (!Error.IsEmpty())
		{
			Fail(Error);
		}
		return;
	}
	if (EventId == PlayerTurnReadyEvent)
	{
		bSuspendedForEnemyTurn = false;
		PlayerTurnWatchdogElapsed = 0.0f;
	}
	if (Progress->ActiveGuideId.IsNone())
	{
		Finish();
		return;
	}
	ApplyActionGate();
}

void UGameXXKTutorial01GuideHost::Tick(
	const float DeltaSeconds,
	const bool bPaused,
	const bool bBattleBusy,
	const EGameXXKCardBattlePhase Phase)
{
	using namespace GameXXKTutorial01GuideHostPrivate;
	if (bStarted && !bSuspendedForEnemyTurn && !bFailed && Coordinator)
	{
		FString RefreshError;
		if (!Coordinator->RefreshTarget(&RefreshError) && !RefreshError.IsEmpty())
		{
			Fail(RefreshError);
			return;
		}
	}
	if (!bStarted || !bSuspendedForEnemyTurn || bFailed
		|| DeltaSeconds <= 0.0f
		|| bPaused
		|| bBattleBusy
		|| Phase == EGameXXKCardBattlePhase::Enemy)
	{
		return;
	}
	PlayerTurnWatchdogElapsed += DeltaSeconds;
	if (PlayerTurnWatchdogElapsed >= PlayerTurnWatchdogSeconds)
	{
		Fail(TEXT("Tutorial guide timed out waiting for the next player turn."));
	}
}

void UGameXXKTutorial01GuideHost::Cancel(const FString& Diagnostic)
{
	if (Registry && GuideEventHandle.IsValid())
	{
		Registry->OnGuideEvent().Remove(GuideEventHandle);
	}
	GuideEventHandle.Reset();
	ClearActionGate();
	if (Coordinator)
	{
		Coordinator->Cancel(Diagnostic);
	}
	else if (UGameXXKGuideOverlayWidget* OverlayWidget = Overlay.Get())
	{
		OverlayWidget->DismissGuide();
	}
	bStarted = false;
	bSuspendedForEnemyTurn = false;
	PlayerTurnWatchdogElapsed = 0.0f;
}

void UGameXXKTutorial01GuideHost::ApplyActionGate()
{
	if (!Registry || !Coordinator || !bStarted || bSuspendedForEnemyTurn)
	{
		return;
	}
	const TWeakObjectPtr<UGameXXKTutorial01GuideHost> WeakThis(this);
	Registry->SetActionGate(this, [WeakThis](const FName ActionId)
	{
		const UGameXXKTutorial01GuideHost* Host = WeakThis.Get();
		return !Host || !Host->Coordinator
			|| Host->Coordinator->CanExecuteAction(ActionId);
	});
}

void UGameXXKTutorial01GuideHost::ClearActionGate()
{
	if (Registry)
	{
		Registry->ClearActionGate(this);
	}
}

void UGameXXKTutorial01GuideHost::Finish()
{
	if (Registry && GuideEventHandle.IsValid())
	{
		Registry->OnGuideEvent().Remove(GuideEventHandle);
	}
	GuideEventHandle.Reset();
	ClearActionGate();
	if (Coordinator)
	{
		Coordinator->SuspendPresentation();
	}
	bStarted = false;
	bSuspendedForEnemyTurn = false;
	PlayerTurnWatchdogElapsed = 0.0f;
}

void UGameXXKTutorial01GuideHost::Fail(const FString& Diagnostic)
{
	if (bFailed)
	{
		return;
	}
	bFailed = true;
	if (Registry && GuideEventHandle.IsValid())
	{
		Registry->OnGuideEvent().Remove(GuideEventHandle);
	}
	GuideEventHandle.Reset();
	ClearActionGate();
	if (Coordinator)
	{
		Coordinator->Cancel(Diagnostic);
	}
	bStarted = false;
	bSuspendedForEnemyTurn = false;
	PlayerTurnWatchdogElapsed = 0.0f;
	if (FailedDelegate.IsBound())
	{
		FailedDelegate.Execute(Diagnostic);
	}
}

void UGameXXKTutorial01GuideHost::HandleCoordinatorFault(
	const FString& Diagnostic)
{
	Fail(Diagnostic);
}
