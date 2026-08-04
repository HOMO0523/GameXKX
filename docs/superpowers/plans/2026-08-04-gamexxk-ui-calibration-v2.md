# GameXXK UI Calibration V2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce one high-fidelity 1920×1080 Hero/Backpack V2 calibration preview that matches the approved town UI composition and art quality while using the exact final Hero Idle source without distortion.

**Architecture:** Use the approved Hero/Backpack image as the visual and composition authority, and use built-in image generation only to create a text-free, hero-free high-fidelity base for missing reusable art. A deterministic Pillow assembler overlays the exact runtime Hero Idle and editable preview text, validates immutable source hashes and geometry, and outputs a side-by-side review image. The V1 procedural wireframe remains untouched as a rejected regression sample.

**Tech Stack:** Built-in image generation, Python 3, Pillow, JSON source locks, `unittest`, existing Photoshop manifest composer after visual approval.

---

## File structure

- `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/calibration-spec.json`: immutable canvas, layer, source and output contract.
- `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/source-lock.json`: SHA-256 locks for the approved reference and final Hero Idle.
- `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Generated/hero_backpack_textless_base.png`: generated high-fidelity base; no hero and no baked dynamic text.
- `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Previews/GameXXK_HeroBackpack_V2.png`: assembled V2 calibration page.
- `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Review/GameXXK_HeroBackpack_V2_compare.png`: approved reference and V2 side-by-side.
- `scripts/build_gamexxk_ui_calibration_v2.py`: deterministic source validation, compositing and comparison-sheet builder only; it must not draw substitute UI art.
- `scripts/test_gamexxk_ui_calibration_v2.py`: source, geometry and output regression tests.
- `docs/production/2026-08-04-gamexxk-ui-calibration-v2-progress.md`: visual decision record and approval boundary.

### Task 1: Lock the V2 calibration contract

**Files:**
- Create: `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/calibration-spec.json`
- Create: `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/source-lock.json`
- Create: `scripts/test_gamexxk_ui_calibration_v2.py`

- [ ] **Step 1: Write the failing source-lock and canvas tests**

```python
class GameXXKUiCalibrationV2Tests(unittest.TestCase):
    def test_contract_locks_reference_and_final_idle(self) -> None:
        spec = load_json(SPEC)
        lock = load_json(SOURCE_LOCK)
        self.assertEqual({"width": 1920, "height": 1080}, spec["canvas"])
        self.assertEqual("GameXXK_HeroBackpack_V2", spec["candidateName"])
        self.assertEqual(
            "SourceArt/UI/PSD/gamexxk-v4/ui-master/Reference/approved_town_hero_backpack.png",
            lock["approvedReference"]["path"],
        )
        self.assertEqual(
            "SourceAssets/AnimationProcessing/Production/character_00_hero_idle/frames/frame_0000.png",
            lock["heroIdle"]["path"],
        )
        for record in lock.values():
            source = PROJECT_ROOT / record["path"]
            self.assertEqual(record["sha256"], sha256(source))
```

- [ ] **Step 2: Run the focused test and verify it fails**

Run: `python -m unittest scripts.test_gamexxk_ui_calibration_v2.GameXXKUiCalibrationV2Tests.test_contract_locks_reference_and_final_idle -v`

Expected: `ERROR` because the V2 contract files do not exist.

- [ ] **Step 3: Create the immutable contract files**

`calibration-spec.json` must define:

```json
{
  "candidateName": "GameXXK_HeroBackpack_V2",
  "canvas": {"width": 1920, "height": 1080},
  "generatedBase": "Generated/hero_backpack_textless_base.png",
  "preview": "Previews/GameXXK_HeroBackpack_V2.png",
  "comparison": "Review/GameXXK_HeroBackpack_V2_compare.png",
  "heroPlacement": {"x": 430, "y": 265, "width": 510, "height": 510, "fitMode": "contain_canvas"},
  "forbiddenVisualSources": ["gamexxk-v4/ui-master/Assets", "gamexxk-v4/ui-master/LayoutAssets"]
}
```

`source-lock.json` records project-relative paths, image dimensions and current SHA-256 values for both sources.

- [ ] **Step 4: Re-run the source-lock test**

Expected: `OK`.

- [ ] **Step 5: Commit the V2 contract**

```powershell
git add -- SourceArt/UI/PSD/gamexxk-v4/calibration-v2/calibration-spec.json SourceArt/UI/PSD/gamexxk-v4/calibration-v2/source-lock.json scripts/test_gamexxk_ui_calibration_v2.py
git commit -m "test: lock GameXXK UI calibration V2"
```

### Task 2: Generate the high-fidelity text-free base

**Files:**
- Create: `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Generated/hero_backpack_textless_base.png`

- [ ] **Step 1: Use the two inputs with explicit roles**

- Input 1, reference image: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Reference/approved_town_hero_backpack.png`; authority for layout, material, icon quality, palette and town visibility.
- Input 2, supporting identity image: `SourceAssets/AnimationProcessing/Production/character_00_hero_idle/frames/frame_0000.png`; reserves the correct silhouette area but is not rendered into the generated base.

- [ ] **Step 2: Generate one landscape V2 base with this normalized prompt**

```text
Use case: ui-mockup
Asset type: high-fidelity 16:9 game UI calibration base
Primary request: Rebuild the approved Chinese ink-and-paper Hero/Backpack interface at production concept-art fidelity. Preserve its composition, compact information density, visible 3D Chinese town backdrop, warm fibrous rice-paper panels, naturally torn edges, expressive broken ink outlines, colored hand-painted equipment and inventory objects, circular left navigation and top resource strip. Remove the central hero only and reconstruct clean parchment beneath that silhouette so the exact runtime Hero Idle can be composited later. Leave every dynamic Chinese text and number area blank; preserve the controls and layout around those blank areas. Do not simplify anything into wireframes or generic rectangles.
Input images: Image 1 is the strict visual/composition reference; Image 2 is the exact hero silhouette and reserved placement reference only.
Composition/framing: 16:9 landscape, same panel coverage and town visibility as Image 1.
Style/medium: polished hand-painted Chinese ink-and-watercolor game UI, restrained low-saturation materials, detailed object icons.
Constraints: no hero, no duplicate character, no baked text, no fake geometric town background, no random speckle texture, no modern flat UI, no red/blue filled button system, no watermark.
```

- [ ] **Step 3: Inspect the generated image before project use**

Reject the result if it contains a character, malformed pseudo-Chinese text, a fake flat town silhouette, uniform noise speckles, sparse wireframe slots, plastic gradients or large colored button fills. Iterate with one targeted prompt change only.

- [ ] **Step 4: Save the accepted base non-destructively**

Copy the selected built-in output to `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Generated/hero_backpack_textless_base.png`. Do not overwrite the approved reference or V1 files.

### Task 3: Build the deterministic V2 preview

**Files:**
- Create: `scripts/build_gamexxk_ui_calibration_v2.py`
- Modify: `scripts/test_gamexxk_ui_calibration_v2.py`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Previews/GameXXK_HeroBackpack_V2.png`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Review/GameXXK_HeroBackpack_V2_compare.png`

- [ ] **Step 1: Add failing assembly tests**

```python
def test_builder_preserves_hero_canvas_aspect(self) -> None:
    report = run_builder(self.output_root)
    self.assertEqual([1920, 1080], report["canvas"])
    self.assertEqual(1.0, report["heroScaleRatioXToY"])
    self.assertEqual([512, 512], report["heroSourceCanvas"])
    self.assertEqual([1920, 1080], image_size(Path(report["preview"])))

def test_builder_never_reads_rejected_procedural_assets(self) -> None:
    report = run_builder(self.output_root)
    consumed = "\n".join(report["consumedSources"])
    self.assertNotIn("ui-master/Assets", consumed)
    self.assertNotIn("ui-master/LayoutAssets", consumed)
```

- [ ] **Step 2: Run the assembly tests and verify failure**

Run: `python -m unittest scripts.test_gamexxk_ui_calibration_v2 -v`

Expected: `FAIL` or `ERROR` because the builder and V2 preview do not exist.

- [ ] **Step 3: Implement a compositing-only builder**

The builder must:

```python
base = Image.open(generated_base).convert("RGBA")
base = ImageOps.fit(base, (1920, 1080), method=Image.Resampling.LANCZOS)
hero = Image.open(hero_source).convert("RGBA")
scale = min(target_width / hero.width, target_height / hero.height)
hero = hero.resize((round(hero.width * scale), round(hero.height * scale)), Image.Resampling.LANCZOS)
preview.alpha_composite(hero, (hero_x, hero_y))
```

It may add exact preview text through Pillow text layers for review, but it must not generate paper, icons, borders, backgrounds or slots. It records consumed source paths, hashes, hero transform, canvas size and output paths in `package-build-report.json`.

- [ ] **Step 4: Run the builder and inspect its JSON report**

Run: `python scripts/build_gamexxk_ui_calibration_v2.py`

Expected: `ok: true`, canvas `[1920, 1080]`, hero scale ratio `1.0`, and only the approved reference lock, generated V2 base and final Hero Idle in `consumedSources`.

- [ ] **Step 5: Re-run all V2 tests**

Run: `python -m unittest scripts.test_gamexxk_ui_calibration_v2 -v`

Expected: all V2 tests pass.

- [ ] **Step 6: Commit the assembler and generated review files**

```powershell
git add -- scripts/build_gamexxk_ui_calibration_v2.py scripts/test_gamexxk_ui_calibration_v2.py SourceArt/UI/PSD/gamexxk-v4/calibration-v2
git commit -m "feat: build GameXXK UI calibration V2"
```

### Task 4: Visual QA and user approval checkpoint

**Files:**
- Create: `docs/production/2026-08-04-gamexxk-ui-calibration-v2-progress.md`
- Inspect: `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Review/GameXXK_HeroBackpack_V2_compare.png`

- [ ] **Step 1: Review the comparison at original resolution**

Check panel footprint, retained 3D town visibility, paper frequency and edges, ink-line weight variation, colored icon material, slot density, resource-strip detail, circular navigation readability, Hero identity, Hero aspect ratio and foot placement.

- [ ] **Step 2: Record structural and visual status separately**

The progress record must list source hashes, generated paths, test commands and results. It must state `UE import: blocked pending user visual approval` and must not call structural test success a visual approval.

- [ ] **Step 3: Present only the V2 preview and comparison for approval**

Do not build the 18-screen contact sheet, PSD component library or UE runtime assets until the user approves this calibration page.

