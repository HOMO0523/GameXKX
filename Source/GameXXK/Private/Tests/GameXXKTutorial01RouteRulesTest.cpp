#include "MVP/GameXXKTutorial01RouteRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTutorial01RouteRulesTest,
	"GameXXK.Tutorial01.RouteRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTutorial01RouteRulesTest::RunTest(const FString& Parameters)
{
	FGameXXKTutorial01RouteState Route;
	FGameXXKTutorial01RouteRules::Initialize(Route);
	TestTrue(TEXT("start is already complete"),
		Route.VisitedNodeIds.Num() == 1
		&& Route.VisitedNodeIds.Contains(FGameXXKTutorial01RouteRules::StartNodeId));
	TestTrue(TEXT("battle is the only reachable node"),
		Route.ReachableNodeIds.Num() == 1
		&& Route.ReachableNodeIds.Contains(FGameXXKTutorial01RouteRules::BattleNodeId));
	TestFalse(TEXT("battle is not in progress initially"), Route.bBattleInProgress);
	TestFalse(TEXT("battle is not won initially"), Route.bBattleWon);

	const TArray<FGameXXKRouteMapNode> Nodes =
		FGameXXKTutorial01RouteRules::BuildNodes(Route);
	TestEqual(TEXT("fixed tutorial route has three nodes"), Nodes.Num(), 3);
	if (Nodes.Num() == 3)
	{
		TestEqual(TEXT("start node id"), Nodes[0].NodeId,
			FGameXXKTutorial01RouteRules::StartNodeId);
		TestEqual(TEXT("battle node id"), Nodes[1].NodeId,
			FGameXXKTutorial01RouteRules::BattleNodeId);
		TestEqual(TEXT("return node id"), Nodes[2].NodeId,
			FGameXXKTutorial01RouteRules::ReturnTownNodeId);
		TestEqual(TEXT("start node kind"), Nodes[0].NodeKind, EGameXXKNodeKind::Start);
		TestEqual(TEXT("battle node kind"), Nodes[1].NodeKind, EGameXXKNodeKind::Battle);
		TestEqual(TEXT("return node uses a non-combat icon kind"), Nodes[2].NodeKind, EGameXXKNodeKind::Camp);
		TestEqual(TEXT("start position"), Nodes[0].NormalizedPosition, FVector2D(0.50f, 0.12f));
		TestEqual(TEXT("battle position"), Nodes[1].NormalizedPosition, FVector2D(0.50f, 0.50f));
		TestEqual(TEXT("return position"), Nodes[2].NormalizedPosition, FVector2D(0.50f, 0.86f));
	}

	const TArray<FGameXXKRouteMapEdge> Edges =
		FGameXXKTutorial01RouteRules::BuildEdges();
	TestEqual(TEXT("fixed tutorial route has two edges"), Edges.Num(), 2);
	if (Edges.Num() == 2)
	{
		TestEqual(TEXT("start connects to battle"), Edges[0].FromNodeId,
			FGameXXKTutorial01RouteRules::StartNodeId);
		TestEqual(TEXT("start edge target"), Edges[0].ToNodeId,
			FGameXXKTutorial01RouteRules::BattleNodeId);
		TestEqual(TEXT("battle connects to return"), Edges[1].FromNodeId,
			FGameXXKTutorial01RouteRules::BattleNodeId);
		TestEqual(TEXT("battle edge target"), Edges[1].ToNodeId,
			FGameXXKTutorial01RouteRules::ReturnTownNodeId);
	}

	const TMap<int32, FText> Labels = FGameXXKTutorial01RouteRules::BuildLabels();
	TestEqual(TEXT("start label"), Labels.FindRef(FGameXXKTutorial01RouteRules::StartNodeId).ToString(), FString(TEXT("起点")));
	TestEqual(TEXT("battle label"), Labels.FindRef(FGameXXKTutorial01RouteRules::BattleNodeId).ToString(), FString(TEXT("0-1 战斗")));
	TestEqual(TEXT("return label"), Labels.FindRef(FGameXXKTutorial01RouteRules::ReturnTownNodeId).ToString(), FString(TEXT("返回青山镇")));
	TestTrue(TEXT("completion notice starts empty"),
		FGameXXKTutorial01RouteRules::BuildCompletionNotice(Route).IsEmpty());

	EGameXXKTutorial01RouteAction Action = EGameXXKTutorial01RouteAction::None;
	TestFalse(TEXT("start cannot execute"),
		FGameXXKTutorial01RouteRules::RequestNode(
			Route,
			FGameXXKTutorial01RouteRules::StartNodeId,
			Action));
	TestFalse(TEXT("return cannot execute before victory"),
		FGameXXKTutorial01RouteRules::RequestNode(
			Route,
			FGameXXKTutorial01RouteRules::ReturnTownNodeId,
			Action));
	TestTrue(TEXT("battle executes once"),
		FGameXXKTutorial01RouteRules::RequestNode(
			Route,
			FGameXXKTutorial01RouteRules::BattleNodeId,
			Action));
	TestEqual(TEXT("battle action"), Action, EGameXXKTutorial01RouteAction::StartBattle);
	TestTrue(TEXT("battle enters progress"), Route.bBattleInProgress);
	TestFalse(TEXT("duplicate battle selection rejects"),
		FGameXXKTutorial01RouteRules::RequestNode(
			Route,
			FGameXXKTutorial01RouteRules::BattleNodeId,
			Action));

	FGameXXKTutorial01RouteRules::MarkBattleAborted(Route);
	TestFalse(TEXT("abort clears in-progress state"), Route.bBattleInProgress);
	TestTrue(TEXT("abort re-enables battle"),
		Route.ReachableNodeIds.Contains(FGameXXKTutorial01RouteRules::BattleNodeId));
	TestTrue(TEXT("battle can restart after abort"),
		FGameXXKTutorial01RouteRules::RequestNode(
			Route,
			FGameXXKTutorial01RouteRules::BattleNodeId,
			Action));
	TestTrue(TEXT("victory advances route"),
		FGameXXKTutorial01RouteRules::MarkVictory(Route));
	TestTrue(TEXT("battle is complete"),
		Route.VisitedNodeIds.Num() == 2
		&& Route.VisitedNodeIds.Contains(FGameXXKTutorial01RouteRules::StartNodeId)
		&& Route.VisitedNodeIds.Contains(FGameXXKTutorial01RouteRules::BattleNodeId));
	TestTrue(TEXT("return is now the only reachable node"),
		Route.ReachableNodeIds.Num() == 1
		&& Route.ReachableNodeIds.Contains(FGameXXKTutorial01RouteRules::ReturnTownNodeId));
	TestEqual(TEXT("completion notice is exact"),
		FGameXXKTutorial01RouteRules::BuildCompletionNotice(Route).ToString(),
		FString(TEXT("0-1 完成")));
	TestFalse(TEXT("victory cannot apply twice"),
		FGameXXKTutorial01RouteRules::MarkVictory(Route));
	TestTrue(TEXT("return executes"),
		FGameXXKTutorial01RouteRules::RequestNode(
			Route,
			FGameXXKTutorial01RouteRules::ReturnTownNodeId,
			Action));
	TestEqual(TEXT("return action"), Action, EGameXXKTutorial01RouteAction::ReturnTown);
	TestTrue(TEXT("return node is consumed"),
		Route.VisitedNodeIds.Contains(FGameXXKTutorial01RouteRules::ReturnTownNodeId)
		&& Route.ReachableNodeIds.IsEmpty());

	return true;
}

#endif
