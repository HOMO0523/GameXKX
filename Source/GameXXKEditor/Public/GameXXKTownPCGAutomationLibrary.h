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
	static FString CreateOrUpdateTaggedSpline(
		const FString& ActorLabel,
		const TArray<FVector>& WorldPoints,
		bool bClosedLoop,
		const TArray<FName>& Tags);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town PCG")
	static FString CreateOrUpdateTownPCGGraph(
		const FString& GraphAssetPath,
		const FString& StaticMeshPath,
		const TArray<FTransform>& BuildingTransforms);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town PCG")
	static FString CreateOrUpdateTownPCGGraphAdvanced(
		const FString& GraphAssetPath,
		const FString& StaticMeshPath,
		const TArray<FTransform>& PointTransforms,
		int32 BaseSeed,
		const TArray<FString>& MaterialOverridePaths,
		const TArray<FString>& ConsumedRoadEdgeActorLabels,
		const FString& ConsumedRoadEdgeGeometryDigest,
		float MinimumRoadClearanceCm);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town PCG")
	static FString AttachTownPCGGraph(
		const FString& ActorLabel,
		const FString& GraphAssetPath,
		const FVector& Center,
		const FVector& Extent);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town PCG")
	static FString AttachTownPCGGraphAdvanced(
		const FString& ActorLabel,
		const FString& GraphAssetPath,
		const FVector& Center,
		const FVector& Extent,
		int32 ComponentSeed);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town PCG")
	static FString GenerateTownPCG(const FString& ActorLabel);

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town PCG")
	static FString GetTownPCGStatus(const FString& ActorLabel);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town PCG")
	static FString ClearTownPCG(const FString& ActorLabel);
};
