// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quick_RoadEditorModeCommands.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "Quick_RoadEditorModeCommands"

FName FQuick_RoadEditorModeCommands::TerrainPaletteName(TEXT("Terrain"));
FName FQuick_RoadEditorModeCommands::CityPaletteName(TEXT("City"));
FName FQuick_RoadEditorModeCommands::BakePaletteName(TEXT("Bake"));

FQuick_RoadEditorModeCommands::FQuick_RoadEditorModeCommands()
	: TCommands<FQuick_RoadEditorModeCommands>("Quick_RoadEditorMode",
		NSLOCTEXT("Quick_RoadEditorMode", "Quick_RoadEditorModeCommands", "Quick Road Editor Mode"),
		NAME_None,
		FAppStyle::GetAppStyleSetName())
{
}

void FQuick_RoadEditorModeCommands::RegisterCommands()
{
	TArray<TSharedPtr<FUICommandInfo>>& TerrainCommands = Commands.FindOrAdd(TerrainPaletteName);

	UI_COMMAND(GenerateTerrainTool, "生成地形", "Create landscape from heightmap", EUserInterfaceActionType::ToggleButton, FInputChord());
	TerrainCommands.Add(GenerateTerrainTool);

	UI_COMMAND(HeightfieldNoiseTool, "高度噪声", "Apply Houdini-style heightfield noise to landscape", EUserInterfaceActionType::ToggleButton, FInputChord());
	TerrainCommands.Add(HeightfieldNoiseTool);

	UI_COMMAND(AutoErosionTool, "自动侵蚀", "Apply automatic erosion to terrain", EUserInterfaceActionType::ToggleButton, FInputChord());
	TerrainCommands.Add(AutoErosionTool);

	TArray<TSharedPtr<FUICommandInfo>>& CityCommands = Commands.FindOrAdd(CityPaletteName);

	UI_COMMAND(CityScopeTool, "区域绘制", "Draw layout area boundary spline on landscape", EUserInterfaceActionType::ToggleButton, FInputChord());
	CityCommands.Add(CityScopeTool);

	UI_COMMAND(RoadTool, "路网绘制", "Draw main road splines and generate road mesh", EUserInterfaceActionType::ToggleButton, FInputChord());
	CityCommands.Add(RoadTool);

	UI_COMMAND(RiverTool, "获取数据", "Extract layout data from generated road meshes", EUserInterfaceActionType::ToggleButton, FInputChord());
	CityCommands.Add(RiverTool);

	TArray<TSharedPtr<FUICommandInfo>>& BakeCommands = Commands.FindOrAdd(BakePaletteName);

	UI_COMMAND(BakeTool, "烘焙", "Bake procedural road meshes to static mesh assets", EUserInterfaceActionType::ToggleButton, FInputChord());
	BakeCommands.Add(BakeTool);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> FQuick_RoadEditorModeCommands::GetCommands()
{
	return FQuick_RoadEditorModeCommands::Get().Commands;
}

#undef LOCTEXT_NAMESPACE
