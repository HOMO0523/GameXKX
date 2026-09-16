---
unit_id: 2026-09-16-architecture-presentation-timeline
status: reference
owner: codex
updated_at: 2026-09-16T13:30:00+08:00
source_commit: edf280cbfb274655e381c9223b98d61904875e41
depends_on: []
---

# 架构层 / 表现层 演进时间线

> 本文写于 2026-09-16，回顾 2026-07-01 → 2026-09-11 的设计演进。
> 文中出现的 `2026-09-11-*` 名称（如打包目录 `ShippingF10-20260911-*`）是当时构建的真实名字，未改名。

## 0. 这份文档回答什么

项目里"架构层"和"表现层"的**初始约定是什么、后来怎么变的、现在是什么**。

每个结论都标了出处文件，可以逐条核对。本文是回顾性整理，不改变任何运行时语义；语义仍然以
`docs/production/current-goal-acceptance.md` 和 `docs/superpowers/specs/` 里的原始设计为准。

规模参考：`docs/superpowers/specs/` **118 份**设计，`docs/superpowers/plans/` **188 份**计划，
时间跨度 2026-07-01 → 2026-09-15（7 月 54 份、8 月 54 份、9 月 10 份设计）。

## 1. 先澄清"prompt"在项目里指什么

**项目没有独立的 prompt 文件。** 这套工程用的是 `superpowers` 工作流，用两种文档承载需求：

| 载体 | 位置 | 作用 |
|---|---|---|
| `specs/` | `docs/superpowers/specs/` | 设计：确认后的意图与契约 |
| `plans/` | `docs/superpowers/plans/` | 实施：分步计划 |
| `00-raw-input.md` | `docs/production/<unit>/` | **逐字记录用户原话**。全项目只有 1 份 |
| `AGENTS.md` | 项目根 | AI agent 的长期硬约束 |

- **初始 prompt** ≈ `docs/superpowers/specs/2026-07-01-gamexxk-mvp-design.md`，首页写着
  `Status: confirmed by user on 2026-07-01`。这是立项当天用户确认的设计。
- **唯一逐字 prompt** = `docs/production/2026-07-03-playable-entry-flow/00-raw-input.md`。
- **长期约束 prompt** = `AGENTS.md`。

## 2. 初始分工（2026-07-01）

项目名《霞客行》，UE 5.8。出处：`2026-07-01-gamexxk-mvp-design.md` §Technical Architecture。

### 2.1 三层职责

| 层 | 归属 | 内容 |
|---|---|---|
| **规则层** | **C++ 独占** | 世界地图区域解锁、任务状态、城镇交互规则、跟随者状态、节点地牢模型与可达性、战斗模型与回合结算、背包/装备/交易数据、SaveGame 读写 |
| **表现层** | **Blueprint + UMG + Paper2D + PaperZD** | 城镇关卡拼装、TileMap 或 blockout 美术、Widget 布局、PaperZD 动画图与翻页书指派、右键面板、节点地图显示、战斗指令 UI |
| **数据层** | **DataAsset / DataTable** | 道具、装备数值、技能、敌人、Boss、商店库存、地牢节点定义、区域定义 |

### 2.2 同期定下的硬性要求

- **可测试性**：规则必须能在"不进入完整交互式 UE 场景"的情况下测试。文档列了 10 条最小验证目标
  （任务未接受时阻止进地牢、只暴露可达节点、速度决定出手顺序、破盾持续一回合、失败保留经验金币、
  Boss 胜利解锁潭江渡口、买卖正确改金币与堆叠、存档持久化区域/任务/等级/经验/金币）。
- **参考项目边界**：`D:\UE5 demo\TestMap`（UE 5.4）只作为原型与参考内容迁移，
  **不允许其 Blueprint 原型逻辑成为全局运行时真相**。
- **表现层可变性**：文档写明"如果时间紧，**先砍表现层**——保留 C++ 规则和 UI 流程，推迟战斗动画和城镇打磨"。

## 3. 时间线

### 阶段 ① 2026-07-01 · 立项，分工确立

`2026-07-01-gamexxk-mvp-design.md`（14.5 KB）。表现层基座 = Paper2D TileMap 城镇 + UMG 地牢/战斗。
权威流程：主菜单 → 世界地图 UI → 青山驿站 → F 接任务 → 跟随者 → 节点地牢 → 战斗/事件/营地/Boss →
失败回城交易重试，或胜利解锁下一区域。

### 阶段 ② 2026-07-02 ~ 07-09 · 原型迁移，第一条闭环

| 日期 | 文档 | 内容 |
|---|---|---|
| 07-02 | `1game-ui-framework-analysis` | 迁移 `Content/1Game`（Slay-the-Spire 式节点地图原型） |
| **07-03** | `playable-entry-flow` | **唯一的 `00-raw-input.md`**，逐字记录验收要求 |
| 07-05 | `1game-island-battle-map` | 战斗地图 |
| 07-06 | `gamexxk-owned-route-map` | 路线图由参考项目转为**自有资产** |
| 07-07 | `battle-command-targeting` | 战斗指令与选靶（44.8 KB） |

### 阶段 ③ 2026-07-10 ~ 07-13 · 3D 青山镇 —— 表现层第一次转向

`qingshan-town-pcg-whitebox` → `qingshan-pcg-dress-b1` → `qingshan-golden-inn-gate1` →
`asian-village-migration` → `player-occlusion-reveal` → `occlusion-foot-seal` → `player-occlusion-depth-gate`

城镇由 Paper2D TileMap 改为 **3D PCG 场景**（Asian Village 资产迁移）。角色仍是 Paper2D/PaperZD
精灵贴在 3D 场景上。随之出现只在 3D 下才有的问题族：玩家遮挡、遮挡材质覆盖、脚部封印、深度门。

### 阶段 ④ 2026-07-14 ~ 07-26 · 表现层成型

| 日期 | 文档 | 内容 |
|---|---|---|
| 07-14~16 | `gamexxk-ui-world-map`、`town-hud-reference-refresh`、`two-tab-task-panel`、**`party-deck-companion-system`（56.9 KB）** | UI 世界地图、城镇 HUD、双页签任务面板、伙伴系统 |
| 07-17~21 | `battle-card-readability`、`battle-intent-slot-feedback`、`battle-status-icon-language`、`central-battle-hud-projection`、`battle-fixed-slot-hud` | 战斗可读性、意图槽、状态图标语言、HUD 投影、固定槽位 |
| 07-22 | `meta-equipment-partner-three-chapter-route`（32.4 KB）、`route-merchant-card-quality-upgrade`（31 KB） | 元层装备/伙伴/三章路线、商人与卡牌品质 |
| 07-24~25 | `video-matting-to-ue-flipbook`、`battle-animation-seven-asset-pilot` | **战斗动画由视频抠像转 PaperZD 翻页书** |
| 07-26 | `fullscreen-hud-battle-visual-system` | 全屏 HUD 战斗视觉体系 |

### 阶段 ⑤ 2026-08-04 ~ 08-06 · UI 主轴统一

`ui-master-components` → `master-ui-family-correction` → `approved-ui-engine-integration` →
`final-ui-runtime-integration`

表现层收敛为一套**「主控件族 + PSD 权威源」**。这是今天 `Content/GameXXK/UI/MasterV2/Approved/`
那批资产的来源。

### 阶段 ⑥ 2026-08-12 · 表现层第二次转向：挂机桌面

出处：`2026-08-12-gamexxk-idle-desktop-migration-design.md`（33 KB）。全项目最重要的一次架构重构。

**方案裁决**：「双结算器、统一状态与账本」。文档明确比较并淘汰了两种替代方案——
"在线离线都用期望值"（怪物死亡与宝箱出现脱节，削弱放置反馈）和
"在线离线都完整重放"（长离线成本、版本兼容、存档复杂度不成比例）。

**架构层拆为 10 个单一职责模块（§5.1）：**

| 模块 | 单一职责 |
|---|---|
| `TimeSource` | 返回当前 UTC；现用本机时间，未来可替换为 Steam/服务器时间 |
| `IdleState` | 章节、效率快照、时间游标、双冷却、离线余额、容量、宝箱 FIFO |
| `IdleEncounterScheduler` | 进程运行时按效率快照产生普通怪/精英怪死亡时间线，**与 UI 帧率解耦** |
| `OnlineDropResolver` | 消费真实怪物死亡事件，执行双宝箱冷却与概率抽取 |
| `OfflineYieldCalculator` | 按时间、刷图效率、掉落配置算期望收益，**不运行完整战斗** |
| `RewardLedger` | 待领金币、经验、宝箱，执行领取事务 |
| `ChestResolver` | 固定种子生成奖励，处理部分领取与剩余奖励 |
| `IdlePresentationModel` | 向完整界面和迷你窗提供**相同的只读展示快照** |
| `AutoBattlePolicy` | 从合法行动中选出牌/目标/结束回合，**不选择路线** |
| `IdleRepository` | 读取、校验、迁移并原子保存；未来可替换远端实现 |

**三条边界规则（§5.2，硬约束）：**

1. **规则模块不引用 UMG 或 Slate**；UI 只能消费只读模型并发出命令。
2. **动效和在线刷怪画面不是奖励权威来源**；奖励只由结算器写入账本。
3. 活动历练在玩家查看背包/队伍/路线/首次通关战斗时**仍在后台继续**；切 UI 不暂停、不重置、不额外加速。

**表现层：** 新增**桌面迷你窗**（独立窗口形态）+ 完整 2D 历练界面。

**3D 城镇退场三阶段（§16）：**

| 阶段 | 内容 |
|---|---|
| 1 并行兼容 | 新增历练状态与 2D 界面；保留 3D 地图/NPC/相机/手工资产；不删除或批量重定向资源；旧入口仍可用于开发验证 |
| 2 2D 成为默认入口 | 新游戏与继续游戏默认进 2D 历练主界面；3D 城镇降为可选旧内容或开发入口 |
| 3 退役裁决 | 仅在任务/商店/NPC/队伍/背包/章节入口/存档迁移全部通过真实流程验收后另行决定。**本文不授权删除任何地图、角色、PaperZD、相机、PCG 或 HD2D 调整资产** |

### 阶段 ⑦ 2026-08-17 ~ 08-30 · 桌面工作台定型

| 日期 | 文档 | 关键性 |
|---|---|---|
| 08-17 | `gamexxk-desktop-training-workbench-design`（33.5 KB） | 桌面工作台 |
| 08-19 | `desktop-training-hud-lazy-lifecycle`、`route-owned-auto-battle-correction` | 懒加载生命周期、路线自动战斗纠偏 |
| **08-21** | **`desktop-2d-battle-canonical-flow`（3.5 KB）** | **权威流程冻结**；`status: approved`，`supersedes: default-3d-town-entry`、`desktop-challenge-route-prerequisite` |
| 08-22 | `gamexxk-graduation-showcase`、`workbench-route-formation-talent-system` | 毕业展示、工作台路线/编队/天赋 |
| 08-26 | `desktop-background-layer-cutout`、`desktop-town-toggle-button` | 桌面背景层、城镇切换 |
| 08-28 | `generic-dialogue-tutorial-shop`（30.4 KB） | 通用对话/教程/商店 |
| 08-29→30 | `dual-window-presentation-state-machine` → `desktop-single-viewport-rollback` | **双窗口方案回滚为单视口** |

**08-21 冻结的权威玩家流程（原文要点）：**

1. 编辑器启动与游戏启动均进入 `/Game/GameXXK/Maps/L_DesktopTrainingHUD`。
2. **`Town` 语义 = 2D 桌面挂机主界面**；进入或返回 `Town` **不加载青山镇**。
3. 桌面工作台的「挑战」**不要求接受青山镇任务、不修改任务状态**，也不依赖城镇 NPC。
4. 点击可挑战关卡后，在**同一个 `L_DesktopTrainingHUD` 世界内**关闭工作台并显示现有全屏
   `UGameXXKBattleBoardWidget`；不得生成内嵌战斗画布或切入 3D 地图。
5. 退出/胜利/失败后的返回动作恢复 `Town` 状态与 2D 工作台，地图仍是 `L_DesktopTrainingHUD`。
6. 同期冻结**目标箭头热点契约**：浮动窗口 viewport-client 本地坐标 + `BattleHudSafeStage`
   `Offset/Scale`；`NativePaint` 用 `SafeStage.Offset + StagePosition × SafeStage.Scale` 回 Board-local；
   **禁止** `StageGeometry.LocalToAbsolute -> AllottedGeometry.AbsoluteToLocal`。源图 `1254×1254`，
   可见右侧箭尖热点 `(1082,608)`。

### 阶段 ⑧ 2026-08-31 ~ 09-11 · 内容与交付

序章地图/月白教程 → `provider-vision-integration-retirement` →
**`card-monster-progression-rebalance`（95.8 KB，全项目最大设计文档）** → 雷击单段、术士内力 →
卡牌/队伍/装备模拟 → 状态图标、遗物、宝石、剧情 → 讨伐与宝箱概率表、本地化总表、打包瘦身。

## 4. 两条主线的对照

| | 2026-07-01 初始 | 当前 |
|---|---|---|
| **架构层** | C++ 独占规则，UMG 只负责显示 | **原则未变**，且被强化为「规则层不引用 UMG/Slate」；新增 10 个挂机模块与奖励账本 |
| **表现层** | Paper2D TileMap 城镇 + UMG 地牢/战斗 | **两度转向**：3D 青山镇 → 2D 桌面浮窗。权威面是 `L_DesktopTrainingHUD` |
| **数据层** | DataAsset / DataTable | 沿用；新增大量总表（`docs/design/`）与运行时目录（`GameXXK*Catalog`） |
| **约束来源** | 开山设计文档 + 用户确认 | `AGENTS.md` + `2026-08-21` 权威流程冻结 |

**结论：架构层从第一天至今是同一个原则**——规则用 C++ 且可脱离场景测试，表现层只读模型、发命令。
真正反复变动的是**表现层的形态**，而不是分层方式。这也是为什么历次迁移都没有推翻 C++ 规则层。

## 5. 关键文档索引

| 用途 | 路径 |
|---|---|
| 初始设计（架构/表现分层出处） | `docs/superpowers/specs/2026-07-01-gamexxk-mvp-design.md` |
| 唯一逐字用户需求 | `docs/production/2026-07-03-playable-entry-flow/00-raw-input.md` |
| 挂机迁移与 10 模块架构 | `docs/superpowers/specs/2026-08-12-gamexxk-idle-desktop-migration-design.md` |
| 权威 2D 流程冻结 | `docs/superpowers/specs/2026-08-21-desktop-2d-battle-canonical-flow.md` |
| AI agent 硬约束 | `AGENTS.md` |
| 当前目标状态（语义真相） | `docs/production/current-goal-acceptance.md` |
| 全部设计 / 计划 | `docs/superpowers/specs/`（118 份）、`docs/superpowers/plans/`（188 份） |

## 6. 核对方法

文中的阶段划分来自 `docs/superpowers/specs/` 的文件名日期序列与各文档正文。逐条复核方式：

```powershell
# 列出全部设计文档（按日期）
Get-ChildItem docs\superpowers\specs -File -Filter *.md | Sort-Object Name | Select-Object Name

# 取某份文档的章节结构
Select-String -Path docs\superpowers\specs\<文件>.md -Pattern '^#{1,3} '

# 取某份文档的某一段
Select-String -Path docs\superpowers\specs\<文件>.md -Pattern '^## 5\. ' -Context 0,30
```

## 7. 本文不包含

- 不重新定义任何运行时语义；如有冲突，以 `current-goal-acceptance.md` 与原始 `specs/` 为准。
- 不评价历史决策的对错。
- 不记录 3D 资产的存废结论——`2026-08-12` §16 明确把该裁决留到另行验收，本文不改变它。
