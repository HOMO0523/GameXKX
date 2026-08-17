---
status: record
owner: codex
updated_at: 2026-08-18T05:36:00+08:00
source_commit: 57bf299
working_tree: dirty (preserve user L_Main.umap, unrelated probes and source-art changes; current rule/test/docs updates are committed through 57bf299)
---
# GameXXK 当前目标(滚动指针)

> 本文件是"当前做到哪了"的**唯一滚动指针**。`AGENTS.md` 不再硬编码验收状态,改指向这里。每次目标收尾后更新本文件。

## 当前基线(更新于 2026-08-18)

- 分支:`main`
- 当前代码 HEAD:`57bf299`(`test: lock travel chest probability parity`)，前置挑战画布提交为 `bdfcf9e`，MasterV2 资源提交为 `4632179`，挑战自动战斗待选牌处理为 `5c60999`；文档基线为 `ffe3613`；前置挑战侧壳为 `74d837f`，背包设置/关闭分离为 `61c92e5`，仓库排序/卸下回仓为 `fbd7e7f`，仓库分页/quick-equip 为 `a70b192`，背包/伙伴导航为 `48b7212`，工作台 RuntimeState read model 为 `dfb5230`，宝箱 Inventory bridge 为 `a650527`、seeded Resolver/cooldown 为 `1a17019`、TravelRunner 为 `23aee95`、桥接为 `7881927`。本轮补上挑战自动战斗对强制弃牌、洞察、任务检索和自动解析队列的确定性处理，并加入待选牌回归测试；桌面历练规则、存档 v20、程序化工作台、PlayerController opt-in、真实 CardBattle 桥接、确定性 TravelRunner、离线补算/待领取账本/收菜入口、奖励/冷却 Resolver、最小宝箱 Inventory 镜像、背包角色/伙伴切换、工具/天赋容器基础导航、仓库 20 格分页、visible-slot quick-equip、确定性排序、装备槽卸下回仓、背包内独立设置与关闭动作、挑战期间左右仓库/历练地图只读外壳与导航锁定已提交；本轮又把顶部 3 敌/3 我站位并入 960×968 ChallengeViewport，BattleBoard 使用 710×535 连续画布，挑战态自动战斗可见而游历失败重试隐藏，并以 `57bf299` 增加“游历与局内共用同一概率/seed，冷却单独门控”的普通与精英回归断言。用户已有 `Content/GameXXK/Maps/L_Main.umap` 修改仍受保护；工作区还存在历史探针和源美术未跟踪物，不属于本轮提交。
- 最近一次目标验收:`docs/production/2026-08-15-battle-target-arrow-alignment-incident.md`(战斗卡牌目标箭头错位修复，自动化/真实 PIE/用户现场验收通过)
- 最近一次全量代码/文档审查与优化方案:`docs/production/2026-08-16-full-project-optimization-proposal.md`;上一轮定向建议见 `docs/production/2026-08-16-optimization-followup.md`
- 最新历史全量自动化:**598/598 通过、0 error**，证据为 `Saved/Automation/ChargeFinishSubject/index.json`（2026-08-16 12:01:35）；最近历史冷 UBT GREEN，证据为 `Saved/HarnessReports/20260816-114544-ai-production-loop.md`。这两份报告只作历史回归参考，不冒充当前工作区的本轮全量运行。
- 本轮新增证据：headless 脚本 `13/13` 通过（`Saved/HarnessReports/20260818-011435-ai-production-loop.md`）；本轮最新冷 UBT `-NoHotReload` 与训练 Automation 均成功（`Saved/HarnessReports/20260818-053115-ai-production-loop.md`）；最新 `GameXXK.Training` `18/18`（`Saved/Automation/TrainingTravelChestParity-20260818/index.json`，含 1-1 全 encounter 无箱、非 1-1 游历与局内同 seed/概率、普通箱 240 秒/精英与首领高级箱 360 秒冷却、离线补算/待领取账本/读档恢复、真实挑战自动推进与待选牌处理）；`GameXXK.DesktopTraining` `2/2`（`Saved/HarnessReports/20260818-051647-ai-production-loop.md`，Automation 目录 `DesktopTrainingChallengeStrip-20260818`，含 960×968 连续挑战画布、6 敌我站位槽、只读侧壳和挑战/游历控件隔离）；MasterV2 资源合同 `1/1`（`Saved/Automation/MasterV2NavDiscGreen-20260818-r1/index.json`），覆盖 PanelLarge、ItemSlot、EquipmentSlot、Tab、Route 和五张等比 NavDisc；`GameXXK.MVP.SaveGame` `12/12`（`Saved/HarnessReports/20260818-041625-ai-production-loop.md`，Automation 目录 `SaveGameTravelOfflineV20Green-20260818`）。挑战侧壳只读/导航锁定、设置/关闭分离、仓库分页/排序/卸下仍有前置证据。最新 asset-contract 报告 `Saved/HarnessReports/20260818-012130-ai-production-loop.md` 仍为 `51/66`，15 个测试文件失败；本次完成后复核已明确其未关闭，故 Phase 0 总门禁仍未通过。
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

## 桌面历练工作台当前进度(2026-08-18)

- **规则/存档已推进但未完成玩法闭环**：`GameXXKTrainingRules.*` 覆盖三难度、27 个稳定关卡 ID、挑战/游历分离、普通 1-1 默认通关、失败策略、章节编制、seeded challenge/travel reward resolver 和普通/高级 Travel 240/360 秒冷却；1-1 游历普通/精英/首领均无箱，只保留阶段金币/经验；v20 新增 UTC 基线、最多 24 小时离线补算、待领取奖励账本、收菜入库和阵亡暂停标记；`FGameXXKSaveState` 的 Training 字段为 v20，迁移、挑战/游历 encounter index、冷却负值和 seed 校验已接入。
- **程序化工作台、真实挑战桥接和 TravelRunner 已提交且默认关闭**：`GameXXKDesktopTrainingWorkbenchWidget.*` 提供 1920×1080 几何合同、仓库 4 列/20 格分页、背包比例约 1.76:1、右侧 27 节点/三难度页签、挑战/游历按钮、顶部 3 敌+3 我挂机条；挂机条读取 TravelRunner 的遭遇、阶段和 HP。背包内部现在有主角/两名永久伙伴切换、当前角色对可见仓库格 quick-equip、确定性排序、装备槽卸下回仓、独立设置面板和独立关闭动作，工具替换右栏，天赋替换中栏，挑战期间导航只读。本轮工作区增量已将批准的 MasterV2 `PanelLarge`、`ItemSlot`、`EquipmentSlot`、页签、路线节点，以及五张圆形 `NavDisc` 以缓存纹理和九宫格/等比槽位接入；资源合同与 DesktopTraining 回归已通过，但仍不是最终 PSD/manifest 交付。`StartTrainingChallenge` 已创建真实 `FGameXXKCardBattleAdapter` 会话并支持单步推进；自动模式会对强制弃牌、洞察、任务检索和自动解析队列做有界确定性选择；`StartTrainingTravel`/`AdvanceTrainingTravelStep` 已支持走动、单敌人自动攻击、掉血、击杀、Boss 结算、重试/回退和 1-1 一血规则，但仍是 opt-in 运行时，`GameXXKMVPPlayerController` 默认保持 3D 城镇。
- **章节敌人语义已按最终口径冻结到规则与测试**：普通候选为公鸡/狸猫，次级精英为山羊/黄鼬，每场 4 个普通槽、2 个精英槽和 1 个首领；1-1 山羊、1-2 黄鼬、1-3 青角羊王。挑战生命与游历 1 HP 例外已分离；挑战/游历真实表现和结算仍待 PIE。
- **当前仍未完成**：TravelRunner 的实际 Actor/动画与最终后台 Timer/视觉、完整路线卡战斗 UX、真实天赋 read model/最终概率、FIFO 箱批/容量/箱内物品、仓库转移/容量与完整装备点击交互、工具真实数据、PSD/图标 manifest、1920/2560 截图与 TaskBarHero 四组性能采样。当前已具备金币、装备实例/六槽、物品数量、canonical 宝箱 item 的最小 RuntimeState 镜像、v20 离线补算/待领取/收菜、基础角色/伙伴切换、仓库 20 格分页、visible-slot quick-equip、确定性排序、装备槽卸下回仓、独立设置与关闭动作；完整逐项复核见 `docs/production/2026-08-18-desktop-training-goal-review.md`。

## 仅规划未实施 / 已搁置

- **旧历练桌面迁移 7 包索引**(历练放置、离线收益、双宝箱、旧 2D 主界面、桌面迷你窗、自动战斗、任务 NPC 显式入队 + 默认入口迁移):**已标记 shelved，不得按旧计划直接执行**。新桌面工作台设计见 `docs/superpowers/specs/2026-08-17-gamexxk-desktop-training-workbench-design.md`；当前已进入 opt-in 规则/壳实现，但 PSD、真实战斗、挂机表现和性能/PIE 验收仍未完成。旧计划中的 v16/v17/v18 边界失效，现行工作区已推进到 v20；后续迁移必须继续使用新编号，不得复用旧索引。
- 项目自身优化:`docs/production/2026-08-16-full-project-optimization-proposal.md` Phase 0 正在执行；Phase 1/2/3/4 仍未实施，旧 `optimization-plan.md` 仅作历史索引。

## 下一步待办

- Phase 0 基线证据：`docs/production/2026-08-17-phase0-baseline.md`；执行计划：`docs/superpowers/plans/2026-08-17-gamexxk-phase0-source-of-truth-and-gates.md`；本轮完整复核：`docs/production/2026-08-18-desktop-training-goal-review.md`。
- 项目自身优化:见 `docs/production/2026-08-16-full-project-optimization-proposal.md`(Phase 0 → Phase 4 全量方案)。Phase 0 门禁为 harness 无 finding、默认生产循环全绿、headless 脚本全绿、all 不启动编辑器、`git diff --check` 通过。
- 玩法顺序:Phase 0 收尾已基本完成 → Phase 1 地形增益重设计(先复核 §5 山河三档与 `TriggerTerrainBenefit` 两个口径)→ Phase 2 数值迭代(以 `2026-08-12-balance-tuning-ledger.md` §4.8 为最新基准)。
- 非阻塞测试工具维护:更新 `scripts/gamexxk_real_play_flow_mcp.py` 的旧 `pointer_matches_target` 吸附断言，使其符合 `6668146` 冻结的自由鼠标跟随语义；Phase 0 已把 `--script-tests all` 分成 `headless`、`asset-contract`、`mcp-live` 三类，历史 64/86 仅保留作迁移前对照，当前 headless 13/13、asset-contract 51/66、mcp-live 未运行。
