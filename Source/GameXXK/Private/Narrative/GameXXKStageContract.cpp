#include "Narrative/GameXXKStageContract.h"

bool UGameXXKStageContract::RequiresSlot(const FName SlotId) const
{
	return RequiredSlotIds.Contains(SlotId);
}

#if WITH_EDITOR
EDataValidationResult UGameXXKStageContract::IsDataValid(FDataValidationContext& Context) const
{
	bool bInvalid = Super::IsDataValid(Context) == EDataValidationResult::Invalid;
	if (StageContractId.IsNone() || RequiredSlotIds.IsEmpty() || RequiredSlotIds.Contains(NAME_None))
	{
		Context.AddError(FText::FromString(TEXT("StageContract requires an ID and non-empty stable slots.")));
		bInvalid = true;
	}
	return bInvalid ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
