#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Quick_RoadAutomationLibrary.generated.h"

/** Editor-only, B1-map-guarded automation facade for repeatable QuickRoad infrastructure. */
UCLASS()
class QUICK_ROAD_API UQuick_RoadAutomationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Quick Road|Automation")
	static FString ResetRoadInfrastructure();

	UFUNCTION(BlueprintCallable, Category = "Quick Road|Automation")
	static FString EnsureLandscapeInfrastructure(
		const FString& ActorLabel,
		int32 QuadsPerSection,
		int32 SectionsPerComponent,
		FIntPoint ComponentCount,
		FVector DesiredGridCenterWorld,
		FRotator DesiredRotationWorld,
		FVector DesiredScale,
		FName EditLayerName,
		const FString& MaterialAssetPath,
		const FString& AbsoluteHeightmapPath);

	UFUNCTION(BlueprintCallable, Category = "Quick Road|Automation")
	static FString GenerateRoadNetwork(
		FName RoadTag,
		float RoadWidthCm,
		float InfluenceFalloffCm,
		float InfluenceBlendStrength,
		float RoadMeshSampleDistanceCm,
		int32 RoadMeshWidthSubdivisions,
		bool bAdaptiveCurvature,
		float CurvatureThresholdDeg,
		int32 MaxCurvatureSubdivisions,
		float IntersectionExtendLengthCm,
		float IntersectionDetectSampleCm,
		const FString& RoadMaterialPath);

	UFUNCTION(BlueprintCallable, Category = "Quick Road|Automation")
	static FString RebuildRoadIntersections(
		FName RoadTagExpression,
		float ExtendLengthCm,
		float DetectSampleCm,
		float GridCellSizeCm,
		bool bGridRebuild,
		float CornerRadiusScale,
		float RoadMeshSampleDistanceCm,
		int32 RoadMeshWidthSubdivisions,
		bool bAdaptiveCurvature,
		float CurvatureThresholdDeg,
		int32 MaxCurvatureSubdivisions,
		const FString& RoadMaterialPath);

	UFUNCTION(BlueprintCallable, Category = "Quick Road|Automation")
	static FString ClearAndApplyAllRoadInfluence(
		const FString& LandscapeLabel,
		FName EditLayerName,
		float InfluenceFalloffCm,
		float BlendStrength,
		float VerticalOffsetCm,
		int32 SmoothIterations,
		float SmoothStrength);

	UFUNCTION(BlueprintCallable, Category = "Quick Road|Automation")
	static FString ExtractRoadEdges(
		float MinPolylineLengthCm,
		float VertexWeldToleranceCm,
		bool bIncludeIntersectionPatches,
		bool bEnableSplineResample,
		float SplineResampleDistanceCm);

	UFUNCTION(BlueprintCallable, Category = "Quick Road|Automation")
	static FString BakeSingleRoadNetwork(
		const FString& SaveDirectory,
		float StraightSegmentSplitDistanceCm,
		bool bDeleteSourceProcMesh);

	UFUNCTION(BlueprintCallable, Category = "Quick Road|Automation")
	static FString AuditInfrastructure(
		const FString& LandscapeLabel,
		FName EditLayerName,
		FName RoadTagExpression);
};
