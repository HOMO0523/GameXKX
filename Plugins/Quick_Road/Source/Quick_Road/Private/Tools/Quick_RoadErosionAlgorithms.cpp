// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/Quick_RoadErosionAlgorithms.h"
#include "Quick_RoadLog.h"

namespace Quick_RoadErosionLocals
{
	float SampleBilinear(const TArray<float>& Heights, int32 Width, int32 Height, float X, float Y)
	{
		const float ClampedX = FMath::Clamp(X, 0.f, static_cast<float>(Width - 1));
		const float ClampedY = FMath::Clamp(Y, 0.f, static_cast<float>(Height - 1));

		const int32 X0 = FMath::FloorToInt(ClampedX);
		const int32 Y0 = FMath::FloorToInt(ClampedY);
		const int32 X1 = FMath::Min(X0 + 1, Width - 1);
		const int32 Y1 = FMath::Min(Y0 + 1, Height - 1);

		const float TX = ClampedX - static_cast<float>(X0);
		const float TY = ClampedY - static_cast<float>(Y0);

		const float H00 = Heights[Y0 * Width + X0];
		const float H10 = Heights[Y0 * Width + X1];
		const float H01 = Heights[Y1 * Width + X0];
		const float H11 = Heights[Y1 * Width + X1];

		const float HX0 = FMath::Lerp(H00, H10, TX);
		const float HX1 = FMath::Lerp(H01, H11, TX);
		return FMath::Lerp(HX0, HX1, TY);
	}

	void AddHeightAtCell(TArray<float>& Heights, int32 Width, int32 Height, int32 X, int32 Y, float Amount)
	{
		if (X < 0 || Y < 0 || X >= Width || Y >= Height)
		{
			return;
		}
		Heights[Y * Width + X] += Amount;
	}

	void AddHeightBilinear(TArray<float>& Heights, int32 Width, int32 Height, float X, float Y, float Amount)
	{
		const float ClampedX = FMath::Clamp(X, 0.f, static_cast<float>(Width - 1));
		const float ClampedY = FMath::Clamp(Y, 0.f, static_cast<float>(Height - 1));

		const int32 X0 = FMath::FloorToInt(ClampedX);
		const int32 Y0 = FMath::FloorToInt(ClampedY);
		const int32 X1 = FMath::Min(X0 + 1, Width - 1);
		const int32 Y1 = FMath::Min(Y0 + 1, Height - 1);

		const float TX = ClampedX - static_cast<float>(X0);
		const float TY = ClampedY - static_cast<float>(Y0);

		AddHeightAtCell(Heights, Width, Height, X0, Y0, Amount * (1.f - TX) * (1.f - TY));
		AddHeightAtCell(Heights, Width, Height, X1, Y0, Amount * TX * (1.f - TY));
		AddHeightAtCell(Heights, Width, Height, X0, Y1, Amount * (1.f - TX) * TY);
		AddHeightAtCell(Heights, Width, Height, X1, Y1, Amount * TX * TY);
	}

	void ReportProgress(const FQuick_RoadErosionProgressCallback& OnProgress, float ProgressStart, float ProgressEnd, float LocalT)
	{
		if (OnProgress)
		{
			OnProgress(FMath::Lerp(ProgressStart, ProgressEnd, FMath::Clamp(LocalT, 0.f, 1.f)));
		}
	}

	void BoxBlurPass(const TArray<float>& Source, TArray<float>& Dest, int32 Width, int32 Height, int32 Radius, bool bHorizontal)
	{
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				float Sum = 0.f;
				int32 Count = 0;

				if (bHorizontal)
				{
					for (int32 Offset = -Radius; Offset <= Radius; ++Offset)
					{
						const int32 SampleX = FMath::Clamp(X + Offset, 0, Width - 1);
						Sum += Source[Y * Width + SampleX];
						++Count;
					}
				}
				else
				{
					for (int32 Offset = -Radius; Offset <= Radius; ++Offset)
					{
						const int32 SampleY = FMath::Clamp(Y + Offset, 0, Height - 1);
						Sum += Source[SampleY * Width + X];
						++Count;
					}
				}

				Dest[Y * Width + X] = Sum / static_cast<float>(Count);
			}
		}
	}
}

void FQuick_RoadErosionAlgorithms::RunHydraulicParticle(
	TArray<float>& InOutHeights,
	int32 Width,
	int32 Height,
	const FQuick_RoadHydraulicErosionParams& Params,
	int32 Seed,
	const FQuick_RoadErosionProgressCallback& OnProgress,
	float ProgressStart,
	float ProgressEnd)
{
	if (InOutHeights.Num() != Width * Height || Width < 2 || Height < 2)
	{
		return;
	}

	FRandomStream Rng(Seed);
	const float AreaScale = FMath::Sqrt(static_cast<float>(Width * Height) / static_cast<float>(512 * 512));
	const int32 DropletCount = FMath::Max(500, FMath::RoundToInt(Params.DropletsPerIteration * AreaScale));

	for (int32 Iteration = 0; Iteration < Params.Iterations; ++Iteration)
	{
		for (int32 DropletIndex = 0; DropletIndex < DropletCount; ++DropletIndex)
		{
			float PosX = Rng.FRandRange(0.f, static_cast<float>(Width - 1));
			float PosY = Rng.FRandRange(0.f, static_cast<float>(Height - 1));
			float DirX = 0.f;
			float DirY = 0.f;
			float Speed = 1.f;
			float Water = 1.f;
			float Sediment = 0.f;

			for (int32 Step = 0; Step < Params.MaxDropletSteps; ++Step)
			{
				const int32 NodeX = FMath::Clamp(FMath::FloorToInt(PosX), 0, Width - 1);
				const int32 NodeY = FMath::Clamp(FMath::FloorToInt(PosY), 0, Height - 1);

				const float HeightHere = Quick_RoadErosionLocals::SampleBilinear(InOutHeights, Width, Height, PosX, PosY);
				const float HeightRight = Quick_RoadErosionLocals::SampleBilinear(InOutHeights, Width, Height, PosX + 1.f, PosY);
				const float HeightLeft = Quick_RoadErosionLocals::SampleBilinear(InOutHeights, Width, Height, PosX - 1.f, PosY);
				const float HeightDown = Quick_RoadErosionLocals::SampleBilinear(InOutHeights, Width, Height, PosX, PosY + 1.f);
				const float HeightUp = Quick_RoadErosionLocals::SampleBilinear(InOutHeights, Width, Height, PosX, PosY - 1.f);

				const float GradientX = HeightRight - HeightLeft;
				const float GradientY = HeightDown - HeightUp;

				DirX = DirX * Params.Inertia - GradientX * (1.f - Params.Inertia);
				DirY = DirY * Params.Inertia - GradientY * (1.f - Params.Inertia);

				const float DirLen = FMath::Sqrt(DirX * DirX + DirY * DirY);
				if (DirLen < 1e-5f)
				{
					break;
				}

				DirX /= DirLen;
				DirY /= DirLen;

				const float NewPosX = PosX + DirX;
				const float NewPosY = PosY + DirY;
				if (NewPosX < 0.f || NewPosY < 0.f || NewPosX >= Width - 1 || NewPosY >= Height - 1)
				{
					break;
				}

				const float NewHeight = Quick_RoadErosionLocals::SampleBilinear(InOutHeights, Width, Height, NewPosX, NewPosY);
				const float DeltaHeight = NewHeight - HeightHere;

				const float Capacity = FMath::Max(-DeltaHeight, 0.01f) * Speed * Water * Params.SedimentCapacity;

				if (Sediment > Capacity || DeltaHeight > 0.f)
				{
					const float DepositAmount = (DeltaHeight > 0.f)
						? FMath::Min(DeltaHeight, Sediment)
						: (Sediment - Capacity) * Params.DepositionRate;
					Sediment -= DepositAmount;
					Quick_RoadErosionLocals::AddHeightBilinear(InOutHeights, Width, Height, PosX, PosY, DepositAmount);
				}
				else
				{
					const float ErodeAmount = FMath::Min((Capacity - Sediment) * Params.ErosionRate, -DeltaHeight);
					Sediment += ErodeAmount;
					Quick_RoadErosionLocals::AddHeightBilinear(InOutHeights, Width, Height, PosX, PosY, -ErodeAmount);
				}

				Speed = FMath::Sqrt(FMath::Max(0.f, Speed * Speed + DeltaHeight * Params.Gravity));
				Water *= (1.f - Params.Evaporation);
				if (Water < 0.01f)
				{
					break;
				}

				PosX = NewPosX;
				PosY = NewPosY;
			}
		}

		Quick_RoadErosionLocals::ReportProgress(
			OnProgress,
			ProgressStart,
			ProgressEnd,
			static_cast<float>(Iteration + 1) / static_cast<float>(Params.Iterations));
	}
}

void FQuick_RoadErosionAlgorithms::RunThermalTalus(
	TArray<float>& InOutHeights,
	int32 Width,
	int32 Height,
	const FQuick_RoadThermalErosionParams& Params,
	const FQuick_RoadErosionProgressCallback& OnProgress,
	float ProgressStart,
	float ProgressEnd)
{
	if (InOutHeights.Num() != Width * Height || Width < 3 || Height < 3)
	{
		return;
	}

	TArray<float> Scratch;
	Scratch.SetNumUninitialized(InOutHeights.Num());

	static const int32 NeighborOffsetX[] = { -1, 1, 0, 0, -1, 1, -1, 1 };
	static const int32 NeighborOffsetY[] = { 0, 0, -1, 1, -1, -1, 1, 1 };

	for (int32 Iteration = 0; Iteration < Params.Iterations; ++Iteration)
	{
		FMemory::Memcpy(Scratch.GetData(), InOutHeights.GetData(), InOutHeights.Num() * sizeof(float));

		for (int32 Y = 1; Y < Height - 1; ++Y)
		{
			for (int32 X = 1; X < Width - 1; ++X)
			{
				const float HeightHere = InOutHeights[Y * Width + X];
				float MaxDiff = 0.f;
				int32 BestNeighborX = X;
				int32 BestNeighborY = Y;

				for (int32 NeighborIndex = 0; NeighborIndex < 8; ++NeighborIndex)
				{
					const int32 NeighborX = X + NeighborOffsetX[NeighborIndex];
					const int32 NeighborY = Y + NeighborOffsetY[NeighborIndex];
					const float Diff = HeightHere - InOutHeights[NeighborY * Width + NeighborX];
					if (Diff > MaxDiff)
					{
						MaxDiff = Diff;
						BestNeighborX = NeighborX;
						BestNeighborY = NeighborY;
					}
				}

				if (MaxDiff > Params.TalusThreshold)
				{
					const float MoveAmount = FMath::Min((MaxDiff - Params.TalusThreshold) * Params.Strength, MaxDiff * 0.5f);
					Scratch[Y * Width + X] -= MoveAmount;
					Scratch[BestNeighborY * Width + BestNeighborX] += MoveAmount;
				}
			}
		}

		InOutHeights = MoveTemp(Scratch);
		Scratch.SetNumUninitialized(InOutHeights.Num());

		Quick_RoadErosionLocals::ReportProgress(
			OnProgress,
			ProgressStart,
			ProgressEnd,
			static_cast<float>(Iteration + 1) / static_cast<float>(Params.Iterations));
	}
}

void FQuick_RoadErosionAlgorithms::RunCombined(
	TArray<float>& InOutHeights,
	int32 Width,
	int32 Height,
	const FQuick_RoadErosionJobSettings& Settings,
	const FQuick_RoadErosionProgressCallback& OnProgress)
{
	RunThermalTalus(InOutHeights, Width, Height, Settings.Thermal, OnProgress, 0.f, 0.5f);
	RunHydraulicParticle(InOutHeights, Width, Height, Settings.Hydraulic, Settings.Seed, OnProgress, 0.5f, 1.f);
}

void FQuick_RoadErosionAlgorithms::ApplyMacroSmooth(
	TArray<float>& InOutHeights,
	int32 Width,
	int32 Height,
	int32 Passes,
	int32 Radius)
{
	if (Passes <= 0 || InOutHeights.Num() != Width * Height || Width < 3 || Height < 3)
	{
		return;
	}

	const int32 ClampedRadius = FMath::Clamp(Radius, 1, 8);
	TArray<float> Scratch;
	Scratch.SetNumUninitialized(InOutHeights.Num());

	for (int32 PassIndex = 0; PassIndex < Passes; ++PassIndex)
	{
		Quick_RoadErosionLocals::BoxBlurPass(InOutHeights, Scratch, Width, Height, ClampedRadius, true);
		Quick_RoadErosionLocals::BoxBlurPass(Scratch, InOutHeights, Width, Height, ClampedRadius, false);
	}
}

void FQuick_RoadErosionAlgorithms::ApplyPeakSharpen(
	TArray<float>& InOutHeights,
	int32 Width,
	int32 Height,
	float Strength,
	int32 Passes)
{
	if (Strength <= 0.f || Passes <= 0 || InOutHeights.Num() != Width * Height || Width < 3 || Height < 3)
	{
		return;
	}

	const float ClampedStrength = FMath::Clamp(Strength, 0.f, 2.f);
	const float LaplacianScale = ClampedStrength * 0.125f;

	TArray<float> Scratch;
	Scratch.SetNumUninitialized(InOutHeights.Num());

	for (int32 PassIndex = 0; PassIndex < Passes; ++PassIndex)
	{
		for (int32 Y = 1; Y < Height - 1; ++Y)
		{
			for (int32 X = 1; X < Width - 1; ++X)
			{
				const int32 Index = Y * Width + X;
				const float HeightHere = InOutHeights[Index];
				const float HeightLeft = InOutHeights[Y * Width + (X - 1)];
				const float HeightRight = InOutHeights[Y * Width + (X + 1)];
				const float HeightUp = InOutHeights[(Y - 1) * Width + X];
				const float HeightDown = InOutHeights[(Y + 1) * Width + X];

				const float Laplacian = 4.f * HeightHere - HeightLeft - HeightRight - HeightUp - HeightDown;
				Scratch[Index] = HeightHere + Laplacian * LaplacianScale;
			}
		}

		for (int32 Y = 0; Y < Height; ++Y)
		{
			Scratch[Y * Width] = InOutHeights[Y * Width];
			Scratch[Y * Width + (Width - 1)] = InOutHeights[Y * Width + (Width - 1)];
		}
		for (int32 X = 0; X < Width; ++X)
		{
			Scratch[X] = InOutHeights[X];
			Scratch[(Height - 1) * Width + X] = InOutHeights[(Height - 1) * Width + X];
		}

		InOutHeights = MoveTemp(Scratch);
		Scratch.SetNumUninitialized(InOutHeights.Num());
	}
}
