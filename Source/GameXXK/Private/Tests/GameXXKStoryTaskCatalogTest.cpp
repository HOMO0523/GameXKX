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
	const FGameXXKStoryDefinition* Story =
		FGameXXKStoryCatalog::FindStory(TEXT("Story.Main.XuXiakeTreasure"));
	const FGameXXKTaskDefinition* Prologue =
		FGameXXKStoryCatalog::FindTask(TEXT("Task.Main.XuXiake.Prologue"));
	if (!TestNotNull(TEXT("main story exists"), Story)
		|| !TestNotNull(TEXT("prologue task exists"), Prologue))
	{
		return false;
	}
	TestTrue(TEXT("prologue belongs to story"), Story->TaskIds.Contains(Prologue->TaskId));
	TestEqual(TEXT("river scroll is entry"),
		Prologue->EntryStepId, FName(TEXT("Step.Main.XuXiake.RiverScroll")));
	const FGameXXKTaskStepDefinition* RiverStep = Prologue->Steps.FindByPredicate(
		[](const FGameXXKTaskStepDefinition& Step)
		{
			return Step.StepId == TEXT("Step.Main.XuXiake.RiverScroll");
		});
	const FGameXXKTaskStepDefinition* CombatStep = Prologue->Steps.FindByPredicate(
		[](const FGameXXKTaskStepDefinition& Step)
		{
			return Step.StepId == TEXT("Step.Main.XuXiake.CombatTutorial");
		});
	TestTrue(TEXT("river step references scene-independent sequence"),
		RiverStep
			&& RiverStep->SequenceId == TEXT("Sequence.Main.XuXiake.CarriageArrival")
			&& RiverStep->StageContractId == TEXT("Stage.Tutorial.River")
			&& RiverStep->NextStepIds == TArray<FName>{TEXT("Step.Main.XuXiake.CombatTutorial")});
	TestTrue(TEXT("combat step references fixed route and encounter"),
		CombatStep
			&& CombatStep->RouteId == TEXT("Route.Tutorial.CombatBasics")
			&& CombatStep->EncounterId == TEXT("Encounter.Main.XuXiake.0-1"));

	FGameXXKNarrativeProgress Progress;
	FString Error;
	TestTrue(TEXT("main story starts"), FGameXXKStoryRules::StartStory(*Story, Progress, &Error));
	TestTrue(TEXT("prologue task starts"), FGameXXKStoryRules::StartTask(*Prologue, Progress, &Error));

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
	TestTrue(TEXT("track main task"),
		FGameXXKStoryRules::TrackTask(Prologue->TaskId, Progress, &Error));
	TestEqual(TEXT("two stories remain active"), Progress.StoryProgressById.Num(), 2);
	TestEqual(TEXT("two tasks remain active"), Progress.TaskProgressById.Num(), 2);
	TestEqual(TEXT("tracked task is independent"), Progress.TrackedTaskId, Prologue->TaskId);
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
	const FGameXXKStoryDefinition* Story =
		FGameXXKStoryCatalog::FindStory(TEXT("Story.Main.XuXiakeTreasure"));
	const FGameXXKTaskDefinition* Prologue =
		FGameXXKStoryCatalog::FindTask(TEXT("Task.Main.XuXiake.Prologue"));
	if (!Story || !Prologue)
	{
		return false;
	}
	FGameXXKNarrativeProgress Progress;
	FString Error;
	FGameXXKStoryRules::StartStory(*Story, Progress, &Error);
	FGameXXKStoryRules::StartTask(*Prologue, Progress, &Error);
	TestFalse(TEXT("task cannot jump to an unknown step"),
		FGameXXKStoryRules::AdvanceTask(*Prologue, TEXT("Step.Missing"), Progress, &Error));
	TestEqual(TEXT("failed jump keeps river step"),
		Progress.TaskProgressById.FindChecked(Prologue->TaskId).CurrentStepId,
		Prologue->EntryStepId);
	TestTrue(TEXT("river advances only to combat tutorial"),
		FGameXXKStoryRules::AdvanceTask(
			*Prologue, TEXT("Step.Main.XuXiake.CombatTutorial"), Progress, &Error));
	TestTrue(TEXT("terminal task completes"),
		FGameXXKStoryRules::CompleteTask(*Prologue, Progress, &Error));
	TestEqual(TEXT("task is completed before reward"),
		Progress.TaskProgressById.FindChecked(Prologue->TaskId).State,
		EGameXXKTaskState::Completed);
	TestTrue(TEXT("reward commits once"),
		FGameXXKStoryRules::CommitTaskReward(*Prologue, Progress, &Error));
	TestEqual(TEXT("task is rewarded"),
		Progress.TaskProgressById.FindChecked(Prologue->TaskId).State,
		EGameXXKTaskState::Rewarded);
	TestFalse(TEXT("reward cannot commit twice"),
		FGameXXKStoryRules::CommitTaskReward(*Prologue, Progress, &Error));
	return true;
}

#endif
