# Desktop HUD Adaptive Body and Cursor Acceptance

## Result

Commit `821593b` separates the draggable idle dock from the expanded workbench body. The transparent native host now covers the current monitor work area, while the Win32 Region continues to expose only visible HUD surfaces. Dragging keeps the current layout rigid; releasing the left mouse button recalculates vertical direction and one shared horizontal body correction from the final idle-strip position.

Warehouse, backpack, navigation, training, and tools now use one body coordinate domain. Story and town buttons share the same correction, so toggling warehouse or moving the body upward no longer detaches them. Carried items render in a separate work-area cursor canvas and use native client pixels divided by window DPI; dock position, body correction, warehouse extension, and upward offset never enter the item position.

## Final layout contract

- Manual scale remains exactly `50% / 75% / 100%`; there is no screen-driven scale selection.
- Native desktop host is `(0,0,work-area width,work-area height)` at every manual scale.
- Idle strip, notification controls, Tab, chest controls, and retry remain the fixed dock group.
- Warehouse and right-side papers use logical Y `244..925`, height `681`, matching backpack plus navigation.
- Each side paper is continuous, with an internal divider at logical Y `786`.
- Warehouse retains 36 logical slots per page in a 4×9 scrollable grid; totals and batch actions occupy the lower shelf.
- Training upper area contains title, difficulty, chapter tabs, three vertical stage nodes, current selection, and current travel state. Challenge and travel actions occupy the lower shelf.
- Tools keeps its 3×3 input area above the divider and its confirmation action in the lower shelf.
- At a right-edge 100% dock with warehouse and a right panel open, the body correction is `(-304,0)` while the dock stays fixed.
- Upward expansion contributes one shared body correction of `(0,-210)` before any horizontal fit correction.

## Verification

- Cold UBT: `GameXXKEditor Win64 Development -NoHotReload` succeeded after the final source edit.
- Complete `GameXXK.DesktopTraining.Workbench` report: `69` success, `1` success with warning, `4` failed, `0` not run (`Saved/Automation/WorkbenchAdaptiveFinal2/index.json`). The four failures are the pre-existing `InnerGeometry`, `TravelCombatPresentation`, `TravelPartyAtlasAsyncFallback`, and `TravelPartyAtlasFallbackInventory` baselines.
- New and affected checks passed, including `StableDockPlacement`, `UpwardNativeRegion`, `ReferenceGeometry`, `StablePresentationScale`, `CarriedRightClickCancelKeepsBackpack`, `StructuralGeometry`, `TownTogglePresentation`, `NativeWindowRegion`, `IdleStripControlRailAndRetryStates`, and `ThreeNodeChapterMapPanel`.

## Live PIE evidence

- The native host measured `[0,0,1920,1020]` on the 1920×1020 work area.
- Warehouse, backpack, and vertical training panel coexist in `Saved/Diagnostics/hud-adaptive-all-panels-controlled.png`.
- Right-edge release produced HUD top-left `[408,45]`, strip top-left `[757.5,45]`, and body offset `[-304,0]`; see `Saved/Diagnostics/hud-adaptive-right-release.png`.
- Upward release produced HUD top-left `[2.62,71.25]`, strip top-left `[352.12,625.5]`, and body offset `[0,-210]`; see `Saved/Diagnostics/hud-adaptive-up-release.png`.
- With the upward layout and all panels open, the carried-item image center matched the native cursor at `(1535,800)`; see `Saved/Diagnostics/hud-adaptive-up-carried.png`.

The user's saved desktop HUD settings were restored after testing: `HudScalePercent=75`, `WindowPositionX=0.578778505`, and `WindowPositionY=0.064710319`.
