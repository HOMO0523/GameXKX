#pragma once

#include "CoreMinimal.h"
#include "Dialogue/GameXXKDialogueTypes.h"
#include "Engine/DataAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "GameXXKDialogueAsset.generated.h"

UCLASS(BlueprintType)
class GAMEXXK_API UGameXXKDialogueAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName DialogueId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue", meta = (ClampMin = "1"))
	int32 DialogueVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FName EntryNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	TArray<FGameXXKDialogueNodeDefinition> Nodes;

	const FGameXXKDialogueNodeDefinition* FindNode(FName NodeId) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
