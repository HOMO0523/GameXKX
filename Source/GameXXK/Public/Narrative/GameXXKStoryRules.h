#pragma once

#include "CoreMinimal.h"
#include "Narrative/GameXXKNarrativeTypes.h"

class GAMEXXK_API FGameXXKStoryRules final
{
public:
	static bool StartStory(
		const FGameXXKStoryDefinition& Story,
		FGameXXKNarrativeProgress& InOutProgress,
		FString* OutError = nullptr);
	static bool StartTask(
		const FGameXXKTaskDefinition& Task,
		FGameXXKNarrativeProgress& InOutProgress,
		FString* OutError = nullptr);
	static bool AdvanceTask(
		const FGameXXKTaskDefinition& Task,
		FName NextStepId,
		FGameXXKNarrativeProgress& InOutProgress,
		FString* OutError = nullptr);
	static bool CompleteTask(
		const FGameXXKTaskDefinition& Task,
		FGameXXKNarrativeProgress& InOutProgress,
		FString* OutError = nullptr);
	static bool CommitTaskReward(
		const FGameXXKTaskDefinition& Task,
		FGameXXKNarrativeProgress& InOutProgress,
		FString* OutError = nullptr);
	static bool TrackTask(
		FName TaskId,
		FGameXXKNarrativeProgress& InOutProgress,
		FString* OutError = nullptr);
};
