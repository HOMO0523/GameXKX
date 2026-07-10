// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ALandscape;

struct FQuick_RoadLandscapeHeightBuffer
{
	int32 MinX = 0;
	int32 MinY = 0;
	int32 MaxX = 0;
	int32 MaxY = 0;
	int32 Width = 0;
	int32 Height = 0;
	TArray<float> FloatHeights;
	TArray<uint16> HeightData;
};

class FQuick_RoadLandscapeHeightEdit
{
public:
	static ALandscape* FindSelectedLandscape();

	/** 解析写入目标 Edit Layer；Quick_Road 模式下无当前层时自动选首个未锁定 Layer。 */
	static FGuid ResolveEditingLayerGuid(ALandscape* Landscape);

	static bool ReadSelectedLandscapeHeights(ALandscape* Landscape, FQuick_RoadLandscapeHeightBuffer& OutBuffer, FText& OutErrorMessage);

	static bool WriteLandscapeHeights(
		ALandscape* Landscape,
		const FQuick_RoadLandscapeHeightBuffer& Buffer,
		const FText& TransactionDescription);
};
