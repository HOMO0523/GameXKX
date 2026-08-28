#pragma once

#include "CoreMinimal.h"
#include "Narrative/GameXXKNarrativeTypes.h"

class GAMEXXK_API FGameXXKStoryCatalog final
{
public:
	static const TArray<FGameXXKStoryDefinition>& GetStories();
	static const TArray<FGameXXKTaskDefinition>& GetTasks();
	static const FGameXXKStoryDefinition* FindStory(FName StoryId);
	static const FGameXXKTaskDefinition* FindTask(FName TaskId);
	static bool Validate(FString* OutError = nullptr);
};
