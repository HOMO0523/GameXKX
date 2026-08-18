# Hero left walk-loop pilot

This is an isolated review asset for the desktop training strip. It does not
replace the approved hero idle/attack atlases and is not imported into Unreal.

## Generation contract

- Static contact pose: `input/hero_walk_contact_1600.png`, normalized from the
  current 1600×1600 hero safe frame. It keeps the same identity and a pure
  `#FF00FF` background.
- Dreamina/即梦 `frames2video`: `seedance2.0_vip`, 720p, 5 seconds, using the
  same contact pose as both `--first` and `--last`.
- Submission: `106160d3-2298-4087-ae1b-17dd25b4b266`, successful, 70 credits.
- Raw result: 960×960, 24 fps source, 5.042 seconds. The project extraction
  samples 60 frames at 12 fps and forces extracted frame 0000 and 0059 to the
  same normalized contact pose so the loop seam is exact.

## Split/composite outputs

The sibling processing manifest records the complete review output:
`SourceAssets/AnimationProcessing/walkloop_pilot_v1/character_00_hero_walk_left/manifest.json`.

- 60 RGBA frames, 512×512 cells, bottom-center anchor, 8×8 grid.
- 4K working master: `atlas_4K/character_00_hero_walk_left_atlas.png`.
- Requested 2K atlas: `atlas_2K/character_00_hero_walk_left_atlas.png` (2048²,
  256 px cells).
- Requested 1K atlas: `atlas_1K/character_00_hero_walk_left_atlas.png` (1024²,
  128 px cells).
- Contact sheet for visual review: `contact_sheet.png`.

Magenta is removed with the shared chroma/spill-suppression rules before atlas
packing. The output is review-only; importing it or replacing runtime assets
requires explicit visual approval and a separate UE import/PIE gate.
