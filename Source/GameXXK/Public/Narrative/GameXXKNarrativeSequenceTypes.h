#pragma once

#include "CoreMinimal.h"

#include "GameXXKNarrativeSequenceTypes.generated.h"

UENUM(BlueprintType)
enum class EGameXXKNarrativeStepType : uint8
{
	Command,
	Wait,
	Dialogue,
	BranchOnOutcome,
	End
};

UENUM(BlueprintType)
enum class EGameXXKNarrativeRequestType : uint8
{
	None,
	Command,
	Wait,
	Dialogue,
	Ended
};

UENUM(BlueprintType)
enum class EGameXXKNarrativeCommandStatus : uint8
{
	Completed,
	Pending,
	Failed
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKNarrativeCommandDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName CommandId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName CommandType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	TMap<FName, FString> Arguments;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	bool bOptional = false;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKNarrativeSequenceStepDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName StepId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	EGameXXKNarrativeStepType Type = EGameXXKNarrativeStepType::End;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FGameXXKNarrativeCommandDefinition Command;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName WaitType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	TMap<FName, FString> WaitArguments;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName DialogueId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	TMap<FName, FName> OutcomeToStepId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName NextStepId;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKNarrativeSequenceSessionState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	FName StoryId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	int32 StoryVersion = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	FName TaskId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	FName StepId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	FName SequenceId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	int32 SequenceVersion = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	FName StageContractId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	FName CurrentSequenceStepId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	FName AwaitedDialogueId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	FName LastOutcomeId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	TSet<FName> ExecutedCommandKeys;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	TMap<FName, FName> CharacterIdByRole;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Narrative")
	FString PauseReason;
};

struct GAMEXXK_API FGameXXKNarrativeStartContext
{
	FName StoryId;
	int32 StoryVersion = 1;
	FName TaskId;
	FName StepId;
	FName StageContractId;
	TMap<FName, FName> CharacterIdByRole;
};

struct GAMEXXK_API FGameXXKNarrativeRequest
{
	EGameXXKNarrativeRequestType Type = EGameXXKNarrativeRequestType::None;
	FName SequenceStepId;
	FGameXXKNarrativeCommandDefinition Command;
	FName WaitType;
	TMap<FName, FString> WaitArguments;
	FName DialogueId;
	bool bEnded = false;
};
