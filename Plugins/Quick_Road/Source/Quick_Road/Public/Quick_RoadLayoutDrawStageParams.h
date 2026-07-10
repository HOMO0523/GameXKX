// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Quick_RoadLayoutDrawStageParams.generated.h"

/** 阶段 1：区域样条 + 布局面片（字段 Category 由外层 UPROPERTY 继承） */

USTRUCT()

struct FQuick_RoadLayoutCityScopeStageParams

{

	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (DisplayName = "采样间距 (cm)", ClampMin = "10", DisplayPriority = 10))

	float SampleDistanceCm = 2000.f;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (DisplayName = "布局面细分", ClampMin = "1", DisplayPriority = 11))

	int32 LayoutMeshSubdivisions = 60;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "最大斜坡角度",
		ClampMin = "0",
		ClampMax = "89.9",
		UIMin = "0",
		UIMax = "60",
		DisplayPriority = 12,
		ToolTip = "网格边允许的最大坡度（度）。仅限制过陡区域并做平滑，不会把面片压平；角度越大越保留地形起伏。"))
	float MaxSlopeAngleDegrees = 15.f;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "地形贴合强度",
		ClampMin = "0",
		ClampMax = "1",
		UIMin = "0",
		UIMax = "1",
		DisplayPriority = 13,
		ToolTip = "1 = 尽量跟随地形采样高度；0 = 趋近区域平均高度（更平）。要有坡度请设为 0.7 以上。"))
	float TerrainBlendStrength = 0.85f;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (DisplayName = "编辑衰减 (cm)", ClampMin = "0", DisplayPriority = 14))
	float EditInfluenceFalloffCm = 500.f;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "默认材质",
		DisplayPriority = 15,
		ToolTip = "生成布局面片时自动赋予；留空则使用插件默认材质。"))
	TObjectPtr<class UMaterialInterface> DefaultMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "台阶高度 (cm)",
		ClampMin = "10",
		DisplayPriority = 20,
		ToolTip = "每层平台之间的垂直高度差。值越小台阶越多。"))
	float TerraceStepSizeCm = 300.f;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "自动高度范围",
		DisplayPriority = 21,
		ToolTip = "开启后根据当前布局面片顶点的最低/最高 Z 自动确定阶梯化范围。"))
	bool bTerraceAutoHeightRange = true;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "最低高度 (cm)",
		DisplayPriority = 22,
		EditCondition = "!bTerraceAutoHeightRange",
		ToolTip = "阶梯化生效的最低世界高度（关闭自动范围时有效）。"))
	float TerraceMinHeightCm = 0.f;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "最高高度 (cm)",
		DisplayPriority = 23,
		EditCondition = "!bTerraceAutoHeightRange",
		ToolTip = "阶梯化生效的最高世界高度（关闭自动范围时有效）。"))
	float TerraceMaxHeightCm = 10000.f;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "原高度混合",
		ClampMin = "0",
		ClampMax = "1",
		UIMin = "0",
		UIMax = "1",
		DisplayPriority = 24,
		ToolTip = "0 = 纯台阶；1 = 保持原高度。中间值混合原地形与台阶。"))
	float TerraceFade = 0.f;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "崖壁平滑",
		ClampMin = "0",
		ClampMax = "1",
		UIMin = "0",
		UIMax = "1",
		DisplayPriority = 25,
		ToolTip = "在台阶边界向邻接高度过渡，减轻垂直硬边。"))
	float TerraceSmoothEdges = 0.3f;

};

/** 阶段 2：主路（字段 Category 由外层 UPROPERTY 继承，如「道路」） */

USTRUCT()

struct FQuick_RoadLayoutMainRoadStageParams

{

	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (DisplayName = "采样间距 (cm)", ClampMin = "10", DisplayPriority = 10))

	float SampleDistanceCm = 1000.f;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (DisplayName = "主路宽度 (cm)", ClampMin = "100", DisplayPriority = 11))

	float MainRoadWidthCm = 500.f;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (DisplayName = "影响衰减 (cm)", ClampMin = "0", DisplayPriority = 12))

	float InfluenceFalloffCm = 300.f;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "影响混合强度",
		ClampMin = "0",
		ClampMax = "1",
		UIMin = "0",
		UIMax = "1",
		DisplayPriority = 13,
		ToolTip = "道路路带对地形与布局面片的高度混合权重，1 为完全贴合道路高度。"))
	float InfluenceBlendStrength = 1.f;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "道路 Tag",
		DisplayPriority = 14,
		ToolTip = "「生成道路」时扫描全场，仅处理 Actor 或 Spline 组件上带有此 Tag 的样条线；生成后会额外写入 Quick_Road_IntersectionSource。"))
	FName RoadTag = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "默认材质",
		DisplayPriority = 15,
		ToolTip = "生成道路网格与交叉口补丁时自动赋予；留空则使用插件默认材质。"))
	TObjectPtr<class UMaterialInterface> DefaultRoadMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "网格采样间距 (cm)",
		ClampMin = "25",
		DisplayPriority = 20,
		ToolTip = "沿样条方向的 Ribbon 采样间距。越小纵向面数越多、弯道越平滑。"))
	float RoadMeshSampleDistanceCm = 500.f;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "路宽细分",
		ClampMin = "1",
		ClampMax = "16",
		DisplayPriority = 21,
		ToolTip = "路宽方向分段数。1=仅左右两条边；越大横截面越密、面数越多。"))
	int32 RoadMeshWidthSubdivisions = 3;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "弯道自适应加密",
		DisplayPriority = 22,
		ToolTip = "在曲率较大的区段自动插入额外采样点，减少弯道多边形折线感。"))
	bool bRoadMeshAdaptiveCurvature = true;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "弯道角度阈值 (度)",
		ClampMin = "1",
		ClampMax = "45",
		DisplayPriority = 23,
		EditCondition = "bRoadMeshAdaptiveCurvature",
		ToolTip = "相邻采样切线夹角超过此值时继续细分。"))
	float RoadMeshCurvatureThresholdDeg = 8.f;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "弯道最大细分层级",
		ClampMin = "0",
		ClampMax = "6",
		DisplayPriority = 24,
		EditCondition = "bRoadMeshAdaptiveCurvature",
		ToolTip = "单段递归细分的最大层级，防止面数失控。"))
	int32 RoadMeshMaxCurvatureSubdivisions = 3;

};

/** 阶段 2：交叉口（字段 Category 由外层 UPROPERTY 继承，如「交叉口」） */

USTRUCT()

struct FQuick_RoadLayoutMainRoadIntersectionParams

{

	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "道路 Tag",
		DisplayPriority = 29,
		ToolTip = "检测交叉点 / 拆开并重构时使用。可填单个 Tag（如 aa），或用 | 连接多个 Tag（如 aa|dd），表示处理这些 Tag 各自及彼此之间的交叉口。"))
	FName RoadTag = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "延伸距离 (cm)",
		ClampMin = "50",
		DisplayPriority = 30,
		ToolTip = "从交叉点沿每条样条方向向外延伸的路口补丁长度；路段 Ribbon 在此距离之外开始。"))
	float ExtendLengthCm = 600.f;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "交叉点检测采样 (cm)",
		ClampMin = "25",
		DisplayPriority = 31))
	float IntersectionDetectSampleCm = 100.f;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "沿样条采样间距 (cm)",
		ClampMin = "10",
		DisplayPriority = 32,
		ToolTip = "路口各臂沿样条方向的拓扑采样间距。"))
	float IntersectionGridCellSizeCm = 50.f;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "样条拓扑重建路口",
		DisplayPriority = 33,
		ToolTip = "按交叉口样条走势生成程序化 mesh。"))
	bool bGridRebuildIntersectionPatches = true;

	UPROPERTY(EditAnywhere, Category = "Quick Road", meta = (
		DisplayName = "街角半径系数",
		ClampMin = "0.25",
		ClampMax = "4.0",
		DisplayPriority = 34,
		ToolTip = "缩放路口中心扇形（内半径）：>1 往外扩、街角更开阔；<1 往里收、街角更尖。1.0 为自适应 miter 临界值。"))
	float IntersectionCornerRadiusScale = 1.3f;

};
