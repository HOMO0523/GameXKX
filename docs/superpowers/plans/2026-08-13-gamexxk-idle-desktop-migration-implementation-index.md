---
status: shelved
updated_at: 2026-08-17
shelved_reason: legacy migration index; do not execute
superseded_by: docs/superpowers/specs/2026-08-17-gamexxk-desktop-training-workbench-design.md
---
# GameXXK Idle Desktop Migration Implementation Index

> 执行冻结：当前 `CurrentSaveVersion=17`，本文的 v15/v16/v17/v18 迁移边界不能直接执行。恢复历练实现必须以 `2026-08-17-gamexxk-desktop-training-workbench-design.md` 和新的 Phase 0 基线重新编排迁移编号。

> **状态(2026-08-14):用户决定整体搁置。** 执行前必须处理以下两点:
> 1. **存档版本边界已过期**:本文按 v16/v17/v18 规划,但奖励体系重构(`00002f1`)已把 `CurrentSaveVersion` 提到 **17**。恢复执行时须重排为:包 1-5 落在 v17→v18(历练核心+存档)、包 6 v18→v19(任务 NPC 语义)、包 7 v19→v20(默认入口),并同步修订包 1/6/7 各自计划中的版本断言。
> 2. 进入时须以 `docs/production/2026-08-14-unfinished-inventory-optimization-roadmap.md` Phase 3 为准,先取干净基线再开工。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把已确认的历练放置、双宝箱、2D 主界面、桌面迷你窗、局内自动战斗、任务 NPC 显式入队和默认入口迁移拆成七个可独立回滚、可独立验收的实施包。

**Architecture:** 历练规则先进入无 UI 的纯 C++ 层，并通过 `FGameXXKRuntimeState` 与新存档版本持久化；完整界面和原生迷你窗只消费同一只读展示模型。自动战斗只调用现有 `FGameXXKCardBattleAdapter` 与结果预演，任务 NPC 使用独立版本迁移结束 v15 follower 语义，最后才把默认入口切换到 2D 历练，3D 城镇资产始终保留。

**Tech Stack:** Unreal Engine 5.8、C++17、USTRUCT/SaveGame、UMG、Slate、GameXXK MVP/CardBattle/Equipment 规则、UE Automation、UBT、UnrealEditor-Cmd、项目 MCP/PIE Python harness。

---

## 0. 本索引的约束

设计真源：`docs/superpowers/specs/2026-08-12-gamexxk-idle-desktop-migration-design.md`。

当前代码基线为 `main@99c669041436be526ab22a14298ecfb8c30cc3f5`，但工作区仍有上一项目标预演任务的 tracked 修改：

```text
Content/Python/gamexxk_probe_real_play_flow.py
Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp
Source/GameXXK/Private/Tests/GameXXKBattleHudFixtureTest.cpp
Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h
scripts/gamexxk_real_play_flow_mcp.py
scripts/test_gamexxk_real_play_flow_mcp.py
```

这些文件不属于本文档提交。执行任何实施包前必须先让上一任务独立提交或明确撤销，重新 `fetch`，并确认目标文件无重叠。GameXXK 禁止 worktree、UnrealBridge、Live Coding 和 Hot Reload。

统一命令变量：

```powershell
$Project = 'D:\UE5 demo\GameXXK\GameXXK.uproject'
$EditorCmd = 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$Build = 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat'
$ReportRoot = 'D:\UE5 demo\GameXXK\Saved\Automation'
```

统一冷编译：

```powershell
& $Build GameXXKEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReload -NoHotReloadFromIDE
```

预期：退出码 `0`，日志末尾 `Result: Succeeded`。每次 Automation 都使用新的 `-ReportOutputPath`，并核对 `index.json` 的 discovered/succeeded/failed/notRun/inProcess/warnings/errors；目录名中的 `GREEN` 不算证据。

## 1. 文件与版本边界

| 包 | 存档边界 | 核心产物 | 独立完成态 |
|---|---:|---|---|
| 1. 历练核心与存档 | v15→v16 | `IdleState`、UTC 时间源、离线结算、章节解锁、原子 repository | 无 UI 也可保存、加载、分段结算 |
| 2. 在线掉箱与奖励账本 | 保持 v16 | 在线 scheduler/drop、两类箱、FIFO、固定种子、领取/开箱 | 单机后台可稳定产出并交易性领取 |
| 3. 完整 2D 历练界面 | 保持 v16 | PresentationModel、全屏 IdleHome、折叠工具栏 | 可在现入口显式打开，不切默认入口 |
| 4. 桌面迷你窗 | 保持 v16 | 原生 `640×180` Slate 窗、置顶/拖动/DPI/抽屉 | 与全屏界面共享快照，不重复结算 |
| 5. 自动战斗与 2× | 保持 v16 | AutoBattlePolicy、Board 驱动、稳定选择、表现倍率 | 首次通关战斗可随时自动/安全关闭 |
| 6. 任务 NPC 新语义 | v16→v17 | 接任务不跟随、显式入队/移出、旧档迁移 | Accepted、位置、PartySelection 三者解耦 |
| 7. 默认入口与认证 | v17→v18 | 2D 默认入口、3D 开发入口、端到端与调优报告 | 新建/继续均进入 IdleHome，旧资产未动 |

v16 只引入历练结构，v17 专门结束 follower 兼容语义，v18 专门规范默认 2D 入口。不得把三个版本变化合成一次不可审查的迁移，也不得在已经生成的 current-version 存档上偷偷改变 Screen 语义。

## 2. 计划清单

1. [历练核心与存档](2026-08-13-gamexxk-idle-core-save-implementation.md)
2. [在线掉箱、账本与开箱](2026-08-13-gamexxk-idle-chest-ledger-implementation.md)
3. [完整 2D 历练界面](2026-08-13-gamexxk-idle-home-ui-implementation.md)
4. [桌面迷你窗](2026-08-13-gamexxk-desktop-mini-window-implementation.md)
5. [自动战斗与 2× 表现](2026-08-13-gamexxk-auto-battle-implementation.md)
6. [任务 NPC 显式入队](2026-08-13-gamexxk-explicit-quest-npc-party-implementation.md)
7. [默认入口与端到端认证](2026-08-13-gamexxk-idle-default-entry-e2e-implementation.md)

## 3. 统一类型名称

后续七份计划只能使用下列公共名称，禁止再造同义类型：

```cpp
enum class EGameXXKIdleChestType : uint8 { Normal, Boss };
enum class EGameXXKIdleChestContainer : uint8 { PendingLedger, Inventory };
enum class EGameXXKIdleMonsterKind : uint8 { Normal, Elite };
enum class EGameXXKIdleMode : uint8 { Offline, Online };

struct FGameXXKIdleEfficiencySnapshot;
struct FGameXXKIdleChestBatch;
struct FGameXXKIdleState;
struct FGameXXKIdleSettlementRequest;
struct FGameXXKIdleSettlementResult;
struct FGameXXKIdlePresentationSnapshot;
struct FGameXXKAutoBattleDecision;
```

`FGameXXKRuntimeState` 新增唯一字段 `FGameXXKIdleState IdleState`；宝箱实例权威只在 `IdleState.ChestBatches`，`RuntimeState.Inventory` 的两种箱数量只是兼容/UI 镜像。章节使用整数 `1..3`，与 `FGameXXKRouteProgress.CurrentChapter` 保持一致。

## 4. 工作包执行门禁

- [ ] **Step 1: 完成上一项未提交任务**

只允许上一任务自己提交或撤销六个 dirty 文件；本迁移不得替它清理。

- [ ] **Step 2: 获取干净入口**

```powershell
git fetch origin main
git branch --show-current
git rev-parse HEAD
git rev-parse origin/main
git status --short --untracked-files=no
```

预期：`main`、两个 SHA 相同、tracked/index 空。若不满足，停止当前包，不用 `git reset --hard` 或 `checkout --`。

- [ ] **Step 3: 每包严格 RED→GREEN**

每个实现任务都依次执行：写精确失败测试、冷 UBT/精确 Automation 亲眼确认 RED、最小实现、冷 UBT、精确与邻接 GREEN、mutation RED、恢复、最终 GREEN、只读复审。

- [ ] **Step 4: 每包独立提交**

每次只 `git add` 该任务列出的目标文件，再运行：

```powershell
git diff --cached --check
git diff --cached --name-only
git commit -m '<plan-specified message>'
git push origin main
git fetch origin main
git rev-parse HEAD
git rev-parse origin/main
git status --short --untracked-files=no
```

预期：提交范围精确、push 后两个 SHA 相同、tracked/index 空。

## 5. 跨包不变量

- UI 打开、Hover、Tick、动效和窗口重建都不得推进结算游标。
- 所有资源变更都使用候选状态，只有保存成功后才替换运行时。
- 完整界面和迷你窗只能读取 `FGameXXKIdlePresentationSnapshot`。
- 在线与离线共享一个 `LastSettlementTimeUtcTicks`，不能消费同一时间段两次。
- 历练容量只统计 `Container==PendingLedger` 的宝箱；转入背包即释放历练容量，但批次元数据继续保留到开箱完成。
- 两类宝箱独立冷却/随机序列，容量满时一起冻结；金币经验继续到 7 天。
- AI、模拟器和 UI 不复制卡牌公式；出牌只走 Adapter，评分只读真实候选结果。
- 接受任务不自动跟随或入队；只有显式 PartySelection 影响战斗投影。
- 任何实施包都不得删除或批量修改 3D 城镇、角色、PaperZD、地图、相机、PCG、HD2D 手调资产。
- 后续装备十档稀有度、珠子、9 合 1、Steam 登录/市场/服务器均不在这七包内。

## 6. 最终交付顺序

```text
clean baseline
  → idle core/save v16
  → online chest/ledger
  → full 2D idle home
  → native mini window
  → auto battle / 2×
  → quest NPC split v17
  → default-entry cutover v18 + E2E certification
```

不得跳过包 1/2 直接让 UI 自己算奖励，也不得在包 3 就切默认入口。包 5 可在包 3 后开发，但最终切入口前必须与包 6 一起通过真实 PIE。
