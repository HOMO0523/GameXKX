#include "MVP/GameXXKTutorial01RouteRules.h"

void FGameXXKTutorial01RouteRules::Initialize(
	FGameXXKTutorial01RouteState& OutState)
{
	OutState = FGameXXKTutorial01RouteState();
	OutState.VisitedNodeIds.Add(StartNodeId);
	OutState.ReachableNodeIds.Add(BattleNodeId);
}

TArray<FGameXXKRouteMapNode> FGameXXKTutorial01RouteRules::BuildNodes(
	const FGameXXKTutorial01RouteState& State)
{
	(void)State;
	return {
		FGameXXKRouteMapNode(
			StartNodeId,
			0,
			0,
			EGameXXKNodeKind::Start,
			FVector2D(0.50f, 0.12f),
			{BattleNodeId}),
		FGameXXKRouteMapNode(
			BattleNodeId,
			1,
			0,
			EGameXXKNodeKind::Battle,
			FVector2D(0.50f, 0.50f),
			{ReturnTownNodeId}),
		FGameXXKRouteMapNode(
			ReturnTownNodeId,
			2,
			0,
			EGameXXKNodeKind::Camp,
			FVector2D(0.50f, 0.86f),
			{}),
	};
}

TArray<FGameXXKRouteMapEdge> FGameXXKTutorial01RouteRules::BuildEdges()
{
	return {
		FGameXXKRouteMapEdge(StartNodeId, BattleNodeId),
		FGameXXKRouteMapEdge(BattleNodeId, ReturnTownNodeId),
	};
}

TMap<int32, FText> FGameXXKTutorial01RouteRules::BuildLabels()
{
	return {
		{StartNodeId, FText::FromString(TEXT("起点"))},
		{BattleNodeId, FText::FromString(TEXT("0-1 战斗"))},
		{ReturnTownNodeId, FText::FromString(TEXT("返回青山镇"))},
	};
}

FText FGameXXKTutorial01RouteRules::BuildCompletionNotice(
	const FGameXXKTutorial01RouteState& State)
{
	return State.bBattleWon
		? FText::FromString(TEXT("0-1 完成"))
		: FText::GetEmpty();
}

bool FGameXXKTutorial01RouteRules::RequestNode(
	FGameXXKTutorial01RouteState& InOutState,
	const int32 NodeId,
	EGameXXKTutorial01RouteAction& OutAction)
{
	OutAction = EGameXXKTutorial01RouteAction::None;
	if (!InOutState.ReachableNodeIds.Contains(NodeId)
		|| InOutState.VisitedNodeIds.Contains(NodeId))
	{
		return false;
	}

	if (NodeId == BattleNodeId)
	{
		if (InOutState.bBattleInProgress || InOutState.bBattleWon)
		{
			return false;
		}
		InOutState.bBattleInProgress = true;
		InOutState.ReachableNodeIds.Remove(BattleNodeId);
		OutAction = EGameXXKTutorial01RouteAction::StartBattle;
		return true;
	}

	if (NodeId == ReturnTownNodeId && InOutState.bBattleWon)
	{
		InOutState.VisitedNodeIds.Add(ReturnTownNodeId);
		InOutState.ReachableNodeIds.Remove(ReturnTownNodeId);
		OutAction = EGameXXKTutorial01RouteAction::ReturnTown;
		return true;
	}

	return false;
}

bool FGameXXKTutorial01RouteRules::MarkVictory(
	FGameXXKTutorial01RouteState& InOutState)
{
	if (!InOutState.bBattleInProgress || InOutState.bBattleWon)
	{
		return false;
	}
	InOutState.bBattleInProgress = false;
	InOutState.bBattleWon = true;
	InOutState.VisitedNodeIds.Add(BattleNodeId);
	InOutState.ReachableNodeIds.Reset();
	InOutState.ReachableNodeIds.Add(ReturnTownNodeId);
	return true;
}

void FGameXXKTutorial01RouteRules::MarkBattleAborted(
	FGameXXKTutorial01RouteState& InOutState)
{
	if (!InOutState.bBattleInProgress || InOutState.bBattleWon)
	{
		return;
	}
	InOutState.bBattleInProgress = false;
	InOutState.ReachableNodeIds.Reset();
	InOutState.ReachableNodeIds.Add(BattleNodeId);
}
