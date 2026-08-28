#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "GameXXKCharacterCatalog.generated.h"

class AActor;

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCharacterDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName CharacterId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FSoftObjectPath PortraitPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FSoftObjectPath AnimationLibraryPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	TSet<FName> SupportedActionIds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName DefaultInteractionSequenceId;
};

UCLASS(BlueprintType)
class GAMEXXK_API UGameXXKCharacterCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	TArray<FGameXXKCharacterDefinition> Characters;

	const FGameXXKCharacterDefinition* FindCharacter(FName CharacterId) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
