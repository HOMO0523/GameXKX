#pragma once
#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"
class UTextBlock;
class UWidgetTree;
class UPanelWidget;

namespace GameXXKCardNameStyle
{
	/** Call after setting the normal font/color. Common cards keep that appearance. */
	GAMEXXK_API void Apply(UTextBlock* Text, EGameXXKCardQuality Quality, int32 NormalOutlineSize = 0);
	/** A thin non-interactive frame, transparent at the center and hidden at common quality. */
	GAMEXXK_API void AttachFrame(UWidgetTree* Tree, UPanelWidget* Face, UTextBlock* Title,
		const FVector2D& ReferenceSize = FVector2D(206.0f,285.0f));
}
