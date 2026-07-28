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
	TestTrue(TEXT("the main-menu route can open the world map"), UGameXXKMVPRules::OpenWorldMap(State));
	TestTrue(TEXT("the world map can enter Qingshan"), UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("the Qingshan quest can be accepted without changing its legacy follower narrative flag"), UGameXXKMVPRules::AcceptTownQuest(State));
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
	TestTrue(TEXT("a regular route battle contains the canonical MoneyRat monster"), State.ActiveBattleEnemies.ContainsByPredicate([](const FGameXXKBattleRuntimeUnit& Unit)
	{
		return Unit.Id == TEXT("MoneyRat");
	}));
	TestFalse(TEXT("legacy narrative follower state does not silently become a combat party member"), State.ActiveBattleParty.ContainsByPredicate([](const FGameXXKBattleRuntimeUnit& Unit)
	{
		return Unit.Id == TEXT("Follower");
	}));
	return true;
}

#endif
