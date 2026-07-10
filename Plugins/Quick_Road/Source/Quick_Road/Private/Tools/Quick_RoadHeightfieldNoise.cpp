// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/Quick_RoadHeightfieldNoise.h"

namespace Quick_RoadHeightfieldNoiseLocals
{
	FVector2D BuildSeedOffset(int32 Seed)
	{
		const float SeedF = static_cast<float>(Seed);
		return FVector2D(SeedF * 17.31f, SeedF * 43.87f);
	}

	FVector2D GridToNoiseSpace(float GridX, float GridY, const FQuick_RoadHeightfieldNoiseParams& Params)
	{
		const float SafeElementSize = FMath::Max(Params.ElementSize, 1.f);
		const FVector2D SeedOffset = BuildSeedOffset(Params.Seed);

		return FVector2D(
			(GridX + Params.Offset.X + SeedOffset.X) / SafeElementSize * Params.Scale.X,
			(GridY + Params.Offset.Y + SeedOffset.Y) / SafeElementSize * Params.Scale.Y);
	}

	float SamplePerlin01(const FVector2D& NoisePos)
	{
		return FMath::PerlinNoise2D(NoisePos);
	}

	float ShapeNoiseValue(float RawNoise, EQuick_RoadHeightfieldNoiseType NoiseType, bool bCenterNoise)
	{
		float Value = RawNoise;

		switch (NoiseType)
		{
		case EQuick_RoadHeightfieldNoiseType::Ridged:
			Value = 1.f - FMath::Abs(RawNoise);
			Value = Value * Value;
			Value = bCenterNoise ? (Value * 2.f - 1.f) : Value;
			break;
		case EQuick_RoadHeightfieldNoiseType::Billowy:
			Value = FMath::Abs(RawNoise);
			Value = bCenterNoise ? (Value * 2.f - 1.f) : Value;
			break;
		case EQuick_RoadHeightfieldNoiseType::Perlin:
		default:
			if (!bCenterNoise)
			{
				Value = (RawNoise + 1.f) * 0.5f;
			}
			break;
		}

		return Value;
	}

	float EvaluateBaseNoise(const FVector2D& NoisePos, const FQuick_RoadHeightfieldNoiseParams& Params)
	{
		const float RawNoise = SamplePerlin01(NoisePos);
		return ShapeNoiseValue(RawNoise, Params.NoiseType, Params.bCenterNoise);
	}

	float EvaluateStandardFractal(const FVector2D& NoisePos, const FQuick_RoadHeightfieldNoiseParams& Params)
	{
		const int32 Octaves = FMath::Clamp(Params.MaxOctaves, 1, 16);
		float Sum = 0.f;
		float Amplitude = 1.f;
		float Frequency = 1.f;
		float MaxAmplitude = 0.f;

		for (int32 OctaveIndex = 0; OctaveIndex < Octaves; ++OctaveIndex)
		{
			const FVector2D OctavePos(NoisePos.X * Frequency, NoisePos.Y * Frequency);
			Sum += EvaluateBaseNoise(OctavePos, Params) * Amplitude;
			MaxAmplitude += Amplitude;
			Amplitude *= Params.Roughness;
			Frequency *= Params.Lacunarity;
		}

		return MaxAmplitude > 0.f ? Sum / MaxAmplitude : 0.f;
	}

	float EvaluateTerrainFractal(const FVector2D& NoisePos, const FQuick_RoadHeightfieldNoiseParams& Params, bool bHybrid)
	{
		const int32 Octaves = FMath::Clamp(Params.MaxOctaves, 1, 16);
		float Sum = 0.f;
		float Amplitude = 1.f;
		float Frequency = 1.f;
		float Weight = 1.f;

		for (int32 OctaveIndex = 0; OctaveIndex < Octaves; ++OctaveIndex)
		{
			if (Weight <= 1e-4f)
			{
				break;
			}

			const FVector2D OctavePos(NoisePos.X * Frequency, NoisePos.Y * Frequency);
			float NoiseValue = SamplePerlin01(OctavePos);

			if (Params.NoiseType == EQuick_RoadHeightfieldNoiseType::Ridged)
			{
				NoiseValue = 1.f - FMath::Abs(NoiseValue);
				NoiseValue = NoiseValue * NoiseValue;
			}
			else if (Params.NoiseType == EQuick_RoadHeightfieldNoiseType::Billowy)
			{
				NoiseValue = FMath::Abs(NoiseValue);
			}
			else if (!Params.bCenterNoise)
			{
				NoiseValue = (NoiseValue + 1.f) * 0.5f;
			}

			const float Signal = NoiseValue * Weight;
			Sum += Signal * Amplitude;

			if (bHybrid)
			{
				Weight = FMath::Clamp(NoiseValue * Weight * 2.f, 0.f, 1.f);
			}
			else
			{
				Weight = FMath::Clamp(NoiseValue, 0.f, 1.f);
			}

			Amplitude *= Params.Roughness;
			Frequency *= Params.Lacunarity;
		}

		return Params.bCenterNoise ? Sum : FMath::Clamp(Sum, 0.f, 1.f);
	}

	float CombineHeights(float ExistingHeight, float NoiseDisplacement, EQuick_RoadHeightfieldCombineMode CombineMode)
	{
		switch (CombineMode)
		{
		case EQuick_RoadHeightfieldCombineMode::Replace:
			return NoiseDisplacement;
		case EQuick_RoadHeightfieldCombineMode::Multiply:
			return ExistingHeight * (1.f + NoiseDisplacement);
		case EQuick_RoadHeightfieldCombineMode::Maximum:
			return FMath::Max(ExistingHeight, ExistingHeight + NoiseDisplacement);
		case EQuick_RoadHeightfieldCombineMode::Minimum:
			return FMath::Min(ExistingHeight, ExistingHeight + NoiseDisplacement);
		case EQuick_RoadHeightfieldCombineMode::Add:
		default:
			return ExistingHeight + NoiseDisplacement;
		}
	}
}

float FQuick_RoadHeightfieldNoise::SampleNoiseAtGridPoint(float GridX, float GridY, const FQuick_RoadHeightfieldNoiseParams& Params)
{
	using namespace Quick_RoadHeightfieldNoiseLocals;

	const FVector2D NoisePos = GridToNoiseSpace(GridX, GridY, Params);
	float NoiseValue = 0.f;

	switch (Params.FractalType)
	{
	case EQuick_RoadHeightfieldFractalType::None:
		NoiseValue = EvaluateBaseNoise(NoisePos, Params);
		break;
	case EQuick_RoadHeightfieldFractalType::Standard:
		NoiseValue = EvaluateStandardFractal(NoisePos, Params);
		break;
	case EQuick_RoadHeightfieldFractalType::HybridTerrain:
		NoiseValue = EvaluateTerrainFractal(NoisePos, Params, true);
		break;
	case EQuick_RoadHeightfieldFractalType::Terrain:
	default:
		NoiseValue = EvaluateTerrainFractal(NoisePos, Params, false);
		break;
	}

	return NoiseValue * Params.Amplitude;
}

void FQuick_RoadHeightfieldNoise::ApplyNoise(
	TArray<float>& InOutHeights,
	int32 Width,
	int32 Height,
	const FQuick_RoadHeightfieldNoiseParams& Params)
{
	if (InOutHeights.Num() != Width * Height || Width < 1 || Height < 1 || FMath::IsNearlyZero(Params.Amplitude))
	{
		return;
	}

	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 Index = Y * Width + X;
			const float Displacement = SampleNoiseAtGridPoint(static_cast<float>(X), static_cast<float>(Y), Params);
			InOutHeights[Index] = Quick_RoadHeightfieldNoiseLocals::CombineHeights(
				InOutHeights[Index], Displacement, Params.CombineMode);
		}
	}
}
