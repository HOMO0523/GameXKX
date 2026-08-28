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
