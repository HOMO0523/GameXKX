#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "GameXXKInteractionComponent.generated.h"

UCLASS(ClassGroup = (GameXXK), meta = (BlueprintSpawnableComponent))
class GAMEXXK_API UGameXXKInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGameXXKInteractionComponent();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Interaction")
	AActor* GetFocusedActor() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Interaction")
	FKey GetInteractionKey() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Interaction")
	void Interact();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Interaction")
	void SetFocusedActor(AActor* Actor);

	void SetFocusedActorForTest(AActor* Actor);

private:
	UPROPERTY()
	TObjectPtr<AActor> FocusedActor;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Interaction")
	FKey InteractionKey;
};
