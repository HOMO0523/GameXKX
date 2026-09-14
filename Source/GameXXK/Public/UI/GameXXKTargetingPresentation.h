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
}
