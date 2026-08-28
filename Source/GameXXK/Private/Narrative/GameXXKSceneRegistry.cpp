#include "Narrative/GameXXKSceneRegistry.h"

#include "Narrative/GameXXKSceneProfile.h"
#include "Narrative/GameXXKStageContract.h"

bool UGameXXKSceneRegistry::RegisterActiveProfile(
	const UGameXXKStageContract& Contract,
	UGameXXKSceneProfile& Profile,
	const UGameXXKCharacterCatalog* CharacterCatalog,
	const FSoftObjectPath& CurrentMapPath,
	FString* OutError)
{
	if (CurrentMapPath.IsNull() || Profile.MapPath != CurrentMapPath)
	{
		if (OutError)
		{
			*OutError = TEXT("SceneProfile map does not match the active map.");
		}
		return false;
	}
	if (!Profile.ValidateAgainstContract(Contract, CharacterCatalog, OutError))
	{
		return false;
	}
	ActiveProfiles.Add(Contract.StageContractId, &Profile);
	if (OutError)
	{
		OutError->Reset();
	}
	return true;
}

UGameXXKSceneProfile* UGameXXKSceneRegistry::ResolveProfile(const FName StageContractId) const
{
	return ActiveProfiles.FindRef(StageContractId);
}
