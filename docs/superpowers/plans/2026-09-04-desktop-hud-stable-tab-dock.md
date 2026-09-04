# Desktop HUD Stable Tab Dock Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep the desktop idle strip and Backpack Tab at fixed screen coordinates while the backpack expands above or below them, and align the upward Win32 Region with the rendered controls.

**Architecture:** `GameXXKDesktopTrainingLayout` owns the persistent dock geometry and upward center-content offset. The workbench consumes those values for UMG slots, while native Region generation consumes the same values for clipping and hit testing. Placement clamps one strip anchor against the complete dock group and reuses it across collapsed and expanded states.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, Win32 Region APIs, Unreal Automation Tests, project UBT and MCP test scripts.

---

### Task 1: Prove the current placement and Region faults

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [x] Add placement assertions for top/down and bottom/up strip invariance at 50%, 75%, and 100%.
- [x] Update the expanded control-rail expectations to the approved invariant geometry so the existing implementation fails.
- [x] Add upward native-Region assertions for the visually shifted content and toolbar coordinates.
- [x] Build and run the focused automation tests; record failures caused by strip drift, Tab drift, and stale Region coordinates.

### Task 2: Centralize the fixed dock geometry

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp`

- [x] Define the persistent `1038 × 202` expanded strip rectangle, fixed notice/control-rail origin, and shared 210-unit upward-content offset.
- [x] Clamp the physical strip anchor once against the complete strip-plus-notice group.
- [x] Use a 941-unit downward canvas and a `941 + notice height` upward canvas.
- [x] Apply the shared upward offset to native center-content, navigation, and toolbar Region shapes.

### Task 3: Make UMG consume the canonical dock geometry

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`

- [x] Keep the idle strip at `1038 × 202` and keep the expanded progress rail at 420 units.
- [x] Build the Tab at `(953, 202)` relative to the strip in collapsed, downward, and upward states.
- [x] Size the upward design canvas to include the fixed rail below the strip.
- [x] Replace the hard-coded upward transform eligibility and amount with layout-module helpers.

### Task 4: Verify and document

**Files:**
- Modify: `docs/production/current-goal-acceptance.md` only if its rolling pointer needs the new HUD evidence.

- [x] Rebuild `GameXXKEditor` with the editor closed; do not use Live Coding or Hot Reload.
- [x] Run the focused stable-dock and native-Region automation tests.
- [x] Run the full DesktopTraining Workbench automation group and compare only against known baseline failures.
- [x] Launch the canonical `/Game/GameXXK/Maps/L_DesktopTrainingHUD` surface and visually exercise top/down and bottom/up Tab toggles at the persisted player scale.
- [x] Commit only the scoped source, tests, and documentation without staging unrelated user assets.
