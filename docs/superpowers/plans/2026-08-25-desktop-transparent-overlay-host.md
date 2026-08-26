# Desktop Transparent Overlay Host Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the opaque, resized game-window hack with a full-work-area per-pixel-transparent overlay whose inner desktop HUD can be dragged, folded, expanded, and clicked like TaskbarHero.

**Architecture:** A borderless `SWindow` remains the size of the selected monitor work area and uses `EWindowTransparency::PerPixel`; its background/border brushes are empty. The workbench UMG tree owns a full-host transparent canvas and positions one scaled HUD group inside it from a persisted normalized anchor. A narrow Win32 hit-test hook only returns client input for visible HUD regions and returns `HTTRANSPARENT` elsewhere; dragging changes the inner HUD anchor rather than moving or resizing the native window.

**Tech Stack:** Unreal Engine 5.8 C++, Slate `SWindow`, UMG, Windows native hit testing, UE Automation Tests, UBT.

---

### Task 1: Lock the overlay contract with failing tests

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h`

- [ ] **Step 1: Write a failing layout contract test**

Add assertions that the native host uses the whole monitor work area, while the collapsed visible HUD remains `1038x202` at 100% and `519x101` at 50%. Assert that a persisted normalized anchor resolves to a clamped inner-HUD position without changing the host rectangle.

- [ ] **Step 2: Run the targeted test and verify RED**

Run the editor automation filter `GameXXK.DesktopTraining.Workbench.TransparentOverlayHost` through the project test pipeline. Expected: compile failure or assertion failure because the host/anchor layout API does not exist yet.

### Task 2: Add pure overlay placement rules

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Implement minimal placement helpers**

Add pure helpers that compute the effective scale, visible HUD size, collapsed-strip anchor, group top-left for upward/downward expansion, and normalized anchor clamping against a host work area.

- [ ] **Step 2: Run the targeted test and verify GREEN**

Expected: all placement assertions pass without constructing a native window.

### Task 3: Make the workbench a transparent full-host canvas

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Write a failing widget-tree test**

Assert that the workbench root is a full-host transparent canvas containing a named HUD group slot, and that Tab changes only the group content state rather than the native host size.

- [ ] **Step 2: Verify RED**

Expected: the named host canvas/group slot is absent in the current tree.

- [ ] **Step 3: Implement the full-host root and inner group**

Wrap the existing scale/reference canvas in a transparent root canvas; update the inner canvas slot from host geometry, scale preference, expansion direction, and normalized anchor. Add mouse capture/move/release handling so dragging a non-control part of the idle strip moves the inner group and persists its anchor.

- [ ] **Step 4: Verify GREEN**

Expected: the widget-tree contract and existing Workbench tests pass.

### Task 4: Create the per-pixel transparent Slate host

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`

- [ ] **Step 1: Write a failing native-window construction test seam**

Expose a test-only construction seam and assert `EWindowTransparency::PerPixel`, `FWindowStyle::GetBorderless()`, fixed work-area sizing, no title bar, and input acceptance.

- [ ] **Step 2: Verify RED**

Expected: the overlay window does not exist and the old viewport/color-key path remains active.

- [ ] **Step 3: Implement the transparent host**

Create a dedicated full-work-area `SWindow`, attach `DesktopTrainingWorkbenchWidget->TakeWidget()`, avoid `AddToViewport`, and restore/destroy it during controller close/end-play. In standalone game mode hide the old game viewport while the desktop overlay is open and restore it when the workbench closes.

- [ ] **Step 4: Replace the old native hook behavior**

Remove `LWA_COLORKEY`, native window resizing, title-region clipping, and `HTCAPTION`. Keep only the subclass needed for selective `HTCLIENT`/`HTTRANSPARENT`, move persistence notifications, and topmost state.

- [ ] **Step 5: Verify GREEN**

Expected: native host contract tests and the complete Workbench automation suite pass.

### Task 5: Compile and perform real desktop verification

**Files:**
- No production file changes unless verification identifies a reproducible defect.

- [ ] **Step 1: Save editor packages safely if the editor has dirty packages**

Use the project UE 5.8 MCP scripts; do not force-close an editor with unsaved packages.

- [ ] **Step 2: Close the editor and compile without Live Coding**

Run the project UBT/test pipeline. Expected: `GameXXKEditor Win64 Development` succeeds.

- [ ] **Step 3: Run targeted and Workbench automation**

Expected: the transparent host test and all `GameXXK.DesktopTraining.Workbench` tests pass with no new errors.

- [ ] **Step 4: Launch the canonical 2D map**

Start `/Game/GameXXK/Maps/L_DesktopTrainingHUD` in a dedicated editor game window. Verify that the desktop/editor is visible through empty pixels, empty regions click through, Tab expands and collapses the inner HUD, and dragging changes only the inner HUD position.

- [ ] **Step 5: Leave the `.uproject` open for user validation**

Open the canonical project/map at the exact state needed for direct testing.
