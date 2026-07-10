// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FQuick_RoadEditorModeCommands : public TCommands<FQuick_RoadEditorModeCommands>
{
public:
	FQuick_RoadEditorModeCommands();

	virtual void RegisterCommands() override;
	static TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetCommands();

	static FName TerrainPaletteName;
	static FName CityPaletteName;
	static FName BakePaletteName;

	TSharedPtr<FUICommandInfo> GenerateTerrainTool;
	TSharedPtr<FUICommandInfo> HeightfieldNoiseTool;
	TSharedPtr<FUICommandInfo> AutoErosionTool;

	TSharedPtr<FUICommandInfo> CityScopeTool;
	TSharedPtr<FUICommandInfo> RoadTool;
	TSharedPtr<FUICommandInfo> RiverTool;

	TSharedPtr<FUICommandInfo> BakeTool;

protected:
	TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> Commands;
};
