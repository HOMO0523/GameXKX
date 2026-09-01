#include "Misc/AutomationTest.h"

#include "Narrative/GameXXKNarrativeTypes.h"
#include "Narrative/GameXXKStoryCatalog.h"
#include "Narrative/GameXXKStoryRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKStoryTaskCatalogConcurrencyTest,
	"GameXXK.Narrative.StoryTask.ConcurrentAndTracked",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKStoryTaskCatalogConcurrencyTest::RunTest(const FString& Parameters)
{
	TestNull(TEXT("retired main story is absent"),
		FGameXXKStoryCatalog::FindStory(TEXT("Story.Main.XuXiakeTreasure")));
	TestNull(TEXT("retired prologue task is absent"),
		FGameXXKStoryCatalog::FindTask(TEXT("Task.Main.XuXiake.Prologue")));
	TestTrue(TEXT("retired catalog has no player-facing stories"),
		FGameXXKStoryCatalog::GetStories().IsEmpty());
	TestTrue(TEXT("retired catalog has no player-facing tasks"),
		FGameXXKStoryCatalog::GetTasks().IsEmpty());
	TestTrue(TEXT("empty catalog remains structurally valid"),
		FGameXXKStoryCatalog::Validate(nullptr));

	FGameXXKNarrativeProgress Progress;
	FString Error;
	FGameXXKStoryDefinition SideStory;
	SideStory.StoryId = TEXT("Story.Side.Test");
	SideStory.Version = 1;
	SideStory.TaskIds = {TEXT("Task.Side.Test")};
	FGameXXKTaskDefinition SideTask;
	SideTask.TaskId = TEXT("Task.Side.Test");
	SideTask.StoryId = SideStory.StoryId;
	SideTask.EntryStepId = TEXT("Step.Side.Test");
	FGameXXKTaskStepDefinition SideStep;
	SideStep.StepId = SideTask.EntryStepId;
	SideTask.Steps = {SideStep};
	TestTrue(TEXT("side story starts concurrently"),
		FGameXXKStoryRules::StartStory(SideStory, Progress, &Error));
	TestTrue(TEXT("side task starts concurrently"),
		FGameXXKStoryRules::StartTask(SideTask, Progress, &Error));
	TestTrue(TEXT("track synthetic task"),
		FGameXXKStoryRules::TrackTask(SideTask.TaskId, Progress, &Error));
	TestEqual(TEXT("one synthetic story remains active"), Progress.StoryProgressById.Num(), 1);
	TestEqual(TEXT("one synthetic task remains active"), Progress.TaskProgressById.Num(), 1);
	TestEqual(TEXT("tracked task is synthetic"), Progress.TrackedTaskId, SideTask.TaskId);
	TestEqual(TEXT("side task remains active"),
		Progress.TaskProgressById.FindChecked(SideTask.TaskId).State,
		EGameXXKTaskState::Active);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKStoryTaskProgressionTest,
	"GameXXK.Narrative.StoryTask.ProgressionAndRewardOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKStoryTaskProgressionTest::RunTest(const FString& Parameters)
{
	FGameXXKStoryDefinition Story;
	Story.StoryId = TEXT("Story.Test.Progression");
	Story.Version = 1;
	Story.TaskIds = {TEXT("Task.Test.Progression")};
	FGameXXKTaskDefinition TaskDefinition;
	TaskDefinition.TaskId = Story.TaskIds[0];
	TaskDefinition.StoryId = Story.StoryId;
	TaskDefinition.EntryStepId = TEXT("Step.Test.One");
	FGameXXKTaskStepDefinition FirstStep;
	FirstStep.StepId = TaskDefinition.EntryStepId;
	FirstStep.NextStepIds = {TEXT("Step.Test.Two")};
	FGameXXKTaskStepDefinition SecondStep;
	SecondStep.StepId = TEXT("Step.Test.Two");
	TaskDefinition.Steps = {FirstStep, SecondStep};
	FGameXXKNarrativeProgress Progress;
	FString Error;
	FGameXXKStoryRules::StartStory(Story, Progress, &Error);
	FGameXXKStoryRules::StartTask(TaskDefinition, Progress, &Error);
	TestFalse(TEXT("task cannot jump to an unknown step"),
		FGameXXKStoryRules::AdvanceTask(TaskDefinition, TEXT("Step.Missing"), Progress, &Error));
	TestEqual(TEXT("failed jump keeps entry step"),
		Progress.TaskProgressById.FindChecked(TaskDefinition.TaskId).CurrentStepId,
		TaskDefinition.EntryStepId);
	TestTrue(TEXT("entry advances only to its authored successor"),
		FGameXXKStoryRules::AdvanceTask(
			TaskDefinition, SecondStep.StepId, Progress, &Error));
	TestTrue(TEXT("terminal task completes"),
		FGameXXKStoryRules::CompleteTask(TaskDefinition, Progress, &Error));
	TestEqual(TEXT("task is completed before reward"),
		Progress.TaskProgressById.FindChecked(TaskDefinition.TaskId).State,
		EGameXXKTaskState::Completed);
	TestTrue(TEXT("reward commits once"),
		FGameXXKStoryRules::CommitTaskReward(TaskDefinition, Progress, &Error));
	TestEqual(TEXT("task is rewarded"),
		Progress.TaskProgressById.FindChecked(TaskDefinition.TaskId).State,
		EGameXXKTaskState::Rewarded);
	TestFalse(TEXT("reward cannot commit twice"),
		FGameXXKStoryRules::CommitTaskReward(TaskDefinition, Progress, &Error));
	return true;
}

#endif
