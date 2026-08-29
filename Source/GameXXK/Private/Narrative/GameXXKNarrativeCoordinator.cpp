#include "Narrative/GameXXKNarrativeCoordinator.h"

#include "GameXXKMVPRules.h"
#include "Narrative/GameXXKNarrativeSequenceAsset.h"
#include "Narrative/GameXXKNarrativeSequenceRules.h"

namespace GameXXKNarrativeCoordinatorPrivate
{
	constexpr int32 MaximumDispatchDepth = 256;

	bool SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	void ClearError(FString* OutError)
	{
		if (OutError)
		{
			OutError->Reset();
		}
	}
}

void UGameXXKNarrativeCoordinator::BeginDestroy()
{
	PauseAndRelease();
	ClearDialogueStartDelegates();
	CandidateValidator.Unbind();
	Super::BeginDestroy();
}

void UGameXXKNarrativeCoordinator::BindState(
	FGameXXKRuntimeState& InRuntimeState,
	FGameXXKNarrativeSequenceSessionState& InSessionState)
{
	RuntimeState = &InRuntimeState;
	SessionState = &InSessionState;
}

bool UGameXXKNarrativeCoordinator::RegisterExecutor(
	const FName CommandType,
	TSharedRef<IGameXXKNarrativeCommandExecutor> Executor)
{
	if (CommandType.IsNone() || Executors.Contains(CommandType) || !Executor->Supports(CommandType))
	{
		return false;
	}
	Executors.Add(CommandType, Executor);
	return true;
}

void UGameXXKNarrativeCoordinator::SetDialogueStartDelegate(
	FGameXXKNarrativeDialogueStartRequest Delegate)
{
	SetDialogueStartDelegate(
		EGameXXKNarrativeDialogueHost::LegacyNpc3D,
		MoveTemp(Delegate));
	if (DialogueHost == EGameXXKNarrativeDialogueHost::None)
	{
		SelectDialogueHost(EGameXXKNarrativeDialogueHost::LegacyNpc3D, nullptr);
	}
}

void UGameXXKNarrativeCoordinator::SetDialogueStartDelegate(
	const EGameXXKNarrativeDialogueHost Host,
	FGameXXKNarrativeDialogueStartRequest Delegate)
{
	if (FGameXXKNarrativeDialogueStartRequest* const Target =
		ResolveDialogueStartDelegate(Host))
	{
		*Target = MoveTemp(Delegate);
	}
}

void UGameXXKNarrativeCoordinator::ClearDialogueStartDelegate(
	const EGameXXKNarrativeDialogueHost Host)
{
	if (FGameXXKNarrativeDialogueStartRequest* const Target =
		ResolveDialogueStartDelegate(Host))
	{
		Target->Unbind();
	}
	if (DialogueHost == Host)
	{
		DialogueHost = EGameXXKNarrativeDialogueHost::None;
		InvalidateDialogueCompletion();
	}
	if (!SessionState || !SessionState->bActive)
	{
		ActiveDialogueHostAffinity = EGameXXKNarrativeDialogueHost::None;
	}
}

void UGameXXKNarrativeCoordinator::ClearDialogueStartDelegates()
{
	DesktopDialogueStartDelegate.Unbind();
	LegacyDialogueStartDelegate.Unbind();
	DialogueHost = EGameXXKNarrativeDialogueHost::None;
	InvalidateDialogueCompletion();
	if (!SessionState || !SessionState->bActive)
	{
		ActiveDialogueHostAffinity = EGameXXKNarrativeDialogueHost::None;
	}
}

bool UGameXXKNarrativeCoordinator::SelectDialogueHost(
	const EGameXXKNarrativeDialogueHost Host,
	FString* OutError)
{
	using namespace GameXXKNarrativeCoordinatorPrivate;
	if (Host == EGameXXKNarrativeDialogueHost::None)
	{
		return SetError(OutError, TEXT("Narrative dialogue host must be explicit."));
	}
	if (SessionState
		&& SessionState->bActive
		&& ActiveDialogueHostAffinity != EGameXXKNarrativeDialogueHost::None
		&& ActiveDialogueHostAffinity != Host)
	{
		return SetError(OutError, TEXT("The active narrative sequence has a different dialogue-host affinity."));
	}
	if (DialogueHost == Host)
	{
		if (SessionState
			&& SessionState->bActive
			&& ActiveDialogueHostAffinity == EGameXXKNarrativeDialogueHost::None)
		{
			ActiveDialogueHostAffinity = Host;
		}
		ClearError(OutError);
		return true;
	}
	if (SessionState
		&& SessionState->bActive
		&& ActiveDialogueHostAffinity == EGameXXKNarrativeDialogueHost::None)
	{
		ActiveDialogueHostAffinity = Host;
	}
	DialogueHost = Host;
	ClearError(OutError);
	return true;
}

void UGameXXKNarrativeCoordinator::SetCandidateValidator(
	FGameXXKNarrativeCandidateValidator Delegate)
{
	CandidateValidator = MoveTemp(Delegate);
}

bool UGameXXKNarrativeCoordinator::StartSequence(
	UGameXXKNarrativeSequenceAsset& Asset,
	const FGameXXKNarrativeStartContext& Context,
	FString* OutError)
{
	using namespace GameXXKNarrativeCoordinatorPrivate;
	if (!RuntimeState || !SessionState)
	{
		return SetError(OutError, TEXT("Narrative coordinator has no bound state."));
	}
	if (SessionState->bActive)
	{
		return SetError(OutError, TEXT("A narrative sequence is already active."));
	}
	if (ActiveAsset)
	{
		CancelPendingExecutor();
		InvalidateDialogueCompletion();
		ActiveAsset = nullptr;
	}
	ActiveDialogueHostAffinity = EGameXXKNarrativeDialogueHost::None;

	FGameXXKNarrativeSequenceSessionState Candidate = *SessionState;
	FGameXXKNarrativeRequest Request;
	if (!FGameXXKNarrativeSequenceRules::Start(Asset, Context, Candidate, Request, OutError))
	{
		return false;
	}
	*SessionState = MoveTemp(Candidate);
	ActiveAsset = &Asset;
	ActiveDialogueHostAffinity = DialogueHost;
	bInputTokenHeld = SessionState->bActive;
	return DispatchRequest(Request, OutError);
}

bool UGameXXKNarrativeCoordinator::Resume(FString* OutError)
{
	using namespace GameXXKNarrativeCoordinatorPrivate;
	if (!RuntimeState || !SessionState || !ActiveAsset || !SessionState->bActive)
	{
		return SetError(OutError, TEXT("No narrative sequence can be resumed."));
	}
	FGameXXKNarrativeSequenceSessionState Candidate = *SessionState;
	FGameXXKNarrativeRequest Request;
	if (!FGameXXKNarrativeSequenceRules::Resume(*ActiveAsset, Candidate, Request, OutError))
	{
		return false;
	}
	*SessionState = MoveTemp(Candidate);
	if (ActiveDialogueHostAffinity == EGameXXKNarrativeDialogueHost::None)
	{
		ActiveDialogueHostAffinity = DialogueHost;
	}
	bInputTokenHeld = true;
	return DispatchRequest(Request, OutError);
}

bool UGameXXKNarrativeCoordinator::ResumeSequence(
	UGameXXKNarrativeSequenceAsset& Asset,
	FString* OutError)
{
	using namespace GameXXKNarrativeCoordinatorPrivate;
	if (!RuntimeState || !SessionState || !SessionState->bActive)
	{
		return SetError(OutError, TEXT("No narrative sequence can be restored."));
	}
	if (SessionState->SequenceId != Asset.SequenceId
		|| SessionState->SequenceVersion != Asset.SequenceVersion)
	{
		return SetError(OutError, TEXT("Narrative restore asset does not match the saved session."));
	}
	if (ActiveAsset && ActiveAsset != &Asset)
	{
		CancelPendingExecutor();
	}
	ActiveAsset = &Asset;
	bInputTokenHeld = true;
	return Resume(OutError);
}

bool UGameXXKNarrativeCoordinator::DispatchRequest(
	const FGameXXKNarrativeRequest& Request,
	FString* OutError)
{
	using namespace GameXXKNarrativeCoordinatorPrivate;
	if (++DispatchDepth > MaximumDispatchDepth)
	{
		--DispatchDepth;
		ReleaseInputToken();
		return SetError(OutError, TEXT("Narrative coordinator exceeded its dispatch-depth limit."));
	}

	bool bResult = false;
	switch (Request.Type)
	{
	case EGameXXKNarrativeRequestType::Command:
		bResult = DispatchCommand(Request, OutError);
		break;

	case EGameXXKNarrativeRequestType::Wait:
		ClearError(OutError);
		bResult = true;
		break;

	case EGameXXKNarrativeRequestType::Dialogue:
	{
		FGameXXKNarrativeDialogueStartRequest* const StartDelegate =
			ResolveDialogueStartDelegate(DialogueHost);
		if (!StartDelegate || !StartDelegate->IsBound())
		{
			SessionState->PauseReason = TEXT("The selected narrative dialogue host is not bound.");
			ReleaseInputToken();
			bResult = SetError(OutError, SessionState->PauseReason);
			break;
		}
		ActiveDialogueCompletionGeneration = ++DialogueCompletionGenerationCounter;
		FGameXXKNarrativeDialogueCompleted Completion =
			FGameXXKNarrativeDialogueCompleted::CreateUObject(
				this,
				&UGameXXKNarrativeCoordinator::HandleDialogueCompleted,
				ActiveDialogueCompletionGeneration);
#if WITH_DEV_AUTOMATION_TESTS
		LastIssuedDialogueCompletionForTest = Completion;
#endif
		StartDelegate->Execute(
			Request.DialogueId,
			MoveTemp(Completion));
		ClearError(OutError);
		bResult = true;
		break;
	}

	case EGameXXKNarrativeRequestType::Ended:
		ReleaseInputToken();
		InvalidateDialogueCompletion();
		ActiveDialogueHostAffinity = EGameXXKNarrativeDialogueHost::None;
		ActiveAsset = nullptr;
		ClearError(OutError);
		bResult = true;
		break;

	default:
		ReleaseInputToken();
		bResult = SetError(OutError, TEXT("Narrative coordinator received an empty request."));
		break;
	}
	--DispatchDepth;
	return bResult;
}

bool UGameXXKNarrativeCoordinator::DispatchCommand(
	const FGameXXKNarrativeRequest& Request,
	FString* OutError)
{
	using namespace GameXXKNarrativeCoordinatorPrivate;
	TSharedPtr<IGameXXKNarrativeCommandExecutor> Executor = Executors.FindRef(Request.Command.CommandType);
	if (!Executor || !Executor->Supports(Request.Command.CommandType))
	{
		FGameXXKNarrativeSequenceSessionState Candidate = *SessionState;
		FGameXXKNarrativeRequest NextRequest;
		const bool bAdvanced = FGameXXKNarrativeSequenceRules::CompleteCommand(
			*ActiveAsset,
			EGameXXKNarrativeCommandStatus::Failed,
			Candidate,
			NextRequest,
			OutError);
		*SessionState = MoveTemp(Candidate);
		if (!bAdvanced)
		{
			ReleaseInputToken();
			return false;
		}
		return DispatchRequest(NextRequest, OutError);
	}

	FGameXXKRuntimeState RuntimeCandidate = *RuntimeState;
	FGameXXKNarrativeSequenceSessionState SessionCandidate = *SessionState;
	const FGameXXKNarrativeCommandResult Result = Executor->Execute(Request.Command, RuntimeCandidate);
	if (Result.Status == EGameXXKNarrativeCommandStatus::Pending)
	{
		PendingCommandType = Request.Command.CommandType;
		ClearError(OutError);
		return true;
	}
	if (Result.Status == EGameXXKNarrativeCommandStatus::Failed)
	{
		RuntimeCandidate = *RuntimeState;
	}

	FGameXXKNarrativeRequest NextRequest;
	const bool bAdvanced = FGameXXKNarrativeSequenceRules::CompleteCommand(
		*ActiveAsset,
		Result.Status,
		SessionCandidate,
		NextRequest,
		OutError);
	if (!bAdvanced)
	{
		*SessionState = MoveTemp(SessionCandidate);
		ReleaseInputToken();
		return false;
	}
	return CommitAdvancedCandidate(
		MoveTemp(RuntimeCandidate),
		MoveTemp(SessionCandidate),
		NextRequest,
		OutError);
}

bool UGameXXKNarrativeCoordinator::CompletePendingCommand(
	const EGameXXKNarrativeCommandStatus Status,
	FString* OutError)
{
	using namespace GameXXKNarrativeCoordinatorPrivate;
	if (!RuntimeState || !SessionState || !ActiveAsset || PendingCommandType.IsNone())
	{
		return SetError(OutError, TEXT("No pending narrative command exists."));
	}
	if (Status == EGameXXKNarrativeCommandStatus::Pending)
	{
		ClearError(OutError);
		return true;
	}

	FGameXXKRuntimeState RuntimeCandidate = *RuntimeState;
	FGameXXKNarrativeSequenceSessionState SessionCandidate = *SessionState;
	FGameXXKNarrativeRequest NextRequest;
	const bool bAdvanced = FGameXXKNarrativeSequenceRules::CompleteCommand(
		*ActiveAsset,
		Status,
		SessionCandidate,
		NextRequest,
		OutError);
	PendingCommandType = NAME_None;
	if (!bAdvanced)
	{
		*SessionState = MoveTemp(SessionCandidate);
		ReleaseInputToken();
		return false;
	}
	return CommitAdvancedCandidate(
		MoveTemp(RuntimeCandidate),
		MoveTemp(SessionCandidate),
		NextRequest,
		OutError);
}

bool UGameXXKNarrativeCoordinator::CompletePendingWait(FString* OutError)
{
	using namespace GameXXKNarrativeCoordinatorPrivate;
	if (!SessionState || !ActiveAsset || !SessionState->bActive)
	{
		return SetError(OutError, TEXT("No pending narrative wait exists."));
	}
	FGameXXKNarrativeSequenceSessionState Candidate = *SessionState;
	FGameXXKNarrativeRequest NextRequest;
	if (!FGameXXKNarrativeSequenceRules::CompleteWait(
		*ActiveAsset,
		Candidate,
		NextRequest,
		OutError))
	{
		return false;
	}
	*SessionState = MoveTemp(Candidate);
	return DispatchRequest(NextRequest, OutError);
}

bool UGameXXKNarrativeCoordinator::CommitAdvancedCandidate(
	FGameXXKRuntimeState&& RuntimeCandidate,
	FGameXXKNarrativeSequenceSessionState&& SessionCandidate,
	const FGameXXKNarrativeRequest& NextRequest,
	FString* OutError)
{
	using namespace GameXXKNarrativeCoordinatorPrivate;
	if (CandidateValidator.IsBound())
	{
		FString ValidationError;
		if (!CandidateValidator.Execute(RuntimeCandidate, ValidationError))
		{
			ReleaseInputToken();
			return SetError(OutError, ValidationError.IsEmpty()
				? TEXT("Narrative candidate validation failed.")
				: ValidationError);
		}
	}
	*RuntimeState = MoveTemp(RuntimeCandidate);
	*SessionState = MoveTemp(SessionCandidate);
	return DispatchRequest(NextRequest, OutError);
}

void UGameXXKNarrativeCoordinator::HandleDialogueCompleted(
	const FName OutcomeId,
	const uint64 CompletionGeneration)
{
	if (CompletionGeneration == 0
		|| CompletionGeneration != ActiveDialogueCompletionGeneration
		|| !SessionState
		|| !ActiveAsset
		|| !SessionState->bActive)
	{
		return;
	}
	ActiveDialogueCompletionGeneration = 0;
	FGameXXKNarrativeSequenceSessionState Candidate = *SessionState;
	FGameXXKNarrativeRequest NextRequest;
	FString Error;
	if (!FGameXXKNarrativeSequenceRules::CompleteDialogue(
		*ActiveAsset,
		OutcomeId,
		Candidate,
		NextRequest,
		&Error))
	{
		SessionState->PauseReason = Error;
		ReleaseInputToken();
		return;
	}
	*SessionState = MoveTemp(Candidate);
	DispatchRequest(NextRequest, nullptr);
}

void UGameXXKNarrativeCoordinator::PauseAndRelease()
{
	CancelPendingExecutor();
	InvalidateDialogueCompletion();
	ReleaseInputToken();
}

void UGameXXKNarrativeCoordinator::CancelForMapTravel()
{
	PauseAndRelease();
}

bool UGameXXKNarrativeCoordinator::IsInputTokenHeld() const
{
	return bInputTokenHeld;
}

void UGameXXKNarrativeCoordinator::ReleaseInputToken()
{
	bInputTokenHeld = false;
}

void UGameXXKNarrativeCoordinator::CancelPendingExecutor()
{
	if (TSharedPtr<IGameXXKNarrativeCommandExecutor> Executor = Executors.FindRef(PendingCommandType))
	{
		Executor->CancelPending();
	}
	PendingCommandType = NAME_None;
}

void UGameXXKNarrativeCoordinator::InvalidateDialogueCompletion()
{
	++DialogueCompletionGenerationCounter;
	ActiveDialogueCompletionGeneration = 0;
}

FGameXXKNarrativeDialogueStartRequest*
UGameXXKNarrativeCoordinator::ResolveDialogueStartDelegate(
	const EGameXXKNarrativeDialogueHost Host)
{
	switch (Host)
	{
	case EGameXXKNarrativeDialogueHost::Desktop2D:
		return &DesktopDialogueStartDelegate;
	case EGameXXKNarrativeDialogueHost::LegacyNpc3D:
		return &LegacyDialogueStartDelegate;
	default:
		return nullptr;
	}
}
