// Copyright Aoife. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ALandscape;

namespace QuickRoadLandscapeCompat
{
	bool IsLayerEditable(const ALandscape* Landscape, const FGuid& LayerGuid);
	FGuid ResolveEditingLayerGuid(ALandscape* Landscape);
	bool IsLayerLocked(const ALandscape* Landscape, const FGuid& LayerGuid);
	FString GetLayerDisplayName(const ALandscape* Landscape, const FGuid& LayerGuid);
}
