#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKMVPRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKQuestNpcDefaultLoadoutTest,
	"GameXXK.Data.Companion.QuestNpcSeededLoadouts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKQuestNpcDefaultLoadoutTest::RunTest(const FString& Parameters)
{
	constexpr int32 RouteSeed = 0x24681357;
	for (const FGameXXKQuestNpcDefinition& Definition : FGameXXKCompanionCatalog::GetQuestNpcDefinitions())
	{
		TestEqual(FString::Printf(TEXT("the named task NPC retains four fixed candidates (%s)"), *Definition.NpcId.ToString()), Definition.FixedCardIds.Num(), 4);
		TArray<FName> ExpectedSelection;
		FString Error;
		if (!TestTrue(
			FString::Printf(TEXT("the seeded NPC selection builds for %s"), *Definition.NpcId.ToString()),
			FGameXXKCompanionRules::BuildQuestNpcRouteCardSelection(
				Definition.NpcId,
				RouteSeed,
				ExpectedSelection,
				&Error)))
		{
			return false;
		}

		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.RouteSeed = RouteSeed;
		if (!TestTrue(FString::Printf(TEXT("the route attaches %s with its seed-selected loadout: %s"), *Definition.NpcId.ToString(), *Error),
			FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(State, Definition.NpcId, {}, &Error)))
		{
			return false;
		}
		TestEqual(FString::Printf(TEXT("the route stores the deterministic three-card NPC loadout in order (%s)"), *Definition.NpcId.ToString()),
			State.CardRun.PartySelection.QuestNpc.SelectedCardIds,
			ExpectedSelection);
		TestEqual(FString::Printf(TEXT("the route stores the task NPC identity with its seeded loadout (%s)"), *Definition.NpcId.ToString()),
			State.CardRun.ActiveTemporaryQuestNpcId,
			Definition.NpcId);
	}

	return true;
}

#endif
