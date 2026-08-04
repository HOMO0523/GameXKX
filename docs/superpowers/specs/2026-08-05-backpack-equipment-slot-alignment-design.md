# Backpack and Meta Shop Alignment Design

Date: 2026-08-05

## Goal

Straighten the six equipment slots on Master UI page `03_主角背包` and correct control-text centering on pages `03_主角背包` and `07_商店交易`, without changing the approved V2 art, hero placement, equipped items, backpack grid, shop contents, or runtime UI.

## Approved Layout

- Use two fixed equipment-slot columns at `x = 420` and `x = 930`.
- Use three evenly spaced rows at `y = 340`, `515`, and `690`.
- Render every slot at `118 × 124`.
- Render every equipment icon at `88 × 88`, inset `15` pixels from its slot origin.
- Preserve the existing left/right slot source art and the six starter-equipment icons.
- Preserve the hero, tabs, inventory grid, right-side scrollbar, text, and all other page positions.

## Approved Text Alignment

- Add one reusable centered-text placement path that measures the rendered font bounds instead of estimating Chinese text width.
- On `03_主角背包`, center the five tab labels within their `105 × 62` tab bounds.
- On `03_主角背包`, center the five inventory category labels within explicit equal-height category regions and distribute the four bottom stat labels evenly.
- On `07_商店交易`, center each of the seven product names and prices beneath its own product card.
- On `07_商店交易`, center the selected-product title within the detail column and center `购买 100` within the purchase button.
- Keep page titles, subtitles, descriptive paragraphs, probability text, HUD resource values, and capacity text left-aligned unless they already use a centered control region.
- Do not bake text into images; all text remains editable and independently manifested.

## Validation

This is a pure Master UI visual adjustment and does not use TDD. After generation:

1. Confirm the three left slots share one x-coordinate and the three right slots share one x-coordinate.
2. Confirm both columns use the same three y-coordinates.
3. Confirm all six frames and icons retain uniform dimensions and insets.
4. Confirm each approved short label is centered from measured font bounds within its control or assigned region.
5. Inspect the regenerated `03_主角背包` and `07_商店交易` previews at `1920 × 1080` for visual alignment, readable hierarchy, and unwanted overlap.
6. Run `git diff --check` and record both regenerated preview hashes.

## Scope Boundary

No C++, UE assets, WBP wiring, gameplay logic, equipment data, shop product rules, or other Master pages are changed.
