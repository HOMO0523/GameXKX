#include "Guide/GameXXKGuideAsset.h"

#include "Guide/GameXXKGuideTargetRegistry.h"

const FGameXXKGuideStepDefinition* UGameXXKGuideAsset::FindStep(const FName StepId) const
{
	return Steps.FindByPredicate([StepId](const FGameXXKGuideStepDefinition& Step)
	{
		return Step.StepId == StepId;
	});
}

#if WITH_EDITOR
EDataValidationResult UGameXXKGuideAsset::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	bool bInvalid = SuperResult == EDataValidationResult::Invalid;
	const auto AddError = [&Context, &bInvalid](const FString& Message)
	{
		Context.AddError(FText::FromString(Message));
		bInvalid = true;
	};

	if (GuideId.IsNone())
	{
		AddError(TEXT("GuideId must not be empty."));
	}
	if (GuideVersion <= 0)
	{
		AddError(TEXT("GuideVersion must be positive."));
	}
	if (Steps.IsEmpty())
	{
		AddError(TEXT("Guide must contain at least one step."));
	}

	TSet<FName> StepIds;
	for (const FGameXXKGuideStepDefinition& Step : Steps)
	{
		if (Step.StepId.IsNone() || StepIds.Contains(Step.StepId))
		{
			AddError(TEXT("Guide step IDs must be non-empty and unique."));
		}
		else
		{
			StepIds.Add(Step.StepId);
		}
	}
	if (EntryStepId.IsNone() || !StepIds.Contains(EntryStepId))
	{
		AddError(TEXT("Guide EntryStepId must resolve."));
	}

	int32 TerminalCount = 0;
	for (const FGameXXKGuideStepDefinition& Step : Steps)
	{
		if (Step.StepId.IsNone())
		{
			continue;
		}
		if (Step.TriggerEventId.IsNone()
			|| !FGameXXKGuideTargetRegistry::IsKnownTriggerEventId(Step.TriggerEventId))
		{
			AddError(FString::Printf(TEXT("Guide step has an unknown trigger event: %s"), *Step.StepId.ToString()));
		}
		if (Step.TargetId.IsNone()
			|| !FGameXXKGuideTargetRegistry::IsKnownTargetId(Step.TargetId))
		{
			AddError(FString::Printf(TEXT("Guide step has an unknown target: %s"), *Step.StepId.ToString()));
		}
		if (Step.Text.IsEmpty())
		{
			AddError(FString::Printf(TEXT("Guide step text must not be empty: %s"), *Step.StepId.ToString()));
		}
		if (Step.CompletionEventId.IsNone()
			|| !FGameXXKGuideTargetRegistry::IsKnownCompletionEventId(Step.CompletionEventId))
		{
			AddError(FString::Printf(TEXT("Guide step has an unknown completion event: %s"), *Step.StepId.ToString()));
		}
		if (Step.InputPolicy == EGameXXKGuideInputPolicy::Forced && Step.AllowedActionIds.IsEmpty())
		{
			AddError(FString::Printf(TEXT("Forced guide step requires at least one allowed action: %s"), *Step.StepId.ToString()));
		}
		for (const FName ActionId : Step.AllowedActionIds)
		{
			if (!FGameXXKGuideTargetRegistry::IsKnownActionId(ActionId))
			{
				AddError(FString::Printf(TEXT("Guide step has an unknown action: %s"), *ActionId.ToString()));
			}
		}
		if (Step.NextStepId.IsNone())
		{
			++TerminalCount;
		}
		else if (!StepIds.Contains(Step.NextStepId))
		{
			AddError(FString::Printf(TEXT("Guide step next target does not resolve: %s"), *Step.StepId.ToString()));
		}
	}
	if (TerminalCount != 1)
	{
		AddError(TEXT("Guide must contain exactly one terminal step."));
	}

	if (!EntryStepId.IsNone() && StepIds.Contains(EntryStepId))
	{
		TSet<FName> Reachable;
		FName Cursor = EntryStepId;
		while (!Cursor.IsNone() && !Reachable.Contains(Cursor))
		{
			Reachable.Add(Cursor);
			const FGameXXKGuideStepDefinition* Step = FindStep(Cursor);
			Cursor = Step ? Step->NextStepId : NAME_None;
		}
		if (!Cursor.IsNone())
		{
			AddError(TEXT("Guide contains a cycle."));
		}
		for (const FName StepId : StepIds)
		{
			if (!Reachable.Contains(StepId))
			{
				AddError(FString::Printf(TEXT("Guide contains an unreachable step: %s"), *StepId.ToString()));
			}
		}
	}

	return bInvalid ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
