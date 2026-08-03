#include "Misc/AutomationTest.h"

#include <initializer_list>

#include "Components/ScrollBox.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattleOverlayCoordinator.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SWindow.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	class FRecordingBattleOverlayHost final : public IGameXXKBattleOverlayHost
	{
	public:
		FGameXXKBattleOverlaySnapshot SnapshotToCapture;
		bool bApplySucceeds = true;
		bool bReenterEnterDuringCapture = false;
		bool bExitDuringCapture = false;
		bool bExitDuringApply = false;
		bool bMutateRouteAfterExitDuringApply = false;
		bool bReenterEnterAfterExitDuringApply = false;
		bool bReenterExitDuringCancel = false;
		bool bReenterExitDuringRestore = false;
		bool bReenterEnterDuringRestore = false;
		UGameXXKBattleOverlayCoordinator* Coordinator = nullptr;
		UGameXXKBattleBoardWidget* CaptureReentryBattleWidget = nullptr;

		mutable TArray<FString> Calls;
		mutable int32 CaptureCount = 0;
		mutable bool bCaptureReentrantEnterResult = true;
		bool bApplyReentrantEnterResult = true;
		int32 ApplyCount = 0;
		int32 CancelCount = 0;
		int32 RestoreCount = 0;
		mutable const UGameXXKOneGameRouteMapWidget* CapturedRouteWidget = nullptr;
		UGameXXKOneGameRouteMapWidget* AppliedRouteWidget = nullptr;
		UGameXXKBattleBoardWidget* AppliedBattleWidget = nullptr;
		UGameXXKOneGameRouteMapWidget* RestoredRouteWidget = nullptr;
		UGameXXKBattleBoardWidget* RestoredBattleWidget = nullptr;
		uint64 AppliedSessionToken = 0;
		uint64 SessionTokenObservedAfterApplyExit = MAX_uint64;
		uint64 CancelledSessionToken = 0;
		bool bClosingSessionWasCurrentDuringCancel = true;
		uint64 SessionTokenObservedDuringCancel = MAX_uint64;
		uint64 SessionTokenObservedDuringRestore = MAX_uint64;
		bool bRestoreReentrantEnterResult = true;
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
			if (bExitDuringCapture && CaptureCount == 1 && Coordinator)
			{
				FRecordingBattleOverlayHost& MutableHost = const_cast<FRecordingBattleOverlayHost&>(*this);
				Coordinator->Exit(MutableHost);
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
				SessionTokenObservedAfterApplyExit = Coordinator->GetSessionToken();
				if (bReenterEnterAfterExitDuringApply)
				{
					bApplyReentrantEnterResult = Coordinator->Enter(*this, RouteWidget, BattleWidget);
				}
				if (bMutateRouteAfterExitDuringApply)
				{
					RouteWidget.SetVisibility(ESlateVisibility::Hidden);
					RouteWidget.RestoreScrollOffset(37.0f);
				}
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
			UGameXXKOneGameRouteMapWidget* RouteWidget,
			UGameXXKBattleBoardWidget* BattleWidget) override
		{
			Calls.Add(TEXT("Restore"));
			++RestoreCount;
			RestoredSnapshot = Snapshot;
			RestoredRouteWidget = RouteWidget;
			RestoredBattleWidget = BattleWidget;
			if (RouteWidget)
			{
				RouteWidget->SetVisibility(Snapshot.RouteVisibility);
				RouteWidget->RestoreScrollOffset(Snapshot.RouteScrollOffset);
			}
			if (Coordinator)
			{
				SessionTokenObservedDuringRestore = Coordinator->GetSessionToken();
				if (bReenterExitDuringRestore && RestoreCount == 1)
				{
					Coordinator->Exit(*this);
				}
				if (bReenterEnterDuringRestore && RestoreCount == 1 && RouteWidget && BattleWidget)
				{
					bRestoreReentrantEnterResult = Coordinator->Enter(*this, *RouteWidget, *BattleWidget);
				}
			}
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

	class FFakeBattleOverlayLifecycleOwner
	{
	public:
		explicit FFakeBattleOverlayLifecycleOwner(const FGameXXKBattleOverlaySnapshot& Snapshot)
			: Coordinator(NewObject<UGameXXKBattleOverlayCoordinator>())
			, RouteWidget(NewObject<UGameXXKOneGameRouteMapWidget>())
			, BattleWidget(NewObject<UGameXXKBattleBoardWidget>())
		{
			ConfigureRouteForSnapshot(*RouteWidget, Snapshot);
			Host.SnapshotToCapture = Snapshot;
			Host.Coordinator = Coordinator.Get();
		}

		bool Enter()
		{
			return Coordinator->Enter(Host, *RouteWidget, *BattleWidget);
		}

		void SimulateEndPlay()
		{
			ExitBeforeWidgetRelease(TEXT("ReleaseWidgets"));
		}

		void SimulatePreTravel()
		{
			ExitBeforeWidgetRelease(TEXT("BeginTravel"));
		}

		bool HasWidgetReferences() const
		{
			return RouteWidget.IsValid() || BattleWidget.IsValid();
		}

		UGameXXKOneGameRouteMapWidget* GetRouteWidget() const
		{
			return RouteWidget.Get();
		}

		UGameXXKBattleBoardWidget* GetBattleWidget() const
		{
			return BattleWidget.Get();
		}

		FRecordingBattleOverlayHost Host;

	private:
		void ExitBeforeWidgetRelease(const TCHAR* Marker)
		{
			Coordinator->Exit(Host);
			Host.Calls.Add(Marker);
			RouteWidget.Reset();
			BattleWidget.Reset();
		}

		TStrongObjectPtr<UGameXXKBattleOverlayCoordinator> Coordinator;
		TStrongObjectPtr<UGameXXKOneGameRouteMapWidget> RouteWidget;
		TStrongObjectPtr<UGameXXKBattleBoardWidget> BattleWidget;
	};

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

	UGameXXKBattleOverlayCoordinator* ExhaustedCoordinator = NewObject<UGameXXKBattleOverlayCoordinator>();
	UGameXXKOneGameRouteMapWidget* ExhaustedRouteWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	UGameXXKBattleBoardWidget* ExhaustedBattleWidget = NewObject<UGameXXKBattleBoardWidget>();
	ConfigureRouteForSnapshot(*ExhaustedRouteWidget, Before);
	FRecordingBattleOverlayHost ExhaustedHost;
	ExhaustedHost.SnapshotToCapture = Before;
	ExhaustedHost.Coordinator = ExhaustedCoordinator;
	ExhaustedCoordinator->SetLastIssuedSessionTokenForTest(MAX_uint64);
	TestFalse(TEXT("token exhaustion fails entry closed"), ExhaustedCoordinator->Enter(ExhaustedHost, *ExhaustedRouteWidget, *ExhaustedBattleWidget));
	TestEqual(TEXT("token exhaustion performs zero captures"), ExhaustedHost.CaptureCount, 0);
	TestEqual(TEXT("token exhaustion performs zero applies"), ExhaustedHost.ApplyCount, 0);
	TestFalse(TEXT("token exhaustion remains inactive"), ExhaustedCoordinator->IsActive());
	TestEqual(TEXT("token exhaustion exposes no session token"), ExhaustedCoordinator->GetSessionToken(), uint64(0));

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

	UGameXXKBattleOverlayCoordinator* PartialExpiryCoordinator = NewObject<UGameXXKBattleOverlayCoordinator>();
	UGameXXKOneGameRouteMapWidget* PartialExpiryRouteWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	UGameXXKBattleBoardWidget* PartialExpiryBattleWidget = NewObject<UGameXXKBattleBoardWidget>();
	ConfigureRouteForSnapshot(*PartialExpiryRouteWidget, Before);
	FRecordingBattleOverlayHost PartialExpiryHost;
	PartialExpiryHost.SnapshotToCapture = Before;
	PartialExpiryHost.Coordinator = PartialExpiryCoordinator;
	TestTrue(TEXT("partial-expiry scenario enters"), PartialExpiryCoordinator->Enter(PartialExpiryHost, *PartialExpiryRouteWidget, *PartialExpiryBattleWidget));
	PartialExpiryRouteWidget->MarkAsGarbage();
	TestFalse(TEXT("partial-expiry route weak reference expires"), TWeakObjectPtr<UGameXXKOneGameRouteMapWidget>(PartialExpiryRouteWidget).IsValid());
	PartialExpiryCoordinator->Exit(PartialExpiryHost);
	TestCallOrder(*this, TEXT("partial expiry still cancels then restores"), PartialExpiryHost.Calls, {TEXT("Capture"), TEXT("Apply"), TEXT("Cancel"), TEXT("Restore")});
	TestEqual(TEXT("partial expiry cancels once"), PartialExpiryHost.CancelCount, 1);
	TestEqual(TEXT("partial expiry restores once"), PartialExpiryHost.RestoreCount, 1);
	TestNull(TEXT("partial expiry passes a null route widget"), PartialExpiryHost.RestoredRouteWidget);
	TestEqual(TEXT("partial expiry preserves the surviving battle widget"), PartialExpiryHost.RestoredBattleWidget, PartialExpiryBattleWidget);
	TestSnapshotsEqual(*this, Before, PartialExpiryHost.RestoredSnapshot);

	UGameXXKBattleOverlayCoordinator* FullExpiryCoordinator = NewObject<UGameXXKBattleOverlayCoordinator>();
	UGameXXKOneGameRouteMapWidget* FullExpiryRouteWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	UGameXXKBattleBoardWidget* FullExpiryBattleWidget = NewObject<UGameXXKBattleBoardWidget>();
	ConfigureRouteForSnapshot(*FullExpiryRouteWidget, Before);
	FRecordingBattleOverlayHost FullExpiryHost;
	FullExpiryHost.SnapshotToCapture = Before;
	FullExpiryHost.Coordinator = FullExpiryCoordinator;
	TestTrue(TEXT("full-expiry scenario enters"), FullExpiryCoordinator->Enter(FullExpiryHost, *FullExpiryRouteWidget, *FullExpiryBattleWidget));
	FullExpiryRouteWidget->MarkAsGarbage();
	FullExpiryBattleWidget->MarkAsGarbage();
	TestFalse(TEXT("full-expiry route weak reference expires"), TWeakObjectPtr<UGameXXKOneGameRouteMapWidget>(FullExpiryRouteWidget).IsValid());
	TestFalse(TEXT("full-expiry battle weak reference expires"), TWeakObjectPtr<UGameXXKBattleBoardWidget>(FullExpiryBattleWidget).IsValid());
	FullExpiryCoordinator->Exit(FullExpiryHost);
	TestCallOrder(*this, TEXT("full expiry still cancels then restores"), FullExpiryHost.Calls, {TEXT("Capture"), TEXT("Apply"), TEXT("Cancel"), TEXT("Restore")});
	TestEqual(TEXT("full expiry cancels once"), FullExpiryHost.CancelCount, 1);
	TestEqual(TEXT("full expiry restores once"), FullExpiryHost.RestoreCount, 1);
	TestNull(TEXT("full expiry passes a null route widget"), FullExpiryHost.RestoredRouteWidget);
	TestNull(TEXT("full expiry passes a null battle widget"), FullExpiryHost.RestoredBattleWidget);
	TestSnapshotsEqual(*this, Before, FullExpiryHost.RestoredSnapshot);

	UGameXXKBattleOverlayCoordinator* CaptureExitCoordinator = NewObject<UGameXXKBattleOverlayCoordinator>();
	UGameXXKOneGameRouteMapWidget* CaptureExitRouteWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	UGameXXKBattleBoardWidget* CaptureExitBattleWidget = NewObject<UGameXXKBattleBoardWidget>();
	ConfigureRouteForSnapshot(*CaptureExitRouteWidget, Before);
	FRecordingBattleOverlayHost CaptureExitHost;
	CaptureExitHost.SnapshotToCapture = Before;
	CaptureExitHost.bExitDuringCapture = true;
	CaptureExitHost.Coordinator = CaptureExitCoordinator;
	TestFalse(TEXT("capture-time exit aborts outer entry"), CaptureExitCoordinator->Enter(CaptureExitHost, *CaptureExitRouteWidget, *CaptureExitBattleWidget));
	TestCallOrder(*this, TEXT("capture-time exit aborts before Apply"), CaptureExitHost.Calls, {TEXT("Capture")});
	TestEqual(TEXT("capture-time exit never applies overlay mutation"), CaptureExitHost.ApplyCount, 0);
	TestEqual(TEXT("capture-time exit does not cancel an unstarted overlay"), CaptureExitHost.CancelCount, 0);
	TestEqual(TEXT("capture-time exit does not restore an unmutated overlay"), CaptureExitHost.RestoreCount, 0);
	TestFalse(TEXT("capture-time exit leaves the coordinator inactive"), CaptureExitCoordinator->IsActive());
	TestEqual(TEXT("capture-time exit leaves no public session token"), CaptureExitCoordinator->GetSessionToken(), uint64(0));
	CaptureExitHost.bExitDuringCapture = false;
	CaptureExitHost.Calls.Reset();
	TestTrue(TEXT("capture-time exit leaves a clean coordinator for retry"), CaptureExitCoordinator->Enter(CaptureExitHost, *CaptureExitRouteWidget, *CaptureExitBattleWidget));
	TestCallOrder(*this, TEXT("retry after capture-time exit performs a fresh transaction"), CaptureExitHost.Calls, {TEXT("Capture"), TEXT("Apply")});
	CaptureExitCoordinator->Exit(CaptureExitHost);

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

	UGameXXKBattleOverlayCoordinator* RestoreReentryCoordinator = NewObject<UGameXXKBattleOverlayCoordinator>();
	UGameXXKOneGameRouteMapWidget* RestoreReentryRouteWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	UGameXXKBattleBoardWidget* RestoreReentryBattleWidget = NewObject<UGameXXKBattleBoardWidget>();
	ConfigureRouteForSnapshot(*RestoreReentryRouteWidget, Before);
	FRecordingBattleOverlayHost RestoreReentryHost;
	RestoreReentryHost.SnapshotToCapture = Before;
	RestoreReentryHost.bReenterExitDuringRestore = true;
	RestoreReentryHost.bReenterEnterDuringRestore = true;
	RestoreReentryHost.Coordinator = RestoreReentryCoordinator;
	TestTrue(TEXT("restore-reentry scenario enters"), RestoreReentryCoordinator->Enter(RestoreReentryHost, *RestoreReentryRouteWidget, *RestoreReentryBattleWidget));
	const uint64 RestoreReentrySession = RestoreReentryCoordinator->GetSessionToken();
	RestoreReentryCoordinator->Exit(RestoreReentryHost);
	TestCallOrder(*this, TEXT("restore reentry preserves cancel-then-restore ordering"), RestoreReentryHost.Calls, {TEXT("Capture"), TEXT("Apply"), TEXT("Cancel"), TEXT("Restore")});
	TestEqual(TEXT("restore reentry cancels only once"), RestoreReentryHost.CancelCount, 1);
	TestEqual(TEXT("restore reentry restores only once"), RestoreReentryHost.RestoreCount, 1);
	TestFalse(TEXT("restore reentry cannot start a successor session"), RestoreReentryHost.bRestoreReentrantEnterResult);
	TestEqual(TEXT("restore observes an invalidated token"), RestoreReentryHost.SessionTokenObservedDuringRestore, uint64(0));
	TestFalse(TEXT("restore reentry ends inactive"), RestoreReentryCoordinator->IsActive());
	TestEqual(TEXT("restore reentry ends without a public token"), RestoreReentryCoordinator->GetSessionToken(), uint64(0));
	TestFalse(TEXT("restore reentry leaves its closing token stale"), RestoreReentryCoordinator->IsCurrentSession(RestoreReentrySession));

	UGameXXKBattleOverlayCoordinator* ApplyExitCoordinator = NewObject<UGameXXKBattleOverlayCoordinator>();
	UGameXXKOneGameRouteMapWidget* ApplyExitRouteWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	UGameXXKBattleBoardWidget* ApplyExitBattleWidget = NewObject<UGameXXKBattleBoardWidget>();
	ConfigureRouteForSnapshot(*ApplyExitRouteWidget, Before);
	FRecordingBattleOverlayHost ApplyExitHost;
	ApplyExitHost.SnapshotToCapture = Before;
	ApplyExitHost.bExitDuringApply = true;
	ApplyExitHost.bMutateRouteAfterExitDuringApply = true;
	ApplyExitHost.bReenterEnterAfterExitDuringApply = true;
	ApplyExitHost.Coordinator = ApplyExitCoordinator;
	TestFalse(TEXT("entry reports failure when Apply synchronously exits"), ApplyExitCoordinator->Enter(ApplyExitHost, *ApplyExitRouteWidget, *ApplyExitBattleWidget));
	TestCallOrder(*this, TEXT("Apply-triggered exit defers cancel and restore until Apply unwinds"), ApplyExitHost.Calls, {TEXT("Capture"), TEXT("Apply"), TEXT("Cancel"), TEXT("Restore")});
	TestEqual(TEXT("Apply-triggered exit cancels once"), ApplyExitHost.CancelCount, 1);
	TestEqual(TEXT("Apply-triggered exit restores once"), ApplyExitHost.RestoreCount, 1);
	TestEqual(TEXT("Apply-triggered exit invalidates the token before Apply continues"), ApplyExitHost.SessionTokenObservedAfterApplyExit, uint64(0));
	TestFalse(TEXT("Apply-triggered exit rejects a nested successor entry"), ApplyExitHost.bApplyReentrantEnterResult);
	TestEqual(TEXT("Apply-triggered exit restores exact visibility after later Apply mutation"), ApplyExitRouteWidget->GetVisibility(), Before.RouteVisibility);
	TestEqual(TEXT("Apply-triggered exit restores exact scroll after later Apply mutation"), ApplyExitRouteWidget->GetCurrentScrollOffset(), Before.RouteScrollOffset);
	TestSnapshotsEqual(*this, Before, ApplyExitHost.RestoredSnapshot);
	TestFalse(TEXT("Apply-triggered exit leaves the coordinator inactive"), ApplyExitCoordinator->IsActive());
	TestEqual(TEXT("Apply-triggered exit creates no successor session"), ApplyExitCoordinator->GetSessionToken(), uint64(0));

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

	FFakeBattleOverlayLifecycleOwner TeardownOwner(Before);
	UGameXXKOneGameRouteMapWidget* TeardownRouteWidget = TeardownOwner.GetRouteWidget();
	UGameXXKBattleBoardWidget* TeardownBattleWidget = TeardownOwner.GetBattleWidget();
	TestTrue(TEXT("teardown owner enters"), TeardownOwner.Enter());
	TeardownOwner.SimulateEndPlay();
	TestCallOrder(*this, TEXT("EndPlay restores before releasing widgets"), TeardownOwner.Host.Calls, {TEXT("Capture"), TEXT("Apply"), TEXT("Cancel"), TEXT("Restore"), TEXT("ReleaseWidgets")});
	TestEqual(TEXT("EndPlay restores once"), TeardownOwner.Host.RestoreCount, 1);
	TestEqual(TEXT("EndPlay restores the route before release"), TeardownOwner.Host.RestoredRouteWidget, TeardownRouteWidget);
	TestEqual(TEXT("EndPlay restores the battle widget before release"), TeardownOwner.Host.RestoredBattleWidget, TeardownBattleWidget);
	TestFalse(TEXT("EndPlay clears the owner's widget references after restore"), TeardownOwner.HasWidgetReferences());

	FFakeBattleOverlayLifecycleOwner TravelOwner(Before);
	UGameXXKOneGameRouteMapWidget* TravelRouteWidget = TravelOwner.GetRouteWidget();
	UGameXXKBattleBoardWidget* TravelBattleWidget = TravelOwner.GetBattleWidget();
	TestTrue(TEXT("pre-travel owner enters"), TravelOwner.Enter());
	TravelOwner.SimulatePreTravel();
	TestCallOrder(*this, TEXT("pre-travel cleanup restores before travel begins"), TravelOwner.Host.Calls, {TEXT("Capture"), TEXT("Apply"), TEXT("Cancel"), TEXT("Restore"), TEXT("BeginTravel")});
	TestEqual(TEXT("pre-travel cleanup restores once"), TravelOwner.Host.RestoreCount, 1);
	TestEqual(TEXT("pre-travel cleanup restores the route before travel"), TravelOwner.Host.RestoredRouteWidget, TravelRouteWidget);
	TestEqual(TEXT("pre-travel cleanup restores the battle widget before travel"), TravelOwner.Host.RestoredBattleWidget, TravelBattleWidget);
	TestFalse(TEXT("pre-travel cleanup clears widget references after restore"), TravelOwner.HasWidgetReferences());

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestTrue(TEXT("scroll test starts a game"), Subsystem->StartGame());
	TestTrue(TEXT("scroll test selects Qingshan"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("scroll test accepts the quest"), Subsystem->AcceptQuest());
	TestTrue(TEXT("scroll test opens the dungeon"), Subsystem->OpenDungeonFromTownExit());
	UGameXXKOneGameRouteMapWidget* ScrollRouteWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	ScrollRouteWidget->SetMVPSubsystem(Subsystem);
	ScrollRouteWidget->SetRouteMapViewportGeometry(FVector2D::ZeroVector, FVector2D(1280.0f, 100.0f));
	ScrollRouteWidget->RestoreScrollOffset(Before.RouteScrollOffset);
	TestNull(TEXT("scroll box is absent before Slate construction"), ScrollRouteWidget->GetRouteScrollBoxForTest());
	TestEqual(TEXT("scroll accessor falls back to the cached offset before construction"), ScrollRouteWidget->GetCurrentScrollOffset(), Before.RouteScrollOffset);
	TestTrue(TEXT("scroll test widget initializes"), ScrollRouteWidget->Initialize());
	TSharedRef<SWidget> RouteSlateWidget = ScrollRouteWidget->TakeWidget();
	ScrollRouteWidget->NativeConstruct();
	TSharedRef<SWindow> RouteLayoutWindow = SNew(SWindow)
		.ClientSize(FVector2D(1280.0f, 360.0f))
		.ScreenPosition(FVector2D(-10000.0f, -10000.0f))
		.AutoCenter(EAutoCenter::None)
		.CreateTitleBar(false)
		.SizingRule(ESizingRule::FixedSize)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.HasCloseButton(false)
		[
			RouteSlateWidget
		];
	FSlateApplication::Get().AddWindow(RouteLayoutWindow, true);
	RouteLayoutWindow->SlatePrepass(1.0f);
	IConsoleVariable* SkipHeadlessSlateDraw = IConsoleManager::Get().FindConsoleVariable(TEXT("Slate.SkipWidgetDrawingInHeadlessMode"));
	const int32 PreviousSkipHeadlessSlateDraw = SkipHeadlessSlateDraw ? SkipHeadlessSlateDraw->GetInt() : 1;
	if (SkipHeadlessSlateDraw)
	{
		SkipHeadlessSlateDraw->Set(0, ECVF_SetByCode);
	}
	FSlateApplication::Get().ForceRedrawWindow(RouteLayoutWindow);
	if (SkipHeadlessSlateDraw)
	{
		SkipHeadlessSlateDraw->Set(PreviousSkipHeadlessSlateDraw, ECVF_SetByCode);
	}
	UScrollBox* LiveRouteScrollBox = ScrollRouteWidget->GetRouteScrollBoxForTest();
	TestNotNull(TEXT("scroll test exposes the constructed live scroll box"), LiveRouteScrollBox);
	TestTrue(TEXT("scroll test arranges nonzero live geometry"), LiveRouteScrollBox && LiveRouteScrollBox->GetCachedGeometry().GetLocalSize().Y > 0.0f);
	TestTrue(TEXT("scroll test has enough clamped range"), ScrollRouteWidget->GetMaxScrollOffsetForTest() >= Before.RouteScrollOffset);
	ScrollRouteWidget->RestoreScrollOffset(Before.RouteScrollOffset);
	TestEqual(TEXT("scroll restore updates the live scroll box"), LiveRouteScrollBox ? LiveRouteScrollBox->GetScrollOffset() : -1.0f, Before.RouteScrollOffset);
	TestEqual(TEXT("scroll accessor returns the live restored offset"), ScrollRouteWidget->GetCurrentScrollOffset(), Before.RouteScrollOffset);
	ScrollRouteWidget->RefreshFromState();
	TestEqual(TEXT("refresh preserves the live scroll after initial initialization"), LiveRouteScrollBox ? LiveRouteScrollBox->GetScrollOffset() : -1.0f, Before.RouteScrollOffset);
	ScrollRouteWidget->RestoreScrollOffset(100000.0f);
	const float ArithmeticFallbackEnd = FMath::Max(0.0f, ScrollRouteWidget->GetRouteContentSizeForTest().Y - 100.0f);
	const float ArrangedLiveEnd = LiveRouteScrollBox ? LiveRouteScrollBox->GetScrollOffsetOfEnd() : -1.0f;
	TestTrue(TEXT("arranged live end differs from configured arithmetic fallback"), !FMath::IsNearlyEqual(ArrangedLiveEnd, ArithmeticFallbackEnd));
	TestEqual(TEXT("upper clamping reaches the authoritative arranged live end"), LiveRouteScrollBox ? LiveRouteScrollBox->GetScrollOffset() : -1.0f, ArrangedLiveEnd);
	TestEqual(TEXT("upper clamping synchronizes the cached offset to live end"), ScrollRouteWidget->GetLastAppliedScrollOffsetForTest(), ArrangedLiveEnd);
	ScrollRouteWidget->RestoreScrollOffset(-100.0f);
	TestEqual(TEXT("lower clamping reaches the live scroll box"), LiveRouteScrollBox ? LiveRouteScrollBox->GetScrollOffset() : -1.0f, 0.0f);
	const float LiveOnlyScrollOffset = 257.0f;
	if (LiveRouteScrollBox)
	{
		LiveRouteScrollBox->SetScrollOffset(LiveOnlyScrollOffset);
	}
	TestEqual(TEXT("scroll accessor reads the live box instead of the stale cache"), ScrollRouteWidget->GetCurrentScrollOffset(), LiveOnlyScrollOffset);
	TestEqual(TEXT("setting the live box alone does not rewrite the fallback cache"), ScrollRouteWidget->GetLastAppliedScrollOffsetForTest(), 0.0f);
	const float UserScrollOffset = 321.0f;
	if (LiveRouteScrollBox)
	{
		LiveRouteScrollBox->SetScrollOffset(UserScrollOffset);
		LiveRouteScrollBox->OnUserScrolled.Broadcast(UserScrollOffset);
	}
	TestEqual(TEXT("scroll accessor observes user scrolling on the live box"), ScrollRouteWidget->GetCurrentScrollOffset(), UserScrollOffset);
	TestEqual(TEXT("user-scroll delegate updates the cached fallback"), ScrollRouteWidget->GetLastAppliedScrollOffsetForTest(), UserScrollOffset);
	FSlateApplication::Get().DestroyWindowImmediately(RouteLayoutWindow);

	UGameXXKOneGameRouteMapWidget* IdentityRouteWidget = NewObject<UGameXXKOneGameRouteMapWidget>();
	IdentityRouteWidget->SetMVPSubsystem(Subsystem);
	IdentityRouteWidget->SetRouteMapViewportGeometry(FVector2D::ZeroVector, FVector2D(1280.0f, 100.0f));
	IdentityRouteWidget->RefreshFromState();
	const float InitialRouteEnd = IdentityRouteWidget->GetMaxScrollOffsetForTest();
	TestEqual(TEXT("first generated route initializes at its end"), IdentityRouteWidget->GetCurrentScrollOffset(), InitialRouteEnd);
	const float SameRouteUserOffset = 123.0f;
	IdentityRouteWidget->RestoreScrollOffset(SameRouteUserOffset);
	IdentityRouteWidget->RefreshFromState();
	TestEqual(TEXT("ordinary refresh preserves same-route user scroll"), IdentityRouteWidget->GetCurrentScrollOffset(), SameRouteUserOffset);

	FGameXXKRuntimeState& MutableRouteState = Subsystem->GetMutableRuntimeState();
	const int32 ReplacementSeed = MutableRouteState.RouteSeed + 7919;
	UGameXXKMVPRules::GenerateRouteMapForSeed(MutableRouteState, ReplacementSeed);
	IdentityRouteWidget->RefreshFromState();
	TestEqual(TEXT("changed route identity discards the old top offset"), IdentityRouteWidget->GetCurrentScrollOffset(), IdentityRouteWidget->GetMaxScrollOffsetForTest());

	IdentityRouteWidget->RestoreScrollOffset(41.0f);
	if (!MutableRouteState.RouteMapNodes.IsEmpty())
	{
		MutableRouteState.RouteMapNodes[0].ColumnIndex += 1;
	}
	IdentityRouteWidget->RefreshFromState();
	TestEqual(TEXT("same-seed topology replacement reapplies initial scroll"), IdentityRouteWidget->GetCurrentScrollOffset(), IdentityRouteWidget->GetMaxScrollOffsetForTest());

	MutableRouteState = UGameXXKMVPRules::CreateNewGame();
	IdentityRouteWidget->RefreshFromState();
	IdentityRouteWidget->RestoreScrollOffset(17.0f);
	MutableRouteState.Screen = EGameXXKScreen::DungeonMap;
	MutableRouteState.bDungeonActive = true;
	UGameXXKMVPRules::GenerateRouteMapForSeed(MutableRouteState, ReplacementSeed + 104729);
	IdentityRouteWidget->RefreshFromState();
	TestEqual(TEXT("new run after no-generated state reapplies initial scroll"), IdentityRouteWidget->GetCurrentScrollOffset(), IdentityRouteWidget->GetMaxScrollOffsetForTest());

	return true;
}

#endif
