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

	return true;
}

#endif
