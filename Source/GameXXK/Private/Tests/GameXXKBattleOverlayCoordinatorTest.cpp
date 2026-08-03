#include "Misc/AutomationTest.h"

#include <initializer_list>

#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattleOverlayCoordinator.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	class FRecordingBattleOverlayHost final : public IGameXXKBattleOverlayHost
	{
	public:
		FGameXXKBattleOverlaySnapshot SnapshotToCapture;
		bool bApplySucceeds = true;
		bool bReenterEnterDuringCapture = false;
		bool bExitDuringApply = false;
		bool bReenterExitDuringCancel = false;
		UGameXXKBattleOverlayCoordinator* Coordinator = nullptr;
		UGameXXKBattleBoardWidget* CaptureReentryBattleWidget = nullptr;

		mutable TArray<FString> Calls;
		mutable int32 CaptureCount = 0;
		mutable bool bCaptureReentrantEnterResult = true;
		int32 ApplyCount = 0;
		int32 CancelCount = 0;
		int32 RestoreCount = 0;
		mutable const UGameXXKOneGameRouteMapWidget* CapturedRouteWidget = nullptr;
		UGameXXKOneGameRouteMapWidget* AppliedRouteWidget = nullptr;
		UGameXXKBattleBoardWidget* AppliedBattleWidget = nullptr;
		UGameXXKOneGameRouteMapWidget* RestoredRouteWidget = nullptr;
		UGameXXKBattleBoardWidget* RestoredBattleWidget = nullptr;
		uint64 AppliedSessionToken = 0;
		uint64 CancelledSessionToken = 0;
		bool bClosingSessionWasCurrentDuringCancel = true;
		uint64 SessionTokenObservedDuringCancel = MAX_uint64;
		FGameXXKBattleOverlaySnapshot RestoredSnapshot;

		virtual FGameXXKBattleOverlaySnapshot CaptureBattleOverlaySnapshot(
			const UGameXXKOneGameRouteMapWidget& RouteWidget) const override
		{
			Calls.Add(TEXT("Capture"));
			++CaptureCount;
			CapturedRouteWidget = &RouteWidget;
			if (bReenterEnterDuringCapture && CaptureCount == 1 && Coordinator && CaptureReentryBattleWidget)
			{
				FRecordingBattleOverlayHost& MutableHost = const_cast<FRecordingBattleOverlayHost&>(*this);
				UGameXXKOneGameRouteMapWidget& MutableRouteWidget = const_cast<UGameXXKOneGameRouteMapWidget&>(RouteWidget);
				bCaptureReentrantEnterResult = Coordinator->Enter(MutableHost, MutableRouteWidget, *CaptureReentryBattleWidget);
			}

			FGameXXKBattleOverlaySnapshot Snapshot = SnapshotToCapture;
			Snapshot.RouteVisibility = RouteWidget.GetVisibility();
			Snapshot.RouteScrollOffset = RouteWidget.GetCurrentScrollOffset();
			return Snapshot;
		}

		virtual bool ApplyBattleOverlayEntry(
			UGameXXKOneGameRouteMapWidget& RouteWidget,
			UGameXXKBattleBoardWidget& BattleWidget,
			uint64 SessionToken) override
		{
			Calls.Add(TEXT("Apply"));
			++ApplyCount;
			AppliedRouteWidget = &RouteWidget;
			AppliedBattleWidget = &BattleWidget;
			AppliedSessionToken = SessionToken;
			RouteWidget.SetVisibility(ESlateVisibility::Collapsed);
			RouteWidget.RestoreScrollOffset(0.0f);
			if (bExitDuringApply && Coordinator)
			{
				Coordinator->Exit(*this);
			}
			return bApplySucceeds;
		}

		virtual void CancelBattleVisualLoads(uint64 ClosingSessionToken) override
		{
			Calls.Add(TEXT("Cancel"));
			++CancelCount;
			CancelledSessionToken = ClosingSessionToken;
			if (Coordinator)
			{
				bClosingSessionWasCurrentDuringCancel = Coordinator->IsCurrentSession(ClosingSessionToken);
				SessionTokenObservedDuringCancel = Coordinator->GetSessionToken();
				if (bReenterExitDuringCancel && CancelCount == 1)
				{
					Coordinator->Exit(*this);
				}
			}
		}

		virtual void RestoreBattleOverlaySnapshot(
			const FGameXXKBattleOverlaySnapshot& Snapshot,
			UGameXXKOneGameRouteMapWidget& RouteWidget,
			UGameXXKBattleBoardWidget& BattleWidget) override
		{
			Calls.Add(TEXT("Restore"));
			++RestoreCount;
			RestoredSnapshot = Snapshot;
			RestoredRouteWidget = &RouteWidget;
			RestoredBattleWidget = &BattleWidget;
			RouteWidget.SetVisibility(Snapshot.RouteVisibility);
			RouteWidget.RestoreScrollOffset(Snapshot.RouteScrollOffset);
		}
	};

	FGameXXKBattleOverlaySnapshot MakeNonDefaultSnapshot()
	{
		FGameXXKBattleOverlaySnapshot Before;
		Before.bGamePaused = true;
		Before.bWorldRenderingEnabled = false;
		Before.bShowMouseCursor = false;
		Before.bEnableClickEvents = false;
		Before.bEnableMouseOverEvents = true;
		Before.bMoveInputIgnored = true;
		Before.bLookInputIgnored = false;
		Before.InputMode = EGameXXKTrackedInputMode::GameOnly;
		Before.RouteVisibility = ESlateVisibility::SelfHitTestInvisible;
		Before.RouteScrollOffset = 713.0f;
		return Before;
	}

	void ConfigureRouteForSnapshot(
		UGameXXKOneGameRouteMapWidget& RouteWidget,
		const FGameXXKBattleOverlaySnapshot& Snapshot)
	{
		RouteWidget.SetRouteMapViewportGeometry(FVector2D::ZeroVector, FVector2D(1280.0f, 100.0f));
		RouteWidget.SetVisibility(Snapshot.RouteVisibility);
		RouteWidget.RestoreScrollOffset(Snapshot.RouteScrollOffset);
	}

	void TestSnapshotsEqual(
		FAutomationTestBase& Test,
		const FGameXXKBattleOverlaySnapshot& Expected,
		const FGameXXKBattleOverlaySnapshot& Actual)
	{
		Test.TestEqual(TEXT("paused state restores exactly"), Actual.bGamePaused, Expected.bGamePaused);
		Test.TestEqual(TEXT("world-rendering state restores exactly"), Actual.bWorldRenderingEnabled, Expected.bWorldRenderingEnabled);
		Test.TestEqual(TEXT("cursor state restores exactly"), Actual.bShowMouseCursor, Expected.bShowMouseCursor);
		Test.TestEqual(TEXT("click-event state restores exactly"), Actual.bEnableClickEvents, Expected.bEnableClickEvents);
		Test.TestEqual(TEXT("mouse-over state restores exactly"), Actual.bEnableMouseOverEvents, Expected.bEnableMouseOverEvents);
		Test.TestEqual(TEXT("move-input state restores exactly"), Actual.bMoveInputIgnored, Expected.bMoveInputIgnored);
		Test.TestEqual(TEXT("look-input state restores exactly"), Actual.bLookInputIgnored, Expected.bLookInputIgnored);
		Test.TestEqual(TEXT("input mode restores exactly"), Actual.InputMode, Expected.InputMode);
		Test.TestEqual(TEXT("route visibility restores exactly"), Actual.RouteVisibility, Expected.RouteVisibility);
		Test.TestEqual(TEXT("route scroll offset restores exactly"), Actual.RouteScrollOffset, Expected.RouteScrollOffset);
	}

	bool TestCallOrder(
		FAutomationTestBase& Test,
		const TCHAR* Context,
		const TArray<FString>& Actual,
		std::initializer_list<const TCHAR*> Expected)
	{
		bool bMatches = Actual.Num() == static_cast<int32>(Expected.size());
		int32 Index = 0;
		for (const TCHAR* ExpectedCall : Expected)
		{
			bMatches = bMatches && Actual.IsValidIndex(Index) && Actual[Index] == ExpectedCall;
			++Index;
		}
		return Test.TestTrue(Context, bMatches);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleOverlayCoordinatorTest,
	"GameXXK.MVP.Battle.OverlayCoordinator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleOverlayCoordinatorTest::RunTest(const FString& Parameters)
{
	const FGameXXKBattleOverlaySnapshot Before = MakeNonDefaultSnapshot();

	UGameXXKBattleOverlayCoordinator* Coordinator = NewObject<UGameXXKBattleOverlayCoordinator>();
	UGameXXKOneGameRouteMapWidget* RouteWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	UGameXXKBattleBoardWidget* BattleWidget = NewObject<UGameXXKBattleBoardWidget>();
	TestNotNull(TEXT("coordinator exists"), Coordinator);
	TestNotNull(TEXT("route widget exists"), RouteWidget);
	TestNotNull(TEXT("battle widget exists"), BattleWidget);
	if (!Coordinator || !RouteWidget || !BattleWidget)
	{
		return false;
	}

	ConfigureRouteForSnapshot(*RouteWidget, Before);
	FRecordingBattleOverlayHost Host;
	Host.SnapshotToCapture = Before;
	Host.Coordinator = Coordinator;

	TestFalse(TEXT("coordinator starts inactive"), Coordinator->IsActive());
	TestEqual(TEXT("coordinator starts without a session token"), Coordinator->GetSessionToken(), uint64(0));
	TestTrue(TEXT("ordinary overlay entry succeeds"), Coordinator->Enter(Host, *RouteWidget, *BattleWidget));
	const uint64 SessionA = Coordinator->GetSessionToken();
	TestTrue(TEXT("entry creates a nonzero session token"), SessionA != 0);
	TestTrue(TEXT("entry activates the coordinator"), Coordinator->IsActive());
	TestTrue(TEXT("session A is current while active"), Coordinator->IsCurrentSession(SessionA));
	TestTrue(TEXT("entry captures the exact route widget"), Host.CapturedRouteWidget == RouteWidget);
	TestEqual(TEXT("entry applies to the exact route widget"), Host.AppliedRouteWidget, RouteWidget);
	TestEqual(TEXT("entry applies to the exact battle widget"), Host.AppliedBattleWidget, BattleWidget);
	TestEqual(TEXT("entry passes its current session token"), Host.AppliedSessionToken, SessionA);
	TestCallOrder(*this, TEXT("ordinary entry captures before applying"), Host.Calls, {TEXT("Capture"), TEXT("Apply")});

	TestFalse(TEXT("repeated entry while active is rejected"), Coordinator->Enter(Host, *RouteWidget, *BattleWidget));
	TestEqual(TEXT("repeated entry does not recapture"), Host.CaptureCount, 1);
	TestEqual(TEXT("repeated entry does not reapply"), Host.ApplyCount, 1);

	Coordinator->Exit(Host);
	TestFalse(TEXT("ordinary exit deactivates the coordinator"), Coordinator->IsActive());
	TestEqual(TEXT("ordinary exit invalidates the public session token"), Coordinator->GetSessionToken(), uint64(0));
	TestFalse(TEXT("session A callback is invalid after exit"), Coordinator->IsCurrentSession(SessionA));
	TestFalse(TEXT("closing token is invalid before cancellation"), Host.bClosingSessionWasCurrentDuringCancel);
	TestEqual(TEXT("zero token is observable before cancellation"), Host.SessionTokenObservedDuringCancel, uint64(0));
	TestEqual(TEXT("cancellation receives the closing session token"), Host.CancelledSessionToken, SessionA);
	TestCallOrder(*this, TEXT("ordinary exit cancels before restoring"), Host.Calls, {TEXT("Capture"), TEXT("Apply"), TEXT("Cancel"), TEXT("Restore")});
	TestEqual(TEXT("ordinary exit restores once"), Host.RestoreCount, 1);
	TestEqual(TEXT("ordinary exit restores the same route widget"), Host.RestoredRouteWidget, RouteWidget);
	TestEqual(TEXT("ordinary exit restores the same battle widget"), Host.RestoredBattleWidget, BattleWidget);
	TestSnapshotsEqual(*this, Before, Host.RestoredSnapshot);
	TestEqual(TEXT("route visibility is restored on the same widget"), RouteWidget->GetVisibility(), Before.RouteVisibility);
	TestEqual(TEXT("route scroll is restored on the same widget"), RouteWidget->GetCurrentScrollOffset(), Before.RouteScrollOffset);

	Coordinator->Exit(Host);
	TestEqual(TEXT("repeated exit cancels only once"), Host.CancelCount, 1);
	TestEqual(TEXT("repeated exit restores only once"), Host.RestoreCount, 1);

	TestTrue(TEXT("session B can enter after session A exits"), Coordinator->Enter(Host, *RouteWidget, *BattleWidget));
	const uint64 SessionB = Coordinator->GetSessionToken();
	TestTrue(TEXT("session tokens increase monotonically"), SessionB > SessionA);
	TestFalse(TEXT("session A callback remains invalid after session B starts"), Coordinator->IsCurrentSession(SessionA));
	TestTrue(TEXT("session B callback is current"), Coordinator->IsCurrentSession(SessionB));
	Coordinator->Exit(Host);

	UGameXXKBattleOverlayCoordinator* FailedCoordinator = NewObject<UGameXXKBattleOverlayCoordinator>();
	UGameXXKOneGameRouteMapWidget* FailedRouteWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	UGameXXKBattleBoardWidget* FailedBattleWidget = NewObject<UGameXXKBattleBoardWidget>();
	ConfigureRouteForSnapshot(*FailedRouteWidget, Before);
	FRecordingBattleOverlayHost FailedHost;
	FailedHost.SnapshotToCapture = Before;
	FailedHost.bApplySucceeds = false;
	FailedHost.Coordinator = FailedCoordinator;
	TestFalse(TEXT("failed entry reports failure"), FailedCoordinator->Enter(FailedHost, *FailedRouteWidget, *FailedBattleWidget));
	TestFalse(TEXT("failed entry ends inactive"), FailedCoordinator->IsActive());
	TestEqual(TEXT("failed entry invalidates its session"), FailedCoordinator->GetSessionToken(), uint64(0));
	TestCallOrder(*this, TEXT("failed entry rolls back immediately"), FailedHost.Calls, {TEXT("Capture"), TEXT("Apply"), TEXT("Cancel"), TEXT("Restore")});
	TestFalse(TEXT("failed entry invalidates before cancellation"), FailedHost.bClosingSessionWasCurrentDuringCancel);
	TestEqual(TEXT("failed entry cancels once"), FailedHost.CancelCount, 1);
	TestEqual(TEXT("failed entry restores once"), FailedHost.RestoreCount, 1);
	TestEqual(TEXT("failed entry restores the exact route widget"), FailedHost.RestoredRouteWidget, FailedRouteWidget);
	TestEqual(TEXT("failed entry restores route visibility"), FailedRouteWidget->GetVisibility(), Before.RouteVisibility);
	TestEqual(TEXT("failed entry restores route scroll"), FailedRouteWidget->GetCurrentScrollOffset(), Before.RouteScrollOffset);
	TestSnapshotsEqual(*this, Before, FailedHost.RestoredSnapshot);

	UGameXXKBattleOverlayCoordinator* ReentrantCoordinator = NewObject<UGameXXKBattleOverlayCoordinator>();
	UGameXXKOneGameRouteMapWidget* ReentrantRouteWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	UGameXXKBattleBoardWidget* ReentrantBattleWidget = NewObject<UGameXXKBattleBoardWidget>();
	ConfigureRouteForSnapshot(*ReentrantRouteWidget, Before);
	FRecordingBattleOverlayHost ReentrantHost;
	ReentrantHost.SnapshotToCapture = Before;
	ReentrantHost.bReenterExitDuringCancel = true;
	ReentrantHost.Coordinator = ReentrantCoordinator;
	TestTrue(TEXT("reentrant-exit scenario enters"), ReentrantCoordinator->Enter(ReentrantHost, *ReentrantRouteWidget, *ReentrantBattleWidget));
	ReentrantCoordinator->Exit(ReentrantHost);
	TestEqual(TEXT("reentrant exit cancels only once"), ReentrantHost.CancelCount, 1);
	TestEqual(TEXT("reentrant exit restores only once"), ReentrantHost.RestoreCount, 1);
	TestFalse(TEXT("reentrant exit ends inactive"), ReentrantCoordinator->IsActive());

	UGameXXKBattleOverlayCoordinator* ApplyExitCoordinator = NewObject<UGameXXKBattleOverlayCoordinator>();
	UGameXXKOneGameRouteMapWidget* ApplyExitRouteWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	UGameXXKBattleBoardWidget* ApplyExitBattleWidget = NewObject<UGameXXKBattleBoardWidget>();
	ConfigureRouteForSnapshot(*ApplyExitRouteWidget, Before);
	FRecordingBattleOverlayHost ApplyExitHost;
	ApplyExitHost.SnapshotToCapture = Before;
	ApplyExitHost.bExitDuringApply = true;
	ApplyExitHost.Coordinator = ApplyExitCoordinator;
	TestFalse(TEXT("entry reports failure when Apply synchronously exits"), ApplyExitCoordinator->Enter(ApplyExitHost, *ApplyExitRouteWidget, *ApplyExitBattleWidget));
	TestEqual(TEXT("Apply-triggered exit cancels once"), ApplyExitHost.CancelCount, 1);
	TestEqual(TEXT("Apply-triggered exit restores once"), ApplyExitHost.RestoreCount, 1);
	TestFalse(TEXT("Apply-triggered exit leaves the coordinator inactive"), ApplyExitCoordinator->IsActive());

	UGameXXKBattleOverlayCoordinator* CaptureReentryCoordinator = NewObject<UGameXXKBattleOverlayCoordinator>();
	UGameXXKOneGameRouteMapWidget* CaptureReentryRouteWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	UGameXXKBattleBoardWidget* CaptureReentryBattleWidget = NewObject<UGameXXKBattleBoardWidget>();
	ConfigureRouteForSnapshot(*CaptureReentryRouteWidget, Before);
	FRecordingBattleOverlayHost CaptureReentryHost;
	CaptureReentryHost.SnapshotToCapture = Before;
	CaptureReentryHost.bReenterEnterDuringCapture = true;
	CaptureReentryHost.Coordinator = CaptureReentryCoordinator;
	CaptureReentryHost.CaptureReentryBattleWidget = CaptureReentryBattleWidget;
	TestTrue(TEXT("capture-reentry scenario outer entry succeeds"), CaptureReentryCoordinator->Enter(CaptureReentryHost, *CaptureReentryRouteWidget, *CaptureReentryBattleWidget));
	TestFalse(TEXT("entry rejects recursive capture"), CaptureReentryHost.bCaptureReentrantEnterResult);
	TestEqual(TEXT("recursive capture does not recapture"), CaptureReentryHost.CaptureCount, 1);
	TestEqual(TEXT("recursive capture does not double-apply"), CaptureReentryHost.ApplyCount, 1);
	CaptureReentryCoordinator->Exit(CaptureReentryHost);

	UGameXXKBattleOverlayCoordinator* TeardownCoordinator = NewObject<UGameXXKBattleOverlayCoordinator>();
	UGameXXKOneGameRouteMapWidget* TeardownRouteWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	UGameXXKBattleBoardWidget* TeardownBattleWidget = NewObject<UGameXXKBattleBoardWidget>();
	ConfigureRouteForSnapshot(*TeardownRouteWidget, Before);
	FRecordingBattleOverlayHost TeardownHost;
	TeardownHost.SnapshotToCapture = Before;
	TeardownHost.Coordinator = TeardownCoordinator;
	TestTrue(TEXT("teardown scenario enters"), TeardownCoordinator->Enter(TeardownHost, *TeardownRouteWidget, *TeardownBattleWidget));
	TeardownCoordinator->Exit(TeardownHost);
	TestEqual(TEXT("explicit teardown cleanup restores once"), TeardownHost.RestoreCount, 1);
	TestEqual(TEXT("explicit teardown cleanup restores scroll"), TeardownRouteWidget->GetCurrentScrollOffset(), Before.RouteScrollOffset);

	UGameXXKBattleOverlayCoordinator* TravelCoordinator = NewObject<UGameXXKBattleOverlayCoordinator>();
	UGameXXKOneGameRouteMapWidget* TravelRouteWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	UGameXXKBattleBoardWidget* TravelBattleWidget = NewObject<UGameXXKBattleBoardWidget>();
	ConfigureRouteForSnapshot(*TravelRouteWidget, Before);
	FRecordingBattleOverlayHost TravelHost;
	TravelHost.SnapshotToCapture = Before;
	TravelHost.Coordinator = TravelCoordinator;
	TestTrue(TEXT("pre-travel scenario enters"), TravelCoordinator->Enter(TravelHost, *TravelRouteWidget, *TravelBattleWidget));
	TravelCoordinator->Exit(TravelHost);
	TestEqual(TEXT("explicit pre-travel cleanup restores once"), TravelHost.RestoreCount, 1);
	TestEqual(TEXT("explicit pre-travel cleanup restores scroll"), TravelRouteWidget->GetCurrentScrollOffset(), Before.RouteScrollOffset);

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestTrue(TEXT("scroll test starts a game"), Subsystem->StartGame());
	TestTrue(TEXT("scroll test selects Qingshan"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("scroll test accepts the quest"), Subsystem->AcceptQuest());
	TestTrue(TEXT("scroll test opens the dungeon"), Subsystem->OpenDungeonFromTownExit());
	UGameXXKOneGameRouteMapWidget* ScrollRouteWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	ScrollRouteWidget->SetMVPSubsystem(Subsystem);
	ScrollRouteWidget->SetRouteMapViewportGeometry(FVector2D::ZeroVector, FVector2D(1280.0f, 100.0f));
	TestTrue(TEXT("scroll test widget initializes"), ScrollRouteWidget->Initialize());
	ScrollRouteWidget->NativeConstruct();
	TestTrue(TEXT("scroll test has enough clamped range"), ScrollRouteWidget->GetMaxScrollOffsetForTest() >= Before.RouteScrollOffset);
	ScrollRouteWidget->RestoreScrollOffset(Before.RouteScrollOffset);
	TestEqual(TEXT("scroll accessor returns the exact restored offset"), ScrollRouteWidget->GetCurrentScrollOffset(), Before.RouteScrollOffset);
	ScrollRouteWidget->RefreshFromState();
	TestEqual(TEXT("refresh preserves scroll after initial initialization"), ScrollRouteWidget->GetCurrentScrollOffset(), Before.RouteScrollOffset);
	ScrollRouteWidget->RestoreScrollOffset(100000.0f);
	TestEqual(TEXT("scroll restoration preserves upper clamping"), ScrollRouteWidget->GetCurrentScrollOffset(), ScrollRouteWidget->GetMaxScrollOffsetForTest());
	ScrollRouteWidget->RestoreScrollOffset(-100.0f);
	TestEqual(TEXT("scroll restoration preserves lower clamping"), ScrollRouteWidget->GetCurrentScrollOffset(), 0.0f);

	return true;
}

#endif
