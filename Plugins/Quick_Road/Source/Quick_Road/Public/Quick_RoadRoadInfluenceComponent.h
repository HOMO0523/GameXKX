// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"

#include "Quick_RoadRoadMeshTypes.h"

#include "Quick_RoadRoadInfluenceComponent.generated.h"

class USplineComponent;

class UProceduralMeshComponent;

/**

 * 道路网络 Actor 上的影响组件：读取同 Actor 上全部 ProcMesh（RoadMesh、交叉口补丁等），将高度写入 Landscape 与布局面片。

 */

UCLASS(ClassGroup = (QuickRoad), meta = (BlueprintSpawnableComponent, DisplayName = "道路影响"))

class QUICK_ROAD_API UQuick_RoadRoadInfluenceComponent : public UActorComponent

{

	GENERATED_BODY()

public:

	UQuick_RoadRoadInfluenceComponent();

	UPROPERTY(EditAnywhere, Category = "道路影响", meta = (DisplayName = "路带半宽 (cm)", ClampMin = "50", DisplayPriority = 10))

	float HalfWidthCm = 600.f;

	UPROPERTY(EditAnywhere, Category = "道路影响", meta = (DisplayName = "影响衰减 (cm)", ClampMin = "0", DisplayPriority = 11))

	float InfluenceFalloffCm = 300.f;

	UPROPERTY(EditAnywhere, Category = "道路影响", meta = (

		DisplayName = "混合强度",

		ClampMin = "0",

		ClampMax = "1",

		DisplayPriority = 12))

	float BlendStrength = 1.f;

	UPROPERTY(EditAnywhere, Category = "道路影响", meta = (DisplayName = "高度偏移 (cm)", DisplayPriority = 13))

	float VerticalOffsetCm = -5.f;

	UPROPERTY(EditAnywhere, Category = "道路影响", meta = (DisplayName = "样条采样间距 (cm)", ClampMin = "25", DisplayPriority = 14))

	float SampleDistanceCm = 1000.f;

	UPROPERTY(VisibleAnywhere, Category = "道路影响", meta = (

		DisplayName = "道路 Tag",

		DisplayPriority = 15,

		ToolTip = "生成道路时从面板写入，用于识别场景中的主路样条。"))

	FName RoadSplineTag;

	UPROPERTY(VisibleAnywhere, Category = "道路影响", meta = (DisplayName = "延伸距离 (cm)", DisplayPriority = 16))

	float ExtendLengthCm = 600.f;

	UPROPERTY(VisibleAnywhere, Category = "道路影响", meta = (
		DisplayName = "交叉点检测采样 (cm)",
		DisplayPriority = 17,
		ToolTip = "生成/拆分/烘焙时用于交叉口检测的样条采样间距。"))
	float IntersectionDetectSampleCm = 100.f;

	UPROPERTY(VisibleAnywhere, Category = "道路影响", meta = (DisplayName = "路段网格参数", DisplayPriority = 18))

	FQuick_RoadRoadRibbonBuildParams RibbonParams;

	UFUNCTION(CallInEditor, Category = "道路影响", meta = (DisplayName = "应用道路影响", DisplayPriority = 100))

	void ApplyRoadInfluence();

	UPROPERTY(EditAnywhere, Category = "边缘平滑", meta = (DisplayName = "平滑强度", ClampMin = "0", ClampMax = "1", DisplayPriority = 20))

	float EdgeSmoothStrength = 0.65f;

	UPROPERTY(EditAnywhere, Category = "边缘平滑", meta = (DisplayName = "平滑迭代", ClampMin = "1", ClampMax = "32", DisplayPriority = 21))

	int32 EdgeSmoothIterations = 4;

	UFUNCTION(CallInEditor, Category = "边缘平滑", meta = (DisplayName = "边缘平滑", DisplayPriority = 100))

	void SmoothRoadEdge();

private:

	void CollectOwnerProcMeshes(TArray<UProceduralMeshComponent*>& OutMeshes) const;

};
