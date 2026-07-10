// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/Quick_RoadGenerateTerrainTool.h"
#include "Tools/Quick_RoadLandscapeCreator.h"
#include "InteractiveToolManager.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "Quick_RoadGenerateTerrainTool"

namespace
{
	int32 GetQuadsPerSection(EQuick_RoadLandscapeQuadsPerSection Value)
	{
		const int32 Quads = static_cast<int32>(Value);
		return Quads > 0 ? Quads : 63;
	}

	int32 GetSectionsPerComponent(EQuick_RoadLandscapeSectionsPerComponent Value)
	{
		const int32 Sections = static_cast<int32>(Value);
		return Sections > 0 ? Sections : 2;
	}
}

UQuick_RoadGenerateTerrainToolProperties::UQuick_RoadGenerateTerrainToolProperties()
{
	QuadsPerSection = EQuick_RoadLandscapeQuadsPerSection::Quads63;
	SectionsPerComponent = EQuick_RoadLandscapeSectionsPerComponent::Section2x2;
	ComponentCount = FIntPoint(8, 8);
	RefreshComputedValues();
}

void UQuick_RoadGenerateTerrainToolProperties::RefreshComputedValues()
{
	int32 ResolutionX = 0;
	int32 ResolutionY = 0;
	int32 ComputedTotalComponents = 0;
	FQuick_RoadLandscapeCreator::ComputeGridStats(
		GetQuadsPerSection(QuadsPerSection),
		GetSectionsPerComponent(SectionsPerComponent),
		ComponentCount,
		ResolutionX,
		ResolutionY,
		ComputedTotalComponents);

	OverallResolutionX = ResolutionX;
	OverallResolutionY = ResolutionY;
	TotalComponents = ComputedTotalComponents;
}

void UQuick_RoadGenerateTerrainToolProperties::CreateLandscape()
{
	if (OwningTool)
	{
		OwningTool->ExecuteCreateLandscape();
	}
}

UInteractiveTool* UQuick_RoadGenerateTerrainToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UQuick_RoadGenerateTerrainTool* NewTool = NewObject<UQuick_RoadGenerateTerrainTool>(SceneState.ToolManager);
	return NewTool;
}

void UQuick_RoadGenerateTerrainTool::Setup()
{
	Properties = NewObject<UQuick_RoadGenerateTerrainToolProperties>(this);
	Properties->SetOwningTool(this);
	Properties->RefreshComputedValues();
	AddToolPropertySource(Properties);

	TargetWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

void UQuick_RoadGenerateTerrainTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	if (PropertySet == Properties)
	{
		Properties->RefreshComputedValues();
	}
}

void UQuick_RoadGenerateTerrainTool::ExecuteCreateLandscape()
{
	if (!Properties)
	{
		return;
	}

	UWorld* World = TargetWorld;
	if (!World && GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}

	FQuick_RoadGenerateTerrainSettings Settings;
	Settings.Material = Properties->Material;
	Settings.QuadsPerSection = GetQuadsPerSection(Properties->QuadsPerSection);
	Settings.SectionsPerComponent = GetSectionsPerComponent(Properties->SectionsPerComponent);
	Settings.ComponentCount = Properties->ComponentCount;
	Settings.HeightmapFilePath = Properties->HeightmapFilePath.FilePath;

	FText ErrorMessage;
	if (!FQuick_RoadLandscapeCreator::TryCreateLandscape(World, Settings, ErrorMessage))
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
	}
}

#undef LOCTEXT_NAMESPACE
