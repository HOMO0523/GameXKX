#include "Narrative/GameXXKNarrativeSequenceRules.h"

#include "Narrative/GameXXKNarrativeSequenceAsset.h"

namespace GameXXKNarrativeSequenceRulesPrivate
{
	constexpr int32 MaximumImmediateSteps = 256;

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

	bool HasCompleteContext(const FGameXXKNarrativeStartContext& Context)
	{
		if (Context.StoryId.IsNone()
			|| Context.StoryVersion <= 0
			|| Context.TaskId.IsNone()
			|| Context.StepId.IsNone()
			|| Context.StageContractId.IsNone())
		{
			return false;
		}
		for (const TPair<FName, FName>& Role : Context.CharacterIdByRole)
		{
			if (Role.Key.IsNone() || Role.Value.IsNone())
			{
				return false;
			}
		}
		return true;
	}

	bool SessionMatchesAsset(
		const UGameXXKNarrativeSequenceAsset& Asset,
		const FGameXXKNarrativeSequenceSessionState& Session,
		FString* OutError)
	{
		if (!Session.bActive)
		{
			return SetError(OutError, TEXT("Narrative sequence session is not active."));
		}
		if (Session.SequenceId != Asset.SequenceId || Session.SequenceVersion != Asset.SequenceVersion)
		{
			return SetError(OutError, TEXT("Narrative sequence asset does not match the active session."));
		}
		return true;
	}

	void ClearActiveContext(FGameXXKNarrativeSequenceSessionState& Session)
	{
		Session.bActive = false;
		Session.StoryId = NAME_None;
		Session.StoryVersion = 0;
		Session.TaskId = NAME_None;
		Session.StepId = NAME_None;
		Session.SequenceId = NAME_None;
		Session.SequenceVersion = 0;
		Session.StageContractId = NAME_None;
		Session.CurrentSequenceStepId = NAME_None;
		Session.AwaitedDialogueId = NAME_None;
		Session.LastOutcomeId = NAME_None;
		Session.CharacterIdByRole.Reset();
		Session.PauseReason.Reset();
	}

	bool AdvanceToRequest(
		const UGameXXKNarrativeSequenceAsset& Asset,
		FGameXXKNarrativeSequenceSessionState& Session,
		FGameXXKNarrativeRequest& OutRequest,
		FString* OutError)
	{
		for (int32 ImmediateIndex = 0; ImmediateIndex < MaximumImmediateSteps; ++ImmediateIndex)
		{
			const FGameXXKNarrativeSequenceStepDefinition* Step = Asset.FindStep(Session.CurrentSequenceStepId);
			if (!Step)
			{
				return SetError(OutError, FString::Printf(
					TEXT("Narrative sequence step does not exist: %s"),
					*Session.CurrentSequenceStepId.ToString()));
			}

			FGameXXKNarrativeRequest CandidateRequest;
			CandidateRequest.SequenceStepId = Step->StepId;
			switch (Step->Type)
			{
			case EGameXXKNarrativeStepType::Command:
			{
				const FName CommandKey = FGameXXKNarrativeSequenceRules::MakeCommandKey(
					Session.StoryId,
					Session.TaskId,
					Session.StepId,
					Step->Command.CommandId);
				if (CommandKey.IsNone())
				{
					return SetError(OutError, TEXT("Narrative command context is incomplete."));
				}
				if (Session.ExecutedCommandKeys.Contains(CommandKey))
				{
					Session.CurrentSequenceStepId = Step->NextStepId;
					continue;
				}
				CandidateRequest.Type = EGameXXKNarrativeRequestType::Command;
				CandidateRequest.Command = Step->Command;
				OutRequest = MoveTemp(CandidateRequest);
				ClearError(OutError);
				return true;
			}

			case EGameXXKNarrativeStepType::Wait:
				CandidateRequest.Type = EGameXXKNarrativeRequestType::Wait;
				CandidateRequest.WaitType = Step->WaitType;
				CandidateRequest.WaitArguments = Step->WaitArguments;
				OutRequest = MoveTemp(CandidateRequest);
				ClearError(OutError);
				return true;

			case EGameXXKNarrativeStepType::Dialogue:
				Session.AwaitedDialogueId = Step->DialogueId;
				CandidateRequest.Type = EGameXXKNarrativeRequestType::Dialogue;
				CandidateRequest.DialogueId = Step->DialogueId;
				OutRequest = MoveTemp(CandidateRequest);
				ClearError(OutError);
				return true;

			case EGameXXKNarrativeStepType::BranchOnOutcome:
			{
				const FName* Target = Step->OutcomeToStepId.Find(Session.LastOutcomeId);
				if (!Target || Target->IsNone())
				{
					return SetError(OutError, FString::Printf(
						TEXT("Narrative outcome has no branch: %s"),
						*Session.LastOutcomeId.ToString()));
				}
				Session.CurrentSequenceStepId = *Target;
				Session.LastOutcomeId = NAME_None;
				continue;
			}

			case EGameXXKNarrativeStepType::End:
				CandidateRequest.Type = EGameXXKNarrativeRequestType::Ended;
				CandidateRequest.bEnded = true;
				ClearActiveContext(Session);
				OutRequest = MoveTemp(CandidateRequest);
				ClearError(OutError);
				return true;

			default:
				return SetError(OutError, TEXT("Narrative sequence step has an unsupported type."));
			}
		}
		return SetError(OutError, TEXT("Narrative sequence exceeded the immediate-step limit."));
	}
}

bool FGameXXKNarrativeSequenceRules::Start(
	const UGameXXKNarrativeSequenceAsset& Asset,
	const FGameXXKNarrativeStartContext& Context,
	FGameXXKNarrativeSequenceSessionState& InOutSession,
	FGameXXKNarrativeRequest& OutRequest,
	FString* OutError)
{
	using namespace GameXXKNarrativeSequenceRulesPrivate;
	if (InOutSession.bActive)
	{
		return SetError(OutError, TEXT("A narrative sequence is already active."));
	}
	if (Asset.SequenceId.IsNone()
		|| Asset.SequenceVersion <= 0
		|| Asset.EntryStepId.IsNone()
		|| Asset.StageContractId.IsNone())
	{
		return SetError(OutError, TEXT("Narrative sequence asset identity is invalid."));
	}
	if (!HasCompleteContext(Context) || Context.StageContractId != Asset.StageContractId)
	{
		return SetError(OutError, TEXT("Narrative start context is incomplete or uses the wrong stage contract."));
	}

	FGameXXKNarrativeSequenceSessionState Candidate;
	Candidate.ExecutedCommandKeys = InOutSession.ExecutedCommandKeys;
	Candidate.bActive = true;
	Candidate.StoryId = Context.StoryId;
	Candidate.StoryVersion = Context.StoryVersion;
	Candidate.TaskId = Context.TaskId;
	Candidate.StepId = Context.StepId;
	Candidate.SequenceId = Asset.SequenceId;
	Candidate.SequenceVersion = Asset.SequenceVersion;
	Candidate.StageContractId = Context.StageContractId;
	Candidate.CurrentSequenceStepId = Asset.EntryStepId;
	Candidate.CharacterIdByRole = Asset.DefaultCharacterIdByRole;
	for (const TPair<FName, FName>& Role : Context.CharacterIdByRole)
	{
		Candidate.CharacterIdByRole.Add(Role.Key, Role.Value);
	}

	FGameXXKNarrativeRequest CandidateRequest;
	if (!AdvanceToRequest(Asset, Candidate, CandidateRequest, OutError))
	{
		return false;
	}
	InOutSession = MoveTemp(Candidate);
	OutRequest = MoveTemp(CandidateRequest);
	return true;
}

bool FGameXXKNarrativeSequenceRules::Resume(
	const UGameXXKNarrativeSequenceAsset& Asset,
	FGameXXKNarrativeSequenceSessionState& InOutSession,
	FGameXXKNarrativeRequest& OutRequest,
	FString* OutError)
{
	using namespace GameXXKNarrativeSequenceRulesPrivate;
	if (!SessionMatchesAsset(Asset, InOutSession, OutError))
	{
		return false;
	}
	FGameXXKNarrativeSequenceSessionState Candidate = InOutSession;
	Candidate.PauseReason.Reset();
	FGameXXKNarrativeRequest CandidateRequest;
	if (!AdvanceToRequest(Asset, Candidate, CandidateRequest, OutError))
	{
		return false;
	}
	InOutSession = MoveTemp(Candidate);
	OutRequest = MoveTemp(CandidateRequest);
	return true;
}

bool FGameXXKNarrativeSequenceRules::CompleteCommand(
	const UGameXXKNarrativeSequenceAsset& Asset,
	const EGameXXKNarrativeCommandStatus Status,
	FGameXXKNarrativeSequenceSessionState& InOutSession,
	FGameXXKNarrativeRequest& OutRequest,
	FString* OutError)
{
	using namespace GameXXKNarrativeSequenceRulesPrivate;
	if (!SessionMatchesAsset(Asset, InOutSession, OutError))
	{
		return false;
	}
	const FGameXXKNarrativeSequenceStepDefinition* Step = Asset.FindStep(InOutSession.CurrentSequenceStepId);
	if (!Step || Step->Type != EGameXXKNarrativeStepType::Command)
	{
		return SetError(OutError, TEXT("Current narrative step is not a command."));
	}

	FGameXXKNarrativeSequenceSessionState Candidate = InOutSession;
	if (Status == EGameXXKNarrativeCommandStatus::Pending)
	{
		Candidate.PauseReason.Reset();
		FGameXXKNarrativeRequest CandidateRequest;
		if (!AdvanceToRequest(Asset, Candidate, CandidateRequest, OutError))
		{
			return false;
		}
		InOutSession = MoveTemp(Candidate);
		OutRequest = MoveTemp(CandidateRequest);
		return true;
	}
	if (Status == EGameXXKNarrativeCommandStatus::Failed && !Step->Command.bOptional)
	{
		Candidate.PauseReason = FString::Printf(
			TEXT("Required narrative command failed: %s"),
			*Step->Command.CommandId.ToString());
		InOutSession = MoveTemp(Candidate);
		OutRequest = FGameXXKNarrativeRequest();
		return SetError(OutError, InOutSession.PauseReason);
	}

	const FName CommandKey = MakeCommandKey(
		Candidate.StoryId,
		Candidate.TaskId,
		Candidate.StepId,
		Step->Command.CommandId);
	if (CommandKey.IsNone())
	{
		return SetError(OutError, TEXT("Narrative command context is incomplete."));
	}
	Candidate.ExecutedCommandKeys.Add(CommandKey);
	Candidate.CurrentSequenceStepId = Step->NextStepId;
	Candidate.PauseReason.Reset();
	FGameXXKNarrativeRequest CandidateRequest;
	if (!AdvanceToRequest(Asset, Candidate, CandidateRequest, OutError))
	{
		return false;
	}
	InOutSession = MoveTemp(Candidate);
	OutRequest = MoveTemp(CandidateRequest);
	return true;
}

bool FGameXXKNarrativeSequenceRules::CompleteWait(
	const UGameXXKNarrativeSequenceAsset& Asset,
	FGameXXKNarrativeSequenceSessionState& InOutSession,
	FGameXXKNarrativeRequest& OutRequest,
	FString* OutError)
{
	using namespace GameXXKNarrativeSequenceRulesPrivate;
	if (!SessionMatchesAsset(Asset, InOutSession, OutError))
	{
		return false;
	}
	const FGameXXKNarrativeSequenceStepDefinition* Step = Asset.FindStep(InOutSession.CurrentSequenceStepId);
	if (!Step || Step->Type != EGameXXKNarrativeStepType::Wait)
	{
		return SetError(OutError, TEXT("Current narrative step is not a wait."));
	}
	FGameXXKNarrativeSequenceSessionState Candidate = InOutSession;
	Candidate.CurrentSequenceStepId = Step->NextStepId;
	FGameXXKNarrativeRequest CandidateRequest;
	if (!AdvanceToRequest(Asset, Candidate, CandidateRequest, OutError))
	{
		return false;
	}
	InOutSession = MoveTemp(Candidate);
	OutRequest = MoveTemp(CandidateRequest);
	return true;
}

bool FGameXXKNarrativeSequenceRules::CompleteDialogue(
	const UGameXXKNarrativeSequenceAsset& Asset,
	const FName OutcomeId,
	FGameXXKNarrativeSequenceSessionState& InOutSession,
	FGameXXKNarrativeRequest& OutRequest,
	FString* OutError)
{
	using namespace GameXXKNarrativeSequenceRulesPrivate;
	if (!SessionMatchesAsset(Asset, InOutSession, OutError))
	{
		return false;
	}
	const FGameXXKNarrativeSequenceStepDefinition* Step = Asset.FindStep(InOutSession.CurrentSequenceStepId);
	if (!Step
		|| Step->Type != EGameXXKNarrativeStepType::Dialogue
		|| InOutSession.AwaitedDialogueId != Step->DialogueId)
	{
		return SetError(OutError, TEXT("Current narrative step is not awaiting this dialogue."));
	}
	if (OutcomeId.IsNone())
	{
		return SetError(OutError, TEXT("Dialogue outcome must not be empty."));
	}
	FGameXXKNarrativeSequenceSessionState Candidate = InOutSession;
	Candidate.AwaitedDialogueId = NAME_None;
	Candidate.LastOutcomeId = OutcomeId;
	Candidate.CurrentSequenceStepId = Step->NextStepId;
	FGameXXKNarrativeRequest CandidateRequest;
	if (!AdvanceToRequest(Asset, Candidate, CandidateRequest, OutError))
	{
		return false;
	}
	InOutSession = MoveTemp(Candidate);
	OutRequest = MoveTemp(CandidateRequest);
	return true;
}

FName FGameXXKNarrativeSequenceRules::MakeCommandKey(
	const FName StoryId,
	const FName TaskId,
	const FName StepId,
	const FName CommandId)
{
	if (StoryId.IsNone() || TaskId.IsNone() || StepId.IsNone() || CommandId.IsNone())
	{
		return NAME_None;
	}
	return FName(*FString::Printf(
		TEXT("%s/%s/%s/%s"),
		*StoryId.ToString(),
		*TaskId.ToString(),
		*StepId.ToString(),
		*CommandId.ToString()));
}
