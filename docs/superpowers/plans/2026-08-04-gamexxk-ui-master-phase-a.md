# GameXXK UI Master Phase A Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a reviewable GameXXK UI master candidate containing one approved-style common-component page, seventeen grayscale layout pages, a contact sheet, deterministic manifests, and Photoshop save-close-reopen evidence without importing anything into Unreal Engine.

**Architecture:** A small Python contract layer owns page IDs, grid geometry, source locks, component tokens, deterministic raster generation, page preview composition, and package validation. The existing Photoshop JSX generator remains the PSD writer, but gains transparent-canvas-aware proportional placement and hierarchical page-group support. All generated work goes to a new `gamexxk-v4/ui-master` candidate root and a new candidate PSD path; the rejected Hero/Backpack V1 and existing UE assets remain untouched.

**Tech Stack:** Python 3 + Pillow + unittest, Node.js JSX generation, Adobe Photoshop ExtendScript/COM, JSON manifests, SHA-256 source locks, Git on `main` without worktrees.

---

## Scope Boundary

This plan implements only Phase A from `docs/superpowers/specs/2026-08-04-gamexxk-ui-master-components-design.md`:

- complete `00_公共组件`;
- grayscale composition for page groups `01` through `17`;
- per-page previews and one contact sheet;
- candidate layered PSD and readback report;
- no final color pass for all screens;
- no runtime promotion, UE import, WBP change, C++ change, map change, or PIE work.

The root project stays on `main`. Every commit command stages only paths named in that task because the worktree contains unrelated user assets and animation production files.

## File Structure

### New checked-in source and tooling

- `SourceArt/UI/PSD/gamexxk-v4/ui-master/ui-master-spec.json` — canonical 18-page Phase A contract.
- `SourceArt/UI/PSD/gamexxk-v4/ui-master/source-lock.json` — approved reference and final Idle hashes.
- `SourceArt/UI/PSD/gamexxk-v4/ui-master/component-variants.json` — approved component IDs, states, dimensions, and visual rules.
- `SourceArt/UI/PSD/gamexxk-v4/ui-master/Reference/approved_town_hero_backpack.png` — copied user-approved full-screen reference.
- `scripts/gamexxk_ui_master_contract.py` — contract parsing, grid offsets, source-lock validation.
- `scripts/gamexxk_ui_master_assets.py` — deterministic text-free component raster generation.
- `scripts/gamexxk_ui_master_pages.py` — per-page grayscale preview composition.
- `scripts/build_gamexxk_ui_master.py` — package orchestration, manifests, runtime draft assets, and contact sheet.
- `scripts/validate_gamexxk_ui_master.py` — independent package and Photoshop-readback validator.
- `scripts/test_gamexxk_ui_master_contract.py` — contract and source-lock tests.
- `scripts/test_gamexxk_ui_master_assets.py` — component visual-rule tests.
- `scripts/test_gamexxk_ui_master_pages.py` — page roster and preview tests.
- `scripts/test_gamexxk_ui_master_build.py` — isolated build and contact-sheet tests.
- `scripts/test_gamexxk_ui_master_validation.py` — validator acceptance and rejection tests.

### Existing files modified

- `scripts/ui_psd_pipeline/build-psd.js` — canvas-aware proportional image placement, nested groups, optional scaled overview, group readback.
- `scripts/test_town_psd_package.py` — JSX regression coverage for the new generic composer behavior.

### Generated candidate artifacts

- `SourceArt/UI/PSD/gamexxk-v4/ui-master/Assets/*.png`
- `SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeAssets/*.png`
- `SourceArt/UI/PSD/gamexxk-v4/ui-master/Previews/*.png`
- `SourceArt/UI/PSD/gamexxk-v4/ui-master/GameXXK_UI_Master_ContactSheet.png`
- `SourceArt/UI/PSD/gamexxk-v4/ui-master/master-manifest.json`
- `SourceArt/UI/PSD/gamexxk-v4/ui-master/runtime-assets-manifest.json`
- `SourceArt/UI/PSD/gamexxk-v4/ui-master/compose.jsx`
- `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd`
- `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.validation.json`

Generated candidates and previews are not promoted as canonical or committed until the user approves them.

---

### Task 1: Lock the Phase A contract and approved sources

**Files:**
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/ui-master-spec.json`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/source-lock.json`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Reference/approved_town_hero_backpack.png`
- Create: `scripts/gamexxk_ui_master_contract.py`
- Create: `scripts/test_gamexxk_ui_master_contract.py`

- [ ] **Step 1: Copy and hash the approved reference without editing it**

Run:

```powershell
New-Item -ItemType Directory -Force 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Reference'
Copy-Item -LiteralPath 'C:/Users/shxuw/AppData/Local/Temp/codex-clipboard-fec224d3-5cea-4d3a-805f-4b6b37f74632.png' -Destination 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Reference/approved_town_hero_backpack.png'
Get-FileHash -Algorithm SHA256 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Reference/approved_town_hero_backpack.png'
Get-FileHash -Algorithm SHA256 'SourceAssets/AnimationProcessing/Production/character_00_hero_idle/frames/frame_0000.png'
Get-FileHash -Algorithm SHA256 'SourceAssets/AnimationProcessing/Production/character_01_blade_idle/frames/frame_0000.png'
Get-FileHash -Algorithm SHA256 'SourceAssets/AnimationProcessing/Production/enemy_01_rooster_idle/frames/frame_0000.png'
```

Expected: both files exist and each command prints one SHA-256 value. Record those exact values in `source-lock.json`.

- [ ] **Step 2: Write the failing contract test**

Create `scripts/test_gamexxk_ui_master_contract.py` with:

```python
import json
import unittest
from pathlib import Path

from scripts.gamexxk_ui_master_contract import load_contract, validate_source_lock

ROOT = Path(__file__).resolve().parents[1]
PACKAGE = ROOT / "SourceArt/UI/PSD/gamexxk-v4/ui-master"

EXPECTED = [
    "00_公共组件", "01_主菜单", "02_城镇HUD", "03_主角背包",
    "04_伙伴编队", "05_图鉴", "06_任务日志", "07_商店交易",
    "08_路线图", "09_路线事件", "10_战斗HUD", "11_战斗奖励结算",
    "12_系统菜单", "13_主角背包_物品选中", "14_伙伴编队_角色选中",
    "15_图鉴_怪物选中", "16_路线图_节点选中", "17_战斗HUD_卡牌选中目标",
]

class GameXXKUiMasterContractTests(unittest.TestCase):
    def test_contract_has_exact_grid_and_page_roster(self):
        contract = load_contract(PACKAGE / "ui-master-spec.json")
        self.assertEqual((10080, 4680), contract.master_size)
        self.assertEqual((1920, 1080), contract.page_size)
        self.assertEqual(5, contract.columns)
        self.assertEqual(120, contract.gap)
        self.assertEqual(EXPECTED, [page.name for page in contract.pages])
        self.assertEqual((0, 0), contract.page_origin(0))
        self.assertEqual((8160, 0), contract.page_origin(4))
        self.assertEqual((0, 1200), contract.page_origin(5))

    def test_source_lock_accepts_final_idle_and_rejects_retired_portraits(self):
        result = validate_source_lock(PACKAGE / "source-lock.json", ROOT)
        self.assertTrue(result["ok"], result)
        lock = json.loads((PACKAGE / "source-lock.json").read_text(encoding="utf-8"))
        self.assertEqual(
            "SourceAssets/AnimationProcessing/Production/character_00_hero_idle/frames/frame_0000.png",
            lock["heroIdle"]["path"],
        )
        self.assertEqual(
            "SourceAssets/AnimationProcessing/Production/character_01_blade_idle/frames/frame_0000.png",
            lock["partnerIdle"]["path"],
        )
        self.assertEqual(
            "SourceAssets/AnimationProcessing/Production/enemy_01_rooster_idle/frames/frame_0000.png",
            lock["monsterIdle"]["path"],
        )
        self.assertNotIn("PartyDeck/card-portraits/generated", lock["heroIdle"]["path"])

if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 3: Run the test to verify RED**

Run:

```powershell
python -m unittest scripts.test_gamexxk_ui_master_contract -v
```

Expected: `ERROR` because `scripts.gamexxk_ui_master_contract` does not exist.

- [ ] **Step 4: Implement the minimal contract loader and lock validator**

Create `scripts/gamexxk_ui_master_contract.py` with dataclasses `PageSpec` and `UiMasterContract`, `page_origin(index)`, JSON field validation, SHA-256 checking, dimension checking with Pillow, and explicit rejection of every prefix listed in `retiredSourceRoots`.

The public behavior must match:

```python
contract = load_contract(path)
assert contract.master_size == (10080, 4680)
assert contract.page_origin(6) == (2040, 1200)
report = validate_source_lock(lock_path, project_root)
assert report == {
    "ok": True,
    "checked": ["approvedReference", "heroIdle", "partnerIdle", "monsterIdle"],
    "errors": [],
}
```

Create `ui-master-spec.json` with canvas `10080 × 4680`, page size `1920 × 1080`, 5 columns, 120-pixel gap, the exact 18 names in the test, output PSD `D:/UE5 demo/GameXXK/outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd`, and overview scale `0.25`.

Create `source-lock.json` with `approvedReference`, `heroIdle`, `partnerIdle`, `monsterIdle`, and:

```json
"retiredSourceRoots": [
  "SourceAssets/PartyDeck/card-portraits/generated"
]
```

- [ ] **Step 5: Run the focused test to verify GREEN**

Run:

```powershell
python -m unittest scripts.test_gamexxk_ui_master_contract -v
```

Expected: two tests pass.

- [ ] **Step 6: Commit only the contract slice**

```powershell
git add -- SourceArt/UI/PSD/gamexxk-v4/ui-master/ui-master-spec.json SourceArt/UI/PSD/gamexxk-v4/ui-master/source-lock.json SourceArt/UI/PSD/gamexxk-v4/ui-master/Reference/approved_town_hero_backpack.png scripts/gamexxk_ui_master_contract.py scripts/test_gamexxk_ui_master_contract.py
git commit -m "feat: lock GameXXK UI master contract"
```

---

### Task 2: Fix transparent-canvas proportional placement in the generic PSD composer

**Files:**
- Modify: `scripts/ui_psd_pipeline/build-psd.js`
- Modify: `scripts/test_town_psd_package.py`

- [ ] **Step 1: Add a failing JSX regression test**

Extend `TownPsdPackageTest.test_composer_supports_grouped_scaled_layers` so its temporary manifest includes:

```python
{
    "name": "transparent_character",
    "path": "clean_assets/character.png",
    "x": 100,
    "y": 80,
    "width": 490,
    "height": 490,
    "fitMode": "contain_canvas",
    "group": "03_主角背包/30_角色",
}
```

Add assertions:

```python
self.assertIn("sourceCanvasWidth", compose)
self.assertIn("sourceCanvasHeight", compose)
self.assertIn("item.fitMode == 'contain_canvas'", compose)
self.assertIn("Math.min(item.width / sourceCanvasWidth, item.height / sourceCanvasHeight)", compose)
self.assertIn("duplicated.resize(canvasScale * 100, canvasScale * 100", compose)
self.assertIn("ensureGroupPath(doc, item.group)", compose)
self.assertNotIn("resizeLayerTo(duplicated, item.width, item.height)", compose)
```

- [ ] **Step 2: Run the focused test to verify RED**

Run:

```powershell
python -m unittest scripts.test_town_psd_package.TownPsdPackageTest.test_composer_supports_grouped_scaled_layers -v
```

Expected: assertion failure for missing `sourceCanvasWidth`.

- [ ] **Step 3: Implement one proportional placement path**

In generated JSX, capture the source canvas and content offsets before duplication:

```javascript
var sourceCanvasWidth = source.width.as('px');
var sourceCanvasHeight = source.height.as('px');
var sourceBounds = sourceLayer.bounds;
var sourceLeft = sourceBounds[0].as('px');
var sourceTop = sourceBounds[1].as('px');
```

For `fitMode == 'contain_canvas'`, use one uniform scale and position the content relative to the complete transparent canvas:

```javascript
var canvasScale = Math.min(item.width / sourceCanvasWidth, item.height / sourceCanvasHeight);
duplicated.resize(canvasScale * 100, canvasScale * 100, AnchorPosition.TOPLEFT);
var canvasWidth = sourceCanvasWidth * canvasScale;
var canvasHeight = sourceCanvasHeight * canvasScale;
var canvasX = item.x + (item.width - canvasWidth) / 2;
var canvasY = item.y + (item.height - canvasHeight) / 2;
var desiredLeft = canvasX + sourceLeft * canvasScale;
var desiredTop = canvasY + sourceTop * canvasScale;
```

Keep the existing explicit stretch path only for assets whose manifest declares `fitMode: "stretch"`. Do not infer fit mode from transparency. Move layers through `ensureGroupPath(doc, item.group)` so `03_主角背包/30_角色` creates one top-level page group and one nested role group.

Preserve the existing Photoshop safeguards already present in the dirty worktree: activate `duplicated` before transformation, resize hidden reference layers before restoring their hidden state, and keep layer-specific `Failed to resize layer <name>` diagnostics.

- [ ] **Step 4: Run composer and legacy tests to verify GREEN**

Run:

```powershell
python -m unittest scripts.test_town_psd_package scripts.test_hero_backpack_psd_package -v
```

Expected: all tests pass. The rejected V1 package must remain buildable, but no files under it are overwritten by this test.

- [ ] **Step 5: Commit the generic composer fix**

```powershell
git add -- scripts/ui_psd_pipeline/build-psd.js scripts/test_town_psd_package.py
git commit -m "fix: preserve transparent canvas proportions in PSD composer"
```

---

### Task 3: Generate the common component system without colored button fills

**Files:**
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/component-variants.json`
- Create: `scripts/gamexxk_ui_master_assets.py`
- Create: `scripts/test_gamexxk_ui_master_assets.py`

- [ ] **Step 1: Write failing component-rule tests**

Create tests for deterministic size, alpha, states, and button color coverage:

```python
import colorsys
import tempfile
import unittest
from pathlib import Path
from PIL import Image

from scripts.gamexxk_ui_master_assets import build_component_assets

class GameXXKUiMasterAssetTests(unittest.TestCase):
    def test_builds_required_text_free_component_states(self):
        with tempfile.TemporaryDirectory() as td:
            manifest = build_component_assets(Path(td))
            required = {
                "button_normal", "button_hover", "button_pressed", "button_primary",
                "button_danger", "button_disabled",
                "tab_normal", "tab_hover", "tab_pressed", "tab_selected", "tab_disabled",
                "nav_normal", "nav_hover", "nav_selected", "nav_reminder", "nav_locked",
                "nav_backpack", "nav_companion", "nav_codex", "nav_task", "nav_route",
                "item_slot_empty", "item_slot_hover", "item_slot_selected", "item_slot_locked",
                "equipment_slot_empty", "equipment_slot_hover", "equipment_slot_selected",
                "card_frame_role", "card_frame_monster", "card_frame_general",
                "card_frame_terrain", "card_frame_rare", "card_frame_boss",
                "panel_large", "panel_medium", "panel_small",
                "progress_track", "progress_fill", "resource_strip", "tooltip_panel",
            }
            self.assertTrue(required.issubset(manifest))
            for record in manifest.values():
                image = Image.open(Path(td) / record["file"]).convert("RGBA")
                self.assertEqual(record["size"], list(image.size))
                self.assertEqual((0, 255), image.getchannel("A").getextrema())

    def test_buttons_do_not_use_large_red_blue_or_green_fills(self):
        with tempfile.TemporaryDirectory() as td:
            manifest = build_component_assets(Path(td))
            for key in ("button_normal", "button_hover", "button_pressed", "button_primary", "button_danger"):
                image = Image.open(Path(td) / manifest[key]["file"]).convert("RGBA")
                vivid = 0
                opaque = 0
                for r, g, b, a in image.getdata():
                    if a < 128:
                        continue
                    opaque += 1
                    _, saturation, _ = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
                    vivid += saturation > 0.42
                self.assertLess(vivid / max(opaque, 1), 0.06, key)
```

- [ ] **Step 2: Run the test to verify RED**

```powershell
python -m unittest scripts.test_gamexxk_ui_master_assets -v
```

Expected: module import error.

- [ ] **Step 3: Implement deterministic component generation**

Implement these pure functions in `scripts/gamexxk_ui_master_assets.py`. This is the minimum complete implementation shape; visual iteration changes tokens and geometry without changing the public API:

```python
from __future__ import annotations

import json
import random
from pathlib import Path

from PIL import Image, ImageDraw

PAPER = (232, 215, 179, 255)
INK = (43, 40, 34, 255)
SECONDARY_INK = (93, 85, 72, 210)
CINNABAR = (161, 79, 54, 255)


def make_torn_paper(size: tuple[int, int], seed: int, edge_strength: int) -> Image.Image:
    width, height = size
    rng = random.Random(seed)
    step = max(12, min(width, height) // 8)
    points: list[tuple[int, int]] = []
    for x in range(4, width - 3, step):
        points.append((x, rng.randint(2, 2 + edge_strength)))
    for y in range(4, height - 3, step):
        points.append((width - 3 - rng.randint(0, edge_strength), y))
    for x in range(width - 4, 3, -step):
        points.append((x, height - 3 - rng.randint(0, edge_strength)))
    for y in range(height - 4, 3, -step):
        points.append((2 + rng.randint(0, edge_strength), y))
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).polygon(points, fill=255)
    image = Image.new("RGBA", size, PAPER)
    image.putalpha(mask)
    draw = ImageDraw.Draw(image)
    for _ in range(max(80, width * height // 900)):
        x = rng.randrange(width)
        y = rng.randrange(height)
        radius = rng.choice((1, 1, 2))
        shade = rng.choice(((92, 78, 56, 12), (255, 248, 221, 15)))
        draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=shade)
    draw.line(points + [points[0]], fill=SECONDARY_INK, width=2, joint="curve")
    image.putalpha(mask)
    return image


def make_ink_button(size: tuple[int, int], state: str) -> Image.Image:
    image = make_torn_paper(size, 240804 + sum(map(ord, state)), 3)
    draw = ImageDraw.Draw(image)
    width, height = size
    inset = 7 if state != "pressed" else 9
    outline = INK if state in {"hover", "primary", "danger"} else SECONDARY_INK
    draw.rectangle((inset, inset, width - inset - 1, height - inset - 1), outline=outline, width=3)
    if state == "hover":
        draw.line((inset + 8, height - inset - 7, width - inset - 8, height - inset - 7), fill=INK, width=3)
    elif state == "pressed":
        draw.rectangle((inset + 2, inset + 2, width - inset - 3, height - inset - 3), fill=(43, 40, 34, 18))
    elif state == "primary":
        draw.line((inset + 12, height // 2 + 8, width - inset - 12, height // 2 + 5), fill=(43, 40, 34, 76), width=8)
    elif state == "danger":
        draw.ellipse((width - 31, 10, width - 13, 28), fill=CINNABAR)
    elif state == "disabled":
        alpha = image.getchannel("A").point(lambda value: value * 3 // 5)
        image.putalpha(alpha)
    return image


def make_ink_tab(size: tuple[int, int], state: str) -> Image.Image:
    image = make_ink_button(size, "hover" if state == "selected" else state)
    if state == "selected":
        draw = ImageDraw.Draw(image)
        draw.line((10, size[1] - 10, size[0] - 10, size[1] - 12), fill=INK, width=7)
    return image


def make_nav_disc(size: int, state: str) -> Image.Image:
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    outer = (3, 3, size - 4, size - 4)
    draw.ellipse(outer, fill=PAPER, outline=INK if state in {"hover", "selected"} else SECONDARY_INK, width=4)
    if state == "selected":
        draw.arc((10, 10, size - 11, size - 11), 205, 515, fill=INK, width=7)
    elif state == "reminder":
        draw.ellipse((size - 24, 5, size - 6, 23), fill=CINNABAR)
    elif state == "locked":
        draw.ellipse(outer, fill=(43, 40, 34, 78), outline=SECONDARY_INK, width=4)
    return image


def make_nav_icon(size: int, kind: str) -> Image.Image:
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    if kind == "backpack":
        draw.rounded_rectangle((18, 24, size - 18, size - 13), 8, outline=INK, width=5)
        draw.arc((26, 10, size - 26, 38), 180, 360, fill=INK, width=5)
    elif kind == "companion":
        draw.ellipse((12, 15, 38, 41), outline=INK, width=5)
        draw.ellipse((size - 39, 22, size - 13, 48), outline=INK, width=5)
        draw.arc((7, 28, 45, size - 8), 190, 350, fill=INK, width=5)
        draw.arc((size - 46, 35, size - 8, size - 5), 190, 350, fill=INK, width=5)
    elif kind == "codex":
        draw.polygon(((7, 20), (size // 2 - 2, 28), (size // 2 - 2, size - 12), (8, size - 20)), outline=INK)
        draw.polygon(((size - 7, 20), (size // 2 + 2, 28), (size // 2 + 2, size - 12), (size - 8, size - 20)), outline=INK)
        draw.line((size // 2, 27, size // 2, size - 12), fill=INK, width=4)
    elif kind == "task":
        draw.rounded_rectangle((16, 9, size - 16, size - 9), 5, outline=INK, width=5)
        draw.line((25, 25, size - 25, 25), fill=INK, width=4)
        draw.line((25, 38, size - 25, 38), fill=INK, width=4)
        draw.line((25, 51, size - 34, 51), fill=INK, width=4)
    elif kind == "route":
        draw.line((13, size - 18, 28, 24, 45, 43, size - 13, 14), fill=INK, width=5)
        for x, y in ((13, size - 18), (28, 24), (45, 43), (size - 13, 14)):
            draw.ellipse((x - 5, y - 5, x + 5, y + 5), fill=PAPER, outline=INK, width=3)
    else:
        raise ValueError(f"unknown nav icon: {kind}")
    return image


def make_slot(size: tuple[int, int], family: str, state: str, tint: tuple[int, int, int] | None = None) -> Image.Image:
    base = (*tint, 255) if tint else PAPER
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    width, height = size
    draw.rectangle((4, 4, width - 5, height - 5), fill=base, outline=SECONDARY_INK, width=3)
    if family == "equipment":
        draw.line((12, 11, width - 13, 11), fill=(255, 248, 221, 120), width=3)
    if state in {"hover", "selected"}:
        draw.rectangle((8, 8, width - 9, height - 9), outline=INK, width=4 if state == "selected" else 2)
    if state == "locked":
        draw.rectangle((4, 4, width - 5, height - 5), fill=(43, 40, 34, 82), outline=INK, width=3)
    return image


def build_component_assets(output_root: Path) -> dict[str, dict]:
    output_root.mkdir(parents=True, exist_ok=True)
    records: dict[str, dict] = {}

    def write(key: str, image: Image.Image) -> None:
        filename = f"UIV4_{key}.png"
        image.save(output_root / filename)
        records[key] = {"file": filename, "size": list(image.size), "textBaked": False}

    for state in ("normal", "hover", "pressed", "primary", "danger", "disabled"):
        write(f"button_{state}", make_ink_button((220, 72), state))
    for state in ("normal", "hover", "pressed", "selected", "disabled"):
        write(f"tab_{state}", make_ink_tab((160, 58), state))
    for state in ("normal", "hover", "selected", "reminder", "locked"):
        write(f"nav_{state}", make_nav_disc(112, state))
    for kind in ("backpack", "companion", "codex", "task", "route"):
        write(f"nav_{kind}", make_nav_icon(72, kind))
    for state in ("empty", "hover", "selected", "locked"):
        write(f"item_slot_{state}", make_slot((104, 104), "item", state))
    for state in ("empty", "hover", "selected"):
        write(f"equipment_slot_{state}", make_slot((124, 130), "equipment", state))
    card_tints = {
        "role": (222, 205, 170), "monster": (210, 197, 178),
        "general": (218, 211, 185), "terrain": (196, 207, 184),
        "rare": (204, 194, 210), "boss": (213, 186, 167),
    }
    for family, tint in card_tints.items():
        write(f"card_frame_{family}", make_slot((300, 420), "card", "empty", tint))
    write("panel_large", make_torn_paper((1440, 780), 240811, 9))
    write("panel_medium", make_torn_paper((760, 520), 240812, 7))
    write("panel_small", make_torn_paper((420, 260), 240813, 5))
    write("progress_track", make_ink_button((420, 24), "normal"))
    fill = Image.new("RGBA", (280, 10), (0, 0, 0, 0))
    ImageDraw.Draw(fill).rounded_rectangle((0, 0, 279, 9), 5, fill=(91, 126, 96, 255))
    write("progress_fill", fill)
    write("resource_strip", make_torn_paper((680, 92), 240814, 4))
    write("tooltip_panel", make_torn_paper((520, 240), 240815, 5))
    (output_root / "component-assets.json").write_text(
        json.dumps(records, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    return records
```

Use fixed seed `240804`, palette `paper=#E8D7B3`, `ink=#2B2822`, `secondaryInk=#5D5548`, `cinnabar=#A14F36`, and no saturated background rectangle. `button_danger` may place one small cinnabar seal whose area remains below 6% of opaque pixels. The six card frames use low-saturation paper tints and distinct local motifs, never full white or neon rarity fills. The five navigation icons must have different silhouettes and internal subjects. Do not draw any labels into raster assets.

Write `component-variants.json` with the exact component IDs, allowed states, pixel sizes, nine-slice margins, `textBaked: false`, and `buttonColorFill: "paper_and_ink_only"`.

- [ ] **Step 4: Run asset tests to verify GREEN**

```powershell
python -m unittest scripts.test_gamexxk_ui_master_assets -v
```

Expected: all tests pass with no Pillow warnings.

- [ ] **Step 5: Commit the component generator**

```powershell
git add -- SourceArt/UI/PSD/gamexxk-v4/ui-master/component-variants.json scripts/gamexxk_ui_master_assets.py scripts/test_gamexxk_ui_master_assets.py
git commit -m "feat: add GameXXK ink-paper UI components"
```

---

### Task 4: Define and render all 18 page-group previews

**Files:**
- Create: `scripts/gamexxk_ui_master_pages.py`
- Create: `scripts/test_gamexxk_ui_master_pages.py`

- [ ] **Step 1: Write failing page-roster and proportional-Hero tests**

```python
import tempfile
import unittest
from pathlib import Path
from PIL import Image

from scripts.gamexxk_ui_master_contract import load_contract
from scripts.gamexxk_ui_master_pages import build_page_previews, contain_canvas

class GameXXKUiMasterPageTests(unittest.TestCase):
    def test_contain_canvas_keeps_one_uniform_scale(self):
        placement = contain_canvas((512, 512), (490, 490), (118, 48, 378, 471))
        self.assertEqual(placement.scale_x, placement.scale_y)
        self.assertAlmostEqual(490 / 512, placement.scale_x)
        self.assertAlmostEqual(260 * placement.scale_x, placement.content_width)
        self.assertAlmostEqual(423 * placement.scale_y, placement.content_height)

    def test_builds_exactly_eighteen_full_hd_previews(self):
        with tempfile.TemporaryDirectory() as td:
            output = Path(td)
            records = build_page_previews(output)
            self.assertEqual(18, len(records))
            self.assertEqual("00_公共组件", records[0]["group"])
            self.assertEqual("17_战斗HUD_卡牌选中目标", records[-1]["group"])
            for record in records:
                with Image.open(output / record["file"]) as image:
                    self.assertEqual((1920, 1080), image.size)
```

- [ ] **Step 2: Run tests to verify RED**

```powershell
python -m unittest scripts.test_gamexxk_ui_master_pages -v
```

Expected: module import error.

- [ ] **Step 3: Implement grayscale pages using only approved components**

Create a `PageBuilder` that records both a preview canvas and manifest layers. Each page must have the page group as its top-level group and nested semantic groups such as `10_World`, `20_Shell`, `30_Content`, `70_RuntimeText`.

Implement `contain_canvas` as:

```python
scale = min(target_width / source_width, target_height / source_height)
content_width = (alpha_right - alpha_left) * scale
content_height = (alpha_bottom - alpha_top) * scale
```

For `03_主角背包`, reproduce the approved reference hierarchy: current town context, left HUD, top resource strip, circular nav, torn-paper main panel, six equipment slots, proportionally placed final Hero Idle, inventory area, detail panel, and paper/ink action controls. Other page groups use real page titles and functional grayscale hierarchy, not blank boxes or lorem ipsum.

- [ ] **Step 4: Verify the focused page tests are GREEN**

```powershell
python -m unittest scripts.test_gamexxk_ui_master_pages -v
```

Expected: exact 18 previews and uniform Hero scale pass.

- [ ] **Step 5: Commit page composition code**

```powershell
git add -- scripts/gamexxk_ui_master_pages.py scripts/test_gamexxk_ui_master_pages.py
git commit -m "feat: define GameXXK UI master page layouts"
```

---

### Task 5: Build the isolated master package and contact sheet

**Files:**
- Create: `scripts/build_gamexxk_ui_master.py`
- Create: `scripts/test_gamexxk_ui_master_build.py`

- [ ] **Step 1: Write a failing isolated-build test**

```python
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
BUILDER = ROOT / "scripts/build_gamexxk_ui_master.py"

class GameXXKUiMasterBuildTests(unittest.TestCase):
    def test_cli_builds_master_manifest_previews_and_contact_sheet(self):
        with tempfile.TemporaryDirectory() as td:
            result = subprocess.run(
                [sys.executable, str(BUILDER), "--output-root", td],
                cwd=ROOT, capture_output=True, text=True, check=False,
            )
            self.assertEqual(0, result.returncode, result.stderr)
            report = json.loads(result.stdout)
            self.assertEqual([10080, 4680], report["masterCanvas"])
            self.assertEqual(18, report["pageGroups"])
            output = Path(td)
            manifest = json.loads((output / "master-manifest.json").read_text(encoding="utf-8"))
            self.assertEqual(18, len(manifest["pages"]))
            self.assertEqual(18, len(list((output / "Previews").glob("*.png"))))
            with Image.open(output / "GameXXK_UI_Master_ContactSheet.png") as sheet:
                self.assertEqual((2400, 1080), sheet.size)
```

- [ ] **Step 2: Run the build test to verify RED**

```powershell
python -m unittest scripts.test_gamexxk_ui_master_build -v
```

Expected: the builder file is missing.

- [ ] **Step 3: Implement the package orchestrator**

`build_gamexxk_ui_master.py` must:

1. load and validate the contract and source lock;
2. generate `Assets` and draft `RuntimeAssets` from one component map;
3. generate exactly 18 previews;
4. place 480×270 thumbnails in a 5×4 `2400 × 1080` contact sheet with page labels in the evidence layer only;
5. offset every page layer by `page_origin(index)` for the master manifest;
6. keep all dynamic text in `textLayers` and every raster record text-free;
7. keep the generic composer contract at top level: `document`, `imageLayers`, and `textLayers`, then add the Phase A `pages` records beside them;
8. write `master-manifest.json`, `runtime-assets-manifest.json`, and a machine-readable stdout report.

Do not invoke Photoshop from the Python builder. Photoshop remains a separate, observable step.

- [ ] **Step 4: Run build tests to verify GREEN**

```powershell
python -m unittest scripts.test_gamexxk_ui_master_build -v
```

Expected: isolated build passes and writes no files outside its temporary root.

- [ ] **Step 5: Commit the orchestrator**

```powershell
git add -- scripts/build_gamexxk_ui_master.py scripts/test_gamexxk_ui_master_build.py
git commit -m "feat: build GameXXK UI master review package"
```

---

### Task 6: Add master PSD nested groups and readback evidence

**Files:**
- Modify: `scripts/ui_psd_pipeline/build-psd.js`
- Modify: `scripts/test_town_psd_package.py`

- [ ] **Step 1: Extend the failing composer test for group readback and scaled overview**

Add assertions that generated JSX contains:

```python
self.assertIn("function ensureGroupPath", compose)
self.assertIn("topLevelGroups.push", compose)
self.assertIn("actualTopLevelGroups", compose)
self.assertIn("previewDoc.resizeImage", compose)
self.assertIn("spec.overviewScale", compose)
```

The test must invoke the Node builder with `--manifest master-manifest.json` and prove that an explicit manifest filename is accepted while legacy packages still default to `manifest.json`. Add this rejection case as well:

```python
rejected = subprocess.run(
    ["node", str(PSD_COMPOSER), "--root", str(root), "--manifest", "../manifest.json"],
    capture_output=True, text=True, check=False,
)
self.assertNotEqual(0, rejected.returncode)
self.assertIn("manifest filename must stay inside package root", rejected.stderr)
```

- [ ] **Step 2: Run focused test to verify RED**

```powershell
python -m unittest scripts.test_town_psd_package.TownPsdPackageTest.test_composer_supports_grouped_scaled_layers -v
```

Expected: missing `actualTopLevelGroups` assertion.

- [ ] **Step 3: Implement nested groups and richer validation**

`ensureGroupPath(doc, "03_主角背包/30_角色")` must create or reuse `03_主角背包` at document root, then create or reuse `30_角色` inside it. The readback report must include:

```json
{
  "width": 10080,
  "height": 4680,
  "expectedTopLevelGroups": 18,
  "actualTopLevelGroups": [
    "00_公共组件", "01_主菜单", "02_城镇HUD", "03_主角背包",
    "04_伙伴编队", "05_图鉴", "06_任务日志", "07_商店交易",
    "08_路线图", "09_路线事件", "10_战斗HUD", "11_战斗奖励结算",
    "12_系统菜单", "13_主角背包_物品选中", "14_伙伴编队_角色选中",
    "15_图鉴_怪物选中", "16_路线图_节点选中", "17_战斗HUD_卡牌选中目标"
  ],
  "expectedTextLayers": 42,
  "actualTextLayers": 42,
  "textRoundTripMatch": true
}
```

The text count shown above is the focused test fixture value; the real report must read both text counts from the generated manifest and reopened PSD. If `spec.overviewScale` is below `1`, duplicate and flatten the document, resize it by that factor, and save `master-overview.png` instead of exporting a full-size 10080×4680 PNG.

Add `readManifestName(argumentsList)` in the Node entrypoint. It returns the value after `--manifest`, rejects missing values and path traversal, and otherwise returns `manifest.json`. Resolve the selected filename inside `--root` only.

- [ ] **Step 4: Run all composer regressions**

```powershell
python -m unittest scripts.test_town_psd_package scripts.test_hero_backpack_psd_package -v
```

Expected: all tests pass.

- [ ] **Step 5: Commit the PSD writer changes**

```powershell
git add -- scripts/ui_psd_pipeline/build-psd.js scripts/test_town_psd_package.py
git commit -m "feat: support UI master page groups in Photoshop composer"
```

---

### Task 7: Add independent package and Photoshop validation

**Files:**
- Create: `scripts/validate_gamexxk_ui_master.py`
- Create: `scripts/test_gamexxk_ui_master_validation.py`

- [ ] **Step 1: Write failing validation tests**

Build one temporary valid package fixture, then test three cases:

```python
def test_validator_accepts_complete_phase_a_package(self):
    report = validate_package(self.package_root, self.project_root)
    self.assertTrue(report["ok"], report)

def test_validator_rejects_missing_page_group(self):
    self.validation["actualTopLevelGroups"].remove("05_图鉴")
    report = validate_package(self.package_root, self.project_root)
    self.assertIn("missing top-level group: 05_图鉴", report["errors"])

def test_validator_rejects_changed_idle_hash(self):
    self.source_lock["heroIdle"]["sha256"] = "0" * 64
    report = validate_package(self.package_root, self.project_root)
    self.assertIn("source hash mismatch: heroIdle", report["errors"])
```

- [ ] **Step 2: Run tests to verify RED**

```powershell
python -m unittest scripts.test_gamexxk_ui_master_validation -v
```

Expected: validator module import error.

- [ ] **Step 3: Implement exact validation checks**

`validate_package(package_root, project_root)` must check:

- contract and source hashes;
- master manifest canvas `10080 × 4680`;
- exact 18 page-group names and no unexpected group;
- 18 full-HD previews and one `2400 × 1080` contact sheet;
- runtime draft assets exist, have alpha, and contain no baked text records;
- candidate PSD exists;
- Photoshop JSON canvas, group names, layer counts, and text round trip match;
- Hero placement records use `fitMode: "contain_canvas"` and one scale value;
- no source path begins with a retired source root.

The CLI prints JSON and exits `0` only when `ok` is true.

- [ ] **Step 4: Run validation tests to verify GREEN**

```powershell
python -m unittest scripts.test_gamexxk_ui_master_validation -v
```

Expected: all acceptance and rejection cases pass.

- [ ] **Step 5: Commit validator code**

```powershell
git add -- scripts/validate_gamexxk_ui_master.py scripts/test_gamexxk_ui_master_validation.py
git commit -m "test: validate GameXXK UI master candidates"
```

---

### Task 8: Generate and inspect the real Phase A candidate

**Files:**
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Assets/*`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeAssets/*`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Previews/*`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/GameXXK_UI_Master_ContactSheet.png`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/master-manifest.json`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/runtime-assets-manifest.json`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/compose.jsx`
- Generate: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd`
- Generate: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.validation.json`

- [ ] **Step 1: Run all Phase A automated tests before generation**

```powershell
python -m unittest scripts.test_gamexxk_ui_master_contract scripts.test_gamexxk_ui_master_assets scripts.test_gamexxk_ui_master_pages scripts.test_gamexxk_ui_master_build scripts.test_gamexxk_ui_master_validation scripts.test_town_psd_package scripts.test_town_psd_image_ops scripts.test_hero_backpack_psd_package -v
```

Expected: all tests pass with no warnings or errors.

- [ ] **Step 2: Build the deterministic package**

```powershell
python scripts/build_gamexxk_ui_master.py --output-root SourceArt/UI/PSD/gamexxk-v4/ui-master
```

Expected stdout includes:

```json
{"ok": true, "masterCanvas": [10080, 4680], "pageGroups": 18, "phase": "A"}
```

- [ ] **Step 3: Generate Photoshop JSX and run its safe package check**

```powershell
node scripts/ui_psd_pipeline/build-psd.js --root SourceArt/UI/PSD/gamexxk-v4/ui-master --manifest master-manifest.json
powershell -ExecutionPolicy Bypass -File scripts/ui_psd_pipeline/run-photoshop.ps1 -Root SourceArt/UI/PSD/gamexxk-v4/ui-master -CheckOnly
```

Expected: 18 page groups are represented in the manifest, `compose.jsx` exists, and the runner reports the package ready.

- [ ] **Step 4: Run the real Photoshop composition**

```powershell
powershell -ExecutionPolicy Bypass -File scripts/ui_psd_pipeline/run-photoshop.ps1 -Root SourceArt/UI/PSD/gamexxk-v4/ui-master
```

Expected: `Photoshop composition finished.` and both candidate PSD and validation JSON receive current timestamps. Do not force-close Photoshop if it reports unsaved unrelated work.

- [ ] **Step 5: Run independent validation**

```powershell
python scripts/validate_gamexxk_ui_master.py --package-root SourceArt/UI/PSD/gamexxk-v4/ui-master --project-root .
```

Expected: JSON with `"ok": true`, 18 actual top-level groups, matching text layers, matching source hashes, and no retired paths.

- [ ] **Step 6: Visually inspect the component page and contact sheet**

Open with original-detail inspection:

```text
SourceArt/UI/PSD/gamexxk-v4/ui-master/Previews/00_公共组件.png
SourceArt/UI/PSD/gamexxk-v4/ui-master/GameXXK_UI_Master_ContactSheet.png
SourceArt/UI/PSD/gamexxk-v4/ui-master/Previews/03_主角背包.png
```

Acceptance observations to record:

- Hero is no longer horizontally stretched;
- no red, blue, or green full-fill action button exists;
- normal/primary/danger buttons remain distinguishable by ink weight and the small cinnabar mark;
- all pages reuse the same HUD, panel, tab, button, slot, and text hierarchy;
- background town remains visible around compact panels;
- `03_主角背包` visually tracks the approved reference rather than the rejected V1.

- [ ] **Step 7: Present review artifacts without promoting them**

Give the user clickable paths to the public component preview, contact sheet, Hero/Backpack preview, candidate PSD, and validation JSON. Mark each page `pending_visual_review`. Do not stage generated candidates or runtime assets yet.

---

## Final Verification Checklist

- [ ] Exact 18-page roster and 5×4 grid are present.
- [ ] `00_公共组件` contains all required families and states.
- [ ] Seventeen other pages are grayscale but functionally meaningful.
- [ ] Contact sheet is 5×4 and contains all 18 pages.
- [ ] Hero uses the final Idle source and uniform scale.
- [ ] Buttons contain no large red, blue, or green fill.
- [ ] Dynamic text remains editable and separate from raster components.
- [ ] Photoshop save-close-reopen validation passes.
- [ ] Existing Hero/Backpack V1, town assets, WBP assets, maps, cameras, and UE project state are untouched.
- [ ] No generated candidate is promoted or imported before explicit user approval.
