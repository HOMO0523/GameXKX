// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/Quick_RoadLandscapeErosion.h"
#include "Tools/Quick_RoadLandscapeHeightEdit.h"
#include "Tools/Quick_RoadErosionAlgorithms.h"
#include "Quick_RoadLog.h"

#include "Landscape.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "Quick_RoadLandscapeErosion"

ALandscape* FQuick_RoadLandscapeErosion::FindSelectedLandscape()
{
	return FQuick_RoadLandscapeHeightEdit::FindSelectedLandscape();
}

bool FQuick_RoadLandscapeErosion::ApplyErosionToLandscape(
	ALandscape* Landscape,
	const FQuick_RoadErosionJobSettings& Settings,
	FText& OutErrorMessage)
{
	FQuick_RoadLandscapeHeightBuffer Buffer;
	if (!FQuick_RoadLandscapeHeightEdit::ReadSelectedLandscapeHeights(Landscape, Buffer, OutErrorMessage))
	{
		return false;
	}

	const int32 Width = Buffer.Width;
	const int32 Height = Buffer.Height;

	auto LogProgress = [](float NormalizedProgress)
	{
		const float Percent = FMath::Clamp(NormalizedProgress, 0.f, 1.f) * 100.f;
		UE_LOG(LogQuickRoad, Display, TEXT("Quick_Road 自动侵蚀进度: %.1f%%"), Percent);
	};

	UE_LOG(LogQuickRoad, Display, TEXT("Quick_Road 自动侵蚀开始 (%dx%d)"), Width, Height);
	LogProgress(0.f);

	switch (Settings.Algorithm)
	{
	case EQuick_RoadErosionAlgorithmType::HydraulicParticle:
		FQuick_RoadErosionAlgorithms::RunHydraulicParticle(
			Buffer.FloatHeights, Width, Height, Settings.Hydraulic, Settings.Seed, LogProgress);
		break;
	case EQuick_RoadErosionAlgorithmType::ThermalTalus:
		FQuick_RoadErosionAlgorithms::RunThermalTalus(
			Buffer.FloatHeights, Width, Height, Settings.Thermal, LogProgress);
		break;
	case EQuick_RoadErosionAlgorithmType::Combined:
		FQuick_RoadErosionAlgorithms::RunCombined(
			Buffer.FloatHeights, Width, Height, Settings, LogProgress);
		break;
	default:
		OutErrorMessage = LOCTEXT("UnknownAlgorithm", "未知侵蚀算法。");
		return false;
	}

	if (Settings.MacroSmoothPasses > 0)
	{
		FQuick_RoadErosionAlgorithms::ApplyMacroSmooth(Buffer.FloatHeights, Width, Height, Settings.MacroSmoothPasses);
	}

	if (Settings.PeakSharpenStrength > 0.f && Settings.PeakSharpenPasses > 0)
	{
		FQuick_RoadErosionAlgorithms::ApplyPeakSharpen(
			Buffer.FloatHeights, Width, Height, Settings.PeakSharpenStrength, Settings.PeakSharpenPasses);
	}

	LogProgress(1.f);

	if (!FQuick_RoadLandscapeHeightEdit::WriteLandscapeHeights(
		Landscape, Buffer, LOCTEXT("ErosionTransaction", "Quick Road Auto Erosion")))
	{
		OutErrorMessage = LOCTEXT("WriteFailed", "写入 Landscape 高度失败。");
		return false;
	}

	UE_LOG(LogQuickRoad, Display, TEXT("Quick_Road 自动侵蚀完成。"));
	return true;
}

#undef LOCTEXT_NAMESPACE
