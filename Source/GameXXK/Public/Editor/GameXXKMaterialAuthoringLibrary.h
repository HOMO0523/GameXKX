#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameXXKMaterialAuthoringLibrary.generated.h"

class UMaterial;

UCLASS()
class GAMEXXK_API UGameXXKMaterialAuthoringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Clears UE's inherited opaque optimization after an authored OpacityMask is connected. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Editor|Materials")
	static bool ForceMaskedMaterialCompilation(UMaterial* Material);
};
