#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "GameXXKGuideAsset.generated.h"

UENUM(BlueprintType)
enum class EGameXXKGuideInputPolicy : uint8
{
	Soft,
	Forced
};

UENUM(BlueprintType)
enum class EGameXXKGuideMissingTargetPolicy : uint8
{
	SkipStep,
	AbortGuide
};

UENUM(BlueprintType)
enum class EGameXXKGuidePreference : uint8
{
	Unset,
	NewPlayer,
	ExperiencedPlayer
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKGuideStepDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide")
	FName StepId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide")
	FName TriggerEventId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide")
	FName TargetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide")
	TArray<FName> AdditionalTargetIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide")
	FName BubbleAnchorTargetId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide")
	EGameXXKGuideMissingTargetPolicy MissingTargetPolicy =
		EGameXXKGuideMissingTargetPolicy::SkipStep;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide")
	EGameXXKGuideInputPolicy InputPolicy = EGameXXKGuideInputPolicy::Soft;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide")
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide")
	TSet<FName> AllowedActionIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide")
	FName CompletionEventId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide")
	FName NextStepId;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKGuideProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Guide")
	EGameXXKGuidePreference Preference = EGameXXKGuidePreference::Unset;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Guide")
	FName ActiveGuideId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Guide")
	FName ActiveGuideStepId;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Guide")
	TSet<FName> CompletedGuideStepIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Guide")
	FString LastDiagnostic;
};

UCLASS(BlueprintType)
class GAMEXXK_API UGameXXKGuideAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide")
	FName GuideId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide", meta = (ClampMin = "1"))
	int32 GuideVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide")
	FName EntryStepId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guide")
	TArray<FGameXXKGuideStepDefinition> Steps;

	const FGameXXKGuideStepDefinition* FindStep(FName StepId) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
