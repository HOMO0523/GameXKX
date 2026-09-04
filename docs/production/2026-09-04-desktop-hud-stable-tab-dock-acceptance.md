# Desktop HUD Stable Tab Dock Acceptance

## Result

Commit `b12937b` keeps the desktop idle strip and Backpack Tab at fixed screen coordinates while the backpack expands above or below them. The player scale remains a manual 50%, 75%, or 100% choice; the existing 96/120-DPI bridge is unchanged.

## Final geometry

- Persistent idle strip: `1038 × 202` logical units in every state.
- Persistent Tab offset from the strip: `(953, 202)` with size `72 × 24`.
- Persistent wave progress width: `420` logical units.
- Expanded strip X: `318`, right-aligning its 1038-unit width with the authored center shell.
- Upward strip Y: `739`; the notice/control rail remains below it at Y `941`.
- Upward design height: `941 + current notice height`, normally `993`.
- Fixed native host: maximum `1820 × 993` logical units in collapsed and expanded states.

The content placement may extend past a horizontal work-area edge so the dock does not move. The native host remains clamped to the work area, and its content offset places the visible center shell and dock correctly inside that host.

## Region alignment

The upward center-content offset remains `(0, -210)`, but its rule and value now live in `GameXXKDesktopTrainingLayout`. UMG rendering and native Region generation both consume that value. The content, navigation, and toolbar Region shapes therefore follow their rendered coordinates; the strip, notice rail, Tab, chest controls, warehouse, and right-side panels remain unshifted.

## TDD and verification

The new automation cases failed against the old implementation before production changes:

- all 50%/75%/100% top/down and bottom/up strip anchors drifted;
- left and right work-area edge anchors drifted;
- upward content and toolbar were missing from their rendered Region positions;
- the old above-strip Tab rail still owned input;
- the below-strip upward Tab rail was absent from the Region.

After implementation:

- `GameXXK.DesktopTraining.Workbench.StableDockPlacement`: passed;
- `GameXXK.DesktopTraining.Workbench.UpwardNativeRegion`: passed;
- `GameXXK.DesktopTraining.Workbench.IdleStripControlRailAndRetryStates`: passed;
- `GameXXK.DesktopTraining.Workbench.ReferenceGeometry`: passed;
- `GameXXK.DesktopTraining.Workbench.StablePresentationScale`: passed;
- cold `GameXXKEditor Win64 Development -NoHotReload`: passed;
- complete Workbench group: 70/74 passed.

The four complete-group failures are unchanged baselines: `InnerGeometry`, `TravelCombatPresentation`, `TravelPartyAtlasAsyncFallback`, and `TravelPartyAtlasFallbackInventory`.

## Live desktop evidence

The final run used the persisted 50% player scale on a 120-DPI desktop and a right/bottom dock. Collapsed and upward-expanded states both kept the same physical native window rectangle `(783,371)-(1921,992)`, size `1138 × 621` physical pixels.

- `Saved/Diagnostics/hud-stable-dock-final-collapsed.png`
- `Saved/Diagnostics/hud-stable-dock-final-expanded-up.png`

The expanded backpack is visible above the unchanged dock without the previous upward Region clipping. The temporary bottom-dock test position was restored to the user's original `WindowPositionX=1` and `WindowPositionY=0.0587708652` after capture.
