// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quick_RoadLayoutLandscapeConformComponent.h"
#include "Quick_RoadLayoutMeshComponent.h"
#include "Tools/Quick_RoadLandscapeConform.h"
#include "Tools/Quick_RoadLandscapeHeightEdit.h"
#include "Quick_RoadLog.h"

#include "ProceduralMeshComponent.h"
#include "Landscape.h"
#include "EngineUtils.h"
#include "Misc/MessageDialog.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "Quick_RoadLayoutLandscapeConformComponent"

UQuick_RoadLayoutLandscapeConformComponent::UQuick_RoadLayoutLandscapeConformComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

ALandscape* UQuick_RoadLayoutLandscapeConformComponent::ResolveTargetLandscape() const
{
	if (ALandscape* SelectedLandscape = FQuick_RoadLandscapeHeightEdit::FindSelectedLandscape())
	{
		return SelectedLandscape;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		return *It;
	}

	return nullptr;
}

UProceduralMeshComponent* UQuick_RoadLayoutLandscapeConformComponent::ResolveLayoutMeshComponent() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (UQuick_RoadLayoutMeshComponent* LayoutMesh = Owner->FindComponentByClass<UQuick_RoadLayoutMeshComponent>())
	{
		return LayoutMesh;
	}

	TInlineComponentArray<UProceduralMeshComponent*> ProcMeshes(Owner);
	for (UProceduralMeshComponent* Mesh : ProcMeshes)
	{
		if (Mesh && Mesh->GetNumSections() > 0 && Mesh->GetProcMeshSection(0) != nullptr)
		{
			return Mesh;
		}
	}

	return nullptr;
}

void UQuick_RoadLayoutLandscapeConformComponent::ConformLandscape()
{
	UProceduralMeshComponent* LayoutMesh = ResolveLayoutMeshComponent();
	if (!LayoutMesh || LayoutMesh->GetProcMeshSection(0) == nullptr)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoMesh", "未找到有效的 LayoutMesh 网格数据。"));
		return;
	}

	ALandscape* Landscape = ResolveTargetLandscape();
	if (!Landscape)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NeedLandscape", "请先选中关卡中的 Landscape，或确保场景中存在 Landscape。"));
		return;
	}

	FQuick_RoadConformToMeshParams Params;
	Params.Smoothness = ConformSmoothness;
	Params.EdgeOffsetCm = ConformEdgeOffsetCm;
	Params.VerticalOffset = ConformVerticalOffset;
	Params.BlendStrength = ConformBlendStrength;

	TArray<UPrimitiveComponent*> ConformComponents;
	ConformComponents.Add(LayoutMesh);

	const FScopedTransaction Transaction(LOCTEXT("ConformLandscapeToLayoutMesh", "Conform Landscape To Layout Mesh"));

	FText ErrorMessage;
	if (!FQuick_RoadLandscapeConform::ApplyConformToLandscape(
		Landscape,
		ConformComponents,
		Params,
		ErrorMessage,
		EQuick_RoadConformSurfaceMode::Top))
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		return;
	}

	UE_LOG(LogQuickRoad, Display, TEXT("Quick_Road 地面贴合：已将 Landscape 贴合到 LayoutMesh 表面。"));
}

void UQuick_RoadLayoutLandscapeConformComponent::SmoothConformEdge()
{
	ALandscape* Landscape = ResolveTargetLandscape();
	if (!Landscape)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NeedLandscapeForEdge", "请先选中关卡中的 Landscape，或确保场景中存在 Landscape。"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoWorldForEdge", "当前世界无效。"));
		return;
	}

	FQuick_RoadConformEdgeSmoothParams Params;
	Params.SmoothWidthCm = FMath::Max(EdgeSmoothWidthCm, 50.f);
	Params.SmoothStrength = FMath::Clamp(EdgeSmoothStrength, 0.f, 1.f);
	Params.SmoothIterations = FMath::Clamp(EdgeSmoothIterations, 1, 32);

	const FScopedTransaction Transaction(LOCTEXT("SmoothConformEdge", "Smooth Landscape Conform Edge"));

	FText ErrorMessage;
	if (!FQuick_RoadLandscapeConform::SmoothLandscapeAtCityScopeBoundary(
		World,
		Landscape,
		Params,
		ErrorMessage))
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		return;
	}

	UE_LOG(LogQuickRoad, Display, TEXT("Quick_Road 边缘平滑：已沿城市范围样条边界平滑 Landscape。"));
}

#undef LOCTEXT_NAMESPACE
