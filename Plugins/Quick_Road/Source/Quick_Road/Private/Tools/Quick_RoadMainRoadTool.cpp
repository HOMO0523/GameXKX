// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/Quick_RoadMainRoadTool.h"
#include "Tools/Quick_RoadLayoutRoadGenerator.h"
#include "Quick_RoadLayoutRoadTags.h"
#include "Quick_RoadLog.h"

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

#define LOCTEXT_NAMESPACE "Quick_RoadMainRoadTool"

namespace Quick_RoadMainRoadToolPrivate
{
	static const FLinearColor PreviewLineColor(0.1f, 0.85f, 1.0f, 1.0f);
	static const FLinearColor PreviewPointColor(1.0f, 0.55f, 0.1f, 1.0f);

	static void ApplyRoadSplineTags(AActor* SplineActor, USplineComponent* SplineComponent, FName UserRoadTag)
	{
		Quick_RoadLayoutRoadTags::ApplyUserRoadTagToSpline(SplineActor, SplineComponent, UserRoadTag);
	}
}

UQuick_RoadMainRoadToolProperties::UQuick_RoadMainRoadToolProperties() = default;

void UQuick_RoadMainRoadToolProperties::DrawMainRoad()
{
	if (OwningTool)
	{
		OwningTool->ExecuteStartDrawing();
	}
}

void UQuick_RoadMainRoadToolProperties::GenerateMainRoad()
{
	if (OwningTool)
	{
		OwningTool->ExecuteGenerateMainRoad();
	}
}

void UQuick_RoadMainRoadToolProperties::RefreshIntersectionPreview()
{
	if (OwningTool)
	{
		OwningTool->ExecuteRefreshIntersectionPreview();
	}
}

void UQuick_RoadMainRoadToolProperties::SplitAndRebuildIntersections()
{
	if (OwningTool)
	{
		OwningTool->ExecuteSplitAndRebuildIntersections();
	}
}

UInteractiveTool* UQuick_RoadMainRoadToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UQuick_RoadMainRoadTool* NewTool = NewObject<UQuick_RoadMainRoadTool>(SceneState.ToolManager);
	NewTool->SetWorld(SceneState.World);
	return NewTool;
}

void UQuick_RoadMainRoadTool::SetWorld(UWorld* World)
{
	TargetWorld = World;
}

void UQuick_RoadMainRoadTool::Setup()
{
	Properties = NewObject<UQuick_RoadMainRoadToolProperties>(this);
	Properties->SetOwningTool(this);
	AddToolPropertySource(Properties);
	UMouseHoverBehavior* HoverBehavior = NewObject<UMouseHoverBehavior>();
	HoverBehavior->Initialize(this);
	AddInputBehavior(HoverBehavior);

	UClickDragInputBehavior* ClickBehavior = NewObject<UClickDragInputBehavior>();
	ClickBehavior->Initialize(this);
	AddInputBehavior(ClickBehavior);
}

void UQuick_RoadMainRoadTool::Shutdown(EToolShutdownType ShutdownType)
{
	bIsDrawing = false;
	bIsDragging = false;
	DrawPoints.Reset();
	bHasValidPreview = false;

	Super::Shutdown(ShutdownType);
}

FInputRayHit UQuick_RoadMainRoadTool::FindGroundHit(const FRay& WorldRay, FVector& OutHitPoint) const
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

void UQuick_RoadMainRoadTool::AppendDrawPoint(const FVector& NewPoint, bool bIncludeEndpoint)
{
	const float MinDistance = Properties
		? FMath::Max(10.f, Properties->MainRoadStage.SampleDistanceCm)
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

FInputRayHit UQuick_RoadMainRoadTool::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	if (!bIsDrawing)
	{
		return FInputRayHit();
	}

	FVector HitPoint;
	return FindGroundHit(PressPos.WorldRay, HitPoint);
}

void UQuick_RoadMainRoadTool::OnClickPress(const FInputDeviceRay& PressPos)
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

void UQuick_RoadMainRoadTool::OnClickDrag(const FInputDeviceRay& DragPos)
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

void UQuick_RoadMainRoadTool::OnClickRelease(const FInputDeviceRay& ReleasePos)
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

void UQuick_RoadMainRoadTool::OnTerminateDragSequence()
{
	if (!bIsDragging)
	{
		return;
	}

	bIsDragging = false;
	DrawPoints.Reset();
	bHasValidPreview = false;
}

FInputRayHit UQuick_RoadMainRoadTool::BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos)
{
	if (!bIsDrawing)
	{
		return FInputRayHit();
	}

	FVector HitPoint;
	return FindGroundHit(PressPos.WorldRay, HitPoint);
}

bool UQuick_RoadMainRoadTool::OnUpdateHover(const FInputDeviceRay& DevicePos)
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

void UQuick_RoadMainRoadTool::OnEndHover()
{
	bHasValidPreview = false;
}

bool UQuick_RoadMainRoadTool::HasValidMainRoadTag() const
{
	return Properties && Quick_RoadLayoutRoadTags::IsValidUserRoadTag(Properties->MainRoadStage.RoadTag);
}

void UQuick_RoadMainRoadTool::ExecuteStartDrawing()
{
	if (!TargetWorld)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoWorldMsg", "当前没有可用的编辑世界。"));
		return;
	}

	if (!Properties || !Quick_RoadLayoutRoadTags::IsValidUserRoadTag(Properties->MainRoadStage.RoadTag))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("DrawNoRoadTag", "请先在「道路」的「道路 Tag」中填写有效 Tag。"));
		return;
	}

	bIsDrawing = true;
	bIsDragging = false;
	DrawPoints.Reset();
	bHasValidPreview = false;
}

void UQuick_RoadMainRoadTool::ExecuteGenerateMainRoad()
{
	if (!Properties || !TargetWorld)
	{
		return;
	}

	if (!Quick_RoadLayoutRoadTags::IsValidUserRoadTag(Properties->MainRoadStage.RoadTag))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("GenerateNoRoadTag", "请先在「道路」的「道路 Tag」中填写有效 Tag。"));
		return;
	}

	TArray<FQuick_RoadLayoutRoadResult> Results;
	FText ErrorMessage;
	if (!FQuick_RoadLayoutRoadGenerator::GenerateRoadMeshes(
		TargetWorld,
		Properties->MainRoadStage,
		Properties->IntersectionStage,
		Results,
		ErrorMessage))
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		return;
	}

	const FQuick_RoadLayoutRoadResult& Result = Results[0];
	Properties->IntersectionStage.RoadTag = Properties->MainRoadStage.RoadTag;
	NotifyOfPropertyChangeByTool(Properties);

	FMessageDialog::Open(
		EAppMsgType::Ok,
		FText::Format(
			LOCTEXT("GenerateMainRoadSuccess", "道路网络已生成（Ribbon 程序化网格）。\n道路 Tag：{0}\n路宽：{1} cm\n识别样条：{2} 条\nRibbon 段：{3}\n顶点：{4}，三角面：{5}\n可在下方「交叉口」中「拆开并重构」。"),
			FText::FromName(Properties->MainRoadStage.RoadTag),
			FText::AsNumber(Properties->MainRoadStage.MainRoadWidthCm),
			FText::AsNumber(Result.SplineCount),
			FText::AsNumber(Result.RibbonSegmentCount),
			FText::AsNumber(Result.VertexCount),
			FText::AsNumber(Result.TriangleCount)));
}

void UQuick_RoadMainRoadTool::ExecuteRefreshIntersectionPreview()
{
	if (!Properties || !TargetWorld)
	{
		return;
	}

	const FName RoadTagExpression = Properties->IntersectionStage.RoadTag;
	if (!Quick_RoadLayoutRoadTags::IsValidUserRoadTagExpression(RoadTagExpression))
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			LOCTEXT("PreviewNoRoadTag", "请先在「交叉口」的「道路 Tag」中填写有效 Tag 或 Tag 表达式（如 aa|dd）。"));
		return;
	}

	TArray<USplineComponent*> Splines;
	FQuick_RoadLayoutRoadGenerator::CollectMainRoadSplinesByTagExpression(TargetWorld, Splines, RoadTagExpression);
	if (Splines.Num() == 0)
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("PreviewNoSplines", "场景中未找到匹配 Tag 表达式「{0}」的样条线。请先点击「生成道路」。"),
				FText::FromName(RoadTagExpression)));
		return;
	}

	TArray<FQuick_RoadRoadIntersectionInfo> Intersections;
	Properties->DetectedIntersectionCount = FQuick_RoadLayoutRoadGenerator::DetectMainRoadIntersections(
		TargetWorld,
		RoadTagExpression,
		Properties->IntersectionStage.IntersectionDetectSampleCm,
		Intersections);

	FMessageDialog::Open(
		EAppMsgType::Ok,
		FText::Format(
			LOCTEXT("PreviewSuccess", "交叉点检测完成。\n道路 Tag：{0}\n样条：{1} 条\n交叉点：{2} 个\n延伸距离：{3} cm"),
			FText::FromName(RoadTagExpression),
			FText::AsNumber(Splines.Num()),
			FText::AsNumber(Properties->DetectedIntersectionCount),
			FText::AsNumber(Properties->IntersectionStage.ExtendLengthCm)));

	UE_LOG(
		LogQuickRoad,
		Display,
		TEXT("Quick_Road 交叉口检测（Tag=%s）：%d 条样条，%d 个交叉点（延伸距离 %g cm）"),
		*RoadTagExpression.ToString(),
		Splines.Num(),
		Properties->DetectedIntersectionCount,
		Properties->IntersectionStage.ExtendLengthCm);

}

void UQuick_RoadMainRoadTool::ExecuteSplitAndRebuildIntersections()
{
	if (!Properties || !TargetWorld)
	{
		return;
	}

	const FName RoadTagExpression = Properties->IntersectionStage.RoadTag;
	if (!Quick_RoadLayoutRoadTags::IsValidUserRoadTagExpression(RoadTagExpression))
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			LOCTEXT("SplitNoRoadTag", "请先在「交叉口」填写有效道路 Tag 或 Tag 表达式（如 aa|dd）。"));
		return;
	}

	const FQuick_RoadRoadSplitRebuildSettings Settings =
		FQuick_RoadLayoutRoadGenerator::MakeSplitRebuildSettingsFromStage(
			Properties->MainRoadStage,
			Properties->IntersectionStage);

	FText ErrorMessage;
	int32 RemovedNetworkCount = 0;
	int32 PatchCount = 0;
	int32 RibbonSegments = 0;
	if (!FQuick_RoadLayoutRoadGenerator::SplitAndRebuildConsolidatedRoadNetworks(
		TargetWorld,
		RoadTagExpression,
		Settings,
		ErrorMessage,
		&RemovedNetworkCount,
		&PatchCount,
		&RibbonSegments))
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		return;
	}

	Properties->DetectedIntersectionCount = PatchCount;
	FMessageDialog::Open(
		EAppMsgType::Ok,
		FText::Format(
			LOCTEXT(
				"SplitSuccess",
				"交叉口已拆分重构。\n道路 Tag：{0}\n保留道路网络：1 个\n删除旧道路网络：{1} 个\n路口补丁：{2} 个\n样条拓扑重建：{3}\n路段 Ribbon 段：{4}"),
			FText::FromName(RoadTagExpression),
			FText::AsNumber(RemovedNetworkCount),
			FText::AsNumber(PatchCount),
			Properties->IntersectionStage.bGridRebuildIntersectionPatches
				? LOCTEXT("TopologyOn", "是")
				: LOCTEXT("TopologyOff", "否"),
			FText::AsNumber(RibbonSegments)));
}

bool UQuick_RoadMainRoadTool::TryCompleteStroke(bool bShowErrorDialog)
{
	const int32 MinPoints = 2;
	if (DrawPoints.Num() < MinPoints)
	{
		if (bShowErrorDialog)
		{
			FMessageDialog::Open(
				EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("NotEnoughPoints", "至少需要 {0} 个控制点才能生成主路。"),
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

bool UQuick_RoadMainRoadTool::SpawnSplineActorFromDrawPoints(FText& OutErrorMessage)
{
	if (!TargetWorld || DrawPoints.Num() == 0)
	{
		OutErrorMessage = LOCTEXT("InvalidState", "没有可用的绘制点。");
		return false;
	}

	const FVector PivotLocation = DrawPoints[0];
	const FScopedTransaction Transaction(LOCTEXT("CreateMainRoadSpline", "Create Main Road Spline"));

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
	SplineActor->SetActorLabel(TEXT("MainRoad"));

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
	SplineComponent->SetClosedLoop(false, false);
	SplineComponent->bSplineHasBeenEdited = true;
	SplineComponent->UpdateSpline();

	const FName RoadTag = Properties ? Properties->MainRoadStage.RoadTag : NAME_None;
	Quick_RoadMainRoadToolPrivate::ApplyRoadSplineTags(SplineActor, SplineComponent, RoadTag);

	if (GEditor)
	{
		GEditor->SelectNone(false, true, false);
		GEditor->SelectActor(SplineActor, true, true, true);
	}

	return true;
}

void UQuick_RoadMainRoadTool::DrawPreviewLines(FPrimitiveDrawInterface* PDI) const
{
	using namespace Quick_RoadMainRoadToolPrivate;

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

	if (bHasValidPreview && PointCount > 0)
	{
		PDI->DrawLine(DrawPoints.Last(), PreviewPoint, PreviewLineColor, SDPG_Foreground, 1.5f, 0.0f, true);
	}

	for (const FVector& Point : DrawPoints)
	{
		PDI->DrawPoint(Point, PreviewPointColor, 10.0f, SDPG_Foreground);
	}
}

void UQuick_RoadMainRoadTool::Render(IToolsContextRenderAPI* RenderAPI)
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
