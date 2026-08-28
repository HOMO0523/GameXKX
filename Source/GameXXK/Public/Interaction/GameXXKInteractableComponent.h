#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "GameXXKInteractableComponent.generated.h"

class USceneComponent;

UCLASS(ClassGroup = (GameXXK), meta = (BlueprintSpawnableComponent))
class GAMEXXK_API UGameXXKInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGameXXKInteractableComponent();

	void Configure(
		FName InInteractionId,
		FText InDisplayName,
		FName InNarrativeSequenceId,
		int32 InPriority,
		USceneComponent* InPromptAnchor);
	void ConfigureForCharacterId(
		FName CharacterId,
		USceneComponent* InPromptAnchor,
		int32 InPriority = 0);

	FName GetInteractionId() const;
	FText GetDisplayName() const;
	FName GetNarrativeSequenceId() const;
	int32 GetPriority() const;
	USceneComponent* GetPromptAnchor() const;
	bool IsInteractionEnabled() const;
	void SetInteractionEnabled(bool bEnabled);

private:
	UPROPERTY(EditAnywhere, Category = "Interaction")
	FName InteractionId;

	UPROPERTY(EditAnywhere, Category = "Interaction")
	FText DisplayName;

	UPROPERTY(EditAnywhere, Category = "Interaction")
	FName NarrativeSequenceId;

	UPROPERTY(EditAnywhere, Category = "Interaction")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, Category = "Interaction")
	bool bInteractionEnabled = false;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> PromptAnchor;
};
