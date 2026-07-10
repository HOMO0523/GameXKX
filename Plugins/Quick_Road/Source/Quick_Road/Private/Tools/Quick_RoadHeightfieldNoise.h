// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EQuick_RoadHeightfieldNoiseType : uint8
{
	Perlin,
	Ridged,
	Billowy
};

enum class EQuick_RoadHeightfieldFractalType : uint8
{
	None,
	Standard,
	Terrain,
	HybridTerrain
};

enum class EQuick_RoadHeightfieldCombineMode : uint8
{
	Add,
	Replace,
	Multiply,
	Maximum,
	Minimum
};

struct FQuick_RoadHeightfieldNoiseParams
{
	float Amplitude = 30.f;
	float ElementSize = 512.f;
	FVector2D Offset = FVector2D::ZeroVector;
	FVector2D Scale = FVector2D(1.f, 1.f);
	bool bCenterNoise = true;
	EQuick_RoadHeightfieldNoiseType NoiseType = EQuick_RoadHeightfieldNoiseType::Perlin;
	EQuick_RoadHeightfieldFractalType FractalType = EQuick_RoadHeightfieldFractalType::Terrain;
	int32 MaxOctaves = 4;
	float Lacunarity = 2.f;
	float Roughness = 0.5f;
	EQuick_RoadHeightfieldCombineMode CombineMode = EQuick_RoadHeightfieldCombineMode::Add;
	int32 Seed = 12345;
};

class FQuick_RoadHeightfieldNoise
{
public:
	static void ApplyNoise(TArray<float>& InOutHeights, int32 Width, int32 Height, const FQuick_RoadHeightfieldNoiseParams& Params);

	static float SampleNoiseAtGridPoint(float GridX, float GridY, const FQuick_RoadHeightfieldNoiseParams& Params);
};
