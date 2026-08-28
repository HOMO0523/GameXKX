#pragma once

#include "CoreMinimal.h"

#include "GameXXKNarrativeTypes.generated.h"

UENUM(BlueprintType)
enum class EGameXXKStoryState : uint8
{
	Inactive,
	Active,
	Completed
};

UENUM(BlueprintType)
enum class EGameXXKTaskState : uint8
{
	Locked,
	Available,
	Active,
	Completed,
	Rewarded
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTaskStepDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName StepId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName SequenceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName EncounterId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName RouteId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName StageContractId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName GuideId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	TArray<FName> NextStepIds;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTaskDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName TaskId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName StoryId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	TArray<FName> PrerequisiteTaskIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName EntryStepId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	TArray<FGameXXKTaskStepDefinition> Steps;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKStoryDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName StoryId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	int32 Version = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	TArray<FName> TaskIds;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKStoryProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	int32 Version = 1;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	EGameXXKStoryState State = EGameXXKStoryState::Inactive;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	TSet<FName> ActiveTaskIds;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	TSet<FName> CompletedTaskIds;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTaskProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	EGameXXKTaskState State = EGameXXKTaskState::Locked;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	FName CurrentStepId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	TMap<FName, int32> ObjectiveCounts;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bRewardCommitted = false;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKNarrativeProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	TMap<FName, FGameXXKStoryProgress> StoryProgressById;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	TMap<FName, FGameXXKTaskProgress> TaskProgressById;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	FName TrackedTaskId;
};
