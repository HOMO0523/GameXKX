# Desktop HUD Adaptive Body and Cursor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the desktop workbench body fit the monitor independently of its dock and make carried items follow the physical cursor under every layout transform.

**Architecture:** The native desktop window becomes a fixed work-area host. Existing programmatic widgets remain in the authored canvas, but a single canonical body translation is applied to every expanded body surface and mirrored into the native Region. Carried-item visuals move to an independent cursor canvas using physical-client-to-Slate conversion only.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, Win32 Region APIs, Unreal Automation Tests, project UBT and UE MCP pipeline.

---

### Task 1: Reproduce the four coordinate failures

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopHudStableDockTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] Add failing left/right body-union fit tests at 50%, 75%, and 100%.
- [ ] Add a failing work-area native-host test.
- [ ] Add failing cursor-rect tests for 96/120 DPI and warehouse/upward offsets.
- [ ] Add failing Region tests for translated warehouse, backpack, training, and attached buttons.
- [ ] Add failing body-height and 36-slot warehouse-scroll structure tests.
- [ ] Cold-build and record the expected failures before production changes.

### Task 2: Replace the compact fixed host with a work-area host

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`

- [ ] Resolve the fixed desktop host as `(0,0,work-area width,work-area height)`.
- [ ] Position the HUD content inside that host using its physical work-area coordinates and DPI bridge.
- [ ] Keep drag/carry full-region behavior and normal shaped Region behavior.
- [ ] Recalculate expansion direction after an expanded drag is released.

### Task 3: Add the canonical body-fit transform

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`

- [ ] Compute active body bounds from warehouse, backpack, right panel, navigation, toolbar, and attached controls.
- [ ] Resolve one logical body translation from physical work-area overflow.
- [ ] Apply the same translation to UMG widgets and native Region shapes.
- [ ] Move story/town controls into the body coordinate domain.
- [ ] Share chest-control geometry between rendering and drag exclusion.

### Task 4: Reflow auxiliary panels to backpack height

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] Set warehouse and right panel shells to the 533-unit backpack body height.
- [ ] Keep warehouse page size 36 and place its 4×9 grid inside a vertical scroll viewport.
- [ ] Reflow training to one horizontal three-node row with pinned stage and action controls.
- [ ] Reflow tools into the same body-height shell.

### Task 5: Add an independent cursor layer

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`

- [ ] Add a cursor canvas above the scaled HUD content.
- [ ] Convert native client pixels directly to cursor-canvas Slate position and size.
- [ ] Keep cursor visuals out of upward and horizontal body transforms.
- [ ] Verify picked slot identity and cursor-center alignment.

### Task 6: Verify, capture, and checkpoint

**Files:**
- Create: `docs/production/2026-09-04-desktop-hud-adaptive-body-and-cursor-acceptance.md`
- Modify: `docs/production/current-goal-acceptance.md`

- [ ] Cold-build without Live Coding or Hot Reload.
- [ ] Run the focused dock/body/cursor/Region tests.
- [ ] Run the complete DesktopTraining Workbench group and compare known baselines.
- [ ] Exercise left/right/corner drag, both directions, warehouse and training together, and item pickup in canonical PIE.
- [ ] Capture before/after evidence and restore the user's saved HUD position.
- [ ] Commit and push only scoped code, tests, and documentation.

