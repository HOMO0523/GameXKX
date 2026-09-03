#pragma once

#include "CoreMinimal.h"

enum class EGameXXKCardTooltipMode : uint8
{
	Compact,
	Detail,
	Pills
};

/** Hover-local reading state. Ctrl toggles once per press; Shift temporarily overrides it. */
struct GAMEXXK_API FGameXXKCardTooltipInteraction
{
	bool Update(bool bHovered, bool bShiftDown, bool bControlDown, bool bEscapeDown);
	void Reset();
	EGameXXKCardTooltipMode GetMode() const;

private:
	bool bWasHovered = false;
	bool bWasControlDown = false;
	bool bPillsOpen = false;
	EGameXXKCardTooltipMode Mode = EGameXXKCardTooltipMode::Compact;
};
