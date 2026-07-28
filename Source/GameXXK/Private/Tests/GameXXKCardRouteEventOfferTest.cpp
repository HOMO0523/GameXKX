#include "GameXXKMVPRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardRouteEventOfferTest,
	"GameXXK.Integration.CardRoute.EventOffer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardRouteEventOfferTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("the event offer starts from a route-capable quest state"),
		UGameXXKMVPRules::OpenWorldMap(State)
		&& UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan())
		&& UGameXXKMVPRules::AcceptTownQuest(State)
		&& UGameXXKMVPRules::EnterDungeon(State));

	State.RouteSeed = 24681357;
	State.bHasGeneratedRouteMap = true;
	State.Screen = EGameXXKScreen::DungeonMap;
	State.RouteMapNodes = { FGameXXKRouteMapNode(17, 1, 0, EGameXXKNodeKind::Event, FVector2D(0.5f, 0.2f), {}) };
	State.RouteMapEdges.Reset();
	State.VisitedRouteNodeIds.Reset();
	State.ReachableRouteNodeIds = { 17 };
	State.PendingRouteNodeId = INDEX_NONE;

	TestTrue(TEXT("selecting an event creates its deterministic saved event offer"), UGameXXKMVPRules::SelectRouteNodeById(State, 17));
	TestEqual(TEXT("the selected event opens the route event screen"), State.Screen, EGameXXKScreen::RouteEvent);
	TestEqual(TEXT("the pending event records its stable route node source"), State.CardRun.PendingEvent.SourceNodeId, 17);
	TestFalse(TEXT("the event offer records a concrete NPC/event identity"), State.CardRun.PendingEvent.EventNpcId.IsNone());
	return true;
}

#endif
