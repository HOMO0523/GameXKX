// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FQuick_RoadHydraulicErosionParams
{
	int32 Iterations = 20;
	int32 DropletsPerIteration = 500;
	float Inertia = 0.6f;
	float ErosionRate = 0.5f;
	float DepositionRate = 0.25f;
	float SedimentCapacity = 8.0f;
	float Evaporation = 0.01f;
	float Gravity = 4.0f;
	int32 MaxDropletSteps = 66;
};

struct FQuick_RoadThermalErosionParams
{
	int32 Iterations = 15;
	float TalusThreshold = 1.0f;
	float Strength = 0.35f;
};

enum class EQuick_RoadErosionAlgorithmType : uint8
{
	HydraulicParticle,
	ThermalTalus,
	Combined
};

struct FQuick_RoadErosionJobSettings
{
	EQuick_RoadErosionAlgorithmType Algorithm = EQuick_RoadErosionAlgorithmType::Combined;
	int32 Seed = 12345;
	FQuick_RoadHydraulicErosionParams Hydraulic;
	FQuick_RoadThermalErosionParams Thermal;
	/** 侵蚀后宏观平滑次数，用于削弱细碎噪点、保留大沟谷走向。0 表示关闭。 */
	int32 MacroSmoothPasses = 1;
	/** 峰顶锐化强度：抬高凸处（山头/山脊）、加深凹处（沟谷），增强轮廓棱角。0 关闭。 */
	float PeakSharpenStrength = 0.6288f;
	int32 PeakSharpenPasses = 1;
};

using FQuick_RoadErosionProgressCallback = TFunction<void(float NormalizedProgress)>;

class FQuick_RoadErosionAlgorithms
{
public:
	static void RunHydraulicParticle(
		TArray<float>& InOutHeights,
		int32 Width,
		int32 Height,
		const FQuick_RoadHydraulicErosionParams& Params,
		int32 Seed,
		const FQuick_RoadErosionProgressCallback& OnProgress,
		float ProgressStart = 0.f,
		float ProgressEnd = 1.f);

	static void RunThermalTalus(
		TArray<float>& InOutHeights,
		int32 Width,
		int32 Height,
		const FQuick_RoadThermalErosionParams& Params,
		const FQuick_RoadErosionProgressCallback& OnProgress,
		float ProgressStart = 0.f,
		float ProgressEnd = 1.f);

	static void RunCombined(
		TArray<float>& InOutHeights,
		int32 Width,
		int32 Height,
		const FQuick_RoadErosionJobSettings& Settings,
		const FQuick_RoadErosionProgressCallback& OnProgress);

	static void ApplyMacroSmooth(
		TArray<float>& InOutHeights,
		int32 Width,
		int32 Height,
		int32 Passes,
		int32 Radius = 2);

	static void ApplyPeakSharpen(
		TArray<float>& InOutHeights,
		int32 Width,
		int32 Height,
		float Strength,
		int32 Passes);
};
