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
	TestTrue(TEXT("the Qingshan main quest can be accepted"),
		UGameXXKMVPRules::OpenWorldMap(State)
		&& UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan())
		&& UGameXXKMVPRules::AcceptTownQuest(State));
	// New semantics: accepting the quest keeps the guide NPC in town. Simulate the dialog
	// 入队 recruit (controller RecruitPendingTownNpc) so the route assertions below still
	// exercise the recruited narrative follower and its recorded location.
	TestFalse(TEXT("accepting the Qingshan quest keeps the guide NPC in town until 入队"), State.bFollowerJoined);
	State.bFollowerJoined = true;
	const FVector NarrativeFollowerLocation(180.0f, -64.0f, 72.0f);
	State.bHasQuestNpcLocation = true;
	State.QuestNpcLocation = NarrativeFollowerLocation;
	TestTrue(TEXT("the player explicitly selects Tusi Chief before entering the first route"),
		FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(State, TusiChiefId, {}));
	TestTrue(TEXT("the accepted Qingshan main quest enters its first route"), UGameXXKMVPRules::EnterDungeon(State));

	TestTrue(TEXT("the accepted Qingshan quest keeps its narrative follower active"), State.bFollowerJoined);
	TestTrue(TEXT("entering the route keeps the narrative follower location flag"), State.bHasQuestNpcLocation);
	TestEqual(TEXT("entering the route keeps the narrative follower location"), State.QuestNpcLocation, NarrativeFollowerLocation);
	TestEqual(TEXT("entering Qingshan preserves the explicitly selected temporary task NPC"),
		State.CardRun.ActiveTemporaryQuestNpcId, TusiChiefId);
	TestEqual(TEXT("the selected task NPC uses the canonical quest-NPC selection"),
		State.CardRun.PartySelection.QuestNpc.NpcId, TusiChiefId);
	TestEqual(TEXT("the selected task NPC contributes its fixed three-card selection"),
		State.CardRun.PartySelection.QuestNpc.SelectedCardIds.Num(), 3);
	TestEqual(TEXT("the selected task NPC never writes to the permanent companion roster"),
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
	TestTrue(TEXT("route failure preserves the still-accepted narrative follower"), State.bFollowerJoined);
	TestTrue(TEXT("route failure preserves the narrative follower location flag"), State.bHasQuestNpcLocation);
	TestEqual(TEXT("route failure preserves the narrative follower location"), State.QuestNpcLocation, NarrativeFollowerLocation);
	TestTrue(TEXT("the player explicitly selects Tusi Chief again before a later route"),
		FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(State, TusiChiefId, {}));
	TestTrue(TEXT("the still-accepted Qingshan quest can enter another route"), UGameXXKMVPRules::EnterDungeon(State));
	TestEqual(TEXT("re-entering Qingshan preserves the newly selected temporary task NPC"),
		State.CardRun.ActiveTemporaryQuestNpcId, TusiChiefId);
	TestEqual(TEXT("re-entering Qingshan rebuilds all three selected task-NPC cards"),
		State.CardRun.PartySelection.QuestNpc.SelectedCardIds.Num(), 3);
	return true;
}

#endif
