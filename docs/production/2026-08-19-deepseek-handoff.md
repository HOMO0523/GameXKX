---
status: handoff_in_progress
owner: codex
handoff_to: DeepSeek
updated_at: 2026-08-19
branch: main
head_at_handoff: 69c5f4b
source_of_truth: docs/production/current-goal-acceptance.md
---

# GameXXK 后续开发交接 / 进程文档

这份文档用于把当前 `main` 根工作区交给下一位代理继续。它不是“项目已完成”声明，而是当前代码、证据、未完成门禁和安全边界的准确快照。接手后先读本文件、`AGENTS.md`、`docs/production/current-goal-acceptance.md`，再开始执行命令。

## 1. 一句话结论

“局内自动出牌 + 两层退出确认”运行时功能已经实现，v23 战斗入口检查点、Battle 退回原地图节点、RouteMap 结算退出、UI 确认弹窗和聚焦 C++ 测试均已落地；目前剩余的是 Task 6 的最终真实 PIE/多分辨率视觉取证、Luna 复核、滚动证据更新和安全提交。不要把更大的“桌面历练 2D 工作台”总目标标记为完成。

## 2. 用户已经冻结的产品语义

必须保持下面的语义，不要重新设计成自动选路或自动领取奖励：

- 进入传送门后沿用杀戮尖塔式地图；玩家自己点击地图节点。
- 遭遇怪物后，`自动战斗` 只在战斗中自动选择合法卡牌并推进战斗；它不点击路线节点、奖励、重试、商店或任何确认按钮。
- Battle 界面右上角固定放置 `自动战斗`，其右侧是 `关闭`；`回合结束` 继续在右下角。
- Battle 的 `关闭` 必须弹窗确认。取消是严格 no-op；确认后丢弃本场战斗和未领取奖励，回到进入本场前的路线节点，当前精英/普通怪仍可重新挑战。
- RouteMap 右上角固定放置 `关闭挑战`，也必须弹窗确认。弹窗显示现有 `/20` 金币、`/10` 强化石规则计算出的准确预览；确认后按当前关卡进度结算一次并回到 Town，取消不改变任何状态。
- 两个弹窗打开后都要阻断底层点击和战斗/路线状态突变；Escape 等价于取消。

批准的设计和实施计划：

- `docs/superpowers/specs/2026-08-19-battle-retreat-route-abandon-controls-design.md`
- `docs/superpowers/plans/2026-08-19-battle-retreat-route-abandon-controls.md`

## 3. 已完成的实现

### 3.1 v23 战斗入口检查点和迁移

主要文件：

- `Source/GameXXK/Public/GameXXKMVPRules.h`
- `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- `Source/GameXXK/Private/Tests/GameXXKBattleRetreatTest.cpp`

已完成内容：

- 新增 `FGameXXKBattleEntryCheckpoint`，保存源节点、进入前的 CurrentRouteNode、DungeonMap 索引、HP/MP、visited/reachable 快照。
- `CurrentSaveVersion` 已提升到 23。
- v22 旧存档只在“生成路线、正在 Battle/Elite/Boss、存在唯一 visited 入边父节点”时自动恢复可撤退检查点；入边不明确时保留安全的无效检查点并写 warning，不能伪造退回位置。
- 进入 Battle/Elite/Boss 前在任何状态覆盖前捕获检查点；战斗胜利仅打开未领取奖励时不清除检查点。
- 已提交 checkpoint 和规则交易相关提交：`7d2a9f2`、`afbce91`。

### 3.2 Battle 退回交易

- `UGameXXKMVPRules::RetreatCurrentBattleToRoute` 以 Candidate 副本执行原子恢复，失败不泄漏部分状态。
- 恢复进入前的 current/index/HP/MP/visited/reachable，清理 pending node、CardBattle、legacy battle、敌方意图/表现投影、pending reward、target/choice 残留和检查点。
- `UGameXXKMVPSubsystem::RetreatCurrentBattleToRoute` 只是规则委托，不负责旅行、关闭 Widget、切换自动战斗或隐式存档。
- 已提交规则交易：`afbce91`。

### 3.3 RouteMap 放弃结算

- `PreviewAbandonedRouteSettlement` 为纯预览，不修改 runtime、receipt、库存或永久货币。
- `AbandonDungeonToTown` 复用已有 `/20` 永久金币、`/10` 强化石和 receipt 幂等规则。
- UI 确认后由子系统应用结算，之后由 UI 负责打开 Town；重复确认不会重复奖励。
- 已提交 facade/UI 相关提交：`ebd739a`、`69c5f4b`。

### 3.4 BattleBoard UI

主要文件：

- `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`

已完成内容：

- 顶部右侧工具栏顺序为 `自动战斗` → `关闭`，不再占用回合结束区域。
- Battle 退出弹窗使用现有 ink button texture 和 MasterV2 paper，不生成新艺术资源。
- 弹窗打开时暂停自动战斗和所有会改变状态的操作；表现队列排空前确认按钮保持禁用。
- 确认成功后调用 subsystem retreat，再回到生成路线图；失败保留弹窗并显示错误。
- Escape 取消；自动战斗开关状态不因打开/取消弹窗被意外翻转。
- 纯几何/交互 seam 已覆盖 1280×720、1672×941、1920×1080 的安全区和 Party Qi/End Turn 关系。

### 3.5 RouteMap UI

主要文件：

- `Source/GameXXK/Public/UI/GameXXKOneGameRouteMapWidget.h`
- `Source/GameXXK/Private/UI/GameXXKOneGameRouteMapWidget.cpp`

已完成内容：

- `关闭挑战` 是 RootOverlay 的固定右上角按钮，不随 ScrollBox 地图滚动。
- 弹窗打开时禁用 ScrollBox、节点点击和拖拽；取消不产生 mutation。
- 确认时重新读取预览、应用结算、返回 Town；失败保留弹窗。
- Escape 取消。

## 4. 已提交的功能提交

当前 HEAD：`69c5f4b feat: settle route from map close control`

相关提交从新到旧：

```text
69c5f4b feat: settle route from map close control
3f5b3b1 feat: add battle retreat confirmation
ebd739a feat: expose abandoned route settlement
afbce91 feat: restore pre-encounter route state
7d2a9f2 feat: save route battle entry checkpoints
9234b5c docs: align route exit TDD checkpoints
4cfeb50 docs: plan two-level route exit controls
91aabe1 docs: approve two-level route exit design
3fd5035 fix: drive auto battle from wall clock
9f8c4b6 feat: auto-play legal cards in existing battles
```

## 5. 当前未提交但属于本轮 Task 6 的改动

这些改动已经存在工作树，接手后先审阅再决定是否拆分提交：

- `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- `Source/GameXXK/Private/Tests/GameXXKBattleRetreatTest.cpp`
- `Content/Python/gamexxk_probe_real_play_flow.py`
- `scripts/gamexxk_real_play_flow_mcp.py`
- `scripts/test_gamexxk_real_play_flow_mcp.py`
- `scripts/test_gamexxk_real_play_flow_probe.py`

新增内容主要是：

- 非生产路径的 `RouteExitAcceptanceFixtureForTest`：在真实 PIE 中把第一个可达 Battle 临时变成 Elite，种入 `route money=99`、`acquisition count=29`，并保存可恢复的 transient backup；clear 时恢复原状，不写生产存档。
- Probe 的 runtime summary：HP/MP、route ids、checkpoint、money/acquisition、phase/round、active battle、pending reward、stones、auto setting、稳定 GUID。
- Probe/harness 的双层退出验收流程和状态 fingerprint。
- Probe 新增 `--high-res-screenshot`，当前还没有证明在 PIE 中可靠完成。

不要把这个测试 fixture 调进生产玩家流程或 Challenge 入口。

## 6. 已有验证证据

最后一次已知成功的冷 UBT（fixture 变更后）：

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=1
```

退出码为 0。接手后仍需在最终提交前重跑一次冷 UBT。

已知聚焦 C++ 结果：

| suite | 最近已知结果 |
|---|---:|
| `GameXXK.Route.BattleRetreat` | 12/12 |
| `GameXXK.Route.Settlement` | 4/4 |
| `GameXXK.MVP.SaveGame` | 12/12 |
| `GameXXK.MVP.RouteMap` | 6/6 |
| `GameXXK.Integration.CardRoute` | 19/19 |
| `GameXXK.Integration.CardBattle` | 38/38 |
| `GameXXK.Integration.CardBattle.BoardRetreat` | 4/4 |
| `GameXXK.Integration.CardBattle.BoardAutoPlay` | 8/8 |
| `GameXXK.Route.BattleRetreat.DevelopmentFixture` | 1/1 |

Python harness 在高分辨率改动之前曾通过 113/113；高分辨率改动之后只做过定向 py_compile/contract 检查，因此最终提交前必须重新跑完整 Python 子集。

之前一次完整真实流程曾观察到以下语义证据：

- 在后台/节流窗口仍出现至少两个权威自动出牌变化；
- Battle 关闭弹窗取消后 state fingerprint 完全不变；
- 确认退出恢复进入前 current/index/HP/MP/visited/reachable，精英节点仍可重试；
- RouteMap 预览为 `永久金币 +6 / 强化石 +2`；
- 取消不改变状态，确认回 Town 且只结算一次；
- fixture 清理、PIE 停止和默认存档恢复成功。

但是，这次成功运行的 JSON 已被后续多分辨率尝试覆盖，不能把当前报告当作绿灯。

## 7. 当前真实阻塞和证据位置

最新报告：`Saved/HarnessReports/battle-retreat-route-abandon-real-flow.json`

当前报告明确是 `ok=false`，失败原因：

```text
Resolution evidence mismatch for route_exit_battle_retreat_modal:
requested=1672x941 captured=(1556, 884)
resize={logical_size:[1672,941], physical_size:[2090,1176], dpi:96, logical_scale:[0.8,0.8]}
```

已观察到的截图/接口问题：

1. 通过 PreviewWindow resize，1280×720 截图实际得到过 1280×720；1672×941 在系统 DPI 0.8 下实际得到 1556×884；1920 轮次因前一项失败没有完成。
2. `unreal.AutomationLibrary.take_high_res_screenshot` 在 PIE 主菜单 smoke 中轮询 30 秒后返回 `screenshot_task_timeout`，不能直接宣称可用。
3. `Saved/VisualReview/20260819-battle-retreat-route-abandon/` 目前只留下 `_highres_smoke_1280x720.png`；旧的 1536×816/1280×720证据在 `Saved/Codex/`，但不是三分辨率最终门禁。
4. Luna 的布局建议已经保存，但还没有对最终三分辨率截图做最终 max 审查：
   - `Saved/VisualReview/20260819-route-owned-auto-battle/luna-top-right-layout-advice.md`
   - `Saved/VisualReview/20260819-route-owned-auto-battle/luna-route-close-layout-advice.md`

处理原则：修好接口后重新生成完整证据；如果环境确实无法提供目标分辨率，必须记录实际分辨率和原因，不要改 JSON 伪造通过。

## 8. DeepSeek 的推荐续作顺序

### 第一步：安全接管并确认编辑器状态

```powershell
git branch --show-current
git log -1 --oneline
git status --short
(Get-FileHash -Algorithm SHA256 'Content/GameXXK/Maps/L_Main.umap').Hash
Get-Process UnrealEditor -ErrorAction SilentlyContinue | Select-Object Id,ProcessName,Path
```

预期分支是 `main`；`L_Main.umap` 的保护 hash 是：

```text
EE6E8394E40298321F2A57CC030018BDD1109EED36248597A7D7F414E387E46B
```

如果编辑器仍运行且可能有 dirty packages，先通过 UE MCP 保存；不要强制关闭编辑器。

### 第二步：先修高分辨率取证，再改生产代码

优先检查：

- `scripts/gamexxk_real_play_flow_mcp.py::capture_resolution_matrix`
- `PreviewWindowController.resize_preview_window_logical`
- `Content/Python/gamexxk_probe_real_play_flow.py::_handle_high_res_screenshot`
- 已有可参考实现：`Content/Python/gamexxk_qingshan_dress_b1_acceptance.py` 中的 `take_high_res_screenshot` 轮询和 viewport 准备逻辑。

先做最小 smoke，不要把错误截图当验收证据。确认 PIE Game View 是否支持 `AutomationLibrary` 的 task，或改用 MCP Slate 截图/明确的窗口物理尺寸换算；最终 JSON 必须记录请求尺寸、实际 PNG 尺寸、DPI 和 transport。

### 第三步：重跑 TDD、冷 UBT 和聚焦/回归门禁

```powershell
python -m py_compile Content/Python/gamexxk_probe_real_play_flow.py scripts/gamexxk_real_play_flow_mcp.py
python -m unittest scripts.test_gamexxk_real_play_flow_mcp scripts.test_gamexxk_real_play_flow_probe scripts.test_ue_pie_lifecycle

& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=1

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

python scripts/ai_production_loop.py --run-script-tests --script-tests all --json
python scripts/harness_state_validator.py --json
git diff --check
```

已知的 `GameXXK.MVP.UI.MainMenuPlayerFlow.SaveMigration` 历史基线失败若重现，要单独记录，不要隐藏也不要算作本轮新回归。

### 第四步：重跑真实双层退出流程

编辑器已保存、MCP 可用时执行：

```powershell
python scripts/gamexxk_real_play_flow_mcp.py --two-level-exit-acceptance --timeout 600 --report Saved/HarnessReports/battle-retreat-route-abandon-real-flow.json
```

必须检查报告中的 named checkpoints：进入 Town → 路线图 → 临时 Elite → 自动后台出牌 → Battle 取消 → Battle 确认退回 → 精英重试 → 奖励处理 → RouteMap 预览取消 → RouteMap 确认 → Town → fixture/存档清理。

### 第五步：完成视觉审查和证据文档

目标截图目录：`Saved/VisualReview/20260819-battle-retreat-route-abandon/`。至少需要 Battle/RouteMap 两个弹窗在 1280×720、1672×941、1920×1080 的实际 PNG，并通过 Luna max：

```powershell
& 'C:\Users\shxuw\.claude\skills\codex-vision\scripts\codex_vision.ps1' -Effort max
```

审查结论要覆盖：右上角工具栏可读且不压住标题/敌方意图；End Turn/Party Qi 不重叠；RouteMap 关闭按钮固定；两个弹窗居中且不变形；没有拉伸、重复工作台战斗壳或新生成艺术。

随后更新：

- `docs/production/current-goal-acceptance.md`
- `docs/production/2026-08-19-goal-progress-evidence.md`
- `docs/superpowers/plans/2026-08-19-battle-retreat-route-abandon-controls.md` 的 Task 6 复选框

只写真实报告路径、计数和已知失败，不把旧报告冒充本轮结果。

### 第六步：只提交本轮意图文件

在所有验证结束后，先查看暂存清单：

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
  docs/production/2026-08-19-goal-progress-evidence.md `
git diff --cached --name-only
git commit -m "test: verify two-level route exit flow"
```

绝对不要用 `git add -A` 或 `git add .`。以下内容属于用户资产、历史探针或外部源，除非用户明确授权，不得覆盖、回滚或纳入本次提交：

- `Content/GameXXK/Maps/L_Main.umap`
- `scripts/test_battle_camera_framing.py`
- `SourceAssets/`
- `SourceArt/`
- 根目录未跟踪的 `Private/`、`Public/`
- `Content/Python/_*.py` 一次性探针

## 9. 更大的项目目标仍未完成

即使 Task 6 完成，也不能结束总目标。当前滚动真源仍指出：

- 四组同机性能门禁（空壳、历练静置、局内 ChallengeViewport、3D 青山镇）尚未全部完成，默认 2D 入口不能提前切换。
- ImageTruth 目前只有 8 张确认图；顶部按钮、Tab、节点状态、挑战/游历/重试、工具和局内图标仍需逐张确认。
- 最终 PSD 可编辑交付、完整 1920/2560 工作台截图、三敌三我连续画布和最终字体/tooltip 可读性仍缺。
- 普通/困难/地狱状态、失败重试、真实天赋 read model、FIFO 箱批/容量/箱内物品和工具真实配方仍需继续。
- asset-contract 当前存在显式 blocker（外部 PSD/地形源缺失、保护地图 hash 漂移、视觉合同漂移和未实现 golden-asset 合同）；不能修改保护资产追求假绿。

这些事项继续以 `docs/production/current-goal-acceptance.md` 为唯一滚动状态，以 `docs/production/2026-08-19-goal-progress-evidence.md` 为证据分类记录。

## 10. 交接完成标准

DeepSeek 只有在下列条件全部有新鲜证据后，才可以把本轮“两层退出功能”标记完成：

- 真实 PIE/MCP 双层流程报告 `ok=true`，且不依赖生产自动点击确认。
- Battle 取消 no-op、确认恢复精确检查点且可重试；RouteMap 取消 no-op、确认精确结算一次。
- 冷 UBT、聚焦/回归 Automation、Python harness、状态 validator 均通过；已知历史 baseline 单独列出。
- 三个实际分辨率的 Battle/RouteMap 弹窗截图存在并通过 Luna max；若环境限制无法达成，则明确记录阻塞，不伪造通过。
- `L_Main.umap` 保护 hash 未变，用户调过的地图、相机、HD2D plane、动画和源美术未被覆盖。
- 任务计划、滚动指针和证据日志已更新，提交清单不含保护文件。
