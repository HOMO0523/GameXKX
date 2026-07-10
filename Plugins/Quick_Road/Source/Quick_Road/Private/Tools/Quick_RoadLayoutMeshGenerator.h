// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;
class USplineComponent;
class AActor;
class UMaterialInterface;
class UQuick_RoadLayoutMeshComponent;

struct FQuick_RoadLayoutMeshParams
{
	int32 Subdivisions = 20;
	float MaxSlopeAngleDegrees = 15.f;
	/** 0 = 趋近区域平均高度；1 = 尽量跟随地形采样（仍受坡度约束）。 */
	float TerrainBlendStrength = 0.85f;
	int32 MaxSlopeRelaxIterations = 128;
	float SlopeConvergenceThresholdCm = 0.5f;
	TObjectPtr<UMaterialInterface> DefaultMaterial = nullptr;
};

struct FQuick_RoadLayoutTerraceParams
{
	float StepSizeCm = 300.f;
	bool bAutoHeightRange = true;
	float MinHeightCm = 0.f;
	float MaxHeightCm = 10000.f;
	float Fade = 0.f;
	float SmoothEdges = 0.3f;
};

struct FQuick_RoadLayoutMeshResult
{
	TObjectPtr<AActor> LayoutActor = nullptr;
	int32 VertexCount = 0;
	int32 TriangleCount = 0;
};

class FQuick_RoadLayoutMeshGenerator
{
public:
	static bool FindCityScopeSpline(UWorld* World, USplineComponent*& OutSpline, FText& OutErrorMessage);

	static bool FindCityScopeLayoutMesh(
		UWorld* World,
		AActor*& OutActor,
		UQuick_RoadLayoutMeshComponent*& OutMeshComponent);

	static bool GenerateFromCityScope(
		UWorld* World,
		USplineComponent* Spline,
		const FQuick_RoadLayoutMeshParams& Params,
		FQuick_RoadLayoutMeshResult& OutResult,
		FText& OutErrorMessage);

	/** 对已有 CityScopeLayoutMesh 顶点做阶梯化，仅修改程序化网格，不触碰 Landscape。 */
	static bool ApplyTerraceToLayoutMesh(
		UWorld* World,
		const FQuick_RoadLayoutTerraceParams& Params,
		FQuick_RoadLayoutMeshResult& OutResult,
		FText& OutErrorMessage);
};
