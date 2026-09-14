#pragma once

#include "CoreMinimal.h"

namespace GameXXKTargetingPresentation
{
	inline FVector2D CurveControl(const FVector2D Start, const FVector2D End)
	{
		const float Distance = static_cast<float>((End - Start).Size());
		// Screen Y grows downward. A fixed upward bow is independent of team side.
		// Limit short arcs so near-vertical pointers do not overshoot and turn back.
		const float Bow = FMath::Min(FMath::Clamp(Distance * .22f, 24.0f, 180.0f), Distance * .35f);
		return (Start + End) * .5 + FVector2D(0, -Bow);
	}

	inline FVector2D ArrowSize() { return FVector2D(132.0f, 132.0f); }
	inline FVector2D ArrowTipHotspot(const FVector2D Size)
	{
		// Measured visible tip of the versioned 1254px gemstone sprite.
		return FVector2D(Size.X * (1148.5 / 1254.0), Size.Y * (625.0 / 1254.0));
	}

	struct FTrapezoidDash
	{
		FVector2D Start;
		FVector2D End;
		float BackHalfWidth;
		float FrontHalfWidth;
	};

	inline TArray<FTrapezoidDash> BuildTrapezoidTrail(const FVector2D Start, const FVector2D End)
	{
		TArray<FTrapezoidDash> Result;
		const float Distance = static_cast<float>((End - Start).Size());
		const float SourceGap = 20.0f;
		const float HeadGap = ArrowSize().X * .86f;
		if (Distance < 8.0f) return Result;
		const FVector2D Control = CurveControl(Start, End);
		constexpr int32 SampleCount = 48;
		FVector2D Samples[SampleCount + 1];
		float Lengths[SampleCount + 1] = {};
		Samples[0] = Start;
		for (int32 Index = 1; Index <= SampleCount; ++Index)
		{
			const float T = static_cast<float>(Index) / SampleCount;
			const float U = 1 - T;
			Samples[Index] = Start * (U * U) + Control * (2 * U * T) + End * (T * T);
			Lengths[Index] = Lengths[Index - 1] + static_cast<float>((Samples[Index] - Samples[Index - 1]).Size());
		}
		const float Available = Lengths[SampleCount] - SourceGap - HeadGap;
		if (Available < 36.0f) return Result;
		const int32 Count = FMath::Clamp(FMath::RoundToInt(Available / 140.0f), 1, 8);
		const float CellLength = Available / Count;
		const auto Point = [&](const float AlongCurve)
		{
			for (int32 Index = 1; Index <= SampleCount; ++Index)
			{
				if (Lengths[Index] >= AlongCurve)
				{
					const float T = (AlongCurve - Lengths[Index - 1]) / FMath::Max(Lengths[Index] - Lengths[Index - 1], .001f);
					return FMath::Lerp(Samples[Index - 1], Samples[Index], T);
				}
			}
			return End;
		};
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Progress = Count > 1 ? static_cast<float>(Index) / (Count - 1) : .5f;
			const float Length = FMath::Min(CellLength * .74f, FMath::Lerp(28.0f, 58.0f, Progress));
			const float Center = SourceGap + (Index + .5f) * CellLength;
			// The trail grows from the character toward the arrowhead.
			const float Width = FMath::Lerp(5.0f, 14.0f, Progress);
			Result.Add({Point(Center - Length * .5f), Point(Center + Length * .5f), Width * .72f, Width});
		}
		return Result;
	}
}
