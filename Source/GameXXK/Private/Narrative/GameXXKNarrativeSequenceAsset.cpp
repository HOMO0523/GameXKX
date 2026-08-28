#include "Narrative/GameXXKNarrativeSequenceAsset.h"

const FGameXXKNarrativeSequenceStepDefinition* UGameXXKNarrativeSequenceAsset::FindStep(const FName StepId) const
{
	return Steps.FindByPredicate([StepId](const FGameXXKNarrativeSequenceStepDefinition& Step)
	{
		return Step.StepId == StepId;
	});
}

#if WITH_EDITOR
EDataValidationResult UGameXXKNarrativeSequenceAsset::IsDataValid(FDataValidationContext& Context) const
{
	bool bInvalid = Super::IsDataValid(Context) == EDataValidationResult::Invalid;
	if (SequenceId.IsNone() || SequenceVersion <= 0 || StageContractId.IsNone() || EntryStepId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("Sequence identity, version, stage contract and entry are required.")));
		bInvalid = true;
	}

	TSet<FName> StepIds;
	TSet<FName> CommandIds;
	for (const FGameXXKNarrativeSequenceStepDefinition& Step : Steps)
	{
		if (Step.StepId.IsNone() || StepIds.Contains(Step.StepId))
		{
			Context.AddError(FText::FromString(TEXT("Sequence step IDs must be non-empty and unique.")));
			bInvalid = true;
		}
		StepIds.Add(Step.StepId);
	}
	if (!StepIds.Contains(EntryStepId))
	{
		Context.AddError(FText::FromString(TEXT("Sequence entry step must resolve.")));
		bInvalid = true;
	}
	for (const TPair<FName, FName>& Role : DefaultCharacterIdByRole)
	{
		if (Role.Key.IsNone() || Role.Value.IsNone())
		{
			Context.AddError(FText::FromString(TEXT("Sequence role bindings must not be empty.")));
			bInvalid = true;
		}
	}

	for (const FGameXXKNarrativeSequenceStepDefinition& Step : Steps)
	{
		auto ValidateNext = [&](const FName NextStepId)
		{
			if (NextStepId.IsNone() || !StepIds.Contains(NextStepId))
			{
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("Sequence step has an invalid next step: %s"),
					*Step.StepId.ToString())));
				bInvalid = true;
			}
		};

		switch (Step.Type)
		{
		case EGameXXKNarrativeStepType::Command:
			if (Step.Command.CommandId.IsNone()
				|| Step.Command.CommandType.IsNone()
				|| CommandIds.Contains(Step.Command.CommandId))
			{
				Context.AddError(FText::FromString(TEXT("Sequence command IDs/types must be non-empty and command IDs unique.")));
				bInvalid = true;
			}
			CommandIds.Add(Step.Command.CommandId);
			ValidateNext(Step.NextStepId);
			break;

		case EGameXXKNarrativeStepType::Wait:
			if (Step.WaitType.IsNone())
			{
				Context.AddError(FText::FromString(TEXT("Sequence wait type must not be empty.")));
				bInvalid = true;
			}
			ValidateNext(Step.NextStepId);
			break;

		case EGameXXKNarrativeStepType::Dialogue:
			if (Step.DialogueId.IsNone())
			{
				Context.AddError(FText::FromString(TEXT("Sequence dialogue ID must not be empty.")));
				bInvalid = true;
			}
			ValidateNext(Step.NextStepId);
			break;

		case EGameXXKNarrativeStepType::BranchOnOutcome:
			if (Step.OutcomeToStepId.IsEmpty())
			{
				Context.AddError(FText::FromString(TEXT("Outcome branch must contain at least one mapping.")));
				bInvalid = true;
			}
			for (const TPair<FName, FName>& Branch : Step.OutcomeToStepId)
			{
				if (Branch.Key.IsNone() || Branch.Value.IsNone() || !StepIds.Contains(Branch.Value))
				{
					Context.AddError(FText::FromString(TEXT("Outcome branch IDs and targets must resolve.")));
					bInvalid = true;
				}
			}
			break;

		case EGameXXKNarrativeStepType::End:
			break;

		default:
			Context.AddError(FText::FromString(TEXT("Sequence step type is invalid.")));
			bInvalid = true;
			break;
		}
	}
	return bInvalid ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
