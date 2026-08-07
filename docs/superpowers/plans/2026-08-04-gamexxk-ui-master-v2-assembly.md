# GameXXK UI Master V2 Assembly Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild the approved common-component and Hero/Backpack Master pages from the clean V2 parchment base, separately exported controls, final Hero Idle, separate content icons, and editable text without consuming the rejected procedural V1 artwork.

**Architecture:** Extend the V2 calibration package with deterministic placement and content-icon manifests, then let the existing 18-page Master builder route only the approved `00_公共组件` and `03_主角背包` pages through a V2 compositor. All remaining pages stay explicit drafts until they are migrated onto the approved component family. The Master manifest and contact sheet remain the review authority and no UE asset is imported in this phase.

**Tech Stack:** Python 3, Pillow RGBA compositing, JSON manifests/source locks, `unittest`, existing GameXXK Master manifest/Photoshop composer.

---

## File structure

- `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/calibration-spec.json`: source-coordinate crops, component placement and icon extraction contract.
- `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/component-layout.json`: generated placement records for the 28 approved controls.
- `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Content/`: transparent equipment, inventory, HUD, navigation and resource icon layers extracted from the approved high-fidelity V2 source.
- `scripts/build_gamexxk_ui_calibration_v2.py`: deterministic component/icon exporter and V2 report builder.
- `scripts/gamexxk_ui_master_pages.py`: V2 page assembly route for common components and Hero/Backpack.
- `scripts/test_gamexxk_ui_master_v2.py`: V2-only Master source, layer, preview and rejected-source regression tests.
- `SourceArt/UI/PSD/gamexxk-v4/ui-master/Previews/00_公共组件.png`: approved component family page.
- `SourceArt/UI/PSD/gamexxk-v4/ui-master/Previews/03_主角背包.png`: component-assembled Hero/Backpack page.
- `SourceArt/UI/PSD/gamexxk-v4/ui-master/GameXXK_UI_Master_ContactSheet.png`: updated 18-page overview with migrated pages identifiable from their source manifest.

### Task 1: Lock V2 component placements and content layers

**Files:**
- Modify: `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/calibration-spec.json`
- Modify: `scripts/test_gamexxk_ui_calibration_v2.py`
- Modify: `scripts/build_gamexxk_ui_calibration_v2.py`
- Create: `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/component-layout.json`
- Create: `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Content/*.png`

- [ ] **Step 1: Write the failing layout and content-export tests**

Add tests requiring `component-layout.json` to contain 28 unique source-space placements and requiring every exported content icon to be RGBA with transparent corners and an opaque subject center.

- [ ] **Step 2: Run the focused tests and verify the missing manifest/content failure**

Run: `python -m unittest scripts.test_gamexxk_ui_calibration_v2 -v`

Expected: `FAIL` or `ERROR` because placement and content exports do not exist yet.

- [ ] **Step 3: Export placement metadata and separate content icons**

The builder records each component's original source box, trimmed source placement, role, file and pixel size. Icon crops are taken only from `Generated/hero_backpack_textless_base_clean.png`; border-paper colors are converted to alpha and the source crop/placement is recorded without image-generation relabeling.

- [ ] **Step 4: Re-run the calibration suite**

Run: `python -m unittest scripts.test_gamexxk_ui_calibration_v2 -v`

Expected: all calibration tests pass and `component-layout.json` reports 28 controls.

### Task 2: Assemble V2 common-component and Hero/Backpack Master pages

**Files:**
- Create: `scripts/test_gamexxk_ui_master_v2.py`
- Modify: `scripts/gamexxk_ui_master_pages.py`
- Modify: `scripts/build_gamexxk_ui_master.py`
- Modify: `SourceArt/UI/PSD/gamexxk-v4/ui-master/ui-master-spec.json`

- [ ] **Step 1: Write failing Master V2 routing tests**

Require pages `00_公共组件` and `03_主角背包` to consume the clean V2 base and calibration component/content folders, require page 03 to contain 28 separately named control layers, require the exact final Hero Idle with uniform scaling, and reject `ui-master/Assets` or `LayoutAssets` as visual sources for those two pages.

- [ ] **Step 2: Run the focused tests and verify the old procedural route fails**

Run: `python -m unittest scripts.test_gamexxk_ui_master_v2 -v`

Expected: failure because both pages still use procedural Master assets.

- [ ] **Step 3: Implement V2 page compositors**

`00_公共组件` presents the 28 approved controls on the clean parchment. `03_主角背包` composites the clean V2 base, 28 control layers at scaled source placements, separate content icons, final Hero Idle, and editable runtime text. Other pages keep their existing `pending_visual_review` status and remain visibly draft.

- [ ] **Step 4: Rebuild and inspect the two full-HD page previews**

Run: `python scripts/build_gamexxk_ui_master.py`

Expected: `ok: true`, 18 pages, V2 migrated page list `["00_公共组件", "03_主角背包"]`, and both previews at `1920 × 1080`.

- [ ] **Step 5: Run Master and calibration regressions**

Run: `python -m unittest scripts.test_gamexxk_ui_master_v2 scripts.test_gamexxk_ui_master_build scripts.test_gamexxk_ui_master_pages scripts.test_gamexxk_ui_calibration_v2 -v`

Expected: all tests pass.

### Task 3: Produce the reviewable Master deliverables

**Files:**
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Previews/00_公共组件.png`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Previews/03_主角背包.png`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/GameXXK_UI_Master_ContactSheet.png`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/master-manifest.json`
- Modify: `docs/production/2026-08-04-gamexxk-ui-calibration-v2-progress.md`

- [ ] **Step 1: Generate canonical Master outputs**

Run: `python scripts/build_gamexxk_ui_master.py`

Expected: manifest paths resolve, migrated pages list V2 sources, and no dynamic text is baked into reusable runtime components.

- [ ] **Step 2: Inspect the two migrated previews and contact sheet at original resolution**

Check Hero aspect, component alignment, icon separation, text clearances, paper frequency, retained town visibility and the absence of rectangular crop seams.

- [ ] **Step 3: Record status and preserve the UE approval gate**

Update the production record with source paths, output hashes, test counts and `UE import: blocked pending Master V2 visual approval`.

- [ ] **Step 4: Present the common component page, Hero/Backpack page and Master contact sheet for approval**

Do not import or replace UE runtime assets until the user approves these Master V2 outputs.
