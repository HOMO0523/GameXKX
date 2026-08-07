# GameXXK Confirmed Master UI Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Consolidate the approved out-of-run UI directly in `GameXXK_UI_Master_V1.psd`, using Master page 03 as the backpack authority and the approved compact ingot strip across all out-of-run pages.

**Architecture:** A PowerShell coordinator performs preflight, backup, Photoshop execution, and deterministic validation. A Node builder emits one transactional JSX mutation that edits only the eight approved pages, duplicates page-03 backpack geometry into page 13, preserves superseded layers as hidden legacy content, and rolls back on failure.

**Tech Stack:** PowerShell 7, Node.js, Adobe Photoshop ExtendScript/COM, PSD and PNG assets.

---

### Task 1: Lock the approved source precedence

**Files:**
- Modify: `docs/superpowers/specs/2026-08-05-gamexxk-shared-currency-strip-rollout-design.md`

- [x] **Step 1: Record Master page 03 as the backpack source of truth**

  State that the standalone backpack PSD is not imported and page 13 inherits page-03 shell, equipment, inventory, and scrollbar geometry.

- [x] **Step 2: Record the permitted page-13 state difference**

  Permit only selected-slot styling, item detail, and item actions to differ from page 03.

### Task 2: Build the transactional Photoshop mutation

**Files:**
- Create: `scripts/ui_psd_pipeline/build-confirmed-master-ui-integration-jsx.js`
- Create: `scripts/ui_psd_pipeline/run-confirmed-master-ui-integration.ps1`

- [x] **Step 1: Generate a Photoshop JSX with exact target metadata**

  Encode the eight target pages and page origins, require exactly 18 top-level pages, and resolve direct layer groups by exact names before mutation.

- [x] **Step 2: Replace out-of-run currency presentation non-destructively**

  For each target page, hide its long currency paper, copper icon, and old balance text as legacy layers; add `01_CompactCurrencyPaper_320`, `02_IngotIcon`, and editable `03_IngotValue` at page-local `[1570, 28, 320, 86]`.

- [x] **Step 3: Convert every shop price icon to ingot**

  Traverse visible and hidden groups in `07_商店交易`; hide each shop copper icon and duplicate the approved ingot icon into the same visual bounds while leaving every price value unchanged. Change the insufficient-funds wording from `铜钱不足，还需 50` to `元宝不足，还需 50`.

- [x] **Step 4: Make page 13 a state peer of page 03**

  Preserve page-13 `45_Selection` and item-action/detail content as an overlay, move the old base groups into hidden `99_Legacy_*` groups, and duplicate the approved page-03 base groups with the page-origin delta `(0, 2400)`.

- [x] **Step 5: Add rollback and receipts**

  Capture the initial Photoshop history state, restore it on error, save only after structural checks pass, and write a UTF-8 JSON report containing target names, currency boxes, shop icon counts, state-pair signatures, and non-target signatures.

### Task 3: Execute against the canonical Master PSD

**Files:**
- Modify: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd`
- Create: `outputs/UI_PSD/Candidates/Backups/GameXXK_UI_Master_V1.before-confirmed-ui-integration.psd`
- Create: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.confirmed-ui-integration.report.json`
- Create: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.confirmed-ui-integration.validation.json`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/ConfirmedIntegration/After/*.png`

- [x] **Step 1: Run syntax and PowerShell parse checks**

  Run:

  ```powershell
  node --check scripts/ui_psd_pipeline/build-confirmed-master-ui-integration-jsx.js
  [void][ScriptBlock]::Create((Get-Content -Raw scripts/ui_psd_pipeline/run-confirmed-master-ui-integration.ps1))
  ```

  Expected: no output and exit code 0.

- [x] **Step 2: Run the integration coordinator**

  Run:

  ```powershell
  & scripts/ui_psd_pipeline/run-confirmed-master-ui-integration.ps1
  ```

  Expected: `Confirmed Master UI integration PASS`, eight 1920 x 1080 review PNGs, and a changed Master PSD SHA256.

- [x] **Step 3: Inspect page 03 and page 13 renders**

  Confirm the six equipment slots, hero placement, backpack grid, contents, and scrollbar match exactly; confirm page 13 adds only the selected-item state/detail/actions.

- [x] **Step 4: Inspect all out-of-run currency renders**

  Confirm the compact paper is 320 x 86, the icon is an ingot, the balance remains centered, and all shop price icons are ingots.

### Task 4: Verify isolation and archive the result

**Files:**
- Modify: `docs/superpowers/plans/2026-08-06-gamexxk-confirmed-master-ui-integration.md`

- [x] **Step 1: Verify structural invariants**

  Require 18 top-level pages, unchanged signatures for pages outside the eight-target set, unchanged values for all shop price text layers, no visible copper icon in shop/out-of-run currency groups, and exact page-03/page-13 base signatures after origin normalization.

- [x] **Step 2: Record hashes and review paths**

  Write the before, backup, and after PSD SHA256 values plus all exported PNG dimensions into the validation JSON.

- [ ] **Step 3: Mark the plan complete after visual review**

  Check off the remaining steps only after the receipts and rendered pages confirm the approved design.
