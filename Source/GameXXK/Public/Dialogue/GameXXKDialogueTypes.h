#pragma once

#include "CoreMinimal.h"

#include "GameXXKDialogueTypes.generated.h"

UENUM(BlueprintType)
enum class EGameXXKDialogueNodeType : uint8
{
	Line,
	Choice,
	End
};

UENUM(BlueprintType)
enum class EGameXXKDialoguePresentation : uint8
{
	Bubble,
	DialoguePanel,
	None
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKDialogueOptionDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName OptionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName TextId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName OutcomeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName NextNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TMap<FName, FString> Conditions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FText DisabledReason;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKDialogueNodeDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	EGameXXKDialogueNodeType Type = EGameXXKDialogueNodeType::Line;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	EGameXXKDialoguePresentation Presentation = EGameXXKDialoguePresentation::DialoguePanel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName SpeakerId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName TextId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName EndOutcomeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName NextNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TArray<FGameXXKDialogueOptionDefinition> Options;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TMap<FName, FString> Conditions;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKDialogueHistoryEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Dialogue")
	FName SpeakerId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Dialogue")
	FName TextId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Dialogue")
	FText Text;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Dialogue")
	FName SelectedOptionId;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKDialogueSessionState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Dialogue")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Dialogue")
	FName StoryId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Dialogue")
	int32 StoryVersion = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Dialogue")
	FName TaskId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Dialogue")
	FName StepId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Dialogue")
	FName SequenceId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Dialogue")
	FName StageContractId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Dialogue")
	FName DialogueId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Dialogue")
	int32 DialogueVersion = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Dialogue")
	FName CurrentNodeId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Dialogue")
	TSet<FName> SeenNodeIds;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Dialogue")
	TArray<FName> SelectedOptionIds;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Dialogue")
	TArray<FGameXXKDialogueHistoryEntry> History;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Dialogue")
	FString PauseReason;
};

struct GAMEXXK_API FGameXXKDialogueStartContext
{
	FName StoryId;
	int32 StoryVersion = 1;
	FName TaskId;
	FName StepId;
	FName SequenceId;
	FName StageContractId;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKDialogueVisibleOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FName OptionId;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FText Text;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FText DisabledReason;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKDialogueOutput
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FName NodeId;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	EGameXXKDialoguePresentation Presentation = EGameXXKDialoguePresentation::None;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FName SpeakerId;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FName TextId;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FText Text;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	TArray<FGameXXKDialogueVisibleOption> Options;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	FName OutcomeId;

	UPROPERTY(BlueprintReadOnly, Category = "Dialogue")
	bool bEnded = false;
};
