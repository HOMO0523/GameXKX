#include "UI/GameXXKCardTooltipInteraction.h"

bool FGameXXKCardTooltipInteraction::Update(
	const bool bHovered, const bool bShiftDown, const bool bControlDown, const bool bEscapeDown)
{
	const EGameXXKCardTooltipMode Previous = Mode;
	if (!bHovered || !bWasHovered || bEscapeDown)
	{
		bPillsOpen = false;
	}
	else if (bControlDown && !bWasControlDown)
	{
		bPillsOpen = !bPillsOpen;
	}
	bWasHovered = bHovered;
	bWasControlDown = bControlDown;
	Mode = !bHovered ? EGameXXKCardTooltipMode::Compact
		: bShiftDown ? EGameXXKCardTooltipMode::Detail
		: bPillsOpen ? EGameXXKCardTooltipMode::Pills
		: EGameXXKCardTooltipMode::Compact;
	return Previous != Mode;
}

void FGameXXKCardTooltipInteraction::Reset()
{
	bWasHovered = false;
	bWasControlDown = false;
	bPillsOpen = false;
	Mode = EGameXXKCardTooltipMode::Compact;
}

EGameXXKCardTooltipMode FGameXXKCardTooltipInteraction::GetMode() const
{
	return Mode;
}
