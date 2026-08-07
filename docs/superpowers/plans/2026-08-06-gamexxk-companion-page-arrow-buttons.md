# GameXXK Companion Page Arrow Buttons Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce two `36 × 62` transparent ink page-switch buttons and migrate gray companion portraits from the obsolete locked meaning to the approved inactive meaning.

**Architecture:** Generate one left-facing pure two-stroke ink chevron with the built-in image-generation tool, remove its flat chroma background, normalize it into a narrow transparent hit canvas, then create the right arrow as an exact horizontal mirror. Existing ink controls inform brush weight and edge character only; their circle/X geometry is not reused. Keep portrait and button preparation separate: the existing portrait cutter owns normal/inactive portraits, while a focused script owns the generated page-arrow source, normalized assets, and manifest.

**Tech Stack:** Python 3, Pillow, deterministic PNG/JSON verification

---

### Task 1: Migrate gray portrait semantics

**Files:**
- Modify: `scripts/ui_psd_pipeline/prepare-partner-switch-portraits.py`
- Rename: `SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits/partner_portrait_*_locked.png`
- Modify: `SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits/partner_switch_portraits_manifest.json`
- Modify: `SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits/partner_switch_portraits_contact_sheet.png`

- [ ] **Step 1: Rename the render state and filenames**

Rename `render_locked` to `render_inactive`, output `_inactive.png`, and record state `inactive`. Do not change pixel transforms: the gray portrait remains the approved owned-but-not-deployed presentation.

- [ ] **Step 2: Regenerate the portrait batch**

Run:

```powershell
python scripts/ui_psd_pipeline/prepare-partner-switch-portraits.py
```

Expected: exactly six normal and six inactive `105 × 62` RGBA portraits; no `_locked` files remain.

### Task 2: Generate the pure-chevron page arrows

**Files:**
- Read: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Assets/Controls/close_button_ink_v2.png` (brush character reference only)
- Create: `scripts/ui_psd_pipeline/prepare-companion-page-buttons.py`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchControls/companion_page_left_Button.png`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchControls/companion_page_right_Button.png`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchControls/companion_page_buttons_contact_sheet.png`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchControls/companion_page_buttons_manifest.json`

- [ ] **Step 1: Generate and cut out the left two-stroke chevron**

Use built-in ImageGen to create `<` from exactly two tapered, continuous diagonal ink strokes meeting at one inward vertex on a flat chroma background. Remove the chroma locally, normalize the generated ink into `36 × 62`, and keep the canvas transparent. Do not include a shaft, horizontal connector, paper frame, circle, or text.

- [ ] **Step 2: Create the strict mirror pair**

Save the normalized left button as `companion_page_left_Button.png`. Produce the right button only with a horizontal transpose of the final left canvas so alpha bounds, ink weight, and vertical center remain identical.

- [ ] **Step 3: Validate and write the manifest**

Fail unless both images are RGBA `36 × 62`, contain transparent and visible pixels, have equal alpha-pixel counts, satisfy exact mirror equality, and pass the geometry rule that no horizontal stem extends away from the inward vertex. Record the construction version, reference SHA-256, output hashes, alpha bounds, dimensions, geometry rule, and mirror result.

### Task 3: Review and commit the asset batch

**Files:**
- Verify: `SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits/*`
- Verify: `SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchControls/*`

- [ ] **Step 1: Review the contact sheets**

Confirm the arrows remain readable as pure `<` / `>` at `36 × 62`, use only two diagonal ink strokes, have no horizontal stem, paper rectangle, or circular backing, and do not resemble the white-X close control. Confirm gray portraits are labeled inactive, not locked.

- [ ] **Step 2: Run deterministic verification twice**

Run both generators twice and compare hashes. Expected: zero mismatches; 12 portrait records and 2 arrow records pass their manifests.

- [ ] **Step 3: Commit only the focused files**

```powershell
git add -- scripts/ui_psd_pipeline/prepare-partner-switch-portraits.py scripts/ui_psd_pipeline/prepare-companion-page-buttons.py SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchControls docs/superpowers/plans/2026-08-06-gamexxk-companion-page-arrow-buttons.md
git commit -m "art: add companion page arrow buttons"
```

Do not stage the master PSD or unrelated existing worktree changes.
