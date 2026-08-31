#pragma once

#include "CoreMinimal.h"

class GAMEXXK_API FGameXXKInventoryItemPresentation final
{
public:
	static FString ResolveIconPath(FName ItemId);
	static bool IsInspectable(FName ItemId);
	static FString InspectTexturePath(FName ItemId);
};

DECLARE_DELEGATE_RetVal(bool, FGameXXKTutorialMapInspectionRequested);
