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
	TSet<FName> SeenOptionIds;
	TSet<FName> SeenOutcomeIds;
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

	for (const FGameXXKDialogueNodeDefinition& Node : Nodes)
	{
		if (Node.NodeId.IsNone())
		{
			continue;
		}
		if (Node.Type == EGameXXKDialogueNodeType::Line)
		{
			if (Node.NextNodeId.IsNone() || !SeenNodeIds.Contains(Node.NextNodeId))
			{
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("Line node has a missing next node: %s"),
					*Node.NodeId.ToString())));
				bInvalid = true;
			}
		}
		else if (Node.Type == EGameXXKDialogueNodeType::Choice)
		{
			if (Node.Options.IsEmpty() || Node.Options.Num() > 4)
			{
				Context.AddError(FText::FromString(FString::Printf(
					TEXT("Choice node must contain one to four options: %s"),
					*Node.NodeId.ToString())));
				bInvalid = true;
			}
			for (const FGameXXKDialogueOptionDefinition& Option : Node.Options)
			{
				if (Option.OptionId.IsNone() || SeenOptionIds.Contains(Option.OptionId))
				{
					Context.AddError(FText::FromString(TEXT("Dialogue option IDs must be non-empty and unique.")));
					bInvalid = true;
				}
				SeenOptionIds.Add(Option.OptionId);
				if (Option.OutcomeId.IsNone() || SeenOutcomeIds.Contains(Option.OutcomeId))
				{
					Context.AddError(FText::FromString(TEXT("Dialogue outcome IDs must be non-empty and unique.")));
					bInvalid = true;
				}
				SeenOutcomeIds.Add(Option.OutcomeId);
				if (Option.NextNodeId.IsNone() || !SeenNodeIds.Contains(Option.NextNodeId))
				{
					Context.AddError(FText::FromString(TEXT("Dialogue option next node must resolve.")));
					bInvalid = true;
				}
			}
		}
		else if (Node.Type == EGameXXKDialogueNodeType::End)
		{
			if (Node.EndOutcomeId.IsNone() || SeenOutcomeIds.Contains(Node.EndOutcomeId))
			{
				Context.AddError(FText::FromString(TEXT("Dialogue end outcomes must be non-empty and unique.")));
				bInvalid = true;
			}
			SeenOutcomeIds.Add(Node.EndOutcomeId);
		}
	}

	return bInvalid ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif
