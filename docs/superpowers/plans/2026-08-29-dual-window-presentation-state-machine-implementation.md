# Dual-Window Presentation State Machine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Minimize the primary GameViewport while the desktop idle overlay owns presentation, then restore it as borderless fullscreen for route gameplay, battle, and 3D town presentation.

**Architecture:** Add a pure presentation/action planner to `AGameXXKMVPPlayerController` and execute those same planned actions against the real GameViewport `SWindow`. Existing overlay attachment remains the safety gate: the primary window is minimized only after composition succeeds, while all non-desktop surfaces restore, switch to `WindowedFullscreen`, and take focus.

**Tech Stack:** UE 5.8 C++, Slate `SWindow`, Win32 window state, DirectComposition project plugin, UE Automation, UBT, UE MCP, PowerShell/Python acceptance probes.

---

## File map

- Modify `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h` — presentation and primary-window action enums, pure planner seams, native lifecycle state.
- Modify `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp` — action planning, GameViewport window acquisition and native execution.
- Modify `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp` — policy matrix and exact action-order regressions.
- Optionally modify `scripts/launch_desktop_mvp_demo.ps1` only if real acceptance proves the runtime policy is overridden by launch arguments; do not change it pre-emptively.
- Evidence under `Saved/HarnessReports/` and `Saved/Codex/` — ignored diagnostic artifacts only.

### Task 1: Define the pure presentation policy

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Write failing policy-matrix tests**

Add Automation assertions for:

```cpp
TestEqual(TEXT("desktop Town with an attached visible overlay minimizes the primary window"),
    AGameXXKMVPPlayerController::ResolveWindowPresentationForTest(true, EGameXXKScreen::Town, true, true),
    EGameXXKWindowPresentation::DesktopIdleOverlay);
TestEqual(TEXT("desktop route merchant restores fullscreen gameplay"),
    AGameXXKMVPPlayerController::ResolveWindowPresentationForTest(true, EGameXXKScreen::RouteMerchant, false, true),
    EGameXXKWindowPresentation::FullscreenGameplay);
TestEqual(TEXT("3D Town restores fullscreen gameplay"),
    AGameXXKMVPPlayerController::ResolveWindowPresentationForTest(false, EGameXXKScreen::Town, true, false),
    EGameXXKWindowPresentation::FullscreenGameplay);
TestEqual(TEXT("failed desktop overlay keeps the viewport fallback visible"),
    AGameXXKMVPPlayerController::ResolveWindowPresentationForTest(true, EGameXXKScreen::Town, true, false),
    EGameXXKWindowPresentation::ViewportFallback);
```

Cover every route-owned `Screen` value in one table-driven loop.

- [ ] **Step 2: Cold-compile RED**

Save dirty packages through UE MCP, close the editor safely, then run the Editor target UBT. Expected: compile failure because the presentation enum and resolver do not exist.

- [ ] **Step 3: Implement the minimal pure resolver**

Define `EGameXXKWindowPresentation` and implement the exact table from the approved design. The resolver must not inspect globals or native window state.

- [ ] **Step 4: Cold-compile and run the focused Automation GREEN**

Run the new test plus `GameXXK.DesktopTraining.Workbench.TransparentSlateWindowContract`. Expected: zero failures/errors.

### Task 2: Plan and execute native primary-window actions

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Write failing action-order tests**

Define the wished-for planner and assert:

```cpp
TestEqual(TEXT("desktop policy minimizes rather than hides"),
    BuildPrimaryWindowActionsForTest(DesktopIdleOverlay, true, false, false),
    {EGameXXKPrimaryWindowAction::Minimize});
TestEqual(TEXT("hidden minimized gameplay restores before fullscreen focus"),
    BuildPrimaryWindowActionsForTest(FullscreenGameplay, false, true, false),
    {Show, Restore, SetWindowedFullscreen, BringToFront});
TestEqual(TEXT("already-correct fullscreen is idempotent"),
    BuildPrimaryWindowActionsForTest(FullscreenGameplay, true, false, true),
    {BringToFront});
TestEqual(TEXT("fallback only restores visibility"),
    BuildPrimaryWindowActionsForTest(ViewportFallback, false, true, false),
    {Show, Restore});
```

No action list may contain a `Hide` action for the primary viewport.

- [ ] **Step 2: Run RED**

Expected: compile failure because the action enum/planner is missing.

- [ ] **Step 3: Implement the planner and real executor**

Extract the existing GameViewport `SWindow` lookup into one helper. Execute planner actions using `ShowWindow`, `Restore`, `SetWindowMode(EWindowMode::WindowedFullscreen)`, `BringToFront(true)`, and `Minimize`. Reject the Overlay window itself. Keep ordinary editor PIE outside native management.

- [ ] **Step 4: Run focused GREEN**

Run the policy/action Automation tests and existing transparent-window lifecycle tests.

### Task 3: Route existing transitions through the policy

**Files:**
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Test: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Write failing lifecycle contract tests**

Assert that overlay show selects `DesktopIdleOverlay` only after composition is attached; overlay hide selects `FullscreenGameplay`; an unattached overlay selects `ViewportFallback`; and the initial reassert budget is bounded rather than permanent.

- [ ] **Step 2: Run RED**

Expected: assertions fail under the old Hide/Show boolean lifecycle.

- [ ] **Step 3: Replace old visibility semantics**

`ShowDesktopTrainingOverlayWindow()` shows the attached Overlay and applies desktop minimization. `HideDesktopTrainingOverlayWindow()` hides Overlay and restores fullscreen gameplay. `PlayerTick` consumes only the bounded reassert budget. Preserve map-travel capture/restore and composition release ordering.

- [ ] **Step 4: Run Workbench regression GREEN**

Run `GameXXK.DesktopTraining.Workbench` and `GameXXK.MVP.LevelFlow`. Expected: zero failed/error.

### Task 4: Real standalone window acceptance

**Files:**
- Evidence only: `Saved/HarnessReports/dual-window-presentation-20260829.json`
- Evidence only: `Saved/Codex/dual-window-*.png`

- [ ] **Step 1: Cold-build Editor and Game targets**

Run UBT with `-NoHotReload -NoHotReloadFromIDE`; do not use Live Coding.

- [ ] **Step 2: Launch the canonical `-game` desktop surface**

Start `/Game/GameXXK/Maps/L_DesktopTrainingHUD` using the existing launcher. Wait for MCP and Overlay attachment.

- [ ] **Step 3: Verify desktop idle and travel**

Enumerate Win32 windows. Require Overlay visible and the primary window visible-to-Windows but iconic/minimized. Start travel and confirm the runtime encounter clock advances for 60 seconds.

- [ ] **Step 4: Verify every fullscreen gameplay surface**

Enter Challenge route map, route event/camp/merchant where reachable, Battle, and 3D Town. At each checkpoint require Overlay hidden, primary non-iconic, monitor-sized borderless `WindowedFullscreen`, and correct input focus.

- [ ] **Step 5: Verify both return paths**

Return from Battle/route settlement and from 3D Town. Require Overlay attachment/show before the primary becomes iconic again; workbench snapshot and travel continue.

- [ ] **Step 6: Review the captured visual evidence**

Review desktop-idle, route, battle, and town screenshots with a suitable method. Record only observed window/presentation facts.

- [ ] **Step 7: Restore the canonical review surface**

Leave `/Game/GameXXK/Maps/L_DesktopTrainingHUD` in its normal desktop idle state for user review.
