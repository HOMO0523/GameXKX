# Desktop HUD Drag Stability Design

Date: 2026-08-30

## Goal

Make the desktop idle-strip window follow a left-button drag without snapping, overshooting, or oscillating. The point pressed on the strip should remain under the pointer until the desktop work-area boundary prevents further movement. Releasing the button must persist the final position.

## Confirmed Root Cause

The current drag update reconstructs a physical pointer position by combining two coordinate sources that update on different schedules:

- `FGeometry::AbsoluteToLocal(...)`, whose result depends on the moving Slate window geometry;
- `GetWindowRect(...)`, which reads the native window position after `SetWindowPos(...)`.

While the captured window is moving, Slate geometry can still describe the previous window position while Win32 already reports the new one. Adding the two values can count part of the window movement twice. The following event then observes a different pairing and compensates in the opposite direction, producing the visible large back-and-forth jump.

The drag implementation was introduced with commit `0107aeb`. The recently removed system-wide low-level mouse hook is a separate issue and must remain removed.

## Selected Approach

Use an immutable drag-start snapshot and a global physical-pointer delta.

When a drag begins on a non-control area of the idle strip, record:

- the current Win32 cursor position in physical desktop pixels;
- the current `DesktopWindowPositionNormalized` value.

For every captured drag update:

1. Read the current cursor position in physical desktop pixels.
2. Compute `PointerDelta = CurrentPointer - DragStartPointer`.
3. Compute the available anchor travel from the physical work-area size and collapsed strip size.
4. Resolve the new anchor as:

   `DragStartNormalizedAnchor + PointerDelta / AvailableAnchorTravel`

5. Clamp each normalized component to `[0, 1]`.
6. Feed the result into the existing overlay-placement and native-window-layout path.

This calculation never reads the moving window's current Geometry or native rectangle. For any given pointer position it therefore produces one deterministic anchor, independent of whether Slate and Win32 have completed the previous window move.

## Components and Responsibilities

### Pure drag resolver

Add a small pure function to `GameXXKDesktopTrainingLayout` that accepts the drag-start normalized anchor, drag-start/current physical pointer positions, physical host size, and collapsed strip size. It returns the clamped normalized anchor.

The function owns only drag mathematics. It does not read Slate state, Win32 state, configuration, or widget fields, making the regression directly testable.

### Workbench drag state

Replace the moving-window-relative pointer offset with two immutable values for the lifetime of a drag:

- drag-start physical pointer position;
- drag-start normalized anchor.

The existing `bDesktopHudDragging` flag remains the drag lifetime authority.

### Platform pointer sampling

On Windows, sample the cursor with `GetCursorPos` only while a local idle-strip drag is beginning or active. This is ordinary polling from the widget's captured input events; it does not install a global hook and does not intercept or synthesize player input.

If initial pointer sampling fails, do not begin a drag. If sampling fails during an active drag, leave the anchor unchanged for that event. Mouse-up or capture-loss must still end the drag and release capture safely.

## Preserved Behavior

This change must not alter:

- idle-strip button and control hit regions;
- folded/expanded state or backpack/warehouse behavior;
- click-through policy outside interactive surfaces;
- always-on-top behavior;
- window visibility or minimization policy;
- story, guide, task, battle, or town flow;
- final position persistence on mouse-up or capture-loss;
- existing work-area boundary clamping;
- the absence of `WH_MOUSE_LL` and other system-wide mouse hooks.

`SetWindowPos` flags are deliberately out of scope for the first fix. Changing them at the same time would introduce a second variable and make the root-cause test inconclusive.

## Verification

### Automated tests

Add focused tests for the pure resolver before production changes:

- zero pointer movement returns the exact starting anchor;
- horizontal, vertical, and diagonal physical-pixel deltas map one-for-one into anchor travel;
- repeating the same pointer sample returns the same anchor, independent of any hypothetical window position;
- negative and excessive deltas clamp cleanly to the work-area boundaries;
- a degenerate travel axis remains finite and deterministic;
- the existing no-global-mouse-hook policy test continues to pass.

The new behavior test must be observed failing before the implementation is added.

### Build and manual acceptance

After tests pass, perform a cold UBT build with no Live Coding or Hot Reload. Then launch only the ordinary `/Game/GameXXK/Maps/L_DesktopTrainingHUD` game window with no automation runner or synthesized clicks.

Manual acceptance covers:

- slow and fast dragging in both axes;
- folded and expanded idle-strip states;
- dragging near all work-area edges;
- no movement when the pointer is held still;
- no snap when beginning the drag;
- the original pressed point remaining under the pointer until boundary clamping;
- the saved position surviving close and relaunch;
- idle-strip buttons remaining clickable and global mouse responsiveness remaining normal.

## Non-goals

- Cross-monitor drag redesign.
- Native title-bar or `WM_NCLBUTTONDOWN` window dragging.
- Smoothing, inertia, animation, or cursor warping.
- Any viewport, story, guide, task-panel, or idle-strip UX redesign.
