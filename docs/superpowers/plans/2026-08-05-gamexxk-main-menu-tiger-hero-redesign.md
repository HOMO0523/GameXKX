# GameXXK Main Menu Tiger-Hero Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild only `01_主菜单` as the approved full-screen tiger-and-hero illustration, with the title `霞客行` at upper left and four editable menu actions on the left.

**Architecture:** Generate one text-free 1920×1080 illustration from the approved layout reference plus the current tiger and hero identity assets. Insert it non-destructively into the existing master PSD, retain the old menu groups as hidden legacy layers, and rebuild title/buttons as separate editable PSD groups. Export only page 01 for the next user review gate.

**Tech Stack:** OpenAI ImageGen, Adobe Photoshop JSX, PowerShell, existing GameXXK PSD component kit.

---

## Task 1: Preserve the current page and source assets

**Files:**
- Read: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd`
- Read: `SourceAssets/AnimationProduction/safe_frame_1600/enemies/21_tiger_boss.png`
- Read: `SourceAssets/AnimationProcessing/Production/enemy_21_tiger_boss_idle/contact_sheet.png`
- Read: `SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/hero_runtime_idle_frame_0000.png`
- Read: `C:/Users/shxuw/AppData/Local/Temp/codex-clipboard-3487416d-6f44-4b91-bcc6-35aaac3243b0.png`
- Create: `outputs/UI_PSD/Candidates/Backups/GameXXK_UI_Master_V1.before-main-menu-tiger-hero.psd`

- [x] Confirm all three reference images and the master PSD exist.
- [x] Copy the master PSD to the exact backup path without overwriting an existing backup.
- [x] Export the current `01_主菜单` once as the before image at `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MainMenuTigerHero/Before/01_主菜单.png`.
- [x] Record the master PSD size and SHA-256 before modification.

## Task 2: Generate the text-free main-menu illustration

**Files:**
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/main-menu/main_menu_tiger_hero_v9_loose_inkwash.png`

- [x] Use ImageGen with the layout reference for composition only, the final `21_tiger_boss.png` plus idle contact sheet for tiger identity, and `hero_runtime_idle_frame_0000.png` for hero identity.
- [x] Request a 16:9 full-screen illustration with the giant tiger coiled diagonally through the middle, head lowered toward the hero, one forepaw thrust into the stream foreground, the other braced at right, the small hero reacting at lower right, and a lower-contrast left menu zone. Match the approved reference's loose warm-gray charcoal linework and muted salmon-orange translucent watercolor; keep contours continuous but organic, simplify the hero to a few iconic shapes, and avoid polygon facets, repeated microtexture and hard digital inking.
- [x] Require no title, no menu text, no buttons, no logos, no watermark, and no extra characters.
- [x] Preserve the final boss's low four-legged crouch, massive forepaws, looped upright tail, angular charcoal stripes, white swept eyebrows, blocky cream cheek/chin ruff, round golden eyes and short natural fangs; preserve the hero's bun, brown outer coat, gray-blue inner clothing, bamboo basket/backpack and travel bag.
- [x] Inspect the result at original resolution. Allow one focused regeneration only if tiger/hero identity or left-side negative space is materially wrong.
- [x] Copy the selected generation into the workspace at the exact create path without replacing unrelated art.

## Task 3: Build the page-01-only Photoshop automation

**Files:**
- Create: `scripts/ui_psd_pipeline/build-main-menu-tiger-hero-jsx.js`
- Create: `scripts/ui_psd_pipeline/run-main-menu-tiger-hero.ps1`
- Create: `scripts/ui_psd_pipeline/build-main-menu-illustration-replace-jsx.js`
- Create: `scripts/ui_psd_pipeline/run-main-menu-illustration-replace.ps1`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/main-menu/components/menu_brush_primary_from_kit.png`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/main-menu/components/menu_brush_normal_from_kit.png`
- Generate: `tmp/ui_psd_pipeline/apply-main-menu-tiger-hero.jsx`
- Modify: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd`

- [x] Generate a JSX that opens the master PSD and targets only top-level group `01_主菜单`.
- [x] Preserve the existing direct page groups by renaming the displaced visual groups with the `99_Legacy_` prefix when needed and hiding them; do not delete user layers.
- [x] Create/update direct group `10_MenuIllustration` and place the generated illustration at canvas origin, covering exactly 1920×1080.
- [x] Create/update direct group `20_HeroIdentityCorrection`; leave it empty and hidden unless a precise current-hero overlay is required after visual inspection.
- [x] Create/update direct group `30_Title`, with editable title text `霞客行` placed at upper left within the safe area.
- [x] Derive primary and normal deep-teal brush PNGs from the alpha of the approved `category_selected_ink.png` component, then create/update direct group `40_MenuButtons` and place four vertically aligned states on the left without asking Photoshop to recolor pixels.
- [x] Set the four editable labels exactly to `开始游戏`, `加载存档`, `设置游戏`, `退出`; make `开始游戏` the selected/emphasized state.
- [x] Keep runtime text separate in `70_RuntimeText` and ensure no central parchment/shop panel remains visible on page 01.
- [x] Save the PSD in place and write an execution report to `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.main-menu-tiger-hero.report.json`.
- [x] If the illustration is refined after the menu layers have already been assembled, replace only art layer `00_TigerHero_MainMenu_Illustration` inside `10_MenuIllustration`, preserve every peer group, and write a separate v9 replacement report and validation file.

## Task 4: Export and visually verify page 01

**Files:**
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MainMenuTigerHero/After/01_主菜单.png`
- Create: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.main-menu-tiger-hero.validation.json`

- [x] Export only `01_主菜单` at 1920×1080.
- [x] Verify the title reads `霞客行`, the four labels are exact, and title/menu alignment does not collide with the tiger or hero.
- [x] Verify the tiger is the dominant subject at center/right, the hero is small at lower right, and the left menu has clear contrast.
- [x] Verify the old central parchment panel is not visible and no generated text/watermark is present.
- [x] Verify PSD top-level page count and every non-target page remain unchanged.
- [x] Write PASS/FAIL evidence, layer names, export dimensions, and before/after PSD hashes to the validation JSON.

## Task 5: User review gate

- [x] Show `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MainMenuTigerHero/After/01_主菜单.png` to the user.
- [x] Stop after page 01 and wait for explicit approval or corrections before changing another interface.

## Verification Commands

Run from `D:/UE5 demo/GameXXK`:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/ui_psd_pipeline/run-main-menu-tiger-hero.ps1
Get-FileHash -Algorithm SHA256 outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd
Get-Content outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.main-menu-tiger-hero.validation.json
```

Expected result: Photoshop automation exits successfully, the validation file reports `PASS`, page 01 exports at 1920×1080, and no UE assets or other page groups are modified.

## Scope Guardrails

- This is visual-production work; per user instruction, do not add TDD tests.
- Do not modify `Content/`, `Source/`, gameplay logic, UE runtime UI, character sprite sheets, PaperZD assets, levels, camera transforms, or HD2D planes.
- Do not regenerate, restyle, or export pages 02–17 during this task.
- Do not commit the large PSD, generated review assets, or unrelated dirty-worktree files; commit only authored documentation and automation scripts.
