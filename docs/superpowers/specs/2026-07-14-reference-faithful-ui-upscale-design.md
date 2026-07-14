# Reference-Faithful Task UI Upscale Design

## Goal

Improve the resolution of selected task-panel UI atoms while preserving the supplied Tencent Docs artwork as the visual authority. The result must still read as the original water-ink, old-paper UI art—not as a newly designed or vectorized UI set.

## Scope

The user-approved panel composition, large paper frame, paper-arrow treatment, and selected-tab treatment are style anchors. Do not change those approved atoms in this pass.

Rework only these source atoms first:

- `reward_coin.png`
- `reward_exp.png`
- `reward_token.png`

Keep the currently accepted panel/card/button base textures, arrow, and tab textures unchanged during this correction. Do not change widgets, gameplay, UMG layout, maps, or unrelated assets.

## Source Authority

Each atom begins from its matching crop in `docs/reference-assets/2026-07-14-tencent-town-ui-source.png`. The crop controls the final silhouette, composition, palette, glyphs, icon symbol, irregular edge, and opaque/transparent behavior.

## Generation Rule

Built-in image generation is used only as a source-faithful super-resolution edit. It may improve sharpness, recover local paper/ink detail, and reduce blur. It must not redraw, reinterpret, simplify, vectorize, clean up, recolor, move, add, remove, or replace any visual element.

Use this prompt for each crop:

> Upscale and restore this exact supplied UI crop only. Preserve the original water-ink hand-drawn style, old-paper texture, irregular brush edge, silhouette, composition, symbol, color, ink stroke, and background/alpha behavior exactly. Improve resolution and clarity only. Do not redraw, redesign, reinterpret, simplify, vectorize, add, remove, replace, recolor, restyle, move, or change any element. Keep all Chinese and Latin glyphs exactly unchanged. No new text or watermark.

## Acceptance Criteria

An output is accepted only when compared beside its crop and all of the following are true:

1. The object remains recognizably the same artwork: same contour, internal symbol, proportions, direction, and palette.
2. Water-ink brush strokes and aged-paper texture remain irregular; no clean geometric/flat-vector appearance.
3. No new border, checkerboard, opaque canvas, text, icon, or watermark appears.
4. `EXP` remains exactly `EXP`; no Chinese or Latin glyph changes are accepted.
5. The image is visibly clearer at intended UMG size without trading away source fidelity.

If a generated result changes the art, discard it. Keep the original source crop as the fallback rather than substituting a cleaner but different design.

## Verification

For each atom, retain a temporary side-by-side comparison with its source crop during review. Before import, verify PNG dimensions, alpha/opaque behavior, and that the approved output is the intended file. Re-run the existing task UI importer and verify `imported_count: 13`; then inspect the updated textures in the final UMG task panel once the panel implementation exists.
