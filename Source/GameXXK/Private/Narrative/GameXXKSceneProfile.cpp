#include "Narrative/GameXXKSceneProfile.h"

#include "Narrative/GameXXKCharacterCatalog.h"
#include "Narrative/GameXXKStageContract.h"

namespace GameXXKSceneProfilePrivate
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
}

const FGameXXKSceneSlotBinding* UGameXXKSceneProfile::FindSlot(const FName SlotId) const
{
	return SlotBindings.FindByPredicate([SlotId](const FGameXXKSceneSlotBinding& Binding)
	{
		return Binding.SlotId == SlotId;
	});
}

bool UGameXXKSceneProfile::ValidateAgainstContract(
	const UGameXXKStageContract& Contract,
	const UGameXXKCharacterCatalog* CharacterCatalog,
	FString* OutError) const
{
	using namespace GameXXKSceneProfilePrivate;
	if (SceneProfileId.IsNone()
		|| StageContractId.IsNone()
		|| StageContractId != Contract.StageContractId
		|| MapPath.IsNull()
		|| SceneRootTag.IsNone())
	{
		return SetError(OutError, TEXT("SceneProfile identity, map, root or contract is invalid."));
	}
	TSet<FName> SlotIds;
	for (const FGameXXKSceneSlotBinding& Binding : SlotBindings)
	{
		if (Binding.SlotId.IsNone() || SlotIds.Contains(Binding.SlotId))
		{
			return SetError(OutError, TEXT("SceneProfile slot IDs must be non-empty and unique."));
		}
		SlotIds.Add(Binding.SlotId);
	}
	for (const FName RequiredSlotId : Contract.RequiredSlotIds)
	{
		if (!SlotIds.Contains(RequiredSlotId))
		{
			return SetError(OutError, FString::Printf(
				TEXT("SceneProfile is missing required slot %s."),
				*RequiredSlotId.ToString()));
		}
	}
	if (SafeSlotId.IsNone() || !SlotIds.Contains(SafeSlotId))
	{
		return SetError(OutError, TEXT("SceneProfile safe slot must resolve."));
	}
	TSet<FName> NpcIds;
	for (const FGameXXKNpcScenePlacement& Placement : NpcPlacements)
	{
		if (Placement.CharacterId.IsNone()
			|| NpcIds.Contains(Placement.CharacterId)
			|| !SlotIds.Contains(Placement.HomeSlotId)
			|| !SlotIds.Contains(Placement.InteractionAnchorSlotId)
			|| (CharacterCatalog && !CharacterCatalog->FindCharacter(Placement.CharacterId)))
		{
			return SetError(OutError, TEXT("SceneProfile NPC placement is invalid."));
		}
		NpcIds.Add(Placement.CharacterId);
	}
	TSet<FName> TriggerIds;
	for (const FGameXXKSceneTriggerRegion& Trigger : TriggerRegions)
	{
		if (Trigger.TriggerId.IsNone()
			|| TriggerIds.Contains(Trigger.TriggerId)
			|| !SlotIds.Contains(Trigger.AnchorSlotId)
			|| Trigger.Extent.X < 0.0
			|| Trigger.Extent.Y < 0.0
			|| Trigger.Extent.Z < 0.0)
		{
			return SetError(OutError, TEXT("SceneProfile trigger region is invalid."));
		}
		TriggerIds.Add(Trigger.TriggerId);
	}
	ClearError(OutError);
	return true;
}

bool UGameXXKSceneProfile::ResolveWorldTransform(
	const FName SlotId,
	const FTransform& SceneRootTransform,
	FTransform& OutWorldTransform,
	FString* OutError) const
{
	using namespace GameXXKSceneProfilePrivate;
	if (const FGameXXKSceneSlotBinding* Binding = FindSlot(SlotId))
	{
		OutWorldTransform = Binding->RelativeTransform * SceneRootTransform;
		ClearError(OutError);
		return true;
	}
	if (const FGameXXKSceneSlotBinding* Safe = FindSlot(SafeSlotId))
	{
		OutWorldTransform = Safe->RelativeTransform * SceneRootTransform;
	}
	else
	{
		OutWorldTransform = SceneRootTransform;
	}
	return SetError(OutError, FString::Printf(
		TEXT("SceneProfile slot does not exist: %s. Returned safe slot %s."),
		*SlotId.ToString(),
		*SafeSlotId.ToString()));
}
