# PSD Authoritative Town Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild the five approved town-facing UI surfaces from the reviewed PSD v2 pipeline, preserve their working game behavior, and produce a validated project-local editable PSD.

**Architecture:** Store the portable PSD source package under `SourceArt/UI/PSD/town-v2`, keep visual coordinates and semantic asset IDs in JSON, and import selected transparent atoms to a new `/Game/GameXXK/UI/Town/Textures/PSD` tree. A small C++ style module owns paths, semantic button states, and the 1024 logical design transform; HUD, task, inventory, companion, and character widgets consume it without changing subsystem commands or state mutation rules.

**Tech Stack:** Python 3 standard library and Pillow, Node.js, Photoshop JSX/COM, Unreal Engine 5.8 C++/UMG, UE MCP import/save, UBT cold compile, Unreal automation tests.

---

## File structure

| Path | Responsibility |
| --- | --- |
| `SourceArt/UI/PSD/town-v2/generated/` | Copied v2 source atlases, retained as immutable visual inputs. |
| `SourceArt/UI/PSD/town-v2/clean_assets/` | Per-component transparent PNG output from the portable clean/cut script. |
| `SourceArt/UI/PSD/town-v2/manifest.json` | Image layers, editable text layers, 1024 logical coordinates, 4K output declaration. |
| `SourceArt/UI/PSD/town-v2/semantic-map.json` | Maps manifest layer names to asset groups, UE texture names, button semantics, and nine-slice margins. |
| `scripts/ui_psd_pipeline/rebuild_clean_assets.py` | Project-root-relative v2 clean/cut/manifest rebuild. |
| `scripts/ui_psd_pipeline/build-psd.js` | Writes SVG text plus `compose.jsx` from the manifest. |
| `scripts/ui_psd_pipeline/run-photoshop.ps1` | Runs the generated JSX and creates Photoshop validation output. |
| `scripts/validate_town_psd_package.py` | Validates source layout, layer paths, semantic coverage, text/image separation, and 4K contract. |
| `scripts/test_town_psd_package.py` | Python regression tests for the package validator. |
| `Content/Python/gamexxk_import_town_psd_v2_assets.py` | Imports named PSD atoms into the isolated UE destination with UI texture settings. |
| `Content/Python/gamexxk_validate_town_psd_v2_assets.py` | UE-side audit that verifies all selected asset paths exist and have correct texture settings. |
| `Source/GameXXK/Public/UI/GameXXKTownPsdStyle.h` | Public `FGameXXKTownPsdStyle` asset IDs, semantic button enum, and 1024 design transform. |
| `Source/GameXXK/Private/UI/GameXXKTownPsdStyle.cpp` | Texture loading, brush construction, nine-slice margins, and design-to-canvas helpers. |
| `Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp` | Replaces old fixed asset paths and positions with style-module semantic assets and PSD logical layout. |
| `Source/GameXXK/Private/UI/GameXXKTaskPanelWidget.cpp` | Uses PSD tabs, rows, reward atoms and teal primary action brushes. |
| `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp` | Uses PSD frame, slots, five tabs, neutral/teal/ochre semantic actions. |
| `Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp` | Uses PSD companion frame/cards/tabs while retaining 12 slots, cards, progression, recruitment and tooltip behavior. |
| `Source/GameXXK/Private/UI/GameXXKCharacterPanelWidget.cpp` | Presents existing summary/equipment data inside PSD character atoms. |
| `Source/GameXXK/Private/Tests/GameXXKTownPsdStyleTest.cpp` | UE automation tests for semantic resource paths, button grammar, design transform, and no-duplicate-overlay contract. |
| `Source/GameXXK/Private/Tests/GameXXKCompanionRosterWidgetTest.cpp` | Updates existing PSD path assertions without weakening the 12-slot, card pool, and tooltip tests. |

### Task 1: Add the package-contract validator before porting assets

**Files:**
- Create: `scripts/validate_town_psd_package.py`
- Create: `scripts/test_town_psd_package.py`
- Create: `SourceArt/UI/PSD/town-v2/semantic-map.json`
- Test: `scripts/test_town_psd_package.py`

- [ ] **Step 1: Write failing package-contract tests**

Create `scripts/test_town_psd_package.py` with an isolated temporary package. The tests must call the public `validate_package(root: Path) -> list[str]` function and assert these exact failure conditions:

```python
def test_rejects_non_4k_manifest(tmp_path: Path) -> None:
    write_package(tmp_path, width=1024, height=1024, scale=1)
    assert "document must be 4096x4096 at scale 4" in validate_package(tmp_path)

def test_rejects_text_baked_into_image_layer(tmp_path: Path) -> None:
    write_package(tmp_path, image_name="按钮_确认_文字")
    assert "image layer name must not declare baked runtime text" in validate_package(tmp_path)

def test_rejects_missing_semantic_button_family(tmp_path: Path) -> None:
    write_package(tmp_path, semantic_buttons=["neutral", "primary"])
    assert "missing semantic button family: destructive" in validate_package(tmp_path)

def test_accepts_complete_minimum_package(tmp_path: Path) -> None:
    write_package(tmp_path, semantic_buttons=["neutral", "primary", "destructive"])
    assert validate_package(tmp_path) == []
```

`write_package` must create `generated/`, `clean_assets/`, `manifest.json`, and `semantic-map.json`; every image record references an existing transparent one-pixel PNG created by the test fixture.

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
python scripts/test_town_psd_package.py
```

Expected: failure because `validate_town_psd_package.py` does not exist.

- [ ] **Step 3: Implement the validator with explicit rules**

Implement only these public helpers:

```python
def load_json(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8-sig"))

def validate_package(root: Path) -> list[str]:
    errors: list[str] = []
    manifest = load_json(root / "manifest.json")
    document = manifest.get("document", {})
    if (document.get("width"), document.get("height"), document.get("scale")) != (4096, 4096, 4):
        errors.append("document must be 4096x4096 at scale 4")
    for layer in manifest.get("imageLayers", []):
        if "文字" in str(layer.get("name", "")):
            errors.append("image layer name must not declare baked runtime text")
        if not (root / str(layer.get("path", ""))).is_file():
            errors.append(f"missing image layer file: {layer.get('path', '')}")
    required = {"neutral", "primary", "destructive"}
    present = {str(item.get("buttonSemantic", "")) for item in load_json(root / "semantic-map.json").get("assets", [])}
    for semantic in sorted(required.difference(present)):
        errors.append(f"missing semantic button family: {semantic}")
    return errors
```

The implementation also checks that each semantic record has a unique `ueAssetName`, that each `manifestLayer` exists once, that `textLayers` have nonempty `name`, `text`, `font`, `color`, `x`, `y`, and `fontSize`, and that `outputPsd` resolves under `outputs/UI_PSD/`.

- [ ] **Step 4: Add the production semantic mapping**

Create `semantic-map.json` with records for these exact groups and semantics:

```json
{
  "assets": [
    {"manifestLayer":"按钮_详细属性","group":"Controls","ueAssetName":"T_TownPsd_ButtonNeutral","buttonSemantic":"neutral","nineSlice":[12,8,12,8]},
    {"manifestLayer":"按钮_任务_2","group":"Controls","ueAssetName":"T_TownPsd_ButtonPrimary","buttonSemantic":"primary","nineSlice":[12,8,12,8]},
    {"manifestLayer":"按钮_背包分解","group":"Controls","ueAssetName":"T_TownPsd_ButtonDestructive","buttonSemantic":"destructive","nineSlice":[12,8,12,8]}
  ]
}
```

Extend this same file with the selected HUD, Nav, Character, Companion, Task, and Backpack records. Each record identifies exactly one `manifestLayer`, one `cleanAssetFile`, one UE group and one `ueAssetName`; fixed decorative labels may be mapped, but runtime labels are not permitted as images.

- [ ] **Step 5: Run package tests and validate the real package only after it exists**

Run:

```powershell
python scripts/test_town_psd_package.py
python scripts/validate_town_psd_package.py --root SourceArt/UI/PSD/town-v2
```

Expected: the unit tests pass; the production command is run after Task 2 and reports `ok: true`, 4096×4096, scale 4, and all three button families.

- [ ] **Step 6: Commit**

```powershell
git add scripts/validate_town_psd_package.py scripts/test_town_psd_package.py SourceArt/UI/PSD/town-v2/semantic-map.json
git commit -m "test: validate PSD town package contract"
```

### Task 2: Port and prove the v2 PSD pipeline inside the project

**Files:**
- Create: `SourceArt/UI/PSD/town-v2/generated/*.png`
- Create: `SourceArt/UI/PSD/town-v2/manifest.seed.json`
- Create: `scripts/ui_psd_pipeline/rebuild_clean_assets.py`
- Create: `scripts/ui_psd_pipeline/build-psd.js`
- Create: `scripts/ui_psd_pipeline/run-photoshop.ps1`
- Create: `SourceArt/UI/PSD/town-v2/clean_assets/*.png`
- Create: `SourceArt/UI/PSD/town-v2/manifest.json`
- Create: `SourceArt/UI/PSD/town-v2/previews/town-clean-preview.png`
- Create: `outputs/UI_PSD/GameXXK_Town_4K.psd`
- Create: `outputs/UI_PSD/GameXXK_Town_4K.validation.json`
- Test: `scripts/test_town_psd_package.py`

- [ ] **Step 1: Copy only the reviewed v2 inputs and scripts**

Use the following source directory exactly, read-only:

```text
C:\Users\shxuw\Downloads\nw-studio-nwueball-https-github-com\nw-studio-nwueball-https-github-com\work\psd_rebuild
```

Copy `generated_v2/*.png`, `manifest.json`, `rebuild_clean_assets.py`, `build-psd.js`, and `run-photoshop.ps1` into the paths above. Do not copy or invoke historical `prepare-assets.js`, `process_assets.py`, `split_assets.py`, or hard-coded `compose.jsx`.

- [ ] **Step 2: Run the validator to verify the unported package fails**

Run:

```powershell
python scripts/validate_town_psd_package.py --root SourceArt/UI/PSD/town-v2
```

Expected: failure for historical absolute paths and the missing project-local clean asset files.

- [ ] **Step 3: Make the clean/cut script project-root-relative**

In `rebuild_clean_assets.py`, replace external root discovery and output declarations with:

```python
PROJECT_ROOT = Path(__file__).resolve().parents[2]
PACKAGE_ROOT = PROJECT_ROOT / "SourceArt" / "UI" / "PSD" / "town-v2"
SOURCE = PACKAGE_ROOT / "generated"
OUT = PACKAGE_ROOT / "clean_assets"
MANIFEST_SEED = PACKAGE_ROOT / "manifest.seed.json"
MANIFEST = PACKAGE_ROOT / "manifest.json"
OUTPUT_PSD = PROJECT_ROOT / "outputs" / "UI_PSD" / "GameXXK_Town_4K.psd"
```

Keep the source’s `SCALE = 4`, `CANVAS = 4096`, alpha cleaning, grid split coordinates and source image ordering unchanged. Load from `MANIFEST_SEED`, replace every image layer path with `clean_assets/<zero-padded>.png`, set `document.outputPsd` to `OUTPUT_PSD.as_posix()`, and write UTF-8 JSON with `ensure_ascii=False`.

- [ ] **Step 4: Rebuild clean assets and inspect the manifest**

Run:

```powershell
python scripts/ui_psd_pipeline/rebuild_clean_assets.py
python scripts/validate_town_psd_package.py --root SourceArt/UI/PSD/town-v2
```

Expected: exactly 133 image layer files, 130 text records, `document.width=4096`, `document.height=4096`, `document.scale=4`, and no external `qlma` or Downloads path in `manifest.json`.

- [ ] **Step 5: Rebase the JSX generator and run its pre-Photoshop validation**

Set `root` in `build-psd.js` to `path.resolve(__dirname, "..", "..", "SourceArt", "UI", "PSD", "town-v2")` only if it is run from the project root; otherwise accept `--root` and derive `manifestPath`, `svg_text`, `compose.jsx`, and `validation.json` from that argument. Preserve its existing SVG `<text>` generation and Photoshop round-trip checks. Write `compose.jsx` under `SourceArt/UI/PSD/town-v2/` and require `document.outputPsd` to equal `outputs/UI_PSD/GameXXK_Town_4K.psd`.

Run:

```powershell
node scripts/ui_psd_pipeline/build-psd.js --root SourceArt/UI/PSD/town-v2
```

Expected: output declares 133 image layers, 130 text layers and 130 SVG files.

- [ ] **Step 6: Compose the PSD only through Photoshop and verify the round trip**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/ui_psd_pipeline/run-photoshop.ps1 -Root SourceArt/UI/PSD/town-v2
Get-Content outputs/UI_PSD/GameXXK_Town_4K.validation.json
```

Expected: Photoshop writes the PSD and validation JSON reports `width: 4096`, `height: 4096`, `expectedImageLayers: 133`, `expectedTextLayers: 130`, `actualTextLayers: 130`, and `textRoundTripMatch: true`. If Photoshop is unavailable, stop after JSX generation and record that the PSD round-trip is unverified.

- [ ] **Step 7: Commit**

```powershell
git add SourceArt/UI/PSD/town-v2 scripts/ui_psd_pipeline outputs/UI_PSD/GameXXK_Town_4K.psd outputs/UI_PSD/GameXXK_Town_4K.validation.json
git commit -m "feat: port validated town PSD pipeline"
```

### Task 3: Import PSD atoms to an isolated Unreal texture tree

**Files:**
- Create: `Content/Python/gamexxk_import_town_psd_v2_assets.py`
- Create: `Content/Python/gamexxk_validate_town_psd_v2_assets.py`
- Modify: `Content/Python/gamexxk_import_town_ui_assets.py`
- Test: `scripts/test_town_psd_package.py`

- [ ] **Step 1: Add a failing semantic-map/import selection test**

Extend `scripts/test_town_psd_package.py` so it imports `selected_imports` from `gamexxk_import_town_psd_v2_assets.py` through a source-only helper module. The test must expect the ordered groups:

```python
assert [item.group for item in selected_imports(semantic_map)] == [
    "HUD", "Nav", "Character", "Companion", "Task", "Backpack", "Controls"
]
assert {item.button_semantic for item in selected_imports(semantic_map)} >= {
    "neutral", "primary", "destructive"
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```powershell
python scripts/test_town_psd_package.py
```

Expected: failure because the source-only import selector does not exist.

- [ ] **Step 3: Implement import and validation scripts**

`gamexxk_import_town_psd_v2_assets.py` must read `SourceArt/UI/PSD/town-v2/semantic-map.json` and import every selected `clean_assets/<file>` into:

```text
/Game/GameXXK/UI/Town/Textures/PSD/<group>/<ueAssetName>
```

Use `unreal.AssetImportTask` and then set exactly these texture properties:

```python
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

The import script must refuse an absent `cleanAssetFile`, duplicate destination name, invalid button semantic or overwrite of any texture outside `/Game/GameXXK/UI/Town/Textures/PSD`. It may reimport only assets in this isolated destination. `gamexxk_validate_town_psd_v2_assets.py` must load all listed assets and emit JSON containing the asset path, dimensions, `lod_group`, `mip_gen_settings`, `never_stream`, and `has_alpha` result.

- [ ] **Step 4: Update the legacy town import guard**

In `gamexxk_import_town_ui_assets.py`, replace `HERO_DETAIL_036_SOURCE`’s external Downloads path with the matching project-local `SourceArt/UI/PSD/town-v2/clean_assets/036.png`. Preserve the SHA-256 check only after calculating the local source hash; do not let the legacy importer import any texture whose destination starts with `/Game/GameXXK/UI/Town/Textures/PSD`.

- [ ] **Step 5: Run the source tests and UE import/audit**

Run:

```powershell
python scripts/test_town_psd_package.py
python scripts/ue_mcp_smoke.py
python scripts/ue_mcp_client.py --script Content/Python/gamexxk_import_town_psd_v2_assets.py
python scripts/ue_mcp_client.py --script Content/Python/gamexxk_validate_town_psd_v2_assets.py
```

Expected: source tests pass; the import JSON lists every semantic-map atom; UE audit reports only `/Game/GameXXK/UI/Town/Textures/PSD/...` paths and the required UI texture settings.

- [ ] **Step 6: Save editor packages through MCP and commit**

Run the project MCP save operation. Then:

```powershell
git add Content/Python/gamexxk_import_town_psd_v2_assets.py Content/Python/gamexxk_validate_town_psd_v2_assets.py Content/Python/gamexxk_import_town_ui_assets.py
git commit -m "feat: import semantic town PSD atoms"
```

### Task 4: Introduce one C++ owner for PSD paths, semantic buttons, and logical layout

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKTownPsdStyle.h`
- Create: `Source/GameXXK/Private/UI/GameXXKTownPsdStyle.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKTownPsdStyleTest.cpp`
- Modify: `Source/GameXXK/GameXXK.Build.cs`
- Test: `Source/GameXXK/Private/Tests/GameXXKTownPsdStyleTest.cpp`

- [ ] **Step 1: Write failing UE automation tests**

Create `FGameXXKTownPsdStyleTest` registered as `GameXXK.UI.TownPsdStyle.Contract`. It must assert:

```cpp
TestEqual(TEXT("town design canvas width"), FGameXXKTownPsdStyle::LogicalWidth, 1024.0f);
TestEqual(TEXT("town design canvas height"), FGameXXKTownPsdStyle::LogicalHeight, 1024.0f);
TestTrue(TEXT("neutral button resolves to PSD tree"), FGameXXKTownPsdStyle::GetTexturePath(EGameXXKTownPsdAsset::ButtonNeutral).StartsWith(TEXT("/Game/GameXXK/UI/Town/Textures/PSD/Controls/")));
TestTrue(TEXT("primary button resolves to PSD tree"), FGameXXKTownPsdStyle::GetTexturePath(EGameXXKTownPsdAsset::ButtonPrimary).StartsWith(TEXT("/Game/GameXXK/UI/Town/Textures/PSD/Controls/")));
TestTrue(TEXT("destructive button resolves to PSD tree"), FGameXXKTownPsdStyle::GetTexturePath(EGameXXKTownPsdAsset::ButtonDestructive).StartsWith(TEXT("/Game/GameXXK/UI/Town/Textures/PSD/Controls/")));
TestNotEqual(TEXT("primary and destructive use different art"), FGameXXKTownPsdStyle::GetTexturePath(EGameXXKTownPsdAsset::ButtonPrimary), FGameXXKTownPsdStyle::GetTexturePath(EGameXXKTownPsdAsset::ButtonDestructive));
```

Add assertions that `DesignRect(18, 112, 90, 48, FVector2D(2048, 2048))` has top-left `(36, 224)` and size `(180, 96)`, and that a layout rect is never negative.

- [ ] **Step 2: Run the test to verify it fails**

Run the relevant Unreal automation filter from an editor built without Live Coding. Expected: the filter finds no `TownPsdStyle.Contract` test before the new source is compiled.

- [ ] **Step 3: Implement the style module**

Define these enums and methods:

```cpp
enum class EGameXXKTownPsdButtonSemantic : uint8 { Neutral, Primary, Destructive };
enum class EGameXXKTownPsdAsset : uint8 { HudProfile, HudCoin, NavTask, NavBackpack, CharacterFrame, CompanionFrame, TaskFrame, BackpackFrame, BackpackSlot, ButtonNeutral, ButtonPrimary, ButtonDestructive };

struct FGameXXKTownPsdStyle
{
    static constexpr float LogicalWidth = 1024.0f;
    static constexpr float LogicalHeight = 1024.0f;
    static FString GetTexturePath(EGameXXKTownPsdAsset Asset);
    static FSlateBrush MakeBrush(EGameXXKTownPsdAsset Asset, const FVector2D& Size, const FMargin& Margin = FMargin(0.0f));
    static FButtonStyle MakeButtonStyle(EGameXXKTownPsdButtonSemantic Semantic, const FVector2D& Size);
    static FSlateRect DesignRect(float X, float Y, float Width, float Height, const FVector2D& Viewport);
};
```

`MakeButtonStyle` must use neutral parchment for `Neutral`, teal for `Primary`, and ochre for `Destructive`; its hover and pressed brushes must preserve the same source texture while increasing/decreasing only a controlled tint/opacity. It must not reference `T_TownBackpack_ActionBlank`.

- [ ] **Step 4: Run the test and cold compile**

After saving all packages through MCP and closing the editor, run:

```powershell
& "D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" GameXXKEditor Win64 Development -Project="D:\UE5 demo\GameXXK\GameXXK.uproject" -NoHotReload
```

Then start the editor with its direct `.uproject` command and run `GameXXK.UI.TownPsdStyle.Contract`. Expected: compile success and all style contract assertions pass.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/GameXXK.Build.cs Source/GameXXK/Public/UI/GameXXKTownPsdStyle.h Source/GameXXK/Private/UI/GameXXKTownPsdStyle.cpp Source/GameXXK/Private/Tests/GameXXKTownPsdStyleTest.cpp
git commit -m "feat: centralize town PSD style grammar"
```

### Task 5: Migrate the persistent HUD and task panel without changing gameplay commands

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKTaskPanelWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKTownHudWidget.h`
- Modify: `Source/GameXXK/Public/UI/GameXXKTaskPanelWidget.h`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKTownPsdStyleTest.cpp`

- [ ] **Step 1: Add failing visual-contract assertions**

Extend `FGameXXKTownPsdStyleTest` and the town portion of `FGameXXKPlayerFlowWidgetTest` to assert that the HUD returns PSD resource paths for profile, task nav and backpack nav, and the task panel returns PSD paths for frame, tab, row and primary action. Add public test-only getters such as `GetTaskPanelFrameResourcePathForTest()` and `GetTaskPanelPrimaryActionResourcePathForTest()`; do not expose UMG internals as mutable production state.

- [ ] **Step 2: Run tests and verify the current resources fail the new contract**

Run the two UI automation filters. Expected: failures because current paths point to legacy HUD/Nav/Task locations.

- [ ] **Step 3: Replace HUD asset paths and coordinates through `FGameXXKTownPsdStyle`**

In `UGameXXKTownHudWidget::BuildProgrammaticLayout`, replace local `HudTextureRoot`, `NavTextureRoot`, `BackpackTextureRoot`, `CharacterTextureRoot`, and `CompanionTextureRoot` constants with `FGameXXKTownPsdStyle` assets. Place the persistent atoms from the manifest’s 1024 logical coordinates:

```text
profile: (17, 13)
left nav: task (18,112), backpack (18,178), refine (18,246), map (18,320), companion (18,391)
resources: x 563, 688, 806; mail x 928; settings x 970
```

Use `DesignRect` and viewport anchors instead of hard-coded 1920 coordinates. `HandleTaskClicked`, `HandleInventoryClicked`, `HandleCharacterClicked`, `HandleMapClicked`, `HandleCompanionClicked`, `HandleCompanionRosterClicked` and `CloseAuxiliaryPanels` remain the only command paths.

- [ ] **Step 4: Replace task panel art and action semantics**

In `UGameXXKTaskPanelWidget::BuildProgrammaticLayout`, apply the PSD task frame, categories, task-row visual, reward atoms and arrow. Use `MakeButtonStyle(Primary, ...)` only for existing track/go actions, use `Neutral` for close/back, and retain `SelectTaskCategory`, `TriggerTaskAction`, `RefreshTabVisuals`, `RebuildTaskList`, and `HandleCloseClicked` unchanged.

- [ ] **Step 5: Run UI tests, cold compile, and capture town screenshots**

Run the updated UI automation tests, save through MCP, use the cold UBT command, and open PIE. Capture 16:9 screenshots for HUD, task offers and accepted tasks. Expected: no duplicated old town overlay buttons, every displayed nav action opens the same destination as before, task action remains functional, and all asserted paths resolve under `/Game/GameXXK/UI/Town/Textures/PSD/`.

- [ ] **Step 6: Commit**

```powershell
git add Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp Source/GameXXK/Private/UI/GameXXKTaskPanelWidget.cpp Source/GameXXK/Public/UI/GameXXKTownHudWidget.h Source/GameXXK/Public/UI/GameXXKTaskPanelWidget.h Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKTownPsdStyleTest.cpp
git commit -m "feat: migrate town HUD and tasks to PSD v2"
```

### Task 6: Migrate the inventory and companion roster, preserving existing mechanics

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKCompanionRosterWidget.h`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCompanionRosterWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKTownPsdStyleTest.cpp`

- [ ] **Step 1: Add failing inventory and roster visual assertions**

Add tests that require the inventory frame, slot, five category tabs, sort button, destructive disassemble action, confirmation primary action and cancel action to resolve from the PSD asset tree. Update the existing roster test’s legacy-frame assertions to expect PSD frame and slot resources while retaining these existing assertions unchanged:

```cpp
TestEqual(TEXT("the companion backpack always reserves twelve roster slots"), Widget->GetRosterSlotCountForTest(), 12);
TestEqual(TEXT("the companion backpack fixes the roster at three columns"), Widget->GetRosterColumnCountForTest(), 3);
TestEqual(TEXT("the selected companion exposes its deterministic twelve-card personal pool"), Widget->GetVisiblePersonalCardIds().Num(), 12);
TestTrue(TEXT("the companion deck tooltip remains input-transparent"), Widget->IsCardTooltipHitTestInvisibleForTest());
```

- [ ] **Step 2: Run tests and verify legacy paths fail**

Run the companion roster and player flow filters. Expected: only the newly added PSD resource-path assertions fail before implementation; gameplay assertions remain green.

- [ ] **Step 3: Apply semantic buttons to inventory**

In `UGameXXKInventoryWindowWidget::BuildProgrammaticLayout` and confirmation refresh code:

```text
Frame, header, tabs, item slots -> direct PSD atoms
Close, back, filter, secondary action, cancel -> Neutral
Equip, buy, primary confirmation, sort -> Primary
Disassemble, sell confirmation, destructive confirmation -> Destructive
```

Keep `OpenFreeInventory`, `OpenMerchantTrade`, `SelectInventoryFilter`, `SortInventory`, `RequestBuyForSelectedItem`, `RequestSellForSelectedItem`, `RequestDecomposeForSelectedItem`, `RequestEnhanceForSelectedItem`, `ConfirmDialog`, `CancelDialog`, and all test helpers behaviorally unchanged. Do not convert a sell action to decompose or vice versa.

- [ ] **Step 4: Apply PSD frame/card-slot grammar to the companion roster**

In `UGameXXKCompanionRosterWidget::BuildProgrammaticLayout`, use PSD companion category atoms, frame, roster slots and primary/neutral/destructive actions. Keep the existing named widgets `CompanionRosterGrid`, `CompanionRosterPersonalCardScroll`, `CompanionRosterPersonalCardGrid`, `CompanionRosterSlot_01`, `CompanionRosterApplyLoadout`, deck Tooltip, recruitment and star-promotion surfaces. Do not replace card art or the locked status icon assets.

- [ ] **Step 5: Run tests and visual acceptance**

Run the inventory, companion roster and player flow automation filters. In PIE, recruit a seeded companion, open the companion roster, verify all 12 slots and all 12 personal cards appear, switch the active companion, stage exactly five cards, apply, then open inventory and test sort, filter, a non-destructive action, and a disassemble confirmation. Expected: no behavior change and every tested button uses its semantic PSD family.

- [ ] **Step 6: Commit**

```powershell
git add Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp Source/GameXXK/Public/UI/GameXXKCompanionRosterWidget.h Source/GameXXK/Private/Tests/GameXXKCompanionRosterWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKTownPsdStyleTest.cpp
git commit -m "feat: migrate inventory and companions to PSD v2"
```

### Task 7: Finish the character surface, remove duplicate legacy presentation, and verify the release slice

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKCharacterPanelWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKCharacterPanelWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKTownOverlayWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`
- Modify: `docs/production/` acceptance record for this slice
- Test: `Source/GameXXK/Private/Tests/GameXXKTownPsdStyleTest.cpp`

- [ ] **Step 1: Add failing character and duplicate-overlay tests**

Add tests that `UGameXXKCharacterPanelWidget` resolves its frame, attributes and equipment slots from the PSD tree and that `UGameXXKTownOverlayWidget` still supplies its public test actions while no longer mounts a second player-facing command stack when `UGameXXKTownHudWidget` is visible.

- [ ] **Step 2: Run the test to verify it fails**

Run the player-flow and town-style automation filters. Expected: resource-path and duplicate-presentation assertions fail before code changes.

- [ ] **Step 3: Migrate the character presentation**

Wrap the existing `SetCharacterSummary` and `RefreshPlayerSummary` data in PSD character atoms: character detail art, attributes frame, equipment frames, section tabs, a neutral detail button and live `TextBlock` data. The widget must not add attributes, gear, currency, experience, or upgrade behavior that the existing summary does not own.

- [ ] **Step 4: Retire only the duplicate visible overlay controls**

In `UGameXXKTownOverlayWidget`, preserve `IsTownOverlayVisible`, `HasTownActionForTest`, `SaveToSlotOne`, `EnterRouteMap` and all existing testing paths. Collapse or render hit-test-invisible only the old player-facing button stack that duplicates HUD navigation; never delete the testing interface and never alter the controller’s state transitions.

- [ ] **Step 5: Run full verification**

1. Run `python scripts/test_town_psd_package.py` and `python scripts/validate_town_psd_package.py --root SourceArt/UI/PSD/town-v2`.
2. Run the UE-side PSD texture audit through MCP.
3. Save dirty packages through MCP, close the editor, and run:

```powershell
& "D:\UE_5.8\Engine\Build\BatchFiles\Build.bat" GameXXKEditor Win64 Development -Project="D:\UE5 demo\GameXXK\GameXXK.uproject" -NoHotReload
```

4. Start the editor directly with the `.uproject` and `-ModelContextProtocolStartServer -ModelContextProtocolPort=18765`.
5. Run the relevant UI automation filters plus `GameXXK.PlayerFlow`.
6. Use `scripts/gamexxk_real_play_flow_mcp.py` for: main menu → world map → town → `F` task path → task panel → companion roster → inventory → town/route entry.
7. Capture 1920×1080 and 2560×1440 screenshots for HUD, character, companion roster, task list, and backpack. Compare each to the manifest preview: no cropped text, no overlapping sidebar/hand area, no empty buttons, no status-icon replacement.

Expected: all asset/package checks, UBT, automation tests and real-play checks pass; the 4K PSD validation reports a successful text round trip.

- [ ] **Step 6: Record acceptance and commit**

Record source hashes, manifest counts, validation JSON, build command, test filters and screenshot paths in the production acceptance record. Then:

```powershell
git add Source/GameXXK/Private/UI/GameXXKCharacterPanelWidget.cpp Source/GameXXK/Public/UI/GameXXKCharacterPanelWidget.h Source/GameXXK/Private/UI/GameXXKTownOverlayWidget.cpp Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKTownPsdStyleTest.cpp docs/production
git commit -m "feat: complete authoritative town PSD migration"
```

## Plan self-review

- **Spec coverage:** Tasks 1–3 implement the local PSD pipeline, semantic map, import and UE asset audit. Tasks 4–7 migrate every approved first-batch surface and preserve existing behavior. Task 7 supplies PSD, compile, automation, PIE and screenshot verification.
- **No-placeholder scan:** All source paths, asset roots, required semantic states, target widget methods, validation commands and acceptance checks are explicit; the plan does not defer a required artifact.
- **Consistency check:** All UE texture paths use `/Game/GameXXK/UI/Town/Textures/PSD/`; the design canvas remains 1024 logical units exported to 4096 at scale 4; all button consumers use the same three semantic families; status icons remain outside the migration.
