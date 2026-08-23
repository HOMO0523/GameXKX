#include "GameXXKMVPRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKRouteEconomyRules.h"
#include "GameXXKRelicRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	static const FGameXXKRouteMapNode* FindRouteNodeById(const FGameXXKRuntimeState& State, int32 NodeId)
	{
		return State.RouteMapNodes.FindByPredicate([NodeId](const FGameXXKRouteMapNode& Node)
		{
			return Node.NodeId == NodeId;
		});
	}

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

	static int32 CountNodesInLayer(const FGameXXKRuntimeState& State, int32 LayerIndex)
	{
		int32 Count = 0;
		for (const FGameXXKRouteMapNode& Node : State.RouteMapNodes)
		{
			if (Node.LayerIndex == LayerIndex)
			{
				++Count;
			}
		}
		return Count;
	}

	static int32 CountIncomingEdges(const FGameXXKRuntimeState& State, int32 NodeId)
	{
		int32 Count = 0;
		for (const FGameXXKRouteMapEdge& Edge : State.RouteMapEdges)
		{
			if (Edge.ToNodeId == NodeId)
			{
				++Count;
			}
		}
		return Count;
	}

	static TSet<int32> BuildForwardReachableNodeIds(const FGameXXKRuntimeState& State, int32 StartNodeId)
	{
		TSet<int32> Visited;
		TArray<int32> Stack;
		Stack.Add(StartNodeId);
		while (!Stack.IsEmpty())
		{
			const int32 NodeId = Stack.Pop(EAllowShrinking::No);
			if (Visited.Contains(NodeId))
			{
				continue;
			}
			Visited.Add(NodeId);
			if (const FGameXXKRouteMapNode* Node = FindRouteNodeById(State, NodeId))
			{
				for (int32 OutgoingNodeId : Node->OutgoingNodeIds)
				{
					Stack.Add(OutgoingNodeId);
				}
			}
		}
		return Visited;
	}

	static bool HasPathToBoss(const FGameXXKRuntimeState& State, int32 StartNodeId, int32 BossNodeId)
	{
		return BuildForwardReachableNodeIds(State, StartNodeId).Contains(BossNodeId);
	}

	static FGameXXKRuntimeState BuildPendingRoomRouteState(EGameXXKNodeKind RoomKind)
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		if (!UGameXXKMVPRules::OpenWorldMap(State)
			|| !UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan())
			|| !UGameXXKMVPRules::AcceptTownQuest(State))
		{
			return FGameXXKRuntimeState();
		}
		State.RouteSeed = 404;
		if (!UGameXXKMVPRules::EnterDungeon(State))
		{
			return FGameXXKRuntimeState();
		}

		// EnterDungeon establishes the stable card-entry authority and route economy.
		// This fixture only replaces the visible topology with one focused room.
		State.bHasGeneratedRouteMap = true;
		State.CurrentRouteNodeId = 0;
		State.PendingRouteNodeId = INDEX_NONE;
		State.RouteMapNodes.Reset();
		State.RouteMapEdges.Reset();
		State.VisitedRouteNodeIds.Reset();
		State.ReachableRouteNodeIds.Reset();
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{0, 0, 0, EGameXXKNodeKind::Start, FVector2D(0.5f, 0.0f), TArray<int32>{1}});
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{1, 1, 0, RoomKind, FVector2D(0.5f, 0.35f), TArray<int32>{2}});
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{2, 2, 0, EGameXXKNodeKind::Boss, FVector2D(0.5f, 1.0f), TArray<int32>{}});
		State.RouteMapEdges.Add(FGameXXKRouteMapEdge{0, 1});
		State.RouteMapEdges.Add(FGameXXKRouteMapEdge{1, 2});
		State.VisitedRouteNodeIds.Add(0);
		State.ReachableRouteNodeIds.Add(1);
		return State;
	}

	static bool ForceActiveCardBattleVictory(FGameXXKRuntimeState& State)
	{
		if (!State.CardRun.bHasActiveCardBattle)
		{
			return false;
		}
		for (FGameXXKCardCombatUnit& Unit : State.CardRun.ActiveBattle.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Enemy)
			{
				Unit.HP = 0;
				Unit.bLiving = false;
			}
		}
		State.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
		return true;
	}

	static bool ResolvePendingRouteRoom(FGameXXKRuntimeState& State, EGameXXKNodeKind NodeKind)
	{
		if (State.Screen == EGameXXKScreen::Battle)
		{
			if (!ForceActiveCardBattleVictory(State))
			{
				return false;
			}
			if (!UGameXXKMVPRules::ResolveBattleVictory(State, NodeKind == EGameXXKNodeKind::Boss))
			{
				return false;
			}
			if (!State.CardRun.PendingReward.CardIds.IsEmpty() || !State.CardRun.PendingReward.Options.IsEmpty())
			{
				FString RewardError;
				return FGameXXKCardBattleAdapter::SkipPendingRouteReward(State, &RewardError)
					&& UGameXXKMVPRules::ResolveBattleVictory(State, NodeKind == EGameXXKNodeKind::Boss);
			}
			return State.Screen == EGameXXKScreen::DungeonMap;
		}
		if (State.Screen == EGameXXKScreen::RouteEvent)
		{
			if (NodeKind == EGameXXKNodeKind::Chest)
			{
				return UGameXXKMVPRules::ResolveRouteEncounterChoice(State, 0);
			}
			return UGameXXKMVPRules::ResolveEventReward(State, true);
		}
		if (State.Screen == EGameXXKScreen::RouteCamp)
		{
			const bool bOwnsLifeSavingTalisman = FGameXXKRelicRules::OwnsLifeSavingTalisman(State);
			return UGameXXKMVPRules::ResolveCampReward(State, !bOwnsLifeSavingTalisman);
		}
		if (State.Screen == EGameXXKScreen::RouteMerchant)
		{
			return UGameXXKMVPRules::ResolveMerchantRouteNode(State);
		}
		return State.Screen == EGameXXKScreen::DungeonMap;
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
	TestTrue(TEXT("generated full route has at least fifteen nodes"), Seed101A.RouteMapNodes.Num() >= 15);
	TestTrue(TEXT("generated route has branching edges"), Seed101A.RouteMapEdges.Num() > Seed101A.RouteMapNodes.Num());
	TestEqual(TEXT("generated route uses seven layers including start and boss"), CountNodesInLayer(Seed101A, 6), 1);
	TestEqual(TEXT("generated route has one start node"), CountNodesInLayer(Seed101A, 0), 1);
	for (int32 LayerIndex = 1; LayerIndex <= 5; ++LayerIndex)
	{
		const int32 LayerNodeCount = CountNodesInLayer(Seed101A, LayerIndex);
		TestTrue(
			FString::Printf(TEXT("generated route layer %d has two to four nodes"), LayerIndex),
			LayerNodeCount >= 2 && LayerNodeCount <= 4);
	}
	const bool bHasBranchingNode = Seed101A.RouteMapNodes.ContainsByPredicate([](const FGameXXKRouteMapNode& Node)
	{
		return Node.OutgoingNodeIds.Num() > 1;
	});
	TestTrue(TEXT("generated route has at least one branching node"), bHasBranchingNode);
	const FGameXXKRouteMapNode* StartNode = Seed101A.RouteMapNodes.FindByPredicate([](const FGameXXKRouteMapNode& Node)
	{
		return Node.LayerIndex == 0 && Node.NodeKind == EGameXXKNodeKind::Start;
	});
	const FGameXXKRouteMapNode* BossNode = Seed101A.RouteMapNodes.FindByPredicate([](const FGameXXKRouteMapNode& Node)
	{
		return Node.LayerIndex == 6 && Node.NodeKind == EGameXXKNodeKind::Boss;
	});
	TestNotNull(TEXT("generated route has a start node in layer zero"), StartNode);
	TestNotNull(TEXT("generated route has a boss node in final layer"), BossNode);
	if (!StartNode || !BossNode)
	{
		return false;
	}
	const TSet<int32> ReachableFromStart = BuildForwardReachableNodeIds(Seed101A, StartNode->NodeId);
	for (const FGameXXKRouteMapNode& Node : Seed101A.RouteMapNodes)
	{
		TestTrue(
			FString::Printf(TEXT("node %d is reachable from start"), Node.NodeId),
			ReachableFromStart.Contains(Node.NodeId));
		TestTrue(
			FString::Printf(TEXT("node %d can reach boss"), Node.NodeId),
			HasPathToBoss(Seed101A, Node.NodeId, BossNode->NodeId));
		if (Node.LayerIndex > 0)
		{
			TestTrue(
				FString::Printf(TEXT("node %d has incoming route line"), Node.NodeId),
				CountIncomingEdges(Seed101A, Node.NodeId) > 0);
		}
		if (Node.LayerIndex < BossNode->LayerIndex)
		{
			TestTrue(
				FString::Printf(TEXT("node %d has outgoing route line"), Node.NodeId),
				Node.OutgoingNodeIds.Num() > 0);
			TestTrue(
				FString::Printf(TEXT("node %d has at most three outgoing route lines"), Node.NodeId),
				Node.OutgoingNodeIds.Num() <= 3);
		}
	}
	TestEqual(TEXT("generated route starts with one reachable node"), Seed101A.ReachableRouteNodeIds.Num(), 1);
	TestTrue(TEXT("generated route starts with reachable start node"), Seed101A.ReachableRouteNodeIds.Contains(0));

	FGameXXKRuntimeState Seed101B = UGameXXKMVPRules::CreateNewGame();
	UGameXXKMVPRules::GenerateRouteMapForSeed(Seed101B, 101);
	TestEqual(TEXT("same seed produces same route signature"), BuildRouteSignature(Seed101B), BuildRouteSignature(Seed101A));

	FGameXXKRuntimeState Seed102 = UGameXXKMVPRules::CreateNewGame();
	UGameXXKMVPRules::GenerateRouteMapForSeed(Seed102, 102);
	TestNotEqual(TEXT("different seed changes visible route signature"), BuildRouteSignature(Seed102), BuildRouteSignature(Seed101A));

	FGameXXKRuntimeState SeedZero = UGameXXKMVPRules::CreateNewGame();
	UGameXXKMVPRules::GenerateRouteMapForSeed(SeedZero, 0);
	TestEqual(TEXT("zero seed normalizes to one"), SeedZero.RouteSeed, 1);

	FGameXXKRuntimeState SeedNegative = UGameXXKMVPRules::CreateNewGame();
	UGameXXKMVPRules::GenerateRouteMapForSeed(SeedNegative, -42);
	TestEqual(TEXT("negative seed stores positive magnitude"), SeedNegative.RouteSeed, 42);

	FGameXXKRuntimeState SeedMinInt = UGameXXKMVPRules::CreateNewGame();
	UGameXXKMVPRules::GenerateRouteMapForSeed(SeedMinInt, MIN_int32);
	TestTrue(TEXT("minimum int seed stores positive nonzero"), SeedMinInt.RouteSeed > 0);

	UGameInstance* EntryGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* EntrySubsystem = NewObject<UGameXXKMVPSubsystem>(EntryGameInstance);
	TestTrue(TEXT("entry starts game"), EntrySubsystem->StartGame());
	TestTrue(TEXT("entry selects Qingshan"), EntrySubsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("entry accepts quest"), EntrySubsystem->AcceptQuest());
	TestTrue(TEXT("entry opens dungeon route map"), EntrySubsystem->OpenDungeonFromTownExit());
	TestNotEqual(TEXT("dungeon entry assigns a nonzero route seed"), EntrySubsystem->GetRuntimeState().RouteSeed, 0);

	FGameXXKRuntimeState FlowState = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("generated route flow reaches accepted Qingshan town"),
		UGameXXKMVPRules::OpenWorldMap(FlowState)
			&& UGameXXKMVPRules::EnterWorldRegion(FlowState, UGameXXKMVPRules::RegionQingshan())
			&& UGameXXKMVPRules::AcceptTownQuest(FlowState));
	FlowState.RouteSeed = 101;
	TestTrue(TEXT("generated route flow enters through the canonical route initializer"),
		UGameXXKMVPRules::EnterDungeon(FlowState));
	TestTrue(TEXT("start node selection succeeds"), UGameXXKMVPRules::SelectRouteNodeById(FlowState, 0));
	TestEqual(TEXT("non-combat start keeps route map active"), FlowState.Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("start node is visited"), FlowState.VisitedRouteNodeIds.Contains(0));
	TestTrue(TEXT("start node opens multiple first-layer choices"), FlowState.ReachableRouteNodeIds.Num() > 1);

	const TArray<int32> FirstLayerChoices = FlowState.ReachableRouteNodeIds;
	const int32 ChosenFirstLayerNodeId = FirstLayerChoices[0];
	const FGameXXKRouteMapNode* ChosenFirstLayerNode = FindRouteNodeById(FlowState, ChosenFirstLayerNodeId);
	TestNotNull(TEXT("chosen first-layer node exists"), ChosenFirstLayerNode);
	if (!ChosenFirstLayerNode)
	{
		return false;
	}
	const TArray<int32> ExpectedNextReachable = ChosenFirstLayerNode->OutgoingNodeIds;
	TestTrue(TEXT("chosen first-layer node selection succeeds"), UGameXXKMVPRules::SelectRouteNodeById(FlowState, ChosenFirstLayerNodeId));
	TestTrue(TEXT("chosen first-layer room resolves back to route map"), ResolvePendingRouteRoom(FlowState, ChosenFirstLayerNode->NodeKind));
	for (int32 FirstLayerNodeId : FirstLayerChoices)
	{
		if (FirstLayerNodeId != ChosenFirstLayerNodeId)
		{
			TestFalse(
				FString::Printf(TEXT("unchosen first-layer node %d is no longer reachable"), FirstLayerNodeId),
				FlowState.ReachableRouteNodeIds.Contains(FirstLayerNodeId));
		}
	}
	for (int32 ExpectedNodeId : ExpectedNextReachable)
	{
		TestTrue(
			FString::Printf(TEXT("chosen node outgoing %d is reachable"), ExpectedNodeId),
			FlowState.ReachableRouteNodeIds.Contains(ExpectedNodeId));
	}
	TestEqual(TEXT("route progression only keeps the chosen node outgoing frontier"), FlowState.ReachableRouteNodeIds.Num(), ExpectedNextReachable.Num());

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

	TestTrue(TEXT("combat victory fixture marks all card-runtime enemies defeated"), ForceActiveCardBattleVictory(FlowState));
	TestTrue(TEXT("battle victory opens the saved tiered three-choice route reward"), UGameXXKMVPRules::ResolveBattleVictory(FlowState, false));
	TestEqual(TEXT("pending battle reward keeps the player on the battle screen"), FlowState.Screen, EGameXXKScreen::Battle);
	TestEqual(TEXT("pending battle reward presents exactly three route reward options"), FlowState.CardRun.PendingReward.Options.Num(), 3);
	FString RewardError;
	TestTrue(FString::Printf(TEXT("skipping the pending reward unlocks route completion: %s"), *RewardError),
		FGameXXKCardBattleAdapter::SkipPendingRouteReward(FlowState, &RewardError));
	TestTrue(TEXT("resolved battle reward completes the pending route node"), UGameXXKMVPRules::ResolveBattleVictory(FlowState, false));
	TestEqual(TEXT("battle victory returns to route map"), FlowState.Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("pending node is cleared"), FlowState.PendingRouteNodeId, INDEX_NONE);
	TestTrue(TEXT("battle node is visited after victory"), FlowState.VisitedRouteNodeIds.Contains(CombatNodeId));

	FGameXXKRuntimeState EventState = BuildPendingRoomRouteState(EGameXXKNodeKind::Event);
	const int32 EventGoldBefore = EventState.PlayerGold;
	const int32 EventTravelMoneyBefore = EventState.CardRun.RouteTravelMoney;
	TestTrue(TEXT("event route node selection succeeds"), UGameXXKMVPRules::SelectRouteNodeById(EventState, 1));
	TestEqual(TEXT("event route node opens event scene"), EventState.Screen, EGameXXKScreen::RouteEvent);
	TestEqual(TEXT("event route node becomes pending"), EventState.PendingRouteNodeId, 1);
	TestFalse(TEXT("pending event route node is not visited until event scene resolves"), EventState.VisitedRouteNodeIds.Contains(1));
	TestTrue(TEXT("pending formal event resolves through an explicit catalog choice"), UGameXXKMVPRules::ResolveRouteEncounterChoice(EventState, 0));
	TestEqual(TEXT("event reward returns to route map"), EventState.Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("event reward clears pending node"), EventState.PendingRouteNodeId, INDEX_NONE);
	TestTrue(TEXT("event reward marks node visited"), EventState.VisitedRouteNodeIds.Contains(1));
	TestEqual(TEXT("event reward does not grant permanent gold"), EventState.PlayerGold, EventGoldBefore);
	TestEqual(TEXT("formal event choices add no hidden base travel money"), EventState.CardRun.RouteTravelMoney, EventTravelMoneyBefore);

	FGameXXKRuntimeState ChestState = BuildPendingRoomRouteState(EGameXXKNodeKind::Chest);
	TestTrue(TEXT("chest route node selection succeeds"), UGameXXKMVPRules::SelectRouteNodeById(ChestState, 1));
	TestEqual(TEXT("chest route node opens event scene"), ChestState.Screen, EGameXXKScreen::RouteEvent);
	TestEqual(TEXT("chest route node becomes pending"), ChestState.PendingRouteNodeId, 1);
	TestFalse(TEXT("pending chest route node is not visited until event scene resolves"), ChestState.VisitedRouteNodeIds.Contains(1));
	TestEqual(TEXT("chest route node offers three relic choices"), ChestState.CardRun.PendingRelicOffer.RelicIds.Num(), 3);
	const FName ChosenChestRelic = ChestState.CardRun.PendingRelicOffer.RelicIds.IsValidIndex(0)
		? ChestState.CardRun.PendingRelicOffer.RelicIds[0]
		: NAME_None;
	TestTrue(TEXT("pending chest reward resolves through its explicit relic choice"), UGameXXKMVPRules::ResolveRouteEncounterChoice(ChestState, 0));
	TestEqual(TEXT("chest reward returns to route map"), ChestState.Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("chest reward marks node visited"), ChestState.VisitedRouteNodeIds.Contains(1));
	TestTrue(TEXT("chest reward grants the selected relic"), ChestState.CardRun.Relics.ContainsByPredicate([ChosenChestRelic](const FGameXXKRelicInstance& Relic)
	{
		return Relic.RelicId == ChosenChestRelic;
	}));

	FGameXXKRuntimeState CampState = BuildPendingRoomRouteState(EGameXXKNodeKind::Camp);
	CampState.PlayerHP = 1;
	TestTrue(TEXT("camp route node selection succeeds"), UGameXXKMVPRules::SelectRouteNodeById(CampState, 1));
	TestEqual(TEXT("camp route node opens camp scene"), CampState.Screen, EGameXXKScreen::RouteCamp);
	TestEqual(TEXT("camp route node becomes pending"), CampState.PendingRouteNodeId, 1);
	TestFalse(TEXT("pending camp route node is not visited until camp scene resolves"), CampState.VisitedRouteNodeIds.Contains(1));
	TestTrue(TEXT("pending camp reward resolves from camp scene"), UGameXXKMVPRules::ResolveCampReward(CampState, true));
	TestEqual(TEXT("camp reward returns to route map"), CampState.Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("camp reward marks node visited"), CampState.VisitedRouteNodeIds.Contains(1));
	TestEqual(TEXT("camp reward never heals player directly"), CampState.PlayerHP, 1);
	TestTrue(TEXT("camp reward grants the life-saving talisman"), FGameXXKRelicRules::OwnsLifeSavingTalisman(CampState));

	FGameXXKRuntimeState MerchantState = BuildPendingRoomRouteState(EGameXXKNodeKind::Merchant);
	TestTrue(TEXT("merchant route node selection succeeds"), UGameXXKMVPRules::SelectRouteNodeById(MerchantState, 1));
	TestEqual(TEXT("merchant route node opens merchant scene"), MerchantState.Screen, EGameXXKScreen::RouteMerchant);
	TestEqual(TEXT("merchant route node becomes pending"), MerchantState.PendingRouteNodeId, 1);
	TestFalse(TEXT("pending merchant route node is not visited until merchant scene resolves"), MerchantState.VisitedRouteNodeIds.Contains(1));
	TestTrue(TEXT("pending merchant resolves from merchant scene"), UGameXXKMVPRules::ResolveMerchantRouteNode(MerchantState));
	TestEqual(TEXT("merchant completion returns to route map"), MerchantState.Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("merchant completion marks node visited"), MerchantState.VisitedRouteNodeIds.Contains(1));

	return true;
}

#endif
