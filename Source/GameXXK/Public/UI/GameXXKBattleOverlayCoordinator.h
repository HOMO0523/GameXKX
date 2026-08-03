#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "UObject/Object.h"
#include "GameXXKBattleOverlayCoordinator.generated.h"

class UGameXXKBattleBoardWidget;
class UGameXXKOneGameRouteMapWidget;

enum class EGameXXKTrackedInputMode : uint8
{
	GameOnly,
	GameAndUI,
	UIOnly
};

struct GAMEXXK_API FGameXXKBattleOverlaySnapshot
{
	bool bGamePaused = false;
	bool bWorldRenderingEnabled = true;
	bool bShowMouseCursor = false;
	bool bEnableClickEvents = false;
	bool bEnableMouseOverEvents = false;
	bool bMoveInputIgnored = false;
	bool bLookInputIgnored = false;
	EGameXXKTrackedInputMode InputMode = EGameXXKTrackedInputMode::GameAndUI;
	ESlateVisibility RouteVisibility = ESlateVisibility::Visible;
	float RouteScrollOffset = 0.0f;
};

class GAMEXXK_API IGameXXKBattleOverlayHost
{
public:
	virtual ~IGameXXKBattleOverlayHost() = default;

	virtual FGameXXKBattleOverlaySnapshot CaptureBattleOverlaySnapshot(
		const UGameXXKOneGameRouteMapWidget& RouteWidget) const = 0;
	virtual bool ApplyBattleOverlayEntry(
		UGameXXKOneGameRouteMapWidget& RouteWidget,
		UGameXXKBattleBoardWidget& BattleWidget,
		uint64 SessionToken) = 0;
	virtual void CancelBattleVisualLoads(uint64 ClosingSessionToken) = 0;
	virtual void RestoreBattleOverlaySnapshot(
		const FGameXXKBattleOverlaySnapshot& Snapshot,
		UGameXXKOneGameRouteMapWidget& RouteWidget,
		UGameXXKBattleBoardWidget& BattleWidget) = 0;
};

UCLASS()
class GAMEXXK_API UGameXXKBattleOverlayCoordinator : public UObject
{
	GENERATED_BODY()

public:
	bool Enter(
		IGameXXKBattleOverlayHost& Host,
		UGameXXKOneGameRouteMapWidget& RouteWidget,
		UGameXXKBattleBoardWidget& BattleWidget);
	void Exit(IGameXXKBattleOverlayHost& Host);
	void InvalidateSession();
	uint64 GetSessionToken() const;
	bool IsCurrentSession(uint64 Candidate) const;
	bool IsActive() const;

private:
	uint64 LastIssuedSessionToken = 0;
	uint64 CurrentSessionToken = 0;
	bool bSessionValid = false;
	bool bActive = false;
	bool bEnterInProgress = false;
	bool bExitInProgress = false;
	bool bHasSnapshot = false;
	FGameXXKBattleOverlaySnapshot SavedSnapshot;
	TWeakObjectPtr<UGameXXKOneGameRouteMapWidget> SavedRouteWidget;
	TWeakObjectPtr<UGameXXKBattleBoardWidget> SavedBattleWidget;
};
