// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/Quick_RoadHeightfieldNoiseTool.h"
#include "Tools/Quick_RoadLandscapeHeightEdit.h"
#include "Tools/Quick_RoadHeightfieldNoise.h"
#include "Quick_RoadLog.h"
#include "InteractiveToolManager.h"

#include "Landscape.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "Quick_RoadHeightfieldNoiseTool"

UQuick_RoadHeightfieldNoiseToolProperties::UQuick_RoadHeightfieldNoiseToolProperties()
{
	Amplitude = 30.f;
	ElementSize = 512.f;
	bCenterNoise = true;
	NoiseType = EQuick_RoadHeightfieldNoiseTypeUI::Perlin;
	FractalType = EQuick_RoadHeightfieldFractalTypeUI::Terrain;
	MaxOctaves = 4;
	Lacunarity = 2.f;
	Roughness = 0.5f;
	CombineMode = EQuick_RoadHeightfieldCombineModeUI::Add;
	Seed = 12345;
	Offset = FVector2D::ZeroVector;
	Scale = FVector2D(1.f, 1.f);
}

FQuick_RoadHeightfieldNoiseParams UQuick_RoadHeightfieldNoiseToolProperties::BuildParams() const
{
	FQuick_RoadHeightfieldNoiseParams Params;
	Params.Amplitude = Amplitude;
	Params.ElementSize = ElementSize;
	Params.bCenterNoise = bCenterNoise;
	Params.MaxOctaves = MaxOctaves;
	Params.Lacunarity = Lacunarity;
	Params.Roughness = Roughness;
	Params.Seed = Seed;
	Params.Offset = Offset;
	Params.Scale = Scale;

	switch (NoiseType)
	{
	case EQuick_RoadHeightfieldNoiseTypeUI::Ridged:
		Params.NoiseType = EQuick_RoadHeightfieldNoiseType::Ridged;
		break;
	case EQuick_RoadHeightfieldNoiseTypeUI::Billowy:
		Params.NoiseType = EQuick_RoadHeightfieldNoiseType::Billowy;
		break;
	default:
		Params.NoiseType = EQuick_RoadHeightfieldNoiseType::Perlin;
		break;
	}

	switch (FractalType)
	{
	case EQuick_RoadHeightfieldFractalTypeUI::None:
		Params.FractalType = EQuick_RoadHeightfieldFractalType::None;
		break;
	case EQuick_RoadHeightfieldFractalTypeUI::Standard:
		Params.FractalType = EQuick_RoadHeightfieldFractalType::Standard;
		break;
	case EQuick_RoadHeightfieldFractalTypeUI::HybridTerrain:
		Params.FractalType = EQuick_RoadHeightfieldFractalType::HybridTerrain;
		break;
	default:
		Params.FractalType = EQuick_RoadHeightfieldFractalType::Terrain;
		break;
	}

	switch (CombineMode)
	{
	case EQuick_RoadHeightfieldCombineModeUI::Replace:
		Params.CombineMode = EQuick_RoadHeightfieldCombineMode::Replace;
		break;
	case EQuick_RoadHeightfieldCombineModeUI::Multiply:
		Params.CombineMode = EQuick_RoadHeightfieldCombineMode::Multiply;
		break;
	case EQuick_RoadHeightfieldCombineModeUI::Maximum:
		Params.CombineMode = EQuick_RoadHeightfieldCombineMode::Maximum;
		break;
	case EQuick_RoadHeightfieldCombineModeUI::Minimum:
		Params.CombineMode = EQuick_RoadHeightfieldCombineMode::Minimum;
		break;
	default:
		Params.CombineMode = EQuick_RoadHeightfieldCombineMode::Add;
		break;
	}

	return Params;
}

void UQuick_RoadHeightfieldNoiseToolProperties::ApplyNoise()
{
	if (OwningTool)
	{
		OwningTool->ExecuteApplyNoise();
	}
}

UInteractiveTool* UQuick_RoadHeightfieldNoiseToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UQuick_RoadHeightfieldNoiseTool>(SceneState.ToolManager);
}

void UQuick_RoadHeightfieldNoiseTool::Setup()
{
	Properties = NewObject<UQuick_RoadHeightfieldNoiseToolProperties>(this);
	Properties->SetOwningTool(this);
	AddToolPropertySource(Properties);
}

void UQuick_RoadHeightfieldNoiseTool::ExecuteApplyNoise()
{
	if (!Properties)
	{
		return;
	}

	ALandscape* Landscape = FQuick_RoadLandscapeHeightEdit::FindSelectedLandscape();
	FQuick_RoadLandscapeHeightBuffer Buffer;
	FText ErrorMessage;
	if (!FQuick_RoadLandscapeHeightEdit::ReadSelectedLandscapeHeights(Landscape, Buffer, ErrorMessage))
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		return;
	}

	const FQuick_RoadHeightfieldNoiseParams Params = Properties->BuildParams();

	UE_LOG(LogQuickRoad, Display, TEXT("Quick_Road HeightField Noise 开始 (%dx%d)"), Buffer.Width, Buffer.Height);
	FQuick_RoadHeightfieldNoise::ApplyNoise(Buffer.FloatHeights, Buffer.Width, Buffer.Height, Params);

	if (!FQuick_RoadLandscapeHeightEdit::WriteLandscapeHeights(
		Landscape, Buffer, LOCTEXT("NoiseTransaction", "Quick Road HeightField Noise")))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("WriteFailed", "写入 Landscape 高度失败。"));
		return;
	}

	UE_LOG(LogQuickRoad, Display, TEXT("Quick_Road HeightField Noise 完成。"));
}

#undef LOCTEXT_NAMESPACE
