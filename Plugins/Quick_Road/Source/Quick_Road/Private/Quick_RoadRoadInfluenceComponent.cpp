// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quick_RoadRoadInfluenceComponent.h"

#include "Tools/Quick_RoadLayoutRoadGenerator.h"

#include "Quick_RoadLog.h"

#include "ProceduralMeshComponent.h"

#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "Quick_RoadRoadInfluenceComponent"

UQuick_RoadRoadInfluenceComponent::UQuick_RoadRoadInfluenceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UQuick_RoadRoadInfluenceComponent::CollectOwnerProcMeshes(TArray<UProceduralMeshComponent*>& OutMeshes) const
{
	OutMeshes.Reset();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TInlineComponentArray<UProceduralMeshComponent*> MeshComponents(Owner);
	for (UProceduralMeshComponent* MeshComponent : MeshComponents)
	{
		if (!MeshComponent || MeshComponent->GetNumSections() == 0)
		{
			continue;
		}

		bool bHasTriangles = false;
		for (int32 SectionIndex = 0; SectionIndex < MeshComponent->GetNumSections(); ++SectionIndex)
		{
			const FProcMeshSection* Section = MeshComponent->GetProcMeshSection(SectionIndex);
			if (Section && Section->ProcIndexBuffer.Num() >= 3)
			{
				bHasTriangles = true;
				break;
			}
		}

		if (bHasTriangles)
		{
			OutMeshes.Add(MeshComponent);
		}
	}
}

void UQuick_RoadRoadInfluenceComponent::ApplyRoadInfluence()
{
	UWorld* World = GetWorld();
	TArray<UProceduralMeshComponent*> RoadMeshes;
	CollectOwnerProcMeshes(RoadMeshes);

	if (!World || RoadMeshes.Num() == 0)
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			LOCTEXT("InvalidProcMeshes", "当前道路网络 Actor 上没有可用的 ProcMesh。请先「生成道路」或「拆开并重构」。"));
		return;
	}

	FQuick_RoadRoadInfluenceParams Params;
	Params.HalfWidthCm = FMath::Max(HalfWidthCm, 50.f);
	Params.InfluenceFalloffCm = FMath::Max(InfluenceFalloffCm, 0.f);
	Params.BlendStrength = FMath::Clamp(BlendStrength, 0.f, 1.f);
	Params.VerticalOffsetCm = VerticalOffsetCm;
	Params.EdgeSmoothIterations = EdgeSmoothIterations;
	Params.EdgeSmoothStrength = EdgeSmoothStrength;

	int32 LandscapeHits = 0;
	int32 LayoutHits = 0;
	FText ErrorMessage;
	if (!FQuick_RoadLayoutRoadGenerator::ApplyRoadInfluenceDualFromProcMeshes(
		World,
		RoadMeshes,
		Params,
		ErrorMessage,
		&LandscapeHits,
		&LayoutHits))
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		return;
	}

	FMessageDialog::Open(
		EAppMsgType::Ok,
		FText::Format(
			LOCTEXT("ApplySuccess", "道路影响已应用（{0} 个 ProcMesh）。\nLandscape：{1} 点\n布局面片：{2} 点"),
			FText::AsNumber(RoadMeshes.Num()),
			FText::AsNumber(LandscapeHits),
			FText::AsNumber(LayoutHits)));

	UE_LOG(LogQuickRoad, Display, TEXT("Quick_Road 道路影响组件：已按 ProcMesh 应用双域写入。"));
}

void UQuick_RoadRoadInfluenceComponent::SmoothRoadEdge()
{
	UWorld* World = GetWorld();
	TArray<UProceduralMeshComponent*> RoadMeshes;
	CollectOwnerProcMeshes(RoadMeshes);

	if (!World || RoadMeshes.Num() == 0)
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			LOCTEXT("InvalidProcMeshesSmooth", "当前道路网络 Actor 上没有可用的 ProcMesh。"));
		return;
	}

	FQuick_RoadRoadInfluenceParams Params;
	Params.HalfWidthCm = FMath::Max(HalfWidthCm, 50.f);
	Params.InfluenceFalloffCm = FMath::Max(InfluenceFalloffCm, 0.f);
	Params.EdgeSmoothIterations = FMath::Clamp(EdgeSmoothIterations, 1, 32);
	Params.EdgeSmoothStrength = FMath::Clamp(EdgeSmoothStrength, 0.f, 1.f);

	FText ErrorMessage;
	if (!FQuick_RoadLayoutRoadGenerator::SmoothRoadCorridorDualFromProcMeshes(
		World,
		RoadMeshes,
		Params,
		ErrorMessage))
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		return;
	}

	FMessageDialog::Open(
		EAppMsgType::Ok,
		FText::Format(
			LOCTEXT("SmoothSuccess", "已沿 {0} 个 ProcMesh 的边缘平滑 Landscape 与布局面片。"),
			FText::AsNumber(RoadMeshes.Num())));
}

#undef LOCTEXT_NAMESPACE
