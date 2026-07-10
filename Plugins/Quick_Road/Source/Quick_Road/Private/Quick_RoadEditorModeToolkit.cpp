// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quick_RoadEditorModeToolkit.h"
#include "Quick_RoadEditorModeCommands.h"
#include "Quick_RoadLocalization.h"
#include "UI/SQuick_RoadModePanel.h"

#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Styling/AppStyle.h"
#include "InteractiveTool.h"
#include "InteractiveToolManager.h"

#define LOCTEXT_NAMESPACE "Quick_RoadEditorModeToolkit"

FQuick_RoadEditorModeToolkit::FQuick_RoadEditorModeToolkit()
{
}

void FQuick_RoadEditorModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	FModeToolkit::Init(InitToolkitHost, InOwningMode);

	SetCurrentPalette(FQuick_RoadEditorModeCommands::TerrainPaletteName);
	BuildModePanel();
}

void FQuick_RoadEditorModeToolkit::BuildModePanel()
{
	const FQuick_RoadEditorModeCommands& Commands = FQuick_RoadEditorModeCommands::Get();

	TArray<FQuick_RoadCategoryDef> Categories;
	FQuick_RoadCategoryDef TerrainCategory;
	TerrainCategory.CategoryId = FQuick_RoadEditorModeCommands::TerrainPaletteName;
	TerrainCategory.Label = LOCTEXT("TerrainCategory", "地形");
	TerrainCategory.IconBrush = FAppStyle::GetBrush("ClassIcon.Landscape");
	TerrainCategory.ToolCommands = {
		Commands.GenerateTerrainTool,
		Commands.HeightfieldNoiseTool,
		Commands.AutoErosionTool
	};
	Categories.Add(TerrainCategory);

	FQuick_RoadCategoryDef CityCategory;
	CityCategory.CategoryId = FQuick_RoadEditorModeCommands::CityPaletteName;
	CityCategory.Label = LOCTEXT("CityCategory", "布局");
	CityCategory.IconBrush = FAppStyle::GetBrush("ClassIcon.Blueprint");
	CityCategory.ToolCommands = {
		Commands.CityScopeTool,
		Commands.RoadTool,
		Commands.RiverTool
	};
	Categories.Add(CityCategory);

	FQuick_RoadCategoryDef BakeCategory;
	BakeCategory.CategoryId = FQuick_RoadEditorModeCommands::BakePaletteName;
	BakeCategory.Label = LOCTEXT("BakeCategory", "烘焙");
	BakeCategory.IconBrush = FAppStyle::GetBrush("ClassIcon.StaticMesh");
	BakeCategory.ToolCommands = {
		Commands.BakeTool
	};
	Categories.Add(BakeCategory);

	ToolkitWidget = SAssignNew(ModePanel, SQuick_RoadModePanel)
		.CommandList(GetToolkitCommands())
		.DetailsView(DetailsView)
		.OwningMode(OwningEditorMode)
		.DefaultCategory(FQuick_RoadEditorModeCommands::TerrainPaletteName)
		.Categories(Categories);
}

void FQuick_RoadEditorModeToolkit::GetToolPaletteNames(TArray<FName>& PaletteNames) const
{
	// 工具面板由 SQuick_RoadModePanel 自定义实现，不向引擎注册默认 Vertical Toolbar 调色板
}

FText FQuick_RoadEditorModeToolkit::GetToolPaletteDisplayName(FName PaletteName) const
{
	return QuickRoadLocalization::GetCategoryLabel(PaletteName);
}

TSharedPtr<SWidget> FQuick_RoadEditorModeToolkit::GetInlineContent() const
{
	return ToolkitWidget;
}

FName FQuick_RoadEditorModeToolkit::GetToolkitFName() const
{
	return FName("Quick_RoadEditorMode");
}

FText FQuick_RoadEditorModeToolkit::GetBaseToolkitName() const
{
	return QuickRoadLocalization::GetEditorModeName();
}

void FQuick_RoadEditorModeToolkit::OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	FModeToolkit::OnToolStarted(Manager, Tool);

	if (ModePanel.IsValid())
	{
		ModePanel->SetDetailsVisibility(EVisibility::Visible);
	}
}

void FQuick_RoadEditorModeToolkit::OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	FModeToolkit::OnToolEnded(Manager, Tool);

	if (ModePanel.IsValid())
	{
		ModePanel->SetDetailsVisibility(EVisibility::Collapsed);
	}
}

#undef LOCTEXT_NAMESPACE
