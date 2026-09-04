#include "UI/GameXXKDesktopTrainingLayout.h"

namespace GameXXKDesktopTrainingLayout
{
	namespace
	{
		const FVector2D ReferenceCanvasSize(1672.0f, 941.0f);
		const FVector4 WarehouseRect(10.0f, 17.0f, 363.0f, 908.0f);
		const FVector4 CenterShellRect(386.0f, 17.0f, 970.0f, 908.0f);
		const FVector4 RightShellRect(1369.0f, 17.0f, 291.0f, 908.0f);
		const FVector4 IdleStripRect(397.0f, 17.0f, 945.0f, 184.0f);
		const FVector4 ContentRect(397.0f, 244.0f, 945.0f, 533.0f);
		const FVector4 NavigationRect(397.0f, 788.0f, 945.0f, 137.0f);
		const FVector2D CollapsedHudLogicalSize(1038.0f, 202.0f);
		const FVector2D FoldedHudInteractiveSize(1025.0f, 24.0f);
		const FVector2D TownToggleButtonSize(144.0f, 144.0f);
		constexpr float TownToggleGap = 14.0f;
		constexpr float StoryQuestVerticalGap = 14.0f;
		constexpr float OpenWarehouseLeftExtension = 148.0f;
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

	FVector2D GetCollapsedHudLogicalSize()
	{
		return CollapsedHudLogicalSize;
	}

	FVector2D GetTownToggleButtonSize()
	{
		return TownToggleButtonSize;
	}

	FVector2D GetStoryQuestButtonSize()
	{
		return TownToggleButtonSize;
	}

	float GetExpandedLeftExtension(const bool bWarehouseOpen)
	{
		return bWarehouseOpen ? OpenWarehouseLeftExtension : 0.0f;
	}

	FVector4 GetTownToggleRect(const bool bWarehouseOpen)
	{
		const float X = bWarehouseOpen
			? 0.0f
			: CenterShellRect.X - TownToggleGap - TownToggleButtonSize.X;
		const float Y = ContentRect.Y + (ContentRect.W - TownToggleButtonSize.Y) * 0.5f;
		return FVector4(X, Y, TownToggleButtonSize.X, TownToggleButtonSize.Y);
	}

	FVector4 GetStoryQuestRect(const bool bWarehouseOpen)
	{
		const FVector4 TownRect = GetTownToggleRect(bWarehouseOpen);
		return FVector4(
			TownRect.X,
			TownRect.Y - TownToggleButtonSize.Y - StoryQuestVerticalGap,
			TownToggleButtonSize.X,
			TownToggleButtonSize.Y);
	}

	float ResolveManualHudScale(const int32 HudScalePercent)
	{
		if (HudScalePercent <= 50)
		{
			return 0.5f;
		}
		return HudScalePercent <= 75 ? 0.75f : 1.0f;
	}

	FDesktopOverlayPlacement ComputeDesktopOverlayPlacement(
		const FVector2D& HostSize,
		const FVector2D& NormalizedStripAnchor,
		const int32 HudScalePercent,
		const bool bExpanded,
		const bool bExpandUpward,
		const float CollapsedNoticeHeight,
		const bool bWarehouseOpen)
	{
		return ComputeDesktopOverlayPlacementAtScale(
			ResolveDesktopHudMetrics(HostSize, HudScalePercent),
			NormalizedStripAnchor,
			bExpanded,
			bExpandUpward,
			CollapsedNoticeHeight,
			bWarehouseOpen);
	}

	FDesktopHudResolvedMetrics ResolveDesktopHudMetrics(
		const FVector2D& PhysicalWorkAreaSize,
		const int32 HudScalePercent)
	{
		FDesktopHudResolvedMetrics Result;
		Result.PhysicalWorkAreaSize = FVector2D(
			FMath::Max(1.0f, PhysicalWorkAreaSize.X),
			FMath::Max(1.0f, PhysicalWorkAreaSize.Y));
		Result.Scale = ResolveManualHudScale(HudScalePercent);
		return Result;
	}

	FDesktopOverlayPlacement ComputeDesktopOverlayPlacementAtScale(
		const FDesktopHudResolvedMetrics& Metrics,
		const FVector2D& NormalizedStripAnchor,
		const bool bExpanded,
		const bool bExpandUpward,
		const float CollapsedNoticeHeight,
		const bool bWarehouseOpen)
	{
		FDesktopOverlayPlacement Result;
		Result.HostSize = FVector2D(
			FMath::Max(1.0f, Metrics.PhysicalWorkAreaSize.X),
			FMath::Max(1.0f, Metrics.PhysicalWorkAreaSize.Y));
		Result.Scale = FMath::Max(0.05f, Metrics.Scale);

		const FVector2D SafeNormalizedAnchor(
			FMath::Clamp(NormalizedStripAnchor.X, 0.0f, 1.0f),
			FMath::Clamp(NormalizedStripAnchor.Y, 0.0f, 1.0f));
		const FVector2D CollapsedStripSize = CollapsedHudLogicalSize * Result.Scale;
		const FVector2D AnchorTravel(
			FMath::Max(0.0f, Result.HostSize.X - CollapsedStripSize.X),
			FMath::Max(0.0f, Result.HostSize.Y - CollapsedStripSize.Y));
		const FVector2D DesiredStripAnchor = AnchorTravel * SafeNormalizedAnchor;

		FVector4 StripRect(0.0f, 0.0f, CollapsedHudLogicalSize.X, CollapsedHudLogicalSize.Y);
		FVector2D HudDesignSize(
			CollapsedHudLogicalSize.X,
			CollapsedHudLogicalSize.Y + FMath::Max(0.0f, CollapsedNoticeHeight));
		if (bExpanded)
		{
			const float LeftExtension = GetExpandedLeftExtension(bWarehouseOpen);
			HudDesignSize = ReferenceCanvasSize + FVector2D(LeftExtension, 0.0f);
			StripRect = IdleStripRect;
			StripRect.X += LeftExtension;
			Result.ContentOffset = FVector2D(LeftExtension, 0.0f);
			Result.TownToggleRect = GetTownToggleRect(bWarehouseOpen);
			Result.StoryQuestRect = GetStoryQuestRect(bWarehouseOpen);
			if (bExpandUpward)
			{
				StripRect.Y = ReferenceCanvasSize.Y - StripRect.W - 18.0f;
			}
		}

		Result.HudSize = HudDesignSize * Result.Scale;
		const FVector2D StripOffset(StripRect.X * Result.Scale, StripRect.Y * Result.Scale);
		const FVector2D MaximumHudTopLeft(
			FMath::Max(0.0f, Result.HostSize.X - Result.HudSize.X),
			FMath::Max(0.0f, Result.HostSize.Y - Result.HudSize.Y));
		const FVector2D DesiredHudTopLeft = DesiredStripAnchor - StripOffset;
		Result.HudTopLeft = FVector2D(
			FMath::Clamp(DesiredHudTopLeft.X, 0.0f, MaximumHudTopLeft.X),
			FMath::Clamp(DesiredHudTopLeft.Y, 0.0f, MaximumHudTopLeft.Y));
		Result.StripTopLeft = Result.HudTopLeft + StripOffset;
		Result.StripSize = FVector2D(StripRect.Z, StripRect.W) * Result.Scale;
		return Result;
	}

	FVector2D ResolvePresentationAnchor(
		const bool bUsePersistedAnchor,
		const FVector2D& PersistedAnchor)
	{
		if (!bUsePersistedAnchor)
		{
			return FVector2D(0.5f, 0.08f);
		}
		return FVector2D(
			FMath::Clamp(PersistedAnchor.X, 0.0f, 1.0f),
			FMath::Clamp(PersistedAnchor.Y, 0.0f, 1.0f));
	}

	FDesktopSlateHostGeometry ResolveDesktopSlateHostGeometry(
		const FDesktopOverlayPlacement& Placement,
		const FVector2D& FixedContentOffset,
		const bool bDesktopWindow)
	{
		FDesktopSlateHostGeometry Result;
		Result.Position = bDesktopWindow
			? FixedContentOffset
			: Placement.HudTopLeft;
		Result.Size = Placement.HudSize;
		return Result;
	}

	FVector2D ResolveDesktopHudDragAnchor(
		const FVector2D& DragStartNormalizedAnchor,
		const FVector2D& DragStartPointerScreen,
		const FVector2D& CurrentPointerScreen,
		const FVector2D& PhysicalWorkAreaSize,
		const FVector2D& CollapsedStripSize)
	{
		const FVector2D SafeStartAnchor(
			FMath::Clamp(DragStartNormalizedAnchor.X, 0.0f, 1.0f),
			FMath::Clamp(DragStartNormalizedAnchor.Y, 0.0f, 1.0f));
		const FVector2D PointerDelta = CurrentPointerScreen - DragStartPointerScreen;
		const auto ResolveAxis = [](const float StartAnchor,
			const float Delta,
			const float HostExtent,
			const float StripExtent)
		{
			const float AvailableTravel = HostExtent - StripExtent;
			if (AvailableTravel <= KINDA_SMALL_NUMBER)
			{
				return 0.0f;
			}
			return FMath::Clamp(StartAnchor + Delta / AvailableTravel, 0.0f, 1.0f);
		};
		return FVector2D(
			ResolveAxis(
				SafeStartAnchor.X,
				PointerDelta.X,
				PhysicalWorkAreaSize.X,
				CollapsedStripSize.X),
			ResolveAxis(
				SafeStartAnchor.Y,
				PointerDelta.Y,
				PhysicalWorkAreaSize.Y,
				CollapsedStripSize.Y));
	}

	FVector2D DesktopClientPointToReference(
		const FVector2D& ClientPoint,
		const float HudScale,
		const FVector2D& VisualHalfSize)
	{
		return ClientPoint / FMath::Max(0.01f, HudScale) - VisualHalfSize;
	}

	TArray<FDesktopNativeRegionShape> BuildDesktopNativeRegionShapes(
		const FDesktopNativeRegionState& State)
	{
		TArray<FDesktopNativeRegionShape> Result;
		const float Scale = FMath::Max(0.01f, State.Scale);
		const auto AddLogicalRect = [&Result, Scale](
			const FVector4& LogicalRect,
			const FVector2D& Offset,
			const EDesktopNativeRegionShapeType Type = EDesktopNativeRegionShapeType::Rectangle)
		{
			if (LogicalRect.Z <= 0.0f || LogicalRect.W <= 0.0f)
			{
				return;
			}
			FDesktopNativeRegionShape Shape;
			Shape.Type = Type;
			Shape.Rect = FVector4(
				(LogicalRect.X + Offset.X) * Scale,
				(LogicalRect.Y + Offset.Y) * Scale,
				LogicalRect.Z * Scale,
				LogicalRect.W * Scale);
			Result.Add(Shape);
		};

		if (!State.bExpanded)
		{
			if (State.bIdleStripFolded)
			{
				AddLogicalRect(
					FVector4(
						0.0f,
						0.0f,
						FoldedHudInteractiveSize.X,
						FoldedHudInteractiveSize.Y),
					FVector2D::ZeroVector);
				if (State.NoticeHeight > FoldedHudInteractiveSize.Y)
				{
					AddLogicalRect(
						FVector4(
							0.0f,
							FoldedHudInteractiveSize.Y,
							420.0f,
							State.NoticeHeight - FoldedHudInteractiveSize.Y),
						FVector2D::ZeroVector);
				}
				return Result;
			}
			const FVector2D CollapsedSize = GetCollapsedHudLogicalSize();
			AddLogicalRect(
				FVector4(0.0f, 0.0f, CollapsedSize.X, CollapsedSize.Y),
				FVector2D::ZeroVector);
			// The notice rail's fold/progress/Tab controls occupy the complete
			// 1025x24 row directly below the travel strip. Keep that visual and
			// input row in the native region even though the scrolling notice text
			// itself only extends across the left 420 pixels.
			AddLogicalRect(
				FVector4(
					0.0f,
					CollapsedSize.Y,
					FoldedHudInteractiveSize.X,
					FoldedHudInteractiveSize.Y),
				FVector2D::ZeroVector);
			if (State.NoticeHeight > 0.0f)
			{
				AddLogicalRect(
					FVector4(0.0f, CollapsedSize.Y, 420.0f, State.NoticeHeight),
					FVector2D::ZeroVector);
			}
			return Result;
		}

		if (State.bExitConfirmationOpen)
		{
			const FVector2D ReferenceSize = GetReferenceCanvasSize();
			AddLogicalRect(
				FVector4(
					0.0f,
					0.0f,
					ReferenceSize.X + State.ContentOffset.X,
					ReferenceSize.Y),
				FVector2D::ZeroVector);
			return Result;
		}

		FVector4 StripRect = GetIdleStripRect();
		if (State.bExpandUpward)
		{
			StripRect.Y = GetReferenceCanvasSize().Y - StripRect.W - 18.0f;
		}
		if (State.bIdleStripFolded)
		{
			StripRect.W = 24.0f;
		}
		AddLogicalRect(StripRect, State.ContentOffset);
		if (!State.bIdleStripFolded)
		{
			const float NoticeY = State.bExpandUpward
				? StripRect.Y - State.NoticeHeight
				: 210.0f;
			AddLogicalRect(
				FVector4(397.0f, NoticeY, 420.0f, State.NoticeHeight),
				State.ContentOffset);
			AddLogicalRect(
				FVector4(397.0f, NoticeY, GetIdleStripRect().Z, 24.0f),
				State.ContentOffset);
		}
		AddLogicalRect(GetContentRect(), State.ContentOffset);
		for (int32 NavigationIndex = 0; NavigationIndex < 5; ++NavigationIndex)
		{
			AddLogicalRect(
				FVector4(421.0f + NavigationIndex * 181.0f, 800.0f, 151.0f, 112.0f),
				State.ContentOffset);
		}
		for (int32 ToolbarIndex = 0; ToolbarIndex < 5; ++ToolbarIndex)
		{
			AddLogicalRect(
				FVector4(1092.0f + ToolbarIndex * 47.0f, 226.0f, 42.0f, 36.0f),
				State.ContentOffset);
		}
		if (State.bWarehouseOpen)
		{
			AddLogicalRect(GetWarehouseRect(), State.ContentOffset);
		}
		if (State.bRightPanelOpen)
		{
			AddLogicalRect(GetRightShellRect(), State.ContentOffset);
		}
		if (State.bTownToggleVisible)
		{
			AddLogicalRect(
				State.TownToggleRect,
				FVector2D::ZeroVector,
				EDesktopNativeRegionShapeType::Ellipse);
		}
		if (State.bStoryQuestVisible)
		{
			AddLogicalRect(
				State.StoryQuestRect,
				FVector2D::ZeroVector,
				EDesktopNativeRegionShapeType::Ellipse);
		}
		return Result;
	}

	FVector4 ResolveDesktopNativeRegionRect(
		const FDesktopNativeRegionShape& Shape,
		const FVector2D& HostOffset,
		const float Padding)
	{
		const float SafePadding = FMath::Max(0.0f, Padding);
		return FVector4(
			Shape.Rect.X + HostOffset.X - SafePadding,
			Shape.Rect.Y + HostOffset.Y - SafePadding,
			Shape.Rect.Z + SafePadding * 2.0f,
			Shape.Rect.W + SafePadding * 2.0f);
	}

	bool IsPointInsideDesktopNativeRegionShapes(
		const TArray<FDesktopNativeRegionShape>& Shapes,
		const FVector2D& Point)
	{
		for (const FDesktopNativeRegionShape& Shape : Shapes)
		{
			const FVector4& Rect = Shape.Rect;
			if (Rect.Z <= 0.0f || Rect.W <= 0.0f)
			{
				continue;
			}
			if (Shape.Type == EDesktopNativeRegionShapeType::Rectangle)
			{
				if (Point.X >= Rect.X
					&& Point.Y >= Rect.Y
					&& Point.X < Rect.X + Rect.Z
					&& Point.Y < Rect.Y + Rect.W)
				{
					return true;
				}
				continue;
			}
			const float RadiusX = Rect.Z * 0.5f;
			const float RadiusY = Rect.W * 0.5f;
			const float NormalizedX = (Point.X - (Rect.X + RadiusX)) / RadiusX;
			const float NormalizedY = (Point.Y - (Rect.Y + RadiusY)) / RadiusY;
			if (NormalizedX * NormalizedX + NormalizedY * NormalizedY <= 1.0f)
			{
				return true;
			}
		}
		return false;
	}

	bool ShouldSynchronizeDesktopMousePassthrough(
		const EDesktopOverlayMouseSignal Signal)
	{
		return Signal == EDesktopOverlayMouseSignal::Move
			|| Signal == EDesktopOverlayMouseSignal::Button
			|| Signal == EDesktopOverlayMouseSignal::Wheel;
	}

	bool ShouldDesktopOverlayPassMouseThrough(const FDesktopOverlayMouseState& State)
	{
		return !State.bPointerOverInteractiveSurface
			&& !State.bCarryingItem
			&& !State.bMouseCaptured
			&& !State.bDragDropActive
			&& !State.bHudDragging;
	}

	EGameXXKDesktopVerticalExpansionDirection ChooseVerticalExpansionDirection(
		const FVector4& WorkAreaRect,
		const FVector4& StripRect,
		const float ExpandedWindowHeight)
	{
		const float RequiredExtraHeight = FMath::Max(0.0f, ExpandedWindowHeight - StripRect.W);
		const float SpaceAbove = FMath::Max(0.0f, StripRect.Y - WorkAreaRect.Y);
		const float WorkAreaBottom = WorkAreaRect.Y + WorkAreaRect.W;
		const float SpaceBelow = FMath::Max(0.0f, WorkAreaBottom - (StripRect.Y + StripRect.W));
		if (SpaceBelow >= RequiredExtraHeight)
		{
			return EGameXXKDesktopVerticalExpansionDirection::Down;
		}
		if (SpaceAbove >= RequiredExtraHeight)
		{
			return EGameXXKDesktopVerticalExpansionDirection::Up;
		}
		return SpaceAbove > SpaceBelow
			? EGameXXKDesktopVerticalExpansionDirection::Up
			: EGameXXKDesktopVerticalExpansionDirection::Down;
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
