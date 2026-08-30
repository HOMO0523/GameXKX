#include "GameXXKCompanionCatalog.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "MVP/GameXXKMVPSubsystem.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKQuestNpcDefaultLoadoutTest,
	"GameXXK.Data.Companion.QuestNpcSeededLoadouts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKQuestNpcDefaultLoadoutTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("owned NPC loadout fixture starts"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState State = Subsystem->GetRuntimeStateCopy();
	for (const FGameXXKQuestNpcDefinition& Definition :
		FGameXXKCompanionCatalog::GetQuestNpcDefinitions())
	{
		TestEqual(
			FString::Printf(TEXT("the named NPC retains four fixed candidates (%s)"),
				*Definition.NpcId.ToString()),
			Definition.FixedCardIds.Num(),
			4);
		const FGameXXKQuestNpcOwnedCardLoadout* OwnedLoadout =
			State.CardRun.PartySelection.QuestNpcCardLoadouts.Find(Definition.NpcId);
		if (!TestNotNull(TEXT("every owned NPC has a persisted loadout"), OwnedLoadout))
		{
			return false;
		}
		TestEqual(TEXT("each persisted NPC loadout selects exactly three cards"),
			OwnedLoadout->SelectedCardIds.Num(),
			3);
		const TArray<FName> ExpectedSelection = OwnedLoadout->SelectedCardIds;
		FString Error;
		if (!TestTrue(
			FString::Printf(TEXT("the formation selects %s with its owned loadout: %s"),
				*Definition.NpcId.ToString(),
				*Error),
			GameXXKPermanentPartyTestFixtures::SelectNpc(
				State,
				Definition.NpcId,
				&Error)))
		{
			return false;
		}
		TestEqual(
			FString::Printf(TEXT("the formation projects the persisted NPC loadout in order (%s)"),
				*Definition.NpcId.ToString()),
			State.CardRun.PartySelection.QuestNpc.SelectedCardIds,
			ExpectedSelection);
		TestEqual(
			FString::Printf(TEXT("the ordered formation stores the NPC identity (%s)"),
				*Definition.NpcId.ToString()),
			GameXXKPermanentPartyTestFixtures::ResolveNpc(State),
			Definition.NpcId);
		TestTrue(TEXT("current owned-NPC fixture keeps temporary provenance empty"),
			State.CardRun.ActiveTemporaryQuestNpcId.IsNone());
	}
	return true;
}

#endif
