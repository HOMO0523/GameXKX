// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "InteractiveTool.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "Quick_RoadLayoutDrawStageParams.h"
#include "Quick_RoadCityScopeTool.generated.h"

UCLASS(Transient)
class QUICK_ROAD_API UQuick_RoadCityScopeToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UQuick_RoadCityScopeToolProperties();

	UPROPERTY(EditAnywhere, Category = "区域", meta = (ShowOnlyInnerProperties, DisplayPriority = 10))
	FQuick_RoadLayoutCityScopeStageParams CityScopeStage;

	UFUNCTION(CallInEditor, Category = "区域", meta = (DisplayName = "绘制区域", DisplayPriority = 100))
	void DrawCityScope();

	UFUNCTION(CallInEditor, Category = "区域", meta = (DisplayName = "生成布局面片", DisplayPriority = 101))
	void GenerateCityScopeLayoutMesh();

	UFUNCTION(CallInEditor, Category = "区域", meta = (DisplayName = "阶梯化", DisplayPriority = 102))
	void TerraceCityScopeLayoutMesh();

	void SetOwningTool(class UQuick_RoadCityScopeTool* InTool) { OwningTool = InTool; }

private:
	UPROPERTY(Transient)
	TObjectPtr<class UQuick_RoadCityScopeTool> OwningTool;
};

UCLASS()
class QUICK_ROAD_API UQuick_RoadCityScopeToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

UCLASS()
class QUICK_ROAD_API UQuick_RoadCityScopeTool : public UInteractiveTool, public IClickDragBehaviorTarget, public IHoverBehaviorTarget
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
	void ExecuteGenerateCityScopeLayoutMesh();
	void ExecuteTerraceCityScopeLayoutMesh();

	bool IsDrawing() const { return bIsDrawing; }
	bool IsDragging() const { return bIsDragging; }
	int32 GetDrawPointCount() const { return DrawPoints.Num(); }
	UWorld* GetTargetWorld() const { return TargetWorld; }

protected:
	UPROPERTY()
	TObjectPtr<UQuick_RoadCityScopeToolProperties> Properties;

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
