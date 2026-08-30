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
	TestEqual(TEXT("v30 follows the narrative-stage-guide boundary"), FGameXXKSaveMigration::CurrentSaveVersion, 30);

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
	TestTrue(TEXT("v28 dialogue remains active"), Migrated.RuntimeState.DialogueSession.bActive);
	TestEqual(TEXT("v28 dialogue ID preserved"),
		Migrated.RuntimeState.DialogueSession.DialogueId,
		OrdinaryV28.RuntimeState.DialogueSession.DialogueId);

	FGameXXKSaveState ActiveV27 = MakeSave(27, EGameXXKTutorialQuestState::Active);
	TestTrue(TEXT("active v27 tutorial migrates"),
		FGameXXKSaveMigration::MigrateToCurrent(ActiveV27, Migrated, Report));
	const FName StoryId(TEXT("Story.Main.XuXiakeTreasure"));
	const FName TaskId(TEXT("Task.Main.XuXiake.Prologue"));
	const FGameXXKStoryProgress* ActiveStory = Migrated.RuntimeState.NarrativeProgress.StoryProgressById.Find(StoryId);
	const FGameXXKTaskProgress* ActiveTask = Migrated.RuntimeState.NarrativeProgress.TaskProgressById.Find(TaskId);
	TestNotNull(TEXT("active tutorial creates main story"), ActiveStory);
	TestNotNull(TEXT("active tutorial creates prologue task"), ActiveTask);
	if (ActiveStory && ActiveTask)
	{
		TestEqual(TEXT("main story remains active"), ActiveStory->State, EGameXXKStoryState::Active);
		TestEqual(TEXT("prologue task remains active"), ActiveTask->State, EGameXXKTaskState::Active);
		TestEqual(TEXT("legacy entry maps to river step"),
			ActiveTask->CurrentStepId, FName(TEXT("Step.Main.XuXiake.RiverScroll")));
	}
	TestEqual(TEXT("migrated active task is tracked"),
		Migrated.RuntimeState.NarrativeProgress.TrackedTaskId, TaskId);

	FGameXXKSaveState CompletedV27 = MakeSave(27, EGameXXKTutorialQuestState::Completed);
	TestTrue(TEXT("completed v27 tutorial migrates"),
		FGameXXKSaveMigration::MigrateToCurrent(CompletedV27, Migrated, Report));
	const FGameXXKStoryProgress* CompletedStory = Migrated.RuntimeState.NarrativeProgress.StoryProgressById.Find(StoryId);
	const FGameXXKTaskProgress* CompletedTask = Migrated.RuntimeState.NarrativeProgress.TaskProgressById.Find(TaskId);
	TestNotNull(TEXT("completed tutorial creates story receipt"), CompletedStory);
	TestNotNull(TEXT("completed tutorial creates task receipt"), CompletedTask);
	if (CompletedStory && CompletedTask)
	{
		TestEqual(TEXT("story migration completes"), CompletedStory->State, EGameXXKStoryState::Completed);
		TestEqual(TEXT("task migration prevents reward replay"), CompletedTask->State, EGameXXKTaskState::Rewarded);
		TestTrue(TEXT("task reward receipt is committed"), CompletedTask->bRewardCommitted);
	}
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

	FGameXXKSaveState ValidGuide = Current;
	FGameXXKTaskProgress* CombatTutorialTask =
		ValidGuide.RuntimeState.NarrativeProgress.TaskProgressById.Find(TEXT("Task.Main.XuXiake.Prologue"));
	if (!TestNotNull(TEXT("combat-guide fixture has prologue task"), CombatTutorialTask))
	{
		return false;
	}
	CombatTutorialTask->CurrentStepId = TEXT("Step.Main.XuXiake.CombatTutorial");
	ValidGuide.RuntimeState.GuideProgress.Preference = EGameXXKGuidePreference::NewPlayer;
	ValidGuide.RuntimeState.GuideProgress.ActiveGuideId = TEXT("Guide.RouteMap.Basic");
	ValidGuide.RuntimeState.GuideProgress.ActiveGuideStepId = TEXT("Guide.RouteMap.Basic.SelectNext");
	TestTrue(FString::Printf(TEXT("guide attached to active combat tutorial validates: %s"), *Report.Error),
		FGameXXKSaveMigration::MigrateToCurrent(ValidGuide, RoundTrip, Report));

	FGameXXKSaveState ValidSequence = Current;
	FGameXXKNarrativeSequenceSessionState& Sequence = ValidSequence.RuntimeState.NarrativeSequenceSession;
	Sequence.bActive = true;
	Sequence.StoryId = TEXT("Story.Main.XuXiakeTreasure");
	Sequence.StoryVersion = 1;
	Sequence.TaskId = TEXT("Task.Main.XuXiake.Prologue");
	Sequence.StepId = TEXT("Step.Main.XuXiake.RiverScroll");
	Sequence.SequenceId = TEXT("Sequence.Main.XuXiake.CarriageArrival");
	Sequence.SequenceVersion = 1;
	Sequence.StageContractId = TEXT("Stage.Tutorial.River");
	Sequence.CurrentSequenceStepId = TEXT("Step.Sequence.Arrival");
	Sequence.CharacterIdByRole.Add(TEXT("Hero"), TEXT("Character.Hero"));
	TestTrue(FString::Printf(TEXT("sequence matching active task validates: %s"), *Report.Error),
		FGameXXKSaveMigration::MigrateToCurrent(ValidSequence, RoundTrip, Report));

	ValidSequence.RuntimeState.NarrativeSequenceSession.StageContractId = TEXT("Stage.Wrong");
	TestFalse(TEXT("sequence stage detached from task rejects"),
		FGameXXKSaveMigration::MigrateToCurrent(ValidSequence, RoundTrip, Report));
	return true;
}

#endif
