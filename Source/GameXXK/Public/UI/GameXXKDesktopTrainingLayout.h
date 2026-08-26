#pragma once

#include "CoreMinimal.h"

namespace GameXXKDesktopTrainingLayout
{
	enum class EGameXXKDesktopVerticalExpansionDirection : uint8
	{
		Down,
		Up
	};

	struct GAMEXXK_API FFitTransform
	{
		float Scale = 1.0f;
		FVector2D Offset = FVector2D::ZeroVector;

		FVector2D ApplyPoint(const FVector2D& Point) const;
		FVector2D ApplySize(const FVector2D& Size) const;
		FVector4 ApplyRect(const FVector4& Rect) const;
	};

	/** Physical/logical placement of the visible HUD group inside a fixed transparent desktop host. */
	struct GAMEXXK_API FDesktopOverlayPlacement
	{
		FVector2D HostSize = FVector2D::ZeroVector;
		FVector2D HudTopLeft = FVector2D::ZeroVector;
		FVector2D HudSize = FVector2D::ZeroVector;
		FVector2D StripTopLeft = FVector2D::ZeroVector;
		FVector2D StripSize = FVector2D::ZeroVector;
		FVector2D ContentOffset = FVector2D::ZeroVector;
		FVector4 TownToggleRect = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
		float Scale = 1.0f;
	};

	/** Physical work-area and the single scale shared by every HUD layout state in one session. */
	struct GAMEXXK_API FDesktopHudResolvedMetrics
	{
		FVector2D PhysicalWorkAreaSize = FVector2D::ZeroVector;
		float Scale = 1.0f;
	};

	enum class EDesktopNativeRegionShapeType : uint8
	{
		Rectangle,
		Ellipse
	};

	struct GAMEXXK_API FDesktopNativeRegionShape
	{
		EDesktopNativeRegionShapeType Type = EDesktopNativeRegionShapeType::Rectangle;
		FVector4 Rect = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
	};

	struct GAMEXXK_API FDesktopNativeRegionState
	{
		bool bExpanded = false;
		bool bIdleStripFolded = false;
		bool bExpandUpward = false;
		bool bWarehouseOpen = false;
		bool bRightPanelOpen = false;
		bool bExitConfirmationOpen = false;
		bool bTownToggleVisible = false;
		float NoticeHeight = 0.0f;
		float Scale = 1.0f;
		FVector2D ContentOffset = FVector2D::ZeroVector;
		FVector4 TownToggleRect = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
	};

	struct GAMEXXK_API FDesktopOverlayMouseState
	{
		bool bPointerOverInteractiveSurface = false;
		bool bCarryingItem = false;
		bool bMouseCaptured = false;
		bool bDragDropActive = false;
		bool bHudDragging = false;
	};

	enum class EDesktopOverlayMouseSignal : uint8
	{
		Move,
		Button,
		Wheel,
		Other
	};

	GAMEXXK_API FVector2D GetReferenceCanvasSize();
	GAMEXXK_API FVector4 GetWarehouseRect();
	GAMEXXK_API FVector4 GetCenterShellRect();
	GAMEXXK_API FVector4 GetRightShellRect();
	GAMEXXK_API FVector4 GetIdleStripRect();
	GAMEXXK_API FVector4 GetContentRect();
	GAMEXXK_API FVector4 GetNavigationRect();
	GAMEXXK_API FVector2D GetCollapsedHudLogicalSize();
	GAMEXXK_API FVector2D GetTownToggleButtonSize();
	GAMEXXK_API float GetExpandedLeftExtension(bool bWarehouseOpen);
	GAMEXXK_API FVector4 GetTownToggleRect(bool bWarehouseOpen);
	GAMEXXK_API float ComputeAutomaticHudScale(const FVector2D& LogicalWorkAreaSize);
	GAMEXXK_API float ComputeEffectiveHudScale(
		const FVector2D& LogicalWorkAreaSize,
		int32 HudScalePercent);
	GAMEXXK_API FDesktopOverlayPlacement ComputeDesktopOverlayPlacement(
		const FVector2D& HostSize,
		const FVector2D& NormalizedStripAnchor,
		int32 HudScalePercent,
		bool bExpanded,
		bool bExpandUpward,
		float CollapsedNoticeHeight,
		bool bWarehouseOpen = false);
	GAMEXXK_API FDesktopHudResolvedMetrics ResolveDesktopHudMetrics(
		const FVector2D& PhysicalWorkAreaSize,
		int32 HudScalePercent);
	GAMEXXK_API FDesktopOverlayPlacement ComputeDesktopOverlayPlacementAtScale(
		const FDesktopHudResolvedMetrics& Metrics,
		const FVector2D& NormalizedStripAnchor,
		bool bExpanded,
		bool bExpandUpward,
		float CollapsedNoticeHeight,
		bool bWarehouseOpen = false);
	GAMEXXK_API FVector2D ResolvePresentationAnchor(
		bool bUsePersistedAnchor,
		const FVector2D& PersistedAnchor);
	GAMEXXK_API FVector2D PhysicalPixelsToSlateHost(
		const FVector2D& PhysicalPixels,
		float DpiScale);
	GAMEXXK_API FVector2D DesktopClientPointToReference(
		const FVector2D& ClientPoint,
		float HudScale,
		const FVector2D& VisualHalfSize);
	GAMEXXK_API TArray<FDesktopNativeRegionShape> BuildDesktopNativeRegionShapes(
		const FDesktopNativeRegionState& State);
	GAMEXXK_API bool IsPointInsideDesktopNativeRegionShapes(
		const TArray<FDesktopNativeRegionShape>& Shapes,
		const FVector2D& Point);
	GAMEXXK_API bool ShouldDesktopOverlayPassMouseThrough(
		const FDesktopOverlayMouseState& State);
	GAMEXXK_API bool ShouldSynchronizeDesktopMousePassthrough(
		EDesktopOverlayMouseSignal Signal);
	GAMEXXK_API EGameXXKDesktopVerticalExpansionDirection ChooseVerticalExpansionDirection(
		const FVector4& WorkAreaRect,
		const FVector4& StripRect,
		float ExpandedWindowHeight);
	GAMEXXK_API FFitTransform MakeFitTransform(const FVector2D& ViewportSize);
}
