---
status: in_progress
owner: codex
updated_at: 2026-08-19T17:10:00+08:00
source_commit: 0fd4c889a825ab54c5813fab5c40829cd69ffdb5
---
# 2026-08-19 桌面历练 goal 进度与复核证据

本记录只描述当前工作区可复现的证据，不把历史 598/596 报告当作本轮结果，也不把用户未确认的美术当作生产真源。

## 基线与保护

- 规格冻结基线：`ba90810a56e06a3b70ed0e3125c4ef67a59a0685`。
- 当前工作区：根目录 `main`，运行时/证据源码基线 `0fd4c889a825ab54c5813fab5c40829cd69ffdb5`；本记录由其后的 docs-only commit 承载。原 `codex/desktop-training-2d-hud-migration` 分支保留在 `57a06e4`，工作树中的用户资产/探针未因切换改变。
- `Content/GameXXK/Maps/L_Main.umap` 当前 SHA256 仍为 `EE6E8394E40298321F2A57CC030018BDD1109EED36248597A7D7F414E387E46B`；本轮没有写入该地图。
- 未跟踪 `SourceAssets/`、`SourceArt/` 和历史 `Content/Python/_*.py` 探针未被批量加入本轮真源或生产资产。

## 本轮已验证

### ImageTruth 与顶部置顶状态

- `python scripts/gamexxk_ui_image_truth_check.py --json`：`ok=true`，`confirmedCount=8`，`manifestCount=8`，`findings=[]`。
- 黑色开启态与灰色关闭态图钉均为 1254×1254 RGBA、透明背景、同一居中轮廓；灰色版由用户明确确认后晋升。
- MCP 导入结果：
  - `T_TrainingTopToolbarAlwaysOnTop`
  - `T_TrainingTopToolbarAlwaysOnTopOffGray`
- `GameXXK.DesktopTraining.Workbench.ImageTruthNavigationBinding`：1/1。

### 运行时与存档

- 17:10 当前源码冷 UBT：`GameXXKEditor Win64 Development -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2`，四个动作均标记 `[NoUba]`，`Result: Succeeded`。
- 当前源码聚焦 Automation：`GameXXK.MVP.SaveGame` 12/12（`Saved/HarnessReports/20260819-165042-ai-production-loop.md`）、`GameXXK.Training` 21/21（`20260819-165119`）、`GameXXK.DesktopTraining.Workbench` 19/19（`20260819-165216`），均 0 failed、0 error。Workbench 的 6 条 warning 已归类为 3 条 BattleVisual 生命周期诊断与 3 个尚未导入的 Guard/宋金宝/月白 1K idle atlas，不冒充零 warning。
- `UGameXXKMVPRules::MakeSaveState` 对副本执行 `FGameXXKDesktopInventoryRules::Normalize`，避免物品 map 已更新而物理背包格尚未重建时生成不可加载的 v22 存档；不会修改 live RuntimeState。
- v8/v9 迁移测试夹具明确使用有效三人配置或关闭 TravelRunner，避免把“不完整旧测试夹具”误判为生产迁移失败；版本边界保持 v17 历史基线 → v18/v19/v20/v21/v22，后续字段从 v23 继续。

### Phase 0 脚本门禁

- `python scripts/harness_state_validator.py --json`：`findings=[]`。
- `python scripts/ai_production_loop.py --run-script-tests --script-tests all --json`：当前最终复验通过，报告 `Saved/HarnessReports/20260819-170948-ai-production-loop.md`；all 模式未启动 UnrealEditor，新增 HP 启动合同 3/3 进入默认 headless 门禁。
- `git diff --check`：退出码 0（仅报告现有工作区的换行转换 warning，无 whitespace error）。
- asset-contract 当前报告 `Saved/HarnessReports/20260819-170933-ai-production-loop.md`：58/69 通过、11 个显式失败；`test_gamexxk_ui_master_pages.py` 已从旧失败集合关闭，且没有新增失败。剩余分类见本文末表格。
- mcp-live 当前报告 `Saved/HarnessReports/20260819-170230-ai-production-loop.md`：标签内六个脚本全部通过；HP HUD 真实流已覆盖 MainMenu 直达 Town、接任务、路线图、战斗、治疗/伤害表现期间与结算后的权威 HP 同步，`failures=[]`；PartyDeck 198 张目录合同 16/16。测试编辑器最终由 MCP 确认 `dirty_before=[]`、`dirty_after=[]` 后正常退出。

## 未通过与回滚点

1. 性能：`Saved/HarnessReports/desktop-training-hud-memory-20260819-113244.json` 的 HUD-only 编辑器测量为 20 秒 Working 3248.6/Private 4652.8 MiB、50 秒 Working 3387.1/Private 4759.6 MiB，仍远高于 TaskBarHero 参照和目标包络。当前只有 HUD-only 一组，空壳/静置/局内/3D 四组尚未完成；禁止切默认 2D 入口。回滚点是保留 `main` 的 3D 青山镇入口与隔离地图 `L_DesktopTrainingHUD`。
2. 视觉交付：ImageTruth 已有 8 张，但顶部音量/邮件/商店/退出、Tab、历练节点状态、挑战/游历/重试、工具五模式和宝箱/局内专用图标仍未逐张确认；主 PSD 页面尚未完成最终可编辑交付。
3. 真实 PIE：工作台/训练自动化为绿，但完整 1920×1080/2560×1440 截图、挑战三敌三我连续画布、字体与 tooltip 可读性和最终 Luna 证据仍缺。
4. 玩法：普通/困难/地狱状态、失败重试、1-1 同配置、两精英与首领 tooltip 已有规则/聚焦测试，但最终实际路线图节点交互、天赋 read model/掉率配置、FIFO 箱批和工具真实配方仍未完成。

## 下一步

- 扩展内存采样器以固定同机记录空壳、历练静置、局内 ChallengeViewport、3D 青山镇四组数据，并记录 CPU/GPU/Working/Private/GPU Dedicated。
- 以四组同机性能采样作为下一门禁；在取得空壳/历练/挑战/3D 可比数据前不切默认入口。
- 继续按 ImageTruth 候选队列逐张确认新图，只有确认后才导入 PSD/UE。

## 2026-08-19 16:46 Phase 0 合同复现与分类

- 根目录已在不改工作树内容的前提下把 `main` 快进到 `57a06e4` 并切回 `main`；随后以 `60b9e08`（运行时/测试）、`cfee4df`（验证资产）和 `0fd4c88`（工具链/证据）三批 checkpoint 收口。该运行时/证据基线比 `origin/main` 领先 18 个提交，滚动指针由后续 docs-only commit 承载。
- `L_Main.umap` 仍为 `EE6E8394E40298321F2A57CC030018BDD1109EED36248597A7D7F414E387E46B`；`L_QingshanInn.umap` 当前观测值为 `7856B66D188213A2878AD2F72569BFFCBDBC0EECEFC0E199BDE90194441FE92C`，两者均未在本轮写入。
- mcp-live 两个旧合同已先以 headless RED 复现，再做最小修复：HP HUD runner 接受当前 `MainMenu -> Town` 直达语义；PartyDeck live runner 改读由 `GameXXK.Data.CardDocumentation` 校验的 198 张生成目录，不再正则耦合 C++ `AddCard` 调用形态。HP 启动器另锁定只启动一个编辑器、使用隔离 DDC/无 Zen 自动启动参数、MCP 断开不掩盖原始结果；`test_hp_hud_entry_contract.py` 3/3、`test_party_deck_real_play_acceptance.py` 16/16、完整 mcp-live 均通过。

| asset-contract 测试 | 复现分类 | 当前处置 |
|---|---|---|
| `test_battle_terrain_art.py` | 缺失其测试所需的 approved manifest；父目录现有 `battle-terrain-manifest-v2.json` 是不同 schema，不能伪装成同一合同 | 保留 blocker，需地形美术包补齐对应 manifest/review artifacts |
| `test_gamexxk_ui_master_pages.py` | 内部测试漂移：18 张预览均生成，第 0 张公共组件页有 image layer 但按设计无 text layer | 已收紧为仅后 17 页要求文字层；2/2 PASS |
| `test_party_deck_card_portrait_pipeline.py` | 内部语义漂移：生产脚本已扩为 13 party + 3 route + 21 enemy（37），旧测试仍要求 17、临时输出参数和绝不替换已有资产 | 保留 blocker；需单独审定 37 张合同与用户调图保护语义后更新测试/接口 |
| `test_party_deck_sprite_import_pipeline.py` | 外部源缺失：PSD clean cutout `063.png`、`064.png`、`065.png` 不在登记的个人 Downloads 路径 | 保留显式 external-source blocker |
| `test_party_deck_sprite_manifest.py` | 与上一项同根因；12 个 packed atlas 已标 ready，但三份辅助 PSD 身份源缺失导致总校验 `ok=false` | 保留显式 external-source blocker |
| `test_psd_card_frame_pipeline.py` | 外部源缺失：批准的 `057.png` 仅登记在个人 Downloads 路径 | 保留显式 external-source blocker |
| `test_qingshan_b1_heightmap.py` | 保护地图基线漂移：合同期待 `a363...`，当前用户 `L_QingshanInn.umap` 为 `7856...` | 禁止覆盖/回滚地图；等待用户批准后单独重定基线 |
| `test_qingshan_building_concepts.py` | 明确未实现：golden asset、canonical JSON、prompt packet 三个函数仍抛 `NotImplementedError` | 独立功能 blocker，不归因于环境，不在桌面历练批次伪绿 |
| `test_qingshan_dress_b1_config.py` | 与保护地图哈希同根因 | 等待批准重定基线 |
| `test_qingshan_dress_b1_scripts.py` | 75 项中 74 通过；唯一错误与保护地图哈希同根因 | 等待批准重定基线 |
| `test_reference_faithful_task_ui_icons.py` | 视觉合同漂移：`reward_coin/exp/token` aspect 变化，`reward_exp` 还发生缩小 | 表现类 blocker；需 Luna/用户视觉复核，不自动改图 |
| `test_town_hud_psd_visual_contract.py` | 混合 blocker：外部 `036.png` 缺失，同时旧测试仍要求当前 Town HUD 已不引用的 `T_TownBackpack_ActionBlank` | 外部源与运行时合同拆开独立复核，不修改保护 UI 追求假绿 |
