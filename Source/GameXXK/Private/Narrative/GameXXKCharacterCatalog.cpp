#include "Narrative/GameXXKCharacterCatalog.h"

const FGameXXKCharacterDefinition* UGameXXKCharacterCatalog::FindCharacter(const FName CharacterId) const
{
	return Characters.FindByPredicate([CharacterId](const FGameXXKCharacterDefinition& Definition)
	{
		return Definition.CharacterId == CharacterId;
	});
}

#if WITH_EDITOR
EDataValidationResult UGameXXKCharacterCatalog::IsDataValid(FDataValidationContext& Context) const
{
	bool bInvalid = Super::IsDataValid(Context) == EDataValidationResult::Invalid;
	TSet<FName> SeenCharacterIds;
	for (const FGameXXKCharacterDefinition& Definition : Characters)
	{
		if (Definition.CharacterId.IsNone() || SeenCharacterIds.Contains(Definition.CharacterId))
		{
			Context.AddError(FText::FromString(TEXT("Character IDs must be non-empty and unique.")));
			bInvalid = true;
		}
		SeenCharacterIds.Add(Definition.CharacterId);
		if (Definition.SupportedActionIds.Contains(NAME_None))
		{
			Context.AddError(FText::FromString(TEXT("Character action IDs must not be empty.")));
			bInvalid = true;
		}
	}
	return bInvalid ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
