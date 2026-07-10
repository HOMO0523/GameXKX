// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveTool.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "Quick_RoadLayoutDrawStageParams.h"
#include "Quick_RoadMainRoadTool.generated.h"

UCLASS(Transient)
class QUICK_ROAD_API UQuick_RoadMainRoadToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UQuick_RoadMainRoadToolProperties();

	UPROPERTY(EditAnywhere, Category = "1 道路", meta = (DisplayName = "参数", DisplayPriority = 10))
	FQuick_RoadLayoutMainRoadStageParams MainRoadStage;

	UFUNCTION(CallInEditor, Category = "1 道路", meta = (DisplayName = "绘制道路", DisplayPriority = 100))
	void DrawMainRoad();

	UFUNCTION(CallInEditor, Category = "1 道路", meta = (DisplayName = "生成道路", DisplayPriority = 101))
	void GenerateMainRoad();

	UPROPERTY(EditAnywhere, Category = "2 交叉口", meta = (DisplayName = "参数", DisplayPriority = 50))
	FQuick_RoadLayoutMainRoadIntersectionParams IntersectionStage;

	UPROPERTY(VisibleAnywhere, Category = "2 交叉口", meta = (DisplayName = "已检测交叉点", DisplayPriority = 60))
	int32 DetectedIntersectionCount = 0;

	UFUNCTION(CallInEditor, Category = "2 交叉口", meta = (DisplayName = "检测交叉点", DisplayPriority = 100))
	void RefreshIntersectionPreview();

	UFUNCTION(CallInEditor, Category = "2 交叉口", meta = (DisplayName = "拆开并重构", DisplayPriority = 101))
	void SplitAndRebuildIntersections();

	void SetOwningTool(class UQuick_RoadMainRoadTool* InTool) { OwningTool = InTool; }

private:
	UPROPERTY(Transient)
	TObjectPtr<class UQuick_RoadMainRoadTool> OwningTool;
};

UCLASS()
class QUICK_ROAD_API UQuick_RoadMainRoadToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UCLASS()
class QUICK_ROAD_API UQuick_RoadMainRoadTool : public UInteractiveTool, public IClickDragBehaviorTarget, public IHoverBehaviorTarget
{
	GENERATED_BODY()

public:
	void SetWorld(UWorld* World);

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& PressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& DragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& ReleasePos) override;
	virtual void OnTerminateDragSequence() override;

	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override {}
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	virtual void OnEndHover() override;

	void ExecuteStartDrawing();
	void ExecuteGenerateMainRoad();
	void ExecuteRefreshIntersectionPreview();
	void ExecuteSplitAndRebuildIntersections();

	bool IsDrawing() const { return bIsDrawing; }
	bool IsDragging() const { return bIsDragging; }
	int32 GetDrawPointCount() const { return DrawPoints.Num(); }
	UWorld* GetTargetWorld() const { return TargetWorld; }
	bool HasValidMainRoadTag() const;

protected:
	UPROPERTY()
	TObjectPtr<UQuick_RoadMainRoadToolProperties> Properties;

	UWorld* TargetWorld = nullptr;

	TArray<FVector> DrawPoints;
	FVector PreviewPoint = FVector::ZeroVector;
	bool bHasValidPreview = false;
	bool bIsDrawing = false;
	bool bIsDragging = false;

	FInputRayHit FindGroundHit(const FRay& WorldRay, FVector& OutHitPoint) const;
	void AppendDrawPoint(const FVector& NewPoint, bool bIncludeEndpoint = false);
	bool TryCompleteStroke(bool bShowErrorDialog);
	bool SpawnSplineActorFromDrawPoints(FText& OutErrorMessage);
	void DrawPreviewLines(FPrimitiveDrawInterface* PDI) const;
};
