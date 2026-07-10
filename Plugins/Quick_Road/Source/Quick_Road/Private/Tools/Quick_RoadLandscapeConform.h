// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ALandscape;
class AActor;
class UPrimitiveComponent;
class UWorld;

enum class EQuick_RoadConformSurfaceMode : uint8
{
	/** StaticMesh 等：取朝下底面，地形上抬贴合模型底部。 */
	Bottom,
	/** 布局面片等：取朝上顶面，地形贴合网格表面高度。 */
	Top
};

struct FQuick_RoadConformToMeshParams
{
	/** 相对模型底面的垂直偏移（cm，正值抬高地形）。 */
	float VerticalOffset = -5.f;

	/** 贴合区外缘向原地形过渡的宽度（高度图格数），0=硬边。 */
	int32 Smoothness = 2;

	/** 相对模型底面 XY 轮廓的边缘偏移（cm）。 */
	float EdgeOffsetCm = 100.f;

	/** 0 保持原地形，1 完全贴合目标高度。 */
	float BlendStrength = 1.f;
};

struct FQuick_RoadConformEdgeSmoothParams
{
	/** 沿城市范围样条边界向内外扩展的平滑带宽度（cm）。 */
	float SmoothWidthCm = 400.f;

	/** 单次迭代的平滑权重，0~1。 */
	float SmoothStrength = 0.75f;

	/** Laplacian 平滑迭代次数。 */
	int32 SmoothIterations = 6;

	/** 样条采样点数，用于构建边界多边形。 */
	int32 SplineSampleCount = 64;
};

class FQuick_RoadLandscapeConform
{
public:
	/**
	 * 识别模型表面 → 重心插值高度 → 直写当前 Edit Layer 高度图 → RequestLayersContentUpdate。
	 * @param SurfaceMode Bottom=模型底面；Top=布局面片顶面（ProceduralMesh）。
	 */
	static bool ApplyConformToLandscape(
		ALandscape* Landscape,
		const TArray<UPrimitiveComponent*>& ConformComponents,
		const FQuick_RoadConformToMeshParams& Params,
		FText& OutErrorMessage,
		EQuick_RoadConformSurfaceMode SurfaceMode = EQuick_RoadConformSurfaceMode::Bottom);

	/** 沿城市范围样条边界平滑 Landscape 高度，消除贴合后的锯齿陡坎。 */
	static bool SmoothLandscapeAtCityScopeBoundary(
		UWorld* World,
		ALandscape* Landscape,
		const FQuick_RoadConformEdgeSmoothParams& Params,
		FText& OutErrorMessage);
};
