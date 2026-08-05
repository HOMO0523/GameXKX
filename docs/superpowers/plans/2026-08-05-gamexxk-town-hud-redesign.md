# GameXXK Town HUD Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Work directly in the root project on `main`; GameXXK explicitly forbids worktrees. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild only `02_城镇HUD` as the approved unobstructed out-of-run town HUD, with the calibrated shop identity block, five permanent navigation icons, and a compact 320 px ingot strip.

**Architecture:** Add one focused Photoshop JSX builder plus one guarded PowerShell runner. The builder mutates only the top-level town-HUD page, keeps prior content as hidden legacy groups, imports the clean town reference and kit paper assets, duplicates the approved shop global content for identity/navigation parity, replaces only the shop copper-coin display with an editable ingot/value pair, and exports before/after review PNGs. The runner protects unsaved Photoshop work, creates a hash-locked backup, validates sources and outputs, and writes a machine-readable validation receipt.

**Tech Stack:** Node.js JSX generation, Adobe Photoshop ExtendScript/COM, PowerShell 5.1-compatible orchestration, System.Drawing for deterministic nine-slice paper derivation and PNG dimension checks, SHA-256 structural receipts. This is visual/PSD work; per user instruction it uses verification after implementation rather than TDD.

---

## File Map

- Create `scripts/ui_psd_pipeline/build-town-hud-redesign-jsx.js`: validate inputs and generate the page-scoped Photoshop JSX.
- Create `scripts/ui_psd_pipeline/run-town-hud-redesign.ps1`: create the backup/derived strip, run Photoshop, and validate receipts and exports.
- Create `SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/town-hud/components/currency_strip_320.png`: deterministic 320 x 86 nine-slice derivative of the approved paper strip.
- Create `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/TownHUD/Before/02_城镇HUD.png`: current-page review export.
- Create `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/TownHUD/After/02_城镇HUD.png`: rebuilt-page review export.
- Create `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.town-hud.report.json`: Photoshop page-mutation receipt.
- Create `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.town-hud.validation.json`: orchestration and hash validation receipt.
- Create `outputs/UI_PSD/Candidates/Backups/GameXXK_UI_Master_V1.before-town-hud-v2.psd`: exact pre-mutation safety copy.
- Modify `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd`: only top-level page `02_城镇HUD`.
- Do not modify or stage `SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeAssets/`, Unreal assets, or other top-level PSD pages.

### Task 1: Build the guarded town-HUD runner

**Files:**
- Create: `scripts/ui_psd_pipeline/run-town-hud-redesign.ps1`

- [ ] **Step 1: Declare exact sources and destinations**

Use these defaults and construct Chinese page/output names from character codes so the runner remains ASCII-only under Windows PowerShell 5.1:

```powershell
param(
    [string]$Psd = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd',
    [string]$TownBackground = 'SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Generated/town_background_clean_no_ui.png',
    [string]$ShellComponents = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents',
    [string]$CurrencyPanel = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/currency_panel.png',
    [string]$IngotIcon = 'SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Content/resource_gold.png',
    [string]$Backup = 'outputs/UI_PSD/Candidates/Backups/GameXXK_UI_Master_V1.before-town-hud-v2.psd',
    [string]$Report = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.town-hud.report.json',
    [string]$Validation = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.town-hud.validation.json'
)
$townHudName = '02_' + (-join @([char]0x57CE,[char]0x9547)) + 'HUD'
$reviewRoot = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/TownHUD'
$beforeExport = Join-Path $reviewRoot ('Before/' + $townHudName + '.png')
$afterExport = Join-Path $reviewRoot ('After/' + $townHudName + '.png')
$derivedStrip = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/town-hud/components/currency_strip_320.png'
```

- [ ] **Step 2: Add non-destructive preflight and backup checks**

Resolve every source with `Resolve-Path`, reject missing inputs and pre-existing report/export destinations, attach to Photoshop through COM, and refuse to proceed when the target PSD is open and unsaved. If the backup does not exist, create its parent and copy the current PSD once; if it exists, require its SHA-256 to equal the current pre-mutation PSD hash.

```powershell
$beforeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if (-not (Test-Path -LiteralPath $backupPath)) {
    [IO.Directory]::CreateDirectory((Split-Path -Parent $backupPath)) | Out-Null
    Copy-Item -LiteralPath $psdPath -Destination $backupPath
}
$backupHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $backupPath).Hash.ToLowerInvariant()
if ($backupHash -ne $beforeHash) { throw 'Town-HUD backup does not match the current PSD' }
```

- [ ] **Step 3: Derive the 320 x 86 strip from the existing kit paper**

Use System.Drawing to copy 54 px left/right caps from `currency_panel.png` and stretch only the center section into a transparent 320 x 86 PNG. Validate the finished dimensions before launching Photoshop.

```powershell
$targetWidth = 320
$targetHeight = 86
$cap = 54
$source = [Drawing.Bitmap]::new($currencyPanelPath)
$target = [Drawing.Bitmap]::new($targetWidth,$targetHeight,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [Drawing.Graphics]::FromImage($target)
try {
    $graphics.DrawImage($source,[Drawing.Rectangle]::new(0,0,$cap,$targetHeight),[Drawing.Rectangle]::new(0,0,$cap,$source.Height),[Drawing.GraphicsUnit]::Pixel)
    $graphics.DrawImage($source,[Drawing.Rectangle]::new($cap,0,$targetWidth-2*$cap,$targetHeight),[Drawing.Rectangle]::new($cap,0,$source.Width-2*$cap,$source.Height),[Drawing.GraphicsUnit]::Pixel)
    $graphics.DrawImage($source,[Drawing.Rectangle]::new($targetWidth-$cap,0,$cap,$targetHeight),[Drawing.Rectangle]::new($source.Width-$cap,0,$cap,$source.Height),[Drawing.GraphicsUnit]::Pixel)
    $target.Save($derivedStripPath,[Drawing.Imaging.ImageFormat]::Png)
} finally {
    $graphics.Dispose(); $target.Dispose(); $source.Dispose()
}
```

- [ ] **Step 4: Generate/run JSX and validate the result**

Invoke `build-town-hud-redesign-jsx.js` with the PSD, clean background, shell-component directory, derived strip, ingot icon, both review exports, and report path. After Photoshop completes, require: report status `PASS`, page `02_城镇HUD`, 18 top-level pages, unchanged non-target signature, unchanged shop peer signature, 320 px strip width, five navigation icons, no visible persistent prompt, and both exports at 1920 x 1080. Write the validation JSON with before/backup/after SHA-256 values and every verified boolean.

Run syntax verification before executing the runner:

```powershell
node --check scripts/ui_psd_pipeline/build-town-hud-redesign-jsx.js
$null = [scriptblock]::Create([IO.File]::ReadAllText('scripts/ui_psd_pipeline/run-town-hud-redesign.ps1',[Text.Encoding]::UTF8))
```

Expected: both commands exit successfully and the runner file contains no byte greater than 127.

### Task 2: Generate the isolated Photoshop mutation

**Files:**
- Create: `scripts/ui_psd_pipeline/build-town-hud-redesign-jsx.js`

- [ ] **Step 1: Parse arguments and lock the page contract**

Require these flags: `--psd`, `--background`, `--shell-components`, `--currency-strip`, `--ingot`, `--output`, `--report`, `--before-export`, and `--after-export`. Require every input file plus the shell-component directory, require `identity_panel.png` and the five `nav_disc_*.png` files inside that directory, create destination parents, and reject an existing report or export.

```js
const required = ['psd','background','shell-components','currency-strip','ingot','output','report','before-export','after-export'];
const pageName = '02_城镇HUD';
const shopPageName = '07_商店交易';
const originX = 4080;
const originY = 0;
const pageWidth = 1920;
const pageHeight = 1080;
```

- [ ] **Step 2: Add reusable JSX helpers**

Carry forward the proven helpers from `build-main-menu-tiger-hero-jsx.js`: direct-group lookup, document lookup, layer bounds, raster import, editable text creation, UTF-8 report writing, page export by duplicate/crop, and recursive non-target signatures. Add helpers to find direct art layers by name and text layers by exact contents.

```jsx
function findTextByContents(container, contents) {
  for (var i = 0; i < container.artLayers.length; i++) {
    var layer = container.artLayers[i];
    if (layer.kind == LayerKind.TEXT && String(layer.textItem.contents) == contents) return layer;
  }
  return null;
}
```

- [ ] **Step 3: Export the before state and create hidden legacy groups**

Before mutation, export the visible `02_城镇HUD` crop to the before path. Record the 18-page count plus non-target and `07_商店交易` signatures. Rename and hide the current direct groups as follows; refuse to overwrite an existing legacy destination:

```text
00_FamilyCorrection -> 99_Legacy_00_FamilyCorrection_PreTownHUDV2
30_Context -> 99_Legacy_30_Context_PreTownHUDV2
70_RuntimeText -> 99_Legacy_70_RuntimeText_PreTownHUDV2
```

- [ ] **Step 4: Build the clean scene and approved paper shell**

Create these direct groups under `02_城镇HUD`:

```text
10_TownScene
20_ShellPaper
21_HeroAndNavigation
30_OutOfRunCurrency
70_RuntimeText
```

Import the clean town background at page-local `[0,0,1920,1080]`. Import the approved identity paper at `[24,14,541,185]`, the five existing shell discs at their family-manifest boxes, and the derived currency strip at `[1570,28,320,86]`. Do not create a world-dim layer or central/context paper.

- [ ] **Step 5: Reuse the calibrated shop identity/navigation content**

Find `07_商店交易/20_GlobalShell`, duplicate its direct layers into `21_HeroAndNavigation`, and translate them by the page-origin delta `(0,-1200)`. Keep only the approved hero portrait, the three identity text layers, and these five real kit icon layers:

```text
hero_portrait
nav_backpack
nav_companion
nav_codex
nav_task
nav_route
```

Exclude the old `coin_20_GlobalShell_500` layer and the shop currency text layer whose exact contents are `500`. Preserve the approved hero identity positions and the navigation icon positions without additional translation.

- [ ] **Step 6: Add the editable out-of-run currency pair**

Import `resource_gold.png` into `30_OutOfRunCurrency` as `01_IngotIcon` at `[1672,50,42,42]`. Add an editable `02_IngotValue` layer to `70_RuntimeText` with contents `500`, 27 pt Microsoft YaHei bold, ink color `#2B2822`, left anchor `[1728,82]`. Do not bake `元宝` into the paper.

- [ ] **Step 7: Verify isolation, export, save, and report**

Before saving, require:

```jsx
if (doc.layerSets.length != 18) throw new Error('Expected eighteen top-level pages');
if (otherPagesBefore != nonTargetSignature(doc,pageName)) throw new Error('A non-target page changed');
if (shopBefore != directGroupSignature(findDirectLayerSet(doc,shopPageName))) throw new Error('Shop reference changed');
if (findDirectLayerSet(page,'30_Context')) throw new Error('Persistent context group is still active');
if (heroNav.artLayers.length != 9) throw new Error('Expected portrait, three identity texts, and five navigation icons');
```

Export the after state, save the PSD, and write a UTF-8 report containing status, page, page count, group names, five navigation layer names, currency icon source, currency strip box, persistent-prompt visibility, non-target match, shop-peer match, and both export paths. On failure, restore the initial Photoshop history state and write `<report>.error.txt`.

### Task 3: Execute and review the town-HUD PSD pass

**Files:**
- Modify: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/town-hud/components/currency_strip_320.png`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/TownHUD/Before/02_城镇HUD.png`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/TownHUD/After/02_城镇HUD.png`
- Create: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.town-hud.report.json`
- Create: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.town-hud.validation.json`
- Create: `outputs/UI_PSD/Candidates/Backups/GameXXK_UI_Master_V1.before-town-hud-v2.psd`

- [ ] **Step 1: Run the guarded Photoshop pass**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ui_psd_pipeline/run-town-hud-redesign.ps1
```

Expected: `Town-HUD PSD update PASS`, distinct before/after PSD hashes, matching backup/before hashes, and valid report/validation paths.

- [ ] **Step 2: Inspect the final 1920 x 1080 export at original resolution**

Confirm visually:

- the clean town scene is unobstructed and has no artificial full-screen dim;
- the calibrated portrait and three identity lines match the shop page;
- five real navigation icons remain evenly aligned on the left;
- no lower-left paper/prompt remains;
- the top-right paper is 320 px wide;
- the ingot icon and `500` read as one centered unit;
- no copper-coin icon or currency label is visible.

- [ ] **Step 3: Run final structural verification**

```powershell
node --check scripts/ui_psd_pipeline/build-town-hud-redesign-jsx.js
git diff --check -- docs/superpowers/plans/2026-08-05-gamexxk-town-hud-redesign.md scripts/ui_psd_pipeline/build-town-hud-redesign-jsx.js scripts/ui_psd_pipeline/run-town-hud-redesign.ps1
```

Parse both JSON receipts with explicit UTF-8 and require every boolean to be true, the current PSD SHA-256 to equal `afterSha256`, both review PNGs to be 1920 x 1080, and the compact strip to be 320 x 86.

- [ ] **Step 4: Stop at the page-review gate**

Show `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/TownHUD/After/02_城镇HUD.png` to the user. Do not modify `03_主角背包`, RuntimeAssets, or Unreal Engine until the user approves this page.

### Task 4: Archive the reproducible workflow

**Files:**
- Modify: `docs/superpowers/plans/2026-08-05-gamexxk-town-hud-redesign.md`
- Create: `scripts/ui_psd_pipeline/build-town-hud-redesign-jsx.js`
- Create: `scripts/ui_psd_pipeline/run-town-hud-redesign.ps1`

- [ ] **Step 1: Mark completed plan checkboxes**

Change each finished `- [ ]` item in this plan to `- [x]`; leave no placeholder marker or unchecked implementation item before claiming completion.

- [ ] **Step 2: Stage only the reproducible workflow files**

```powershell
git add -- docs/superpowers/plans/2026-08-05-gamexxk-town-hud-redesign.md scripts/ui_psd_pipeline/build-town-hud-redesign-jsx.js scripts/ui_psd_pipeline/run-town-hud-redesign.ps1
git diff --cached --check
git diff --cached --name-status
```

Expected: exactly the plan plus the two new scripts are staged; unrelated dirty files remain untouched.

- [ ] **Step 3: Commit after all verification passes**

```powershell
git commit -m "art: rebuild town hud PSD workflow"
```

Expected: one commit on `main` containing only the three workflow files. The large PSD, backup, generated derivative, review exports, and receipts remain local production artifacts unless they are already tracked and intentionally selected later.
