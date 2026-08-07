# GameXXK Hero/Backpack PSD Workflow Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce the first non-destructive, layered, editable 1920×1080 Hero/Backpack PSD candidate using the exact character Idle frame currently shown by the battle HUD, plus previews, manifests, runtime slices, and machine-readable validation.

**Architecture:** Extend the existing project-local PSD composer so one screen package can declare layer groups and deterministic image scaling without changing the canonical `town-v2` package. A Python package builder assembles the Hero/Backpack candidate from approved existing ink assets and `character_00_hero_idle/frames/frame_0000.png`; Photoshop then writes and round-trips the layered PSD. Candidate output lives under `outputs/UI_PSD/Candidates/` and never overwrites `GameXXK_Town_4K.psd`.

**Tech Stack:** Python 3 + Pillow, Node.js, Photoshop JSX/COM automation, JSON manifests, `unittest`.

---

### Task 1: Lock the screen contract and final Idle source

**Files:**
- Create: `SourceArt/UI/PSD/gamexxk-v3/hero-backpack/screen-spec.json`
- Create: `SourceArt/UI/PSD/gamexxk-v3/hero-backpack/source-lock.json`
- Test: `scripts/test_hero_backpack_psd_package.py`

- [ ] **Step 1: Write the failing source-lock test**

```python
def test_source_lock_uses_runtime_idle_frame(self) -> None:
    source_lock = json.loads(SOURCE_LOCK.read_text(encoding="utf-8"))
    self.assertEqual(
        "SourceAssets/AnimationProcessing/Production/character_00_hero_idle/frames/frame_0000.png",
        source_lock["heroIdleSource"],
    )
    self.assertEqual((512, 512), image_size(PROJECT_ROOT / source_lock["heroIdleSource"]))
    self.assertNotIn("PartyDeck/card-portraits", json.dumps(source_lock))
```

- [ ] **Step 2: Run the focused test and confirm it fails**

Run: `python -m unittest scripts.test_hero_backpack_psd_package.HeroBackpackPsdPackageTests.test_source_lock_uses_runtime_idle_frame -v`

Expected: `ERROR` because `source-lock.json` does not exist.

- [ ] **Step 3: Add the 1920×1080 contract and source hash**

`screen-spec.json` must define this immutable first-candidate contract:

```json
{
  "screenId": "town.hero_backpack",
  "canvas": {"width": 1920, "height": 1080, "resolution": 72},
  "safeArea": {"left": 48, "top": 40, "right": 48, "bottom": 40},
  "reference": "Reference/approved_hero_backpack_reference.png",
  "outputPsd": "outputs/UI_PSD/Candidates/GameXXK_HeroBackpack_V1.psd",
  "requiredGroups": ["00_Reference", "10_WorldContext", "20_Shell", "30_Hero", "40_Equipment", "50_Inventory", "60_Detail", "70_RuntimeText"]
}
```

`source-lock.json` records the project-relative source path, SHA-256, source dimensions, UE atlas name `T_character_00_hero_idle_atlas`, and retired roots `SourceAssets/PartyDeck/card-portraits/generated`.

- [ ] **Step 4: Re-run the focused test**

Expected: `OK`.

### Task 2: Add group and scale support to the reusable PSD composer

**Files:**
- Modify: `scripts/ui_psd_pipeline/build-psd.js`
- Modify: `scripts/test_town_psd_package.py`

- [ ] **Step 1: Write failing composer tests**

Add a manifest image layer with `group`, `width`, and `height`, run the composer, and assert the generated JSX contains:

```javascript
ensureGroup(doc, item.group)
resizeLayerTo(duplicated, item.width, item.height)
```

- [ ] **Step 2: Run the composer tests and confirm failure**

Run: `python -m unittest scripts.test_town_psd_package.TownPsdPackageTest.test_composer_supports_grouped_scaled_layers -v`

Expected: `FAIL` because the current composer imports flat, native-sized image layers.

- [ ] **Step 3: Implement minimal backward-compatible JSX generation**

The generated JSX will create named `LayerSet` objects only when `item.group` is present and resize only when both dimensions are positive. PSD round-trip inspection must traverse nested groups recursively when counting text and image layers. Manifests without these fields retain the current `town-v2` behavior.

- [ ] **Step 4: Run all existing town PSD tests**

Run: `python -m unittest scripts.test_town_psd_package scripts.test_town_psd_image_ops -v`

Expected: all tests pass.

### Task 3: Build the Hero/Backpack candidate package

**Files:**
- Create: `scripts/build_hero_backpack_psd_package.py`
- Create: `scripts/test_hero_backpack_psd_package.py`
- Generate: `SourceArt/UI/PSD/gamexxk-v3/hero-backpack/manifest.json`
- Generate: `SourceArt/UI/PSD/gamexxk-v3/hero-backpack/semantic-map.json`
- Generate: `SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/*.png`
- Generate: `SourceArt/UI/PSD/gamexxk-v3/hero-backpack/RuntimeAssets/*.png`
- Generate: `SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Previews/GameXXK_HeroBackpack_V1.png`

- [ ] **Step 1: Write failing package-structure tests**

The tests require the eight named groups, editable text for the title/tabs/resource values/item detail, the correct Idle hash, six equipment slots, twenty inventory slots, three semantic button families, and no runtime text baked into exported slices.

- [ ] **Step 2: Run the package tests and confirm failure**

Run: `python -m unittest scripts.test_hero_backpack_psd_package -v`

Expected: failure because the builder and generated package are absent.

- [ ] **Step 3: Implement deterministic package construction**

The builder will:

```python
CANVAS = (1920, 1080)
HERO_SOURCE = PROJECT_ROOT / "SourceAssets/AnimationProcessing/Production/character_00_hero_idle/frames/frame_0000.png"
OUTPUT_ROOT = PROJECT_ROOT / "SourceArt/UI/PSD/gamexxk-v3/hero-backpack"
```

It reuses approved `town-v2/clean_assets` for tabs, slots, bars, buttons, and inventory icons; composites preview-only backgrounds with Pillow; emits one image layer per editable visual atom; and keeps all Chinese labels in `textLayers`.

- [ ] **Step 4: Generate the package**

Run: `python scripts/build_hero_backpack_psd_package.py`

Expected: JSON reports `ok: true`, canvas `1920x1080`, hero source ending in `character_00_hero_idle/frames/frame_0000.png`, and candidate PSD path under `outputs/UI_PSD/Candidates/`.

- [ ] **Step 5: Re-run the package tests**

Expected: all tests pass.

### Task 4: Compose and validate the layered PSD

**Files:**
- Generate: `SourceArt/UI/PSD/gamexxk-v3/hero-backpack/compose.jsx`
- Generate: `outputs/UI_PSD/Candidates/GameXXK_HeroBackpack_V1.psd`
- Generate: `outputs/UI_PSD/Candidates/GameXXK_HeroBackpack_V1.validation.json`
- Create: `scripts/validate_hero_backpack_psd_candidate.py`
- Extend: `scripts/test_hero_backpack_psd_package.py`

- [ ] **Step 1: Generate JSX and SVG text mirrors**

Run: `node scripts/ui_psd_pipeline/build-psd.js --root SourceArt/UI/PSD/gamexxk-v3/hero-backpack`

Expected: JSON reports the image-layer count, editable-text count, JSX path, and candidate PSD path.

- [ ] **Step 2: Run Photoshop composition**

Run: `powershell -ExecutionPolicy Bypass -File scripts/ui_psd_pipeline/run-photoshop.ps1 -Root SourceArt/UI/PSD/gamexxk-v3/hero-backpack`

Expected: `Photoshop composition finished.`

- [ ] **Step 3: Validate PSD round trip**

Run: `python scripts/validate_hero_backpack_psd_candidate.py --root SourceArt/UI/PSD/gamexxk-v3/hero-backpack`

Expected: `ok: true`; width `1920`; height `1080`; every required group present; actual editable text count equals expected count; source lock matches.

### Task 5: Visual QA checkpoint

**Files:**
- Create: `docs/production/2026-08-04-gamexxk-ui-psd-v3-progress.md`
- Inspect: `SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Previews/GameXXK_HeroBackpack_V1.png`

- [ ] **Step 1: Inspect the preview at original resolution**

Check character identity, 3/4-left facing, unclipped silhouette, equipment-slot alignment, inventory density, item-detail legibility, retained 3D-town visibility, and visual consistency with final monster/character ink art.

- [ ] **Step 2: Record the review boundary**

The progress record states that the first PSD is a candidate, lists exact source hashes and generated files, records structural validation separately from visual approval, and keeps UE import explicitly pending until the user approves the preview.

- [ ] **Step 3: Present the preview for user review**

Do not import into UE or replace runtime textures at this checkpoint.
