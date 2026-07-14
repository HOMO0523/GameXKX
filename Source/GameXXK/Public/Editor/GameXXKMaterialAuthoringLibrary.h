#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameXXKMaterialAuthoringLibrary.generated.h"

class UMaterial;
class UMaterialExpressionCollectionParameter;
class UMaterialParameterCollection;

UCLASS()
class GAMEXXK_API UGameXXKMaterialAuthoringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Clears UE's inherited opaque optimization after an authored OpacityMask is connected. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Editor|Materials")
	static bool ForceMaskedMaterialCompilation(UMaterial* Material);

	/**
	 * Binds a collection-expression node to both the public parameter name and
	 * UE's internal collection GUID.  Authoring through Python must set both.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Editor|Materials")
	static bool BindCollectionParameter(
		UMaterialExpressionCollectionParameter* Expression,
		UMaterialParameterCollection* Collection,
		FName ParameterName);
};
