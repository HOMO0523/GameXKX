// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Quick_RoadLayoutLandscapeConformComponent.generated.h"

class ALandscape;
class UProceduralMeshComponent;

/**
 * 布局面片 Actor 上的独立「地面贴合」组件。
 * 与 LayoutMesh 同级挂载，将 Landscape 贴合到同 Actor 上程序化网格的顶面。
 */
UCLASS(ClassGroup = (QuickRoad), meta = (BlueprintSpawnableComponent, DisplayName = "地面贴合"))
class QUICK_ROAD_API UQuick_RoadLayoutLandscapeConformComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQuick_RoadLayoutLandscapeConformComponent();

	UPROPERTY(EditAnywhere, Category = "地面贴合", meta = (
		DisplayName = "边缘过渡",
		DisplayPriority = 10,
		ToolTip = "贴合区域外缘向原地形渐变的宽度（高度图格数）。0=垂直硬边。"))
	int32 ConformSmoothness = 2;

	UPROPERTY(EditAnywhere, Category = "地面贴合", meta = (
		DisplayName = "边缘偏移 (cm)",
		DisplayPriority = 11,
		ToolTip = "在布局面片 XY 轮廓外再扩展多少厘米。"))
	float ConformEdgeOffsetCm = 100.f;

	UPROPERTY(EditAnywhere, Category = "地面贴合", meta = (
		DisplayName = "高度偏移 (cm)",
		DisplayPriority = 12,
		ToolTip = "相对布局面片表面的垂直偏移（cm，正值抬高地形）。"))
	float ConformVerticalOffset = -5.f;

	UPROPERTY(EditAnywhere, Category = "地面贴合", meta = (
		DisplayName = "混合强度",
		DisplayPriority = 13,
		ToolTip = "贴合目标高度与原地形的混合权重，1 为完全贴合。"))
	float ConformBlendStrength = 1.f;

	UPROPERTY(EditAnywhere, Category = "地面贴合", meta = (
		DisplayName = "边缘平滑宽度 (cm)",
		ClampMin = "50",
		DisplayPriority = 30,
		ToolTip = "沿城市范围样条边界向内外扩展的平滑带宽度。"))
	float EdgeSmoothWidthCm = 400.f;

	UPROPERTY(EditAnywhere, Category = "地面贴合", meta = (
		DisplayName = "边缘平滑强度",
		ClampMin = "0",
		ClampMax = "1",
		UIMin = "0",
		UIMax = "1",
		DisplayPriority = 31,
		ToolTip = "越接近边界平滑越强，用于消除贴合后的锯齿陡坎。"))
	float EdgeSmoothStrength = 0.75f;

	UPROPERTY(EditAnywhere, Category = "地面贴合", meta = (
		DisplayName = "边缘平滑迭代",
		ClampMin = "1",
		ClampMax = "32",
		DisplayPriority = 32,
		ToolTip = "平滑迭代次数，越大过渡越柔和。"))
	int32 EdgeSmoothIterations = 6;

	UFUNCTION(CallInEditor, Category = "地面贴合", meta = (DisplayName = "贴合地形", DisplayPriority = 100))
	void ConformLandscape();

	UFUNCTION(CallInEditor, Category = "地面贴合", meta = (DisplayName = "边缘", DisplayPriority = 101))
	void SmoothConformEdge();

private:
	ALandscape* ResolveTargetLandscape() const;
	UProceduralMeshComponent* ResolveLayoutMeshComponent() const;
};
