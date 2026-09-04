---
status: complete
owner: codex
updated_at: 2026-09-04T13:24:28+08:00
source_commit: f06bb1742709b191a258a63e2456f95adcd04260
---

# Desktop HUD manual scale and native-region trial

## Result

The desktop HUD no longer scales automatically with monitor work-area size. The persisted 50%, 75%, and 100% choices resolve to exactly `0.5`, `0.75`, and `1.0` on every tested work area. A 100% HUD is allowed to extend beyond a smaller screen, as approved for this trial.

The reported clipping was a DPI-boundary error. A visible 120-DPI window lets Slate paint logical units at `1.25`, while the native Region is expressed in physical pixels. The final bridge divides HUD geometry by `1.25` before Slate and multiplies Slate-local mouse input by `1.25`. A 96-DPI automation window uses `1.0`, because Windows scales that window as a whole. Only `GetDpiForWindow` selects this conversion; monitor-resolution inference is not used.

The apparent failure to restore 50% was also real: the visible editor starts with `-UserDir=Saved/InteractiveEditorUser`, so it read a different `GameUserSettings.ini` from automation and earlier diagnostics. HUD scale now persists in `Saved/Config/GameXXKDesktopHudSettings.ini` for editor builds. Launch-specific GameUserSettings files are read only as migration fallbacks and are still synchronized when the player changes the setting.

## Changes

- `95608c8` removes work-area-derived automatic scale.
- `1ec8620` makes production Win32 region construction and its tests share `ResolveDesktopNativeRegionRect`, including the three-unit safety padding.
- `930ba5d` was an intermediate trial that removed the DPI bridge; later visible-PIE evidence superseded that interpretation.
- `27318be` exposes 50%/75%/100% buttons, removes the obsolete automatic-adaptation sentence, and persists 75%.
- `09d9034` verifies that a recreated HUD restores the persisted 75% setting.
- `f076d1e` restores one reversible DPI bridge, distinguishes 96-DPI and 120-DPI target windows, and converts mouse input back to physical HUD coordinates.
- `c1526be` adds the stable project-owned HUD settings file and legacy migration.
- `f06bb17` isolates stable and legacy configuration in Workbench tests.
- `WM_DPICHANGED` and `WM_DISPLAYCHANGE` still invalidate monitor/work-area placement. The fixed maximum transparent host remains, so Tab expansion does not resize the composition swap chain.

## Runtime measurements

The verified visible editor ran at 120 DPI with the HUD setting stored as 50%.

| State | Fixed host | Native Region | Rendered evidence |
|---|---:|---:|---|
| Before stable settings migration | 1820x941 | 1045x261 | Project GameUserSettings contained 50%, but visible PIE's redirected file had no key and loaded the 100% default. |
| Final visible PIE, 50% | 910x471 | 526x134 at `(269,5)` | Non-black bounds `(277,13)-(785,122)` remain inside the Region; only the artwork's 8-10 pixel transparent edge remains. |
| Next visible PIE, 50% | 910x471 | 526x134 | Stable file remained 50%; window and Region were identical after stop/start. |

Captured images:

- `Saved/Diagnostics/desktop-hud-visible-50-persist-dpi-final.png`

The final 50% capture contains the complete idle strip and control rail without Region clipping.

## Verification

- RED `Saved/Automation/DesktopHudManualScale_RED/index.json`: one test discovered and failed with seven expected assertions covering 1280, 1536, and 2560 work areas plus the small-work-area maximum host.
- GREEN `Saved/Automation/DesktopHudManualScale_GREEN/index.json`: 1/1 passed, zero warnings and errors.
- RED cold UBT for the region contract failed only because `ResolveDesktopNativeRegionRect` did not yet exist.
- GREEN `Saved/Automation/DesktopHudRegionAlignment_GREEN/index.json`: 3/3 passed, zero warnings and errors.
- RED cold UBT for the manual-DPI host contract failed because `FDesktopSlateHostGeometry` and `ResolveDesktopSlateHostGeometry` did not yet exist.
- GREEN `Saved/Automation/DesktopHudManualDpiHost_GREEN/index.json`: 3/3 passed, zero warnings and errors.
- RED `Saved/Automation/DesktopHud75_RED/index.json`: both focused tests failed on the missing 75% button, obsolete automatic-adaptation sentence, and four work-area 75% assertions.
- GREEN `Saved/Automation/DesktopHud75Reload_GREEN/index.json`: 2/2 passed, including 75% selection, persistence, recreation, and absence of the obsolete sentence.
- GREEN `Saved/Automation/DesktopHudDpiBridgeFinal_GREEN/index.json`: 4/4 passed for 96/120-DPI resolution, Slate round-trip, Region, manual scale, and persistence controls.
- RED `Saved/Automation/DesktopHudStableSettings_RED/index.json`: stable 50% lost to launch-specific 100%, and selection did not write the stable file.
- GREEN `Saved/Automation/DesktopHudStableSettings_GREEN_v2/index.json`: 1/1 passed for stable precedence, write, and HUD recreation.
- Final broad scope `Saved/Automation/DesktopHudDpiPersistence_FinalWorkbench_v2/index.json`: 72 discovered, 68 passed, 4 failed. The four failures are the existing `InnerGeometry`, `TravelCombatPresentation`, `TravelPartyAtlasAsyncFallback`, and `TravelPartyAtlasFallbackInventory` baselines already recorded in `current-goal-acceptance.md`; this task introduced no new failure.
- Cold UBT completed successfully with `-NoHotReload` before the final runtime measurements.

## Accepted limitation

At 100%, the fixed 1820x941 transparent host is larger than the current 1536x816 work area and can extend past the right or bottom screen edge. The active Win32 Region still follows the visible HUD surfaces, and 75% or 50% can be selected manually when a smaller HUD is preferred.
