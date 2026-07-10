// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveTool.h"
#include "Quick_RoadAutoErosionTool.generated.h"

UENUM(BlueprintType)
enum class EQuick_RoadErosionAlgorithm : uint8
{
	HydraulicParticle UMETA(DisplayName = "水力侵蚀（粒子）"),
	ThermalTalus UMETA(DisplayName = "热侵蚀（休止角）"),
	Combined UMETA(DisplayName = "水力 + 热（组合）")
};

UCLASS(Transient)
class QUICK_ROAD_API UQuick_RoadAutoErosionToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UQuick_RoadAutoErosionToolProperties();

	UPROPERTY(EditAnywhere, Category = "侵蚀", meta = (DisplayName = "侵蚀算法"))
	EQuick_RoadErosionAlgorithm ErosionAlgorithm;

	UPROPERTY(EditAnywhere, Category = "侵蚀", meta = (DisplayName = "随机种子"))
	int32 Seed;

	UPROPERTY(EditAnywhere, Category = "水力侵蚀", meta = (DisplayName = "迭代次数", EditCondition = "ErosionAlgorithm == EQuick_RoadErosionAlgorithm::HydraulicParticle || ErosionAlgorithm == EQuick_RoadErosionAlgorithm::Combined", ClampMin = "1", ClampMax = "200"))
	int32 HydraulicIterations;

	UPROPERTY(EditAnywhere, Category = "水力侵蚀", meta = (DisplayName = "每轮粒子数", EditCondition = "ErosionAlgorithm == EQuick_RoadErosionAlgorithm::HydraulicParticle || ErosionAlgorithm == EQuick_RoadErosionAlgorithm::Combined", ClampMin = "500", ClampMax = "100000"))
	int32 DropletsPerIteration;

	UPROPERTY(EditAnywhere, Category = "水力侵蚀", meta = (DisplayName = "方向惯性", EditCondition = "ErosionAlgorithm == EQuick_RoadErosionAlgorithm::HydraulicParticle || ErosionAlgorithm == EQuick_RoadErosionAlgorithm::Combined", ClampMin = "0.0", ClampMax = "0.95", ToolTip = "越高越保持流向，沟谷更长、更连贯。"))
	float Inertia;

	UPROPERTY(EditAnywhere, Category = "水力侵蚀", meta = (DisplayName = "粒子最大步数", EditCondition = "ErosionAlgorithm == EQuick_RoadErosionAlgorithm::HydraulicParticle || ErosionAlgorithm == EQuick_RoadErosionAlgorithm::Combined", ClampMin = "8", ClampMax = "256", ToolTip = "单粒子最大行进步数，越大水系延伸越远。"))
	int32 MaxDropletSteps;

	UPROPERTY(EditAnywhere, Category = "水力侵蚀", meta = (DisplayName = "侵蚀率", EditCondition = "ErosionAlgorithm == EQuick_RoadErosionAlgorithm::HydraulicParticle || ErosionAlgorithm == EQuick_RoadErosionAlgorithm::Combined", ClampMin = "0.01", ClampMax = "1.0"))
	float ErosionRate;

	UPROPERTY(EditAnywhere, Category = "水力侵蚀", meta = (DisplayName = "沉积率", EditCondition = "ErosionAlgorithm == EQuick_RoadErosionAlgorithm::HydraulicParticle || ErosionAlgorithm == EQuick_RoadErosionAlgorithm::Combined", ClampMin = "0.01", ClampMax = "1.0"))
	float DepositionRate;

	UPROPERTY(EditAnywhere, Category = "水力侵蚀", meta = (DisplayName = "泥沙容量", EditCondition = "ErosionAlgorithm == EQuick_RoadErosionAlgorithm::HydraulicParticle || ErosionAlgorithm == EQuick_RoadErosionAlgorithm::Combined", ClampMin = "0.1", ClampMax = "20.0"))
	float SedimentCapacity;

	UPROPERTY(EditAnywhere, Category = "水力侵蚀", meta = (DisplayName = "蒸发率", EditCondition = "ErosionAlgorithm == EQuick_RoadErosionAlgorithm::HydraulicParticle || ErosionAlgorithm == EQuick_RoadErosionAlgorithm::Combined", ClampMin = "0.0", ClampMax = "0.2"))
	float Evaporation;

	UPROPERTY(EditAnywhere, Category = "热侵蚀", meta = (DisplayName = "迭代次数", EditCondition = "ErosionAlgorithm == EQuick_RoadErosionAlgorithm::ThermalTalus || ErosionAlgorithm == EQuick_RoadErosionAlgorithm::Combined", ClampMin = "1", ClampMax = "200"))
	int32 ThermalIterations;

	UPROPERTY(EditAnywhere, Category = "热侵蚀", meta = (DisplayName = "休止角阈值", EditCondition = "ErosionAlgorithm == EQuick_RoadErosionAlgorithm::ThermalTalus || ErosionAlgorithm == EQuick_RoadErosionAlgorithm::Combined", ClampMin = "0.01", ClampMax = "5.0"))
	float ThermalTalusThreshold;

	UPROPERTY(EditAnywhere, Category = "热侵蚀", meta = (DisplayName = "强度", EditCondition = "ErosionAlgorithm == EQuick_RoadErosionAlgorithm::ThermalTalus || ErosionAlgorithm == EQuick_RoadErosionAlgorithm::Combined", ClampMin = "0.01", ClampMax = "1.0"))
	float ThermalStrength;

	UPROPERTY(EditAnywhere, Category = "宏观后处理", meta = (DisplayName = "宏观平滑次数", ClampMin = "0", ClampMax = "8", ToolTip = "侵蚀后平滑细碎噪点。与峰顶锐化互斥，想变尖请设为 0。"))
	int32 MacroSmoothPasses;

	UPROPERTY(EditAnywhere, Category = "宏观后处理", meta = (DisplayName = "峰顶锐化强度", ClampMin = "0.0", ClampMax = "2.0", ToolTip = "抬高山头/山脊、加深沟谷，增强轮廓棱角。0 关闭。"))
	float PeakSharpenStrength;

	UPROPERTY(EditAnywhere, Category = "宏观后处理", meta = (DisplayName = "峰顶锐化次数", ClampMin = "0", ClampMax = "8", ToolTip = "锐化迭代次数，通常 2–4 即可。"))
	int32 PeakSharpenPasses;

	UFUNCTION(CallInEditor, Category = "宏观后处理", meta = (DisplayName = "开始侵蚀", DisplayPriority = 100))
	void StartErosion();

	void SetOwningTool(class UQuick_RoadAutoErosionTool* InTool) { OwningTool = InTool; }

private:
	UPROPERTY(Transient)
	TObjectPtr<class UQuick_RoadAutoErosionTool> OwningTool;
};

UCLASS()
class QUICK_ROAD_API UQuick_RoadAutoErosionToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UCLASS()
class QUICK_ROAD_API UQuick_RoadAutoErosionTool : public UInteractiveTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;

	void ExecuteStartErosion();

protected:
	UPROPERTY()
	TObjectPtr<UQuick_RoadAutoErosionToolProperties> Properties;
};
