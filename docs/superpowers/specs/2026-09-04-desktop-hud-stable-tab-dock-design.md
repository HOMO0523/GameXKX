# Desktop HUD Stable Tab Dock Design

## Goal

Opening or closing the backpack must not move the idle strip, notice/control rail, or Backpack Tab on the desktop. The backpack body expands into available space above or below that fixed dock group. Player scale remains a manual 50%, 75%, or 100% choice.

## Current fault

The collapsed and expanded states use different idle-strip sizes and different Tab offsets. A bottom-docked collapsed HUD also reserves a 52-unit notice area below the strip, while the upward-expanded layout moves that notice area above the strip. Recomputing the window from those different bounds moves the strip by `notice height × player scale` and moves the Tab to the opposite edge.

The upward layout also moves center widgets by a 210-unit render transform without applying the same offset to the Win32 Region. This clips visible controls and makes part of the rendered surface click-through.

## Fixed dock group

The persistent group contains:

- idle strip: `1038 × 202` logical units in every state;
- notice/control rail: directly below the strip;
- Backpack Tab: fixed at `(953, 202)` relative to the strip;
- full 420-unit wave-progress width in every state.

The expanded strip is right-aligned with the authored center shell. Its local X is therefore `386 + 970 - 1038 = 318`. Keeping the same strip size and the same Tab-relative offset makes both screen rectangles invariant.

For downward expansion the strip starts at local Y `0`. For upward expansion it starts at local Y `739`, leaving the existing shifted backpack body above it. The notice/control rail remains below the strip at local Y `941`; the upward canvas therefore adds the current notice height below the 941-unit reference canvas.

## Placement

The saved normalized position continues to describe the idle-strip anchor. Before either collapsed or expanded placement is calculated, that anchor is clamped once so the complete fixed dock group fits in the work area. Both states then use this same physical strip anchor.

The transparent native host always reserves the maximum `1820 × 993` logical bounds, including the upward notice rail. Changing the backpack state changes only the content offset inside that host. It must preserve the resulting screen rectangles of the idle strip and Backpack Tab whenever the selected manual scale physically fits.

At a horizontal work-area edge, the expanded design canvas may extend beyond the fixed host while the visible center shell remains clipped to the work area. The fixed dock wins over forcing every transparent design-canvas pixel on screen. The fixed native host itself remains clamped to the work area.

## Upward content and native Region

The 210-unit upward offset remains the authored relationship for center content, but its eligibility and offset come from the layout module. UMG positioning and `BuildDesktopNativeRegionShapes` use that same rule. The strip, notice rail, Tab, chest controls, warehouse, right panel, town button, and story button do not receive this center-content offset.

## Acceptance

- At 50%, 75%, and 100%, a top-docked HUD preserves the strip and Tab screen positions when expanding downward.
- At 50%, 75%, and 100%, a bottom-docked HUD preserves the strip and Tab screen positions when expanding upward.
- The expanded idle strip stays `1038 × 202` before manual scaling.
- Upward content and toolbar points are included in the native Region at their rendered positions; their old unshifted-only points are not required to remain interactive.
- DPI conversion remains outside the logical layout and continues to use the existing physical-pixel/Slate bridge.
