# Backpack Equipment Slot Alignment Design

Date: 2026-08-05

## Goal

Straighten the six equipment slots on Master UI page `03_主角背包` without changing the approved V2 art, hero placement, equipped items, backpack grid, or runtime UI.

## Approved Layout

- Use two fixed equipment-slot columns at `x = 420` and `x = 930`.
- Use three evenly spaced rows at `y = 340`, `515`, and `690`.
- Render every slot at `118 × 124`.
- Render every equipment icon at `88 × 88`, inset `15` pixels from its slot origin.
- Preserve the existing left/right slot source art and the six starter-equipment icons.
- Preserve the hero, tabs, inventory grid, right-side scrollbar, text, and all other page positions.

## Validation

This is a pure Master UI visual adjustment and does not use TDD. After generation:

1. Confirm the three left slots share one x-coordinate and the three right slots share one x-coordinate.
2. Confirm both columns use the same three y-coordinates.
3. Confirm all six frames and icons retain uniform dimensions and insets.
4. Inspect the regenerated `03_主角背包` preview at `1920 × 1080` for visual alignment and unwanted overlap.
5. Run `git diff --check` and record the regenerated preview hash.

## Scope Boundary

No C++, UE assets, WBP wiring, gameplay logic, equipment data, or other Master pages are changed.
