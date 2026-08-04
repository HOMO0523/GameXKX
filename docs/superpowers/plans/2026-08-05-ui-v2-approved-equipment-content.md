# UI V2 Approved Equipment Content Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. This art workflow intentionally does not use TDD, per the project `AGENTS.md` rule.

**Goal:** Split the eight approved contact sheets into 45 transparent assets and rebuild the Hero/Backpack UI V2 package with the approved starter equipment and three core items.

**Architecture:** Keep source identity and ordering in `calibration-spec.json`, hash-lock every approved contact sheet in `source-lock.json`, and let the existing Pillow builder own deterministic chroma removal, splitting, normalization, manifest generation, review sheets, and preview composition. Historical source art stays untouched; outputs are written only under the calibration V2 package.

**Tech Stack:** Python 3, Pillow, JSON manifests, `unittest` for post-implementation regression verification.

---

## File Structure

- Modify `AGENTS.md`: persist the user rule that pure art work does not use TDD.
- Modify `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/source-lock.json`: add all eight approved sheets with exact hashes and dimensions.
- Modify `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/calibration-spec.json`: define stable set/slot/item ordering, output directories, preview assignments, and remove obsolete baked equipment/item crop contracts.
- Modify `scripts/build_gamexxk_ui_calibration_v2.py`: add chroma extraction, 45-asset export, contact-sheet generation, manifest generation, and V2 recomposition.
- Modify `scripts/test_gamexxk_ui_calibration_v2.py`: replace obsolete 11-crop assertions with post-implementation checks for the approved 45-asset contract.
- Modify `docs/production/2026-08-04-gamexxk-ui-calibration-v2-progress.md`: record the approved inputs, 45 outputs, verification, and the fact that UE import remains a later step.
- Generate `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Content/Equipment/*.png`: 36 set equipment assets.
- Generate `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Content/StarterEquipment/*.png`: 6 starter assets.
- Generate `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Content/Items/*.png`: 3 core-item assets.
- Generate `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Review/GameXXK_UI_V2_ApprovedContent_45.png`: visual review sheet.
- Regenerate the canonical preview, comparison, content manifest, and package build report.

### Task 1: Persist approved source identities

- [ ] Update `AGENTS.md` with the art-workflow verification rule.
- [ ] Add these exact lock records to `source-lock.json`:

```text
sixSetWeapon      a08de46777b587e8ea4ca79c673fb30417320186df75181d6d68c053c110e574  1672x941
sixSetHead        2dbbe44bbfc800986b3f241c161007cec68a5a3cd5fdbf3cfbdbe43a50fc68df  1672x941
sixSetArmor       a08590835284d8510407df5c04b16ad2fee721979ff1ac11f1d8b9a40174475f  1846x852
sixSetBelt        cb6c64a2457a7eef4f09e4bcd83cafd712ef65e37a95f903f6f2fb4d0bebe459  1935x813
sixSetShoes       df44c11974f2171a1eab45193a9423fc8f9d2cef0481e2ec81b8ede2bff4255a  1844x853
sixSetAccessory   bca1ef4ac6cf2b38495085d879e1d56b65f29a3ffd4a0edf87efe465f032ac96  1820x864
starterEquipment  3bf74ae54ff2465e03b06337bb4b0da219ceb1d5f3425342e3186b3f618d858b  1983x793
coreItems         5f8ce0cc7024e94fb29a962ac25158e6e741efb2ef37b4dd13fae155e49d5395  1920x819
```

- [ ] Define in `calibration-spec.json`:

```json
{
  "equipmentSetOrder": ["pojun", "xuanjia", "qingnang", "zhuifeng", "shigu", "shanhe"],
  "equipmentSlotOrder": ["weapon", "head", "armor", "belt", "shoes", "accessory"],
  "coreItemOrder": ["strengthening_stone", "refinement_sand", "qingshan_suppression_token"],
  "approvedContentCanvas": [512, 512],
  "approvedContentSubjectFill": 0.88,
  "approvedContentReview": "Review/GameXXK_UI_V2_ApprovedContent_45.png"
}
```

- [ ] Remove the six old `equipment_icon`, four old `inventory_icon`, and old `detail_icon` crop records from `contentCrops`; retain navigation, resource HUD, stat, selection, and portrait records.
- [ ] Run JSON parsing for the spec and source lock; expected result is valid JSON with eight new lock records.
- [ ] Commit only the rule/spec/lock changes with `git commit -m "art: lock approved UI V2 equipment sources"`.

### Task 2: Export transparent approved content

- [ ] Add a focused helper in `scripts/build_gamexxk_ui_calibration_v2.py` that converts magenta contact-sheet pixels to soft alpha while despilling red/blue contamination from edge pixels. It must preserve multiple legitimate detached parts inside one cell.
- [ ] Add a sheet splitter that takes a locked source, ordered names, output directory, `512 x 512` canvas, and `0.88` subject fill. The splitter divides the sheet evenly left to right, applies chroma removal, crops the total alpha bounds, preserves aspect ratio, and centers the subject.
- [ ] Export the six equipment sheets by slot. For each slot, zip the six cells with `equipmentSetOrder` and write `Content/Equipment/<set>_<slot>.png`.
- [ ] Export the starter sheet left to right as `Content/StarterEquipment/starter_<slot>.png`.
- [ ] Export the three core items left to right as:

```text
Content/Items/strengthening_stone.png
Content/Items/refinement_sand.png
Content/Items/qingshan_suppression_token.png
```

- [ ] Reject an empty cell, visible alpha on any canvas edge, duplicate output name, or unexpected output count before writing a successful report.
- [ ] Run `python scripts/build_gamexxk_ui_calibration_v2.py` and inspect that it reports exactly 45 approved assets.
- [ ] Commit builder and generated transparent assets with `git commit -m "art: split approved UI V2 equipment content"`.

### Task 3: Recompose and optimize Hero/Backpack V2

- [ ] Extend the content manifest with three categories and exact counts:

```json
{
  "set_equipment": 36,
  "starter_equipment": 6,
  "core_item": 3
}
```

- [ ] Assign the six starter assets to the existing six equipment-slot placements. Scale each with aspect preservation inside its corresponding separated slot asset.
- [ ] Assign strengthening stone, refinement sand, and Qingshan suppression token to the first three backpack cells. Leave the remaining cells empty.
- [ ] Use Qingshan suppression token in the detail slot and update preview-only text to `青山讨伐令` with a short player-facing description; do not bake dynamic runtime quantities into the asset.
- [ ] Remove old baked-equipment/item exports from `content-manifest.json` and `package-build-report.json`.
- [ ] Build a `1920 x 1080` review sheet containing all 45 transparent outputs, grouped as 36 set equipment, 6 starter equipment, and 3 core items.
- [ ] Regenerate `GameXXK_HeroBackpack_V2.png` and its comparison image while preserving town framing, large paper panel, Hero canvas ratio, navigation placement, and separated control geometry.
- [ ] Visually inspect the preview and 45-asset sheet for wrong ordering, clipping, magenta edges, oversized items, and old-art leakage.
- [ ] Commit composition/report changes with `git commit -m "art: optimize Hero Backpack UI V2 content"`.

### Task 4: Post-implementation verification and progress record

- [ ] Update `scripts/test_gamexxk_ui_calibration_v2.py` to verify final outputs rather than drive implementation. Required assertions are exact 45-name membership, category counts `36/6/3`, `512 x 512` RGBA, transparent canvas edges, non-empty alpha, no visible magenta pixels, and absence of old composite crop names.
- [ ] Run:

```powershell
python -m unittest scripts.test_gamexxk_ui_calibration_v2 -v
python scripts/build_gamexxk_ui_calibration_v2.py
python scripts/harness_state_validator.py
git diff --check
```

Expected: all tests pass; builder reports `ok: true` and 45 approved assets; harness validator prints `OK`; `git diff --check` reports no whitespace errors.

- [ ] Update the progress document with approved sources, exact output counts, verification results, and links to the preview/review sheet. State explicitly that UE import and WBP binding have not yet occurred.
- [ ] Review `git status --short` and stage only files owned by this UI V2 unit. Do not stage unrelated generated art, animation production folders, or user-tuned UE assets.
- [ ] Commit the final verification/docs changes with `git commit -m "docs: verify approved UI V2 content package"`.
