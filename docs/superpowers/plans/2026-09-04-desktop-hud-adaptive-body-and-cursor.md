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

- [x] Add failing left/right body-union fit tests at 50%, 75%, and 100%.
- [x] Add a failing work-area native-host test.
- [x] Add failing cursor-rect tests for 96/120 DPI and warehouse/upward offsets.
- [x] Add failing Region tests for translated warehouse, backpack, training, and attached buttons.
- [x] Add failing body-height and 36-slot warehouse-scroll structure tests.
- [x] Cold-build and record the expected failures before production changes.

### Task 2: Replace the compact fixed host with a work-area host

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`

- [x] Resolve the fixed desktop host as `(0,0,work-area width,work-area height)`.
- [x] Position the HUD content inside that host using its physical work-area coordinates and DPI bridge.
- [x] Keep drag/carry full-region behavior and normal shaped Region behavior.
- [x] Freeze the current layout during drag, then recalculate expansion direction and body fit once after release.

### Task 3: Add the canonical body-fit transform

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`

- [x] Compute active body bounds from warehouse, backpack, right panel, navigation, toolbar, and attached controls.
- [x] Resolve one logical body translation from physical work-area overflow.
- [x] Apply the same translation to UMG widgets and native Region shapes.
- [x] Move story/town controls into the body coordinate domain.
- [x] Share chest-control geometry between rendering and drag exclusion.

### Task 4: Reflow auxiliary panels to backpack height

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [x] Set warehouse and right panel shells to one continuous 681-unit paper matching backpack plus navigation.
- [x] Keep warehouse page size 36 and place its 4×9 grid inside a vertical scroll viewport.
- [x] Reflow training with title/difficulty/chapter, three vertical nodes, current selection, and current travel above the divider; keep actions below it.
- [x] Reflow tools into the same shell with confirmation on the lower shelf.

### Task 5: Add an independent cursor layer

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`

- [x] Add a cursor canvas above the scaled HUD content.
- [x] Convert native client pixels directly to cursor-canvas Slate position and size.
- [x] Keep cursor visuals out of upward and horizontal body transforms.
- [x] Verify picked slot identity and cursor-center alignment.

### Task 6: Verify, capture, and checkpoint

**Files:**
- Create: `docs/production/2026-09-04-desktop-hud-adaptive-body-and-cursor-acceptance.md`
- Modify: `docs/production/current-goal-acceptance.md`

- [x] Cold-build without Live Coding or Hot Reload.
- [x] Run the focused dock/body/cursor/Region tests.
- [x] Run the complete DesktopTraining Workbench group and compare known baselines.
- [x] Exercise left/right/corner drag, both directions, warehouse and training together, and item pickup in canonical PIE.
- [x] Capture before/after evidence and restore the user's saved HUD position.
- [x] Commit and push only scoped code, tests, and documentation.
