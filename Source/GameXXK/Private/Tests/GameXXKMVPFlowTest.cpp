#include "GameXXKMVPRules.h"
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

	static bool RouteNodeCanReachKind(const FGameXXKRuntimeState& State, int32 StartNodeId, EGameXXKNodeKind TargetKind)
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
			const FGameXXKRouteMapNode* Node = FindRouteNodeById(State, NodeId);
			if (!Node)
			{
				continue;
			}
			if (Node->NodeKind == TargetKind)
			{
				return true;
			}
			for (int32 OutgoingNodeId : Node->OutgoingNodeIds)
			{
				Stack.Add(OutgoingNodeId);
			}
		}
		return false;
	}

	static const FGameXXKRouteMapNode* FindReachableRouteStepTowardKind(const FGameXXKRuntimeState& State, EGameXXKNodeKind TargetKind)
	{
		for (int32 NodeId : State.ReachableRouteNodeIds)
		{
			const FGameXXKRouteMapNode* Node = FindRouteNodeById(State, NodeId);
			if (Node && Node->NodeKind == TargetKind)
			{
				return Node;
			}
		}
		for (int32 NodeId : State.ReachableRouteNodeIds)
		{
			if (RouteNodeCanReachKind(State, NodeId, TargetKind))
			{
				return FindRouteNodeById(State, NodeId);
			}
		}
		return nullptr;
	}

	static bool AdvanceGeneratedRouteTowardKind(FGameXXKRuntimeState& State, EGameXXKNodeKind TargetKind)
	{
		for (int32 StepGuard = 0; StepGuard < 32 && State.Screen == EGameXXKScreen::DungeonMap; ++StepGuard)
		{
			const FGameXXKRouteMapNode* Node = FindReachableRouteStepTowardKind(State, TargetKind);
			if (!Node)
			{
				return false;
			}
			const int32 NodeId = Node->NodeId;
			const EGameXXKNodeKind NodeKind = Node->NodeKind;
			if (!UGameXXKMVPRules::SelectRouteNodeById(State, NodeId))
			{
				return false;
			}
			if (State.Screen == EGameXXKScreen::Battle && !UGameXXKMVPRules::ResolveBattleVictory(State, NodeKind == EGameXXKNodeKind::Boss))
			{
				return false;
			}
			if (State.Screen == EGameXXKScreen::RouteEvent && !UGameXXKMVPRules::ResolveEventReward(State, true))
			{
				return false;
			}
			if (State.Screen == EGameXXKScreen::RouteCamp && !UGameXXKMVPRules::ResolveCampReward(State, true))
			{
				return false;
			}
			if (State.Screen == EGameXXKScreen::RouteMerchant && !UGameXXKMVPRules::ResolveMerchantRouteNode(State))
			{
				return false;
			}
			if (NodeKind == TargetKind)
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMVPFullFlowTest,
	"GameXXK.MVP.FullFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMVPFullFlowTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();

	TestEqual(TEXT("main menu opens first"), State.Screen, EGameXXKScreen::MainMenu);
	TestEqual(TEXT("new game starts with enough gold to buy starter equipment"), State.PlayerGold, 50);
	TestTrue(TEXT("Qingshan starts unlocked"), State.UnlockedRegions.Contains(UGameXXKMVPRules::RegionQingshan()));
	TestFalse(TEXT("Tanjiang starts locked"), State.UnlockedRegions.Contains(UGameXXKMVPRules::RegionTanjiang()));

	TestTrue(TEXT("main menu opens world map"), UGameXXKMVPRules::OpenWorldMap(State));
	TestEqual(TEXT("world map visible"), State.Screen, EGameXXKScreen::WorldMap);
	TestFalse(TEXT("locked Tanjiang cannot be entered before Boss"), UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionTanjiang()));
	TestTrue(TEXT("world map enters unlocked Qingshan town"), UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan()));
	TestEqual(TEXT("town visible"), State.Screen, EGameXXKScreen::Town);
	TestFalse(TEXT("quest gate blocks dungeon before NPC"), UGameXXKMVPRules::CanEnterDungeon(State));

	TestTrue(TEXT("quest NPC accepts Huangshan route quest"), UGameXXKMVPRules::AcceptTownQuest(State));
	TestEqual(TEXT("quest state accepted"), State.QuestState, EGameXXKQuestState::Accepted);
	TestTrue(TEXT("follower joins after quest"), State.bFollowerJoined);
	TestTrue(TEXT("accepted quest opens dungeon"), UGameXXKMVPRules::CanEnterDungeon(State));

	State.PlayerGold = 500;
	const int32 GoldBeforeTrade = State.PlayerGold;
	TestTrue(TEXT("merchant buy healing item"), UGameXXKMVPRules::BuyItem(State, UGameXXKMVPRules::ItemHealingPowder(), 1));
	TestEqual(TEXT("buy spends gold"), State.PlayerGold, GoldBeforeTrade - 10);
	TestTrue(TEXT("merchant sell healing item"), UGameXXKMVPRules::SellItem(State, UGameXXKMVPRules::ItemHealingPowder(), 1));
	TestEqual(TEXT("sell refunds sell value"), State.PlayerGold, GoldBeforeTrade - 5);
	TestTrue(TEXT("merchant buy weapon"), UGameXXKMVPRules::BuyItem(State, UGameXXKMVPRules::ItemIronSword(), 1));
	const int32 AttackBeforeEquip = State.PlayerAttack;
	TestTrue(TEXT("town can equip weapon"), UGameXXKMVPRules::EquipItem(State, UGameXXKMVPRules::ItemIronSword()));
	TestEqual(TEXT("PPT Qingfeng sword attack bonus applies"), State.PlayerAttack, AttackBeforeEquip + 8);
	TestTrue(TEXT("equipping the same weapon again is idempotent"), UGameXXKMVPRules::EquipItem(State, UGameXXKMVPRules::ItemIronSword()));
	TestEqual(TEXT("repeated equipment does not stack attack"), State.PlayerAttack, AttackBeforeEquip + 8);
	TestTrue(TEXT("merchant buys PPT bamboo light armor"), UGameXXKMVPRules::BuyItem(State, UGameXXKMVPRules::ItemClothArmor(), 1));
	const int32 DefenseBeforeArmor = State.PlayerDefense;
	const int32 MaxHPBeforeArmor = State.PlayerMaxHP;
	TestTrue(TEXT("town can equip armor"), UGameXXKMVPRules::EquipItem(State, UGameXXKMVPRules::ItemClothArmor()));
	TestEqual(TEXT("PPT bamboo light armor defense bonus applies"), State.PlayerDefense, DefenseBeforeArmor + 6);
	TestEqual(TEXT("PPT bamboo light armor does not add HP"), State.PlayerMaxHP, MaxHPBeforeArmor);
	const FName CraneTalisman(TEXT("Item.CranePatternTalisman"));
	TestTrue(TEXT("merchant buys PPT HP accessory"), UGameXXKMVPRules::BuyItem(State, CraneTalisman, 1));
	const int32 MaxHPBeforeAccessory = State.PlayerMaxHP;
	TestTrue(TEXT("town can equip HP accessory"), UGameXXKMVPRules::EquipItem(State, CraneTalisman));
	TestEqual(TEXT("PPT crane talisman HP bonus applies"), State.PlayerMaxHP, MaxHPBeforeAccessory + 30);
	const FName InkstonePendant(TEXT("Item.InkstonePendant"));
	TestTrue(TEXT("merchant buys PPT MP accessory"), UGameXXKMVPRules::BuyItem(State, InkstonePendant, 1));
	const int32 MaxMPBeforePendant = State.PlayerMaxMP;
	TestTrue(TEXT("town can switch accessory to MP pendant"), UGameXXKMVPRules::EquipItem(State, InkstonePendant));
	TestEqual(TEXT("switching accessory removes previous HP bonus"), State.PlayerMaxHP, MaxHPBeforeAccessory);
	TestEqual(TEXT("PPT inkstone pendant MP bonus applies"), State.PlayerMaxMP, MaxMPBeforePendant + 20);
	const FName QingxinTea(TEXT("Item.QingxinTea"));
	TestTrue(TEXT("merchant buys PPT MP consumable"), UGameXXKMVPRules::BuyItem(State, QingxinTea, 1));
	State.PlayerMP = 0;
	TestTrue(TEXT("PPT Qingxin tea can be used from inventory"), UGameXXKMVPRules::UseItem(State, QingxinTea));
	TestEqual(TEXT("Qingxin tea restores 20 MP"), State.PlayerMP, 20);

	TestTrue(TEXT("return to world map before selecting next destination"), UGameXXKMVPRules::OpenWorldMap(State));
	TestFalse(TEXT("world map does not open small-map route directly"), UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionHuangshan()));
	TestTrue(TEXT("world map can re-enter current town"), UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("enter Huangshan dungeon from town quest gate"), UGameXXKMVPRules::EnterDungeon(State));
	TestEqual(TEXT("dungeon map visible"), State.Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("generated route map has full node set"), State.RouteMapNodes.Num() >= 15);
	TestTrue(TEXT("generated route map starts with one clickable node"), State.ReachableRouteNodeIds.Num() == 1 && State.ReachableRouteNodeIds.Contains(0));

	TestTrue(TEXT("start node advances"), UGameXXKMVPRules::AdvanceDungeonNode(State, EGameXXKNodeKind::Start));
	TestTrue(TEXT("battle node opens battle"), UGameXXKMVPRules::AdvanceDungeonNode(State, EGameXXKNodeKind::Battle));
	TestEqual(TEXT("battle screen visible"), State.Screen, EGameXXKScreen::Battle);
	const TArray<FName> Order = UGameXXKMVPRules::BuildTurnOrder(State, false);
	TestEqual(TEXT("speed order fastest wolf"), Order[0], FName(TEXT("Wolf")));
	TestEqual(TEXT("speed order then player"), Order[1], FName(TEXT("Player")));

	const int32 GoldBeforeBattle = State.PlayerGold;
	TestTrue(TEXT("normal battle grants XP/gold/items"), UGameXXKMVPRules::ResolveBattleVictory(State, false));
	TestTrue(TEXT("battle reward gold kept"), State.PlayerGold > GoldBeforeBattle);
	TestTrue(TEXT("battle reward XP kept"), State.PlayerXP > 0);
	TestTrue(TEXT("battle reward item kept"), UGameXXKMVPRules::GetItemCount(State, UGameXXKMVPRules::ItemHealingPowder()) > 0);

	State.PlayerHP = 40;
	const int32 PotionBeforeUse = UGameXXKMVPRules::GetItemCount(State, UGameXXKMVPRules::ItemHealingPowder());
	TestTrue(TEXT("dungeon consumable can be used"), UGameXXKMVPRules::UseHealingItem(State));
	TestEqual(TEXT("used consumable deducted"), UGameXXKMVPRules::GetItemCount(State, UGameXXKMVPRules::ItemHealingPowder()), PotionBeforeUse - 1);
	TestTrue(TEXT("healing item restores HP"), State.PlayerHP > 40);

	const int32 GoldBeforeFailure = State.PlayerGold;
	const int32 XPBeforeFailure = State.PlayerXP;
	const int32 PotionAfterUse = UGameXXKMVPRules::GetItemCount(State, UGameXXKMVPRules::ItemHealingPowder());
	TestTrue(TEXT("dungeon failure returns to town"), UGameXXKMVPRules::FailDungeonToTown(State));
	TestEqual(TEXT("failure screen town"), State.Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("failure keeps accepted quest"), State.QuestState, EGameXXKQuestState::Accepted);
	TestTrue(TEXT("failure keeps follower"), State.bFollowerJoined);
	TestEqual(TEXT("failure keeps earned gold"), State.PlayerGold, GoldBeforeFailure);
	TestEqual(TEXT("failure keeps earned XP"), State.PlayerXP, XPBeforeFailure);
	TestEqual(TEXT("failure keeps consumable deduction"), UGameXXKMVPRules::GetItemCount(State, UGameXXKMVPRules::ItemHealingPowder()), PotionAfterUse);

	TestTrue(TEXT("retry enters dungeon"), UGameXXKMVPRules::EnterDungeon(State));
	TestTrue(TEXT("retry start node"), UGameXXKMVPRules::AdvanceDungeonNode(State, EGameXXKNodeKind::Start));
	State.PlayerHP = 1;
	TestTrue(TEXT("generated route reaches a camp node"), AdvanceGeneratedRouteTowardKind(State, EGameXXKNodeKind::Camp));
	TestEqual(TEXT("camp restored full HP"), State.PlayerHP, State.PlayerMaxHP);
	TestTrue(TEXT("generated route reaches and clears boss"), AdvanceGeneratedRouteTowardKind(State, EGameXXKNodeKind::Boss));

	TestEqual(TEXT("Boss completes quest"), State.QuestState, EGameXXKQuestState::Completed);
	TestFalse(TEXT("follower leaves after clear"), State.bFollowerJoined);
	TestTrue(TEXT("Boss unlocks Tanjiang"), State.UnlockedRegions.Contains(UGameXXKMVPRules::RegionTanjiang()));
	TestTrue(TEXT("world map enters Tanjiang after unlock"), UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionTanjiang()));
	TestEqual(TEXT("Tanjiang town visible"), State.Screen, EGameXXKScreen::Town);

	const FGameXXKSaveState Save = UGameXXKMVPRules::MakeSaveState(State);
	const FGameXXKRuntimeState Restored = UGameXXKMVPRules::RestoreFromSaveState(Save);
	TestEqual(TEXT("save restore persists quest"), Restored.QuestState, State.QuestState);
	TestEqual(TEXT("save restore persists level"), Restored.PlayerLevel, State.PlayerLevel);
	TestEqual(TEXT("save restore persists XP"), Restored.PlayerXP, State.PlayerXP);
	TestEqual(TEXT("save restore persists gold"), Restored.PlayerGold, State.PlayerGold);
	TestTrue(TEXT("save restore persists unlock"), Restored.UnlockedRegions.Contains(UGameXXKMVPRules::RegionTanjiang()));
	TestEqual(TEXT("save restore persists inventory details"), Restored.Inventory.Num(), State.Inventory.Num());
	TestEqual(TEXT("save restore keeps current screen"), Restored.Screen, State.Screen);
	TestEqual(TEXT("save restore keeps current region"), Restored.CurrentRegion, State.CurrentRegion);

	return true;
}

#endif
