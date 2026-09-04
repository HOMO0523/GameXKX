# Desktop HUD Manual Scale and Native Region Alignment

## Status

Implemented and verified as a trial on 2026-09-04. The user accepts that a 100% HUD may extend beyond a small monitor work area.

## Problem

The desktop HUD currently multiplies the user-selected HUD scale by an automatic scale derived from the monitor work area. On a 1536x816 work area, the automatic factor is 80%, so the authored 1038x202 idle strip is rendered at roughly 830x162 physical pixels.

The transparent Win32 host, the UMG HUD slot, and the Win32 window region used related but separately assembled measurements. The initial runtime showed a 1456x753 fixed transparent host and an 837x210 native-region bounding box. The region height included the scaled 52-pixel notice/control rail plus three native client units of defensive padding on each edge. That height difference was intentional, but the reported loss of more than 40 pixels at the left edge was not.

The live trial exposed a second scale path: Slate converts its logical host units through the target window DPI, while the Win32 Region uses native pixels. Removing this bridge made a 120-DPI window paint the HUD at 125% of its Region. The correct boundary divides physical HUD position/size by `GetDpiForWindow()/96` before Slate and multiplies Slate-local mouse input by the same value. A 96-DPI window uses `1.0`; no monitor-resolution inference is allowed.

The visible editor also launches with `-UserDir=Saved/InteractiveEditorUser`, so `GGameUserSettingsIni` is not a stable location across editor and automation launch modes. HUD scale persists in `Saved/Config/GameXXKDesktopHudSettings.ini` in editor builds, with the previous GameUserSettings paths retained only as migration fallbacks.

## Decision

Desktop HUD size will no longer depend on screen resolution or monitor work-area size.

- The 100% setting uses scale `1.0` on every monitor.
- The 75% setting uses scale `0.75` on every monitor.
- The 50% setting uses scale `0.5` on every monitor.
- A 100% HUD may extend beyond the monitor work area. This is accepted for the trial.
- Player scale determines the final physical HUD size. The Slate slot divides that physical position/size by the target window DPI; Slate-local mouse input multiplies by the same DPI to return to physical HUD coordinates.
- DPI conversion uses only the target window DPI. Windows that report 96 DPI use `1.0`; windows that report 120 DPI use `1.25`.
- `WM_DPICHANGED` and `WM_DISPLAYCHANGE` still refresh monitor/work-area placement.
- The fixed maximum transparent host remains in place so Tab expansion does not resize the composition swap chain and reintroduce opaque white pixels.

## Size Contract

At the 100% setting:

| Surface | Authored or physical size |
|---|---:|
| Collapsed idle strip | 1038x202 |
| Collapsed idle strip with one-line notice/control rail | 1038x254 |
| Native region bounding box with 3-unit edge padding | 1044-1045 x 260-261 after integer rounding |
| Maximum fixed transparent host | 1820x941 |

At the 75% and 50% settings, authored HUD measurements are multiplied by `0.75` and `0.5`. Region padding remains three native client units per edge.

The transparent host is deliberately larger than the collapsed HUD. The native region, hit testing, and visible HUD must agree on the active content origin even though the host does not shrink.

## Coordinate Contract

The layout calculation produces one manual HUD scale, one active HUD top-left position, and one fixed-host content offset.

1. Authored UMG geometry is expressed in design units.
2. The manual HUD setting converts design units to desktop-window client units.
3. The fixed-host content offset positions the active HUD inside the maximum transparent host.
4. The UMG slot divides physical position and size by the target window DPI before Slate performs its normal DPI conversion.
5. Slate-local mouse coordinates are multiplied by the target window DPI before HUD hit testing.
6. Native region shapes use the manual HUD scale and fixed-host content offset directly in physical pixels.
7. `SetWindowRgn` adds only the existing three-unit safety padding.

For every active region shape, the native left/top edge may differ from the corresponding rendered HUD edge only by the three-unit region padding, integer rounding, and the artwork's own transparent border. A discrepancy of roughly 40 pixels is a failure.

## Implementation Scope

- Remove the work-area-derived factor from the effective desktop HUD scale.
- Remove the public automatic-scale helper so future code cannot accidentally restore resolution-based scaling.
- Keep the persisted manual setting and expose exactly 50%/75%/100% controls.
- Keep one reversible DPI bridge at the Slate boundary, plus display-change notifications, fixed-host composition, mouse passthrough, and complex region shapes.
- Persist manual scale in one project-owned settings file across editor `-UserDir` variants, and migrate legacy GameUserSettings values once.
- Make the tests compare the HUD slot origin and native region origin through the shared fixed-host offset.
- Do not change idle-strip artwork, authored dimensions, notification modes, or Tab expansion layout in this trial.

## Verification

The implementation will use test-first verification:

1. Add failing layout tests proving that 1280, 1536, 1920, and 2560-pixel work areas resolve to `1.0`, `0.75`, and `0.5` at the 100%, 75%, and 50% settings.
2. Add a failing alignment test proving the collapsed HUD and native region share the same left/top origin within the three-pixel padding.
3. Confirm 96-DPI windows use `1.0`, 120-DPI windows use `1.25`, and Slate output returns to the requested physical HUD size.
4. Confirm a stable 50% setting overrides a conflicting launch-specific legacy value, persists a new selection, and survives HUD recreation and PIE restart.
5. Run the focused desktop-workbench automation tests after implementation.
6. Run the canonical `/Game/GameXXK/Maps/L_DesktopTrainingHUD` desktop presentation and inspect the collapsed left edge, notice/control rail, Tab expansion, mouse passthrough, and transparent background.

## Accepted Limitation

At 100%, the 1820x941 fixed host does not fit a 1536x816 work area. Windows may place part of that transparent host beyond the right or bottom screen edge. The 75% and 50% settings are the manual fallbacks when the user wants a smaller HUD.
