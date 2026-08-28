#include "Dialogue/GameXXKDialogueRules.h"

#include "Dialogue/GameXXKDialogueAsset.h"

namespace GameXXKDialogueRulesPrivate
{
	constexpr int32 MaximumHistoryEntries = 100;

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

	bool HasCompleteStartContext(const FGameXXKDialogueStartContext& Context)
	{
		return !Context.StoryId.IsNone()
			&& Context.StoryVersion > 0
			&& !Context.TaskId.IsNone()
			&& !Context.StepId.IsNone()
			&& !Context.SequenceId.IsNone()
			&& !Context.StageContractId.IsNone();
	}

	bool SessionMatchesAsset(
		const UGameXXKDialogueAsset& Asset,
		const FGameXXKDialogueSessionState& Session,
		FString* OutError)
	{
		if (!Session.bActive)
		{
			return SetError(OutError, TEXT("Dialogue session is not active."));
		}
		if (Session.DialogueId != Asset.DialogueId || Session.DialogueVersion != Asset.DialogueVersion)
		{
			return SetError(OutError, TEXT("Dialogue asset does not match the active session."));
		}
		return true;
	}

	void AppendHistory(
		FGameXXKDialogueSessionState& Session,
		const FGameXXKDialogueHistoryEntry& Entry)
	{
		Session.History.Add(Entry);
		if (Session.History.Num() > MaximumHistoryEntries)
		{
			Session.History.RemoveAt(
				0,
				Session.History.Num() - MaximumHistoryEntries,
				EAllowShrinking::No);
		}
	}

	void ClearActiveContext(FGameXXKDialogueSessionState& Session)
	{
		Session.bActive = false;
		Session.StoryId = NAME_None;
		Session.StoryVersion = 0;
		Session.TaskId = NAME_None;
		Session.StepId = NAME_None;
		Session.SequenceId = NAME_None;
		Session.StageContractId = NAME_None;
		Session.DialogueId = NAME_None;
		Session.DialogueVersion = 0;
		Session.CurrentNodeId = NAME_None;
		Session.PauseReason.Reset();
	}

	bool BuildCurrentOutput(
		const UGameXXKDialogueAsset& Asset,
		FGameXXKDialogueSessionState& Session,
		FGameXXKDialogueOutput& OutOutput,
		FString* OutError)
	{
		const FGameXXKDialogueNodeDefinition* Node = Asset.FindNode(Session.CurrentNodeId);
		if (!Node)
		{
			return SetError(OutError, FString::Printf(
				TEXT("Dialogue node does not exist: %s"),
				*Session.CurrentNodeId.ToString()));
		}

		FGameXXKDialogueOutput CandidateOutput;
		CandidateOutput.NodeId = Node->NodeId;
		CandidateOutput.Presentation = Node->Presentation;
		CandidateOutput.SpeakerId = Node->SpeakerId;
		CandidateOutput.TextId = Node->TextId;
		CandidateOutput.Text = Node->Text;

		switch (Node->Type)
		{
		case EGameXXKDialogueNodeType::Line:
			break;

		case EGameXXKDialogueNodeType::Choice:
			for (const FGameXXKDialogueOptionDefinition& Option : Node->Options)
			{
				FGameXXKDialogueVisibleOption& Visible = CandidateOutput.Options.AddDefaulted_GetRef();
				Visible.OptionId = Option.OptionId;
				Visible.Text = Option.Text;
				Visible.bEnabled = true;
				Visible.DisabledReason = Option.DisabledReason;
			}
			break;

		case EGameXXKDialogueNodeType::End:
			if (Node->EndOutcomeId.IsNone())
			{
				return SetError(OutError, FString::Printf(
					TEXT("Dialogue end node has no outcome: %s"),
					*Node->NodeId.ToString()));
			}
			CandidateOutput.Presentation = EGameXXKDialoguePresentation::None;
			CandidateOutput.OutcomeId = Node->EndOutcomeId;
			CandidateOutput.bEnded = true;
			ClearActiveContext(Session);
			break;

		default:
			return SetError(OutError, TEXT("Dialogue node has an unsupported type."));
		}

		OutOutput = MoveTemp(CandidateOutput);
		ClearError(OutError);
		return true;
	}
}

bool FGameXXKDialogueRules::Start(
	const UGameXXKDialogueAsset& Asset,
	const FGameXXKDialogueStartContext& Context,
	FGameXXKDialogueSessionState& InOutSession,
	FGameXXKDialogueOutput& OutOutput,
	FString* OutError)
{
	using namespace GameXXKDialogueRulesPrivate;
	if (InOutSession.bActive)
	{
		return SetError(OutError, TEXT("A blocking dialogue session is already active."));
	}
	if (Asset.DialogueId.IsNone() || Asset.DialogueVersion <= 0 || Asset.EntryNodeId.IsNone())
	{
		return SetError(OutError, TEXT("Dialogue asset identity or entry is invalid."));
	}
	if (!HasCompleteStartContext(Context))
	{
		return SetError(OutError, TEXT("Dialogue start context is incomplete."));
	}

	FGameXXKDialogueSessionState Candidate;
	Candidate.bActive = true;
	Candidate.StoryId = Context.StoryId;
	Candidate.StoryVersion = Context.StoryVersion;
	Candidate.TaskId = Context.TaskId;
	Candidate.StepId = Context.StepId;
	Candidate.SequenceId = Context.SequenceId;
	Candidate.StageContractId = Context.StageContractId;
	Candidate.DialogueId = Asset.DialogueId;
	Candidate.DialogueVersion = Asset.DialogueVersion;
	Candidate.CurrentNodeId = Asset.EntryNodeId;

	FGameXXKDialogueOutput CandidateOutput;
	if (!BuildCurrentOutput(Asset, Candidate, CandidateOutput, OutError))
	{
		return false;
	}

	InOutSession = MoveTemp(Candidate);
	OutOutput = MoveTemp(CandidateOutput);
	return true;
}

bool FGameXXKDialogueRules::CompletePresentedNode(
	const UGameXXKDialogueAsset& Asset,
	FGameXXKDialogueSessionState& InOutSession,
	FGameXXKDialogueOutput& OutOutput,
	FString* OutError)
{
	using namespace GameXXKDialogueRulesPrivate;
	if (!SessionMatchesAsset(Asset, InOutSession, OutError))
	{
		return false;
	}
	const FGameXXKDialogueNodeDefinition* Node = Asset.FindNode(InOutSession.CurrentNodeId);
	if (!Node || Node->Type != EGameXXKDialogueNodeType::Line)
	{
		return SetError(OutError, TEXT("Only a presented line can be completed."));
	}
	if (Node->NextNodeId.IsNone())
	{
		return SetError(OutError, TEXT("Presented dialogue line has no next node."));
	}

	FGameXXKDialogueSessionState Candidate = InOutSession;
	FGameXXKDialogueHistoryEntry HistoryEntry;
	HistoryEntry.SpeakerId = Node->SpeakerId;
	HistoryEntry.TextId = Node->TextId;
	HistoryEntry.Text = Node->Text;
	AppendHistory(Candidate, HistoryEntry);
	Candidate.SeenNodeIds.Add(Node->NodeId);
	Candidate.CurrentNodeId = Node->NextNodeId;

	FGameXXKDialogueOutput CandidateOutput;
	if (!BuildCurrentOutput(Asset, Candidate, CandidateOutput, OutError))
	{
		return false;
	}
	InOutSession = MoveTemp(Candidate);
	OutOutput = MoveTemp(CandidateOutput);
	return true;
}

bool FGameXXKDialogueRules::Choose(
	const UGameXXKDialogueAsset& Asset,
	const FName OptionId,
	FGameXXKDialogueSessionState& InOutSession,
	FGameXXKDialogueOutput& OutOutput,
	FString* OutError)
{
	using namespace GameXXKDialogueRulesPrivate;
	if (!SessionMatchesAsset(Asset, InOutSession, OutError))
	{
		return false;
	}
	const FGameXXKDialogueNodeDefinition* Node = Asset.FindNode(InOutSession.CurrentNodeId);
	if (!Node || Node->Type != EGameXXKDialogueNodeType::Choice)
	{
		return SetError(OutError, TEXT("Current dialogue node is not a choice."));
	}
	const FGameXXKDialogueOptionDefinition* Option = Node->Options.FindByPredicate(
		[OptionId](const FGameXXKDialogueOptionDefinition& Candidate)
		{
			return Candidate.OptionId == OptionId;
		});
	if (!Option)
	{
		return SetError(OutError, TEXT("Dialogue option does not exist."));
	}
	if (Option->NextNodeId.IsNone())
	{
		return SetError(OutError, TEXT("Dialogue option has no next node."));
	}

	FGameXXKDialogueSessionState Candidate = InOutSession;
	FGameXXKDialogueHistoryEntry HistoryEntry;
	HistoryEntry.TextId = Option->TextId;
	HistoryEntry.Text = Option->Text;
	HistoryEntry.SelectedOptionId = Option->OptionId;
	AppendHistory(Candidate, HistoryEntry);
	Candidate.SeenNodeIds.Add(Node->NodeId);
	Candidate.SelectedOptionIds.AddUnique(Option->OptionId);
	Candidate.CurrentNodeId = Option->NextNodeId;

	FGameXXKDialogueOutput CandidateOutput;
	if (!BuildCurrentOutput(Asset, Candidate, CandidateOutput, OutError))
	{
		return false;
	}
	if (!CandidateOutput.bEnded)
	{
		CandidateOutput.OutcomeId = Option->OutcomeId;
	}
	InOutSession = MoveTemp(Candidate);
	OutOutput = MoveTemp(CandidateOutput);
	return true;
}

bool FGameXXKDialogueRules::Resume(
	const UGameXXKDialogueAsset& Asset,
	FGameXXKDialogueSessionState& InOutSession,
	FGameXXKDialogueOutput& OutOutput,
	FString* OutError)
{
	using namespace GameXXKDialogueRulesPrivate;
	if (!SessionMatchesAsset(Asset, InOutSession, OutError))
	{
		return false;
	}

	FGameXXKDialogueSessionState Candidate = InOutSession;
	FGameXXKDialogueOutput CandidateOutput;
	if (!BuildCurrentOutput(Asset, Candidate, CandidateOutput, OutError))
	{
		return false;
	}
	InOutSession = MoveTemp(Candidate);
	OutOutput = MoveTemp(CandidateOutput);
	return true;
}
