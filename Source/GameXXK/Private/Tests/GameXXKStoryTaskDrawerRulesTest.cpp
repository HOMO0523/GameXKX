#include "Misc/AutomationTest.h"

#include "Narrative/GameXXKStoryCatalog.h"
#include "Narrative/GameXXKStoryTaskDrawerRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKStoryTaskDrawerRulesTestPrivate
{
	const FName PrologueId(TEXT("Task.Main.XuXiake.Prologue"));
	const FName CombatId(TEXT("Task.Main.XuXiake.CombatBasics"));
	const FName FirstJourneyId(TEXT("Task.Main.XuXiake.FirstJourney"));

	const FGameXXKTaskDefinition* FindTask(const FName TaskId)
	{
		return FGameXXKStoryCatalog::FindTask(TaskId);
	}

	FGameXXKTaskProgress& SetTaskProgress(
		FGameXXKNarrativeProgress& Progress,
		const FName TaskId,
		const EGameXXKTaskState State,
		const FName CurrentStepId = NAME_None,
		const bool bRewardCommitted = false,
		const int64 CompletedAtUtcTicks = 0)
	{
		FGameXXKTaskProgress& TaskProgress = Progress.TaskProgressById.FindOrAdd(TaskId);
		TaskProgress.State = State;
		TaskProgress.CurrentStepId = CurrentStepId;
		TaskProgress.bRewardCommitted = bRewardCommitted;
		TaskProgress.CompletedAtUtcTicks = CompletedAtUtcTicks;
		return TaskProgress;
	}

	FGameXXKTaskDefinition MakeIndependentTask(const FName TaskId, const int32 AuthoredOrder)
	{
		FGameXXKTaskDefinition Task;
		Task.TaskId = TaskId;
		Task.StoryId = TEXT("Story.DrawerRules.Sorting");
		Task.AuthoredOrder = AuthoredOrder;
		Task.EntryStepId = FName(*FString::Printf(TEXT("%s.Entry"), *TaskId.ToString()));
		FGameXXKTaskStepDefinition Step;
		Step.StepId = Task.EntryStepId;
		Task.Steps.Add(Step);
		return Task;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKStoryTaskDrawerRulesTest,
	"GameXXK.Narrative.StoryTask.DrawerRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKStoryTaskDrawerRulesTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKStoryTaskDrawerRulesTestPrivate;
	const TArray<FGameXXKTaskDefinition>& Tasks = FGameXXKStoryCatalog::GetTasks();
	const FGameXXKTaskDefinition* Prologue = FindTask(PrologueId);
	const FGameXXKTaskDefinition* Combat = FindTask(CombatId);
	const FGameXXKTaskDefinition* FirstJourney = FindTask(FirstJourneyId);
	if (!TestNotNull(TEXT("prologue definition exists"), Prologue)
		|| !TestNotNull(TEXT("combat definition exists"), Combat)
		|| !TestNotNull(TEXT("first journey definition exists"), FirstJourney))
	{
		return false;
	}

	FGameXXKStoryTaskDrawerUiState UiState;
	UiState.SelectedActionableTaskId = CombatId;
	UiState.SelectedClaimableTaskId = FirstJourneyId;

	FGameXXKNarrativeProgress FreshProgress;
	const FGameXXKStoryTaskDrawerSnapshot FreshSnapshot =
		FGameXXKStoryTaskDrawerRules::BuildSnapshot(Tasks, FreshProgress, UiState);
	if (TestEqual(TEXT("fresh has exactly prologue actionable"), FreshSnapshot.Actionable.Num(), 1)
		&& TestTrue(TEXT("fresh actionable row exists"), FreshSnapshot.Actionable.IsValidIndex(0)))
	{
		TestEqual(TEXT("fresh exposes prologue"), FreshSnapshot.Actionable[0].TaskId, PrologueId);
		TestEqual(TEXT("fresh prologue is available"), FreshSnapshot.Actionable[0].State, EGameXXKTaskState::Available);
		TestEqual(TEXT("fresh prologue accept label"), FreshSnapshot.Actionable[0].ActionLabel.ToString(), FString(TEXT("接取任务")));
	}
	TestEqual(TEXT("fresh default actionable selection is first row"), FreshSnapshot.SelectedActionableTaskId, PrologueId);
	TestEqual(TEXT("fresh no claimable rows"), FreshSnapshot.Claimable.Num(), 0);
	TestFalse(TEXT("fresh has no claim red dot"), FreshSnapshot.bHasClaimableRedDot);
	TestEqual(TEXT("input actionable selection is not mutated"), UiState.SelectedActionableTaskId, CombatId);
	TestEqual(TEXT("input claimable selection is not mutated"), UiState.SelectedClaimableTaskId, FirstJourneyId);
	TestTrue(TEXT("input progress remains empty"), FreshProgress.TaskProgressById.IsEmpty());

	FGameXXKNarrativeProgress ActivePrologueProgress;
	SetTaskProgress(
		ActivePrologueProgress,
		PrologueId,
		EGameXXKTaskState::Active,
		TEXT("Step.Main.XuXiake.RiverScroll"));
	SetTaskProgress(ActivePrologueProgress, CombatId, EGameXXKTaskState::Available);
	const FGameXXKStoryTaskDrawerSnapshot ActivePrologueSnapshot =
		FGameXXKStoryTaskDrawerRules::BuildSnapshot(Tasks, ActivePrologueProgress, FGameXXKStoryTaskDrawerUiState());
	if (TestTrue(TEXT("active prologue actionable row exists"), ActivePrologueSnapshot.Actionable.IsValidIndex(0)))
	{
		TestEqual(TEXT("active prologue precedes available tasks"), ActivePrologueSnapshot.Actionable[0].TaskId, PrologueId);
		TestEqual(TEXT("active prologue uses continue label"), ActivePrologueSnapshot.Actionable[0].ActionLabel.ToString(), FString(TEXT("继续剧情")));
		TestEqual(TEXT("river step resumes narrative replay"), ActivePrologueSnapshot.Actionable[0].Continuation, EGameXXKStoryTaskContinuation::NarrativeReplay);
	}

	FGameXXKNarrativeProgress CompletedPrologueProgress;
	SetTaskProgress(CompletedPrologueProgress, PrologueId, EGameXXKTaskState::Completed, NAME_None, false, 10);
	SetTaskProgress(CompletedPrologueProgress, CombatId, EGameXXKTaskState::Available);
	const FGameXXKStoryTaskDrawerSnapshot CompletedPrologueSnapshot =
		FGameXXKStoryTaskDrawerRules::BuildSnapshot(Tasks, CompletedPrologueProgress, FGameXXKStoryTaskDrawerUiState());
	if (TestTrue(TEXT("completed prologue has actionable combat row"), CompletedPrologueSnapshot.Actionable.IsValidIndex(0)))
	{
		TestEqual(TEXT("available combat remains actionable while prologue is claimable"), CompletedPrologueSnapshot.Actionable[0].TaskId, CombatId);
	}
	if (TestTrue(TEXT("completed prologue has claimable row"), CompletedPrologueSnapshot.Claimable.IsValidIndex(0)))
	{
		TestEqual(TEXT("claimable prologue is visible"), CompletedPrologueSnapshot.Claimable[0].TaskId, PrologueId);
		TestEqual(TEXT("claimable prologue uses claim label"), CompletedPrologueSnapshot.Claimable[0].ActionLabel.ToString(), FString(TEXT("领取奖励")));
	}
	TestTrue(TEXT("claimable prologue raises red dot"), CompletedPrologueSnapshot.bHasClaimableRedDot);

	const FGameXXKTaskDefinition ActiveTask = MakeIndependentTask(TEXT("Task.DrawerRules.Active"), 99);
	const FGameXXKTaskDefinition AvailableEarly = MakeIndependentTask(TEXT("Task.DrawerRules.Available.Early"), 5);
	const FGameXXKTaskDefinition AvailableTieB = MakeIndependentTask(TEXT("Task.DrawerRules.Available.B"), 10);
	const FGameXXKTaskDefinition AvailableTieA = MakeIndependentTask(TEXT("Task.DrawerRules.Available.A"), 10);
	const TArray<FGameXXKTaskDefinition> SortingTasks = {
		AvailableTieB, AvailableEarly, ActiveTask, AvailableTieA};
	FGameXXKNarrativeProgress SortingProgress;
	SetTaskProgress(
		SortingProgress,
		ActiveTask.TaskId,
		EGameXXKTaskState::Active,
		ActiveTask.EntryStepId);
	SortingProgress.TrackedTaskId = ActiveTask.TaskId;
	FGameXXKStoryTaskDrawerUiState SortingUiState;
	SortingUiState.SelectedActionableTaskId = AvailableTieA.TaskId;
	const FGameXXKStoryTaskDrawerSnapshot SortingSnapshot =
		FGameXXKStoryTaskDrawerRules::BuildSnapshot(SortingTasks, SortingProgress, SortingUiState);
	if (TestEqual(TEXT("sorting has the active task and three available tasks"), SortingSnapshot.Actionable.Num(), 4)
		&& TestTrue(TEXT("sorting rows are safe to inspect"), SortingSnapshot.Actionable.IsValidIndex(3)))
	{
		TestEqual(TEXT("active sorts before available tasks"), SortingSnapshot.Actionable[0].TaskId, ActiveTask.TaskId);
		TestEqual(TEXT("available tasks sort by authored order"), SortingSnapshot.Actionable[1].TaskId, AvailableEarly.TaskId);
		TestEqual(TEXT("available authored-order tie sorts by task id first"), SortingSnapshot.Actionable[2].TaskId, AvailableTieA.TaskId);
		TestEqual(TEXT("available authored-order tie sorts by task id second"), SortingSnapshot.Actionable[3].TaskId, AvailableTieB.TaskId);
	}
	TestEqual(TEXT("saved nonfirst actionable selection is preserved"),
		SortingSnapshot.SelectedActionableTaskId, AvailableTieA.TaskId);

	const FGameXXKTaskDefinition FirstActiveTask = MakeIndependentTask(TEXT("Task.DrawerRules.Active.First"), 1);
	const FGameXXKTaskDefinition TrackedActiveTask = MakeIndependentTask(TEXT("Task.DrawerRules.Active.Tracked"), 2);
	const TArray<FGameXXKTaskDefinition> TrackedActiveTasks = {TrackedActiveTask, FirstActiveTask};
	FGameXXKNarrativeProgress TrackedActiveProgress;
	SetTaskProgress(TrackedActiveProgress, FirstActiveTask.TaskId, EGameXXKTaskState::Active, FirstActiveTask.EntryStepId);
	SetTaskProgress(TrackedActiveProgress, TrackedActiveTask.TaskId, EGameXXKTaskState::Active, TrackedActiveTask.EntryStepId);
	TrackedActiveProgress.TrackedTaskId = TrackedActiveTask.TaskId;
	const FGameXXKStoryTaskDrawerSnapshot TrackedActiveSnapshot =
		FGameXXKStoryTaskDrawerRules::BuildSnapshot(
			TrackedActiveTasks, TrackedActiveProgress, FGameXXKStoryTaskDrawerUiState());
	if (TestEqual(TEXT("tracked active fixture has two active rows"), TrackedActiveSnapshot.Actionable.Num(), 2)
		&& TestTrue(TEXT("tracked active rows are safe to inspect"), TrackedActiveSnapshot.Actionable.IsValidIndex(1)))
	{
		TestEqual(TEXT("tracked active task is not the first active row"),
			TrackedActiveSnapshot.Actionable[1].TaskId, TrackedActiveTask.TaskId);
	}
	TestEqual(TEXT("tracked active task restores actionable selection after invalid saved selection"),
		TrackedActiveSnapshot.SelectedActionableTaskId, TrackedActiveTask.TaskId);

	FGameXXKNarrativeProgress ContinuationProgress;
	SetTaskProgress(ContinuationProgress, CombatId, EGameXXKTaskState::Active, TEXT("Step.Main.XuXiake.CombatRoute"));
	const FGameXXKStoryTaskDrawerSnapshot CombatRouteSnapshot =
		FGameXXKStoryTaskDrawerRules::BuildSnapshot(Tasks, ContinuationProgress, FGameXXKStoryTaskDrawerUiState());
	if (TestTrue(TEXT("combat route actionable row exists"), CombatRouteSnapshot.Actionable.IsValidIndex(0)))
	{
		TestEqual(TEXT("combat route resumes route"), CombatRouteSnapshot.Actionable[0].Continuation, EGameXXKStoryTaskContinuation::RouteResume);
	}
	ContinuationProgress.TaskProgressById.Reset();
	SetTaskProgress(ContinuationProgress, FirstJourneyId, EGameXXKTaskState::Active, TEXT("Step.Main.XuXiake.FirstJourneyEncounter"));
	const FGameXXKStoryTaskDrawerSnapshot FirstJourneySnapshot =
		FGameXXKStoryTaskDrawerRules::BuildSnapshot(Tasks, ContinuationProgress, FGameXXKStoryTaskDrawerUiState());
	if (TestTrue(TEXT("first journey actionable row exists"), FirstJourneySnapshot.Actionable.IsValidIndex(0)))
	{
		TestEqual(TEXT("first journey encounter resumes objective"), FirstJourneySnapshot.Actionable[0].Continuation, EGameXXKStoryTaskContinuation::ObjectiveResume);
	}

	FGameXXKTaskDefinition RouteAndObjectiveTask = MakeIndependentTask(TEXT("Task.DrawerRules.RouteAndObjective"), 0);
	if (TestTrue(TEXT("route/objective task step exists"), RouteAndObjectiveTask.Steps.IsValidIndex(0)))
	{
		RouteAndObjectiveTask.Steps[0].RouteId = TEXT("Route.DrawerRules.Priority");
		RouteAndObjectiveTask.Steps[0].ObjectiveId = TEXT("Objective.DrawerRules.Priority");
	}
	FGameXXKNarrativeProgress RouteAndObjectiveProgress;
	SetTaskProgress(RouteAndObjectiveProgress, RouteAndObjectiveTask.TaskId, EGameXXKTaskState::Active, RouteAndObjectiveTask.EntryStepId);
	const FGameXXKStoryTaskDrawerSnapshot RouteAndObjectiveSnapshot =
		FGameXXKStoryTaskDrawerRules::BuildSnapshot(
			TArray<FGameXXKTaskDefinition>{RouteAndObjectiveTask},
			RouteAndObjectiveProgress,
			FGameXXKStoryTaskDrawerUiState());
	if (TestTrue(TEXT("route/objective active row exists"), RouteAndObjectiveSnapshot.Actionable.IsValidIndex(0)))
	{
		TestEqual(TEXT("route takes precedence when current step also has an objective"),
			RouteAndObjectiveSnapshot.Actionable[0].Continuation,
			EGameXXKStoryTaskContinuation::RouteResume);
	}

	FGameXXKNarrativeProgress SortProgress;
	SetTaskProgress(SortProgress, PrologueId, EGameXXKTaskState::Completed, NAME_None, false, 5);
	SetTaskProgress(SortProgress, CombatId, EGameXXKTaskState::Completed, NAME_None, false, 9);
	SetTaskProgress(SortProgress, FirstJourneyId, EGameXXKTaskState::Rewarded, NAME_None, true, 11);
	FGameXXKStoryTaskDrawerUiState RestoreState;
	RestoreState.SelectedClaimableTaskId = PrologueId;
	RestoreState.SelectedActionableTaskId = FirstJourneyId;
	const FGameXXKStoryTaskDrawerSnapshot SortSnapshot =
		FGameXXKStoryTaskDrawerRules::BuildSnapshot(Tasks, SortProgress, RestoreState);
	if (TestEqual(TEXT("rewarded task disappears"), SortSnapshot.Claimable.Num(), 2)
		&& TestTrue(TEXT("sorted claimable rows are safe to inspect"), SortSnapshot.Claimable.IsValidIndex(1)))
	{
		TestEqual(TEXT("claimable newest completion is first"), SortSnapshot.Claimable[0].TaskId, CombatId);
		TestEqual(TEXT("claimable older completion is second"), SortSnapshot.Claimable[1].TaskId, PrologueId);
	}
	TestEqual(TEXT("saved claim selection restores independently"), SortSnapshot.SelectedClaimableTaskId, PrologueId);
	TestEqual(TEXT("empty actionable selection clears to none"), SortSnapshot.SelectedActionableTaskId, NAME_None);

	TArray<FGameXXKTaskDefinition> InvalidTasks = {*Prologue, *Combat, *Combat};
	if (TestTrue(TEXT("invalid task fixture row exists"), InvalidTasks.IsValidIndex(0)))
	{
		InvalidTasks[0].TaskId = NAME_None;
	}
	const FGameXXKStoryTaskDrawerSnapshot InvalidSnapshot =
		FGameXXKStoryTaskDrawerRules::BuildSnapshot(InvalidTasks, FreshProgress, FGameXXKStoryTaskDrawerUiState());
	TestEqual(TEXT("invalid and duplicate task ids are skipped"), InvalidSnapshot.Actionable.Num(), 0);
	TestEqual(TEXT("empty claimable selection clears to none"), InvalidSnapshot.SelectedClaimableTaskId, NAME_None);
	return true;
}

#endif
