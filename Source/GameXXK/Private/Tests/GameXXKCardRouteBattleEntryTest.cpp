#include "GameXXKMVPRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardRouteBattleEntryTest,
	"GameXXK.Integration.CardRoute.BattleEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardRouteBattleEntryTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	// Pin the route seed: a fresh game otherwise rolls it from the process RNG, which would
	// make the chapter-one formation vary across automation runs.  Seed 1 yields the canonical
	// Weasel formation asserted below.
	State.RouteSeed = 1;
	TestTrue(TEXT("the main-menu route can open the world map"), UGameXXKMVPRules::OpenWorldMap(State));
	TestTrue(TEXT("the world map can enter Qingshan"), UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("the Qingshan quest can be accepted through the current follower contract"), UGameXXKMVPRules::AcceptTownQuest(State));
	TestTrue(TEXT("accepting the Qingshan quest activates the narrative follower"), State.bFollowerJoined);
	TestTrue(TEXT("accepting the narrative quest does not auto-select a combat NPC"), State.CardRun.PartySelection.QuestNpc.NpcId.IsNone());
	TestTrue(TEXT("the accepted town quest enters the route map"), UGameXXKMVPRules::EnterDungeon(State));
	// This test isolates the shared BeginBattle entry point rather than making an assumption about
	// the first randomly generated map layer.  The legacy linear fallback reaches the same battle
	// constructor and gives the test a stable, single-node route fixture.
	State.bHasGeneratedRouteMap = false;
	State.RouteMapNodes.Reset();
	State.RouteMapEdges.Reset();
	State.ReachableRouteNodeIds.Reset();
	State.DungeonNodeIndex = 1;

	TestTrue(TEXT("the first battle route node opens"), UGameXXKMVPRules::AdvanceDungeonNode(State, EGameXXKNodeKind::Battle));
	TestEqual(TEXT("battle entry remains on the battle screen"), State.Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("battle entry creates the serialized card authority rather than only legacy action buttons"), State.CardRun.bHasActiveCardBattle);
	TestEqual(TEXT("the shared card battle opens with the fixed five-card hand"), State.CardRun.ActiveBattle.Deck.Hand.Num(), 5);
	TestTrue(TEXT("a regular route battle contains the canonical chapter-one Weasel monster"), State.ActiveBattleEnemies.ContainsByPredicate([](const FGameXXKBattleRuntimeUnit& Unit)
	{
		return Unit.EnemyDefinitionId == TEXT("Enemy.Ch1.Weasel");
	}));
	TestFalse(TEXT("legacy narrative follower state does not silently become a combat party member"), State.ActiveBattleParty.ContainsByPredicate([](const FGameXXKBattleRuntimeUnit& Unit)
	{
		return Unit.Id == TEXT("Follower");
	}));
	return true;
}

#endif
