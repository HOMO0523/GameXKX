# GameXXK Meta Shop PSD Production Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Work directly in the root project on `main`; GameXXK explicitly forbids worktrees. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild the `07_商店交易` UI Master page from the approved component kit so it matches the supplied 1920×1080 effect reference, produce an editable layered PSD plus four review PNGs, and stop for user approval before any RuntimeAssets or UE migration.

**Architecture:** Keep the existing generated UI Master pipeline authoritative. Extend the page builder only enough to support visible/hidden state groups, compose the shop from existing V2 shell/content/components, generate four review states from the same layout function, then rebuild the layered Photoshop document through the existing manifest → JSX → Photoshop pipeline. Do not change `UGameXXKMetaShopWidget`, UE assets, runtime imports, or the in-progress PIE acceptance probe in this plan.

**Tech Stack:** Python 3, Pillow, JSON manifests, existing `scripts/ui_psd_pipeline/build-psd.js`, Photoshop JSX/COM runner, existing UI Master validation scripts.

**Testing policy:** The user explicitly waived TDD for art/UI visual work. Do not add red-first tests. Run the existing package tests and post-build structural/visual checks after each relevant art-pipeline change.

---

## File map

- Modify `scripts/gamexxk_ui_master_pages.py`: authoritative shop layout, semantic state groups, five real navigation icons, hero identity, and copper-coin presentation.
- Modify `scripts/build_gamexxk_ui_master.py`: emit shop review previews and support a PSD-review build that does not refresh `RuntimeAssets`.
- Modify `scripts/ui_psd_pipeline/build-psd.js`: preserve hidden text-layer state when creating the PSD.
- Modify `scripts/test_gamexxk_ui_master_build.py`: post-build contract checks for four review states and the review manifest; this is verification after visual implementation, not TDD.
- Create `SourceArt/UI/PSD/gamexxk-v4/ui-master/meta-shop-component-map.json`: component provenance and transform record.
- Create `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MetaShop/*.png`: four 1920×1080 review states.
- Create `SourceArt/UI/PSD/gamexxk-v4/ui-master/meta-shop-review-manifest.json`: review-state filenames and semantic layer groups.
- Regenerate `SourceArt/UI/PSD/gamexxk-v4/ui-master/Previews/07_商店交易.png`, `master-manifest.json`, `compose.jsx`, and `GameXXK_UI_Master_ContactSheet.png`.
- Regenerate `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd` and its `.validation.json` through Photoshop.
- Do not modify or stage `SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeAssets/` or `runtime-assets-manifest.json`.

### Task 1: Lock the existing component-to-layout mapping

**Files:**
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/meta-shop-component-map.json`
- Modify: `docs/superpowers/specs/2026-08-05-meta-shop-psd-design.md`

- [ ] **Step 1: Confirm the five-navigation correction is present in the approved spec**

Run:

```powershell
rg -n "五个真实主导航|不伪造额外商店导航|resource_coin" docs/superpowers/specs/2026-08-05-meta-shop-psd-design.md
```

Expected: all three constraints are found; the spec contains no instruction to invent a shop navigation button.

- [ ] **Step 2: Create the component provenance map**

Create the JSON with this exact top-level structure and entries:

```json
{
  "version": 1,
  "page": "07_商店交易",
  "canvas": [1920, 1080],
  "reference": "codex-clipboard-80d2c217-6fe9-444d-aeac-4e5ff568bec1.png",
  "runtimeMigrationApproved": false,
  "components": {
    "fullShell": {
      "source": "../calibration-v2/Generated/hero_backpack_large_panel_clean.png",
      "usage": "direct_full_canvas",
      "box": [0, 0, 1920, 1080]
    },
    "heroPortrait": {
      "source": "../calibration-v2/Content/hero_portrait.png",
      "usage": "contain_canvas",
      "box": [48, 32, 132, 132]
    },
    "navigation": {
      "sources": [
        "../calibration-v2/Content/nav_backpack.png",
        "../calibration-v2/Content/nav_companion.png",
        "../calibration-v2/Content/nav_codex.png",
        "../calibration-v2/Content/nav_scroll.png",
        "../calibration-v2/Content/nav_route.png"
      ],
      "usage": "contain_canvas_on_existing_shell_discs",
      "boxes": [[52, 218, 86, 86], [52, 368, 86, 86], [52, 518, 86, 86], [52, 668, 86, 86], [52, 818, 86, 86]]
    },
    "currency": {
      "source": "../calibration-v2/Content/resource_coin.png",
      "usage": "contain_canvas",
      "topBarBox": [1500, 46, 42, 42],
      "cardIconSize": [22, 22],
      "purchaseIconSize": [28, 28]
    },
    "productSlot": {
      "source": "../calibration-v2/Components/inventory_slot_r1_c1.png",
      "usage": "scaled_component"
    },
    "detailSlot": {
      "source": "../calibration-v2/Components/detail_item_slot.png",
      "usage": "scaled_component",
      "box": [1370, 330, 220, 220]
    },
    "selectionInk": {
      "source": "../calibration-v2/Content/category_selected_ink.png",
      "usage": "scaled_component"
    },
    "purchaseButton": {
      "source": "../calibration-v2/Components/tab_02_equipment_selected.png",
      "usage": "nine_slice_candidate",
      "box": [1380, 870, 210, 72]
    }
  }
}
```

- [ ] **Step 3: Validate every mapped source exists and no runtime asset is referenced**

Run a focused Python check:

```powershell
@'
import json
from pathlib import Path
p = Path("SourceArt/UI/PSD/gamexxk-v4/ui-master/meta-shop-component-map.json")
d = json.loads(p.read_text(encoding="utf-8"))
assert d["runtimeMigrationApproved"] is False
assert "RuntimeAssets" not in p.read_text(encoding="utf-8")
assert len(d["components"]["navigation"]["sources"]) == 5
base = p.parent
sources = []
for value in d["components"].values():
    if "source" in value:
        sources.append(value["source"])
    sources.extend(value.get("sources", []))
missing = [source for source in sources if not (base / source).resolve().is_file()]
assert not missing, missing
print("component map ok")
'@ | python -
```

Expected: `component map ok`.

- [ ] **Step 4: Commit the mapping and corrected spec**

```powershell
git add -- docs/superpowers/specs/2026-08-05-meta-shop-psd-design.md SourceArt/UI/PSD/gamexxk-v4/ui-master/meta-shop-component-map.json
git commit -m "docs: lock meta shop PSD component mapping"
```

### Task 2: Add hidden-state support without changing the visual language

**Files:**
- Modify: `scripts/gamexxk_ui_master_pages.py`
- Modify: `scripts/ui_psd_pipeline/build-psd.js`

- [ ] **Step 1: Add a `visible` field to image and text builder methods**

Update `PageBuilder.add_image`, `add_component`, `add_text`, and `add_centered_text` so hidden PSD state layers are recorded but not painted into the visible PNG preview. The implementation must follow this shape:

```python
def add_image(
    self,
    name: str,
    path: Path,
    box: tuple[int, int, int, int],
    subgroup: str,
    *,
    fit_mode: str = "stretch",
    opacity: int = 255,
    visible: bool = True,
) -> None:
    x, y, width, height = box
    with Image.open(path) as opened:
        source = opened.convert("RGBA")
    if fit_mode == "contain_canvas":
        scale = min(width / source.width, height / source.height)
        rendered = source.resize(
            (max(1, round(source.width * scale)), max(1, round(source.height * scale))),
            Image.Resampling.LANCZOS,
        )
        paste_x = x + (width - rendered.width) // 2
        paste_y = y + (height - rendered.height) // 2
    elif fit_mode == "stretch":
        rendered = source.resize((width, height), Image.Resampling.LANCZOS)
        paste_x, paste_y = x, y
    else:
        rendered = source
        paste_x, paste_y = x, y
    if opacity < 255:
        alpha = rendered.getchannel("A").point(lambda value: value * opacity // 255)
        rendered.putalpha(alpha)
    if visible:
        self.canvas.alpha_composite(rendered, (paste_x, paste_y))
    self._layer_index += 1
    layer_record = {
        "name": f"{self._layer_index:03d}_{name}",
        "path": self._manifest_path(path),
        "x": x,
        "y": y,
        "width": width,
        "height": height,
        "fitMode": fit_mode,
        "group": f"{self.group}/{subgroup}",
        "visible": visible,
    }
```

For text, add `visible: bool = True`, draw only when visible, and include `"visible": visible` in every text-layer record. `add_centered_text` must forward the same value to `add_text`.

Add `visible: bool = True` to `add_component` and pass it directly to `add_image`:

```python
def add_component(
    self,
    key: str,
    box: tuple[int, int, int, int],
    subgroup: str,
    *,
    name: str | None = None,
    visible: bool = True,
) -> None:
    record = self.components[key]
    self.add_image(
        name or key,
        self.asset_root / record["file"],
        box,
        subgroup,
        fit_mode="stretch",
        visible=visible,
    )
```

- [ ] **Step 2: Preserve hidden text layers in the Photoshop composer**

In `scripts/ui_psd_pipeline/build-psd.js`, add this line after moving each created text layer into its group:

```javascript
layer.visible = item.visible !== false;
```

Image layers already use the same rule; do not change image behavior beyond the Python builder change.

- [ ] **Step 3: Run existing non-visual pipeline tests**

Run:

```powershell
python -m unittest scripts.test_gamexxk_ui_master_build scripts.test_gamexxk_ui_master_validation
```

Expected: all existing tests pass.

- [ ] **Step 4: Commit hidden-state support**

```powershell
git add -- scripts/gamexxk_ui_master_pages.py scripts/ui_psd_pipeline/build-psd.js
git commit -m "feat: preserve UI Master review state layers"
```

### Task 3: Recompose the selected shop page from the current kit

**Files:**
- Modify: `scripts/gamexxk_ui_master_pages.py`

- [ ] **Step 1: Define the shop review state and shared layout constants**

Add these constants near `META_SHOP_PRODUCTS`:

```python
META_SHOP_REVIEW_STATES = (
    "selected",
    "insufficient_funds",
    "confirmation",
    "result",
)

META_SHOP_NAVIGATION = (
    ("nav_backpack", "nav_backpack.png", (52, 218, 86, 86)),
    ("nav_companion", "nav_companion.png", (52, 368, 86, 86)),
    ("nav_codex", "nav_codex.png", (52, 518, 86, 86)),
    ("nav_task", "nav_scroll.png", (52, 668, 86, 86)),
    ("nav_route", "nav_route.png", (52, 818, 86, 86)),
)

META_SHOP_CARD_POSITIONS = (
    (410, 300), (630, 300), (850, 300), (1070, 300),
    (520, 610), (740, 610), (960, 610),
)
```

- [ ] **Step 2: Build the real identity, navigation, and copper-coin shell**

Create `_add_meta_shop_global_shell(builder)` and make it add exactly:

```python
def _add_meta_shop_global_shell(builder: PageBuilder) -> None:
    builder.add_image("approved_v2_shop_shell", V2_LARGE_PANEL, (0, 0, 1920, 1080), "10_Background")
    builder.add_image("hero_portrait", V2_CONTENT / "hero_portrait.png", (48, 32, 132, 132), "20_GlobalShell", fit_mode="contain_canvas")
    builder.add_text("主角  Lv. 1", (198, 50), 26, "20_GlobalShell", bold=True)
    builder.add_text("行旅者", (198, 88), 19, "20_GlobalShell", color=MUTED)
    builder.add_text("战力 33", (198, 122), 20, "20_GlobalShell", bold=True)
    for name, filename, box in META_SHOP_NAVIGATION:
        builder.add_image(name, V2_CONTENT / filename, box, "20_GlobalShell", fit_mode="contain_canvas")
    builder.add_image("top_currency_coin", V2_CONTENT / "resource_coin.png", (1500, 46, 42, 42), "20_GlobalShell", fit_mode="contain_canvas")
    builder.add_text("500", (1555, 51), 27, "20_GlobalShell", bold=True)
```

Do not add a sixth navigation icon and do not invent a shop icon.

- [ ] **Step 3: Replace text prices with coin icon plus number**

Create this helper and use it for all seven cards and the purchase button:

```python
def _add_coin_price(
    builder: PageBuilder,
    value: str,
    icon_box: tuple[int, int, int, int],
    text_position: tuple[int, int],
    subgroup: str,
    *,
    text_size: int,
    bold: bool = False,
    visible: bool = True,
) -> None:
    builder.add_image(
        f"coin_{subgroup.replace('/', '_')}_{value}",
        V2_CONTENT / "resource_coin.png",
        icon_box,
        subgroup,
        fit_mode="contain_canvas",
        visible=visible,
    )
    builder.add_text(value, text_position, text_size, subgroup, bold=bold, visible=visible)
```

For each card at `(x, y)`, use coin box `(x + 55, y + 218, 22, 22)` and text position `(x + 82, y + 217)`. For the purchase action, use coin box `(1475, 890, 28, 28)` and text position `(1510, 887)`.

- [ ] **Step 4: Rebuild `_page_meta_shop_v2` around semantic groups**

The function signature must become:

```python
def _page_meta_shop_v2(
    builder: PageBuilder,
    *,
    review_state: str = "selected",
    include_hidden_states: bool = True,
) -> None:
```

It must:

- reject a state not in `META_SHOP_REVIEW_STATES` with `ValueError`;
- call `_add_meta_shop_global_shell` first;
- keep title `新商店` and subtitle `套装装备包与伙伴包` in `30_ShopPaper`;
- render the seven product slots/icons/names in `40_ProductGrid` using `META_SHOP_CARD_POSITIONS`;
- render all seven prices through `_add_coin_price` and contain no visible `金` character;
- render the selected product slot and icon in `50_ProductDetail`;
- render the exact description, level, `普通 70% · 稀有 25% · 珍稀 5%`, and six possible equipment parts as editable text;
- add state-specific layers through `_add_meta_shop_state(builder, state, visible=...)`;
- when `include_hidden_states` is true, add the other three state groups with `visible=False` so Photoshop contains all four states while the main preview shows only `selected`.

Use this exact state loop at the end of the function:

```python
states = META_SHOP_REVIEW_STATES if include_hidden_states else (review_state,)
for state in states:
    _add_meta_shop_state(builder, state, visible=state == review_state)
```

Use these semantic groups exactly:

```text
07_商店交易/10_Background
07_商店交易/20_GlobalShell
07_商店交易/30_ShopPaper
07_商店交易/40_ProductGrid
07_商店交易/50_ProductDetail
07_商店交易/71_State_Selected
07_商店交易/72_State_InsufficientFunds
07_商店交易/73_State_Confirmation
07_商店交易/74_State_Result
```

- [ ] **Step 5: Build the four state treatments with existing components**

Implement `_add_meta_shop_state` with these exact visual rules:

```python
def _add_meta_shop_state(builder: PageBuilder, state: str, *, visible: bool) -> None:
    groups = {
        "selected": "71_State_Selected",
        "insufficient_funds": "72_State_InsufficientFunds",
        "confirmation": "73_State_Confirmation",
        "result": "74_State_Result",
    }
    subgroup = groups[state]
    builder.add_image(
        f"{state}_selection_ink",
        V2_CONTENT / "category_selected_ink.png",
        (398, 276, 194, 66),
        subgroup,
        visible=visible,
    )
    if state == "selected":
        builder.add_image("purchase_button", V2_COMPONENTS / "tab_02_equipment_selected.png", (1380, 870, 210, 72), subgroup, visible=visible)
        builder.add_text("购买", (1418, 889), 23, subgroup, bold=True, visible=visible)
        _add_coin_price(builder, "100", (1475, 890, 28, 28), (1510, 887), subgroup, text_size=23, bold=True, visible=visible)
        builder.add_text("点击后再次确认", (1412, 955), 16, subgroup, color=MUTED, visible=visible)
    elif state == "insufficient_funds":
        builder.add_component("button_disabled", (1380, 870, 210, 72), subgroup, name="purchase_disabled", visible=visible)
        builder.add_text("购买", (1418, 889), 23, subgroup, bold=True, visible=visible)
        _add_coin_price(builder, "100", (1475, 890, 28, 28), (1510, 887), subgroup, text_size=23, bold=True, visible=visible)
        builder.add_centered_text("铜钱不足，还需 50", (1330, 952, 310, 28), 16, subgroup, color=(156, 69, 45, 255), visible=visible)
    elif state == "confirmation":
        builder.add_component("tooltip_panel", (1285, 725, 400, 245), subgroup, name="confirmation_panel", visible=visible)
        builder.add_centered_text("确认购买破军装备包？", (1310, 752, 350, 36), 23, subgroup, bold=True, visible=visible)
        builder.add_text("将消耗", (1350, 806), 19, subgroup, visible=visible)
        _add_coin_price(builder, "100", (1435, 806, 24, 24), (1465, 803), subgroup, text_size=20, bold=True, visible=visible)
        builder.add_component("button_primary", (1320, 868, 150, 58), subgroup, name="confirm_purchase", visible=visible)
        builder.add_centered_text("确认购买", (1320, 868, 150, 58), 19, subgroup, bold=True, visible=visible)
        builder.add_component("button_normal", (1490, 868, 150, 58), subgroup, name="cancel_purchase", visible=visible)
        builder.add_centered_text("取消", (1490, 868, 150, 58), 19, subgroup, visible=visible)
    elif state == "result":
        builder.add_component("tooltip_panel", (1285, 725, 400, 245), subgroup, name="result_panel", visible=visible)
        builder.add_centered_text("购买成功", (1310, 752, 350, 38), 25, subgroup, bold=True, visible=visible)
        builder.add_centered_text("获得：破军·武器（普通）", (1310, 812, 350, 32), 19, subgroup, visible=visible)
        builder.add_component("button_primary", (1405, 875, 160, 58), subgroup, name="accept_result", visible=visible)
        builder.add_centered_text("收下", (1405, 875, 160, 58), 19, subgroup, bold=True, visible=visible)
```

Every image and text created by a state must receive the `visible` argument and be placed under only that state's semantic group.

- [ ] **Step 6: Generate a temporary selected preview and inspect it**

Run:

```powershell
python scripts/build_gamexxk_ui_master.py --output-root tmp/meta-shop-psd-review
```

Expected: `tmp/meta-shop-psd-review/Previews/07_商店交易.png` is 1920×1080 and visually contains the real hero identity, five real navigation icons, seven product cards, right detail, and only copper-coin icon prices.

- [ ] **Step 7: Commit the selected-page composition**

```powershell
git add -- scripts/gamexxk_ui_master_pages.py
git commit -m "feat: compose meta shop PSD from the V2 kit"
```

### Task 4: Generate four review states without exporting RuntimeAssets

**Files:**
- Modify: `scripts/build_gamexxk_ui_master.py`
- Modify: `scripts/test_gamexxk_ui_master_build.py`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/meta-shop-review-manifest.json`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MetaShop/07_商店交易_*.png`

- [ ] **Step 1: Add a focused review-preview builder**

Add `build_meta_shop_review_previews` to `scripts/gamexxk_ui_master_pages.py`:

```python
def build_meta_shop_review_previews(
    output_root: Path,
    *,
    asset_root: Path,
    package_root: Path,
) -> list[dict]:
    output_root.mkdir(parents=True, exist_ok=True)
    component_records = build_component_assets(asset_root)
    records = []
    filenames = {
        "selected": "07_商店交易_默认选中.png",
        "insufficient_funds": "07_商店交易_铜钱不足.png",
        "confirmation": "07_商店交易_二次确认.png",
        "result": "07_商店交易_购买结果.png",
    }
    for state in META_SHOP_REVIEW_STATES:
        builder = PageBuilder("07_商店交易", asset_root, component_records, package_root)
        _page_meta_shop_v2(builder, review_state=state, include_hidden_states=False)
        filename = filenames[state]
        builder.save(output_root / filename)
        records.append({"state": state, "file": f"Review/MetaShop/{filename}", "size": [1920, 1080]})
    return records
```

Import `build_meta_shop_review_previews` alongside `build_page_previews` in `scripts/build_gamexxk_ui_master.py`, call it with `output_root / "Review/MetaShop"`, `assets_root`, and `output_root`, and keep the returned records in `review_records`.

- [ ] **Step 2: Add a PSD-review-only build option**

Change `build_package` to accept `update_runtime_assets: bool = True`. When false, do not call `_build_runtime_assets`, do not write `RuntimeAssets`, and do not rewrite `runtime-assets-manifest.json`. Use this exact branch so both modes keep a defined report payload:

```python
if update_runtime_assets:
    runtime_manifest = _build_runtime_assets(output_root, component_records)
else:
    runtime_path = output_root / "runtime-assets-manifest.json"
    runtime_manifest = (
        json.loads(runtime_path.read_text(encoding="utf-8"))
        if runtime_path.is_file()
        else {"assets": {}}
    )
```

Always call `build_meta_shop_review_previews`, and write:

```python
review_manifest = {
    "version": 1,
    "page": "07_商店交易",
    "runtimeMigrationApproved": False,
    "states": review_records,
}
_write_json(output_root / "meta-shop-review-manifest.json", review_manifest)
```

Add `"metaShopReviewStates": len(review_records)` to the `build_package` return payload.

Add CLI flag:

```python
parser.add_argument("--psd-review-only", action="store_true")
report = build_package(args.output_root, update_runtime_assets=not args.psd_review_only)
```

- [ ] **Step 3: Add post-build assertions**

Extend `test_cli_builds_master_manifest_previews_and_contact_sheet` after its current assertions:

```python
review = json.loads((output / "meta-shop-review-manifest.json").read_text(encoding="utf-8"))
self.assertFalse(review["runtimeMigrationApproved"])
self.assertEqual(
    ["selected", "insufficient_funds", "confirmation", "result"],
    [record["state"] for record in review["states"]],
)
for record in review["states"]:
    with Image.open(output / record["file"]) as preview:
        self.assertEqual((1920, 1080), preview.size)
```

- [ ] **Step 4: Run package verification**

Run:

```powershell
python -m unittest scripts.test_gamexxk_ui_master_build scripts.test_gamexxk_ui_master_validation scripts.test_gamexxk_ui_master_contract
```

Expected: all tests pass.

- [ ] **Step 5: Commit review-state generation**

```powershell
git add -- scripts/build_gamexxk_ui_master.py scripts/gamexxk_ui_master_pages.py scripts/test_gamexxk_ui_master_build.py
git commit -m "feat: generate meta shop PSD review states"
```

### Task 5: Rebuild and validate the layered PSD

**Files:**
- Regenerate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Previews/07_商店交易.png`
- Regenerate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MetaShop/*.png`
- Regenerate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/meta-shop-review-manifest.json`
- Regenerate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/master-manifest.json`
- Regenerate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/compose.jsx`
- Regenerate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/GameXXK_UI_Master_ContactSheet.png`
- Regenerate: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd`
- Regenerate: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.validation.json`

- [ ] **Step 1: Build the canonical art-review package without touching RuntimeAssets**

Run:

```powershell
python scripts/build_gamexxk_ui_master.py --output-root SourceArt/UI/PSD/gamexxk-v4/ui-master --psd-review-only
```

Expected: JSON reports 18 pages and four meta-shop review states. Compare `git status --short -- SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeAssets SourceArt/UI/PSD/gamexxk-v4/ui-master/runtime-assets-manifest.json`; it must show no new modifications caused by this task.

- [ ] **Step 2: Regenerate the Photoshop JSX from the updated manifest**

Run:

```powershell
node scripts/ui_psd_pipeline/build-psd.js --root SourceArt/UI/PSD/gamexxk-v4/ui-master --manifest master-manifest.json
```

Expected: `compose.jsx` is regenerated and contains `71_State_Selected`, `72_State_InsufficientFunds`, `73_State_Confirmation`, and `74_State_Result`.

- [ ] **Step 3: Build the editable PSD through Photoshop**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/ui_psd_pipeline/run-photoshop.ps1 -Root SourceArt/UI/PSD/gamexxk-v4/ui-master
```

Expected: Photoshop writes `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd` and the matching validation JSON without flattening editable text or semantic groups.

- [ ] **Step 4: Run structural validation**

Run:

```powershell
python scripts/validate_gamexxk_ui_master.py --package-root SourceArt/UI/PSD/gamexxk-v4/ui-master --project-root .
```

Expected: JSON contains `"ok": true`, 18 top-level groups, matching image/text layer counts, and a successful text round trip.

- [ ] **Step 5: Verify the shop page contains editable semantic groups**

Run:

```powershell
rg -n "20_GlobalShell|40_ProductGrid|50_ProductDetail|71_State_Selected|72_State_InsufficientFunds|73_State_Confirmation|74_State_Result" SourceArt/UI/PSD/gamexxk-v4/ui-master/master-manifest.json
```

Expected: every group is present. Also verify the shop page's text-layer records contain no price string ending in `金`.

### Task 6: Visual QA and user handoff gate

**Files:**
- Review: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Previews/07_商店交易.png`
- Review: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MetaShop/07_商店交易_默认选中.png`
- Review: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MetaShop/07_商店交易_铜钱不足.png`
- Review: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MetaShop/07_商店交易_二次确认.png`
- Review: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MetaShop/07_商店交易_购买结果.png`

- [ ] **Step 1: Inspect all four state previews at original resolution**

Use local image inspection and verify:

- the composition matches the supplied reference;
- the hero identity area is populated and legible;
- exactly five real navigation icons appear;
- no invented shop navigation icon appears;
- the top bar, seven product cards, and purchase action use the copper-coin icon;
- no visible price uses `金` or a yuanbao icon;
- the 4+3 product grid is evenly aligned;
- all text is centered or aligned consistently;
- insufficient-funds, confirmation, and result states stay inside the same Master shell;
- no state is accidentally layered over another in a review PNG.

- [ ] **Step 2: Check only intended source/art files before staging**

Run:

```powershell
git status --short -- scripts/gamexxk_ui_master_pages.py scripts/build_gamexxk_ui_master.py scripts/ui_psd_pipeline/build-psd.js scripts/test_gamexxk_ui_master_build.py SourceArt/UI/PSD/gamexxk-v4/ui-master docs/superpowers/specs/2026-08-05-meta-shop-psd-design.md
```

Do not stage unrelated generated art, existing user-tuned assets, `RuntimeAssets`, UE content, Task 9 probe files, or `Source/GameXXK` changes.

- [ ] **Step 3: Commit the reviewed PSD source package**

Stage only the tracked Master outputs and the new review records:

```powershell
git add -- SourceArt/UI/PSD/gamexxk-v4/ui-master/Previews/07_商店交易.png SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MetaShop SourceArt/UI/PSD/gamexxk-v4/ui-master/meta-shop-review-manifest.json SourceArt/UI/PSD/gamexxk-v4/ui-master/master-manifest.json SourceArt/UI/PSD/gamexxk-v4/ui-master/GameXXK_UI_Master_ContactSheet.png
git commit -m "art: rebuild meta shop PSD review"
```

- [ ] **Step 4: Stop and request user approval**

Deliver clickable links to the PSD and four review images. State explicitly:

```text
PSD review package is ready. No RuntimeAssets were cut or imported and UE was not modified. Please approve or mark changes on these previews; UE migration starts only after approval.
```

Do not resume Task 9 or modify the game UI in the same approval turn.
