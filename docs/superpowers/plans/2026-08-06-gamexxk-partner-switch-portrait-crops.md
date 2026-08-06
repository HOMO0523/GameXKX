# GameXXK Partner Switch Portrait Crops Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce six color-corrected `105 × 62` transparent face portraits and matching gray locked variants for the companion-only 12-slot switcher.

**Architecture:** A focused Pillow/NumPy script reads the six locked final Idle first frames, applies role-specific face crops, removes magenta edge contamination, performs restrained tone normalization, and writes normal/locked PNGs plus a contact sheet and manifest. The master PSD remains untouched in this batch.

**Tech Stack:** Python 3, Pillow, NumPy, deterministic PNG/JSON verification

---

### Task 1: Build the deterministic portrait cutter

**Files:**
- Create: `scripts/ui_psd_pipeline/prepare-partner-switch-portraits.py`
- Read: `SourceAssets/AnimationProcessing/Production/character_01_blade_idle/frames/frame_0000.png`
- Read: `SourceAssets/AnimationProcessing/Production/character_02_guard_idle/frames/frame_0000.png`
- Read: `SourceAssets/AnimationProcessing/Production/character_03_healer_idle/frames/frame_0000.png`
- Read: `SourceAssets/AnimationProcessing/Production/character_04_hunter_idle/frames/frame_0000.png`
- Read: `SourceAssets/AnimationProcessing/Production/character_05_sorcerer_idle/frames/frame_0000.png`
- Read: `SourceAssets/AnimationProcessing/Production/character_06_formation_master_idle/frames/frame_0000.png`

- [ ] **Step 1: Define the six approved sources and face crop boxes**

Use a `RoleSpec` record with `role`, `source`, `crop`, `brightness`, `contrast`, and `saturation`. Crop rectangles keep a `105:62` aspect ratio and are role-specific so face scale, eye line, and hair silhouette align.

- [ ] **Step 2: Add deterministic magenta-edge cleanup**

For translucent or boundary pixels whose red/blue channels dominate green, replace contaminated RGB with the nearest non-magenta opaque subject color before resizing. Preserve the original alpha channel.

- [ ] **Step 3: Add normal and locked rendering**

Normal portraits receive only the recorded restrained tone correction. Locked portraits reuse the exact same crop and transform, reduce saturation to `0.10`, reduce brightness to `0.56`, apply a neutral `#4E4B45` wash, and preserve alpha.

- [ ] **Step 4: Write exact assets and metadata**

Write the following files under `SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits`:

```text
partner_portrait_blade.png
partner_portrait_guard.png
partner_portrait_healer.png
partner_portrait_hunter.png
partner_portrait_sorcerer.png
partner_portrait_formation_master.png
partner_portrait_blade_locked.png
partner_portrait_guard_locked.png
partner_portrait_healer_locked.png
partner_portrait_hunter_locked.png
partner_portrait_sorcerer_locked.png
partner_portrait_formation_master_locked.png
partner_switch_portraits_contact_sheet.png
partner_switch_portraits_manifest.json
```

### Task 2: Generate and verify the asset batch

**Files:**
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits/*`

- [ ] **Step 1: Run the cutter**

Run:

```powershell
python scripts/ui_psd_pipeline/prepare-partner-switch-portraits.py
```

Expected: `Generated 12 partner switch portraits at 105x62`.

- [ ] **Step 2: Verify deterministic asset contracts**

The script must fail unless every portrait is RGBA, exactly `105 × 62`, has non-empty foreground and transparent background pixels, and has no strong magenta boundary pixels. The manifest records each source path, source SHA-256, crop rectangle, output SHA-256, dimensions, alpha bounds, and state.

- [ ] **Step 3: Review the contact sheet**

Inspect the contact sheet at original scale. Confirm that all six faces have comparable visual size, no hair/hat is accidentally clipped, face centers are stable, normal colors remain faithful, and locked portraits are readable but clearly unavailable.

- [ ] **Step 4: Commit only the new script and portrait batch**

```powershell
git add -- scripts/ui_psd_pipeline/prepare-partner-switch-portraits.py SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits docs/superpowers/plans/2026-08-06-gamexxk-partner-switch-portrait-crops.md
git commit -m "art: prepare companion switch portraits"
```

Do not stage unrelated existing worktree changes.
