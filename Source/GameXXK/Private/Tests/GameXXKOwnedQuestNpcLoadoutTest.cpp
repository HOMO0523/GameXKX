#include "GameXXKCompanionCatalog.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKOwnedQuestNpcLoadoutTest,
	"GameXXK.MVP.OwnedQuestNpcLoadouts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKOwnedQuestNpcLoadoutTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("owned-NPC loadout fixture starts a new game"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}

	const TArray<FGameXXKQuestNpcDefinition>& Definitions = FGameXXKCompanionCatalog::GetQuestNpcDefinitions();
	TestEqual(TEXT("catalog exposes the six owned named NPCs"), Definitions.Num(), 6);
	TestEqual(TEXT("new game persists one editable three-card loadout per NPC"),
		Subsystem->GetRuntimeState().CardRun.PartySelection.QuestNpcCardLoadouts.Num(), 6);
	for (const FGameXXKQuestNpcDefinition& Definition : Definitions)
	{
		const FGameXXKQuestNpcOwnedCardLoadout* Loadout =
			Subsystem->GetRuntimeState().CardRun.PartySelection.QuestNpcCardLoadouts.Find(Definition.NpcId);
		TestNotNull(TEXT("every owned NPC has a persisted loadout"), Loadout);
		if (Loadout)
		{
			TestEqual(TEXT("owned NPC keeps exactly three selected cards"), Loadout->SelectedCardIds.Num(), 3);
		}
	}

	const FGameXXKQuestNpcDefinition* TusiChief =
		FGameXXKCompanionCatalog::FindQuestNpcDefinition(TEXT("Npc.TusiChief"));
	const FGameXXKQuestNpcDefinition* YueBai =
		FGameXXKCompanionCatalog::FindQuestNpcDefinition(TEXT("Npc.YueBai"));
	if (!TestNotNull(TEXT("Tusi Chief definition exists"), TusiChief)
		|| !TestNotNull(TEXT("Yue Bai definition exists"), YueBai)
		|| TusiChief->FixedCardIds.Num() != 4)
	{
		return false;
	}
	const TArray<FName> EditedTusiCards = {
		TusiChief->FixedCardIds[1],
		TusiChief->FixedCardIds[2],
		TusiChief->FixedCardIds[3]};
	TestTrue(TEXT("player can edit an owned NPC to another valid three-card loadout"),
		Subsystem->SetTemporaryQuestNpcCardLoadout(TusiChief->NpcId, EditedTusiCards));
	TestEqual(TEXT("editing the active NPC updates the active three-card selection"),
		Subsystem->GetRuntimeState().CardRun.PartySelection.QuestNpc.SelectedCardIds,
		EditedTusiCards);
	TestTrue(TEXT("player can switch the NPC party slot"), Subsystem->SelectTownQuestNpcForParty(YueBai->NpcId));
	TestTrue(TEXT("player can switch back to Tusi Chief"), Subsystem->SelectTownQuestNpcForParty(TusiChief->NpcId));
	TestEqual(TEXT("switching back restores Tusi Chief's edited loadout"),
		Subsystem->GetRuntimeState().CardRun.PartySelection.QuestNpc.SelectedCardIds,
		EditedTusiCards);

	const FGameXXKSaveState Save = UGameXXKMVPRules::MakeSaveState(Subsystem->GetRuntimeState());
	FGameXXKRuntimeState Restored;
	FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("owned NPC loadouts survive the current save boundary"),
		FGameXXKSaveMigration::TryRestoreRuntimeState(Save, Restored, Report));
	const FGameXXKQuestNpcOwnedCardLoadout* RestoredTusi =
		Restored.CardRun.PartySelection.QuestNpcCardLoadouts.Find(TusiChief->NpcId);
	TestNotNull(TEXT("restored save keeps Tusi Chief's loadout"), RestoredTusi);
	if (RestoredTusi)
	{
		TestEqual(TEXT("restored Tusi Chief loadout keeps the edited cards"), RestoredTusi->SelectedCardIds, EditedTusiCards);
	}

	FGameXXKSaveState VersionTwentyOne = Save;
	VersionTwentyOne.SaveVersion = FGameXXKSaveMigration::DesktopInventoryStorageIntroducedSaveVersion;
	VersionTwentyOne.RuntimeState.CardRun.PartySelection.QuestNpcCardLoadouts.Reset();
	FGameXXKSaveState MigratedTwentyTwo;
	FGameXXKSaveMigrationReport MigrationReport;
	TestTrue(TEXT("v21 save migrates into the owned-NPC v22 schema"),
		FGameXXKSaveMigration::MigrateToCurrent(VersionTwentyOne, MigratedTwentyTwo, MigrationReport));
	TestEqual(TEXT("v21 migration writes the current save schema"),
		MigratedTwentyTwo.SaveVersion,
		FGameXXKSaveMigration::CurrentSaveVersion);
	TestEqual(TEXT("v21 migration seeds all six owned NPC loadouts"),
		MigratedTwentyTwo.RuntimeState.CardRun.PartySelection.QuestNpcCardLoadouts.Num(), 6);
	const FGameXXKQuestNpcOwnedCardLoadout* MigratedTusi =
		MigratedTwentyTwo.RuntimeState.CardRun.PartySelection.QuestNpcCardLoadouts.Find(TusiChief->NpcId);
	TestNotNull(TEXT("v21 migration keeps the active Tusi loadout"), MigratedTusi);
	if (MigratedTusi)
	{
		TestEqual(TEXT("v21 migration preserves the active edited cards"), MigratedTusi->SelectedCardIds, EditedTusiCards);
	}
	return true;
}

#endif
