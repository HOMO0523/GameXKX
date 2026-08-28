#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Narrative/GameXXKNarrativeSequenceTypes.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "GameXXKNarrativeSequenceAsset.generated.h"

UCLASS(BlueprintType)
class GAMEXXK_API UGameXXKNarrativeSequenceAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName SequenceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative", meta = (ClampMin = "1"))
	int32 SequenceVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName StageContractId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName EntryStepId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	TMap<FName, FName> DefaultCharacterIdByRole;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	TArray<FGameXXKNarrativeSequenceStepDefinition> Steps;

	const FGameXXKNarrativeSequenceStepDefinition* FindStep(FName StepId) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
