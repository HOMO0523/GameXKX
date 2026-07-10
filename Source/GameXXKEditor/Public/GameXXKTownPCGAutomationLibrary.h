#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameXXKTownPCGAutomationLibrary.generated.h"

UCLASS()
class GAMEXXKEDITOR_API UGameXXKTownPCGAutomationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town PCG")
	static FString CreateOrUpdateTownPCGGraph(
		const FString& GraphAssetPath,
		const FString& StaticMeshPath,
		const TArray<FTransform>& BuildingTransforms);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town PCG")
	static FString AttachTownPCGGraph(
		const FString& ActorLabel,
		const FString& GraphAssetPath,
		const FVector& Center,
		const FVector& Extent);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town PCG")
	static FString GenerateTownPCG(const FString& ActorLabel);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town PCG")
	static FString ClearTownPCG(const FString& ActorLabel);
};
