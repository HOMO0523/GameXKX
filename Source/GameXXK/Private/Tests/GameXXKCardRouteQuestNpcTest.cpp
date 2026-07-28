#include "GameXXKCardBattleAdapter.h"
#include "GameXXKMVPRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardRouteQuestNpcTest,
	"GameXXK.Integration.CardRoute.QuestNpc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardRouteQuestNpcTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("a new run initializes the persistent card party state"), FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State));
	State.CardRun.bLoadoutLockedForRoute = true;

	TestTrue(TEXT("a task NPC met during a locked route can still join its temporary NPC slot before the next battle"),
		FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(State, TEXT("Npc.TusiChief"), {}));
	TestEqual(TEXT("the route remembers the exact temporary task NPC identity"), State.CardRun.ActiveTemporaryQuestNpcId, FName(TEXT("Npc.TusiChief")));
	TestEqual(TEXT("the task NPC contributes exactly its player-selected three-card route configuration"), State.CardRun.PartySelection.QuestNpc.SelectedCardIds.Num(), 3);
	TestEqual(TEXT("a task NPC does not silently add a permanent companion"), State.CardRun.CompanionRoster.PermanentCompanions.Num(), 0);
	return true;
}

#endif
