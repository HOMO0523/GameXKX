// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/Quick_RoadBakeTool.h"

#include "Tools/Quick_RoadLayoutRoadGenerator.h"

#include "InteractiveToolManager.h"

#include "Engine/World.h"

#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "Quick_RoadBakeTool"

UQuick_RoadBakeToolProperties::UQuick_RoadBakeToolProperties()
{
	SaveDirectory.Path = TEXT("/Game/Quick_Road/Bake");
	StraightSegmentSplitDistanceCm = 5000.f;
	bDeleteSourceProcMesh = true;
}

void UQuick_RoadBakeToolProperties::BakeStaticMeshes()
{
	if (OwningTool)
	{
		OwningTool->ExecuteBakeStaticMeshes();
	}
}

UInteractiveTool* UQuick_RoadBakeToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UQuick_RoadBakeTool* NewTool = NewObject<UQuick_RoadBakeTool>(SceneState.ToolManager);
	NewTool->SetWorld(SceneState.World);
	return NewTool;
}

void UQuick_RoadBakeTool::SetWorld(UWorld* World)
{
	TargetWorld = World;
}

void UQuick_RoadBakeTool::Setup()
{
	Properties = NewObject<UQuick_RoadBakeToolProperties>(this);
	Properties->SetOwningTool(this);
	AddToolPropertySource(Properties);
}

void UQuick_RoadBakeTool::ExecuteBakeStaticMeshes()
{
	if (!TargetWorld)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoWorldDialog", "当前没有可用的编辑世界。"));
		return;
	}

	if (!Properties || Properties->SaveDirectory.Path.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoSavePath", "请先设置保存路径。"));
		return;
	}

	FText ErrorMessage;
	int32 BakedCount = 0;
	const bool bSuccess = FQuick_RoadLayoutRoadGenerator::BakeRoadMeshesToStaticMeshes(
		TargetWorld,
		Properties->SaveDirectory.Path,
		Properties->StraightSegmentSplitDistanceCm,
		Properties->bDeleteSourceProcMesh,
		ErrorMessage,
		&BakedCount);

	if (!bSuccess)
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		return;
	}

	FMessageDialog::Open(
		EAppMsgType::Ok,
		FText::Format(
			LOCTEXT("BakeSuccess", "烘焙完成：{0} 个 StaticMesh，已拼装蓝图并放入场景。"),
			BakedCount));
}

#undef LOCTEXT_NAMESPACE
