#pragma once
#include "CoreMinimal.h"
#include "GameXXKEquipmentTypes.h"
class UWidgetTree;
class UButton;
class UImage;
class UTextBlock;

/** Shared equipment presentation: stable fill, animated outline/frame, local aura behind art. */
namespace GameXXKEquipmentQualityStyle
{
	GAMEXXK_API void ApplyName(UTextBlock* Text,EGameXXKEquipmentQuality Quality);
	GAMEXXK_API void ApplySlot(UWidgetTree* Tree,UButton* Button,UImage* Art,EGameXXKEquipmentQuality Quality,FVector2D ReferenceSize);
	GAMEXXK_API FString MaterialPath(EGameXXKEquipmentQuality Quality,const TCHAR* Layer);
}
