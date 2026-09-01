#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "Guide/GameXXKGuideAsset.h"
#include "MVP/GameXXKSaveMigration.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Narrative/GameXXKNarrativeSequenceTypes.h"
#include "Narrative/GameXXKNarrativeTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKNarrativeGuideSaveMigrationTestPrivate
{
	FGameXXKSaveState MakeSave(const int32 Version, const EGameXXKTutorialQuestState TutorialState)
	{
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		FGameXXKRuntimeState State = Subsystem && Subsystem->StartGame()
			? Subsystem->GetRuntimeStateCopy()
			: FGameXXKRuntimeState();
		State.TutorialQuest.State = TutorialState;
		State.TutorialQuest.CurrentStepId = TutorialState == EGameXXKTutorialQuestState::Active
			? FName(TEXT("Tutorial.EnterTown"))
			: NAME_None;
		FGameXXKSaveState Save = UGameXXKMVPRules::MakeSaveState(State);
		Save.SaveVersion = Version;
		return Save;
	}

	void FillDialogue(FGameXXKDialogueSessionState& Session)
	{
		Session = FGameXXKDialogueSessionState();
		Session.bActive = true;
		Session.StoryId = TEXT("Story.Main.XuXiakeTreasure");
		Session.StoryVersion = 1;
		Session.TaskId = TEXT("Task.Main.XuXiake.Prologue");
		Session.StepId = TEXT("Step.Main.XuXiake.RiverScroll");
		Session.SequenceId = TEXT("Sequence.Main.XuXiake.CarriageArrival");
		Session.StageContractId = TEXT("Stage.Tutorial.River");
		Session.DialogueId = TEXT("Dialogue.Main.XuXiake.001");
		Session.DialogueVersion = 1;
		Session.CurrentNodeId = TEXT("line.yuebai.who");
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNarrativeGuideSaveMigrationV29Test,
	"GameXXK.Narrative.SaveMigration.V29",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNarrativeGuideSaveMigrationV29Test::RunTest(const FString& Parameters)
{
	using namespace GameXXKNarrativeGuideSaveMigrationTestPrivate;
	TestEqual(TEXT("narrative-stage-guide owns save version 29"),
		FGameXXKSaveMigration::NarrativeStageGuideIntroducedSaveVersion, 29);
	TestEqual(TEXT("v32 retires the disconnected tutorial narrative"), FGameXXKSaveMigration::CurrentSaveVersion, 32);

	FGameXXKSaveState OrdinaryV28 = MakeSave(28, EGameXXKTutorialQuestState::NotStarted);
	FillDialogue(OrdinaryV28.RuntimeState.DialogueSession);
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	if (!TestTrue(FString::Printf(TEXT("ordinary v28 migrates: %s"), *Report.Error),
		FGameXXKSaveMigration::MigrateToCurrent(OrdinaryV28, Migrated, Report)))
	{
		return false;
	}
	TestEqual(TEXT("save reaches the current schema"),
		Migrated.SaveVersion,
		FGameXXKSaveMigration::CurrentSaveVersion);
	TestTrue(TEXT("ordinary save has no story progress"), Migrated.RuntimeState.NarrativeProgress.StoryProgressById.IsEmpty());
	TestTrue(TEXT("ordinary save has no task progress"), Migrated.RuntimeState.NarrativeProgress.TaskProgressById.IsEmpty());
	TestFalse(TEXT("ordinary save has no sequence session"), Migrated.RuntimeState.NarrativeSequenceSession.bActive);
	TestEqual(TEXT("guide preference defaults unset"),
		Migrated.RuntimeState.GuideProgress.Preference, EGameXXKGuidePreference::Unset);
	TestTrue(TEXT("guide completed steps default empty"),
		Migrated.RuntimeState.GuideProgress.CompletedGuideStepIds.IsEmpty());
	TestFalse(TEXT("retired v28 tutorial dialogue is cleared"),
		Migrated.RuntimeState.DialogueSession.bActive);

	FGameXXKSaveState ActiveV27 = MakeSave(27, EGameXXKTutorialQuestState::Active);
	TestTrue(TEXT("active v27 tutorial migrates"),
		FGameXXKSaveMigration::MigrateToCurrent(ActiveV27, Migrated, Report));
	TestTrue(TEXT("active legacy tutorial creates no retired story"),
		Migrated.RuntimeState.NarrativeProgress.StoryProgressById.IsEmpty());
	TestTrue(TEXT("active legacy tutorial creates no retired task"),
		Migrated.RuntimeState.NarrativeProgress.TaskProgressById.IsEmpty());
	TestTrue(TEXT("active legacy tutorial tracks no retired task"),
		Migrated.RuntimeState.NarrativeProgress.TrackedTaskId.IsNone());

	FGameXXKSaveState CompletedV27 = MakeSave(27, EGameXXKTutorialQuestState::Completed);
	TestTrue(TEXT("completed v27 tutorial migrates"),
		FGameXXKSaveMigration::MigrateToCurrent(CompletedV27, Migrated, Report));
	TestTrue(TEXT("completed legacy tutorial creates no story receipt"),
		Migrated.RuntimeState.NarrativeProgress.StoryProgressById.IsEmpty());
	TestTrue(TEXT("completed legacy tutorial creates no task receipt"),
		Migrated.RuntimeState.NarrativeProgress.TaskProgressById.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNarrativeGuideSaveValidationTest,
	"GameXXK.Narrative.SaveMigration.V29Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNarrativeGuideSaveValidationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKNarrativeGuideSaveMigrationTestPrivate;
	FGameXXKSaveState Source = MakeSave(27, EGameXXKTutorialQuestState::Active);
	FGameXXKSaveState Current;
	FGameXXKSaveMigrationReport Report;
	if (!TestTrue(TEXT("active legacy fixture migrates"),
		FGameXXKSaveMigration::MigrateToCurrent(Source, Current, Report)))
	{
		return false;
	}

	FGameXXKSaveState OrphanGuide = Current;
	OrphanGuide.RuntimeState.NarrativeProgress = FGameXXKNarrativeProgress();
	OrphanGuide.RuntimeState.GuideProgress.Preference = EGameXXKGuidePreference::NewPlayer;
	OrphanGuide.RuntimeState.GuideProgress.ActiveGuideId = TEXT("Guide.RouteMap.Basic");
	OrphanGuide.RuntimeState.GuideProgress.ActiveGuideStepId = TEXT("Guide.RouteMap.Basic.SelectNext");
	FGameXXKSaveState RoundTrip;
	TestFalse(TEXT("active guide without active task rejects"),
		FGameXXKSaveMigration::MigrateToCurrent(OrphanGuide, RoundTrip, Report));

	FGameXXKSaveState UnknownStory = Current;
	UnknownStory.RuntimeState.NarrativeProgress.StoryProgressById.Add(
		TEXT("Story.Unknown"), FGameXXKStoryProgress());
	TestFalse(TEXT("unknown story progress rejects"),
		FGameXXKSaveMigration::MigrateToCurrent(UnknownStory, RoundTrip, Report));

	TestTrue(TEXT("retired legacy narrative leaves an ordinary current save valid"),
		FGameXXKSaveMigration::MigrateToCurrent(Current, RoundTrip, Report));
	return true;
}

#endif
