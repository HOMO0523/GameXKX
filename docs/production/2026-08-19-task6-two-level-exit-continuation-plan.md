---
status: plan
owner: agent
created_at: 2026-08-19T23:10:00+08:00
branch: main
head_at_plan: 643ac9b
source_of_truth: docs/production/current-goal-acceptance.md
---

# Task 6 两层退出控制收尾执行计划（2026-08-19）

> 本文件是当前唯一待执行工作包的行动计划：完成“局内自动出牌 + 两层退出确认”的最终真实 PIE/MCP 验收、视觉取证、回归门禁与安全提交。执行依据仍是
> `docs/superpowers/plans/2026-08-19-battle-retreat-route-abandon-controls.md` 的 Task 6 与
> `docs/production/2026-08-19-deepseek-handoff.md`。

## 0. 已核实的当前状态（计划落笔时）

- Git：根目录 `main`，HEAD `643ac9b docs: add DeepSeek handoff progress`，领先 `origin/main` 38 个提交；工作树 dirty，234 条 status 记录。
- 已提交功能：v23 战斗入口检查点、Battle 原子退回、RouteMap 放弃结算预览/结算、BattleBoard 与 RouteMap 弹窗 UI（`7d2a9f2..69c5f4b`）均已完成。
- 未提交的本轮 Task 6 改动（必须人工拆分提交，禁止 `git add -A`）：
  `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h/.cpp`、`Source/GameXXK/Private/Tests/GameXXKBattleRetreatTest.cpp`、`Content/Python/gamexxk_probe_real_play_flow.py`、`scripts/gamexxk_real_play_flow_mcp.py`、`scripts/test_gamexxk_real_play_flow_mcp.py`、`scripts/test_gamexxk_real_play_flow_probe.py`。
- 用户保护文件未变：`Content/GameXXK/Maps/L_Main.umap` SHA256 仍为
  `EE6E8394E40298321F2A57CC030018BDD1109EED36248597A7D7F414E387E46B`。
- 编辑器：`UnrealEditor.exe` PID 33648 运行中，MCP `127.0.0.1:18765` 可用；`ue_mcp_smoke.py` 通过（68 toolsets，必需工具集齐，无 gamefeature error）；PIE 未运行；`save_dirty_packages` 返回 `dirty_before=[]`、`dirty_after=[]`。
- 脚本门禁：`py_compile` 通过；`python -m unittest scripts.test_gamexxk_real_play_flow_mcp scripts.test_gamexxk_real_play_flow_probe scripts.test_ue_pie_lifecycle` = **115/115 OK**；`git diff --check` 退出码 0（仅 LF→CRLF 警告）。
- `harness_state_validator.py --json` 退出码 0，但有 1 条 warning：`docs/production/2026-08-19-deepseek-handoff.md` 缺 `source_commit` frontmatter；计划内补齐。
- 最新真实流报告 `Saved/HarnessReports/battle-retreat-route-abandon-real-flow.json`：**`ok=false`**。
  流程已完成：主菜单→Town→任务→路线图→fixture 变 Elite→开自动战斗→后台节流观察到 ≥2 次权威变化→关闭自动战斗→打开 Battle 弹窗。
  失败点：分辨率矩阵在 1280×720 通过后，请求 1672×941 得到 PNG `(1556, 884)`；
  记录 `logical_scale=[0.8,0.8]`、`physical_size=[2090,1176]`。因此取消 no-op、确认回退、精英重试、奖励、RouteMap 预览/取消/确认、Town 返回、fixture/存档清理等命名 checkpoint 尚未完成。
- 视觉目录 `Saved/VisualReview/20260819-battle-retreat-route-abandon/` 仅有 `_highres_smoke_1280x720.png`，没有可采信的 Battle/RouteMap 三分辨率弹窗证据。

## 1. 保护边界（执行全程不变）

- 只在根目录 `main` 工作；不创建 worktree；不用 UnrealBridge；不用 Live Coding/Hot Reload。
- 不覆盖/回滚：`L_Main.umap`、`L_QingshanInn.umap`、`scripts/test_battle_camera_framing.py`、根目录 `Private/` `Public/`、`SourceAssets/`、`SourceArt/`、`Content/Python/_*.py` 一次性探针。
- 不新增艺术资源；两个弹窗/工具栏继续复用现有纹理与字体。
- 编辑器如有 dirty package：先经 MCP `save_dirty_packages` 保存，再决定是否停止编辑器；MCP 不可用时不得强关。
- 报告只写真实路径、真实计数、真实失败；不把旧报告冒充本轮绿灯，不修改 JSON 伪造分辨率通过。

## 2. 完成边界（引用自既有计划）

Task 6 只有同时满足下列条件才能标记完成：

1. 真实 PIE/MCP 双层流程报告 `ok=true`，生产自动战斗不代替玩家点击节点、奖励或确认按钮。
2. Battle 取消是严格 no-op；确认恢复进入前 current/index/HP/MP/visited/reachable、丢弃未领取奖励、精英仍可重试。
3. RouteMap 取消是严格 no-op；预览 `永久金币 +4 / 强化石 +2`；确认精确结算一次并回 Town。
4. 冷 UBT、聚焦/回归 Automation、Python harness、状态 validator 通过；已知历史 baseline 单独列出。
5. 1280×720、1672×941、1920×1080 的 Battle/RouteMap 弹窗截图存在并通过 Luna max；若环境确实达不到某分辨率，必须记录实际分辨率与根因，明确保留 blocker，不伪造通过。
6. `L_Main.umap` 保护 hash 未变；用户调过的地图、相机、HD2D plane、动画与源美术未被覆盖。
7. 计划、滚动指针、证据日志更新；提交清单不含保护文件。

## 3. 执行顺序

### 阶段 A：修复分辨率取证（先做，避免继续污染真实流 JSON）

**A1 根因实验（不改生产断言）**

用当前运行中的 MCP 编辑器做最小 smoke，按序验证并记录：

- 当前显示器工作区/DPI 真实值；`GetDpiForWindow`、outer/client rect、Slate screenshot 尺寸与 `player_controller.get_viewport_size()` 的关系。
- 为什么 `take_high_res_screenshot(1672,941)` 输出 `1556×884`：是 OS 窗口被工作区钳制，还是 PIE viewport 未真正 resize。
- 最小可行方案候选：
  1. PIE 内执行 `r.SetRes 1672x941` / `r.SetRes 1920x1080`，随后探针轮询 viewport size 到目标值，再调用 `AutomationLibrary.take_high_res_screenshot`；
  2. 先用 `resize_preview_window_logical` 把 GameXXK Preview 的 outer/client 调到与目标一致，再用 Slate `Screenshot` transport 捕获并校验 PNG 尺寸；
  3. 若两者都只能到 `(1556, 884)` 或更小的显示器工作区，按 handoff 第 7 节记录环境阻塞，不再改 JSON 假绿。

可参考实现：
- `Content/Python/gamexxk_qingshan_dress_b1_acceptance.py::_prepare_level_viewport_for_capture` 的 viewport preflight；
- `scripts/gamexxk_real_play_flow_mcp.py::PreviewWindowController.resize_preview_window_logical`；
- `scripts/gamexxk_real_play_flow_mcp.py::RealFlowHarness.slate_screenshot` 与 `capture_resolution_matrix`。

**A2 实现取证路径**

- 在 `Content/Python/gamexxk_probe_real_play_flow.py::_handle_high_res_screenshot` 中：
  - 记录请求尺寸、preflight viewport size、transport 与 DPI；
  - 若采用 `r.SetRes`：截图前设置目标分辨率并等待 `player_controller.get_viewport_size()` 达到目标；截图完成后恢复原分辨率，恢复失败要显式报错；
  - 返回文件真实 PNG 尺寸，不再只返回请求尺寸。
- 在 `scripts/gamexxk_real_play_flow_mcp.py::capture_resolution_matrix` 中：
  - 为每个分辨率记录 `requested`、`actual`、`dpi`、`logical_scale`、`transport`、`viewport_size`；
  - 对无法达到的分辨率按“记录阻塞并显式失败”处理，错误信息包含实际尺寸与根因，而不是静默 pass。
- 同步补 `scripts/test_gamexxk_real_play_flow_mcp.py` / `scripts/test_gamexxk_real_play_flow_probe.py` 的纯函数单测：
  - viewport resize 计算；
  - matrix 输出 contract；
  - `ok=false` 时错误文本必须包含 `requested` 与 `captured/actual`。

**A3 最小截图 smoke（不跑完整真实流）**

- 启动 PIE 到主菜单或 BattleBoard 可见的 fixture，只调 `--high-res-screenshot` 三次（1280×720、1672×941、1920×1080），检查三张 PNG 实际尺寸与 UI 未变形。
- 全部达到目标尺寸后再进入阶段 C 的真实流；否则先把环境限制写入证据并保留 blocker，同时继续完成其余不依赖分辨率的验收（阶段 B/C 中的语义检查）。

### 阶段 B：宽回归门禁

在编辑器已保存（必要时经 MCP 保存 dirty packages）并关闭后执行：

```powershell
python -m unittest scripts.test_gamexxk_real_play_flow_mcp scripts.test_gamexxk_real_play_flow_probe scripts.test_ue_pie_lifecycle
python scripts/ai_production_loop.py --run-script-tests --script-tests all --json
python scripts/harness_state_validator.py --json
git diff --check

& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2
```

冷 UBT 通过后跑聚焦/回归 Automation：

```powershell
& scripts/run_mvp_test_suites.ps1 -Suites @(
  'GameXXK.Route.BattleRetreat',
  'GameXXK.Route.Settlement',
  'GameXXK.MVP.SaveGame',
  'GameXXK.MVP.RouteMap',
  'GameXXK.Integration.CardRoute',
  'GameXXK.Integration.CardBattle',
  'GameXXK.DesktopTraining.Workbench',
  'GameXXK.Training'
) -TimeoutSeconds 1500
```

判定：
- 新回归必须 0 failed/0 error；`GameXXK.Route.BattleRetreat.DevelopmentFixture` 必须含在内。
- 若 `GameXXK.MVP.UI.MainMenuPlayerFlow.SaveMigration` 历史 baseline 仍失败，单独记录，不隐藏、不当作本轮回归。
- Workbench 的 6 条已知 warning 只归类说明，不冒充 0 warning。

### 阶段 C：重跑真实 PIE/MCP 双层退出验收

在 MCP 可用、PIE 停止、无 dirty package 后执行：

```powershell
python scripts/gamexxk_real_play_flow_mcp.py --two-level-exit-acceptance --timeout 600 --report Saved/HarnessReports/battle-retreat-route-abandon-real-flow.json
```

必须检查 named checkpoints（按 handoff 第 8 节）：
- `captured_default_save_before_real_flow` / `mcp_connected`；
- Town → RouteMap → fixture 临时 Elite；
- 后台/节流窗口下 `route_exit_background_auto_actions` 至少 2 次权威变化；
- `battle_retreat_cancel_no_mutation`；Battle 确认回退后 `battle_retreat_restored_checkpoint`；
- 精英可重试；奖励等待与玩家跳过奖励；
- `route_abandon_preview` 文本为 `永久金币 +4 / 强化石 +2`；RouteMap 取消 no-op；
- RouteMap 确认后 Town 返回且金币/强化石只结算一次；
- fixture clear、PIE 停止、默认存档恢复、`real_flow_cleanup_verdict` 全绿。

失败时保留 JSON 事件、截图与 probe fingerprint，修复最小差异后重跑；绝不删除失败 JSON 后宣称通过。

### 阶段 D：视觉证据与 Luna 复审

目标目录：`Saved/VisualReview/20260819-battle-retreat-route-abandon/`。

- 生成并保留六张关键 PNG：Battle 弹窗与 RouteMap 弹窗 × 1280×720 / 1672×941 / 1920×1080。
- 每张记录 `requested` / `actual` / DPI / transport / window geometry。
- 按 `AGENTS.md` 的表现类问题规则，调用：

```powershell
& 'C:\Users\shxuw\.claude\skills\codex-vision\scripts\codex_vision.ps1' -Effort max
```

- Luna 结论至少覆盖：右上角工具栏可读、不压标题/敌方意图；End Turn/Party Qi 不重叠；RouteMap 关闭按钮固定；两个弹窗居中且不变形；无拉伸/重复工作台战斗壳/新生成艺术。
- 如环境无法达到 1672×941 或 1920×1080：保留 Luna 对可用分辨率（至少 1280×720）的审查，同时在报告中明确 blocker，不写“三分辨率通过”。

### 阶段 E：证据文档与 validator 清零

- 更新 `docs/production/2026-08-19-deepseek-handoff.md`：
  - 补 `source_commit: 69c5f4b`（或最终 HEAD）；
  - 把第 5/6/7 节替换为最新真实报告路径、计数与失败项。
- 更新 `docs/production/current-goal-acceptance.md`：
  - 记录 Task 6 最终结论；若环境分辨率 blocker 未解除，写“两层退出功能部分验收，视觉三分辨率仍阻塞”。
- 更新 `docs/production/2026-08-19-goal-progress-evidence.md` 的真实流/视觉/回归条目。
- 更新 `docs/superpowers/plans/2026-08-19-battle-retreat-route-abandon-controls.md` 的 Task 6 复选框（只勾真正完成的步骤）。
- 重跑 `python scripts/harness_state_validator.py --json`，目标 `findings=[]`。
- 再核对 `L_Main.umap` SHA256 与 `git status --short`。

### 阶段 F：安全提交（仅本轮意图文件）

```powershell
git diff --check
git add Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h `
  Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp `
  Source/GameXXK/Private/Tests/GameXXKBattleRetreatTest.cpp `
  Content/Python/gamexxk_probe_real_play_flow.py `
  scripts/gamexxk_real_play_flow_mcp.py `
  scripts/test_gamexxk_real_play_flow_mcp.py `
  scripts/test_gamexxk_real_play_flow_probe.py `
  docs/production/2026-08-19-deepseek-handoff.md `
  docs/production/current-goal-acceptance.md `
  docs/production/2026-08-19-goal-progress-evidence.md
git diff --cached --name-only
git commit -m "test: verify two-level route exit flow"
```

提交前必须确认暂存清单中没有 `L_Main.umap`、`scripts/test_battle_camera_framing.py`、根目录 `Private/` `Public/`、`SourceAssets/`、`SourceArt/`、`Content/Python/_*.py` 或任何未跟踪用户资产。

## 4. 回滚与中止条件

- 若 MCP 不可用且编辑器可能含未保存修改：停止，报告真实 blocker，不强关编辑器。
- 若截图接口实验证明显示器无法提供 1672×941 / 1920×1080：不伪造；用实际可用分辨率继续语义验收，并把视觉三分辨率登记为未完成的显式 blocker。
- 若真实流在非分辨率 checkpoint 出现失败：修复最小差异并重跑完整真实流，不手工打勾。
- 若冷 UBT/聚焦回归出现新失败：停止提交，先回退本轮最小改动定位；不回滚用户资产。

## 5. 计划内不做的事情

- 不切默认 2D 入口；不把桌面历练 2D 工作台总目标标记完成。
- 不继续四组性能采样、ImageTruth 候选、PSD 交付、天赋 read model、FIFO 箱批、工具真实配方等更大目标。
- 不动 Phase 1 地形增益重设计与 Phase 2 数值迭代。
