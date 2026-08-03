#include "UI/GameXXKBattleOverlayCoordinator.h"

#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"

bool UGameXXKBattleOverlayCoordinator::Enter(
	IGameXXKBattleOverlayHost& Host,
	UGameXXKOneGameRouteMapWidget& RouteWidget,
	UGameXXKBattleBoardWidget& BattleWidget)
{
	if (bActive || bEnterInProgress || bExitInProgress)
	{
		return false;
	}

	bEnterInProgress = true;
	SavedSnapshot = Host.CaptureBattleOverlaySnapshot(RouteWidget);
	bHasSnapshot = true;
	SavedRouteWidget = &RouteWidget;
	SavedBattleWidget = &BattleWidget;

	++LastIssuedSessionToken;
	if (LastIssuedSessionToken == 0)
	{
		++LastIssuedSessionToken;
	}
	CurrentSessionToken = LastIssuedSessionToken;
	bSessionValid = true;
	bActive = true;

	const uint64 EnteredSessionToken = CurrentSessionToken;
	const bool bApplied = Host.ApplyBattleOverlayEntry(RouteWidget, BattleWidget, EnteredSessionToken);
	if (!bApplied || !IsCurrentSession(EnteredSessionToken))
	{
		if (bActive)
		{
			Exit(Host);
		}
		bEnterInProgress = false;
		return false;
	}

	bEnterInProgress = false;
	return true;
}

void UGameXXKBattleOverlayCoordinator::Exit(IGameXXKBattleOverlayHost& Host)
{
	if (!bActive || bExitInProgress)
	{
		return;
	}

	bExitInProgress = true;
	const uint64 ClosingSessionToken = CurrentSessionToken;
	InvalidateSession();
	Host.CancelBattleVisualLoads(ClosingSessionToken);

	UGameXXKOneGameRouteMapWidget* RouteWidget = SavedRouteWidget.Get();
	UGameXXKBattleBoardWidget* BattleWidget = SavedBattleWidget.Get();
	if (bHasSnapshot && RouteWidget && BattleWidget)
	{
		Host.RestoreBattleOverlaySnapshot(SavedSnapshot, *RouteWidget, *BattleWidget);
	}

	SavedRouteWidget.Reset();
	SavedBattleWidget.Reset();
	SavedSnapshot = FGameXXKBattleOverlaySnapshot();
	bHasSnapshot = false;
	CurrentSessionToken = 0;
	bActive = false;
	bExitInProgress = false;
}

void UGameXXKBattleOverlayCoordinator::InvalidateSession()
{
	bSessionValid = false;
}

uint64 UGameXXKBattleOverlayCoordinator::GetSessionToken() const
{
	return bSessionValid ? CurrentSessionToken : 0;
}

bool UGameXXKBattleOverlayCoordinator::IsCurrentSession(uint64 Candidate) const
{
	return bActive && bSessionValid && Candidate != 0 && Candidate == CurrentSessionToken;
}

bool UGameXXKBattleOverlayCoordinator::IsActive() const
{
	return bActive;
}
