#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "GameXXKStageContract.generated.h"

UCLASS(BlueprintType)
class GAMEXXK_API UGameXXKStageContract : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName StageContractId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	TSet<FName> RequiredSlotIds;

	bool RequiresSlot(FName SlotId) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
