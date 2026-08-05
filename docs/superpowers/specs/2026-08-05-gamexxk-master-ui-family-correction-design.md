# GameXXK Master UI Family Correction Design

## Context

`GameXXK_UI_Master_V1.psd` contains eighteen 1920×1080 pages arranged on one 10080×4680 master canvas. `00_公共组件` is the component library, `07_商店交易` is the approved reference workflow, and the remaining visible pages contain a mixture of approved V2 work and Phase A drafts. The shop page now demonstrates the required non-destructive workflow: the original composite shell remains as a hidden backup, the background and eight paper-shell elements are independent layers, and all real content layers remain editable above them.

The next phase applies that workflow to every other visible page without redesigning gameplay, replacing approved art, or migrating anything to Unreal Engine.

## Goal

Correct all sixteen non-shop visible pages so that they use the existing approved component kit consistently, have mathematically aligned and centered content, expose reusable shell elements as independent PSD layers, preserve page-specific content and states, and can be reviewed family by family before any runtime migration.

## Scope

The work covers these page families:

| Family | Base pages | State pages | Shared contract |
|---|---|---|---|
| Character systems | `03_主角背包`, `04_伙伴编队`, `05_图鉴`, `06_任务日志` | `13_主角背包_物品选中`, `14_伙伴编队_角色选中`, `15_图鉴_怪物选中` | Town background, hero identity, resource strip, five navigation positions, page paper, real kit icons |
| Route systems | `08_路线图`, `09_路线事件` | `16_路线图_节点选中` | Route background and page paper, route-specific content, base/state coordinate parity |
| Battle systems | `10_战斗HUD`, `11_战斗奖励结算` | `17_战斗HUD_卡牌选中目标` | Battle-specific HUD and panels; no forced town navigation |
| Menu and HUD | `01_主菜单`, `02_城镇HUD`, `12_系统菜单` | None | Scene-appropriate reuse only; menus and HUDs are not forced into the full shop shell |

`00_公共组件` remains a source library rather than a user-facing page. `07_商店交易` remains the approved reference and is not rebuilt. The already approved backpack content is preserved; its base and selected-state pages still receive structural, alignment, scrollbar, and family-parity review.

## Non-goals

- Do not change gameplay functions, page information architecture, copy, prices, item definitions, quest data, combat rules, or route logic.
- Do not replace real navigation icons, portraits, item icons, currency icons, or approved character art with placeholders or generated substitutes.
- Do not edit `RuntimeAssets`, Unreal Engine `Content`, `Source/GameXXK`, levels, PaperZD assets, character sprites, cameras, or HD2D placement.
- Do not migrate PSD results into the game during this phase.
- Do not delete or flatten user-authored PSD content.

## Page Layer Contract

Each corrected page uses the smallest applicable subset of this structure:

1. Existing page groups such as `10_Background`, `20_GlobalShell`, content groups and state groups keep their current names and relative order.
2. `00_ShellComponents` is added inside the page's existing background group, matching the approved shop structure. It contains the independent scene background and applicable reusable paper-shell layers. Town-family pages may contain hero identity paper, resource paper, five navigation discs, and the main page paper. Route, battle, menu, and HUD pages contain only components appropriate to their context.
3. Existing icon, portrait, text, grid, detail, button, and state groups remain separate and editable above the shell.
4. State-only layers contain selection outlines, detail cards, confirmation overlays, target markers, or result content. They do not duplicate or drift the base shell.
5. The previous composite shell is renamed with a `99_原始合成壳体_备份` prefix and hidden. It remains recoverable.

State pages reuse the exact local 1920×1080 shell boxes of their base page. Only the state delta changes. Page-local coordinates are converted to master-canvas coordinates by adding the page origin from `master-manifest.json`; visual elements are never positioned directly by eye on the 10080×4680 canvas.

## Component and Alignment Rules

- Use the existing V2 component kit and the real content already present in the master PSD.
- Permanent currency uses the approved copper-coin icon. Identity panels use the approved hero portrait and identity text.
- The hero portrait is centered within its circular opening in both axes; no lower-edge gap or clipped rim is allowed.
- Five town navigation discs share one horizontal centerline and a consistent vertical rhythm. Their real icons are centered inside the discs.
- Six equipment slots form two aligned columns of three with consistent slot sizes and row spacing.
- Backpack inventory keeps its right-side scrollbar and supports the existing extended capacity; it is not reduced to sixteen visible-only items as a data assumption.
- Text alignment uses explicit left, center, or right justification tied to component axes. Titles, prices, counts, button labels, tab labels, and short status text must not rely on manually offset point text.
- Main content grids use consistent cell sizes, row baselines, icon centers, and label-price spacing.
- Paper fiber, torn edges, ink lines, and neutral shadows remain recognizable at original resolution. Transparent corners must not contain roof, foliage, stone, bridge, or other scene pixels.
- Text, icons, products, detail content, and buttons remain separate from shell PNGs so later runtime extraction is possible.

## Family-specific Treatment

### Character Systems

Lock the town-family global shell first, then reuse its positions on backpack, companion, codex, and quest pages. Base and selected-state pairs share shell, grid, and navigation coordinates. Selected states add only their item detail, character detail, monster detail, or selection indicator. Existing backpack equipment, core items, starter equipment, scrollbar, and approved character content remain unchanged except for alignment corrections.

### Route Systems

Route-map base and node-selected pages share the same route canvas and panel coordinates. The selected page adds only the selected-node detail and action state. Route-event content keeps its event-specific art and choices while adopting the same spacing, text, and button alignment rules.

### Battle Systems

Battle pages preserve the battle-specific composition and do not inherit the town left navigation. Base and target-selected battle pages share character, enemy, card-row, energy, and turn-indicator coordinates. The selected page adds only targeting state. Reward settlement uses the same approved paper and button language without forcing battle HUD elements into the result screen.

### Menu and HUD

The main menu, town HUD, and system menu reuse only scene-appropriate components. The main menu keeps its player-facing start flow. The town HUD remains usable without opening the shop by default. The system menu uses the approved paper/button family while keeping menu-specific hierarchy.

## Non-destructive Batch Workflow

The families are processed in this order:

1. Character systems.
2. Route systems.
3. Battle systems.
4. Menu and HUD.

Before every family batch:

1. Confirm the target PSD is open and saved.
2. Record its length, last-write time, and SHA-256 hash.
3. Create a timestamped, hash-identical backup beside the PSD.
4. Reject the batch if an unrecorded source version, unsaved Photoshop state, or existing target injection group is detected.

For every page in the batch:

1. Resolve its origin and expected content groups from the master manifest.
2. Build or reuse the applicable shell placement manifest.
3. Generate transparent component assets only when the kit does not already provide a clean reusable asset.
4. Run original-resolution component review before Photoshop injection.
5. Inject the new group, hide the original composite layer, and save in place without flattening.
6. Write a UTF-8 validation receipt containing page, group, imported layer count, preserved groups, backup path, and component names.

If any page fails, the current family stops. Later families are not written. The latest family backup remains the restore point.

## Review Deliverables

Each family produces:

- One 1920×1080 PNG per corrected page exported from the saved PSD.
- One edge-focused difference image per page where shell extraction or reconstruction occurred.
- One family contact sheet for side-by-side alignment and state-parity review.
- One placement manifest with page-local boxes and master origins.
- One validation receipt per injected page or one family receipt containing equivalent per-page records.

The review gate checks:

- Real icons, portraits, items, currency and identity content are present.
- Titles, labels, numbers, icons, slots and buttons are centered or aligned according to their component axes.
- Base and state pages share exact shell and shared-content coordinates.
- No shell component contains baked page text or functional content.
- No bright seams, foreign background fragments, clipped icons, uneven equipment slots, missing scrollbar, or portrait rim gaps are visible.
- Required existing groups remain present and editable.

The user approves one family contact sheet before the next family is modified.

## Verification Strategy

This is visual-art and PSD-structure work, so it does not use TDD. Verification consists of:

- deterministic manifest validation;
- PNG size, alpha, edge, and bounds checks;
- Photoshop DOM checks for layer names, counts, visibility, bounds, base/state coordinate parity, and preserved groups;
- original-resolution exported-page inspection;
- source/backup hash comparison and post-save validation receipts;
- a final scope check proving that no RuntimeAssets or Unreal Engine files were modified.

## Acceptance Criteria

The phase is complete when all sixteen pages satisfy the applicable layer contract, all four family contact sheets are approved, every page can be recovered from its hidden composite layer or timestamped PSD backup, and the final scope check shows no runtime or Unreal Engine migration. Runtime migration begins only after a separate user instruction.
