#include "Misc/AutomationTest.h"

#include "Dialogue/GameXXKDialogueTypes.h"
#include "Engine/GameInstance.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKSaveMigration.h"
#include "MVP/GameXXKMVPSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKDialogueSaveMigrationTestPrivate
{
	FGameXXKSaveState MakeVersionTwentySevenSave()
	{
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		FGameXXKRuntimeState State = Subsystem && Subsystem->StartGame()
			? Subsystem->GetRuntimeStateCopy()
			: FGameXXKRuntimeState();
		State.TutorialQuest.State = EGameXXKTutorialQuestState::Active;
		State.TutorialQuest.CurrentStepId = TEXT("Tutorial.EnterTown");
		FGameXXKSaveState Save = UGameXXKMVPRules::MakeSaveState(State);
		Save.SaveVersion = FGameXXKSaveMigration::TutorialQuestIntroducedSaveVersion;
		return Save;
	}

	void FillValidActiveDialogue(FGameXXKDialogueSessionState& Session)
	{
		Session = FGameXXKDialogueSessionState();
		Session.bActive = true;
		Session.StoryId = TEXT("Story.Test");
		Session.StoryVersion = 1;
		Session.TaskId = TEXT("Task.Test");
		Session.StepId = TEXT("Step.Test");
		Session.SequenceId = TEXT("Sequence.Test");
		Session.StageContractId = TEXT("Stage.Test");
		Session.DialogueId = TEXT("Dialogue.Test");
		Session.DialogueVersion = 1;
		Session.CurrentNodeId = TEXT("line.current");
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDialogueSaveMigrationV28Test,
	"GameXXK.Dialogue.SaveMigration.V28",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDialogueSaveMigrationV28Test::RunTest(const FString& Parameters)
{
	using namespace GameXXKDialogueSaveMigrationTestPrivate;
	TestEqual(TEXT("dialogue runtime owns save version 28"),
		FGameXXKSaveMigration::DialogueRuntimeIntroducedSaveVersion,
		28);
	TestEqual(TEXT("narrative-stage-guide supersedes dialogue as current save version"),
		FGameXXKSaveMigration::CurrentSaveVersion,
		29);

	const FGameXXKSaveState Source = MakeVersionTwentySevenSave();
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	if (!TestTrue(FString::Printf(TEXT("v27 migration succeeds: %s"), *Report.Error),
		FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report)))
	{
		return false;
	}
	TestEqual(TEXT("save reaches current v29"), Migrated.SaveVersion, 29);
	TestFalse(TEXT("dialogue defaults inactive"), Migrated.RuntimeState.DialogueSession.bActive);
	TestTrue(TEXT("dialogue identity defaults empty"), Migrated.RuntimeState.DialogueSession.DialogueId.IsNone());
	TestEqual(TEXT("tutorial state preserved"),
		Migrated.RuntimeState.TutorialQuest.State,
		EGameXXKTutorialQuestState::Active);
	TestEqual(TEXT("tutorial step preserved"),
		Migrated.RuntimeState.TutorialQuest.CurrentStepId,
		FName(TEXT("Tutorial.EnterTown")));

	FGameXXKSaveState ValidCurrent = Migrated;
	FillValidActiveDialogue(ValidCurrent.RuntimeState.DialogueSession);
	FGameXXKSaveState RoundTrip;
	FGameXXKSaveMigrationReport RoundTripReport;
	TestTrue(FString::Printf(TEXT("valid active dialogue round-trips: %s"), *RoundTripReport.Error),
		FGameXXKSaveMigration::MigrateToCurrent(ValidCurrent, RoundTrip, RoundTripReport));
	TestTrue(TEXT("valid active dialogue remains active"), RoundTrip.RuntimeState.DialogueSession.bActive);

	FGameXXKSaveState InactiveWithContext = Migrated;
	InactiveWithContext.RuntimeState.DialogueSession.StoryId = TEXT("Story.Stale");
	TestFalse(TEXT("inactive session rejects retained context"),
		FGameXXKSaveMigration::MigrateToCurrent(InactiveWithContext, RoundTrip, RoundTripReport));

	FGameXXKSaveState ActiveMissingContext = Migrated;
	ActiveMissingContext.RuntimeState.DialogueSession.bActive = true;
	ActiveMissingContext.RuntimeState.DialogueSession.DialogueId = TEXT("Dialogue.Test");
	ActiveMissingContext.RuntimeState.DialogueSession.DialogueVersion = 1;
	ActiveMissingContext.RuntimeState.DialogueSession.CurrentNodeId = TEXT("line.current");
	TestFalse(TEXT("active session rejects missing narrative context"),
		FGameXXKSaveMigration::MigrateToCurrent(ActiveMissingContext, RoundTrip, RoundTripReport));

	FGameXXKSaveState ExcessHistory = Migrated;
	for (int32 Index = 0; Index < 101; ++Index)
	{
		FGameXXKDialogueHistoryEntry Entry;
		Entry.TextId = FName(*FString::Printf(TEXT("history.%d"), Index));
		ExcessHistory.RuntimeState.DialogueSession.History.Add(Entry);
	}
	TestFalse(TEXT("session rejects more than one hundred history entries"),
		FGameXXKSaveMigration::MigrateToCurrent(ExcessHistory, RoundTrip, RoundTripReport));

	FGameXXKSaveState EmptySelection = Migrated;
	EmptySelection.RuntimeState.DialogueSession.SelectedOptionIds.Add(NAME_None);
	TestFalse(TEXT("session rejects empty committed option id"),
		FGameXXKSaveMigration::MigrateToCurrent(EmptySelection, RoundTrip, RoundTripReport));
	return true;
}

#endif
