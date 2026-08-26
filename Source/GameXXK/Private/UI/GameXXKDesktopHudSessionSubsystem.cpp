#include "UI/GameXXKDesktopHudSessionSubsystem.h"

void UGameXXKDesktopHudSessionSubsystem::StoreForMapTravel(
	const FGameXXKDesktopWorkbenchSessionState& State)
{
	PendingState = State;
}

bool UGameXXKDesktopHudSessionSubsystem::ConsumeAfterMapTravel(
	FGameXXKDesktopWorkbenchSessionState& OutState)
{
	if (!PendingState.IsSet())
	{
		return false;
	}
	OutState = MoveTemp(PendingState.GetValue());
	PendingState.Reset();
	return OutState.bValid;
}

void UGameXXKDesktopHudSessionSubsystem::DiscardPending()
{
	PendingState.Reset();
}
