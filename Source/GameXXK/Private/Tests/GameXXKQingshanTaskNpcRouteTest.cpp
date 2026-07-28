#include "GameXXKCardBattleAdapter.h"
#include "GameXXKMVPRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKQingshanTaskNpcRouteTest,
	"GameXXK.Integration.CardRoute.QingshanTaskNpc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKQingshanTaskNpcRouteTest::RunTest(const FString& Parameters)
{
	const FName TusiChiefId(TEXT("Npc.TusiChief"));
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("the accepted Qingshan main quest enters its first route"),
		UGameXXKMVPRules::OpenWorldMap(State)
		&& UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan())
		&& UGameXXKMVPRules::AcceptTownQuest(State)
		&& UGameXXKMVPRules::EnterDungeon(State));

	TestTrue(TEXT("the accepted Qingshan quest keeps its route follower active"), State.bFollowerJoined);
	TestEqual(TEXT("entering Qingshan automatically assigns the mapped temporary task NPC"),
		State.CardRun.ActiveTemporaryQuestNpcId, TusiChiefId);
	TestEqual(TEXT("the automatic task NPC uses the canonical quest-NPC selection"),
		State.CardRun.PartySelection.QuestNpc.NpcId, TusiChiefId);
	TestEqual(TEXT("the automatic task NPC contributes its fixed three-card selection"),
		State.CardRun.PartySelection.QuestNpc.SelectedCardIds.Num(), 3);
	TestEqual(TEXT("the automatic task NPC never writes to the permanent companion roster"),
		State.CardRun.CompanionRoster.PermanentCompanions.Num(), 0);

	State.Screen = EGameXXKScreen::RouteEvent;
	State.RouteMapNodes = {
		FGameXXKRouteMapNode(701, 2, 0, EGameXXKNodeKind::Event, FVector2D(0.5f, 0.4f), {})};
	State.PendingRouteNodeId = 701;
	State.CardRun.PendingEvent.SourceNodeId = 701;
	State.CardRun.PendingEvent.ChoiceSeed = 987654;
	State.CardRun.PendingEvent.EventNpcId = TEXT("Npc.YueBai");
	TestFalse(TEXT("a route event cannot replace the main quest temporary support slot"),
		UGameXXKMVPRules::AcceptRouteEventNpcSupport(State));
	TestEqual(TEXT("a rejected event support offer preserves Tusi Chief"),
		State.CardRun.ActiveTemporaryQuestNpcId, TusiChiefId);

	TestTrue(TEXT("failing the route clears all route-local task NPC state"), UGameXXKMVPRules::FailDungeonToTown(State));
	TestTrue(TEXT("route lifecycle cleanup removes the temporary task NPC"), State.CardRun.ActiveTemporaryQuestNpcId.IsNone());
	TestTrue(TEXT("route lifecycle cleanup clears the task-NPC card selection"), State.CardRun.PartySelection.QuestNpc.NpcId.IsNone());
	TestTrue(TEXT("the still-accepted Qingshan quest can enter another route"), UGameXXKMVPRules::EnterDungeon(State));
	TestEqual(TEXT("re-entering Qingshan restores the mapped temporary task NPC"),
		State.CardRun.ActiveTemporaryQuestNpcId, TusiChiefId);
	TestEqual(TEXT("re-entering Qingshan restores all three task-NPC cards"),
		State.CardRun.PartySelection.QuestNpc.SelectedCardIds.Num(), 3);
	return true;
}

#endif
