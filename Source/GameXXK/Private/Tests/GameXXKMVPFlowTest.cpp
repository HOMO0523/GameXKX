#include "GameXXKMVPRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKBattlePresentation.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKSaveMigration.h"
#include "MVP/GameXXKMVPSubsystem.h"

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

	static bool ResolveActiveCardBattleVictoryAndSkipReward(FGameXXKRuntimeState& State, const bool bBossBattle)
	{
		if (!ForceActiveCardBattleVictory(State)
			|| !UGameXXKMVPRules::ResolveBattleVictory(State, bBossBattle)
			|| State.CardRun.PendingReward.CardIds.Num() != 3)
		{
			return false;
		}
		FString RewardError;
		return FGameXXKCardBattleAdapter::SkipPendingRouteReward(State, &RewardError)
			&& UGameXXKMVPRules::ResolveBattleVictory(State, bBossBattle);
	}

	static bool ResolvePendingRouteChoice(FGameXXKRuntimeState& State)
	{
		for (int32 ChoiceIndex = 0; ChoiceIndex < 3; ++ChoiceIndex)
		{
			FGameXXKRuntimeState Candidate = State;
			if (UGameXXKMVPRules::ResolveRouteEncounterChoice(Candidate, ChoiceIndex))
			{
				State = MoveTemp(Candidate);
				return true;
			}
		}
		return false;
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
			if (State.Screen == EGameXXKScreen::Battle
				&& !ResolveActiveCardBattleVictoryAndSkipReward(State, NodeKind == EGameXXKNodeKind::Boss))
			{
				return false;
			}
			if (State.Screen == EGameXXKScreen::RouteEvent && !ResolvePendingRouteChoice(State))
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
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestTrue(TEXT("the player-facing StartNewGame path initializes the full-flow runtime"), Subsystem->StartNewGame());
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();

	TestEqual(TEXT("player-facing StartNewGame lands directly in Qingshan town"), State.Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("new game starts with ten thousand gold"), State.PlayerGold, 10000);
	TestEqual(TEXT("player-facing StartNewGame grants two permanent companions"), State.CardRun.CompanionRoster.PermanentCompanions.Num(), 2);
	FName StarterCompanionId = NAME_None;
	if (State.CardRun.CompanionRoster.PermanentCompanions.Num() == 2)
	{
		const FGameXXKPermanentCompanion& StarterCompanion = State.CardRun.CompanionRoster.PermanentCompanions[0];
		StarterCompanionId = StarterCompanion.InstanceId;
		TestFalse(TEXT("starter permanent companion has a stable instance id"), StarterCompanionId.IsNone());
		TestTrue(TEXT("starter permanent companion is active for the first route"), StarterCompanion.bIsActive);
		TestEqual(TEXT("starter permanent companion selection is synchronized"), State.CardRun.PartySelection.ActivePermanentCompanionInstanceId, StarterCompanionId);
	}
	if (StarterCompanionId.IsNone())
	{
		return false;
	}
	const FName WoodenSword = UGameXXKMVPRules::ItemWoodenSword();
	const FName StarterClothArmor = UGameXXKMVPRules::ItemStarterClothArmor();
	const FName ClothTalisman = UGameXXKMVPRules::ItemClothTalisman();
	TestEqual(TEXT("new game starts with a wooden sword for equipment replacement testing"), UGameXXKMVPRules::GetItemCount(State, WoodenSword), 1);
	TestEqual(TEXT("new game starts with cloth armor for equipment replacement testing"), UGameXXKMVPRules::GetItemCount(State, StarterClothArmor), 1);
	TestEqual(TEXT("new game starts with a cloth talisman for equipment replacement testing"), UGameXXKMVPRules::GetItemCount(State, ClothTalisman), 1);
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
	const FGameXXKCardBattleRuntime& BattleRuntime = State.CardRun.ActiveBattle;
	TestEqual(TEXT("the Qingshan task route projects exactly companion hero and task NPC"), State.ActiveBattleParty.Num(), 3);
	const FGameXXKCardCombatUnit* StarterCompanionUnit = BattleRuntime.Units.FindByPredicate([StarterCompanionId](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == StarterCompanionId && Unit.Side == EGameXXKCardTargetSide::Party;
	});
	const FGameXXKCardCombatUnit* HeroUnit = BattleRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player") && Unit.Side == EGameXXKCardTargetSide::Party && Unit.Role == EGameXXKCharacterRole::Hero;
	});
	const FGameXXKCardCombatUnit* TaskNpcUnit = BattleRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Npc.TusiChief") && Unit.Side == EGameXXKCardTargetSide::Party && Unit.Role == EGameXXKCharacterRole::QuestNpc;
	});
	TestTrue(TEXT("the task route carries the active permanent companion into battle"), StarterCompanionUnit != nullptr);
	TestTrue(TEXT("the task route carries the hero into battle"), HeroUnit != nullptr);
	TestTrue(TEXT("the task route carries Tusi Chief into battle"), TaskNpcUnit != nullptr);
	if (!StarterCompanionUnit || !HeroUnit || !TaskNpcUnit)
	{
		return false;
	}
	TestEqual(TEXT("the permanent companion uses presentation slot 我 1P"), FGameXXKBattlePresentation::GetSlotNumber(BattleRuntime, StarterCompanionUnit->UnitId), 1);
	TestEqual(TEXT("the hero uses presentation slot 我 2P"), FGameXXKBattlePresentation::GetSlotNumber(BattleRuntime, HeroUnit->UnitId), 2);
	TestEqual(TEXT("the task NPC uses presentation slot 我 3P"), FGameXXKBattlePresentation::GetSlotNumber(BattleRuntime, TaskNpcUnit->UnitId), 3);
	const TArray<FGameXXKBattlePresentationSlot> PresentationSlots = FGameXXKBattlePresentation::BuildSlots(BattleRuntime);
	TestTrue(TEXT("the shared presentation slot list has companion 1P"), PresentationSlots.ContainsByPredicate([StarterCompanionId](const FGameXXKBattlePresentationSlot& Slot)
	{
		return Slot.UnitId == StarterCompanionId && Slot.Side == EGameXXKCardTargetSide::Party && Slot.SlotNumber == 1;
	}));
	TestTrue(TEXT("the shared presentation slot list has hero 2P"), PresentationSlots.ContainsByPredicate([](const FGameXXKBattlePresentationSlot& Slot)
	{
		return Slot.UnitId == TEXT("Player") && Slot.Side == EGameXXKCardTargetSide::Party && Slot.SlotNumber == 2;
	}));
	TestTrue(TEXT("the shared presentation slot list has task NPC 3P"), PresentationSlots.ContainsByPredicate([](const FGameXXKBattlePresentationSlot& Slot)
	{
		return Slot.UnitId == TEXT("Npc.TusiChief") && Slot.Side == EGameXXKCardTargetSide::Party && Slot.SlotNumber == 3;
	}));
	TestTrue(TEXT("the task-route battle saves enemy intents for the presentation layer"), !State.CardRun.EnemyIntents.IsEmpty());
	if (!State.CardRun.EnemyIntents.IsEmpty())
	{
		const FGameXXKCardEnemyIntent& FirstIntent = State.CardRun.EnemyIntents[0];
		TestEqual(TEXT("the saved enemy intent source uses its displayed enemy P slot"),
			FirstIntent.SourceSlotNumber,
			FGameXXKBattlePresentation::GetSlotNumber(BattleRuntime, FirstIntent.SourceUnitId));
		TestEqual(TEXT("the saved enemy intent target uses its displayed party P slot"),
			FirstIntent.TargetSlotNumber,
			FGameXXKBattlePresentation::GetSlotNumber(BattleRuntime, FirstIntent.SuggestedTargetUnitId));
	}

	const int32 TravelMoneyBeforeBattle = State.CardRun.RouteTravelMoney;
	const int32 PermanentGoldBeforeBattle = State.PlayerGold;
	TestTrue(TEXT("normal battle fixture forces the card runtime into victory"), ForceActiveCardBattleVictory(State));
	TestTrue(TEXT("normal card battle victory opens the saved three-card route reward"), UGameXXKMVPRules::ResolveBattleVictory(State, false));
	TestEqual(TEXT("normal card battle stays on the battle screen while the reward is pending"), State.Screen, EGameXXKScreen::Battle);
	TestEqual(TEXT("normal card battle exposes exactly three route reward choices"), State.CardRun.PendingReward.CardIds.Num(), 3);
	FString NormalBattleRewardError;
	TestTrue(FString::Printf(TEXT("skipping the normal battle route reward unlocks victory completion: %s"), *NormalBattleRewardError),
		FGameXXKCardBattleAdapter::SkipPendingRouteReward(State, &NormalBattleRewardError));
	TestTrue(TEXT("normal battle completes only after its route reward is resolved"), UGameXXKMVPRules::ResolveBattleVictory(State, false));
	TestEqual(TEXT("resolved normal battle returns to the dungeon route map"), State.Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("battle reward adds route-local travel money"), State.CardRun.RouteTravelMoney > TravelMoneyBeforeBattle);
	TestEqual(TEXT("battle reward never adds permanent gold before terminal settlement"), State.PlayerGold, PermanentGoldBeforeBattle);
	TestTrue(TEXT("battle reward XP kept"), State.PlayerXP > 0);
	TestTrue(TEXT("battle reward item kept"), UGameXXKMVPRules::GetItemCount(State, UGameXXKMVPRules::ItemHealingPowder()) > 0);

	State.PlayerHP = 40;
	const int32 PotionBeforeUse = UGameXXKMVPRules::GetItemCount(State, UGameXXKMVPRules::ItemHealingPowder());
	TestTrue(TEXT("dungeon consumable can be used"), UGameXXKMVPRules::UseHealingItem(State));
	TestEqual(TEXT("used consumable deducted"), UGameXXKMVPRules::GetItemCount(State, UGameXXKMVPRules::ItemHealingPowder()), PotionBeforeUse - 1);
	TestTrue(TEXT("healing item restores HP"), State.PlayerHP > 40);

	const int32 GoldBeforeFailure = State.PlayerGold;
	const int32 TravelMoneyBeforeFailure = State.CardRun.RouteTravelMoney;
	const int32 XPBeforeFailure = State.PlayerXP;
	const int32 PotionAfterUse = UGameXXKMVPRules::GetItemCount(State, UGameXXKMVPRules::ItemHealingPowder());
	TestTrue(TEXT("dungeon failure returns to town"), UGameXXKMVPRules::FailDungeonToTown(State));
	TestEqual(TEXT("failure screen town"), State.Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("failure keeps accepted quest"), State.QuestState, EGameXXKQuestState::Accepted);
	TestTrue(TEXT("failure keeps follower"), State.bFollowerJoined);
	TestEqual(TEXT("failure converts earned travel money at twenty to one"), State.PlayerGold, GoldBeforeFailure + TravelMoneyBeforeFailure / 20);
	TestEqual(TEXT("failure clears route-local travel money after conversion"), State.CardRun.RouteTravelMoney, 0);
	TestEqual(TEXT("failure keeps earned XP"), State.PlayerXP, XPBeforeFailure);
	TestEqual(TEXT("failure keeps consumable deduction"), UGameXXKMVPRules::GetItemCount(State, UGameXXKMVPRules::ItemHealingPowder()), PotionAfterUse);

	TestTrue(TEXT("retry enters dungeon"), UGameXXKMVPRules::EnterDungeon(State));
	TestTrue(TEXT("retry start node"), UGameXXKMVPRules::AdvanceDungeonNode(State, EGameXXKNodeKind::Start));
	State.PlayerHP = 1;
	TestTrue(TEXT("generated route reaches a camp node"), AdvanceGeneratedRouteTowardKind(State, EGameXXKNodeKind::Camp));
	TestEqual(TEXT("camp restored full HP"), State.PlayerHP, State.PlayerMaxHP);
	TestTrue(TEXT("generated route reaches and clears the chapter one boss"), AdvanceGeneratedRouteTowardKind(State, EGameXXKNodeKind::Boss));
	TestTrue(TEXT("chapter one Boss keeps the route active"), State.bDungeonActive);
	TestEqual(TEXT("chapter one Boss advances to chapter two"), State.CardRun.RouteProgress.CurrentChapter, 2);
	TestTrue(TEXT("generated route reaches and clears the chapter two boss"), AdvanceGeneratedRouteTowardKind(State, EGameXXKNodeKind::Boss));
	TestTrue(TEXT("chapter two Boss keeps the route active"), State.bDungeonActive);
	TestEqual(TEXT("chapter two Boss advances to chapter three"), State.CardRun.RouteProgress.CurrentChapter, 3);
	TestTrue(TEXT("generated route reaches and clears the terminal chapter three boss"), AdvanceGeneratedRouteTowardKind(State, EGameXXKNodeKind::Boss));

	TestEqual(TEXT("Boss completes quest"), State.QuestState, EGameXXKQuestState::Completed);
	TestFalse(TEXT("follower leaves after clear"), State.bFollowerJoined);
	TestTrue(TEXT("Boss unlocks Tanjiang"), State.UnlockedRegions.Contains(UGameXXKMVPRules::RegionTanjiang()));
	TestFalse(TEXT("Tanjiang remains unavailable until it has a playable town target"), UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionTanjiang()));
	TestEqual(TEXT("unimplemented Tanjiang keeps the player on world map"), State.Screen, EGameXXKScreen::WorldMap);
	TestEqual(TEXT("unimplemented Tanjiang keeps the world-map selection clear"), State.CurrentRegion, NAME_None);

	const FGameXXKSaveState Save = UGameXXKMVPRules::MakeSaveState(State);
	FGameXXKRuntimeState Restored;
	FGameXXKSaveMigrationReport RestoreReport;
	if (!TestTrue(
		TEXT("save restores through the typed migration boundary"),
		FGameXXKSaveMigration::TryRestoreRuntimeState(Save, Restored, RestoreReport)))
	{
		return false;
	}
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
