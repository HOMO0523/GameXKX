#pragma once

#include "CoreMinimal.h"

namespace GameXXKDesktopTrainingLayout
{
	struct GAMEXXK_API FFitTransform
	{
		float Scale = 1.0f;
		FVector2D Offset = FVector2D::ZeroVector;

		FVector2D ApplyPoint(const FVector2D& Point) const;
		FVector2D ApplySize(const FVector2D& Size) const;
		FVector4 ApplyRect(const FVector4& Rect) const;
	};

	GAMEXXK_API FVector2D GetReferenceCanvasSize();
	GAMEXXK_API FVector4 GetWarehouseRect();
	GAMEXXK_API FVector4 GetCenterShellRect();
	GAMEXXK_API FVector4 GetRightShellRect();
	GAMEXXK_API FVector4 GetIdleStripRect();
	GAMEXXK_API FVector4 GetContentRect();
	GAMEXXK_API FVector4 GetNavigationRect();
	GAMEXXK_API FFitTransform MakeFitTransform(const FVector2D& ViewportSize);
}
