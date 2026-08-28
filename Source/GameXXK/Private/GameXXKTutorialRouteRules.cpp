#include "GameXXKTutorialRouteRules.h"

namespace GameXXKTutorialRouteRulesPrivate
{
	bool SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	FGameXXKTutorialRouteNodeDefinition Node(
		const TCHAR* NodeId,
		const EGameXXKTutorialRouteNodeKind Kind,
		const TCHAR* GuideId,
		const double X)
	{
		FGameXXKTutorialRouteNodeDefinition Result;
		Result.NodeId = NodeId;
		Result.Kind = Kind;
		Result.GuideId = GuideId;
		Result.NormalizedPosition = FVector2D(X, 0.5);
		return Result;
	}

	FGameXXKTutorialRouteEdge Edge(const FName FromNodeId, const FName ToNodeId)
	{
		FGameXXKTutorialRouteEdge Result;
		Result.FromNodeId = FromNodeId;
		Result.ToNodeId = ToNodeId;
		return Result;
	}

	bool ValidateDefinition(const FGameXXKTutorialRouteDefinition& Definition, FString* OutError)
	{
		if (Definition.RouteId != TEXT("Route.Tutorial.CombatBasics")
			|| Definition.Nodes.Num() != 8
			|| Definition.Edges.Num() != 7)
		{
			return SetError(OutError, TEXT("Tutorial route definition has an invalid identity or size."));
		}
		TSet<FName> NodeIds;
		for (const FGameXXKTutorialRouteNodeDefinition& RouteNode : Definition.Nodes)
		{
			if (RouteNode.NodeId.IsNone()
				|| RouteNode.GuideId.IsNone()
				|| NodeIds.Contains(RouteNode.NodeId))
			{
				return SetError(OutError, TEXT("Tutorial route node IDs and guide IDs must be valid and unique."));
			}
			NodeIds.Add(RouteNode.NodeId);
		}
		for (int32 Index = 0; Index < Definition.Edges.Num(); ++Index)
		{
			if (Definition.Edges[Index].FromNodeId != Definition.Nodes[Index].NodeId
				|| Definition.Edges[Index].ToNodeId != Definition.Nodes[Index + 1].NodeId)
			{
				return SetError(OutError, TEXT("Tutorial route edges must be adjacent and forward-only."));
			}
		}
		return true;
	}

	const FGameXXKTutorialCampActionDefinition* FindCampAction(
		const TArray<FGameXXKTutorialCampActionDefinition>& Actions,
		const EGameXXKTutorialCampChoice Choice)
	{
		return Actions.FindByPredicate([Choice](const FGameXXKTutorialCampActionDefinition& Action)
		{
			return Action.Choice == Choice;
		});
	}
}

FGameXXKTutorialRouteDefinition FGameXXKTutorialRouteRules::BuildDefinition(const int32 IgnoredRandomSeed)
{
	(void)IgnoredRandomSeed;
	using namespace GameXXKTutorialRouteRulesPrivate;
	FGameXXKTutorialRouteDefinition Definition;
	Definition.RouteId = TEXT("Route.Tutorial.CombatBasics");
	Definition.Nodes = {
		Node(TEXT("Tutorial.Start"), EGameXXKTutorialRouteNodeKind::Start, TEXT("Guide.RouteMap.Basic"), 0.04),
		Node(TEXT("Tutorial.Battle.0-1"), EGameXXKTutorialRouteNodeKind::Battle, TEXT("Guide.Battle.Basic"), 0.17),
		Node(TEXT("Tutorial.Merchant.0-1"), EGameXXKTutorialRouteNodeKind::Merchant, TEXT("Guide.Merchant.Basic"), 0.30),
		Node(TEXT("Tutorial.Event.0-1"), EGameXXKTutorialRouteNodeKind::Event, TEXT("Guide.Event.Basic"), 0.43),
		Node(TEXT("Tutorial.Camp.0-1"), EGameXXKTutorialRouteNodeKind::Camp, TEXT("Guide.Camp.Basic"), 0.57),
		Node(TEXT("Tutorial.Chest.0-1"), EGameXXKTutorialRouteNodeKind::Chest, TEXT("Guide.Chest.Basic"), 0.70),
		Node(TEXT("Tutorial.Boss.0-1"), EGameXXKTutorialRouteNodeKind::Boss, TEXT("Guide.Boss.Basic"), 0.83),
		Node(TEXT("Tutorial.Settlement"), EGameXXKTutorialRouteNodeKind::Settlement, TEXT("Guide.Settlement.Basic"), 0.96)};
	for (int32 Index = 0; Index + 1 < Definition.Nodes.Num(); ++Index)
	{
		Definition.Edges.Add(Edge(Definition.Nodes[Index].NodeId, Definition.Nodes[Index + 1].NodeId));
	}
	return Definition;
}

FGameXXKTutorialRouteProgress FGameXXKTutorialRouteRules::CreateInitialProgress(
	const FGameXXKTutorialRouteDefinition& Definition)
{
	FGameXXKTutorialRouteProgress Progress;
	FString Error;
	if (!GameXXKTutorialRouteRulesPrivate::ValidateDefinition(Definition, &Error))
	{
		return Progress;
	}
	Progress.CurrentNodeId = Definition.Nodes[0].NodeId;
	Progress.CompletedNodeIds.Add(Definition.Nodes[0].NodeId);
	Progress.ReachableNodeIds.Add(Definition.Nodes[1].NodeId);
	return Progress;
}

bool FGameXXKTutorialRouteRules::CompleteReachableNode(
	const FGameXXKTutorialRouteDefinition& Definition,
	const FName NodeId,
	FGameXXKTutorialRouteProgress& InOutProgress,
	FString* OutError)
{
	using namespace GameXXKTutorialRouteRulesPrivate;
	if (OutError)
	{
		OutError->Reset();
	}
	if (!ValidateDefinition(Definition, OutError))
	{
		return false;
	}
	if (InOutProgress.bCompleted
		|| NodeId.IsNone()
		|| InOutProgress.CompletedNodeIds.Contains(NodeId)
		|| !InOutProgress.ReachableNodeIds.Contains(NodeId))
	{
		return SetError(OutError, TEXT("Tutorial route node is not the next reachable incomplete node."));
	}
	const int32 NodeIndex = Definition.Nodes.IndexOfByPredicate([NodeId](const FGameXXKTutorialRouteNodeDefinition& RouteNode)
	{
		return RouteNode.NodeId == NodeId;
	});
	if (NodeIndex == INDEX_NONE)
	{
		return SetError(OutError, TEXT("Tutorial route node does not exist."));
	}

	FGameXXKTutorialRouteProgress Candidate = InOutProgress;
	Candidate.CurrentNodeId = NodeId;
	Candidate.CompletedNodeIds.Add(NodeId);
	Candidate.ReachableNodeIds.Reset();
	if (NodeIndex + 1 < Definition.Nodes.Num())
	{
		Candidate.ReachableNodeIds.Add(Definition.Nodes[NodeIndex + 1].NodeId);
	}
	else
	{
		Candidate.bCompleted = true;
	}
	InOutProgress = MoveTemp(Candidate);
	return true;
}

TArray<FGameXXKTutorialCampActionDefinition> FGameXXKTutorialRouteRules::GetCampActions()
{
	FGameXXKTutorialCampActionDefinition Heal;
	Heal.Choice = EGameXXKTutorialCampChoice::HealPartyThirtyPercent;
	Heal.HealingPercent = 30;

	FGameXXKTutorialCampActionDefinition Gold;
	Gold.Choice = EGameXXKTutorialCampChoice::GainRouteGold;
	Gold.RouteGold = 100;
	return {Heal, Gold};
}

bool FGameXXKTutorialRouteRules::ResolveCampChoice(
	const EGameXXKTutorialCampChoice Choice,
	TArray<FGameXXKTutorialPartyHealth>& InOutParty,
	int32& InOutRouteGold,
	FString* OutError)
{
	using namespace GameXXKTutorialRouteRulesPrivate;
	if (OutError)
	{
		OutError->Reset();
	}
	if (InOutParty.IsEmpty() || InOutParty.Num() > 3 || InOutRouteGold < 0)
	{
		return SetError(OutError, TEXT("Tutorial camp requires one to three active party members and nonnegative route gold."));
	}
	TSet<FName> UnitIds;
	for (const FGameXXKTutorialPartyHealth& Unit : InOutParty)
	{
		if (Unit.UnitId.IsNone()
			|| UnitIds.Contains(Unit.UnitId)
			|| Unit.MaxHealth <= 0
			|| Unit.CurrentHealth < 0
			|| Unit.CurrentHealth > Unit.MaxHealth)
		{
			return SetError(OutError, TEXT("Tutorial camp party health is invalid."));
		}
		UnitIds.Add(Unit.UnitId);
	}

	const TArray<FGameXXKTutorialCampActionDefinition> Actions = GetCampActions();
	const FGameXXKTutorialCampActionDefinition* Action = FindCampAction(Actions, Choice);
	if (!Action)
	{
		return SetError(OutError, TEXT("Tutorial camp choice is invalid."));
	}

	TArray<FGameXXKTutorialPartyHealth> CandidateParty = InOutParty;
	int32 CandidateGold = InOutRouteGold;
	if (Choice == EGameXXKTutorialCampChoice::HealPartyThirtyPercent)
	{
		for (FGameXXKTutorialPartyHealth& Unit : CandidateParty)
		{
			const int64 RequestedHealing =
				(static_cast<int64>(Unit.MaxHealth) * Action->HealingPercent + 99) / 100;
			Unit.CurrentHealth = static_cast<int32>(FMath::Min<int64>(
				Unit.MaxHealth,
				static_cast<int64>(Unit.CurrentHealth) + RequestedHealing));
		}
	}
	else if (Choice == EGameXXKTutorialCampChoice::GainRouteGold)
	{
		if (CandidateGold > MAX_int32 - Action->RouteGold)
		{
			return SetError(OutError, TEXT("Tutorial camp route gold would overflow."));
		}
		CandidateGold += Action->RouteGold;
	}
	else
	{
		return SetError(OutError, TEXT("Tutorial camp choice is unsupported."));
	}

	InOutParty = MoveTemp(CandidateParty);
	InOutRouteGold = CandidateGold;
	return true;
}
