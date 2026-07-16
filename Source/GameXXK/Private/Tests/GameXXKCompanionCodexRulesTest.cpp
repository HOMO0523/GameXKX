#include "GameXXKMVPRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	static const FGameXXKCodexEntryView* FindCodexEntryView(const TArray<FGameXXKCodexEntryView>& EntryViews, FName EntryId)
	{
		return EntryViews.FindByPredicate([EntryId](const FGameXXKCodexEntryView& EntryView)
		{
			return EntryView.Id == EntryId;
		});
	}

	static FGameXXKRuntimeState BuildReachableCombatRouteState(EGameXXKNodeKind NodeKind)
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::DungeonMap;
		State.CurrentMapId = TEXT("HuangshanRoute");
		State.QuestState = EGameXXKQuestState::Accepted;
		State.bDungeonActive = true;
		State.bHasGeneratedRouteMap = true;
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{0, 0, 0, EGameXXKNodeKind::Start, FVector2D(0.5f, 0.0f), TArray<int32>{1}});
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{1, 1, 0, NodeKind, FVector2D(0.5f, 1.0f), TArray<int32>{}});
		State.RouteMapEdges.Add(FGameXXKRouteMapEdge{0, 1});
		State.VisitedRouteNodeIds.Add(0);
		State.ReachableRouteNodeIds.Add(1);
		return State;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionCodexRulesTest,
	"GameXXK.MVP.Codex.RulesDiscovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionCodexRulesTest::RunTest(const FString& Parameters)
{
	const FName GuideId(TEXT("Codex.Guide"));
	const FName BanditId(TEXT("Codex.Bandit"));
	const FName WolfId(TEXT("Codex.Wolf"));
	const FName EliteBanditId(TEXT("Codex.EliteBandit"));
	const FName BossId(TEXT("Codex.Boss"));
	const FName UnknownId(TEXT("Codex.Unknown"));

	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	TestEqual(TEXT("new game has five static codex entries"), UGameXXKMVPRules::GetCodexEntryCount(EGameXXKCodexCategory::All), 5);
	TestEqual(TEXT("new game has no spirit codex entries"), UGameXXKMVPRules::GetCodexEntryCount(EGameXXKCodexCategory::Spirit), 0);
	TestEqual(TEXT("new game has no discovered codex entries"), UGameXXKMVPRules::GetDiscoveredCodexEntryCount(State, EGameXXKCodexCategory::All), 0);
	TestFalse(TEXT("new game has no unread codex entries"), UGameXXKMVPRules::HasUnreadCodexEntries(State));

	const TArray<FGameXXKCodexEntryView> AllEntryViews = UGameXXKMVPRules::BuildCodexEntryViews(State, EGameXXKCodexCategory::All);
	const FGameXXKCodexEntryView* GuideView = FindCodexEntryView(AllEntryViews, GuideId);
	TestNotNull(TEXT("all codex view contains the guide"), GuideView);
	if (!GuideView)
	{
		return false;
	}
	TestEqual(TEXT("undiscovered guide uses hidden display name"), GuideView->DisplayName.ToString(), FString(TEXT("????")));
	TestFalse(TEXT("undiscovered guide is not marked discovered"), GuideView->bIsDiscovered);
	TestFalse(TEXT("undiscovered guide is not marked read"), GuideView->bIsRead);

	TestTrue(TEXT("new game opens the world map"), UGameXXKMVPRules::OpenWorldMap(State));
	TestTrue(TEXT("world map enters Qingshan"), UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("town quest is accepted"), UGameXXKMVPRules::AcceptTownQuest(State));
	TestTrue(TEXT("accepting town quest discovers the guide"), State.DiscoveredCodexEntryIds.Contains(GuideId));
	TestTrue(TEXT("newly discovered guide is unread"), UGameXXKMVPRules::HasUnreadCodexEntries(State));
	TestTrue(TEXT("first guide read mark succeeds"), UGameXXKMVPRules::MarkCodexEntryRead(State, GuideId));
	TestFalse(TEXT("reading the only discovered guide clears unread state"), UGameXXKMVPRules::HasUnreadCodexEntries(State));
	TestFalse(TEXT("repeated guide discovery reports no new discovery"), UGameXXKMVPRules::DiscoverCodexEntry(State, GuideId));
	TestTrue(TEXT("repeated guide discovery keeps the entry read"), State.ReadCodexEntryIds.Contains(GuideId));
	TestFalse(TEXT("unknown codex discovery is rejected"), UGameXXKMVPRules::DiscoverCodexEntry(State, UnknownId));
	TestFalse(TEXT("unknown codex read mark is rejected"), UGameXXKMVPRules::MarkCodexEntryRead(State, UnknownId));

	TestTrue(TEXT("accepted quest enters the dungeon"), UGameXXKMVPRules::EnterDungeon(State));
	TestTrue(TEXT("route start node advances"), UGameXXKMVPRules::AdvanceDungeonNode(State, EGameXXKNodeKind::Start));
	TestTrue(TEXT("route battle node begins"), UGameXXKMVPRules::AdvanceDungeonNode(State, EGameXXKNodeKind::Battle));
	TestTrue(TEXT("normal battle discovers the bandit"), State.DiscoveredCodexEntryIds.Contains(BanditId));
	TestTrue(TEXT("normal battle discovers the wolf"), State.DiscoveredCodexEntryIds.Contains(WolfId));
	TestTrue(TEXT("battle discoveries are unread"), UGameXXKMVPRules::HasUnreadCodexEntries(State));

	FGameXXKRuntimeState EliteState = BuildReachableCombatRouteState(EGameXXKNodeKind::Elite);
	TestTrue(TEXT("elite route node selection begins battle"), UGameXXKMVPRules::SelectRouteNodeById(EliteState, 1));
	TestEqual(TEXT("elite route node opens battle screen"), EliteState.Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("elite battle discovers the elite bandit"), EliteState.DiscoveredCodexEntryIds.Contains(EliteBanditId));
	TestTrue(TEXT("elite battle discovers the wolf"), EliteState.DiscoveredCodexEntryIds.Contains(WolfId));

	FGameXXKRuntimeState BossState = BuildReachableCombatRouteState(EGameXXKNodeKind::Boss);
	TestTrue(TEXT("boss route node selection begins battle"), UGameXXKMVPRules::SelectRouteNodeById(BossState, 1));
	TestEqual(TEXT("boss route node opens battle screen"), BossState.Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("boss battle discovers the boss"), BossState.DiscoveredCodexEntryIds.Contains(BossId));

	return true;
}

#endif
