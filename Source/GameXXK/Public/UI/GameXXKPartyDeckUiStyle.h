#pragma once

#include "CoreMinimal.h"

class UScrollBox;

/** Shared PSD-only visual treatment for long PartyDeck lists. */
class GAMEXXK_API FGameXXKPartyDeckUiStyle final
{
public:
	/** Applies the approved paper track and ink thumb to a right-side UMG scroll bar. */
	static void ApplyPaperInkScrollBar(UScrollBox* ScrollBox);

	static FString GetPaperTrackResourcePath();
	static FString GetInkThumbResourcePath();
};
