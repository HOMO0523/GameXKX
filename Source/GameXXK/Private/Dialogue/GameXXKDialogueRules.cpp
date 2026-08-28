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

	bool ParseNonNegativeInteger(const FString& Value, int32& OutValue)
	{
		return LexTryParseString(OutValue, *Value) && OutValue >= 0;
	}

	bool ParseIdAndCount(const FString& Value, FName& OutId, int32& OutCount)
	{
		FString IdString;
		FString CountString;
		if (!Value.Split(TEXT(":"), &IdString, &CountString, ESearchCase::CaseSensitive, ESearchDir::FromEnd)
			|| IdString.IsEmpty()
			|| !ParseNonNegativeInteger(CountString, OutCount))
		{
			return false;
		}
		OutId = FName(*IdString);
		return !OutId.IsNone();
	}

	bool ParseTutorialState(const FString& Value, EGameXXKTutorialQuestState& OutState)
	{
		if (Value == TEXT("NotStarted"))
		{
			OutState = EGameXXKTutorialQuestState::NotStarted;
			return true;
		}
		if (Value == TEXT("Active"))
		{
			OutState = EGameXXKTutorialQuestState::Active;
			return true;
		}
		if (Value == TEXT("Completed"))
		{
			OutState = EGameXXKTutorialQuestState::Completed;
			return true;
		}
		return false;
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
		FString* OutError,
		const FGameXXKDialogueConditionContext* ConditionContext)
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
				bool bConditionsMet = Option.Conditions.IsEmpty();
				if (!Option.Conditions.IsEmpty() && ConditionContext)
				{
					FString ConditionError;
					bConditionsMet = FGameXXKDialogueRules::EvaluateConditions(
						Option.Conditions,
						*ConditionContext,
						&ConditionError);
					if (!ConditionError.IsEmpty())
					{
						return SetError(OutError, ConditionError);
					}
				}
				if (!bConditionsMet && Option.DisabledReason.IsEmpty())
				{
					continue;
				}
				FGameXXKDialogueVisibleOption& Visible = CandidateOutput.Options.AddDefaulted_GetRef();
				Visible.OptionId = Option.OptionId;
				Visible.Text = Option.Text;
				Visible.bEnabled = bConditionsMet;
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
	FString* OutError,
	const FGameXXKDialogueConditionContext* ConditionContext)
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
	if (!BuildCurrentOutput(Asset, Candidate, CandidateOutput, OutError, ConditionContext))
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
	FString* OutError,
	const FGameXXKDialogueConditionContext* ConditionContext)
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
	if (!BuildCurrentOutput(Asset, Candidate, CandidateOutput, OutError, ConditionContext))
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
	FString* OutError,
	const FGameXXKDialogueConditionContext* ConditionContext)
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
	if (!Option->Conditions.IsEmpty())
	{
		if (!ConditionContext)
		{
			return SetError(OutError, TEXT("Dialogue option conditions cannot be evaluated without context."));
		}
		FString ConditionError;
		if (!EvaluateConditions(Option->Conditions, *ConditionContext, &ConditionError))
		{
			return SetError(
				OutError,
				ConditionError.IsEmpty() ? TEXT("Dialogue option is unavailable.") : ConditionError);
		}
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
	if (!BuildCurrentOutput(Asset, Candidate, CandidateOutput, OutError, ConditionContext))
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
	FString* OutError,
	const FGameXXKDialogueConditionContext* ConditionContext)
{
	using namespace GameXXKDialogueRulesPrivate;
	if (!SessionMatchesAsset(Asset, InOutSession, OutError))
	{
		return false;
	}

	FGameXXKDialogueSessionState Candidate = InOutSession;
	FGameXXKDialogueOutput CandidateOutput;
	if (!BuildCurrentOutput(Asset, Candidate, CandidateOutput, OutError, ConditionContext))
	{
		return false;
	}
	InOutSession = MoveTemp(Candidate);
	OutOutput = MoveTemp(CandidateOutput);
	return true;
}

bool FGameXXKDialogueRules::EvaluateConditions(
	const TMap<FName, FString>& Conditions,
	const FGameXXKDialogueConditionContext& Context,
	FString* OutError)
{
	using namespace GameXXKDialogueRulesPrivate;
	ClearError(OutError);
	for (const TPair<FName, FString>& Pair : Conditions)
	{
		const FName Type = Pair.Key;
		const FString& Value = Pair.Value;
		if (Type == TEXT("flag"))
		{
			if (Value.IsEmpty())
			{
				return SetError(OutError, TEXT("flag condition requires an ID."));
			}
			if (!Context.Flags.Contains(FName(*Value)))
			{
				return false;
			}
		}
		else if (Type == TEXT("tutorialState"))
		{
			EGameXXKTutorialQuestState RequiredState;
			if (!ParseTutorialState(Value, RequiredState))
			{
				return SetError(OutError, TEXT("tutorialState condition has an invalid value."));
			}
			if (Context.TutorialState != RequiredState)
			{
				return false;
			}
		}
		else if (Type == TEXT("taskState"))
		{
			FName TaskId;
			int32 RequiredState = 0;
			if (!ParseIdAndCount(Value, TaskId, RequiredState))
			{
				return SetError(OutError, TEXT("taskState condition must be TaskId:nonnegative-state."));
			}
			const int32* ActualState = Context.TaskStateValues.Find(TaskId);
			if (!ActualState || *ActualState != RequiredState)
			{
				return false;
			}
		}
		else if (Type == TEXT("itemAtLeast"))
		{
			FName ItemId;
			int32 RequiredCount = 0;
			if (!ParseIdAndCount(Value, ItemId, RequiredCount))
			{
				return SetError(OutError, TEXT("itemAtLeast condition must be ItemId:nonnegative-count."));
			}
			if (Context.ItemCounts.FindRef(ItemId) < RequiredCount)
			{
				return false;
			}
		}
		else if (Type == TEXT("goldAtLeast"))
		{
			int32 RequiredGold = 0;
			if (!ParseNonNegativeInteger(Value, RequiredGold))
			{
				return SetError(OutError, TEXT("goldAtLeast condition requires a nonnegative integer."));
			}
			if (Context.Gold < RequiredGold)
			{
				return false;
			}
		}
		else if (Type == TEXT("companionUnlocked"))
		{
			if (Value.IsEmpty())
			{
				return SetError(OutError, TEXT("companionUnlocked condition requires an ID."));
			}
			if (!Context.UnlockedCompanionIds.Contains(FName(*Value)))
			{
				return false;
			}
		}
		else if (Type == TEXT("optionSelected"))
		{
			if (Value.IsEmpty())
			{
				return SetError(OutError, TEXT("optionSelected condition requires an ID."));
			}
			if (!Context.SelectedOptionIds.Contains(FName(*Value)))
			{
				return false;
			}
		}
		else if (Type == TEXT("nodeSeen"))
		{
			if (Value.IsEmpty())
			{
				return SetError(OutError, TEXT("nodeSeen condition requires an ID."));
			}
			if (!Context.SeenNodeIds.Contains(FName(*Value)))
			{
				return false;
			}
		}
		else
		{
			return SetError(OutError, FString::Printf(
				TEXT("Unknown dialogue condition: %s"),
				*Type.ToString()));
		}
	}
	return true;
}
