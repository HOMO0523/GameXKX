// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/Quick_RoadGenerateCityTool.h"
#include "Tools/Quick_RoadLayoutRoadGenerator.h"
#include "Quick_RoadLayoutRoadTags.h"
#include "Quick_RoadLayoutSplineTags.h"

#include "Algo/Reverse.h"
#include "Components/SplineComponent.h"
#include "EngineUtils.h"
#include "InteractiveToolManager.h"
#include "Engine/World.h"
#include "Editor.h"
#include "Misc/MessageDialog.h"
#include "SceneManagement.h"
#include "Selection.h"
#include "ScopedTransaction.h"
#include "ToolContextInterfaces.h"

#define LOCTEXT_NAMESPACE "Quick_RoadGenerateCityTool"

namespace Quick_RoadGenerateCityToolLocals
{
	static void GatherSelectedMainRoadNetworks(TArray<AActor*>& OutNetworkActors)
	{
		OutNetworkActors.Reset();

#if WITH_EDITOR
		if (!GEditor)
		{
			return;
		}

		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			AActor* Actor = Cast<AActor>(*It);
			if (Actor && Actor->ActorHasTag(Quick_RoadLayoutRoadTags::MainRoadNetwork))
			{
				OutNetworkActors.Add(Actor);
			}
		}
#endif
	}

	static bool IsRoadEdgeSplineActor(const AActor* Actor)
	{
		return Actor && Actor->ActorHasTag(Quick_RoadLayoutSplineTags::RoadEdge);
	}

	static USplineComponent* FindRoadEdgeSplineComponent(AActor* Actor)
	{
		if (!IsRoadEdgeSplineActor(Actor))
		{
			return nullptr;
		}

		USplineComponent* SplineComponent = Actor->FindComponentByClass<USplineComponent>();
		if (!SplineComponent || !SplineComponent->ComponentHasTag(Quick_RoadLayoutSplineTags::RoadEdge))
		{
			return nullptr;
		}

		return SplineComponent;
	}

	static void GatherSelectedRoadEdgeSplines(TArray<USplineComponent*>& OutSplines)
	{
		OutSplines.Reset();

#if WITH_EDITOR
		if (!GEditor)
		{
			return;
		}

		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			if (USplineComponent* SplineComponent = FindRoadEdgeSplineComponent(Cast<AActor>(*It)))
			{
				OutSplines.Add(SplineComponent);
			}
		}
#endif
	}

	static void GatherAllRoadEdgeSplines(UWorld* World, TArray<USplineComponent*>& OutSplines)
	{
		OutSplines.Reset();
		if (!World)
		{
			return;
		}

		for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
		{
			if (USplineComponent* SplineComponent = FindRoadEdgeSplineComponent(*ActorIt))
			{
				OutSplines.Add(SplineComponent);
			}
		}
	}
}

UQuick_RoadGenerateCityToolProperties::UQuick_RoadGenerateCityToolProperties() = default;

void UQuick_RoadGenerateCityToolProperties::ExtractRoadEdgeSplines()
{
	if (OwningTool)
	{
		OwningTool->ExecuteExtractRoadEdgeSplines();
	}
}

void UQuick_RoadGenerateCityToolProperties::ExtractIntersectionSplines()
{
	if (OwningTool)
	{
		OwningTool->ExecuteExtractIntersectionSplines();
	}
}

void UQuick_RoadGenerateCityToolProperties::ReverseSelectedSplineDirection()
{
	if (OwningTool)
	{
		OwningTool->ExecuteReverseSelectedSplineDirection();
	}
}

UInteractiveTool* UQuick_RoadGenerateCityToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UQuick_RoadGenerateCityTool* NewTool = NewObject<UQuick_RoadGenerateCityTool>(SceneState.ToolManager);
	NewTool->SetWorld(SceneState.World);
	return NewTool;
}

void UQuick_RoadGenerateCityTool::SetWorld(UWorld* World)
{
	TargetWorld = World;
}

void UQuick_RoadGenerateCityTool::Setup()
{
	Properties = NewObject<UQuick_RoadGenerateCityToolProperties>(this);
	Properties->SetOwningTool(this);
	AddToolPropertySource(Properties);
}

void UQuick_RoadGenerateCityTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	if (!RenderAPI || !Properties || !Properties->bDrawIntersectionSplinePointTbn)
	{
		return;
	}

	DrawIntersectionSplinePointTbn(RenderAPI->GetPrimitiveDrawInterface());
}

bool UQuick_RoadGenerateCityTool::TryGetSelectedMainRoadNetwork(AActor*& OutNetworkActor, FText& OutErrorMessage)
{
	OutNetworkActor = nullptr;

	TArray<AActor*> SelectedNetworks;
	Quick_RoadGenerateCityToolLocals::GatherSelectedMainRoadNetworks(SelectedNetworks);

	if (SelectedNetworks.Num() == 0)
	{
		OutErrorMessage = LOCTEXT(
			"SelectNetworkDialog",
			"请先在视口中选中一个 MainRoadNetwork 道路网络 Actor。");
		return false;
	}

	if (SelectedNetworks.Num() > 1)
	{
		OutErrorMessage = LOCTEXT(
			"MultipleNetworksDialog",
			"选中了多个 MainRoadNetwork，请仅保留一个选中后再提取。");
		return false;
	}

	OutNetworkActor = SelectedNetworks[0];
	return true;
}

void UQuick_RoadGenerateCityTool::ExecuteExtractRoadEdgeSplines()
{
	if (!TargetWorld)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoWorldDialog", "当前没有可用的编辑世界。"));
		return;
	}

	AActor* RoadNetworkActor = nullptr;
	FText SelectionError;
	if (!TryGetSelectedMainRoadNetwork(RoadNetworkActor, SelectionError))
	{
		FMessageDialog::Open(EAppMsgType::Ok, SelectionError);
		return;
	}

	FText ErrorMessage;
	int32 SplineCount = 0;
	const bool bSuccess = FQuick_RoadLayoutRoadGenerator::ExtractRoadEdgeSplines(
		TargetWorld,
		RoadNetworkActor,
		Properties ? Properties->MinPolylineLengthCm : 200.f,
		Properties ? Properties->VertexWeldToleranceCm : 2.f,
		Properties ? Properties->bIncludeIntersectionPatches : true,
		Properties ? static_cast<ESplinePointType::Type>(Properties->SplinePointType.GetValue()) : ESplinePointType::Curve,
		Properties ? Properties->bEnableSplineResample : true,
		Properties ? Properties->SplineResampleDistanceCm : 500.f,
		ErrorMessage,
		&SplineCount);

	if (!bSuccess)
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
	}
}

void UQuick_RoadGenerateCityTool::ExecuteExtractIntersectionSplines()
{
	if (!TargetWorld)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoWorldDialog", "当前没有可用的编辑世界。"));
		return;
	}

	AActor* RoadNetworkActor = nullptr;
	FText SelectionError;
	if (!TryGetSelectedMainRoadNetwork(RoadNetworkActor, SelectionError))
	{
		FMessageDialog::Open(EAppMsgType::Ok, SelectionError);
		return;
	}

	FText ErrorMessage;
	int32 SplineCount = 0;
	const bool bSuccess = FQuick_RoadLayoutRoadGenerator::ExtractIntersectionSplines(
		TargetWorld,
		RoadNetworkActor,
		Properties
			? static_cast<ESplinePointType::Type>(Properties->IntersectionSplinePointType.GetValue())
			: ESplinePointType::Curve,
		Properties ? Properties->bEnableIntersectionSplineResample : true,
		Properties ? Properties->IntersectionSplineResampleDistanceCm : 500.f,
		Properties ? Properties->IntersectionVertexWeldToleranceCm : 2.f,
		Properties ? Properties->MinInnerLoopLengthCm : 100.f,
		Properties ? Properties->bIncludeIntersectionPatchesForInnerLoops : true,
		ErrorMessage,
		&SplineCount);

	if (!bSuccess)
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
	}
}

void UQuick_RoadGenerateCityTool::ExecuteReverseSelectedSplineDirection()
{
	TArray<USplineComponent*> SelectedSplines;
	Quick_RoadGenerateCityToolLocals::GatherSelectedRoadEdgeSplines(SelectedSplines);

	if (SelectedSplines.Num() == 0)
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			LOCTEXT("ReverseNoSelectedSpline", "请先在视口中选中一个或多个 Quick_Road 环线样条 Actor。"));
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("ReverseSelectedSplineDirection", "Reverse Selected Spline Direction"));
	int32 ReversedCount = 0;
	for (USplineComponent* SplineComponent : SelectedSplines)
	{
		if (ReverseSplineDirection(SplineComponent))
		{
			++ReversedCount;
		}
	}

	if (ReversedCount == 0)
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			LOCTEXT("ReverseNoValidSpline", "选中的环线样条没有足够的点可反转。"));
	}
}

bool UQuick_RoadGenerateCityTool::ReverseSplineDirection(USplineComponent* SplineComponent)
{
	if (!SplineComponent || SplineComponent->GetNumberOfSplinePoints() < 2)
	{
		return false;
	}

	AActor* Owner = SplineComponent->GetOwner();
	if (Owner)
	{
		Owner->Modify();
	}
	SplineComponent->Modify();

	const bool bClosedLoop = SplineComponent->IsClosedLoop();
	const int32 PointCount = SplineComponent->GetNumberOfSplinePoints();
	TArray<FVector> Points;
	TArray<ESplinePointType::Type> PointTypes;
	Points.Reserve(PointCount);
	PointTypes.Reserve(PointCount);

	for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
	{
		Points.Add(SplineComponent->GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::World));
		PointTypes.Add(SplineComponent->GetSplinePointType(PointIndex));
	}

	Algo::Reverse(Points);
	Algo::Reverse(PointTypes);

	SplineComponent->ClearSplinePoints(false);
	SplineComponent->SetSplinePoints(Points, ESplineCoordinateSpace::World, false);
	SplineComponent->SetClosedLoop(bClosedLoop, false);
	for (int32 PointIndex = 0; PointIndex < SplineComponent->GetNumberOfSplinePoints(); ++PointIndex)
	{
		SplineComponent->SetSplinePointType(PointIndex, PointTypes[PointIndex], false);
	}
	SplineComponent->bSplineHasBeenEdited = true;
	SplineComponent->UpdateSpline();
	SplineComponent->MarkRenderStateDirty();

	return true;
}

void UQuick_RoadGenerateCityTool::DrawIntersectionSplinePointTbn(FPrimitiveDrawInterface* PDI) const
{
	if (!PDI || !TargetWorld)
	{
		return;
	}

	TArray<USplineComponent*> SplinesToDraw;
	Quick_RoadGenerateCityToolLocals::GatherSelectedRoadEdgeSplines(SplinesToDraw);
	if (SplinesToDraw.Num() == 0)
	{
		Quick_RoadGenerateCityToolLocals::GatherAllRoadEdgeSplines(TargetWorld, SplinesToDraw);
	}

	const float AxisLengthCm = FMath::Max(
		Properties ? Properties->IntersectionSplinePointTbnAxisLengthCm : 150.f,
		10.f);
	const float LineThickness = 3.f;
	const float PointSize = 8.f;

	for (USplineComponent* SplineComponent : SplinesToDraw)
	{
		if (!SplineComponent)
		{
			continue;
		}

		SplineComponent->UpdateSpline();
		const int32 PointCount = SplineComponent->GetNumberOfSplinePoints();
		for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
		{
			const FVector Origin = SplineComponent->GetLocationAtSplinePoint(
				PointIndex,
				ESplineCoordinateSpace::World);
			const FVector Tangent = SplineComponent->GetDirectionAtSplinePoint(
				PointIndex,
				ESplineCoordinateSpace::World).GetSafeNormal();
			const FVector Binormal = SplineComponent->GetRightVectorAtSplinePoint(
				PointIndex,
				ESplineCoordinateSpace::World).GetSafeNormal();
			const FVector Normal = SplineComponent->GetUpVectorAtSplinePoint(
				PointIndex,
				ESplineCoordinateSpace::World).GetSafeNormal();

			PDI->DrawPoint(Origin, FLinearColor::Yellow, PointSize, SDPG_Foreground);
			if (!Tangent.IsNearlyZero())
			{
				PDI->DrawLine(Origin, Origin + Tangent * AxisLengthCm, FLinearColor::Red, SDPG_Foreground, LineThickness, 0.f, true);
			}
			if (!Binormal.IsNearlyZero())
			{
				PDI->DrawLine(Origin, Origin + Binormal * AxisLengthCm, FLinearColor::Green, SDPG_Foreground, LineThickness, 0.f, true);
			}
			if (!Normal.IsNearlyZero())
			{
				PDI->DrawLine(Origin, Origin + Normal * AxisLengthCm, FLinearColor::Blue, SDPG_Foreground, LineThickness, 0.f, true);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
