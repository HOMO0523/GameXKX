// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/Quick_RoadCityScopeTool.h"
#include "Tools/Quick_RoadLayoutMeshGenerator.h"
#include "Quick_RoadLayoutSplineTags.h"

#include "InteractiveToolManager.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"

#include "CollisionQueryParams.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "Editor.h"
#include "Misc/MessageDialog.h"
#include "ScopedTransaction.h"
#include "SceneManagement.h"

#define LOCTEXT_NAMESPACE "Quick_RoadCityScopeTool"

namespace Quick_RoadCityScopeToolPrivate
{
	static const FLinearColor PreviewLineColor(0.1f, 0.85f, 1.0f, 1.0f);
	static const FLinearColor PreviewPointColor(1.0f, 0.55f, 0.1f, 1.0f);

	static void ApplyLayoutSplineTags(AActor* SplineActor, USplineComponent* SplineComponent)
	{
		if (SplineActor)
		{
			SplineActor->Tags.AddUnique(Quick_RoadLayoutSplineTags::LayoutSpline);
			SplineActor->Tags.AddUnique(Quick_RoadLayoutSplineTags::CityScope);
		}

		if (SplineComponent)
		{
			SplineComponent->ComponentTags.AddUnique(Quick_RoadLayoutSplineTags::LayoutSpline);
			SplineComponent->ComponentTags.AddUnique(Quick_RoadLayoutSplineTags::CityScope);
		}
	}
}

UQuick_RoadCityScopeToolProperties::UQuick_RoadCityScopeToolProperties() = default;

void UQuick_RoadCityScopeToolProperties::DrawCityScope()
{
	if (OwningTool)
	{
		OwningTool->ExecuteStartDrawing();
	}
}

void UQuick_RoadCityScopeToolProperties::GenerateCityScopeLayoutMesh()
{
	if (OwningTool)
	{
		OwningTool->ExecuteGenerateCityScopeLayoutMesh();
	}
}

void UQuick_RoadCityScopeToolProperties::TerraceCityScopeLayoutMesh()
{
	if (OwningTool)
	{
		OwningTool->ExecuteTerraceCityScopeLayoutMesh();
	}
}

UInteractiveTool* UQuick_RoadCityScopeToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UQuick_RoadCityScopeTool* NewTool = NewObject<UQuick_RoadCityScopeTool>(SceneState.ToolManager);
	NewTool->SetWorld(SceneState.World);
	return NewTool;
}

void UQuick_RoadCityScopeTool::SetWorld(UWorld* World)
{
	TargetWorld = World;
}

void UQuick_RoadCityScopeTool::Setup()
{
	Properties = NewObject<UQuick_RoadCityScopeToolProperties>(this);
	Properties->SetOwningTool(this);
	AddToolPropertySource(Properties);
	UMouseHoverBehavior* HoverBehavior = NewObject<UMouseHoverBehavior>();
	HoverBehavior->Initialize(this);
	AddInputBehavior(HoverBehavior);

	UClickDragInputBehavior* ClickBehavior = NewObject<UClickDragInputBehavior>();
	ClickBehavior->Initialize(this);
	AddInputBehavior(ClickBehavior);
}

void UQuick_RoadCityScopeTool::Shutdown(EToolShutdownType ShutdownType)
{
	bIsDrawing = false;
	bIsDragging = false;
	DrawPoints.Reset();
	bHasValidPreview = false;

	Super::Shutdown(ShutdownType);
}

FInputRayHit UQuick_RoadCityScopeTool::FindGroundHit(const FRay& WorldRay, FVector& OutHitPoint) const
{
	if (!TargetWorld)
	{
		return FInputRayHit();
	}

	FCollisionObjectQueryParams QueryParams(FCollisionObjectQueryParams::AllObjects);
	FHitResult HitResult;
	if (TargetWorld->LineTraceSingleByObjectType(
		HitResult,
		WorldRay.Origin,
		WorldRay.PointAt(99999999.f),
		QueryParams))
	{
		OutHitPoint = HitResult.ImpactPoint;
		return FInputRayHit(HitResult.Distance);
	}

	return FInputRayHit();
}

void UQuick_RoadCityScopeTool::AppendDrawPoint(const FVector& NewPoint, bool bIncludeEndpoint)
{
	const float MinDistance = Properties
		? FMath::Max(10.f, Properties->CityScopeStage.SampleDistanceCm)
		: 100.f;

	const int32 PointsBefore = DrawPoints.Num();
	if (DrawPoints.Num() == 0)
	{
		DrawPoints.Add(NewPoint);
	}
	else
	{
		const FVector FromPoint = DrawPoints.Last();
		const FVector Segment = NewPoint - FromPoint;
		const float SegmentLength = Segment.Size();
		if (SegmentLength >= MinDistance)
		{
			const FVector Direction = Segment / SegmentLength;
			for (float DistanceAlong = MinDistance; DistanceAlong <= SegmentLength; DistanceAlong += MinDistance)
			{
				DrawPoints.Add(FromPoint + Direction * DistanceAlong);
			}
		}

		if (bIncludeEndpoint && SegmentLength > KINDA_SMALL_NUMBER
			&& FVector::Dist(DrawPoints.Last(), NewPoint) > KINDA_SMALL_NUMBER)
		{
			DrawPoints.Add(NewPoint);
		}
	}

	if (DrawPoints.Num() != PointsBefore)
	{
	}

	PreviewPoint = NewPoint;
	bHasValidPreview = true;
}

FInputRayHit UQuick_RoadCityScopeTool::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	if (!bIsDrawing)
	{
		return FInputRayHit();
	}

	FVector HitPoint;
	return FindGroundHit(PressPos.WorldRay, HitPoint);
}

void UQuick_RoadCityScopeTool::OnClickPress(const FInputDeviceRay& PressPos)
{
	if (!bIsDrawing || bIsDragging)
	{
		return;
	}

	FVector HitPoint;
	if (!FindGroundHit(PressPos.WorldRay, HitPoint).bHit)
	{
		return;
	}

	bIsDragging = true;
	DrawPoints.Reset();
	AppendDrawPoint(HitPoint);
}

void UQuick_RoadCityScopeTool::OnClickDrag(const FInputDeviceRay& DragPos)
{
	if (!bIsDrawing || !bIsDragging)
	{
		return;
	}

	FVector HitPoint;
	if (FindGroundHit(DragPos.WorldRay, HitPoint).bHit)
	{
		AppendDrawPoint(HitPoint);
	}
}

void UQuick_RoadCityScopeTool::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	if (!bIsDragging)
	{
		return;
	}

	bIsDragging = false;

	FVector HitPoint;
	if (FindGroundHit(ReleasePos.WorldRay, HitPoint).bHit)
	{
		AppendDrawPoint(HitPoint, true);
	}

	TryCompleteStroke(true);
}

void UQuick_RoadCityScopeTool::OnTerminateDragSequence()
{
	if (!bIsDragging)
	{
		return;
	}

	bIsDragging = false;
	DrawPoints.Reset();
	bHasValidPreview = false;
}

FInputRayHit UQuick_RoadCityScopeTool::BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos)
{
	if (!bIsDrawing)
	{
		return FInputRayHit();
	}

	FVector HitPoint;
	return FindGroundHit(PressPos.WorldRay, HitPoint);
}

bool UQuick_RoadCityScopeTool::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	if (!bIsDrawing)
	{
		return false;
	}

	FVector HitPoint;
	if (FindGroundHit(DevicePos.WorldRay, HitPoint).bHit)
	{
		PreviewPoint = HitPoint;
		bHasValidPreview = true;
		return true;
	}

	return false;
}

void UQuick_RoadCityScopeTool::OnEndHover()
{
	bHasValidPreview = false;
}

void UQuick_RoadCityScopeTool::ExecuteStartDrawing()
{
	if (!TargetWorld)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoWorldMsg", "当前没有可用的编辑世界。"));
		return;
	}

	bIsDrawing = true;
	bIsDragging = false;
	DrawPoints.Reset();
	bHasValidPreview = false;
}

void UQuick_RoadCityScopeTool::ExecuteGenerateCityScopeLayoutMesh()
{
	if (!Properties || !TargetWorld)
	{
		return;
	}

	USplineComponent* CityScopeSpline = nullptr;
	FText ErrorMessage;
	if (!FQuick_RoadLayoutMeshGenerator::FindCityScopeSpline(TargetWorld, CityScopeSpline, ErrorMessage))
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		return;
	}

	FQuick_RoadLayoutMeshParams MeshParams;
	MeshParams.Subdivisions = FMath::Max(Properties->CityScopeStage.LayoutMeshSubdivisions, 1);
	MeshParams.MaxSlopeAngleDegrees = Properties->CityScopeStage.MaxSlopeAngleDegrees;
	MeshParams.TerrainBlendStrength = FMath::Clamp(Properties->CityScopeStage.TerrainBlendStrength, 0.f, 1.f);
	MeshParams.DefaultMaterial = Properties->CityScopeStage.DefaultMaterial;

	FQuick_RoadLayoutMeshResult Result;
	if (!FQuick_RoadLayoutMeshGenerator::GenerateFromCityScope(
		TargetWorld,
		CityScopeSpline,
		MeshParams,
		Result,
		ErrorMessage))
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		return;
	}

	FMessageDialog::Open(
		EAppMsgType::Ok,
		FText::Format(
			LOCTEXT("GenerateLayoutMeshSuccess", "布局面片已生成。\n顶点：{0}，三角面：{1}\n最大斜坡角度：{2}°"),
			FText::AsNumber(Result.VertexCount),
			FText::AsNumber(Result.TriangleCount),
			FText::AsNumber(MeshParams.MaxSlopeAngleDegrees)));
}

void UQuick_RoadCityScopeTool::ExecuteTerraceCityScopeLayoutMesh()
{
	if (!Properties || !TargetWorld)
	{
		return;
	}

	FQuick_RoadLayoutTerraceParams TerraceParams;
	TerraceParams.StepSizeCm = FMath::Max(Properties->CityScopeStage.TerraceStepSizeCm, 10.f);
	TerraceParams.bAutoHeightRange = Properties->CityScopeStage.bTerraceAutoHeightRange;
	TerraceParams.MinHeightCm = Properties->CityScopeStage.TerraceMinHeightCm;
	TerraceParams.MaxHeightCm = Properties->CityScopeStage.TerraceMaxHeightCm;
	TerraceParams.Fade = FMath::Clamp(Properties->CityScopeStage.TerraceFade, 0.f, 1.f);
	TerraceParams.SmoothEdges = FMath::Clamp(Properties->CityScopeStage.TerraceSmoothEdges, 0.f, 1.f);

	FQuick_RoadLayoutMeshResult Result;
	FText ErrorMessage;
	if (!FQuick_RoadLayoutMeshGenerator::ApplyTerraceToLayoutMesh(
		TargetWorld,
		TerraceParams,
		Result,
		ErrorMessage))
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		return;
	}

	FMessageDialog::Open(
		EAppMsgType::Ok,
		FText::Format(
			LOCTEXT("TerraceLayoutMeshSuccess", "布局面片已阶梯化。\n顶点：{0}，三角面：{1}\n台阶高度：{2} cm"),
			FText::AsNumber(Result.VertexCount),
			FText::AsNumber(Result.TriangleCount),
			FText::AsNumber(TerraceParams.StepSizeCm)));
}

bool UQuick_RoadCityScopeTool::TryCompleteStroke(bool bShowErrorDialog)
{
	const int32 MinPoints = 3;
	if (DrawPoints.Num() < MinPoints)
	{
		if (bShowErrorDialog)
		{
			FMessageDialog::Open(
				EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("NotEnoughPoints", "至少需要 {0} 个控制点才能生成区域。"),
					MinPoints));
		}

		DrawPoints.Reset();
		bHasValidPreview = false;
		return false;
	}

	FText ErrorMessage;
	if (!SpawnSplineActorFromDrawPoints(ErrorMessage))
	{
		if (bShowErrorDialog)
		{
			FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		}
		return false;
	}

	bIsDrawing = false;
	bIsDragging = false;
	DrawPoints.Reset();
	bHasValidPreview = false;
	return true;
}

bool UQuick_RoadCityScopeTool::SpawnSplineActorFromDrawPoints(FText& OutErrorMessage)
{
	if (!TargetWorld || DrawPoints.Num() == 0)
	{
		OutErrorMessage = LOCTEXT("InvalidState", "没有可用的绘制点。");
		return false;
	}

	const FVector PivotLocation = DrawPoints[0];
	const FScopedTransaction Transaction(LOCTEXT("CreateCityScopeSpline", "Create City Scope Spline"));

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags = RF_Transactional;

	AActor* SplineActor = TargetWorld->SpawnActor<AActor>(
		AActor::StaticClass(),
		PivotLocation,
		FRotator::ZeroRotator,
		SpawnParams);

	if (!SplineActor)
	{
		OutErrorMessage = LOCTEXT("SpawnFailed", "无法创建 Spline Actor。");
		return false;
	}

	SplineActor->Modify();
	SplineActor->SetActorLabel(TEXT("CityScope"));

	USplineComponent* SplineComponent = NewObject<USplineComponent>(
		SplineActor,
		USplineComponent::StaticClass(),
		TEXT("SplineComponent"),
		RF_Transactional);
	SplineComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	SplineComponent->SetMobility(EComponentMobility::Static);
	SplineComponent->SetRelativeTransform(FTransform::Identity);

	SplineActor->SetRootComponent(SplineComponent);
	SplineActor->AddInstanceComponent(SplineComponent);
	SplineComponent->RegisterComponent();

	SplineActor->SetActorLocation(PivotLocation, false, nullptr, ETeleportType::TeleportPhysics);

	SplineComponent->Modify();
	SplineComponent->ClearSplinePoints(false);
	SplineComponent->SetSplinePoints(DrawPoints, ESplineCoordinateSpace::World, false);
	SplineComponent->SetClosedLoop(true, false);
	SplineComponent->bSplineHasBeenEdited = true;
	SplineComponent->UpdateSpline();

	Quick_RoadCityScopeToolPrivate::ApplyLayoutSplineTags(SplineActor, SplineComponent);

	if (GEditor)
	{
		GEditor->SelectNone(false, true, false);
		GEditor->SelectActor(SplineActor, true, true, true);
	}

	return true;
}

void UQuick_RoadCityScopeTool::DrawPreviewLines(FPrimitiveDrawInterface* PDI) const
{
	using namespace Quick_RoadCityScopeToolPrivate;

	const int32 PointCount = DrawPoints.Num();
	if (PointCount == 0)
	{
		return;
	}

	for (int32 Index = 0; Index < PointCount - 1; ++Index)
	{
		PDI->DrawLine(DrawPoints[Index], DrawPoints[Index + 1], PreviewLineColor, SDPG_Foreground, 2.0f, 0.0f, true);
		PDI->DrawLine(DrawPoints[Index], DrawPoints[Index + 1], PreviewLineColor, SDPG_World, 4.0f, 0.0f, true);
	}

	if (PointCount >= 3)
	{
		PDI->DrawLine(DrawPoints.Last(), DrawPoints[0], PreviewLineColor, SDPG_Foreground, 2.0f, 0.0f, true);
		PDI->DrawLine(DrawPoints.Last(), DrawPoints[0], PreviewLineColor, SDPG_World, 4.0f, 0.0f, true);
	}

	if (bHasValidPreview && PointCount > 0)
	{
		PDI->DrawLine(DrawPoints.Last(), PreviewPoint, PreviewLineColor, SDPG_Foreground, 1.5f, 0.0f, true);
	}

	for (const FVector& Point : DrawPoints)
	{
		PDI->DrawPoint(Point, PreviewPointColor, 10.0f, SDPG_Foreground);
	}
}

void UQuick_RoadCityScopeTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	if (!bIsDrawing && !bIsDragging)
	{
		return;
	}

	if (DrawPoints.Num() == 0)
	{
		return;
	}

	DrawPreviewLines(RenderAPI->GetPrimitiveDrawInterface());
}

#undef LOCTEXT_NAMESPACE
