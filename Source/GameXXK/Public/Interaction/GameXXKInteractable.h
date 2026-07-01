#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameXXKInteractable.generated.h"

UINTERFACE(BlueprintType)
class GAMEXXK_API UGameXXKInteractable : public UInterface
{
	GENERATED_BODY()
};

class GAMEXXK_API IGameXXKInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GameXXK|Interaction")
	FText GetInteractionPrompt() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GameXXK|Interaction")
	void Interact(APawn* InstigatorPawn);
};
