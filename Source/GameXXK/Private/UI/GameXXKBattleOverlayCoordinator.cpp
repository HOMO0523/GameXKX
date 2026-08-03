#include "UI/GameXXKBattleOverlayCoordinator.h"

#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"

bool UGameXXKBattleOverlayCoordinator::Enter(
	IGameXXKBattleOverlayHost& Host,
	UGameXXKOneGameRouteMapWidget& RouteWidget,
	UGameXXKBattleBoardWidget& BattleWidget)
{
	if (bActive || bEnterInProgress || bExitInProgress || LastIssuedSessionToken == MAX_uint64)
	{
		return false;
	}

	bEnterInProgress = true;
	bExitRequestedDuringEnter = false;
	SavedSnapshot = Host.CaptureBattleOverlaySnapshot(RouteWidget);
	if (bExitRequestedDuringEnter)
	{
		SavedRouteWidget.Reset();
		SavedBattleWidget.Reset();
		SavedSnapshot = FGameXXKBattleOverlaySnapshot();
		CurrentSessionToken = 0;
		bSessionValid = false;
		bActive = false;
		bExitRequestedDuringEnter = false;
		bEnterInProgress = false;
		return false;
	}
	SavedRouteWidget = &RouteWidget;
	SavedBattleWidget = &BattleWidget;

	++LastIssuedSessionToken;
	CurrentSessionToken = LastIssuedSessionToken;
	bSessionValid = true;
	bActive = true;

	const uint64 EnteredSessionToken = CurrentSessionToken;
	const bool bApplied = Host.ApplyBattleOverlayEntry(RouteWidget, BattleWidget, EnteredSessionToken);
	bEnterInProgress = false;
	if (bExitRequestedDuringEnter)
	{
		bExitRequestedDuringEnter = false;
		Exit(Host);
		return false;
	}
	if (!bApplied || !IsCurrentSession(EnteredSessionToken))
	{
		if (bActive)
		{
			Exit(Host);
		}
		return false;
	}

	return true;
}

void UGameXXKBattleOverlayCoordinator::Exit(IGameXXKBattleOverlayHost& Host)
{
	if (bEnterInProgress)
	{
		bExitRequestedDuringEnter = true;
		InvalidateSession();
		return;
	}
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
	Host.RestoreBattleOverlaySnapshot(SavedSnapshot, RouteWidget, BattleWidget);

	SavedRouteWidget.Reset();
	SavedBattleWidget.Reset();
	SavedSnapshot = FGameXXKBattleOverlaySnapshot();
	CurrentSessionToken = 0;
	bActive = false;
	bExitRequestedDuringEnter = false;
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

#if WITH_DEV_AUTOMATION_TESTS
void UGameXXKBattleOverlayCoordinator::SetLastIssuedSessionTokenForTest(uint64 SessionToken)
{
	if (!bActive && !bEnterInProgress && !bExitInProgress)
	{
		LastIssuedSessionToken = SessionToken;
	}
}
#endif
