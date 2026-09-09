#include "Narrative/GameXXKStoryCatalog.h"
#include "Narrative/GameXXKMainStoryCatalog.h"

namespace GameXXKStoryCatalogPrivate
{
	bool SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	const TArray<FGameXXKTaskDefinition>& BuildTasks()
	{
		static const TArray<FGameXXKTaskDefinition> Tasks = []
		{
			TArray<FGameXXKTaskDefinition> Result;
			for (const auto& Node : FGameXXKMainStoryCatalog::Nodes())
			{
				FGameXXKTaskDefinition Task;
				Task.TaskId=Node.Id; Task.StoryId=Node.StoryId; Task.EntryStepId=Node.StepId;
				Task.PrerequisiteTaskIds=Node.RequiresAll;
				FGameXXKTaskStepDefinition Step; Step.StepId=Node.StepId;
				Task.Steps.Add(MoveTemp(Step)); Result.Add(MoveTemp(Task));
			}
			return Result;
		}();
		return Tasks;
	}

	const TArray<FGameXXKStoryDefinition>& BuildStories()
	{
		static const TArray<FGameXXKStoryDefinition> Stories = []
		{
			TArray<FGameXXKStoryDefinition> Result;
			for (const auto& Chapter : FGameXXKMainStoryCatalog::Chapters())
			{
				FGameXXKStoryDefinition Story; Story.StoryId=Chapter.StoryId; Story.Version=1; Story.TaskIds=Chapter.Nodes;
				Result.Add(MoveTemp(Story));
			}
			return Result;
		}();
		return Stories;
	}
}

const TArray<FGameXXKStoryDefinition>& FGameXXKStoryCatalog::GetStories()
{
	return GameXXKStoryCatalogPrivate::BuildStories();
}

const TArray<FGameXXKTaskDefinition>& FGameXXKStoryCatalog::GetTasks()
{
	return GameXXKStoryCatalogPrivate::BuildTasks();
}

const FGameXXKStoryDefinition* FGameXXKStoryCatalog::FindStory(const FName StoryId)
{
	return GetStories().FindByPredicate([StoryId](const FGameXXKStoryDefinition& Story)
	{
		return Story.StoryId == StoryId;
	});
}

const FGameXXKTaskDefinition* FGameXXKStoryCatalog::FindTask(const FName TaskId)
{
	return GetTasks().FindByPredicate([TaskId](const FGameXXKTaskDefinition& Task)
	{
		return Task.TaskId == TaskId;
	});
}

bool FGameXXKStoryCatalog::Validate(FString* OutError)
{
	using namespace GameXXKStoryCatalogPrivate;
	TSet<FName> StoryIds;
	TSet<FName> TaskIds;
	for (const FGameXXKStoryDefinition& Story : GetStories())
	{
		if (Story.StoryId.IsNone() || Story.Version <= 0 || StoryIds.Contains(Story.StoryId))
		{
			return SetError(OutError, TEXT("Story IDs must be unique and versions positive."));
		}
		StoryIds.Add(Story.StoryId);
	}
	for (const FGameXXKTaskDefinition& Task : GetTasks())
	{
		if (Task.TaskId.IsNone()
			|| Task.StoryId.IsNone()
			|| Task.EntryStepId.IsNone()
			|| TaskIds.Contains(Task.TaskId)
			|| !StoryIds.Contains(Task.StoryId))
		{
			return SetError(OutError, TEXT("Task identity or story ownership is invalid."));
		}
		TaskIds.Add(Task.TaskId);
		TSet<FName> StepIds;
		for (const FGameXXKTaskStepDefinition& Step : Task.Steps)
		{
			if (Step.StepId.IsNone() || StepIds.Contains(Step.StepId))
			{
				return SetError(OutError, TEXT("Task step IDs must be non-empty and unique."));
			}
			StepIds.Add(Step.StepId);
		}
		if (!StepIds.Contains(Task.EntryStepId))
		{
			return SetError(OutError, TEXT("Task entry step must resolve."));
		}
		for (const FGameXXKTaskStepDefinition& Step : Task.Steps)
		{
			for (const FName NextStepId : Step.NextStepIds)
			{
				if (NextStepId.IsNone() || !StepIds.Contains(NextStepId))
				{
					return SetError(OutError, TEXT("Task next step must resolve."));
				}
			}
		}
	}
	for (const FGameXXKStoryDefinition& Story : GetStories())
	{
		for (const FName TaskId : Story.TaskIds)
		{
			const FGameXXKTaskDefinition* Task = FindTask(TaskId);
			if (!Task || Task->StoryId != Story.StoryId)
			{
				return SetError(OutError, TEXT("Story task membership is invalid."));
			}
		}
	}
	if (OutError)
	{
		OutError->Reset();
	}
	return true;
}
