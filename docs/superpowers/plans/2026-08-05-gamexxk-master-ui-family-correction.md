# GameXXK Master UI Family Correction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Correct all sixteen non-shop PSD pages using the approved shop workflow, existing component kit, non-destructive layer injection, mathematical alignment and one final all-pages review.

**Architecture:** A manifest-driven pipeline first exports the current saved pages, then builds page-local shell and alignment records for four UI families. A focused Photoshop JSX injector imports clean reusable assets, duplicates the approved shop global-content group where applicable, hides legacy shell layers, applies text/content alignment rules and saves the existing PSD in place. Structural receipts and exported 1920×1080 pages verify the result without touching RuntimeAssets or Unreal Engine.

**Tech Stack:** Python 3 with Pillow and JSON, Node.js JSX generation, Photoshop ExtendScript through Windows COM, PowerShell safety wrapper, Git on `main`.

---

## File Responsibilities

- `scripts/build_gamexxk_ui_family_corrections.py`: validates page inventory, creates the family correction manifest, produces review/contact-sheet assets and performs PNG structural checks.
- `scripts/ui_psd_pipeline/build-family-corrections-jsx.js`: converts the correction manifest into Photoshop-compatible JSX without using ExtendScript `JSON.stringify`.
- `scripts/ui_psd_pipeline/run-family-corrections.ps1`: enforces saved-state, source-lock, backup and receipt checks, then runs the JSX once.
- `SourceArt/UI/PSD/gamexxk-v4/ui-master/FamilyCorrections/family-corrections-manifest.json`: exact page origins, shell presets, asset boxes, hidden legacy groups, preserved groups and alignment rules.
- `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/FamilyCorrections/`: current-page exports, corrected-page exports, four family contact sheets and one final overview.
- `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.family-corrections-source.json`: source/backup lock for the uninterrupted correction run.
- `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.family-corrections.validation.json`: final per-page Photoshop receipt.

### Task 1: Lock the Saved PSD and Export the Current Sixteen Pages

**Files:**
- Create: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.before-family-corrections.<timestamp>.psd`
- Create: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.family-corrections-source.json`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/FamilyCorrections/Before/*.png`

- [ ] **Step 1: Verify Photoshop saved state and reject an existing correction group**

Use Windows PowerShell COM to require `GameXXK_UI_Master_V1.psd` with `Saved == True`. Inspect all non-shop page groups and require that none contains `00_FamilyCorrection`.

- [ ] **Step 2: Create a hash-identical timestamped backup**

Resolve the PSD inside `outputs/UI_PSD/Candidates`, copy it without overwriting any existing backup, calculate SHA-256 for source and backup, and require equality before continuing.

- [ ] **Step 3: Write the source receipt**

Record absolute source and backup paths, source length, last-write time and both hashes in UTF-8 JSON.

- [ ] **Step 4: Export current page previews from the live saved PSD**

Duplicate the document as a merged preview, crop each page by the origin/size in `tmp/meta-shop-psd-review/master-manifest.json`, and export indices `01–06` and `08–17` to `Review/FamilyCorrections/Before` at 1920×1080.

- [ ] **Step 5: Verify the export gate**

Run a Pillow check requiring exactly sixteen PNGs, each `(1920, 1080)`, and confirm the live PSD remains `Saved == True` after export.

### Task 2: Build the Family Correction Manifest

**Files:**
- Create: `scripts/build_gamexxk_ui_family_corrections.py`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/FamilyCorrections/family-corrections-manifest.json`
- Read: `tmp/meta-shop-psd-review/master-manifest.json`
- Read: `SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/shell-components-manifest.json`

- [ ] **Step 1: Define the shared asset presets**

Use these page-local boxes:

| Preset item | Source asset | Target box |
|---|---|---|
| Town background | `town_background_dimmed.png` | `[0, 0, 1920, 1080]` |
| Hero identity paper | `identity_panel.png` | `[24, 14, 541, 185]` |
| Copper resource paper | `currency_panel.png` | `[1138, 14, 776, 118]` |
| Navigation: backpack | `nav_disc_backpack.png` | `[27, 210, 153, 154]` |
| Navigation: companion | `nav_disc_companion.png` | `[29, 359, 149, 152]` |
| Navigation: codex | `nav_disc_codex.png` | `[30, 504, 149, 162]` |
| Navigation: task | `nav_disc_task.png` | `[23, 651, 164, 165]` |
| Navigation: route | `nav_disc_route.png` | `[28, 800, 155, 157]` |
| Large town paper | `main_shop_panel.png` | `[310, 171, 1453, 851]` |
| Route/event paper | `main_shop_panel.png` | `[230, 150, 1460, 800]` |
| Reward paper | `main_shop_panel.png` | `[480, 140, 960, 820]` |
| Main-menu paper | `main_shop_panel.png` | `[580, 130, 760, 820]` |
| System-menu paper | `main_shop_panel.png` | `[660, 170, 600, 740]` |

Town-full pages are `03`, `04`, `05`, `06`, `13`, `14`, `15`. Town-HUD page `02` omits the large paper. Route-full pages are `08`, `09`, `16`. Battle pages `10` and `17` use only the independent background; reward page `11` also uses reward paper. Menu pages `01` and `12` use their respective centered paper presets.

- [ ] **Step 2: Define preserved and hidden groups per page**

For town-family draft pages, hide legacy `10_World`, `11_WorldOverlay`, `20_Shell`, and `21_Navigation` but preserve all content groups numbered `30–50` and `70_RuntimeText`. For `03`, hide `10_ApprovedV2Shell` and `15_HudContent` while preserving tabs, character, equipment, inventory, scrollbar and runtime text. For route/menu/reward pages, hide only legacy world/overlay/shell groups. For battle pages, hide only `10_World`. Never hide content, state, character, card, reward, action or runtime-text groups.

- [ ] **Step 3: Define shop-global duplication**

Duplicate `07_商店交易/20_GlobalShell` into pages `02–06` and `13–15`, translate it by `targetOrigin - [4080,1200]`, and rename it `20_GlobalShell_V2`. Hide only legacy global text layers matching these exact contents: `主角  Lv. 1`, `经验  0 / 100     战力 33`, `铜钱 10,000      青玉 2,000      金锭 500`, plus backpack-only scalar layers `Lv. 1`, `0 / 100`, `33`, `10,000`, `2,000`, `500`. Page-specific titles and content text remain visible.

- [ ] **Step 4: Define alignment regions**

Use these local regions and rules:

- Town-family title baseline starts at `[390, 215]`; subtitle starts at `[565, 225]`; both are left-justified.
- Town content stays within `[390, 300, 1250, 610]` and action buttons within `[1320, 850, 260, 80]`.
- Companion and codex card rows use equal-width centers across `x = 500, 735, 970, 1205, 1440`.
- Quest list uses `[450, 320, 400, 480]`; detail uses `[920, 320, 560, 480]`; the tracking button center is `[1460, 880]`.
- Route title is `[330, 190]`; route canvas is `[300, 280, 1320, 620]`; node-selection detail is `[1220, 250, 430, 230]`.
- Event title is `[330, 190]`; body is `[330, 270, 1260, 300]`; three choice-button centers are `[960, 640]`, `[960, 735]`, `[960, 830]`.
- Battle character anchors remain `[420, 555]` and `[1450, 555]`; card centers remain `x = 575, 830, 1085, 1340` at `y = 920`; turn and energy text use left/right anchors `[70, 55]` and `[1850, 55]`.
- Reward cards center at `x = 720, 960, 1200`, `y = 520`; confirmation button center is `[960, 850]`.
- Main-menu and system-menu button centers are `x = 960`; menu baselines use `y = 465, 565, 665, 765`.

Text layers attached to button centers use `Justification.CENTER`. Card captions and short counts use their card center. Descriptive paragraphs remain left-justified.

- [ ] **Step 5: Generate and validate the manifest**

Require sixteen unique page records, exact origins from the master manifest, known assets, no boxes outside 1920×1080, paired base/state presets for `03/13`, `04/14`, `05/15`, `08/16`, and `10/17`, and no references to page `07` as a mutation target.

Run:

```powershell
python scripts/build_gamexxk_ui_family_corrections.py --master-manifest tmp/meta-shop-psd-review/master-manifest.json --shell-manifest SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/shell-components-manifest.json --output SourceArt/UI/PSD/gamexxk-v4/ui-master/FamilyCorrections/family-corrections-manifest.json
```

Expected: `pageCount: 16`, `familyCount: 4`, and `validation: ok`.

### Task 3: Implement the Non-destructive Photoshop Injector

**Files:**
- Create: `scripts/ui_psd_pipeline/build-family-corrections-jsx.js`
- Create: `scripts/ui_psd_pipeline/run-family-corrections.ps1`

- [ ] **Step 1: Validate injector inputs before JSX generation**

Require target PSD, family manifest, master manifest, source receipt, all asset files and a missing final validation receipt. Require the source PSD hash to match the source receipt immediately before Photoshop runs.

- [ ] **Step 2: Generate Photoshop operations**

For each page record, the JSX must:

1. find the existing top-level page group;
2. reject an existing `00_FamilyCorrection` group;
3. create `00_FamilyCorrection` at the page-group top;
4. create `00_ShellComponents` inside it and import the page preset assets at `origin + localBox`;
5. duplicate and translate `20_GlobalShell` from the approved shop page when requested;
6. hide, rename with `99_Legacy_` prefix, but never delete the listed legacy groups;
7. hide only the exact legacy global text matches;
8. translate/resize listed content layers and set explicit text justification/anchors;
9. keep every preserved group and state layer visible and editable;
10. record imported names, hidden legacy groups, preserved groups and final bounds.

The JSX uses a local `jsonQuote` writer and contains no `JSON.stringify` call.

- [ ] **Step 3: Add rollback and saved-state protection**

Store the document history state before the first mutation. If any page fails before save, restore that history state. Reject `doc.saved == false` before mutation. Save once after all sixteen pages validate, then write one UTF-8 receipt.

- [ ] **Step 4: Implement the PowerShell wrapper**

Require a matching `before-family-corrections` backup, source-hash equality, Photoshop `Saved == True`, no existing receipt and no existing `00_FamilyCorrection` group. Parse the final receipt with `Get-Content -Encoding UTF8` and require sixteen successful page records.

- [ ] **Step 5: Run static verification and commit the pipeline**

Run Node syntax checking, PowerShell parser checking, generate the JSX, require all sixteen page names and `jsonQuote`, and reject `JSON.stringify` in generated JSX.

Commit:

```powershell
git add -- scripts/build_gamexxk_ui_family_corrections.py scripts/ui_psd_pipeline/build-family-corrections-jsx.js scripts/ui_psd_pipeline/run-family-corrections.ps1 SourceArt/UI/PSD/gamexxk-v4/ui-master/FamilyCorrections/family-corrections-manifest.json
git commit -m "feat: prepare non-destructive master UI family corrections"
```

### Task 4: Apply All Sixteen Corrections in One Saved PSD Run

**Files:**
- Modify: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd`
- Create: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.family-corrections.validation.json`

- [ ] **Step 1: Recheck the live source lock**

Require Photoshop saved state and the exact source SHA-256 from Task 1. Stop without mutation if either differs.

- [ ] **Step 2: Execute once**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/ui_psd_pipeline/run-family-corrections.ps1 -Psd outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd -Manifest SourceArt/UI/PSD/gamexxk-v4/ui-master/FamilyCorrections/family-corrections-manifest.json -MasterManifest tmp/meta-shop-psd-review/master-manifest.json -SourceReceipt outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.family-corrections-source.json -Receipt outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.family-corrections.validation.json
```

Expected: `PSD family corrections finished.` and sixteen successful page receipts.

- [ ] **Step 3: Inspect the live Photoshop structure**

Require `Saved == True`; each target page has exactly one `00_FamilyCorrection`; shop page `07` still contains its existing `00_ShellComponents`; all state/content groups listed as preserved remain present.

### Task 5: Export, Inspect and Tune the Corrected Families

**Files:**
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/FamilyCorrections/After/*.png`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/FamilyCorrections/character-family.png`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/FamilyCorrections/route-family.png`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/FamilyCorrections/battle-family.png`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/FamilyCorrections/menu-hud-family.png`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/FamilyCorrections/all-pages-final.png`

- [ ] **Step 1: Export all corrected pages from the saved PSD**

Use merged-document duplicates and manifest crops. Require sixteen 1920×1080 PNGs without modifying the source document.

- [ ] **Step 2: Generate four family contact sheets and a final overview**

Use Pillow with labeled tiles at 50% scale. Preserve original aspect ratio and use fixed family ordering from the manifest.

- [ ] **Step 3: Perform original-resolution visual inspection**

Check real icons and identity content, copper coin usage, portrait centering, even navigation rhythm, six aligned equipment slots, right-side backpack scrollbar, centered buttons/card captions, base/state parity, clean torn-paper edges and absence of baked shell text.

- [ ] **Step 4: Tune only failed layers**

If inspection finds a failure, use a focused Photoshop JSX adjustment that changes only the named layer bounds or text anchor. Save, re-export the affected page and repeat the same inspection. Do not rerun the full injector or recreate page groups.

### Task 6: Final Structural and Scope Verification

**Files:**
- Deliver: corrected PSD, timestamped backup, validation receipt and review directory

- [ ] **Step 1: Validate receipt, backup and PSD**

Require PSD size above 50 MB, sixteen successful page records, four family records, all imported assets present, hidden legacy backups present and source/backup hash equality.

- [ ] **Step 2: Verify base/state coordinate parity**

Compare Photoshop bounds for shared shell/global layers in the five base/state pairs after subtracting their page origins. Require exact equality except a maximum two-pixel alpha-bound difference on imported PNG edges.

- [ ] **Step 3: Run final scope check**

Run targeted Git status for the scripts, family manifest, review directory and outputs. Confirm no modifications to `RuntimeAssets`, Unreal Engine `Content`, `Source/GameXXK`, levels, PaperZD assets or gameplay probes.

- [ ] **Step 4: Commit review artifacts**

```powershell
git add -- SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/FamilyCorrections
git commit -m "art: correct the remaining master UI families"
```

- [ ] **Step 5: Deliver once**

Provide absolute clickable paths to the corrected PSD, latest backup, family manifest, validation receipt and final overview. State explicitly that runtime and Unreal Engine files were not modified.
