// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveTool.h"
#include "Components/SplineComponent.h"
#include "Quick_RoadGenerateCityTool.generated.h"

class FPrimitiveDrawInterface;

UCLASS(Transient)
class QUICK_ROAD_API UQuick_RoadGenerateCityToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UQuick_RoadGenerateCityToolProperties();

	UPROPERTY(EditAnywhere, Category = "外环线", meta = (
		DisplayName = "样条点类型",
		ToolTip = "生成样条线的插值类型（线性 / 曲线等）。",
		DisplayPriority = 1))
	TEnumAsByte<ESplinePointType::Type> SplinePointType = ESplinePointType::Curve;

	UPROPERTY(EditAnywhere, Category = "外环线", meta = (
		DisplayName = "样条线重采样",
		ToolTip = "开启后按固定间距沿样条弧长重新分布控制点。",
		DisplayPriority = 2))
	bool bEnableSplineResample = true;

	UPROPERTY(EditAnywhere, Category = "外环线", meta = (
		DisplayName = "重采样间距 (cm)",
		ClampMin = "10",
		EditCondition = "bEnableSplineResample",
		ToolTip = "相邻样条点之间的目标弧长距离。",
		DisplayPriority = 3))
	float SplineResampleDistanceCm = 500.f;

	UPROPERTY(EditAnywhere, Category = "外环线", meta = (
		DisplayName = "顶点焊接容差 (cm)",
		ClampMin = "0.1",
		ToolTip = "合并道路网格时，距离小于此值的顶点会被焊接，以消除组件接缝。",
		DisplayPriority = 10))
	float VertexWeldToleranceCm = 2.f;

	UPROPERTY(EditAnywhere, Category = "外环线", meta = (
		DisplayName = "最短边界长度 (cm)",
		ClampMin = "50",
		ToolTip = "候选边界短于此长度时忽略（用于过滤碎段）。",
		DisplayPriority = 11))
	float MinPolylineLengthCm = 200.f;

	UPROPERTY(EditAnywhere, Category = "外环线", meta = (
		DisplayName = "包含路口补丁",
		ToolTip = "是否同时从路口 IntersectionPatch 网格提取边界。",
		DisplayPriority = 12))
	bool bIncludeIntersectionPatches = true;

	UFUNCTION(CallInEditor, Category = "外环线", meta = (DisplayName = "提取外环线", DisplayPriority = 100))
	void ExtractRoadEdgeSplines();

	UPROPERTY(EditAnywhere, Category = "内环线", meta = (
		DisplayName = "样条点类型",
		ToolTip = "内环样条线的插值类型。",
		DisplayPriority = 1))
	TEnumAsByte<ESplinePointType::Type> IntersectionSplinePointType = ESplinePointType::Curve;

	UPROPERTY(EditAnywhere, Category = "内环线", meta = (
		DisplayName = "样条线重采样",
		ToolTip = "开启后按固定间距沿样条弧长重新分布控制点。",
		DisplayPriority = 2))
	bool bEnableIntersectionSplineResample = true;

	UPROPERTY(EditAnywhere, Category = "内环线", meta = (
		DisplayName = "重采样间距 (cm)",
		ClampMin = "10",
		EditCondition = "bEnableIntersectionSplineResample",
		ToolTip = "相邻样条点之间的目标弧长距离。",
		DisplayPriority = 3))
	float IntersectionSplineResampleDistanceCm = 500.f;

	UPROPERTY(EditAnywhere, Category = "内环线", meta = (
		DisplayName = "顶点焊接容差 (cm)",
		ClampMin = "0.1",
		ToolTip = "从道路网格提取内孔边界时的顶点焊接容差。",
		DisplayPriority = 10))
	float IntersectionVertexWeldToleranceCm = 2.f;

	UPROPERTY(EditAnywhere, Category = "内环线", meta = (
		DisplayName = "最短内环长度 (cm)",
		ClampMin = "20",
		ToolTip = "道路网格内孔（如三岔口中央三角区）边界短于此长度时忽略。",
		DisplayPriority = 11))
	float MinInnerLoopLengthCm = 100.f;

	UPROPERTY(EditAnywhere, Category = "内环线", meta = (
		DisplayName = "包含路口补丁",
		ToolTip = "从网格提取内孔时是否包含 IntersectionPatch。",
		DisplayPriority = 12))
	bool bIncludeIntersectionPatchesForInnerLoops = true;

	UPROPERTY(EditAnywhere, Category = "环线调试", meta = (
		DisplayName = "显示点 TNB",
		ToolTip = "开启后在视口中显示已提取环线样条每个点的 TNB 朝向：红色为 Tangent，绿色为 Normal/侧向，蓝色为 Binormal/上方向。",
		DisplayPriority = 1))
	bool bDrawIntersectionSplinePointTbn = false;

	UPROPERTY(EditAnywhere, Category = "环线调试", meta = (
		DisplayName = "TNB 轴长 (cm)",
		ClampMin = "10",
		EditCondition = "bDrawIntersectionSplinePointTbn",
		ToolTip = "视口中每个 TNB 调试轴的显示长度。",
		DisplayPriority = 2))
	float IntersectionSplinePointTbnAxisLengthCm = 150.f;

	UFUNCTION(CallInEditor, Category = "内环线", meta = (DisplayName = "提取内环线", DisplayPriority = 100))
	void ExtractIntersectionSplines();

	UFUNCTION(CallInEditor, Category = "环线调试", meta = (
		DisplayName = "反转选中样条方向",
		ToolTip = "反转当前选中的 Quick_Road 环线样条点序，从而翻转切线和侧向方向。",
		DisplayPriority = 10))
	void ReverseSelectedSplineDirection();

	void SetOwningTool(class UQuick_RoadGenerateCityTool* InTool) { OwningTool = InTool; }

private:
	UPROPERTY(Transient)
	TObjectPtr<class UQuick_RoadGenerateCityTool> OwningTool;
};

UCLASS()
class QUICK_ROAD_API UQuick_RoadGenerateCityToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UCLASS()
class QUICK_ROAD_API UQuick_RoadGenerateCityTool : public UInteractiveTool
{
	GENERATED_BODY()

public:
	void SetWorld(UWorld* World);
	UWorld* GetTargetWorld() const { return TargetWorld; }

	virtual void Setup() override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	void ExecuteExtractRoadEdgeSplines();
	void ExecuteExtractIntersectionSplines();
	void ExecuteReverseSelectedSplineDirection();

protected:
	UPROPERTY()
	TObjectPtr<UQuick_RoadGenerateCityToolProperties> Properties;

	UWorld* TargetWorld = nullptr;

	static bool TryGetSelectedMainRoadNetwork(AActor*& OutNetworkActor, FText& OutErrorMessage);
	void DrawIntersectionSplinePointTbn(FPrimitiveDrawInterface* PDI) const;
	static bool ReverseSplineDirection(USplineComponent* SplineComponent);
};
