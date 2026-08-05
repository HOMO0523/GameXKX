# Master UI Alignment Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Align the six backpack equipment slots and center approved short control labels on Master UI pages `03_主角背包` and `07_商店交易`.

**Architecture:** Extend `PageBuilder` with one measured-font centered-text helper that still emits ordinary editable text-layer records. Use it only for short control labels, while retaining left alignment for titles and descriptive copy. Normalize the six equipment-frame coordinates without changing any source artwork or runtime code.

**Tech Stack:** Python 3, Pillow, existing GameXXK Master UI generator, PNG/JSON post-generation validation.

---

## Task 1: Align Master UI slots and control text

**Files:**

- Modify: `scripts/gamexxk_ui_master_pages.py`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Previews/03_主角背包.png`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Previews/07_商店交易.png`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/GameXXK_UI_Master_ContactSheet.png`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/master-manifest.json`
- Modify: `docs/production/2026-08-04-gamexxk-ui-calibration-v2-progress.md`

This is pure Master UI visual work. Per the user's standing direction, do not use TDD; validate only after generation.

- [x] **Step 1: Add measured centered-text placement**

Add a `PageBuilder.add_centered_text` method beside `add_text`. Measure the exact font with `ImageDraw.textbbox`, compute the drawing origin that centers the glyph bounds inside the supplied rectangle, then delegate to `add_text` so the manifest continues to contain editable text:

```python
    def add_centered_text(
        self,
        text: str,
        box: tuple[int, int, int, int],
        size: int,
        subgroup: str = "70_RuntimeText",
        *,
        bold: bool = False,
        color: tuple[int, int, int, int] = INK,
        name: str | None = None,
    ) -> None:
        x, y, width, height = box
        font = _font(size, bold=bold)
        bounds = ImageDraw.Draw(Image.new("RGBA", (1, 1))).textbbox((0, 0), text, font=font)
        text_width = bounds[2] - bounds[0]
        text_height = bounds[3] - bounds[1]
        draw_x = round(x + (width - text_width) / 2 - bounds[0])
        draw_y = round(y + (height - text_height) / 2 - bounds[1])
        self.add_text(text, (draw_x, draw_y), size, subgroup, bold=bold, color=color, name=name)
```

- [x] **Step 2: Normalize the six backpack equipment slots**

Replace only the four drifting x-coordinates so both columns share exact origins:

```python
    equipment_frames = (
        ("equipment_slot_left_01.png", 420, 340, "starter_weapon.png"),
        ("equipment_slot_left_02.png", 420, 515, "starter_head.png"),
        ("equipment_slot_left_03.png", 420, 690, "starter_armor.png"),
        ("equipment_slot_right_01.png", 930, 340, "starter_belt.png"),
        ("equipment_slot_right_02.png", 930, 515, "starter_shoes.png"),
        ("equipment_slot_right_03.png", 930, 690, "starter_accessory.png"),
    )
```

Keep frames `118 × 124` and icons `88 × 88` at `x + 15, y + 15`.

- [x] **Step 3: Center the approved backpack-page labels**

Use `add_centered_text` for each tab label within `(x, 220, 105, 62)`. Give the five inventory categories explicit short regions starting at x positions `1128, 1208, 1288, 1368, 1448`, each `70 × 44`, without moving `背包  3 / 200`. Center the four bottom stats in four `130 × 44` regions starting at x `445, 575, 705, 835`.

- [x] **Step 4: Center the approved shop-page labels**

For each product card, center the product name in `(x - 10, y + 178, 190, 34)` and the price in `(x, y + 216, 170, 32)`. Center `破军装备包` in the detail column rectangle `(1305, 575, 350, 44)` and center `购买  100` within the button rectangle `(1380, 870, 210, 72)`. Leave page titles, subtitles, product descriptions, probability copy, permanent gold, and confirmation hint left-aligned.

- [x] **Step 5: Rebuild the Master outputs**

Run:

```powershell
python -m py_compile scripts/gamexxk_ui_master_pages.py scripts/build_gamexxk_ui_master.py
python scripts/build_gamexxk_ui_master.py
```

Expected: syntax exit code `0`; build returns `ok: true`, 18 pages, and V2 pages `00_公共组件`, `03_主角背包`, `07_商店交易`.

- [x] **Step 6: Run post-generation structural checks**

Read `master-manifest.json` and assert:

- the six equipment-frame layer positions are exactly `(420,340)`, `(420,515)`, `(420,690)`, `(930,340)`, `(930,515)`, `(930,690)`;
- all six equipment frames remain `118 × 124` and their icons remain `88 × 88` with 15-pixel inset;
- both priority previews remain `1920 × 1080`;
- the centered text-layer glyph bounds fall within one pixel of the target-box center;
- the backpack still has 20 visible slots and one right-side scrollbar;
- the shop still has exactly seven product icons and no legacy `草药包` or `出售` text;
- `git diff --check` exits `0`.

- [x] **Step 7: Review and record the visual result**

Inspect `03_主角背包.png`, `07_商店交易.png`, and the contact sheet at original resolution. Confirm two exact equipment columns, three equal rows, centered control labels, no overlap, and unchanged titles/descriptive alignment. Append the two preview hashes and review result to `docs/production/2026-08-04-gamexxk-ui-calibration-v2-progress.md`.

- [x] **Step 8: Commit the visual adjustment**

Stage only the page generator, the two priority previews, regenerated contact sheet/manifest, this plan, and the production note:

```powershell
git commit -m "fix: align Master UI slots and labels"
```
