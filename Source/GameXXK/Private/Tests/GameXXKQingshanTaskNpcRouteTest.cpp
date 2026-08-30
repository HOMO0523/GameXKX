#include "GameXXKCardBattleAdapter.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPartyFormationRules.h"
#include "MVP/GameXXKMVPSubsystem.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKQingshanTaskNpcRouteTest,
	"GameXXK.Integration.CardRoute.QingshanTaskNpc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKQingshanTaskNpcRouteTest::RunTest(const FString& Parameters)
{
	const FName TusiChiefId(TEXT("Npc.TusiChief"));
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("Qingshan permanent-NPC fixture starts"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState State = Subsystem->GetRuntimeStateCopy();
	TestTrue(TEXT("the Qingshan main quest can be accepted"),
		UGameXXKMVPRules::AcceptTownQuest(State));
	// New semantics: accepting the quest keeps the guide NPC in town. Simulate the dialog
	// 入队 recruit (controller RecruitPendingTownNpc) so the route assertions below still
	// exercise the recruited narrative follower and its recorded location.
	TestFalse(TEXT("accepting the Qingshan quest keeps the guide NPC in town until 入队"), State.bFollowerJoined);
	State.bFollowerJoined = true;
	const FVector NarrativeFollowerLocation(180.0f, -64.0f, 72.0f);
	State.bHasQuestNpcLocation = true;
	State.QuestNpcLocation = NarrativeFollowerLocation;
	TestTrue(TEXT("the player explicitly selects Tusi Chief before entering the first route"),
		FGameXXKPartyFormationRules::SetQuestNpc(State, TusiChiefId));
	TestTrue(TEXT("the accepted Qingshan main quest enters its first route"), UGameXXKMVPRules::EnterDungeon(State));

	TestTrue(TEXT("the accepted Qingshan quest keeps its narrative follower active"), State.bFollowerJoined);
	TestTrue(TEXT("entering the route keeps the narrative follower location flag"), State.bHasQuestNpcLocation);
	TestEqual(TEXT("entering the route keeps the narrative follower location"), State.QuestNpcLocation, NarrativeFollowerLocation);
	FName EnteredNpcId;
	TestTrue(TEXT("entering Qingshan resolves the permanent NPC"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(State, EnteredNpcId));
	TestEqual(TEXT("entering Qingshan preserves Tusi Chief"), EnteredNpcId, TusiChiefId);
	TestEqual(TEXT("the selected NPC uses the canonical owned selection"),
		State.CardRun.PartySelection.QuestNpc.NpcId, TusiChiefId);
	TestEqual(TEXT("the selected NPC contributes its fixed three-card selection"),
		State.CardRun.PartySelection.QuestNpc.SelectedCardIds.Num(), 3);
	TestEqual(TEXT("the fixed six companion roster remains intact"),
		State.CardRun.CompanionRoster.PermanentCompanions.Num(), 6);
	TestTrue(TEXT("temporary route provenance remains retired"),
		State.CardRun.ActiveTemporaryQuestNpcId.IsNone());

	State.Screen = EGameXXKScreen::RouteEvent;
	State.RouteMapNodes = {
		FGameXXKRouteMapNode(701, 2, 0, EGameXXKNodeKind::Event, FVector2D(0.5f, 0.4f), {})};
	State.PendingRouteNodeId = 701;
	State.CardRun.PendingEvent.SourceNodeId = 701;
	State.CardRun.PendingEvent.ChoiceSeed = 987654;
	State.CardRun.PendingEvent.EncounterId = TEXT("Encounter.Event.MountainSpring");
	State.CardRun.PendingEvent.EventNpcId = TEXT("Event.Attribute.MountainSpring");
	TestFalse(TEXT("the retired support facade cannot replace the permanent NPC slot"),
		UGameXXKMVPRules::AcceptRouteEventNpcSupport(State));
	FName NpcAfterRejectedEvent;
	TestTrue(TEXT("NPC resolves after a rejected event offer"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(State, NpcAfterRejectedEvent));
	TestEqual(TEXT("the rejected support facade preserves Tusi Chief"),
		NpcAfterRejectedEvent, TusiChiefId);

	TestTrue(TEXT("failing the route clears route-local state"), UGameXXKMVPRules::FailDungeonToTown(State));
	FName NpcAfterFailure;
	TestTrue(TEXT("route lifecycle cleanup preserves the permanent NPC"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(State, NpcAfterFailure));
	TestEqual(TEXT("route lifecycle cleanup keeps Tusi Chief"), NpcAfterFailure, TusiChiefId);
	TestEqual(TEXT("route lifecycle cleanup preserves the NPC card selection"),
		State.CardRun.PartySelection.QuestNpc.NpcId,
		TusiChiefId);
	TestTrue(TEXT("route failure preserves the still-accepted narrative follower"), State.bFollowerJoined);
	TestTrue(TEXT("route failure preserves the narrative follower location flag"), State.bHasQuestNpcLocation);
	TestEqual(TEXT("route failure preserves the narrative follower location"), State.QuestNpcLocation, NarrativeFollowerLocation);
	TestTrue(TEXT("the still-accepted Qingshan quest can enter another route"), UGameXXKMVPRules::EnterDungeon(State));
	FName ReenteredNpcId;
	TestTrue(TEXT("re-entered route resolves its permanent NPC"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(State, ReenteredNpcId));
	TestEqual(TEXT("re-entering Qingshan preserves Tusi Chief"), ReenteredNpcId, TusiChiefId);
	TestEqual(TEXT("re-entering Qingshan keeps all three selected NPC cards"),
		State.CardRun.PartySelection.QuestNpc.SelectedCardIds.Num(), 3);
	return true;
}

#endif
