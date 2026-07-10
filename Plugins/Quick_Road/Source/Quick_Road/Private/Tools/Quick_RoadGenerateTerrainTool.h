// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveTool.h"
#include "Quick_RoadGenerateTerrainTool.generated.h"

UENUM(BlueprintType)
enum class EQuick_RoadLandscapeQuadsPerSection : uint8
{
	None = 0 UMETA(Hidden),
	Quads7 = 7 UMETA(DisplayName = "7×7 四边形"),
	Quads15 = 15 UMETA(DisplayName = "15×15 四边形"),
	Quads31 = 31 UMETA(DisplayName = "31×31 四边形"),
	Quads63 = 63 UMETA(DisplayName = "63×63 四边形"),
	Quads127 = 127 UMETA(DisplayName = "127×127 四边形"),
	Quads255 = 255 UMETA(DisplayName = "255×255 四边形")
};

UENUM(BlueprintType)
enum class EQuick_RoadLandscapeSectionsPerComponent : uint8
{
	None = 0 UMETA(Hidden),
	Section1x1 = 1 UMETA(DisplayName = "1×1 区块"),
	Section2x2 = 2 UMETA(DisplayName = "2×2 区块")
};

UCLASS(Transient)
class QUICK_ROAD_API UQuick_RoadGenerateTerrainToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UQuick_RoadGenerateTerrainToolProperties();

	UPROPERTY(EditAnywhere, Category = "地形", meta = (DisplayName = "材质"))
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, Category = "地形", meta = (DisplayName = "区块尺寸"))
	EQuick_RoadLandscapeQuadsPerSection QuadsPerSection;

	UPROPERTY(EditAnywhere, Category = "地形", meta = (DisplayName = "每组件区块数"))
	EQuick_RoadLandscapeSectionsPerComponent SectionsPerComponent;

	UPROPERTY(EditAnywhere, Category = "地形", meta = (DisplayName = "组件数量", ClampMin = "1", ClampMax = "32"))
	FIntPoint ComponentCount;

	UPROPERTY(VisibleAnywhere, Category = "地形", meta = (DisplayName = "总分辨率 X"))
	int32 OverallResolutionX = 0;

	UPROPERTY(VisibleAnywhere, Category = "地形", meta = (DisplayName = "总分辨率 Y"))
	int32 OverallResolutionY = 0;

	UPROPERTY(VisibleAnywhere, Category = "地形", meta = (DisplayName = "组件总数"))
	int32 TotalComponents = 0;

	UPROPERTY(EditAnywhere, Category = "高度图", meta = (
		DisplayName = "高度图文件",
		FilePathFilter = "高度图 (*.png;*.raw)|*.png;*.raw|所有文件 (*.*)|*.*"))
	FFilePath HeightmapFilePath;

	UFUNCTION(CallInEditor, Category = "高度图", meta = (DisplayName = "创建地形", DisplayPriority = 100))
	void CreateLandscape();

	void SetOwningTool(class UQuick_RoadGenerateTerrainTool* InTool) { OwningTool = InTool; }
	void RefreshComputedValues();

private:
	UPROPERTY(Transient)
	TObjectPtr<class UQuick_RoadGenerateTerrainTool> OwningTool;
};

UCLASS()
class QUICK_ROAD_API UQuick_RoadGenerateTerrainToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UCLASS()
class QUICK_ROAD_API UQuick_RoadGenerateTerrainTool : public UInteractiveTool
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	void ExecuteCreateLandscape();

protected:
	UPROPERTY()
	TObjectPtr<UQuick_RoadGenerateTerrainToolProperties> Properties;

	UWorld* TargetWorld = nullptr;
};
