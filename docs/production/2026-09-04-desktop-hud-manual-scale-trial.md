---
status: complete
owner: codex
updated_at: 2026-09-04T10:33:31+08:00
source_commit: 09d9034f95160eb44f35aa697d3fa057c6a789e0
---

# Desktop HUD manual scale and native-region trial

## Result

The desktop HUD no longer scales automatically with monitor work-area size. The persisted 50%, 75%, and 100% choices resolve to exactly `0.5`, `0.75`, and `1.0` on every tested work area. A 100% HUD is allowed to extend beyond a smaller screen, as approved for this trial.

The reported left-edge loss was reproduced as a second, independent scale path. On the current 125% Windows desktop, the manually DPI-managed overlay window and native region were already sized at 100%, while the UMG slot was divided by `1.25` and mouse coordinates were multiplied back by `1.25`. The rendered HUD therefore remained near 80% inside a 100% Region after the first scale change. Commit `930ba5d` removes that duplicate conversion and keeps one desktop-window client-coordinate space for Slate content, input, and `SetWindowRgn`.

## Changes

- `95608c8` removes work-area-derived automatic scale.
- `1ec8620` makes production Win32 region construction and its tests share `ResolveDesktopNativeRegionRect`, including the three-unit safety padding.
- `930ba5d` removes `DesktopInputDpiScale` and `PhysicalPixelsToSlateHost` from the manually DPI-managed overlay, and uses `ResolveDesktopSlateHostGeometry` for the UMG slot.
- `27318be` exposes 50%/75%/100% buttons, removes the obsolete automatic-adaptation sentence, and persists 75%.
- `09d9034` verifies that a recreated HUD restores the persisted 75% setting.
- `WM_DPICHANGED` and `WM_DISPLAYCHANGE` still invalidate monitor/work-area placement. The fixed maximum transparent host remains, so Tab expansion does not resize the composition swap chain.

## Runtime measurements

The verified monitor work area was 1536x816 with Windows desktop scaling at 125% and the HUD setting at 100%.

| State | Fixed host | Native Region | Rendered evidence |
|---|---:|---:|---|
| Before this trial | 1456x753 | 837x210 | Work-area automatic scale was 80%. |
| Manual scale before removing duplicate DPI conversion | 1820x941 | 1044/1045 x 261 | The captured non-black HUD bounds were only 723 pixels wide. |
| Final collapsed | 1820x941 | 1045x261 at `(361,61)` | Non-black bounds `(372,74)-(1389,291)`; the first rendered pixel is 11 units inside the Region because of artwork transparency, not a 40-pixel coordinate loss. |
| Final expanded | 1820x941 | 1120x901 at `(225,14)` | Non-black bounds `(249,26)-(1342,903)`; left circular actions, center panel, bottom navigation, and top strip are complete. |

Captured images:

- `Saved/Diagnostics/desktop-hud-manual100-dpifix-printwindow.png`
- `Saved/Diagnostics/desktop-hud-manual100-dpifix-expanded.png`

The expanded capture contains transparent black outside the authored surfaces and no opaque white resize residue.

## Verification

- RED `Saved/Automation/DesktopHudManualScale_RED/index.json`: one test discovered and failed with seven expected assertions covering 1280, 1536, and 2560 work areas plus the small-work-area maximum host.
- GREEN `Saved/Automation/DesktopHudManualScale_GREEN/index.json`: 1/1 passed, zero warnings and errors.
- RED cold UBT for the region contract failed only because `ResolveDesktopNativeRegionRect` did not yet exist.
- GREEN `Saved/Automation/DesktopHudRegionAlignment_GREEN/index.json`: 3/3 passed, zero warnings and errors.
- RED cold UBT for the manual-DPI host contract failed because `FDesktopSlateHostGeometry` and `ResolveDesktopSlateHostGeometry` did not yet exist.
- GREEN `Saved/Automation/DesktopHudManualDpiHost_GREEN/index.json`: 3/3 passed, zero warnings and errors.
- RED `Saved/Automation/DesktopHud75_RED/index.json`: both focused tests failed on the missing 75% button, obsolete automatic-adaptation sentence, and four work-area 75% assertions.
- GREEN `Saved/Automation/DesktopHud75Reload_GREEN/index.json`: 2/2 passed, including 75% selection, persistence, recreation, and absence of the obsolete sentence.
- Final broad scope `Saved/Automation/DesktopHudManualScale_FinalWorkbench/index.json`: 72 discovered, 68 passed, 4 failed. The four failures are the existing `InnerGeometry`, `TravelCombatPresentation`, `TravelPartyAtlasAsyncFallback`, and `TravelPartyAtlasFallbackInventory` baselines already recorded in `current-goal-acceptance.md`; this task did not touch their inventory/atlas/animation contracts or weaken their assertions.
- Cold UBT completed successfully with `-NoHotReload` before the final runtime measurements.

## Accepted limitation

At 100%, the fixed 1820x941 transparent host is larger than the current 1536x816 work area and can extend past the right or bottom screen edge. The active Win32 Region still follows the visible HUD surfaces, and 75% or 50% can be selected manually when a smaller HUD is preferred.
