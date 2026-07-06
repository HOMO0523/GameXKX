#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	static FString BuildRouteSignature(const FGameXXKRuntimeState& State)
	{
		FString Signature;
		for (const FGameXXKRouteMapNode& Node : State.RouteMapNodes)
		{
			Signature += FString::Printf(
				TEXT("N%d:%d:%d:%d:%.2f:%.2f|"),
				Node.NodeId,
				Node.LayerIndex,
				Node.ColumnIndex,
				static_cast<int32>(Node.NodeKind),
				Node.NormalizedPosition.X,
				Node.NormalizedPosition.Y);
			for (int32 OutgoingNodeId : Node.OutgoingNodeIds)
			{
				Signature += FString::Printf(TEXT(">%d"), OutgoingNodeId);
			}
			Signature += TEXT(";");
		}
		for (const FGameXXKRouteMapEdge& Edge : State.RouteMapEdges)
		{
			Signature += FString::Printf(TEXT("E%d>%d;"), Edge.FromNodeId, Edge.ToNodeId);
		}
		return Signature;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMapSeedRulesTest,
	"GameXXK.MVP.RouteMap.SeedRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMapSeedRulesTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState Seed101A = UGameXXKMVPRules::CreateNewGame();
	UGameXXKMVPRules::GenerateRouteMapForSeed(Seed101A, 101);
	TestTrue(TEXT("explicit seed generates a route map"), Seed101A.bHasGeneratedRouteMap);
	TestEqual(TEXT("explicit seed persists on runtime state"), Seed101A.RouteSeed, 101);
	TestTrue(TEXT("generated route has at least ten nodes"), Seed101A.RouteMapNodes.Num() >= 10);
	TestTrue(TEXT("generated route has branching edges"), Seed101A.RouteMapEdges.Num() > Seed101A.RouteMapNodes.Num());
	const bool bHasBranchingNode = Seed101A.RouteMapNodes.ContainsByPredicate([](const FGameXXKRouteMapNode& Node)
	{
		return Node.OutgoingNodeIds.Num() > 1;
	});
	TestTrue(TEXT("generated route has at least one branching node"), bHasBranchingNode);
	TestEqual(TEXT("generated route starts with one reachable node"), Seed101A.ReachableRouteNodeIds.Num(), 1);
	TestTrue(TEXT("generated route starts with reachable start node"), Seed101A.ReachableRouteNodeIds.Contains(0));

	FGameXXKRuntimeState Seed101B = UGameXXKMVPRules::CreateNewGame();
	UGameXXKMVPRules::GenerateRouteMapForSeed(Seed101B, 101);
	TestEqual(TEXT("same seed produces same route signature"), BuildRouteSignature(Seed101B), BuildRouteSignature(Seed101A));

	FGameXXKRuntimeState Seed102 = UGameXXKMVPRules::CreateNewGame();
	UGameXXKMVPRules::GenerateRouteMapForSeed(Seed102, 102);
	TestNotEqual(TEXT("different seed changes visible route signature"), BuildRouteSignature(Seed102), BuildRouteSignature(Seed101A));

	UGameInstance* EntryGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* EntrySubsystem = NewObject<UGameXXKMVPSubsystem>(EntryGameInstance);
	TestTrue(TEXT("entry starts game"), EntrySubsystem->StartGame());
	TestTrue(TEXT("entry selects Qingshan"), EntrySubsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("entry accepts quest"), EntrySubsystem->AcceptQuest());
	TestTrue(TEXT("entry opens dungeon route map"), EntrySubsystem->OpenDungeonFromTownExit());
	TestNotEqual(TEXT("dungeon entry assigns a nonzero route seed"), EntrySubsystem->GetRuntimeState().RouteSeed, 0);

	FGameXXKRuntimeState& FlowState = Seed101A;
	FlowState.Screen = EGameXXKScreen::DungeonMap;
	FlowState.CurrentMapId = TEXT("HuangshanRoute");
	FlowState.bDungeonActive = true;
	TestTrue(TEXT("start node selection succeeds"), UGameXXKMVPRules::SelectRouteNodeById(FlowState, 0));
	TestEqual(TEXT("non-combat start keeps route map active"), FlowState.Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("start node is visited"), FlowState.VisitedRouteNodeIds.Contains(0));

	const FGameXXKRouteMapNode* ReachableCombatNode = FlowState.RouteMapNodes.FindByPredicate([&FlowState](const FGameXXKRouteMapNode& Node)
	{
		return FlowState.ReachableRouteNodeIds.Contains(Node.NodeId)
			&& (Node.NodeKind == EGameXXKNodeKind::Battle
				|| Node.NodeKind == EGameXXKNodeKind::Elite
				|| Node.NodeKind == EGameXXKNodeKind::Boss);
	});
	TestNotNull(TEXT("combat branch is reachable"), ReachableCombatNode);
	if (!ReachableCombatNode)
	{
		return false;
	}
	const int32 CombatNodeId = ReachableCombatNode->NodeId;

	TestTrue(TEXT("combat node selection succeeds"), UGameXXKMVPRules::SelectRouteNodeById(FlowState, CombatNodeId));
	TestEqual(TEXT("combat node opens battle screen"), FlowState.Screen, EGameXXKScreen::Battle);
	TestEqual(TEXT("combat node becomes pending, not visited"), FlowState.PendingRouteNodeId, CombatNodeId);
	TestFalse(TEXT("pending combat node waits for victory before visited"), FlowState.VisitedRouteNodeIds.Contains(CombatNodeId));

	TestTrue(TEXT("battle victory resolves pending route node"), UGameXXKMVPRules::ResolveBattleVictory(FlowState, false));
	TestEqual(TEXT("battle victory returns to route map"), FlowState.Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("pending node is cleared"), FlowState.PendingRouteNodeId, INDEX_NONE);
	TestTrue(TEXT("battle node is visited after victory"), FlowState.VisitedRouteNodeIds.Contains(CombatNodeId));

	return true;
}

#endif
