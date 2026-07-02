#include "GameXXKMVPRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMVPFullFlowTest,
	"GameXXK.MVP.FullFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMVPFullFlowTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();

	TestEqual(TEXT("main menu opens first"), State.Screen, EGameXXKScreen::MainMenu);
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

	const int32 GoldBeforeTrade = State.PlayerGold;
	TestTrue(TEXT("merchant buy healing item"), UGameXXKMVPRules::BuyItem(State, UGameXXKMVPRules::ItemHealingPowder(), 1));
	TestEqual(TEXT("buy spends gold"), State.PlayerGold, GoldBeforeTrade - 10);
	TestTrue(TEXT("merchant sell healing item"), UGameXXKMVPRules::SellItem(State, UGameXXKMVPRules::ItemHealingPowder(), 1));
	TestEqual(TEXT("sell refunds sell value"), State.PlayerGold, GoldBeforeTrade - 5);
	TestTrue(TEXT("merchant buy weapon"), UGameXXKMVPRules::BuyItem(State, UGameXXKMVPRules::ItemIronSword(), 1));
	const int32 AttackBeforeEquip = State.PlayerAttack;
	TestTrue(TEXT("town can equip weapon"), UGameXXKMVPRules::EquipItem(State, UGameXXKMVPRules::ItemIronSword()));
	TestEqual(TEXT("weapon attack applies"), State.PlayerAttack, AttackBeforeEquip + 6);

	TestTrue(TEXT("return to world map before selecting next destination"), UGameXXKMVPRules::OpenWorldMap(State));
	TestFalse(TEXT("world map does not open small-map route directly"), UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionHuangshan()));
	TestTrue(TEXT("world map can re-enter current town"), UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("enter Huangshan dungeon from town quest gate"), UGameXXKMVPRules::EnterDungeon(State));
	TestEqual(TEXT("dungeon map visible"), State.Screen, EGameXXKScreen::DungeonMap);
	const TArray<EGameXXKNodeKind> Nodes = UGameXXKMVPRules::GetFixedDungeonNodes(0);
	TestEqual(TEXT("fixed node count"), Nodes.Num(), 5);
	TestEqual(TEXT("fixed node start"), Nodes[0], EGameXXKNodeKind::Start);
	TestEqual(TEXT("fixed node boss"), Nodes.Last(), EGameXXKNodeKind::Boss);

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
	TestTrue(TEXT("retry battle node"), UGameXXKMVPRules::AdvanceDungeonNode(State, EGameXXKNodeKind::Battle));
	TestTrue(TEXT("retry normal battle victory"), UGameXXKMVPRules::ResolveBattleVictory(State, false));
	TestTrue(TEXT("event reward works"), UGameXXKMVPRules::ResolveEventReward(State, true));
	State.PlayerHP = 1;
	TestTrue(TEXT("camp heal works"), UGameXXKMVPRules::ResolveCampReward(State, true));
	TestEqual(TEXT("camp restored full HP"), State.PlayerHP, State.PlayerMaxHP);
	TestTrue(TEXT("boss node opens battle"), UGameXXKMVPRules::AdvanceDungeonNode(State, EGameXXKNodeKind::Boss));
	TestTrue(TEXT("boss victory completes route"), UGameXXKMVPRules::ResolveBattleVictory(State, true));

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
	TestEqual(TEXT("save restore does not persist inventory details"), Restored.Inventory.Num(), 0);
	TestEqual(TEXT("save restore starts from main menu"), Restored.Screen, EGameXXKScreen::MainMenu);

	return true;
}

#endif
