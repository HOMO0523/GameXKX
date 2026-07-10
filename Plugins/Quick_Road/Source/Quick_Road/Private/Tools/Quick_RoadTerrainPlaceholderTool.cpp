// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/Quick_RoadTerrainPlaceholderTool.h"
#include "InteractiveToolManager.h"

#define LOCTEXT_NAMESPACE "Quick_RoadTerrainPlaceholderTool"

UQuick_RoadTerrainToolProperties::UQuick_RoadTerrainToolProperties()
{
	Seed = 12345;
	Strength = 1.0f;
}

UInteractiveTool* UQuick_RoadTerrainPlaceholderToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UQuick_RoadTerrainPlaceholderTool* NewTool = NewObject<UQuick_RoadTerrainPlaceholderTool>(SceneState.ToolManager);
	NewTool->SetToolKind(ToolKind);
	return NewTool;
}

void UQuick_RoadTerrainPlaceholderTool::SetToolKind(EQuick_RoadTerrainToolKind InToolKind)
{
	ToolKind = InToolKind;
}

void UQuick_RoadTerrainPlaceholderTool::Setup()
{
	Properties = NewObject<UQuick_RoadTerrainToolProperties>(this);

	switch (ToolKind)
	{
	case EQuick_RoadTerrainToolKind::GenerateTerrain:
		Properties->Description = LOCTEXT("GenerateTerrainDesc", "Placeholder: procedurally generate terrain heightfield.");
		break;
	case EQuick_RoadTerrainToolKind::AutoErosion:
		Properties->Description = LOCTEXT("AutoErosionDesc", "Placeholder: apply automatic hydraulic erosion to terrain.");
		break;
	case EQuick_RoadTerrainToolKind::ConformToMesh:
		Properties->Description = LOCTEXT("ConformToMeshDesc", "Placeholder: conform terrain surface to underlying mesh geometry.");
		break;
	default:
		break;
	}

	AddToolPropertySource(Properties);
}

#undef LOCTEXT_NAMESPACE
