#include "Guide/GameXXKGuideRules.h"

namespace GameXXKGuideRulesPrivate
{
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

	bool ValidateIdentity(const UGameXXKGuideAsset& Asset, FString* OutError)
	{
		if (Asset.GuideId.IsNone()
			|| Asset.GuideVersion <= 0
			|| Asset.EntryStepId.IsNone()
			|| !Asset.FindStep(Asset.EntryStepId))
		{
			return SetError(OutError, TEXT("Guide asset identity or entry step is invalid."));
		}
		return true;
	}

	const FGameXXKGuideStepDefinition* FindFirstIncompleteStep(
		const UGameXXKGuideAsset& Asset,
		const FGameXXKGuideProgress& Progress,
		FName StartStepId,
		FString* OutError)
	{
		TSet<FName> Visited;
		FName Cursor = StartStepId;
		while (!Cursor.IsNone())
		{
			if (Visited.Contains(Cursor))
			{
				SetError(OutError, TEXT("Guide step chain contains a cycle."));
				return nullptr;
			}
			Visited.Add(Cursor);
			const FGameXXKGuideStepDefinition* Step = Asset.FindStep(Cursor);
			if (!Step)
			{
				SetError(OutError, FString::Printf(
					TEXT("Guide step does not exist: %s"),
					*Cursor.ToString()));
				return nullptr;
			}
			if (!Progress.CompletedGuideStepIds.Contains(Cursor))
			{
				return Step;
			}
			Cursor = Step->NextStepId;
		}
		return nullptr;
	}

	bool BuildOutput(
		const UGameXXKGuideAsset& Asset,
		const FGameXXKGuideProgress& Progress,
		FGameXXKGuideOutput& OutOutput,
		FString* OutError)
	{
		if (Progress.ActiveGuideId != Asset.GuideId || Progress.ActiveGuideStepId.IsNone())
		{
			return SetError(OutError, TEXT("Guide asset does not match an active guide session."));
		}
		const FGameXXKGuideStepDefinition* Step = Asset.FindStep(Progress.ActiveGuideStepId);
		if (!Step)
		{
			return SetError(OutError, TEXT("Active guide step does not exist in the asset."));
		}

		FGameXXKGuideOutput Candidate;
		Candidate.bActive = true;
		Candidate.GuideId = Asset.GuideId;
		Candidate.StepId = Step->StepId;
		Candidate.TargetId = Step->TargetId;
		Candidate.InputPolicy = Step->InputPolicy;
		Candidate.Text = Step->Text;
		Candidate.AllowedActionIds = Step->AllowedActionIds;
		OutOutput = MoveTemp(Candidate);
		ClearError(OutError);
		return true;
	}

	bool AdvanceAfterCompletion(
		const UGameXXKGuideAsset& Asset,
		const FGameXXKGuideStepDefinition& CompletedStep,
		FGameXXKGuideProgress& InOutProgress,
		FGameXXKGuideOutput& OutOutput,
		FString* OutError)
	{
		FGameXXKGuideProgress Candidate = InOutProgress;
		Candidate.CompletedGuideStepIds.Add(CompletedStep.StepId);
		const FGameXXKGuideStepDefinition* Next = FindFirstIncompleteStep(
			Asset,
			Candidate,
			CompletedStep.NextStepId,
			OutError);
		if (!CompletedStep.NextStepId.IsNone() && !Next && OutError && !OutError->IsEmpty())
		{
			return false;
		}

		if (!Next)
		{
			Candidate.ActiveGuideId = NAME_None;
			Candidate.ActiveGuideStepId = NAME_None;
			FGameXXKGuideOutput CandidateOutput;
			CandidateOutput.bCompleted = true;
			CandidateOutput.GuideId = Asset.GuideId;
			CandidateOutput.StepId = CompletedStep.StepId;
			InOutProgress = MoveTemp(Candidate);
			OutOutput = MoveTemp(CandidateOutput);
			ClearError(OutError);
			return true;
		}

		Candidate.ActiveGuideStepId = Next->StepId;
		FGameXXKGuideOutput CandidateOutput;
		if (!BuildOutput(Asset, Candidate, CandidateOutput, OutError))
		{
			return false;
		}
		InOutProgress = MoveTemp(Candidate);
		OutOutput = MoveTemp(CandidateOutput);
		return true;
	}
}

bool FGameXXKGuideRules::TryStart(
	const UGameXXKGuideAsset& Asset,
	const FName TriggerEventId,
	FGameXXKGuideProgress& InOutProgress,
	FGameXXKGuideOutput& OutOutput,
	FString* OutError)
{
	using namespace GameXXKGuideRulesPrivate;
	ClearError(OutError);
	if (InOutProgress.Preference == EGameXXKGuidePreference::ExperiencedPlayer)
	{
		return false;
	}
	if (!InOutProgress.ActiveGuideId.IsNone())
	{
		return SetError(OutError, TEXT("Only one guide may be active at a time."));
	}
	if (!ValidateIdentity(Asset, OutError))
	{
		return false;
	}
	const FGameXXKGuideStepDefinition* Entry = FindFirstIncompleteStep(
		Asset,
		InOutProgress,
		Asset.EntryStepId,
		OutError);
	if (!Entry)
	{
		return false;
	}
	if (Entry->TriggerEventId != TriggerEventId)
	{
		ClearError(OutError);
		return false;
	}

	FGameXXKGuideProgress Candidate = InOutProgress;
	Candidate.ActiveGuideId = Asset.GuideId;
	Candidate.ActiveGuideStepId = Entry->StepId;
	Candidate.LastDiagnostic.Reset();
	FGameXXKGuideOutput CandidateOutput;
	if (!BuildOutput(Asset, Candidate, CandidateOutput, OutError))
	{
		return false;
	}
	InOutProgress = MoveTemp(Candidate);
	OutOutput = MoveTemp(CandidateOutput);
	return true;
}

bool FGameXXKGuideRules::HandleEvent(
	const UGameXXKGuideAsset& Asset,
	const FName EventId,
	FGameXXKGuideProgress& InOutProgress,
	FGameXXKGuideOutput& OutOutput,
	FString* OutError)
{
	using namespace GameXXKGuideRulesPrivate;
	ClearError(OutError);
	if (InOutProgress.ActiveGuideId != Asset.GuideId || InOutProgress.ActiveGuideStepId.IsNone())
	{
		return SetError(OutError, TEXT("Guide event does not match an active guide session."));
	}
	const FGameXXKGuideStepDefinition* Step = Asset.FindStep(InOutProgress.ActiveGuideStepId);
	if (!Step)
	{
		return SetError(OutError, TEXT("Active guide step does not exist."));
	}
	if (Step->CompletionEventId != EventId)
	{
		ClearError(OutError);
		return false;
	}
	return AdvanceAfterCompletion(Asset, *Step, InOutProgress, OutOutput, OutError);
}

bool FGameXXKGuideRules::Resume(
	const UGameXXKGuideAsset& Asset,
	FGameXXKGuideProgress& InOutProgress,
	FGameXXKGuideOutput& OutOutput,
	FString* OutError)
{
	using namespace GameXXKGuideRulesPrivate;
	ClearError(OutError);
	return BuildOutput(Asset, InOutProgress, OutOutput, OutError);
}

bool FGameXXKGuideRules::HandleTargetUnavailable(
	const UGameXXKGuideAsset& Asset,
	const FName TargetId,
	FGameXXKGuideProgress& InOutProgress,
	FGameXXKGuideOutput& OutOutput,
	FString* OutError)
{
	using namespace GameXXKGuideRulesPrivate;
	ClearError(OutError);
	if (InOutProgress.ActiveGuideId != Asset.GuideId || InOutProgress.ActiveGuideStepId.IsNone())
	{
		return SetError(OutError, TEXT("Missing target does not match an active guide session."));
	}
	const FGameXXKGuideStepDefinition* Step = Asset.FindStep(InOutProgress.ActiveGuideStepId);
	if (!Step)
	{
		return SetError(OutError, TEXT("Active guide step does not exist."));
	}
	if (Step->InputPolicy != EGameXXKGuideInputPolicy::Forced || Step->TargetId != TargetId)
	{
		return false;
	}

	FGameXXKGuideProgress Candidate = InOutProgress;
	Candidate.LastDiagnostic = FString::Printf(
		TEXT("Forced guide target unavailable: %s"),
		*TargetId.ToString());
	FGameXXKGuideOutput CandidateOutput;
	if (!AdvanceAfterCompletion(Asset, *Step, Candidate, CandidateOutput, OutError))
	{
		return false;
	}
	InOutProgress = MoveTemp(Candidate);
	OutOutput = MoveTemp(CandidateOutput);
	return true;
}

bool FGameXXKGuideRules::CanExecuteAction(
	const UGameXXKGuideAsset& Asset,
	const FGameXXKGuideProgress& Progress,
	const FName ActionId)
{
	if (Progress.ActiveGuideId.IsNone() || Progress.ActiveGuideStepId.IsNone())
	{
		return true;
	}
	if (Progress.ActiveGuideId != Asset.GuideId)
	{
		return false;
	}
	const FGameXXKGuideStepDefinition* Step = Asset.FindStep(Progress.ActiveGuideStepId);
	return Step
		&& (Step->InputPolicy == EGameXXKGuideInputPolicy::Soft
			|| Step->AllowedActionIds.Contains(ActionId));
}

void FGameXXKGuideRules::Cancel(FGameXXKGuideProgress& InOutProgress, const FString& Diagnostic)
{
	InOutProgress.ActiveGuideId = NAME_None;
	InOutProgress.ActiveGuideStepId = NAME_None;
	InOutProgress.LastDiagnostic = Diagnostic;
}

void FGameXXKGuideRules::ResetCombatGuide(FGameXXKGuideProgress& InOutProgress)
{
	InOutProgress.Preference = EGameXXKGuidePreference::Unset;
	InOutProgress.ActiveGuideId = NAME_None;
	InOutProgress.ActiveGuideStepId = NAME_None;
	InOutProgress.CompletedGuideStepIds.Reset();
	InOutProgress.LastDiagnostic.Reset();
}
