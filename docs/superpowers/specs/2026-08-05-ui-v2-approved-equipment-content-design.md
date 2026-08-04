# GameXXK UI V2 Approved Equipment and Content Design

Date: 2026-08-05

## Outcome

Replace the obsolete equipment and item crops in the UI V2 calibration package with the user-approved regenerated art, export deterministic transparent runtime-ready assets, and continue polishing the Hero/Backpack V2 composition without changing maps, cameras, runtime widgets, or user-tuned UE assets.

The source package is complete when it contains exactly 45 approved content assets:

- 36 set equipment icons: six sets by six slots.
- 6 ordinary starter equipment icons: one per slot.
- 3 core item icons: strengthening stone, refinement sand, and Qingshan suppression token.

## Approved Inputs

The following eight contact sheets are authoritative for this work. The user explicitly approved all six regenerated set sheets even where the filename does not contain `_approved`.

| Group | Approved source |
|---|---|
| Six set weapons | `Generated/equipment/sixset_weapon_calibration_v1.png` |
| Six set head items | `Generated/equipment/sixset_head_calibration_v1.png` |
| Six set armor items | `Generated/equipment/sixset_armor_calibration_v1.png` |
| Six set belts | `Generated/equipment/sixset_belt_calibration_v5_approved.png` |
| Six set shoes | `Generated/equipment/sixset_shoes_calibration_v1.png` |
| Six set accessories | `Generated/equipment/sixset_accessory_calibration_v1.png` |
| Ordinary starter equipment | `Generated/items/starter_sixslot_equipment_calibration_v1_approved.png` |
| Core items | `Generated/items/core_resources_calibration_v2_approved.png` |

Excluded inputs:

- `backpack_core_items_calibration_v1_approved.png`; its seven objects are not the requested core-item set.
- `legacy_compat_equipment_calibration_v1.png`; it is not part of this visual package.
- Equipment and item pixels baked into `hero_backpack_textless_base_clean.png`; those are obsolete composition references, not content sources.
- The July Draft B02 alpha set; it remains historical draft material and must not override the newer approved calibration sheets.

All approved source paths, dimensions, and SHA-256 values will be recorded in `source-lock.json`. A missing or changed source must fail the build before outputs are replaced.

## Stable Identity and Order

Every six-set sheet is read left to right in this fixed order:

1. `pojun`
2. `xuanjia`
3. `qingnang`
4. `zhuifeng`
5. `shigu`
6. `shanhe`

Slots use the stable names `weapon`, `head`, `armor`, `belt`, `shoes`, and `accessory`. Set output names therefore follow `<set>_<slot>.png` and total 36 unique files.

The starter sheet is read left to right as `weapon`, `head`, `armor`, `belt`, `shoes`, and `accessory`, producing `starter_<slot>.png`.

The core-item sheet is read left to right as:

1. `strengthening_stone.png` — 强化石
2. `refinement_sand.png` — 洗炼砂
3. `qingshan_suppression_token.png` — 青山讨伐令

## Extraction Pipeline

The builder will split each approved contact sheet into evenly spaced horizontal cells using the expected item count. It will then:

1. Remove the magenta chroma background with a soft alpha edge and despill.
2. Find the complete visible subject within the cell without deleting legitimate detached sub-parts.
3. Preserve aspect ratio and center the result on a transparent `512 x 512` RGBA canvas.
4. Keep a transparent safety margin around every output.
5. Write deterministic PNGs under the calibration package's `Content/Equipment`, `Content/StarterEquipment`, and `Content/Items` directories.

The pipeline will not crop from the old composite UI, stretch objects non-uniformly, bake rarity frames or text into icons, or silently fall back to older assets.

## UI V2 Composition Optimization

After extraction passes, the builder will update the Hero/Backpack V2 source composition:

- The six equipped slots use the six ordinary starter equipment icons.
- Backpack content uses only the three approved core items in the first available item cells; unused cells remain empty.
- The detail area uses one of the three core items and matching generated preview text.
- Old sword, hat, robe, belt, shoes, accessory, bag, crystal, jade, and detail-bag pixels are no longer exported from the baked composite.
- The 36 set equipment icons remain available in the content manifest for later inventory, equipment, loot, and shop screens; they are not forced into the single Hero/Backpack preview.
- Existing approved town framing, clean background, large paper panel, Hero proportions, navigation icons, control geometry, and 16:9 layout remain unchanged unless a deterministic recomposition defect requires a local adjustment.

This phase produces source assets, manifests, review images, and the V2 calibration preview. UE import and runtime UMG binding follow only after these outputs pass automated and visual verification.

## Manifests and Reports

`content-manifest.json` will become the authoritative inventory for the 45 new assets and will record, at minimum:

- stable name and category;
- source sheet and cell index;
- output path and dimensions;
- display role in the V2 preview, when applicable;
- source placement for preview recomposition, when applicable.

`package-build-report.json` will record the locked inputs, all 45 outputs, the refreshed preview and comparison images, and a successful validation summary. The progress document will report the new total and must not claim that UE import or runtime binding has occurred.

## Failure Handling

The build fails without replacing the canonical report when:

- a locked source is missing or its hash/dimensions drift;
- a sheet does not yield the expected number of non-empty cells;
- an output has visible pixels on the canvas edge;
- chroma-colored pixels remain above the visible-alpha threshold;
- an output name is missing or duplicated;
- the manifest does not contain exactly 36 set equipment, 6 starter equipment, and 3 core-item records.

## Verification

Tests will first be updated to express the approved 45-asset contract and observed failing behavior from the old composite-crop pipeline. Implementation then proceeds through the existing red/green cycle.

Required automated checks:

- exact source-lock membership and hashes;
- exact stable identity/order for all 45 outputs;
- `512 x 512`, RGBA, transparent canvas edges, non-empty alpha, and preserved aspect ratio;
- no visible magenta spill;
- exact manifest category counts of `36 + 6 + 3`;
- preview uses starter equipment and only the three approved core items;
- old composite equipment/item crop names are absent;
- full `scripts.test_gamexxk_ui_calibration_v2` suite passes;
- canonical builder returns `ok: true`;
- `git diff --check` passes.

Visual review will compare the refreshed Hero/Backpack V2 preview with the approved framing and inspect a generated contact sheet of all 45 transparent outputs. Visual review validates composition and identity; automated checks validate deterministic file contracts.

## Non-Goals

- No new image generation in this phase.
- No use of UnrealBridge.
- No UE map, camera, HD2D, PaperZD, WBP, or runtime C++ changes.
- No UE import before source-package verification.
- No replacement or deletion of historical drafts or user-tuned assets.
