# Progressive Gem Item Icons Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce, validate, import, and expose thirty transparent Q-version ink-cartoon gem item icons whose shape complexity progresses visibly across ten qualities.

**Architecture:** Image generation produces three anchored evolution chains under `SourceArt`; a deterministic Pillow processor owns normalization, alpha checks, hashes, manifest generation, and the 3×10 review sheet. Focused Unreal Python scripts import only the declared thirty textures and validate their runtime settings. C++ item-ID and icon-path mapping remains in the separate Equipment/Gems/Tools Task 3 so art generation and gameplay rules stay independently reviewable.

**Tech Stack:** built-in `image_gen`, Python 3.12, Pillow, JSON/SHA-256, Unreal Engine 5.8 Python API, project UE MCP scripts.

---

Prerequisites:

- Approved spec: `docs/superpowers/specs/2026-08-24-gem-icon-progression-design.md`.
- Style reference: `SourceArt/Generated/Draft/V1/DRAFT-B02-EQUIPMENT_checkpoint31_contact_sheet.png`.
- Work on root `main`; do not create a worktree and do not use UnrealBridge or Luna.

### Task 1: add the deterministic gem-icon asset processor

**Files:**
- Create: `scripts/process_gem_icons.py`
- Create: `scripts/test_gem_icon_assets.py`
- Create directory: `SourceArt/UI/Items/Gems/generated/`
- Create directory: `SourceArt/UI/Items/Gems/final/`
- Create directory: `SourceArt/UI/Items/Gems/review/`

Pure art processing does not use TDD. The script tests are deterministic contract checks and may initially report missing inputs until Task 5 has generated all thirty assets.

- [ ] **Step 1: define the exact asset matrix**

Use these immutable axes:

```python
GEM_TYPES = ("Attack", "Defense", "MaxHealth")
QUALITIES = (
    "Common", "Rare", "Epic", "Legendary", "Immortal",
    "Treasure", "Transcendent", "Celestial", "Ascendant", "Cosmic",
)

def stem(gem_type: str, quality: str) -> str:
    return f"T_Item_Gem_{gem_type}_{quality}"

def item_id(gem_type: str, quality: str) -> str:
    return f"Item.Gem.{gem_type}.{quality}"

def texture_path(gem_type: str, quality: str) -> str:
    name = stem(gem_type, quality)
    return f"/Game/GameXXK/UI/Items/Gems/{name}.{name}"
```

- [ ] **Step 2: implement genuine-alpha normalization**

`scripts/process_gem_icons.py` reads accepted generator outputs from `SourceArt/UI/Items/Gems/generated`, requires an alpha channel, rejects a fully opaque canvas, rejects any non-transparent corner, crops to the alpha bounding box, scales the longest visible edge to 75% of a 512×512 canvas, and writes RGBA PNGs to `final`.

```python
def normalize_icon(source: Path, destination: Path) -> dict[str, object]:
    image = Image.open(source).convert("RGBA")
    alpha = image.getchannel("A")
    if alpha.getextrema()[0] == 255:
        raise RuntimeError(f"{source.name} has no transparent pixels")
    bbox = alpha.point(lambda value: 255 if value >= 16 else 0).getbbox()
    if bbox is None:
        raise RuntimeError(f"{source.name} is fully transparent")
    if max(image.getpixel(point)[3] for point in CORNERS) != 0:
        raise RuntimeError(f"{source.name} has an opaque canvas corner")
    # Crop, resize with LANCZOS, alpha-composite at center, save 512x512 RGBA.
```

Do not chroma-key or infer transparency from a checkerboard. A baked checkerboard is a hard failure and must be regenerated through built-in image generation.

- [ ] **Step 3: build the manifest and review sheet**

For each icon record:

```json
{
  "item_id": "Item.Gem.Attack.Common",
  "source_png": "SourceArt/UI/Items/Gems/final/T_Item_Gem_Attack_Common.png",
  "texture_path": "/Game/GameXXK/UI/Items/Gems/T_Item_Gem_Attack_Common.T_Item_Gem_Attack_Common",
  "size": [512, 512],
  "mode": "RGBA",
  "alpha_bbox": [64, 64, 448, 448],
  "transparent_border_ratio": 1.0,
  "sha256": "64 lowercase hexadecimal characters computed from the final PNG bytes"
}
```

The actual values are computed, never copied from this example. Write the thirty records to `SourceArt/UI/Items/Gems/gem_icon_manifest.json`.

Build `SourceArt/UI/Items/Gems/review/gem-quality-progression-contact-sheet.png` as a 3×10 matrix. Each cell contains a 160×160 preview plus a 48×48 inset on the approved paper-neutral review background. Rows are Attack, Defense, MaxHealth; columns are Common through Cosmic.

- [ ] **Step 4: add deterministic tests**

`scripts/test_gem_icon_assets.py` imports the processor and verifies the exact 3×10 matrix, stable naming, exact ItemID/texture paths, 512×512 RGBA output, transparent border, non-empty alpha bounds, and thirty unique SHA-256 values.

Run after Task 5:

```powershell
python scripts/process_gem_icons.py
python -m unittest scripts.test_gem_icon_assets -v
```

Expected: processor prints `{"ok": true, "icon_count": 30, ...}` and all tests pass.

- [ ] **Step 5: commit the processor**

```powershell
git add scripts/process_gem_icons.py scripts/test_gem_icon_assets.py
git commit -m "test: add deterministic gem icon asset checks"
```

Do not commit empty generated/final/review directories.

### Task 2: generate the three Common silhouette masters

**Files:**
- Create: `SourceArt/UI/Items/Gems/generated/T_Item_Gem_Attack_Common.png`
- Create: `SourceArt/UI/Items/Gems/generated/T_Item_Gem_Defense_Common.png`
- Create: `SourceArt/UI/Items/Gems/generated/T_Item_Gem_MaxHealth_Common.png`

- [ ] **Step 1: generate the Attack Common master**

Use built-in `image_gen` with the equipment contact sheet as a style reference and this exact production prompt:

```text
Use case: stylized-concept
Asset type: GameXXK square inventory item icon
Primary request: a Common-rank Attack gem, a rough compact arrowhead-shaped red gemstone
Input image: equipment contact sheet is style reference only
Style/medium: Q-version Chinese ink-cartoon equipment icon, confident dark ink outline, hand-painted material shading
Composition/framing: one centered object, consistent three-quarter product-icon camera, subject around 75% of a square canvas
Color palette: vermilion red gemstone, muted charcoal crevices, restrained warm highlight
Constraints: genuine transparent background and preserved alpha; simplest rank-one silhouette; no bezel, rune, halo, particles, frame, text, number, logo, watermark, checkerboard, or shadow panel
```

Copy the accepted output into the exact generated path. Reject and regenerate if the alpha is baked or the silhouette is not immediately sharp/aggressive.

- [ ] **Step 2: generate the Defense Common master**

Use one separate built-in `image_gen` call:

```text
Use case: stylized-concept
Asset type: GameXXK square inventory item icon
Primary request: a Common-rank Defense gem, a rough compact hexagonal shield-face blue gemstone
Input image: equipment contact sheet is style reference only
Style/medium: Q-version Chinese ink-cartoon equipment icon, confident dark ink outline, hand-painted material shading
Composition/framing: one centered object, consistent three-quarter product-icon camera, subject around 75% of a square canvas
Color palette: indigo blue gemstone, muted charcoal crevices, restrained cool highlight
Constraints: genuine transparent background and preserved alpha; simplest rank-one silhouette; no bezel, rune, halo, particles, frame, text, number, logo, watermark, checkerboard, or shadow panel
```

- [ ] **Step 3: generate the Max Health Common master**

Use one separate built-in `image_gen` call:

```text
Use case: stylized-concept
Asset type: GameXXK square inventory item icon
Primary request: a Common-rank Max Health gem, a rough compact rounded jade seed / jade droplet green gemstone
Input image: equipment contact sheet is style reference only
Style/medium: Q-version Chinese ink-cartoon equipment icon, confident dark ink outline, hand-painted material shading
Composition/framing: one centered object, consistent three-quarter product-icon camera, subject around 75% of a square canvas
Color palette: jade green gemstone, muted charcoal crevices, restrained pale highlight
Constraints: genuine transparent background and preserved alpha; simplest rank-one silhouette; no bezel, rune, halo, particles, frame, text, number, logo, watermark, checkerboard, or shadow panel
```

- [ ] **Step 4: inspect all three masters**

Use direct image inspection. Confirm the three silhouettes remain distinguishable in a 48×48 preview, share outline weight/camera/lighting, and contain real transparent corners. Do not use Luna.

### Task 3: derive the Attack quality chain

**Files:**
- Create: `SourceArt/UI/Items/Gems/generated/T_Item_Gem_Attack_<Rare..Cosmic>.png`

For each rank, issue a separate built-in image edit call. Reference the accepted Attack Common master as the silhouette anchor and the immediately previous accepted rank as the edit target.

Every call repeats these invariants:

```text
Preserve exactly: Attack arrowhead core silhouette, vermilion type color, camera, scale, lighting direction, dark ink outline, Q-version ink-cartoon equipment style, transparent background.
Change only: the requested next quality step.
Avoid: type drift, blue/green main color, UI frame, text, number, watermark, baked checkerboard, extra unrelated objects, full-scene background.
```

- [ ] **Step 1: Rare** — polish the raw surface and add the first clean facets; add no external ornament.
- [ ] **Step 2: Epic** — add precise multi-facet cutting; retain a bare gemstone with no bezel.
- [ ] **Step 3: Legendary** — add a small dark-metal bezel and one restrained engraved rune.
- [ ] **Step 4: Immortal** — complete the warm-gold setting and strengthen the rune work without adding particles.
- [ ] **Step 5: Treasure** — add one restrained orbit of small floating gemstone shards.
- [ ] **Step 6: Transcendent** — add a compact red-black ink-energy halo behind the established form.
- [ ] **Step 7: Celestial** — add a celestial ring and subtle cloud-pattern ornament.
- [ ] **Step 8: Ascendant** — add a second halo, richer crown structure, and a short energy trail.
- [ ] **Step 9: Cosmic** — add a star-cloud core, tiny constellation detail, and final crown; keep the arrowhead readable at 48×48.

After each call, inspect alpha and compare it with both prior ranks. Reject any result whose complexity is not visibly stronger than the predecessor or whose core silhouette changes.

### Task 4: derive the Defense quality chain

**Files:**
- Create: `SourceArt/UI/Items/Gems/generated/T_Item_Gem_Defense_<Rare..Cosmic>.png`

Use one built-in edit call per rank. Reference Defense Common plus the previous accepted rank.

Every call repeats:

```text
Preserve exactly: Defense hexagonal shield-face core silhouette, indigo type color, camera, scale, lighting direction, dark ink outline, Q-version ink-cartoon equipment style, transparent background.
Change only: the requested next quality step.
Avoid: sharp Attack arrowhead drift, green health-droplet drift, UI frame, text, number, watermark, checkerboard, unrelated objects, full-scene background.
```

- [ ] **Step 1: Rare** — polish the shield stone and add first clean facets; no external ornament.
- [ ] **Step 2: Epic** — add precise reinforced multi-facets; remain an unframed gemstone.
- [ ] **Step 3: Legendary** — add a small dark-metal shield bezel and one defensive rune.
- [ ] **Step 4: Immortal** — complete a warm-gold reinforced setting and stronger ward runes.
- [ ] **Step 5: Treasure** — add one restrained orbit of small shield-like fragments.
- [ ] **Step 6: Transcendent** — add a compact indigo-black ink-energy halo.
- [ ] **Step 7: Celestial** — add a celestial ring and subtle cloud-pattern ward.
- [ ] **Step 8: Ascendant** — add a second halo, crown-like upper guard, and short energy trail.
- [ ] **Step 9: Cosmic** — add a star-cloud core, constellation ward, and final crown while preserving the hexagonal shield at 48×48.

### Task 5: derive the Max Health quality chain and finalize all PNGs

**Files:**
- Create: `SourceArt/UI/Items/Gems/generated/T_Item_Gem_MaxHealth_<Rare..Cosmic>.png`
- Create: `SourceArt/UI/Items/Gems/final/*.png`
- Create: `SourceArt/UI/Items/Gems/gem_icon_manifest.json`
- Create: `SourceArt/UI/Items/Gems/review/gem-quality-progression-contact-sheet.png`

Use one built-in edit call per rank. Reference MaxHealth Common plus the previous accepted rank.

Every call repeats:

```text
Preserve exactly: Max Health rounded jade seed / jade droplet core silhouette, jade-green type color, camera, scale, lighting direction, dark ink outline, Q-version ink-cartoon equipment style, transparent background.
Change only: the requested next quality step.
Avoid: Attack arrowhead drift, Defense shield drift, UI frame, text, number, watermark, checkerboard, unrelated objects, full-scene background.
```

- [ ] **Step 1: Rare** — polish the jade seed and add first clean facets; no external ornament.
- [ ] **Step 2: Epic** — add precise rounded multi-facets; remain an unframed gemstone.
- [ ] **Step 3: Legendary** — add a small dark-metal cradle and one vitality rune.
- [ ] **Step 4: Immortal** — complete a warm-gold setting with stronger life-vein runes.
- [ ] **Step 5: Treasure** — add one restrained orbit of seed-like jade fragments.
- [ ] **Step 6: Transcendent** — add a compact jade-black ink-energy halo.
- [ ] **Step 7: Celestial** — add a celestial ring and subtle cloud/leaf ornament.
- [ ] **Step 8: Ascendant** — add a second halo, lotus-like crown, and short energy trail.
- [ ] **Step 9: Cosmic** — add a star-cloud core, constellation veins, and final lotus crown while preserving the rounded seed at 48×48.

- [ ] **Step 10: process and inspect the complete set**

```powershell
python scripts/process_gem_icons.py
python -m unittest scripts.test_gem_icon_assets -v
```

Open the contact sheet directly and inspect every row left-to-right at full and inset size. Regenerate a specific source rather than patching a failed icon with opaque paint or fake alpha.

- [ ] **Step 11: commit the final source art**

```powershell
git add SourceArt/UI/Items/Gems/final SourceArt/UI/Items/Gems/gem_icon_manifest.json SourceArt/UI/Items/Gems/review/gem-quality-progression-contact-sheet.png
git commit -m "art: add progressive gem item icons"
```

Do not commit discarded generator variants.

### Task 6: import and validate all thirty Unreal textures

**Files:**
- Create: `Content/Python/gamexxk_import_gem_icons.py`
- Create: `Content/Python/gamexxk_validate_gem_icons.py`
- Create: `scripts/gamexxk_gem_icons_apply.py`
- Create: `scripts/test_gem_icon_import_pipeline.py`
- Create: `Content/GameXXK/UI/Items/Gems/*.uasset`

- [ ] **Step 1: add the focused importer**

Follow `Content/Python/gamexxk_import_relic_icons.py`, but source the exact thirty files declared by `gem_icon_manifest.json` and import to `/Game/GameXXK/UI/Items/Gems`.

```python
def configure(texture: unreal.Texture2D) -> None:
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("compression_no_alpha", False)
```

Reject a missing file, non-PNG source, non-512 dimensions, asset-name mismatch, wrong imported class, or failed package save. The script only replaces these thirty declared textures.

- [ ] **Step 2: add the read-only UE validator**

`gamexxk_validate_gem_icons.py` loads each exact texture path and emits JSON with missing, wrong class, wrong size, wrong texture settings, and validated arrays. `ok` is true only when all thirty load and match the import contract.

- [ ] **Step 3: add the MCP apply wrapper and offline tests**

`scripts/gamexxk_gem_icons_apply.py` uses `UnrealMCPClient.run_project_python_file` for importer then validator, matching `scripts/gamexxk_relic_icons_apply.py`. It must reject PIE and non-OK validator JSON.

`scripts/test_gem_icon_import_pipeline.py` uses a fake `unreal` module to verify exact count/path/settings and checks that importer, validator, wrapper, manifest, and processor all agree on the same thirty stems.

Run:

```powershell
python -m unittest scripts.test_gem_icon_import_pipeline -v
python -m py_compile scripts/process_gem_icons.py scripts/gamexxk_gem_icons_apply.py Content/Python/gamexxk_import_gem_icons.py Content/Python/gamexxk_validate_gem_icons.py
```

- [ ] **Step 4: import through UE MCP**

Start the canonical editor with `Launch_GameXXK_Editor.cmd` on `/Game/GameXXK/Maps/L_DesktopTrainingHUD`. Ensure PIE is stopped, then run:

```powershell
python scripts/gamexxk_gem_icons_apply.py
```

Expected JSON: `ok=true`, `imported_count=30`, `validated_count=30`, with empty missing/wrong arrays.

- [ ] **Step 5: save, close, and commit imported assets**

Save dirty packages through UE MCP, close the editor gracefully, and verify all thirty `.uasset` files exist under `Content/GameXXK/UI/Items/Gems`.

```powershell
git add Content/Python/gamexxk_import_gem_icons.py Content/Python/gamexxk_validate_gem_icons.py scripts/gamexxk_gem_icons_apply.py scripts/test_gem_icon_import_pipeline.py Content/GameXXK/UI/Items/Gems
git commit -m "feat: import progressive gem item textures"
```

### Task 7: final art-package verification and review

- [ ] **Step 1: rerun deterministic checks**

```powershell
python scripts/process_gem_icons.py
python -m unittest scripts.test_gem_icon_assets scripts.test_gem_icon_import_pipeline -v
```

Expected: exactly thirty final files and thirty unique hashes; all tests pass.

- [ ] **Step 2: validate imported textures read-only**

With the editor on `L_DesktopTrainingHUD` and PIE stopped, call `Content/Python/gamexxk_validate_gem_icons.py` through `UnrealMCPClient.run_project_python_file`. Expected: `ok=true`, `validated_count=30`.

- [ ] **Step 3: direct visual review**

Inspect the contact sheet without Luna. Confirm each row preserves its type silhouette and fixed main color, every step is visibly at least as complex as the prior step, and the 48×48 inset remains readable. Any failed cell is regenerated from its immediately preceding accepted rank and reprocessed.

- [ ] **Step 4: independent review**

Request a spec review of the manifest/import contract and a quality review of transparency, deterministic processing, package scope, and import safety. Fix every Critical/Important finding before the C++ Gem task consumes these paths.
