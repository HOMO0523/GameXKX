// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quick_RoadEditorMode.h"
#include "Quick_RoadEditorModeToolkit.h"
#include "Quick_RoadEditorModeCommands.h"
#include "Quick_RoadLocalization.h"
#include "Tools/Quick_RoadGenerateTerrainTool.h"
#include "Tools/Quick_RoadHeightfieldNoiseTool.h"
#include "Tools/Quick_RoadAutoErosionTool.h"
#include "Tools/Quick_RoadCityScopeTool.h"
#include "Tools/Quick_RoadMainRoadTool.h"
#include "Tools/Quick_RoadGenerateCityTool.h"
#include "Tools/Quick_RoadBakeTool.h"

#include "InteractiveToolManager.h"
#include "InteractiveTool.h"

#define LOCTEXT_NAMESPACE "Quick_RoadEditorMode"

const FEditorModeID UQuick_RoadEditorMode::EM_Quick_RoadEditorModeId = TEXT("EM_Quick_RoadEditorMode");

FString UQuick_RoadEditorMode::GenerateTerrainToolName = TEXT("Quick_Road_GenerateTerrain");
FString UQuick_RoadEditorMode::HeightfieldNoiseToolName = TEXT("Quick_Road_HeightfieldNoise");
FString UQuick_RoadEditorMode::AutoErosionToolName = TEXT("Quick_Road_AutoErosion");

FString UQuick_RoadEditorMode::CityScopeToolName = TEXT("Quick_Road_CityScope");
FString UQuick_RoadEditorMode::RoadToolName = TEXT("Quick_Road_Road");
FString UQuick_RoadEditorMode::RiverToolName = TEXT("Quick_Road_River");

FString UQuick_RoadEditorMode::BakeToolName = TEXT("Quick_Road_Bake");

UQuick_RoadEditorMode::UQuick_RoadEditorMode()
{
	FModuleManager::Get().LoadModule("EditorStyle");

	Info = FEditorModeInfo(UQuick_RoadEditorMode::EM_Quick_RoadEditorModeId,
		QuickRoadLocalization::GetEditorModeName(),
		FSlateIcon(),
		true);
}

UQuick_RoadEditorMode::~UQuick_RoadEditorMode()
{
}

void UQuick_RoadEditorMode::ActorSelectionChangeNotify()
{
	UBaseLegacyWidgetEdMode::ActorSelectionChangeNotify();
}

bool UQuick_RoadEditorMode::ShouldDrawWidget() const
{
	// Quick_Road 工具均为面板型被动工具，选中 Actor 时仍显示标准移动/旋转/缩放 Gizmo。
	return UBaseLegacyWidgetEdMode::ShouldDrawWidget();
}

void UQuick_RoadEditorMode::Enter()
{
	UEdMode::Enter();

	const FQuick_RoadEditorModeCommands& ToolCommands = FQuick_RoadEditorModeCommands::Get();

	UQuick_RoadGenerateTerrainToolBuilder* GenerateBuilder = NewObject<UQuick_RoadGenerateTerrainToolBuilder>(this);
	RegisterTool(ToolCommands.GenerateTerrainTool, GenerateTerrainToolName, GenerateBuilder);

	UQuick_RoadHeightfieldNoiseToolBuilder* NoiseBuilder = NewObject<UQuick_RoadHeightfieldNoiseToolBuilder>(this);
	RegisterTool(ToolCommands.HeightfieldNoiseTool, HeightfieldNoiseToolName, NoiseBuilder);

	UQuick_RoadAutoErosionToolBuilder* ErosionBuilder = NewObject<UQuick_RoadAutoErosionToolBuilder>(this);
	RegisterTool(ToolCommands.AutoErosionTool, AutoErosionToolName, ErosionBuilder);

	UQuick_RoadCityScopeToolBuilder* CityScopeBuilder = NewObject<UQuick_RoadCityScopeToolBuilder>(this);
	RegisterTool(ToolCommands.CityScopeTool, CityScopeToolName, CityScopeBuilder);

	UQuick_RoadMainRoadToolBuilder* MainRoadBuilder = NewObject<UQuick_RoadMainRoadToolBuilder>(this);
	RegisterTool(ToolCommands.RoadTool, RoadToolName, MainRoadBuilder);

	UQuick_RoadGenerateCityToolBuilder* GenerateCityBuilder = NewObject<UQuick_RoadGenerateCityToolBuilder>(this);
	RegisterTool(ToolCommands.RiverTool, RiverToolName, GenerateCityBuilder);

	UQuick_RoadBakeToolBuilder* BakeBuilder = NewObject<UQuick_RoadBakeToolBuilder>(this);
	RegisterTool(ToolCommands.BakeTool, BakeToolName, BakeBuilder);
}

void UQuick_RoadEditorMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FQuick_RoadEditorModeToolkit);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> UQuick_RoadEditorMode::GetModeCommands() const
{
	return FQuick_RoadEditorModeCommands::Get().GetCommands();
}

bool UQuick_RoadEditorMode::GetCursor(EMouseCursor::Type& OutCursor) const
{
	UInteractiveToolManager* ToolManager = GetToolManager(EToolsContextScope::Editor);
	if (!ToolManager)
	{
		ToolManager = GetToolManager(EToolsContextScope::EdMode);
	}

	if (ToolManager)
	{
		UInteractiveTool* ActiveTool = ToolManager->GetActiveTool(EToolSide::Mouse);
		if (!ActiveTool)
		{
			ActiveTool = ToolManager->GetActiveTool(EToolSide::Left);
		}

		if (const UQuick_RoadCityScopeTool* CityScopeTool = Cast<UQuick_RoadCityScopeTool>(ActiveTool))
		{
			if (CityScopeTool->IsDrawing())
			{
				OutCursor = EMouseCursor::Crosshairs;
				return true;
			}
		}

		if (const UQuick_RoadMainRoadTool* MainRoadTool = Cast<UQuick_RoadMainRoadTool>(ActiveTool))
		{
			if (MainRoadTool->IsDrawing())
			{
				OutCursor = EMouseCursor::Crosshairs;
				return true;
			}
		}
	}

	return UBaseLegacyWidgetEdMode::GetCursor(OutCursor);
}

#undef LOCTEXT_NAMESPACE
