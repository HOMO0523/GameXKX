// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UWorld;

struct FQuick_RoadGenerateTerrainSettings
{
	TObjectPtr<UMaterialInterface> Material = nullptr;
	int32 QuadsPerSection = 63;
	int32 SectionsPerComponent = 2;
	FIntPoint ComponentCount = FIntPoint(8, 8);
	FString HeightmapFilePath;
};

class FQuick_RoadLandscapeCreator
{
public:
	static bool TryCreateLandscape(UWorld* World, const FQuick_RoadGenerateTerrainSettings& Settings, FText& OutErrorMessage);
	static void ComputeGridStats(int32 QuadsPerSection, int32 SectionsPerComponent, FIntPoint ComponentCount, int32& OutResolutionX, int32& OutResolutionY, int32& OutTotalComponents);
	static bool IsValidSectionSize(int32 QuadsPerSection);
	static bool IsValidSectionsPerComponent(int32 SectionsPerComponent);
};
