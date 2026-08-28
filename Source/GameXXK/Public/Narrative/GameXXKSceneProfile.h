#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "GameXXKSceneProfile.generated.h"

class UGameXXKCharacterCatalog;
class UGameXXKStageContract;

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKSceneSlotBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName SlotId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FTransform RelativeTransform;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKNpcScenePlacement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName CharacterId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName HomeSlotId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName InteractionAnchorSlotId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName PatrolRegionId;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKSceneTriggerRegion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName TriggerId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName AnchorSlotId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FVector Extent = FVector(100.0, 100.0, 100.0);
};

UCLASS(BlueprintType)
class GAMEXXK_API UGameXXKSceneProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName SceneProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName StageContractId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FSoftObjectPath MapPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName SceneRootTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	TArray<FGameXXKSceneSlotBinding> SlotBindings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	TArray<FGameXXKNpcScenePlacement> NpcPlacements;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	TArray<FGameXXKSceneTriggerRegion> TriggerRegions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative")
	FName SafeSlotId;

	const FGameXXKSceneSlotBinding* FindSlot(FName SlotId) const;
	bool ValidateAgainstContract(
		const UGameXXKStageContract& Contract,
		const UGameXXKCharacterCatalog* CharacterCatalog,
		FString* OutError = nullptr) const;
	bool ResolveWorldTransform(
		FName SlotId,
		const FTransform& SceneRootTransform,
		FTransform& OutWorldTransform,
		FString* OutError = nullptr) const;
};
