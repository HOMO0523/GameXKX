#include "Narrative/GameXXKStoryRules.h"

namespace GameXXKStoryRulesPrivate
{
	bool SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	void ClearError(FString* OutError)
	{
		if (OutError)
		{
			OutError->Reset();
		}
	}

	const FGameXXKTaskStepDefinition* FindStep(
		const FGameXXKTaskDefinition& Task,
		const FName StepId)
	{
		return Task.Steps.FindByPredicate([StepId](const FGameXXKTaskStepDefinition& Step)
		{
			return Step.StepId == StepId;
		});
	}

	bool IsTaskDefinitionValid(const FGameXXKTaskDefinition& Task)
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
}

bool FGameXXKStoryRules::StartStory(
	const FGameXXKStoryDefinition& Story,
	FGameXXKNarrativeProgress& InOutProgress,
	FString* OutError)
{
	using namespace GameXXKStoryRulesPrivate;
	if (Story.StoryId.IsNone() || Story.Version <= 0)
	{
		return SetError(OutError, TEXT("Story definition is invalid."));
	}
	if (const FGameXXKStoryProgress* Existing = InOutProgress.StoryProgressById.Find(Story.StoryId))
	{
		if (Existing->State != EGameXXKStoryState::Inactive)
		{
			return SetError(OutError, TEXT("Story has already started."));
		}
	}
	FGameXXKStoryProgress Progress;
	Progress.Version = Story.Version;
	Progress.State = EGameXXKStoryState::Active;
	InOutProgress.StoryProgressById.Add(Story.StoryId, MoveTemp(Progress));
	ClearError(OutError);
	return true;
}

bool FGameXXKStoryRules::StartTask(
	const FGameXXKTaskDefinition& Task,
	FGameXXKNarrativeProgress& InOutProgress,
	FString* OutError)
{
	using namespace GameXXKStoryRulesPrivate;
	if (!IsTaskDefinitionValid(Task))
	{
		return SetError(OutError, TEXT("Task definition is invalid."));
	}
	FGameXXKStoryProgress* Story = InOutProgress.StoryProgressById.Find(Task.StoryId);
	if (!Story || Story->State != EGameXXKStoryState::Active)
	{
		return SetError(OutError, TEXT("Task story is not active."));
	}
	for (const FName PrerequisiteTaskId : Task.PrerequisiteTaskIds)
	{
		const FGameXXKTaskProgress* Prerequisite =
			InOutProgress.TaskProgressById.Find(PrerequisiteTaskId);
		if (!Prerequisite
			|| (Prerequisite->State != EGameXXKTaskState::Completed
				&& Prerequisite->State != EGameXXKTaskState::Rewarded))
		{
			return SetError(OutError, TEXT("Task prerequisites are incomplete."));
		}
	}
	if (const FGameXXKTaskProgress* Existing = InOutProgress.TaskProgressById.Find(Task.TaskId))
	{
		if (Existing->State != EGameXXKTaskState::Locked
			&& Existing->State != EGameXXKTaskState::Available)
		{
			return SetError(OutError, TEXT("Task has already started."));
		}
	}
	FGameXXKTaskProgress Progress;
	Progress.State = EGameXXKTaskState::Active;
	Progress.CurrentStepId = Task.EntryStepId;
	InOutProgress.TaskProgressById.Add(Task.TaskId, MoveTemp(Progress));
	Story->ActiveTaskIds.Add(Task.TaskId);
	ClearError(OutError);
	return true;
}

bool FGameXXKStoryRules::AdvanceTask(
	const FGameXXKTaskDefinition& Task,
	const FName NextStepId,
	FGameXXKNarrativeProgress& InOutProgress,
	FString* OutError)
{
	using namespace GameXXKStoryRulesPrivate;
	FGameXXKTaskProgress* Progress = InOutProgress.TaskProgressById.Find(Task.TaskId);
	if (!Progress || Progress->State != EGameXXKTaskState::Active)
	{
		return SetError(OutError, TEXT("Task is not active."));
	}
	const FGameXXKTaskStepDefinition* Current = FindStep(Task, Progress->CurrentStepId);
	if (!Current
		|| !Current->NextStepIds.Contains(NextStepId)
		|| !FindStep(Task, NextStepId))
	{
		return SetError(OutError, TEXT("Task step transition is not authored."));
	}
	Progress->CurrentStepId = NextStepId;
	ClearError(OutError);
	return true;
}

bool FGameXXKStoryRules::CompleteTask(
	const FGameXXKTaskDefinition& Task,
	FGameXXKNarrativeProgress& InOutProgress,
	FString* OutError)
{
	using namespace GameXXKStoryRulesPrivate;
	FGameXXKTaskProgress* Progress = InOutProgress.TaskProgressById.Find(Task.TaskId);
	FGameXXKStoryProgress* Story = InOutProgress.StoryProgressById.Find(Task.StoryId);
	const FGameXXKTaskStepDefinition* Current =
		Progress ? FindStep(Task, Progress->CurrentStepId) : nullptr;
	if (!Progress
		|| !Story
		|| Progress->State != EGameXXKTaskState::Active
		|| !Current
		|| !Current->NextStepIds.IsEmpty())
	{
		return SetError(OutError, TEXT("Only an active task at a terminal step can complete."));
	}
	Progress->State = EGameXXKTaskState::Completed;
	Story->ActiveTaskIds.Remove(Task.TaskId);
	Story->CompletedTaskIds.Add(Task.TaskId);
	ClearError(OutError);
	return true;
}

bool FGameXXKStoryRules::CommitTaskReward(
	const FGameXXKTaskDefinition& Task,
	FGameXXKNarrativeProgress& InOutProgress,
	FString* OutError)
{
	using namespace GameXXKStoryRulesPrivate;
	FGameXXKTaskProgress* Progress = InOutProgress.TaskProgressById.Find(Task.TaskId);
	if (!Progress || Progress->State != EGameXXKTaskState::Completed || Progress->bRewardCommitted)
	{
		return SetError(OutError, TEXT("Task reward is unavailable or already committed."));
	}
	Progress->bRewardCommitted = true;
	Progress->State = EGameXXKTaskState::Rewarded;
	ClearError(OutError);
	return true;
}

bool FGameXXKStoryRules::TrackTask(
	const FName TaskId,
	FGameXXKNarrativeProgress& InOutProgress,
	FString* OutError)
{
	using namespace GameXXKStoryRulesPrivate;
	const FGameXXKTaskProgress* Progress = InOutProgress.TaskProgressById.Find(TaskId);
	if (!Progress
		|| (Progress->State != EGameXXKTaskState::Active
			&& Progress->State != EGameXXKTaskState::Completed
			&& Progress->State != EGameXXKTaskState::Rewarded))
	{
		return SetError(OutError, TEXT("Only a started task can be tracked."));
	}
	InOutProgress.TrackedTaskId = TaskId;
	ClearError(OutError);
	return true;
}
