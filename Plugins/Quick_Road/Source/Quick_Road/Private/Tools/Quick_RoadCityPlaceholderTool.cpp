// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/Quick_RoadCityPlaceholderTool.h"
#include "InteractiveToolManager.h"

#define LOCTEXT_NAMESPACE "Quick_RoadCityPlaceholderTool"

UQuick_RoadCityToolProperties::UQuick_RoadCityToolProperties()
{
	Seed = 12345;
	Strength = 1.0f;
}

UInteractiveTool* UQuick_RoadCityPlaceholderToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UQuick_RoadCityPlaceholderTool* NewTool = NewObject<UQuick_RoadCityPlaceholderTool>(SceneState.ToolManager);
	NewTool->SetToolKind(ToolKind);
	return NewTool;
}

void UQuick_RoadCityPlaceholderTool::SetToolKind(EQuick_RoadCityToolKind InToolKind)
{
	ToolKind = InToolKind;
}

void UQuick_RoadCityPlaceholderTool::Setup()
{
	Properties = NewObject<UQuick_RoadCityToolProperties>(this);

	switch (ToolKind)
	{
	case EQuick_RoadCityToolKind::CityScope:
		Properties->Description = LOCTEXT("CityScopeDesc", "Placeholder: define city boundary scope.");
		break;
	case EQuick_RoadCityToolKind::Road:
		Properties->Description = LOCTEXT("RoadDesc", "Placeholder: generate and edit road network.");
		break;
	case EQuick_RoadCityToolKind::River:
		Properties->Description = LOCTEXT("GenerateCityDesc", "Placeholder: generate procedural city.");
		break;
	case EQuick_RoadCityToolKind::Lake:
		Properties->Description = LOCTEXT("LakeDesc", "Placeholder: generate and edit lake areas.");
		break;
	default:
		break;
	}

	AddToolPropertySource(Properties);
}

#undef LOCTEXT_NAMESPACE
