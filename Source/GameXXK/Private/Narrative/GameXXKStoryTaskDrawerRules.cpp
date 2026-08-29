#include "Narrative/GameXXKStoryTaskDrawerRules.h"

#include "Narrative/GameXXKStoryRules.h"

namespace GameXXKStoryTaskDrawerRulesPrivate
{
	bool IsTaskDefinitionStructurallyValid(const FGameXXKTaskDefinition& Task)
	{
		if (Task.TaskId.IsNone() || Task.StoryId.IsNone() || Task.EntryStepId.IsNone())
		{
			return false;
		}

		TSet<FName> StepIds;
		for (const FGameXXKTaskStepDefinition& Step : Task.Steps)
		{
			if (Step.StepId.IsNone() || StepIds.Contains(Step.StepId))
			{
				return false;
			}
			StepIds.Add(Step.StepId);
		}
		return StepIds.Contains(Task.EntryStepId);
	}

	const FGameXXKTaskStepDefinition* FindCurrentStep(
		const FGameXXKTaskDefinition& Task,
		const FGameXXKTaskProgress& TaskProgress)
	{
		return Task.Steps.FindByPredicate([&TaskProgress](const FGameXXKTaskStepDefinition& Step)
		{
			return Step.StepId == TaskProgress.CurrentStepId;
		});
	}

	EGameXXKStoryTaskContinuation GetContinuation(
		const FGameXXKTaskDefinition& Task,
		const FGameXXKTaskProgress* TaskProgress)
	{
		if (!TaskProgress || TaskProgress->State != EGameXXKTaskState::Active)
		{
			return EGameXXKStoryTaskContinuation::NarrativeReplay;
		}

		const FGameXXKTaskStepDefinition* CurrentStep = FindCurrentStep(Task, *TaskProgress);
		if (CurrentStep && !CurrentStep->RouteId.IsNone())
		{
			return EGameXXKStoryTaskContinuation::RouteResume;
		}
		if (CurrentStep && !CurrentStep->ObjectiveId.IsNone())
		{
			return EGameXXKStoryTaskContinuation::ObjectiveResume;
		}
		return EGameXXKStoryTaskContinuation::NarrativeReplay;
	}

	FGameXXKStoryTaskDrawerEntryView BuildEntry(
		const FGameXXKTaskDefinition& Task,
		const EGameXXKTaskState State,
		const FGameXXKTaskProgress* TaskProgress)
	{
		FGameXXKStoryTaskDrawerEntryView Entry;
		Entry.TaskId = Task.TaskId;
		Entry.State = State;
		Entry.Title = Task.Title;
		Entry.Summary = Task.Summary;
		Entry.Description = Task.Description;
		Entry.ActionLabel = State == EGameXXKTaskState::Active
			? NSLOCTEXT("GameXXKStoryTaskDrawer", "ContinueStory", "继续剧情")
			: State == EGameXXKTaskState::Completed
				? NSLOCTEXT("GameXXKStoryTaskDrawer", "ClaimReward", "领取奖励")
				: NSLOCTEXT("GameXXKStoryTaskDrawer", "AcceptTask", "接取任务");
		Entry.Continuation = GetContinuation(Task, TaskProgress);
		Entry.MaterialReward = Task.MaterialReward;
		Entry.AuthoredOrder = Task.AuthoredOrder;
		Entry.CompletedAtUtcTicks = TaskProgress ? TaskProgress->CompletedAtUtcTicks : 0;
		return Entry;
	}

	bool IsActionableBefore(
		const FGameXXKStoryTaskDrawerEntryView& Left,
		const FGameXXKStoryTaskDrawerEntryView& Right)
	{
		const int32 LeftStateOrder = Left.State == EGameXXKTaskState::Active ? 0 : 1;
		const int32 RightStateOrder = Right.State == EGameXXKTaskState::Active ? 0 : 1;
		if (LeftStateOrder != RightStateOrder)
		{
			return LeftStateOrder < RightStateOrder;
		}
		if (Left.AuthoredOrder != Right.AuthoredOrder)
		{
			return Left.AuthoredOrder < Right.AuthoredOrder;
		}
		return Left.TaskId.LexicalLess(Right.TaskId);
	}

	bool IsClaimableBefore(
		const FGameXXKStoryTaskDrawerEntryView& Left,
		const FGameXXKStoryTaskDrawerEntryView& Right)
	{
		if (Left.CompletedAtUtcTicks != Right.CompletedAtUtcTicks)
		{
			return Left.CompletedAtUtcTicks > Right.CompletedAtUtcTicks;
		}
		if (Left.AuthoredOrder != Right.AuthoredOrder)
		{
			return Left.AuthoredOrder < Right.AuthoredOrder;
		}
		return Left.TaskId.LexicalLess(Right.TaskId);
	}

	FName RestoreSelection(
		const TArray<FGameXXKStoryTaskDrawerEntryView>& Entries,
		const FName SavedSelection)
	{
		if (Entries.ContainsByPredicate([SavedSelection](const FGameXXKStoryTaskDrawerEntryView& Entry)
		{
			return Entry.TaskId == SavedSelection;
		}))
		{
			return SavedSelection;
		}
		return Entries.IsEmpty() ? NAME_None : Entries[0].TaskId;
	}

	FName RestoreActionableSelection(
		const TArray<FGameXXKStoryTaskDrawerEntryView>& Entries,
		const FName SavedSelection,
		const FName TrackedTaskId)
	{
		if (Entries.ContainsByPredicate([SavedSelection](const FGameXXKStoryTaskDrawerEntryView& Entry)
		{
			return Entry.TaskId == SavedSelection;
		}))
		{
			return SavedSelection;
		}

		const FGameXXKStoryTaskDrawerEntryView* TrackedActive = Entries.FindByPredicate(
			[TrackedTaskId](const FGameXXKStoryTaskDrawerEntryView& Entry)
			{
				return Entry.TaskId == TrackedTaskId && Entry.State == EGameXXKTaskState::Active;
			});
		if (TrackedActive)
		{
			return TrackedActive->TaskId;
		}

		const FGameXXKStoryTaskDrawerEntryView* FirstAvailable = Entries.FindByPredicate(
			[](const FGameXXKStoryTaskDrawerEntryView& Entry)
			{
				return Entry.State == EGameXXKTaskState::Available;
			});
		if (FirstAvailable)
		{
			return FirstAvailable->TaskId;
		}

		return Entries.IsEmpty() ? NAME_None : Entries[0].TaskId;
	}
}

FGameXXKStoryTaskDrawerSnapshot FGameXXKStoryTaskDrawerRules::BuildSnapshot(
	const TArray<FGameXXKTaskDefinition>& Tasks,
	const FGameXXKNarrativeProgress& Progress,
	const FGameXXKStoryTaskDrawerUiState& UiState)
{
	using namespace GameXXKStoryTaskDrawerRulesPrivate;

	FGameXXKStoryTaskDrawerSnapshot Snapshot;

	TMap<FName, int32> TaskIdCounts;
	for (const FGameXXKTaskDefinition& Task : Tasks)
	{
		if (!Task.TaskId.IsNone())
		{
			TaskIdCounts.FindOrAdd(Task.TaskId)++;
		}
	}

	for (const FGameXXKTaskDefinition& Task : Tasks)
	{
		if (!IsTaskDefinitionStructurallyValid(Task) || TaskIdCounts.FindRef(Task.TaskId) != 1)
		{
			continue;
		}

		const FGameXXKTaskProgress* TaskProgress = Progress.TaskProgressById.Find(Task.TaskId);
		if (TaskProgress && TaskProgress->State == EGameXXKTaskState::Completed && !TaskProgress->bRewardCommitted)
		{
			Snapshot.Claimable.Add(BuildEntry(Task, EGameXXKTaskState::Completed, TaskProgress));
			continue;
		}

		if (TaskProgress && TaskProgress->State == EGameXXKTaskState::Active)
		{
			Snapshot.Actionable.Add(BuildEntry(Task, EGameXXKTaskState::Active, TaskProgress));
			continue;
		}

		const bool bCanBeAccepted = (!TaskProgress
			|| TaskProgress->State == EGameXXKTaskState::Locked
			|| TaskProgress->State == EGameXXKTaskState::Available)
			&& FGameXXKStoryRules::IsTaskAvailable(Task, Progress);
		if (bCanBeAccepted)
		{
			Snapshot.Actionable.Add(BuildEntry(Task, EGameXXKTaskState::Available, TaskProgress));
		}
	}

	Snapshot.Actionable.Sort(IsActionableBefore);
	Snapshot.Claimable.Sort(IsClaimableBefore);
	Snapshot.SelectedActionableTaskId = RestoreActionableSelection(
		Snapshot.Actionable, UiState.SelectedActionableTaskId, Progress.TrackedTaskId);
	Snapshot.SelectedClaimableTaskId = RestoreSelection(
		Snapshot.Claimable, UiState.SelectedClaimableTaskId);
	Snapshot.bHasClaimableRedDot = !Snapshot.Claimable.IsEmpty();
	return Snapshot;
}
