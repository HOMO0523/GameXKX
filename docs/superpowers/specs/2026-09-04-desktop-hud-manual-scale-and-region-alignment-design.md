# Desktop HUD Manual Scale and Native Region Alignment

## Status

Implemented and verified as a trial on 2026-09-04. The user accepts that a 100% HUD may extend beyond a small monitor work area.

## Problem

The desktop HUD currently multiplies the user-selected HUD scale by an automatic scale derived from the monitor work area. On a 1536x816 work area, the automatic factor is 80%, so the authored 1038x202 idle strip is rendered at roughly 830x162 physical pixels.

The transparent Win32 host, the UMG HUD slot, and the Win32 window region used related but separately assembled measurements. The initial runtime showed a 1456x753 fixed transparent host and an 837x210 native-region bounding box. The region height included the scaled 52-pixel notice/control rail plus three native client units of defensive padding on each edge. That height difference was intentional, but the reported loss of more than 40 pixels at the left edge was not.

The live trial exposed a second scale path: the overlay `SWindow` already used `bManualManageDPI(true)`, but the UMG slot was additionally divided by the monitor's 125% scale and mouse coordinates were multiplied back by the same value. That made the rendered HUD approximately 80% of the 100% native region. The manual-DPI window must use one client-coordinate space without this second conversion.

## Decision

Desktop HUD size will no longer depend on screen resolution or monitor work-area size.

- The 100% setting uses scale `1.0` on every monitor.
- The 50% setting uses scale `0.5` on every monitor.
- A 100% HUD may extend beyond the monitor work area. This is accepted for the trial.
- The manually DPI-managed `SWindow`, its Slate content, input, and native region use the same client-coordinate units. Monitor DPI is not applied again to the HUD slot or mouse coordinates.
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

At the 50% setting, authored HUD measurements are multiplied by `0.5`. Region padding remains three native client units per edge.

The transparent host is deliberately larger than the collapsed HUD. The native region, hit testing, and visible HUD must agree on the active content origin even though the host does not shrink.

## Coordinate Contract

The layout calculation produces one manual HUD scale, one active HUD top-left position, and one fixed-host content offset.

1. Authored UMG geometry is expressed in design units.
2. The manual HUD setting converts design units to desktop-window client units.
3. The fixed-host content offset positions the active HUD inside the maximum transparent host.
4. The UMG slot uses that position and size directly because its `SWindow` manually manages DPI.
5. Native region shapes use the same manual HUD scale and the same fixed-host content offset in the same client-coordinate space.
6. `SetWindowRgn` adds only the existing three-unit safety padding.

For every active region shape, the native left/top edge may differ from the corresponding rendered HUD edge only by the three-unit region padding, integer rounding, and the artwork's own transparent border. A discrepancy of roughly 40 pixels is a failure.

## Implementation Scope

- Remove the work-area-derived factor from the effective desktop HUD scale.
- Remove the public automatic-scale helper so future code cannot accidentally restore resolution-based scaling.
- Keep the existing 100%/50% persisted setting and controls.
- Keep DPI/display-change notifications, fixed-host composition, mouse passthrough, and complex region shapes; remove the duplicate monitor-DPI conversion from the manually managed window.
- Make the tests compare the HUD slot origin and native region origin through the shared fixed-host offset.
- Do not change idle-strip artwork, authored dimensions, notification modes, or Tab expansion layout in this trial.

## Verification

The implementation will use test-first verification:

1. Add failing layout tests proving that 1280, 1536, 1920, and 2560-pixel work areas all resolve to scale `1.0` at the 100% setting and `0.5` at the 50% setting.
2. Add a failing alignment test proving the collapsed HUD and native region share the same left/top origin within the three-pixel padding.
3. Confirm a 125% Windows desktop does not alter the manually managed HUD slot position or size.
4. Run the focused desktop-workbench automation tests after implementation.
5. Run the canonical `/Game/GameXXK/Maps/L_DesktopTrainingHUD` desktop presentation and inspect the collapsed left edge, notice/control rail, Tab expansion, mouse passthrough, and transparent background.

## Accepted Limitation

At 100%, the 1820x941 fixed host does not fit a 1536x816 work area. Windows may place part of that transparent host beyond the right or bottom screen edge. The 50% setting remains the manual fallback when the user wants the complete expanded HUD visible on a smaller display.
