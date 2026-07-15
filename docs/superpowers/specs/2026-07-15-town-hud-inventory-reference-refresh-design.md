# Town HUD and Backpack Reference Refresh Design

## Goal

Replace the remaining old or placeholder town HUD and backpack visuals with direct crops from the approved Tencent reference art, while preserving every existing runtime interaction and data rule.

## Scope

Two independent UI surfaces are refreshed:

1. **Town HUD:** the top-left portrait card, health bar, and experience bar.
2. **Backpack window:** the complete reference-faithful visual shell: frame, return arrow, five tabs, empty item slot, equipment slot, capacity plate, organize button, decompose/sell button, and confirmation panel.

The HUD bar work is not part of the backpack layout. Backpack interactions remain unchanged: the five filters, sorting, decompose/sell confirmation, equipment enhancement, merchant trade, and close/back behavior use their current gameplay code.

## Visual Source and Asset Rules

- Authoritative source: `docs/reference-assets/2026-07-14-tencent-town-ui-source-2.png`.
- Existing source panels are reference-only guides: `docs/ui/panel_sources/hud_profile.png`, `character_window.png`, and `backpack_window.png`.
- New runtime textures are direct source crops only. Do not redraw, restyle, or use image generation.
- Decorative/fixed text may remain baked into a crop. Dynamic values, item quantities, item icons, capacity counts, HP, and XP are always rendered at runtime.
- Keep prior ImageGen inventory textures in place for rollback; do not overwrite their files or object paths.

## HUD Design

The existing town portrait remains the identity anchor. Add two independent bar widgets beneath/adjacent to the profile information:

- **HP bar:** reference-faithful ink/paper backplate with a red runtime fill driven by `PlayerHP / PlayerMaxHP`.
- **XP bar:** reference-faithful backplate with a green runtime fill driven by current level XP progress. The existing level and XP textual values remain dynamic.

The bar background, fill, and optional end caps are separate textures so a partial fill never exposes baked reference numbers or sample values.

## Backpack Design

Compose the window from source-faithful atomic pieces rather than drawing `backpack_window.png` as one background, because that panel contains sample items and baked `32/80` text.

- The outer parchment frame and return arrow become static background layers.
- Five category tabs and organize/decompose controls use direct cropped artwork; their existing buttons retain current click handlers.
- Empty backpack and equipment slots use the new paper/ink slot art. Runtime item texture, count, selection state, and equipment state remain separate child widgets.
- Capacity uses a cropped label plate plus runtime `current/max` text.
- The confirmation panel uses a source crop with the existing confirmation callbacks.

## Asset and Code Boundaries

- Cut/import path: extend `Content/Python/gamexxk_import_town_ui_assets.py`; it already imports Town UI textures with replacement support.
- HUD runtime: `Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp` and its matching public header.
- Backpack runtime: `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp` and its matching public header.
- Do not use `scripts/slice_inventory_ui_assets.py` or `Content/Python/gamexxk_import_inventory_ui_assets.py` for the refresh; they are tied to the previous ImageGen inventory sheet and cannot replace the existing textures safely.

## Acceptance Criteria

1. HUD shows the reference-style portrait card plus distinct red health and green experience bars driven by live state.
2. Backpack no longer shows old ImageGen frame, slots, equipment slots, action buttons, or confirmation frame.
3. No static crop exposes sample backpack items, sample item quantities, `32/80`, or sample HP/XP values as gameplay data.
4. Existing inventory controls and current game state operations still work without behavioral changes.
5. New assets import in UE 5.8, affected widgets compile, and a focused existing automation path validates inventory interaction after the visual swap.

## Verification Approach

This is an art/layout refresh, so no new TDD loop is added. Validation is limited to: source crop/alpha inspection, UE import and widget compile, visual PIE inspection, and the existing inventory automation test.
