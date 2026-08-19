---
status: in-progress
owner: codex
updated_at: 2026-08-18
source_commit: 628c46a
decision: not-complete
goal_status: active
---

# Desktop Training 2D HUD Migration — branch record

日期：2026-08-18
分支：`codex/desktop-training-2d-hud-migration`
基线：`628c46a` (`checkpoint: desktop training visual mvp and hud-only map`)

## 目标

把“进入青山镇后再按 Tab 打开”的历练工作台迁移为桌面端的纯 2D HUD 入口，同时保留原 3D 城镇作为 Git/资产回退路径。迁移不覆盖用户修改过的 `L_Main.umap`，也不把 3D 场景 Actor 搬进 HUD 地图。

## 运行时接线

1. `GameXXKLevelFlow::MapForScreen(Town)` 在最终验收前继续返回已验收的 `/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo`；`L_DesktopTrainingHUD` 只作为直接加载的独立验收面，不提前切换玩家默认入口。
2. `L_DesktopTrainingHUD.umap` 是 `L_QingshanInn` 的隔离副本，只保留 `PlayerStart`；场景、灯光、雾、出口和放置角色不在副本中。
3. `AGameXXKMVPGameMode::SpawnDefaultPawnAtTransform_Implementation` 在 HUD 地图返回空 Pawn，避免默认城镇 GameMode 注入 3D 主角。
4. HUD 地图直接启动时，`AGameXXKMVPGameMode::BeginPlay` 只规范化 Town runtime state，不生成 3D 城镇 Actor；主菜单仍进入 3D 城镇，直到完整目标最终验收通过。
5. `AGameXXKMVPPlayerController::BeginPlay` 识别 HUD 地图后只在该地图自动启用并打开 `UGameXXKDesktopTrainingWorkbenchWidget`；原 3D 城镇仍是默认玩家入口和回退基线。
6. 工作台默认仍以历练页为主入口；`Tab`、仓库/编队/天赋/工具和挑战/游历交互继续由现有工作台统一承载。

## 回退边界

- `L_QingshanInn`、`/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo` 和用户修改的 `L_Main.umap` 未被迁移代码覆盖。
- 3D 入口已经是默认值；最终验收前不得把 `GameXXKLevelFlow::MapForScreen(Town)` 改到 HUD 地图。HUD 资产可继续通过直接加载独立验证。
- `IsTownGameplayMapPackage` 不把 HUD 地图当作 3D 城镇 Actor 生成地图；`IsDesktopTrainingHUDMapPackage` 是独立识别口。

## 验收证据

| 检查 | 结果 |
| --- | --- |
| 冷 UBT：`GameXXKEditor Win64 Development -NoHotReload -NoHotReloadFromIDE -MaxParallelActions=1` | PASS，`Result: Succeeded` |
| `GameXXK.MVP.LevelFlow` | PASS |
| `GameXXK.MVP.UI.MainMenuPlayerFlow.SaveMigration` | PASS |
| `GameXXK.MVP.PlayableShell.GameModeDefaults` | PASS |
| `scripts/test_desktop_training_hud_migration.py` | 3/3 PASS |
| 既有 walkloop/import 合同 | 6/6 PASS |
| HUD 地图清理报告 | `Saved/HarnessReports/desktop-training-hud-map.json`；仅保留 `PlayerStart` |

入口门禁红灯报告：`Saved/Automation/DesktopTrainingEntryGateRed/index.json`，明确记录旧实现把 Town 错指向 HUD；修复后的绿灯报告为 `Saved/Automation/DesktopTrainingEntryGateGreen/index.json` 与 `Saved/Automation/DesktopTrainingMainMenuGateGreen/index.json`。
此前迁移回归报告：`Saved/Automation/DesktopTrainingHUDMigration/index.json`。
冷编译报告：`Saved/HarnessReports/20260818-100806-ai-production-loop.md`。

### 1672×941 布局纠偏预检

- 预检时间：2026-08-18（Asia/Shanghai）。
- 当前根目录分支：`codex/desktop-training-2d-hud-migration`，HEAD `f0e37ac`；不创建或切换 worktree。
- `Content/GameXXK/Maps/L_Main.umap` 在布局改动前的 SHA256：`EE6E8394E40298321F2A57CC030018BDD1109EED36248597A7D7F414E387E46B`。
- 预检时该 umap 已是用户工作区中的 tracked-modified 文件；本布局批次禁止暂存、保存、覆盖或还原它，最终复核必须比较同一哈希。
- 预检时 UE 编辑器 PID `51492` 正在运行且 MCP `127.0.0.1:18765` 可连接；PIE 正在运行。冷 UBT 前必须通过 MCP 停止 PIE、保存脏包并正常退出编辑器。

## PIE 滚动探针边界

- 旧探针在 UE Python/MCP 调用内部执行 `time.sleep`；该调用运行在编辑器游戏线程上，因此等待期间 `NativeTick` 本身被暂停，`scroll_offset=0` 与 `walk_frame=0` 不能证明运行时未循环。
- `Content/Python/gamexxk_probe_training_visual_mvp.py` 现拆为 `prepare-map`、`start-travel`、`observe`、`advance` 四个立即返回的 phase。
- `scripts/run_training_visual_pie_probe.py` 在 UE 进程外等待 0.35 秒，再发起第二次 MCP 观察；验收条件为 `native_tick_count`、`scroll_offset` 与 `walk_frame` 相对启动快照均前进，并且 atlas/background 资源存在。
- 运行器的 `--launch-editor` 使用项目既有 `ue_tdd_pipeline.py` 路径解析，确保启动的是当前根目录下的 `GameXXK.uproject`；`--build` 可先做冷 UBT，`--keep-pie` 仅用于人工复核。

## 2026-08-18 顶部挂机条战斗表现与动作比例纠偏

### 根因与实现边界

- 旧实现把 1 秒一次的权威 `TravelRunner` 步进直接当作表现节拍：进入战斗时滚动立即归零，结算后又直接替换敌人；因此画面会出现进度卡顿、角色站住却没有完整待机/攻击/受击/死亡表现，以及击杀帧丢失。
- 工作台旧路径还会在逻辑步进时重建 `WidgetTree`，背景只有两段，并把顶部调试说明、奖励句子和冷却句子直接显示给玩家；这些都已从实时表现路径移除。现在使用三段背景、连续指数速度响应和独立的 `Walking → EncounterIdle → HeroAttack → EnemyHit → EnemyDeath`（非致死时包含敌方反击）表现状态机；权威奖励、失败重试和关卡进度语义不变。
- 英雄攻击 atlas 原生向左，敌人 Idle atlas 原生向右；顶部条和局内棋盘都是“敌左、我右”，因此两边都不应额外做 X 轴镜像。此前额外镜像是双方朝向反转的直接原因。
- “走路正常、Idle 缩小、Attack 再缩小”并非 UMG 槽位尺寸变化，而是不同 atlas 的透明内容占格比例不同。实测中，Walk 中位透明包围盒高度约为 cell 的 `90.6%`，Hero Idle 为 `81.15%`、Attack 为 `59.77%`、Hit 为 `69.53%`、Death 为 `80.86%`。现在所有动作使用脚底中心锚点 `(0.5, 1.0)`，保持正向、等比缩放，并按动作分别归一化：Idle `1.117`、Attack `1.516`、Hit `1.303`、Death `1.121`；Walk 保持 `1.0`。这只校正透明留白，不修改或重采样原像素素材。

### RED / GREEN 与实机证据

| 检查 | 结果与证据 |
| --- | --- |
| 朝向、脚底锚点、Idle 比例 RED | `Saved/HarnessReports/20260818-143514-ai-production-loop.md`；新增断言精确失败 6 项 |
| Attack 独立比例 RED | `Saved/HarnessReports/20260818-143805-ai-production-loop.md`；在其余修正后只剩 Attack 比例 1 项失败 |
| 冷 UBT + 朝向/动作比例聚焦 GREEN | `Saved/HarnessReports/20260818-144009-ai-production-loop.md`；`Saved/Automation/20260818-training-strip-facing-scale-green/index.json`，1/1、0 warning、0 error |
| Workbench 全前缀回归 | `Saved/HarnessReports/20260818-144601-ai-production-loop.md`；`Saved/Automation/20260818-training-strip-workbench-final-green/index.json`，14/14、0 failed、0 error |
| Travel 表现运行时 | `Saved/Automation/20260818-training-strip-final-green/index.json`，3/3 通过（CombatPresentation、NonLethalExchange、SmoothSeamlessLoop） |
| 真实 PIE 60×0.1 秒动态采样 | `Saved/HarnessReports/20260818-training-strip-motion-facing-scale-final.json`；观察到 Walking、EncounterIdle、HeroAttack、EnemyHit、EnemyDeath，英雄 Idle/Attack 与敌人 Idle/Hit/Death 均实际出现，`maximum_zero_walking_run=0` |
| Windows 桌面合成截图 | `Saved/VisualReview/20260818-training-strip-facing-scale-final.jpg`；透明 HUD 叠在真实桌面上，非 NullRHI/黑底替代图 |
| 6.3 秒连续序列 | `Saved/VisualReview/20260818-training-strip-sequence/`（36 帧）与 `Saved/VisualReview/20260818-training-strip-sequence-contact-sheet.png`（18 帧联系表） |
| Luna max 最终视觉复核 | `Saved/VisualReview/20260818-training-strip-combat-presentation-final.md`；总判定 PASS：三态身高/脚底、等比缩放、相向关系、单条桌面 HUD 全部通过，无修正项 |

验证收尾时 UE MCP 返回 `dirty_before=[]`、`dirty_after=[]` 后正常停止 PIE。`Content/GameXXK/Maps/L_Main.umap` 的最终 SHA256 仍为 `EE6E8394E40298321F2A57CC030018BDD1109EED36248597A7D7F414E387E46B`，与预检一致；本批次未保存、覆盖、还原或暂存该用户资产。`git diff --check` 通过（仅现有 LF→CRLF 提示）。

## 尚未宣称完成的部分

这次提交是入口迁移与单英雄/单敌顶部表现竖切，不等同于完整目标完成。真实 PIE 已证明当前单目标滚动与战斗表现，但共享的三敌/三我编制（最终需要六角色展示）、后台 Timer/窗口驻留、最终 PSD/图标、1920/2560 整体工作台截图和 TaskBarHero 四组性能采样仍需在本分支继续复核；工作台现有规则/战斗/游历 MVP 也不能替代最终发行实现。

## 下一步

- 在干净 HUD 地图中做一次真实 PIE：确认窗口启动后只出现 HUD，确认顶部滚动条实际位移、战斗暂停和失败重试。
- 将 HUD 入口的窗口尺寸、字体可读性、左右仓库/中间背包/右历练地图比例锁成 1920 与 2560 两档合同。
- 继续补齐 HUD 迁移所需的真实角色/伙伴、敌人和最终 PSD 图标资源，但不重绘已有像素基准。
