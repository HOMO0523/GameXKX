#include "Dialogue/GameXXKDialogueAsset.h"

const FGameXXKDialogueNodeDefinition* UGameXXKDialogueAsset::FindNode(const FName NodeId) const
{
	return Nodes.FindByPredicate([NodeId](const FGameXXKDialogueNodeDefinition& Node)
	{
		return Node.NodeId == NodeId;
	});
}

#if WITH_EDITOR
EDataValidationResult UGameXXKDialogueAsset::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	bool bInvalid = SuperResult == EDataValidationResult::Invalid;

	if (DialogueId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("DialogueId must not be empty.")));
		bInvalid = true;
	}
	if (DialogueVersion <= 0)
	{
		Context.AddError(FText::FromString(TEXT("DialogueVersion must be positive.")));
		bInvalid = true;
	}
	if (EntryNodeId.IsNone() || FindNode(EntryNodeId) == nullptr)
	{
		Context.AddError(FText::FromString(TEXT("EntryNodeId must resolve to a node.")));
		bInvalid = true;
	}

	TSet<FName> SeenNodeIds;
	for (const FGameXXKDialogueNodeDefinition& Node : Nodes)
	{
		if (Node.NodeId.IsNone())
		{
			Context.AddError(FText::FromString(TEXT("Dialogue node IDs must not be empty.")));
			bInvalid = true;
			continue;
		}
		if (SeenNodeIds.Contains(Node.NodeId))
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("Duplicate dialogue node ID: %s"),
				*Node.NodeId.ToString())));
			bInvalid = true;
		}
		SeenNodeIds.Add(Node.NodeId);
	}

	return bInvalid ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
