// Copyright Epic Games, Inc. All Rights Reserved.



#pragma once



#include "CoreMinimal.h"

#include "InteractiveToolBuilder.h"

#include "InteractiveTool.h"

#include "Quick_RoadBakeTool.generated.h"



UCLASS(Transient)

class QUICK_ROAD_API UQuick_RoadBakeToolProperties : public UInteractiveToolPropertySet

{

	GENERATED_BODY()



public:

	UQuick_RoadBakeToolProperties();



	UPROPERTY(EditAnywhere, Category = "烘焙", meta = (

		DisplayName = "保存路径",

		ContentDir,

		ToolTip = "烘焙后的 StaticMesh 与蓝图资产保存目录（Content 目录下）。",

		DisplayPriority = 10))

	FDirectoryPath SaveDirectory;



	UPROPERTY(EditAnywhere, Category = "烘焙", meta = (

		DisplayName = "直线路段拆分距离 (cm)",

		ClampMin = "0",

		ToolTip = "步骤二：对已烘焙的 RoadMesh StaticMesh 按样条弧长距离拆分；0 表示不拆分。",

		DisplayPriority = 11))

	float StraightSegmentSplitDistanceCm = 5000.f;



	UPROPERTY(EditAnywhere, Category = "烘焙", meta = (

		DisplayName = "删除源",

		ToolTip = "烘焙成功后删除选中的 MainRoadNetwork Actor。",

		DisplayPriority = 12))

	bool bDeleteSourceProcMesh = true;



	UFUNCTION(CallInEditor, Category = "烘焙", meta = (DisplayName = "烘焙静态网格体", DisplayPriority = 100))

	void BakeStaticMeshes();



	void SetOwningTool(class UQuick_RoadBakeTool* InTool) { OwningTool = InTool; }



private:

	UPROPERTY(Transient)

	TObjectPtr<class UQuick_RoadBakeTool> OwningTool;

};



UCLASS()

class QUICK_ROAD_API UQuick_RoadBakeToolBuilder : public UInteractiveToolBuilder

{

	GENERATED_BODY()



public:

	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }

	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;

};



UCLASS()

class QUICK_ROAD_API UQuick_RoadBakeTool : public UInteractiveTool

{

	GENERATED_BODY()



public:

	void SetWorld(UWorld* World);

	UWorld* GetTargetWorld() const { return TargetWorld; }



	virtual void Setup() override;



	void ExecuteBakeStaticMeshes();



protected:

	UPROPERTY()

	TObjectPtr<UQuick_RoadBakeToolProperties> Properties;



	UWorld* TargetWorld = nullptr;

};


