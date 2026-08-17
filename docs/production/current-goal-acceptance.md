---
status: record
owner: codex
updated_at: 2026-08-17
source_commit: 8475ba8
working_tree: dirty (opt-in Training runtime work; preserve user L_Main.umap change)
---
# GameXXK 当前目标(滚动指针)

> 本文件是"当前做到哪了"的**唯一滚动指针**。`AGENTS.md` 不再硬编码验收状态,改指向这里。每次目标收尾后更新本文件。

## 当前基线(更新于 2026-08-17)

- 分支:`main`
- 当前 HEAD:`8475ba8`(`docs: record desktop training goal review`);运行时代码基线为 `c4762be`，工作区在当前 HEAD 之上包含未提交的桌面历练规则、存档 v18、程序化工作台和 PlayerController opt-in 接线；用户已有 `Content/GameXXK/Maps/L_Main.umap` 修改仍受保护。
- 最近一次目标验收:`docs/production/2026-08-15-battle-target-arrow-alignment-incident.md`(战斗卡牌目标箭头错位修复，自动化/真实 PIE/用户现场验收通过)
- 最近一次全量代码/文档审查与优化方案:`docs/production/2026-08-16-full-project-optimization-proposal.md`;上一轮定向建议见 `docs/production/2026-08-16-optimization-followup.md`
- 最新历史全量自动化:**598/598 通过、0 error**，证据为 `Saved/Automation/ChargeFinishSubject/index.json`（2026-08-16 12:01:35）；最近历史冷 UBT GREEN，证据为 `Saved/HarnessReports/20260816-114544-ai-production-loop.md`。本轮未把旧报告冒充为 `c4762be` 工作区上的新全量运行。
- 本轮新增证据：headless 脚本 `13/13` 通过（`Saved/HarnessReports/20260817-233157-ai-production-loop.md`）；最新 `GameXXK.DesktopTraining` `1/1`、`GameXXK.Training` `4/4`、`GameXXK.MVP.SaveGame` `12/12` 通过（分别见 `Saved/HarnessReports/20260817-234829-ai-production-loop.md`、`20260817-234854-ai-production-loop.md`、`20260817-234916-ai-production-loop.md`）；最新冷 UBT `-NoHotReload` 成功。asset-contract 独立报告 `Saved/HarnessReports/20260817-233751-ai-production-loop.md` 为 `51/66`，15 项失败，故 Phase 0 总门禁仍未通过。
- 当前工作区保护：`Content/GameXXK/Maps/L_Main.umap` 保留用户已有修改；`SourceAssets/`、`SourceArt/` 及未跟踪探针不在本轮 Phase 0 写入范围。

## 已落地(最近六轮)

- **敌方意图随机化与守卫嘲讽闭环**(`9598072`、`e78be7c`):敌方单体攻击意图默认 `MarkedPartyElseRandom`(只有带标记的队友吸引集火,否则稳定种子随机目标);标记施加类意图随机落点;种子加 Murmur finalizer 防止两名队友机械交替。守卫 16 张加甲牌结算末尾给守卫本人上 1 层标记,与标记优先规则形成坦克闭环;标记既有代价规则(受击增伤/命中消耗)随之生效。聚焦敌方 61/61、守卫 23/23、全量 598/598 通过。
- **战斗 tooltip / 卡牌文本 / HP HUD 一致性热修簇**(`a490235..b6763a0`,13 个提交):补全刀锋特质/药丸/分类型伤害行的卡牌悬浮文本;彩色双字特质药丸与内容自适应纸张;tooltip 行确定性换行、药丸排序与跟随定位;猎手重箭/掠影箭文本;修复未封顶 sentinel 泄漏、自愈后 HUD 生命值冻结、蓄力/终结句缺失卡牌主语。全量自动化 598/598、冷 UBT GREEN。
- **战斗目标箭头视觉回归修复**(`a490235`):移除箭头头部的方向相关平移，恢复贴图中心严格落在实时鼠标端点；保留自由鼠标跟随与 stage-space 目标命中；新增水平/垂直/对角回归测试。真实 PIE 的 Single/Healing 双方向对比通过，用户在保持运行的目标卡验收现场确认 `ok`。
- **奖励体系重构**(`00002f1`):彻底删除路线临时卡体系(路线卡条目/12 容量/18 张配方/事件宝箱商人发卡);新增玩家卡组**首领卡槽 3 个**,击杀首领后选择首领牌(虎/熊)写入空槽并加入当前手牌;战后奖励只剩首领牌(例外)+携带牌品质升级+遗物+气力点+抽牌数;存档 v16→v17 迁移。
- **两项平衡修复**:①模拟 AI 无进展死循环(连续 64 次决策无敌方掉血强制结束回合;5 个回合边界血量无变化判僵持战败);②刀锋延迟保留牌主人阵亡时实例丢失(牌随主人退场)。
- 新增回归测试 `ZhuiFengJinGuiBoss942090`、`BladeRetainedOwnerDefeat1100213`;修复两处进程 RNG 偶发测试;卡牌文档重新生成。
- 全量自动化 **596/596 通过**(奖励重构轮,历史记录);2400 锁定案例 0 卡死/0 错误(胜 2298 / 负 102)。
- 198 张卡(36 主角 + 108 永久伙伴 + 24 任务 NPC + 30 路线牌)完整执行,全 198 × 七地势 + 条件缺失分支通过。

## 关键语义冻结(注意差异)

- **任务接受与跟随者激活语义已切换**。`AcceptTownQuest` 接取青山镇主线**不再**自动设置 `bFollowerJoined`;引导 NPC 留在原地(不跟随、不自动入队)。玩家在 NPC 对话框点"入队"(`RecruitPendingTownNpc`)后,引导 NPC 才成为叙事跟随者(`bFollowerJoined=true`,并清空 `bHasQuestNpcLocation`/`QuestNpcLocation` 随主角离镇)。旧档迁移(`GameXXKSaveMigration`)仍会把 v14 及更早"已接受但未跟随"的存档规范化为跟随已加入,保持兼容。
- 地形增益仍为**旧表**;新模型(每回合全员 1 次 + 对应职业再 1 次 + 山河套再 1 次,敌方统一 1 易伤+1 燃烧)已裁决,待 §5 山河三档与 `TriggerTerrainBenefit` 口径复核后实施,见 `docs/design/2026-08-13-terrain-benefit-redesign.md`。

## 桌面历练工作台当前进度(2026-08-17)

- **规则/存档已开始接线但未完成玩法闭环**：新增 `Source/GameXXK/Public/GameXXKTrainingRules.h`、`Private/GameXXKTrainingRules.cpp`，覆盖三难度、27 个稳定关卡 ID、挑战/游历分离、1-1 默认通关、失败重试策略、章节编制与掉落层级占位；`FGameXXKSaveState` 新增 `Training`，存档版本为 v18，迁移与验证在 `GameXXKSaveMigration.*`。
- **程序化工作台壳已接线且默认关闭**：`UI/GameXXKDesktopTrainingWorkbenchWidget.*` 提供 1920×1080 几何合同、仓库 4 列、背包比例约 1.76:1、右侧 9 节点/三难度页签、挑战/游历按钮、顶部 3 敌+3 我挂机条、挑战视图与自动战斗按钮；`GameXXKMVPPlayerController.*` 仅在 `bEnableDesktopTrainingWorkbench=false` 被显式打开时响应 Tab/工作台入口，默认 3D 城镇与旧回退未切换。
- **已验证的最小交互**：`GameXXK.DesktopTraining` 几何合同 1/1；`GameXXK.Training` 规则 4/4；`GameXXK.MVP.SaveGame` v18 迁移 12/12。当前 `AdvanceTrainingChallengeEncounter` 是规则/状态机 façade，尚未连接真实路线卡牌战斗、真实战斗结算、掉落 RNG、天赋树 Resolver、离线挂机计时与实际金币/经验写回。
- **章节敌人语义仍需产品确认并锁定**：代码目前从现有 catalog 取 4 个 Normal + 2 个 Elite，并将 1-1/1-2 复用现有精英定义为 Boss；这与“公鸡/狸猫普通、山羊/黄鼬次级精英”的最终口径可能存在映射差异，未通过最终验收前不得当作已冻结。

## 仅规划未实施 / 已搁置

- **旧历练桌面迁移 7 包索引**(历练放置、离线收益、双宝箱、旧 2D 主界面、桌面迷你窗、自动战斗、任务 NPC 显式入队 + 默认入口迁移):**已标记 shelved，不得按旧计划直接执行**。新桌面工作台设计见 `docs/superpowers/specs/2026-08-17-gamexxk-desktop-training-workbench-design.md`；当前已进入 opt-in 规则/壳实现，但 PSD、真实战斗、挂机结算和性能/PIE 验收仍未完成。旧计划中的 v16/v17/v18 边界失效，现行工作区为 v18；后续迁移必须继续使用新编号，不得复用旧索引。
- 项目自身优化:`docs/production/2026-08-16-full-project-optimization-proposal.md` Phase 0 正在执行；Phase 1/2/3/4 仍未实施，旧 `optimization-plan.md` 仅作历史索引。

## 下一步待办

- Phase 0 基线证据：`docs/production/2026-08-17-phase0-baseline.md`；执行计划：`docs/superpowers/plans/2026-08-17-gamexxk-phase0-source-of-truth-and-gates.md`；本轮完整复核：`docs/production/2026-08-17-desktop-training-goal-review.md`。
- 项目自身优化:见 `docs/production/2026-08-16-full-project-optimization-proposal.md`(Phase 0 → Phase 4 全量方案)。Phase 0 门禁为 harness 无 finding、默认生产循环全绿、headless 脚本全绿、all 不启动编辑器、`git diff --check` 通过。
- 玩法顺序:Phase 0 收尾已基本完成 → Phase 1 地形增益重设计(先复核 §5 山河三档与 `TriggerTerrainBenefit` 两个口径)→ Phase 2 数值迭代(以 `2026-08-12-balance-tuning-ledger.md` §4.8 为最新基准)。
- 非阻塞测试工具维护:更新 `scripts/gamexxk_real_play_flow_mcp.py` 的旧 `pointer_matches_target` 吸附断言，使其符合 `6668146` 冻结的自由鼠标跟随语义；Phase 0 已把 `--script-tests all` 分成 `headless`、`asset-contract`、`mcp-live` 三类，历史 64/86 仅保留作迁移前对照，当前 headless 13/13、asset-contract 51/66、mcp-live 未运行。
