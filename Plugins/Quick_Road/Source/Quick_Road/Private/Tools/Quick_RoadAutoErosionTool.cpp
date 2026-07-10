// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/Quick_RoadAutoErosionTool.h"
#include "Tools/Quick_RoadLandscapeErosion.h"
#include "Tools/Quick_RoadErosionAlgorithms.h"
#include "InteractiveToolManager.h"

#include "Landscape.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "Quick_RoadAutoErosionTool"

UQuick_RoadAutoErosionToolProperties::UQuick_RoadAutoErosionToolProperties()
{
	ErosionAlgorithm = EQuick_RoadErosionAlgorithm::Combined;
	Seed = 12345;
	HydraulicIterations = 20;
	DropletsPerIteration = 500;
	Inertia = 0.6f;
	MaxDropletSteps = 66;
	ErosionRate = 0.5f;
	DepositionRate = 0.25f;
	SedimentCapacity = 8.f;
	Evaporation = 0.01f;
	ThermalIterations = 15;
	ThermalTalusThreshold = 1.0f;
	ThermalStrength = 0.35f;
	MacroSmoothPasses = 1;
	PeakSharpenStrength = 0.6288f;
	PeakSharpenPasses = 1;
}

void UQuick_RoadAutoErosionToolProperties::StartErosion()
{
	if (OwningTool)
	{
		OwningTool->ExecuteStartErosion();
	}
}

UInteractiveTool* UQuick_RoadAutoErosionToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UQuick_RoadAutoErosionTool* NewTool = NewObject<UQuick_RoadAutoErosionTool>(SceneState.ToolManager);
	return NewTool;
}

void UQuick_RoadAutoErosionTool::Setup()
{
	Properties = NewObject<UQuick_RoadAutoErosionToolProperties>(this);
	Properties->SetOwningTool(this);
	AddToolPropertySource(Properties);
}

void UQuick_RoadAutoErosionTool::ExecuteStartErosion()
{
	if (!Properties)
	{
		return;
	}

	ALandscape* Landscape = FQuick_RoadLandscapeErosion::FindSelectedLandscape();

	FQuick_RoadErosionJobSettings Settings;
	Settings.Seed = Properties->Seed;
	Settings.Hydraulic.Iterations = Properties->HydraulicIterations;
	Settings.Hydraulic.DropletsPerIteration = Properties->DropletsPerIteration;
	Settings.Hydraulic.Inertia = Properties->Inertia;
	Settings.Hydraulic.MaxDropletSteps = Properties->MaxDropletSteps;
	Settings.Hydraulic.ErosionRate = Properties->ErosionRate;
	Settings.Hydraulic.DepositionRate = Properties->DepositionRate;
	Settings.Hydraulic.SedimentCapacity = Properties->SedimentCapacity;
	Settings.Hydraulic.Evaporation = Properties->Evaporation;
	Settings.Thermal.Iterations = Properties->ThermalIterations;
	Settings.Thermal.TalusThreshold = Properties->ThermalTalusThreshold;
	Settings.Thermal.Strength = Properties->ThermalStrength;
	Settings.MacroSmoothPasses = Properties->MacroSmoothPasses;
	Settings.PeakSharpenStrength = Properties->PeakSharpenStrength;
	Settings.PeakSharpenPasses = Properties->PeakSharpenPasses;

	switch (Properties->ErosionAlgorithm)
	{
	case EQuick_RoadErosionAlgorithm::HydraulicParticle:
		Settings.Algorithm = EQuick_RoadErosionAlgorithmType::HydraulicParticle;
		break;
	case EQuick_RoadErosionAlgorithm::ThermalTalus:
		Settings.Algorithm = EQuick_RoadErosionAlgorithmType::ThermalTalus;
		break;
	case EQuick_RoadErosionAlgorithm::Combined:
		Settings.Algorithm = EQuick_RoadErosionAlgorithmType::Combined;
		break;
	default:
		Settings.Algorithm = EQuick_RoadErosionAlgorithmType::HydraulicParticle;
		break;
	}

	FText ErrorMessage;
	if (!FQuick_RoadLandscapeErosion::ApplyErosionToLandscape(Landscape, Settings, ErrorMessage))
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
	}
}

#undef LOCTEXT_NAMESPACE
