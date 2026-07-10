// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Quick_RoadRoadMeshTypes.generated.h"

USTRUCT(BlueprintType)
struct FQuick_RoadRoadRibbonBuildParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "路段网格", meta = (DisplayName = "路带半宽 (cm)", ClampMin = "50"))
	float HalfWidthCm = 250.f;

	UPROPERTY(EditAnywhere, Category = "路段网格", meta = (DisplayName = "网格采样间距 (cm)", ClampMin = "25"))
	float SampleDistanceCm = 500.f;

	UPROPERTY(EditAnywhere, Category = "路段网格", meta = (DisplayName = "路宽细分", ClampMin = "1", ClampMax = "16"))
	int32 WidthSubdivisions = 3;

	UPROPERTY(EditAnywhere, Category = "路段网格", meta = (DisplayName = "弯道自适应加密"))
	bool bAdaptiveCurvature = true;

	UPROPERTY(EditAnywhere, Category = "路段网格", meta = (DisplayName = "弯道角度阈值 (度)", ClampMin = "1", ClampMax = "45"))
	float CurvatureThresholdDeg = 8.f;

	UPROPERTY(EditAnywhere, Category = "路段网格", meta = (DisplayName = "弯道最大细分层级", ClampMin = "0", ClampMax = "6"))
	int32 MaxCurvatureSubdivisions = 3;
};

USTRUCT(BlueprintType)
struct FQuick_RoadRoadSplitRebuildSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "交叉口网格", meta = (DisplayName = "延伸距离 (cm)", ClampMin = "50"))
	float ExtendLengthCm = 600.f;

	UPROPERTY(EditAnywhere, Category = "交叉口网格", meta = (DisplayName = "交叉点检测采样 (cm)", ClampMin = "25"))
	float IntersectionDetectSampleCm = 100.f;

	UPROPERTY(EditAnywhere, Category = "交叉口网格", meta = (DisplayName = "路段网格参数"))
	FQuick_RoadRoadRibbonBuildParams RibbonParams;

	UPROPERTY(EditAnywhere, Category = "交叉口网格", meta = (
		DisplayName = "沿样条采样间距 (cm)",
		ClampMin = "10",
		ToolTip = "路口各臂沿样条方向的拓扑采样间距。越小越精细。"))
	float IntersectionGridCellSizeCm = 50.f;

	UPROPERTY(EditAnywhere, Category = "交叉口网格", meta = (
		DisplayName = "样条拓扑重建路口",
		ToolTip = "开启后按交叉口样条走势生成程序化 mesh；关闭则每臂仅一条 Ribbon。"))
	bool bGridRebuildIntersectionPatches = true;

	UPROPERTY(EditAnywhere, Category = "交叉口网格", meta = (
		DisplayName = "街角半径系数",
		ClampMin = "0.25",
		ClampMax = "4.0",
		ToolTip = "缩放路口中心扇形（内半径）：>1 往外扩、街角更开阔、臂变短；<1 往里收、街角更尖（过小相邻路面会重叠）。1.0 为自适应 miter 临界值。"))
	float IntersectionCornerRadiusScale = 1.3f;
};
