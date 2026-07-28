# 战斗状态图标语言实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让战斗脚下 HUD 使用 PSD 血条、可叠层的低饱和水墨扁平状态图标与真实 Hover Tooltip，并修复场景按 UnitId 差分刷新。

**Architecture:** `GameXXKBattleStatusIconStyle` 负责把权威 Armor/`EGameXXKCardStatus` 投影为图标、说明与优先级；`UGameXXKBattleUnitStatusWidget` 只创建/刷新纸签控件；Presenter 以 UnitId 集合差分更新 Actor。13 张新生成的透明图标被导入 UI 资源路径，UMG 通过稳定资源路径加载。

**Tech Stack:** Unreal Engine 5.8、C++ UMG、Paper2D、Automation Tests、built-in ImageGen、UE MCP/UBT。

---

## 文件结构

- Create: `Source/GameXXK/Public/UI/GameXXKBattleStatusIconStyle.h` — 图标投影、Tooltip 和排序的无状态接口。
- Create: `Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp` — 18 种有效状态与 Armor 的完整映射及未来状态回退。
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleUnitStatusWidget.h` — 纸签/图标/Tooltip 的刷新与测试接口。
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusWidget.cpp` — PSD 生命条、状态纸签、Hover Tooltip。
- Modify: `Source/GameXXK/Public/MVP/GameXXKBattleScenePresenter.h` — 按 UnitId 差分刷新接口。
- Modify: `Source/GameXXK/Private/MVP/GameXXKBattleScenePresenter.cpp` — 保留相同 UnitId Actor、只移除过期 Actor。
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp` — 图标映射、同 UnitId 保留、状态清理/反馈测试。
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardRouteEventSupportTest.cpp` — 伙伴/NPC 进出时差分刷新测试。
- Create/import: `Content/GameXXK/UI/Battle/StatusIcons/` — 13 张生成后导入的透明图标资产。

### Task 1: 生成并验收状态图标素材

**Files:**
- Create: `Content/GameXXK/UI/Battle/StatusIcons/` imported textures and source PNGs under project source-art location.

- [ ] **Step 1: 以统一风格生成 13 张 PNG**

Use built-in ImageGen for each named icon. Prompt invariant: "single flat game UI status icon, no text or digits, transparent-ready flat #00ff00 chroma-key background, pale rice-paper badge base, dark hand-brushed ink outline with only a slight dry-brush and ink-wash irregularity, one muted low-saturation mineral-color center silhouette with no second colored block, Chinese ink-and-paper Jianghu game UI, front-facing, centered, crisp silhouette, low-to-medium contrast, no pure primary colors, no volumetric lighting, no realistic texture, no intricate brushwork, no shadow, no watermark." Use the corresponding subject: shield armor, momentum seal, wing, cracked mask, blood drop, poison vial, flame, target mark, guard shield, rot spiral, immunity talisman, tactic seal, terrain-and-redirect seal.

- [ ] **Step 2: 去除色键并验证透明度**

Run the installed imagegen `remove_chroma_key.py` for every selected PNG. Verify alpha mode and transparent corners; reject assets containing text, gradients outside the badge, or visual style drift.

- [ ] **Step 3: 通过项目既有 UE Python/MCP 导入为 Texture2D**

Import to `/Game/GameXXK/UI/Battle/StatusIcons/` with deterministic names `T_BattleStatus_<Name>`. Do not overwrite existing textures. Save packages through UE MCP after import.

### Task 2: 建立完整、可测试的状态图标投影

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKBattleStatusIconStyle.h`
- Create: `Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`

- [ ] **Step 1: 写入失败的映射测试**

Add assertions for Armor, Poison, Bleed, Burn, DamageOverTime, Vulnerability, Agility, Guard, Momentum, all three next-action statuses, all terrain/redirect statuses, Mark, immunity, and the fallback valid enum. Assert nonempty texture ID, nonempty effect Tooltip, a stable priority, and Armor's shield-first wording.

- [ ] **Step 2: 实现纯映射 API**

Define `FGameXXKBattleStatusIconStyle { FName IconId; FString DisplayName; FString Tooltip; FLinearColor Tint; int32 Priority; }`, plus `ResolveArmorIconStyle()`, `ResolveStatusIconStyle(EGameXXKCardStatus)`, and `DescribeStatusTooltip(Style, Stacks)`. Map every current enum value explicitly; fallback uses `UnknownStatus` with enum-name Tooltip and low priority.

- [ ] **Step 3: 运行目标测试**

After editor is closed run normal UBT, then the `GameXXK.MVP.Battle.SceneActors` automation group. Expected: mapping test compiles and passes.

### Task 3: PSD 血条、状态纸签与纯 Hover Tooltip

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleUnitStatusWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusWidget.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`

- [ ] **Step 1: 写入失败的 Widget 行为测试**

Assert the health progress style uses the approved Town HUD Frame/Fill resource objects, Armor renders a badge with stack count, each nonzero status creates one badge, `OnHovered` displays a `HitTestInvisible` tooltip without mutating status, and a subsequent `SetUnitStatus` that removes a stack removes both badge and tooltip.

- [ ] **Step 2: 实现纸签 UI**

Load `T_TownHUD_HealthBarFrame` / `T_TownHUD_HealthBarFill` as `FProgressBarStyle`; add a horizontal `StatusBadgeRow`. Each `UBorder` has pale paper brush/tint and ink outline, contains `UImage` icon and right-top stack `UTextBlock`; bind button-style hover only, no click delegate. Display max readable badges, use deterministic priority and final `+N` badge if needed. Tooltip is owned by the Widget, uses the existing PSD paper frame, and stays `HitTestInvisible`.

- [ ] **Step 3: Run Widget tests**

Run the focused automation group after a successful non-Live-Coding build. Expected: no flat default health fill, badge visibility tracks current runtime stacks, Hover/Unhover has no gameplay side effects.

### Task 4: Fix Presenter difference refresh and coverage gaps

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKBattleScenePresenter.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKBattleScenePresenter.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardRouteEventSupportTest.cpp`

- [ ] **Step 1: Write preservation tests**

Set up three units, store each spawned Actor pointer by UnitId, change HP/Armor/Statuses for one retained UnitId and remove/add a different UnitId. Assert retained pointer identity is equal, old UnitId is absent, new UnitId is present, retained HUD reports the new values, and feedback returns actor to original transform.

- [ ] **Step 2: Implement UnitId set difference refresh**

`RefreshBattleScene` builds placements, indexes `SpawnedUnitObjects` by UnitId, destroys/removes only keys absent from the next placements, updates retained actors in place (placement, sprite, status), and spawns only missing UnitIds. It must never call `ClearSpawnedUnits` for ordinary membership changes.

- [ ] **Step 3: Test temporary NPC/companion transitions**

Assert hero remains 1P, absence never creates a fake 2P/3P actor, a newly active quest NPC spawns only the NPC, and a removed NPC does not destroy the unchanged hero/companion actors.

### Task 5: Full validation

- [ ] **Step 1: Run normal UBT from the direct `.uproject` path**

Run `D:\UE_5.8\Engine\Build\BatchFiles\Build.bat GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload` only while the editor is closed.

- [ ] **Step 2: Run all affected Automation groups**

Run `UnrealEditor-Cmd.exe 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI -ExecCmds='Automation RunTests GameXXK.MVP.Battle.SceneActors+GameXXK.Integration.CardRoute.EventSupport+GameXXK.Integration.CardBattle.BoardTargeting; Quit'`.

- [ ] **Step 3: Save and verify in PIE through project MCP scripts**

Use the project MCP client and `scripts/gamexxk_real_play_flow_mcp.py` to enter a battle and verify PSD health bar, Armor/status badges and tooltip behavior, retained Actor refresh, enemy intent resolution, and no end-turn lock. Do not use Live Coding or force-close an editor with unsaved packages.

## Plan self-review

- Covers all user requirements: high-saturation icon bottoms, numeric stacks, buffs/debuffs, complete supported status mapping, Hover Tooltip, PSD health style, generated missing assets, and non-rebuilding actor refresh.
- Does not change user-tuned maps, camera, PaperZD, sprites, combat formulas, or save format.
- Uses deterministic fallback for new statuses, preventing a future enum value from silently disappearing.
