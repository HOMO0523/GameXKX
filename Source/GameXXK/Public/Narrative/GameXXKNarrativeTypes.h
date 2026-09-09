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

UENUM(BlueprintType)
enum class EGameXXKMainStoryActivityPhase : uint8
{
	None,
	Dialogue,
	Choice,
	ReadyToTravel,
	AwaitingGate,
	ReadyToBattle,
	AwaitingBattle,
	Result
};

/** Save-authoritative activity and one-journey context for the authored task tree. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKMainStorySession
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, SaveGame) TSet<FName> SeenChapters;
	UPROPERTY(BlueprintReadWrite, SaveGame) TSet<FName> MainlineCompletedChapters;
	UPROPERTY(BlueprintReadWrite, SaveGame) TSet<FName> JourneyStartedNodes;
	UPROPERTY(BlueprintReadWrite, SaveGame) FName ActiveNodeId;
	UPROPERTY(BlueprintReadWrite, SaveGame) EGameXXKMainStoryActivityPhase Phase = EGameXXKMainStoryActivityPhase::None;
	UPROPERTY(BlueprintReadWrite, SaveGame) int32 LineIndex = 0;
	UPROPERTY(BlueprintReadWrite, SaveGame) FName JourneyNodeId;
	UPROPERTY(BlueprintReadWrite, SaveGame) FName JourneyChapterId;
	UPROPERTY(BlueprintReadWrite, SaveGame) FName JourneyStageId;
	UPROPERTY(BlueprintReadWrite, SaveGame) FGuid JourneyId;
	UPROPERTY(BlueprintReadWrite, SaveGame) int32 JourneySeed = 0;
	UPROPERTY(BlueprintReadWrite, SaveGame) TSet<int32> GateNodeIds;
	UPROPERTY(BlueprintReadWrite, SaveGame) int32 GateNodeId = INDEX_NONE;
	UPROPERTY(BlueprintReadWrite, SaveGame) bool bGateEntered = false;
	UPROPERTY(BlueprintReadWrite, SaveGame) bool bBattleStarted = false;
	UPROPERTY(BlueprintReadWrite, SaveGame) bool bBattleWon = false;
	UPROPERTY(BlueprintReadWrite, SaveGame) bool bShowJourneyTree = false;
	UPROPERTY(BlueprintReadWrite, SaveGame) int32 Revision = 0;
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

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	FGameXXKMainStorySession MainStory;
};
