#include "GameXXKCompanionCatalog.h"
#include "GameXXKPermanentPartyTestFixtures.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKQuestNpcCardSelectionTest,
	"GameXXK.Data.QuestNpcCardSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKQuestNpcCardSelectionTest::RunTest(const FString& Parameters)
{
	const FGameXXKRuntimeState BaseState =
		GameXXKPermanentPartyTestFixtures::MakeStartedState();
	for (const FGameXXKQuestNpcDefinition& Definition :
		FGameXXKCompanionCatalog::GetQuestNpcDefinitions())
	{
		const FGameXXKQuestNpcOwnedCardLoadout* OwnedLoadout =
			BaseState.CardRun.PartySelection.QuestNpcCardLoadouts.Find(Definition.NpcId);
		if (!TestNotNull(TEXT("each fixed NPC has a persisted owned loadout"), OwnedLoadout))
		{
			return false;
		}
		const TArray<FName> ExpectedSelection = OwnedLoadout->SelectedCardIds;
		TestEqual(TEXT("each fixed NPC persists exactly three selected cards"),
			ExpectedSelection.Num(),
			3);
		TestEqual(TEXT("each fixed NPC selection contains no duplicates"),
			TSet<FName>(ExpectedSelection).Num(),
			3);
		for (const FName CardId : ExpectedSelection)
		{
			TestTrue(TEXT("each selected card belongs to the NPC's four-card pool"),
				Definition.FixedCardIds.Contains(CardId));
		}

		for (const int32 RouteSeed : {1, 256, 0x24681357})
		{
			FGameXXKRuntimeState Candidate = BaseState;
			Candidate.RouteSeed = RouteSeed;
			Candidate.CardRun.RouteRandomSeed = RouteSeed;
			FString Error;
			TestTrue(TEXT("fixed NPC selects through ordered formation"),
				GameXXKPermanentPartyTestFixtures::SelectNpc(
					Candidate,
					Definition.NpcId,
					&Error));
			TestEqual(TEXT("route seed never rerolls the persisted NPC cards"),
				Candidate.CardRun.PartySelection.QuestNpc.SelectedCardIds,
				ExpectedSelection);
			TestEqual(TEXT("ordered formation keeps the selected NPC identity"),
				GameXXKPermanentPartyTestFixtures::ResolveNpc(Candidate),
				Definition.NpcId);
		}
	}

	FGameXXKRuntimeState RouteState = BaseState;
	FString RouteError;
	TestTrue(TEXT("route persistence fixture selects Tusi Chief"),
		GameXXKPermanentPartyTestFixtures::SelectNpc(
			RouteState,
			TEXT("Npc.TusiChief"),
			&RouteError));
	const TArray<FName> PersistedSelection =
		RouteState.CardRun.PartySelection.QuestNpc.SelectedCardIds;
	TestTrue(TEXT("fixed NPC route fixture reaches accepted Qingshan town"),
		UGameXXKMVPRules::OpenWorldMap(RouteState)
			&& UGameXXKMVPRules::EnterWorldRegion(
				RouteState,
				UGameXXKMVPRules::RegionQingshan())
			&& UGameXXKMVPRules::AcceptTownQuest(RouteState));
	RouteState.RouteSeed = 0x7135;
	TestTrue(TEXT("fixed NPC enters the route"),
		UGameXXKMVPRules::EnterDungeon(RouteState));
	TestEqual(TEXT("route entry preserves the persisted NPC cards"),
		RouteState.CardRun.PartySelection.QuestNpc.SelectedCardIds,
		PersistedSelection);
	FGameXXKRuntimeState ReloadedState = RouteState;
	TestTrue(TEXT("card-run validation accepts the persisted fixed NPC selection"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(
			ReloadedState,
			&RouteError));
	TestEqual(TEXT("reload never rerolls the persisted NPC cards"),
		ReloadedState.CardRun.PartySelection.QuestNpc.SelectedCardIds,
		PersistedSelection);
	return true;
}

#endif
