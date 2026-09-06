#pragma once

#include "CoreMinimal.h"

class UScrollBox;

/** Shared PSD-only visual treatment for long PartyDeck lists. */
class GAMEXXK_API FGameXXKPartyDeckUiStyle final
{
public:
	/** Applies the approved paper track and ink thumb to a right-side UMG scroll bar. */
	static void ApplyPaperInkScrollBar(UScrollBox* ScrollBox);
	/** Native draggable ink thumb, using the backpack's approved ink artwork. */
	static void ApplyBackpackInkScrollBar(UScrollBox* ScrollBox, float Thickness = 18.0f, float MinimumThumbLength = 72.0f);
	static FString GetBackpackInkThumbResourcePath();

	static FString GetPaperTrackResourcePath();
	static FString GetInkThumbResourcePath();
};
