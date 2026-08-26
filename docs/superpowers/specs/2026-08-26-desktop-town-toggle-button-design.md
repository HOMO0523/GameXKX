# Desktop Town Toggle Button Design

**Date:** 2026-08-26  
**Status:** User-approved design  
**Related design:** `docs/superpowers/specs/2026-08-26-desktop-background-layer-cutout-design.md`

## Goal

Add one large, clearly labeled town-transition button to the shared placement HUD. The same control enters the existing 3D Qingshan town from the pure-2D desktop map and exits the town back to the pure-2D desktop map while preserving stable HUD page state.

## Visual contract

- The button is a `144 x 144` transparent square containing one circular ink mark.
- The circle is hand-brushed rather than geometrically perfect: its stroke varies in thickness, contains visible brush-tip taper, and may include a few controlled dry-ink gaps.
- A minimal roofline, gate, and two or three building silhouettes grow from the circle's upper arc. They remain part of the same ink drawing and do not receive a separate icon tile.
- The normal state uses deep neutral ink with a very small warm-brown accent; it has no rectangular paper backing.
- Runtime Chinese text is centered inside the circle in two lines:
  - desktop map: `进入` / `城镇`;
  - 3D town map: `退出` / `城镇`.
- Text is not baked into the texture. This keeps the Chinese crisp at 100% and 50% HUD scale and lets both states share one clean icon asset.
- Hover darkens the ink slightly. Pressed state scales the complete button to approximately 96%. No extra arrow, glow, badge, or decorative frame is added.

## Display and placement

- The control exists only while the backpack/workbench is expanded.
- It is absent from the collapsed idle-strip-only state.
- With the warehouse closed, it occupies the unused space immediately to the left of the backpack/center shell.
- With the warehouse open, it moves to the outside-left edge of the warehouse with a `14` logical-pixel gap.
- Opening the warehouse extends the transparent HUD design bounds to the left by approximately `148` logical pixels. Existing workbench content shifts right inside the native window so the idle-strip screen anchor does not move.
- At the current 100% baseline, the widest state is approximately `1820` pixels and remains within the 1920-pixel work area. Smaller displays continue to use the existing automatic HUD fit scale.
- The button contributes a client-input hit region. Transparent pixels around it and between panels retain desktop click-through.

## Map behavior

The button targets these exact maps:

- enter: `/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo`;
- exit: `/Game/GameXXK/Maps/L_DesktopTrainingHUD`.

The runtime `Screen` remains Town and the transition does not reset training, travel, task, inventory, equipment, card, or combat state.

The action disables itself as soon as map travel begins so a double-click cannot enqueue a second load. The old overlay is closed through its normal lifecycle before map travel. The destination controller reconstructs the shared HUD after the new world is ready.

## Session-state preservation

A transient `UGameInstanceSubsystem` owns one pending workbench-session snapshot across map travel. It is not written to the save file and is consumed exactly once by the destination workbench.

The snapshot preserves stable presentation choices:

- backpack expanded state;
- warehouse open state and warehouse page;
- active center page and right-side panel;
- active navigation entry;
- active tool mode, combine kind, and socket index;
- active character and formation roster;
- selected backpack character, companion, task NPC, and formation candidate;
- training difficulty, chapter, and selected stage;
- roster-member expansion state;
- embedded inventory character tab and pending deck selection;
- HUD settings-panel state.

Before taking the snapshot, the widget safely cancels transient inventory interaction:

- a carried item returns to its authoritative backpack, warehouse, equipment, or tool origin;
- temporary tool-grid reservations are cleared without consuming or duplicating items;
- hover state, open dropdown state, pointer capture, drag state, exit confirmation, and carried-item visuals are not restored after travel.

The destination applies the snapshot only after the widget has a valid MVP subsystem and has completed its normal construction. Invalid character IDs, warehouse pages, stages, or other stale selections are clamped through the same rules used by the existing workbench.

## Presentation-mode behavior

**Pure-2D desktop map:** The HUD uses the project DirectComposition overlay described by the related background-layer design. It remains draggable and restores its persisted desktop anchor.

**3D Qingshan town:** The same HUD is hosted by the existing town viewport path. Its position resets to the default initial anchor, dragging is disabled, and all buttons remain interactive. Stable page/session state is restored after the presentation mode is applied.

## Asset ownership

- Source PNG: `SourceArt/UI/DesktopOverlay/T_DesktopTownToggleInk.png`
- Runtime texture: `/Game/GameXXK/UI/DesktopOverlay/T_DesktopTownToggleInk`
- Deterministic import script: `Content/Python/gamexxk_import_desktop_town_toggle.py`

The source and runtime asset must be square, RGBA, transparent outside the ink, and must not contain baked Chinese text. The imported texture uses UI texture settings, preserves alpha, and must not stretch the circular drawing.

## Failure behavior

- If the target map cannot be resolved, the button remains on the current map and is re-enabled.
- If map travel does not start, the pending session snapshot is discarded rather than leaking into a later unrelated load.
- If the destination cannot restore one field, it restores the remaining valid fields and uses the existing safe default for that field.
- Returning to the desktop does not reintroduce the deleted full-canvas paper background.

## Verification

### Deterministic tests

- Desktop map resolves the enter label and Qingshan target.
- Qingshan and legacy Qingshan map names resolve the exit label and desktop target.
- The button is absent while collapsed and present while expanded.
- Closed-warehouse placement is immediately left of the center shell.
- Open-warehouse placement is immediately left of the warehouse and extends the HUD bounds without moving the strip anchor.
- The button asset has a loadable runtime path and no baked-text dependency.
- A session snapshot round-trip preserves every stable field listed above.
- Transient carry, tool reservations, dropdown, pointer, and modal state do not survive the round-trip.
- Town presentation restores the default fixed anchor and cannot drag; desktop presentation remains draggable.
- A second click during pending travel is ignored.

### Visual and real-flow verification

- Review the generated clean PNG before runtime import.
- Verify the 2 x 2 Chinese text layout at 100% and 50% HUD scale.
- Verify normal, hover, and pressed presentation against the current ink-and-paper UI.
- Open and close the warehouse and confirm that the button moves to the correct outer-left edge without shifting the idle strip.
- Enter Qingshan with several different page/tab states, then exit and confirm identical stable presentation state on both sides.
- Verify carried items and tool reservations return safely before travel.
- Verify both directions in a Development packaged build, not only PIE.

## Non-goals

- No new town map.
- No task, quest, inventory, combat, or training reset.
- No second simultaneous town button.
- No baked Chinese text texture.
- No rectangular button backing.
- No change to the existing town environment art or gameplay.
