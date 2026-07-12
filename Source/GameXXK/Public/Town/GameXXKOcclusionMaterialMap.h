#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

class GAMEXXK_API FGameXXKOcclusionMaterialMap
{
public:
	UMaterialInterface* Resolve(const UMaterialInterface* OriginalMaterial) const;
};
