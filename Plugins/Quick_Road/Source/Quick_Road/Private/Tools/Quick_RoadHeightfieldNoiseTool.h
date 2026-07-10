// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveTool.h"
#include "Tools/Quick_RoadHeightfieldNoise.h"
#include "Quick_RoadHeightfieldNoiseTool.generated.h"

UENUM(BlueprintType)
enum class EQuick_RoadHeightfieldNoiseTypeUI : uint8
{
	Perlin UMETA(DisplayName = "柏林"),
	Ridged UMETA(DisplayName = "尖脊"),
	Billowy UMETA(DisplayName = "圆润")
};

UENUM(BlueprintType)
enum class EQuick_RoadHeightfieldFractalTypeUI : uint8
{
	None UMETA(DisplayName = "无（单层）"),
	Standard UMETA(DisplayName = "标准分形"),
	Terrain UMETA(DisplayName = "地形"),
	HybridTerrain UMETA(DisplayName = "锐谷")
};

UENUM(BlueprintType)
enum class EQuick_RoadHeightfieldCombineModeUI : uint8
{
	Add UMETA(DisplayName = "叠加"),
	Replace UMETA(DisplayName = "替换"),
	Multiply UMETA(DisplayName = "倍增"),
	Maximum UMETA(DisplayName = "取高"),
	Minimum UMETA(DisplayName = "取低")
};

UCLASS(Transient)
class QUICK_ROAD_API UQuick_RoadHeightfieldNoiseToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UQuick_RoadHeightfieldNoiseToolProperties();

	UPROPERTY(EditAnywhere, Category = "噪声", meta = (DisplayName = "振幅", ClampMin = "0.0", ClampMax = "1000.0", ToolTip = "对应 Houdini Amplitude，控制垂直位移强度。"))
	float Amplitude;

	UPROPERTY(EditAnywhere, Category = "噪声", meta = (DisplayName = "元素尺寸", ClampMin = "1.0", ClampMax = "4096.0", ToolTip = "对应 Houdini Element Size，越大特征越大、越平滑。"))
	float ElementSize;

	UPROPERTY(EditAnywhere, Category = "噪声", meta = (DisplayName = "居中噪声", ToolTip = "对应 Houdini Center Noise，使噪声围绕 0 上下波动。"))
	bool bCenterNoise;

	UPROPERTY(EditAnywhere, Category = "噪声", meta = (DisplayName = "噪声类型"))
	EQuick_RoadHeightfieldNoiseTypeUI NoiseType;

	UPROPERTY(EditAnywhere, Category = "噪声", meta = (DisplayName = "分形类型"))
	EQuick_RoadHeightfieldFractalTypeUI FractalType;

	UPROPERTY(EditAnywhere, Category = "噪声", meta = (DisplayName = "最大倍频", ClampMin = "1", ClampMax = "16", ToolTip = "对应 Houdini Max Octaves。"))
	int32 MaxOctaves;

	UPROPERTY(EditAnywhere, Category = "噪声", meta = (DisplayName = "间隙度", ClampMin = "0.1", ClampMax = "8.0", ToolTip = "对应 Houdini Lacunarity，倍频间频率倍率。"))
	float Lacunarity;

	UPROPERTY(EditAnywhere, Category = "噪声", meta = (DisplayName = "粗糙度", ClampMin = "0.01", ClampMax = "1.0", ToolTip = "对应 Houdini Roughness，倍频间振幅衰减。"))
	float Roughness;

	UPROPERTY(EditAnywhere, Category = "噪声", meta = (DisplayName = "叠加模式"))
	EQuick_RoadHeightfieldCombineModeUI CombineMode;

	UPROPERTY(EditAnywhere, Category = "噪声", meta = (DisplayName = "随机种子"))
	int32 Seed;

	UPROPERTY(EditAnywhere, Category = "噪声", meta = (DisplayName = "偏移"))
	FVector2D Offset;

	UPROPERTY(EditAnywhere, Category = "噪声", meta = (DisplayName = "缩放"))
	FVector2D Scale;

	UFUNCTION(CallInEditor, Category = "噪声", meta = (DisplayName = "应用噪声", DisplayPriority = 100))
	void ApplyNoise();

	void SetOwningTool(class UQuick_RoadHeightfieldNoiseTool* InTool) { OwningTool = InTool; }

	FQuick_RoadHeightfieldNoiseParams BuildParams() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<class UQuick_RoadHeightfieldNoiseTool> OwningTool;
};

UCLASS()
class QUICK_ROAD_API UQuick_RoadHeightfieldNoiseToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UCLASS()
class QUICK_ROAD_API UQuick_RoadHeightfieldNoiseTool : public UInteractiveTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;

	void ExecuteApplyNoise();

protected:
	UPROPERTY()
	TObjectPtr<UQuick_RoadHeightfieldNoiseToolProperties> Properties;
};
