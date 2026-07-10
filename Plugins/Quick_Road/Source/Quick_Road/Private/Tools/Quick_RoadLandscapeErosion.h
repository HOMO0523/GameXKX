// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/Quick_RoadErosionAlgorithms.h"

class ALandscape;

class FQuick_RoadLandscapeErosion
{
public:
	static ALandscape* FindSelectedLandscape();
	static bool ApplyErosionToLandscape(ALandscape* Landscape, const FQuick_RoadErosionJobSettings& Settings, FText& OutErrorMessage);
};
