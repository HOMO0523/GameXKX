#include "UI/GameXXKDesktopTrainingLayout.h"

namespace GameXXKDesktopTrainingLayout
{
	namespace
	{
		const FVector2D ReferenceCanvasSize(1672.0f, 941.0f);
		const FVector4 WarehouseRect(10.0f, 17.0f, 363.0f, 908.0f);
		const FVector4 CenterShellRect(386.0f, 17.0f, 970.0f, 908.0f);
		const FVector4 RightShellRect(1369.0f, 17.0f, 291.0f, 908.0f);
		const FVector4 IdleStripRect(394.0f, 21.0f, 953.0f, 202.0f);
		const FVector4 ContentRect(397.0f, 244.0f, 945.0f, 533.0f);
		const FVector4 NavigationRect(397.0f, 788.0f, 945.0f, 137.0f);
	}

	FVector2D FFitTransform::ApplyPoint(const FVector2D& Point) const
	{
		return Offset + Point * Scale;
	}

	FVector2D FFitTransform::ApplySize(const FVector2D& Size) const
	{
		return Size * Scale;
	}

	FVector4 FFitTransform::ApplyRect(const FVector4& Rect) const
	{
		const FVector2D Point = ApplyPoint(FVector2D(Rect.X, Rect.Y));
		const FVector2D Size = ApplySize(FVector2D(Rect.Z, Rect.W));
		return FVector4(Point.X, Point.Y, Size.X, Size.Y);
	}

	FVector2D GetReferenceCanvasSize()
	{
		return ReferenceCanvasSize;
	}

	FVector4 GetWarehouseRect()
	{
		return WarehouseRect;
	}

	FVector4 GetCenterShellRect()
	{
		return CenterShellRect;
	}

	FVector4 GetRightShellRect()
	{
		return RightShellRect;
	}

	FVector4 GetIdleStripRect()
	{
		return IdleStripRect;
	}

	FVector4 GetContentRect()
	{
		return ContentRect;
	}

	FVector4 GetNavigationRect()
	{
		return NavigationRect;
	}

	FFitTransform MakeFitTransform(const FVector2D& ViewportSize)
	{
		FFitTransform Result;
		if (ViewportSize.X <= 0.0f || ViewportSize.Y <= 0.0f)
		{
			return Result;
		}
		Result.Scale = FMath::Min(
			ViewportSize.X / ReferenceCanvasSize.X,
			ViewportSize.Y / ReferenceCanvasSize.Y);
		Result.Offset = (ViewportSize - ReferenceCanvasSize * Result.Scale) * 0.5f;
		return Result;
	}
}
