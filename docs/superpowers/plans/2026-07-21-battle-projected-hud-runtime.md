# Battle projected HUD runtime implementation plan

> **Execution mode:** Continue the user's previously approved subagent-driven execution. Work directly on `main`; preserve all unrelated dirty content and assets.

## Goal

Make the battle HUD visible in real PIE, actor-following, resize/DPI-safe, and visually consistent with the supplied PSD. The board keeps ownership of screen-space HUD widgets; scene actors do not gain WidgetComponents.

## Acceptance evidence

- In the real `L_Main → town → route → battle` flow, every living displayed unit has a visible unit HUD under its sprite with correct HP/max HP, player-only MP/max MP, armor and status rows.
- Its anchor is non-zero and corresponds to the actor after the battle camera is active. A resize/DPI reflow retains the correspondence and avoids hand cards, party Qi, end turn and enemy intent.
- HP/MP use two-resource PSD-style texture brushes (track/frame + fill), never a flat `FSlateColorBrush` final fill.
- Probe output reports useful geometry diagnostics; it cannot silently reduce geometry failures to `None`.
- Unit, UI, battle and real-flow automated verification pass under a cold non-Hot-Reload build.

## Task 1 — expose real geometry/projection evidence before altering layout

**Files:**

- Modify: `Content/Python/gamexxk_probe_real_play_flow.py`
- Modify/add tests: `scripts/test_gamexxk_real_play_flow_probe.py`
- Modify if needed: `Source/GameXXK/Public/UI/GameXXKBattleUnitResourceWidget.h`

1. Add a failing Python test for an arranged widget that raises while reading geometry; require the output summary to include a named geometry-read error instead of raw `None`.
2. Implement a diagnostics-preserving screen-rect helper. Record cached local size, absolute position/size when available, and a short exception string when not.
3. Export `IsManaRowVisibleForTest` (or a single equivalent bool seam) to the UE Python layer; keep production UI behaviour unchanged.
4. Extend the battle-HUD observation report with Board projection data: per-unit received/current anchor, applied slot offset, visibility, canvas size and viewport/DPI revision.
5. Run the focused Python tests before and after the change.

## Task 2 — trace and repair the Controller → Board anchor handoff

**Files:**

- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify/add focused automation: `Source/GameXXK/Private/Tests/*Battle*Test.cpp`

1. Add a failing automation case covering a valid controller-style projected anchor, an immediately following board layout tick, and a single scheduling gap. It must stay visible only for the same viewport revision, then collapse when the viewport changes or the unit is dead.
2. Record compact non-shipping diagnostic state rather than logging every frame: projection source, result, latest valid anchor, canvas/viewport revision and last applied slot position.
3. Keep centre projections for targeting arrows independent from lower-body HUD projections. Try the visual lower bound first; if it cannot project, fall back to a verified actor/visual point plus a documented screen-space lower offset.
4. Replace the destructive per-board-tick reset with a bounded last-valid-anchor cache keyed by unit and viewport/DPI revision. Do not retain across a viewport change, invalid parent/canvas, unit death, or a consecutive projection failure threshold.
5. Preserve stable widget instances: refresh only data/position/visibility, never reconstruct the HUD every frame.
6. Run the focused C++ UI/battle tests; use their failures to choose the exact fallback source rather than guessing.

## Task 3 — make HP/MP resource bars use correctly separated PSD art

**Files / assets:**

- Inspect source: `C:\Users\shxuw\Downloads\nw-studio-nwueball-https-github-com\nw-studio-nwueball-https-github-com\work\psd_rebuild\manifest.json`, `clean_assets_v2/049.png`, `clean_assets_v2/050.png`, and source PSD layers where available.
- Add only if source lacks independent parts: documented PSD-style source/export assets under `Content/ArtSource/UI/Battle/Hud/` and imported textures under `Content/GameXXK/UI/Battle/Textures/PSD/HUD/`.
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleUnitResourceWidget.cpp`
- Modify/add focused test: `Source/GameXXK/Private/Tests/GameXXKBattleUnitResourceWidgetTest.cpp`

1. Treat `049` and `050` as composite reference images. Identify a true frame/track and a true fill texture before changing C++.
2. If a required independent element is missing, create it using the approved image-generation/PSD workflow, preserving subdued single-source ink-and-paper style. Do not substitute a flat colour brush.
3. Make `MakeResourceBarStyle` accept both a background/track texture and fill texture. Use texture-backed Slate brushes for both values and maintain correct fill clipping.
4. Give the bars a scalable preferred size and centre the labels over/with the bar so resource text remains legible without a fixed full-screen assumption.
5. Keep enemies' MP row collapsed and players' MP row visible. Add test seams to verify assigned texture paths/resource objects and current/max display values.

## Task 4 — integrate, cold-build, and prove in real PIE

1. Save current dirty packages through MCP before closing the editor. Do not force-close if the save state cannot be confirmed.
2. Perform a cold editor build only: `Build.bat GameXXKEditor Win64 Development -Project=... -NoHotReload` or `scripts/ue_tdd_pipeline.py`; never Live Coding/Hot Reload.
3. Run focused commandlet suites for Battle UI, MVP Battle and CardBattle integration, then run focused Python probe/lifecycle tests.
4. Start the editor through the project path, wait for any older PIE to end, and execute `scripts/gamexxk_real_play_flow_mcp.py --keep-pie` to reach the battle screen.
5. Capture the enhanced HUD observation and screenshot. Check non-zero anchors, applied slot offsets, visible widgets, text/state values and layout safe zones.
6. Perform a resize/DPI-style geometry change or the existing viewport override/real viewport check, then capture a second report. If real DPI manipulation is unsafe, report that fact and retain the automated viewport-revision proof rather than claiming manual DPI evidence.
7. Save packages after UE-side changes; close only after save confirmation. Re-run compile/test evidence if any C++ changes are made after the first build.

## Guardrails

- No actor WidgetComponent fallback.
- No changes to battle mechanics, card targeting, enemy intent or player state rules.
- No user asset overwrite, map/camera transform rewrite, worktree, Hot Reload or Live Coding verification.
- No final "complete" claim until fresh real PIE screenshot and report confirm the HUD is rendered.
