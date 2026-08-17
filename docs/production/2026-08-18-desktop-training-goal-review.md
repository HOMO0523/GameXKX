---
status: review
owner: codex
updated_at: 2026-08-18T02:00:12+08:00
source_commit: 1a17019
decision: not-complete
goal_status: active
---

# 桌面历练 /goal 完成后复核（2026-08-18）

这份记录是当前目标的收尾审计，不把“程序能编译”写成“产品已完成”。审计输入包括 `docs/production/2026-08-16-full-project-optimization-proposal.md`、`docs/production/2026-08-16-optimization-followup.md`、`docs/production/current-goal-acceptance.md`、桌面工作台设计稿以及本轮实际代码、Automation、冷 UBT 和脚本报告。

## 1. 结论先行

**目标目前不能标记 complete，继续保持 active；默认入口不能切换，3D 城镇仍是回退基线。**

本轮已经把“规则/存档/程序化壳”推进到“挑战可创建真实 CardBattle 会话、游历可按 runner 步进”的阶段，但离用户要求的完整桌面游戏还有四类硬缺口：

1. **游历还没有生产表现和持久在线模型**：确定性 TravelRunner 已支持走动阶段、一次一只怪、自动攻击、掉血、击杀、Boss 结算、阵亡重试/回退和 1-1 一血例外；但还没有实机角色/怪物 Actor、动画、碰撞/移动表现、离线计时和真正收菜窗口。
2. **生产 UI/PSD 尚未闭环**：当前工作台是 Slate 程序化壳，真实背包/仓库数据、现有 Master UI 纹理、透明图标 manifest/hash 和 1920/2560 校准尚未接入。
3. **奖励 Resolver 已进入可验证阶段，但仍未产品闭环**：挑战/游历现在使用稳定 seed + 配置概率 + talent bonus 参数；游历普通箱/高级箱分别有 240/360 秒逻辑冷却，状态迁移到 v19。真实天赋树数据源、实际宝箱物品写入仓库/背包和概率表最终值仍未接入。
4. **PIE/MCP 与性能证据缺失**：本轮没有新的工作台 1920×1080 / 2560×1440 截图、实际点击流、悬停视觉证据或 TaskBarHero 对照采样。

因此，本轮完成的是一个**可回滚、默认关闭、可自动化验证的 opt-in 运行时基础包**，不是可直接替代 3D 城镇的发行版本。

## 2. 工作区与保护边界

| 项目 | 当前事实 |
|---|---|
| 分支 | `main` |
| 本轮代码提交 | `1a17019 feat: add seeded training chest rewards and travel cooldowns`；前置 TravelRunner 为 `23aee95`，桥接提交为 `7881927` |
| 当前存档版本 | `CurrentSaveVersion=19`；v18 引入桌面 Training 进度，v19 引入奖励 seed 与游历宝箱冷却 |
| 默认入口 | `bEnableDesktopTrainingWorkbench=false`；Tab/显式测试开关才会打开工作台 |
| 用户地图保护 | `Content/GameXXK/Maps/L_Main.umap` 保留已有修改，未加入本轮提交、未 reset/checkout |
| 大型未跟踪资产 | `SourceAssets/`、`SourceArt/`、探针脚本和生成物未无差别加入提交 |
| 旧路线图 | 旧历练 v16/v17/v18 规划保持 shelved/superseded，不重新使用旧索引 |
| 回滚点 | 关闭 opt-in flag 即回到 3D 城镇；代码提交可单独回滚；未覆盖 Master UI 或地图资产 |

工作区仍可能存在其他历史/用户未提交内容；本轮提交只包含 Training 运行时、存档接线、测试和程序化工作台，不包含 `L_Main.umap` 与大批源美术。

## 3. 目标工作包逐项验收

| 工作包 | 已落地事实 | 当前判定 | 缺口/下一门禁 |
|---|---|---|---|
| 项目优化 Phase 0 | 真源文档、旧历练 shelved 标记、脚本标签、GBK/路径边界、harness 状态检查已整理 | 部分通过 | asset-contract 仍 51/66；mcp-live 未跑 |
| 2D 工作台壳 | 左仓库 4 列、中背包约 1.76 比例、右 27 节点/三难度、底部 5 导航、挑战/游历按钮、顶部 3 敌+3 我占位 | 几何合同通过 | 仍是程序化壳；没有生产纹理、真实数据、设置/工具内容和视觉校准 |
| Tab/菜单入口 | opt-in 时 Tab 打开工作台并落到背包视图；默认仍不拦截旧 Town HUD | 部分通过 | 未完成真实 PIE 点击流与主入口迁移 |
| 挑战/游历分离 | `StartChallenge` 会暂停游历；`StartTravel` 会拒绝挑战中启动；保存校验拒绝两种状态同时 active | 规则通过 | 游历执行器和真实 UI 状态还没有上线 |
| 27 个关卡 | 普通/困难/地狱各 9 个稳定 StageId；普通 1-1 新档默认通关且可游历；整档难度解锁规则已测试 | 规则通过 | 没有真实地图节点视觉/悬停和解锁流程证据 |
| 第一章编制 | 普通候选：公鸡、狸猫；次级精英：山羊、黄鼬；每条路线 4 普通 + 2 精英 + 1 首领；1-1 山羊、1-2 黄鼬、1-3 青角羊王 | 数据/测试通过 | 仍需在真实 ChallengeViewport/tooltip 做视觉验收；catalog 的 tier 与 Training 语义需保持文档同步 |
| 1-1 生命例外 | `BuildEncounterSequence(Stage, false)` 挑战生命 >1；`true` 游历序列为 1 HP，含首领；TravelRunner 与 subsystem wrapper 复用该规则；游历可按 encounter 概率掉箱并受普通/高级冷却限制 | 规则与 runner 通过 | 没有真实 Actor/动画、离线计时和生产收菜窗口 |
| 挑战真实战斗 | `StartTrainingChallenge` 创建真实 `FGameXXKCardBattleAdapter` 会话，进入 `Battle`，投影现有敌人 catalog；BattleBoard 可挂到 ChallengeViewport | **新增且已自动化通过** | 尚未完成全路线手牌/意图/胜负/结算的 PIE 视觉闭环；pending card choice 会暂停自动步进 |
| 挑战自动战斗 | 每次调用推进一张合法卡、结束玩家阶段或解析敌方阶段；终结后结算并打开下一遭遇 | 部分通过 | 目前是运行时适配器，不是完整自动战斗 UX；需要手牌、意图、血条、tooltip 和战斗结算截图 |
| 游历循环 | `InitializeTravelRunner`/`AdvanceTravelRunner` 与 `AdvanceTrainingTravelStep` 已按 Walking→Combat→Settlement 推进 7 遭遇；失败重试/回退由 `ResolveTrainingTravelFailure` 处理；挂机条读取阶段/HP/遭遇 | **新增且 Automation 通过** | 仍缺真实 Actor/动画、后台/离线时间、失败弹窗和最终收菜 UI |
| 奖励与宝箱 | `ResolveChallengeReward`/`ResolveTravelReward` 使用稳定 seed、Stage 概率、天赋 bonus 参数；普通 encounter 为普通箱，精英/首领为高级箱；游历冷却为普通 240 秒、高级 360 秒；挑战/游历结算推进同一 seed | **规则/聚焦 Automation 通过，产品未闭环** | 天赋真实 read model、最终概率表、宝箱物品生成、仓库/背包写入、重复结算保护仍待接入 |
| 存档迁移 | v18 Training 字段、旧档 Normal 1-1 初始化、challenge/travel 互斥；v19 新增 reward seed 与两档游历冷却；范围/负值校验和 v18→v19 测试已补 | 通过 | 需要补实际战斗中断/恢复、离线时间推进和最终存档 round-trip |
| 性能 | 保留 TaskBarHero 参考包络：Working Set 约 513 MiB、Private 约 1418 MiB、GPU Dedicated 约 282 MiB、整机 CPU 约 2.3%、GPU Engine 约 2% | 未验收 | 没有当前 2D 静置/游历/挑战/3D 四组 Shipping/PIE 采样 |
| PSD/素材规范 | 设计稿冻结复用优先、透明 1:1、禁止拉伸/噪点/文字烘焙、manifest/hash 规则 | 未验收 | 没有本工作台专用 icon manifest/hash；程序化壳不得直接进 PSD/UE 生产资产 |

## 4. 实现边界与代码规范复核

### 4.1 规则与数据层

- `GameXXKTrainingRules.h/.cpp` 是纯、确定性的关卡和奖励规则入口，不让 Widget 直接拼状态。
- StageId 使用 `Training.Normal.1-1`、`Training.Hard.3-3` 等稳定命名；难度解锁和关卡顺序不依赖 UI 索引。
- 第一章的“4 个普通怪”按用户最后口径落为两个普通身份重复出现、两个次级精英各出现一次，而不是把 catalog 的六个 tier 原样暴露给 UI。
- `BuildEncounterSequence(StageId, bTravelMode)` 明确分离挑战/游历生命值，避免把 1 HP 低耗例外泄漏到挑战。
- 保存校验现在检查 active challenge/travel 的 stage 与 encounter index 合法范围，并拒绝 inactive travel 残留 index。

### 4.2 运行时桥接

- `UGameXXKMVPSubsystem::StartTrainingChallenge` 先在 Candidate 状态上启动规则，再通过现有 `FGameXXKCardBattleAdapter::BeginCardBattle` 建立真实卡战；失败不会半提交 RuntimeState。
- 训练敌人使用 `FGameXXKEnemyCatalog::Find/ComputeStats`，因此敌人意图和战斗数据来自现有敌人池，而不是另造一份“历练假敌人”。
- 训练挑战终结后在 Candidate 上结算金币/经验、清理 CardRun、进入下一遭遇或回到 Town；挑战失败按本地重试，不调用路线地下城失败结算。
- `GameXXKDesktopTrainingWorkbenchWidget` 只负责显示和按钮路由，ChallengeViewport 复用现有 `GameXXKBattleBoardWidget`；但目前仍需把真实 inventory/warehouse read model 和生产纹理接入。
- `GameXXKMVPPlayerController` 的新入口是显式 opt-in；没有改动 3D 城镇默认入口，方便逐步验收和回滚。

### 4.3 已知技术风险

1. `AdvanceTrainingCardBattleStep` 遇到需要玩家选择的牌会安全暂停；自动战斗需要产品决定“自动选首个合法目标”还是显示待选状态。
2. `BuildChallengeReward` 仍保留给旧 fixture 的强制结算兼容入口；生产路径已改用 seeded Resolver，但最终概率表、天赋数据源和实际宝箱物品仍不能当作发行掉率。
3. 游历已有独立的纯规则 Runner 和 subsystem tick，但没有生产 Actor/动画、定时器后台/离线时间模型，不能声称已经实现完整挂机。
4. BattleBoard 嵌入工作台的视觉 session 目前是适配性接线，尚未在两种分辨率和真实输入下验证生命周期。

## 5. 验证证据（本轮真实运行）

| 验证 | 证据 | 结果 |
|---|---|---:|
| harness 状态 | 每次 `ai_production_loop.py` 报告中的 `harness_state_validator.py --json` | PASS，`findings=[]` |
| 代码空白 | 同上报告中的 `git diff --check` | PASS（仅 CRLF 提示） |
| headless script gate | `Saved/HarnessReports/20260818-011435-ai-production-loop.md` | 13/13 PASS；不启动 UE |
| Training rules + bridge + TravelRunner | `Saved/HarnessReports/20260818-015846-ai-production-loop.md`；`Saved/Automation/TrainingRewardCooldownGreen-20260818-r3/index.json` | 13/13 PASS；新增 Resolver、Travel cooldown、v18→v19 migration，含 `RealCardBattleBridge`、`TravelRunnerLoop`、`TravelRunnerFailure`、`TravelSubsystemBridge`、`SaveValidation` |
| DesktopTraining geometry | `Saved/HarnessReports/20260818-015908-ai-production-loop.md`；`Saved/Automation/DesktopTrainingRewardCooldownGreen-20260818-r2/index.json` | 1/1 PASS |
| SaveGame migration | `Saved/HarnessReports/20260818-015019-ai-production-loop.md`；`Saved/Automation/SaveGameRewardCooldownGreen-20260818/index.json` | 12/12 PASS |
| Real bridge isolated rerun | `Saved/Automation/TrainingBridge-20260818-r6/index.json` | 1/1 PASS：真实 Battle 屏、1 个 authored enemy、auto step |
| Cold UBT | `Saved/HarnessReports/20260818-015636-ai-production-loop.md`，本轮实际经过秒数接口变更后成功；94 action `-NoHotReload` | PASS；未用 Live Coding/Hot Reload |
| 历史全量回归 | `Saved/Automation/ChargeFinishSubject/index.json` | 598/598 是 2026-08-16 历史证据，只作为回归参考，不冒充本轮全量 |
| asset-contract | `Saved/HarnessReports/20260818-012130-ai-production-loop.md` | 51/66 PASS，15 个测试文件 FAIL，门禁未通过 |
| PIE/MCP 工作台 | 本轮仍无新的 1920×1080 / 2560×1440 工作台截图、点击流或悬停视觉证据 | 未运行 |

### 5.1 asset-contract 失败分类

本轮最新报告（`Saved/HarnessReports/20260818-012130-ai-production-loop.md`）明确失败的 15 个文件是：

`test_battle_party_qi_icon.py`、`test_battle_resource_psd_cuts.py`、`test_battle_terrain_art.py`、`test_gamexxk_ui_master_pages.py`、`test_party_deck_card_portrait_pipeline.py`、`test_party_deck_sprite_atlas_packer.py`、`test_party_deck_sprite_import_pipeline.py`、`test_party_deck_sprite_manifest.py`、`test_psd_card_frame_pipeline.py`、`test_qingshan_b1_heightmap.py`、`test_qingshan_building_concepts.py`、`test_qingshan_dress_b1_config.py`、`test_qingshan_dress_b1_scripts.py`、`test_reference_faithful_task_ui_icons.py`、`test_town_hud_psd_visual_contract.py`。

失败主要来自三类：Pillow API（`get_flattened_data`）与当前环境不兼容；外部/未锁定的 PSD、PartyDeck、Qingshan 源资产或旧 hash 合同；以及生产 UI 代码演进后旧测试仍期待旧文本/字段。它们必须继续按 `asset-contract` 独立处理，不能因为程序化工作台已经可编译就标绿。

## 6. 性能、素材和 UX 的真实判定

### 6.1 性能

目前只有同机 TaskBarHero 参考值，没有当前 GameXXK 工作台采样，因此不能回答“已经贴合性能”。发布前至少要采四组同条件数据：

1. 2D 工作台空载静置；
2. 2D 游历运行/收菜；
3. ChallengeViewport 主动 CardBattle；
4. 原 3D 城镇回退。

在用户批准新包络前暂沿用 `CPU≤3%`、`GPU Engine≤3%`、`GPU Dedicated≤350 MiB`、`Working Set≤650 MiB`、`Private≤1.6 GiB` 作为参考，不以未测量状态宣称达标。

### 6.2 素材规范

最终生产路径必须是：

`UI Master / PSD → reuse/derive/new 语义清单 → 透明切图 → alpha/尺寸/hash manifest → UE 资产导入 → 1920/2560 双分辨率复核`。

现阶段程序化棕橙面板只能证明布局、命中区域和状态机；不能直接作为最终视觉基准。仓库/背包的已有比例应优先复用实机 Master 组件，节点必须保持圆形，图标必须等比缩放，禁止把概念生成图直接裁进 PSD。

### 6.3 UX/入口

已经冻结的 UX 语义是：

- Tab 直接进入角色背包；底部五按钮为仓库、编队、天赋、工具、历练；
- 右侧历练地图与工具替换，点地图节点后挑战替换中间画布；
- 挑战只在局内显示自动战斗；游历只显示失败重试；
- 地图底部固定挑战/游历，游历按钮下方显示当前游历关卡；
- 全部关卡完成后挑战按钮置灰，悬停显示“期待新内容”；
- 设置必须独立于关闭按钮，天赋/称号、强化/洗炼/分解不占背包主导航。

这些语义在规则/程序化壳中已有部分映射，但还没有真实字体、图标、悬停和两分辨率截图证据。

## 7. 不应误报的项目状态

- 不能写“历练已完全挂机”：目前只有确定性 TravelRunner 和工作台 tick，没有 Actor/动画、离线计时或生产收菜。
- 不能写“局内已经完整复用实机战斗”：目前只完成真实 CardBattle 创建和一步推进，整条路线、输入、胜负、结算和视觉仍待 PIE。
- 不能写“宝箱掉落已完成”：Resolver/RNG、两档游历冷却和 v19 存档已完成并有 Automation；最终概率表、天赋真实数据、箱内物品和仓库/背包落库仍未完成。
- 不能写“PSD 已完成”：当前没有本工作台新图标 manifest/hash，也没有 Master UI 导入绑定。
- 不能写“性能符合 TBH”：当前没有工作台的同机四组采样。
- 不能写“主入口已替代 3D 城镇”：flag 明确为 false，3D 城镇仍是默认入口和回退点。

## 8. 关闭目标前的必要顺序

1. 把已通过 Automation 的 TravelRunner 接到生产角色/怪物 Actor、动画/移动表现、失败暂停/重试 UI、后台/离线时间和收菜窗口；保留当前纯规则 runner 作为可回滚核心。
2. 把挑战从“适配器可启动”补到“路线图→卡牌→意图→胜负→奖励→下一遭遇”的真实 PIE/MCP 流，并决定 pending card choice 的自动策略。
3. 绑定真实背包/仓库 read model：仓库 4 列多页/排序、背包金币/六装备槽/角色伙伴切换、工具容器、设置和关闭分离。
4. 按 UI Master 做 reuse/derive/new 清单，只为缺失的历练/节点/挑战/游历/重试图标出透明生产稿，登记尺寸、alpha 和 SHA256。
5. 补 1920×1080 与 2560×1440 PIE 截图和真实点击/悬停证据，重点检查节点不椭圆、图标不挤扁、字体/卡牌/意图可读。
6. 采集四组性能数据；若达不到 TaskBarHero 参考，先由用户批准新的包络，不提前切入口。
7. 清理或明确标注 asset-contract 的 15 个失败，修 Pillow 环境兼容、外部源路径和旧合同漂移；所有 SKIP 必须写明原因和源资产归属。
8. 全部门禁通过后，再把 opt-in flag 变为可选默认入口；保留一键回退 3D 城镇，并重新做完整冷 UBT/Automation/PIE/性能复核。

## 9. Goal 状态与回滚

本轮不调用 `update_goal(status=complete)`：完成判定要求 PSD、真实游历、完整挑战、奖励 RNG/天赋、PIE/MCP、性能和默认入口验收全部通过，当前明显未满足。也不标记 blocked，因为目前仍有可独立推进的代码、资产和验证工作。

最小回滚方案：

1. 把 `bEnableDesktopTrainingWorkbench` 保持/设回 `false`；
2. 回滚 `23aee95` 可移除 TravelRunner/工作台 tick；继续回滚 `7881927` 可移除 Training 运行时和 v18 接线；两者都不触碰 `L_Main.umap`；
3. 不删除或覆盖 `SourceAssets/`、`SourceArt/`、Master UI 和角色/地图资产；
4. 若产品冻结敌人或概率有变化，只更新规则/配置与测试，不把视觉概念图当补丁素材。

## 10. 本次复核的范围与方法

本次“完成后复核”不是对旧截图或旧报告的转述，而是以当前 `main`、当前工作区和刚刚刷新出的验证结果为准。复核按四条证据链执行：

1. **需求链**：逐项对照当前 goal、`2026-08-16-full-project-optimization-proposal.md`、`2026-08-17-gamexxk-desktop-training-workbench-design.md` 与本轮用户已经冻结的布局/交互口径。
2. **实现链**：检查 `GameXXKTrainingRules`、`GameXXKMVPSubsystem`、`GameXXKDesktopTrainingWorkbenchWidget`、`GameXXKMVPPlayerController`、存档迁移和对应测试的实际接口；不以 Widget 文案推断后端已经完成。
3. **验证链**：重新执行 harness、headless 脚本、冷 UBT、Training、DesktopTraining 和 SaveGame 聚焦 Automation；历史 598/598 只保留为历史回归证据。
4. **生产链**：单独审查 PSD/图标来源、真实库存、PIE/MCP 截图和 TBH 同机性能四组采样；没有证据的项一律记为“未验收”，不记为通过。

## 11. 当前目标的逐项交付矩阵

| 目标包 | 当前实现/证据 | 结论 | 关闭条件 |
|---|---|---|---|
| A. 项目状态与优化基线 | `main`/HEAD/脏工作区已记录；Phase 0 指针、旧历练 shelved、脚本标签和 v18→v19 边界已对齐 | **部分通过** | asset-contract 失败清零或逐项标明外部依赖；mcp-live 真实运行并留档 |
| B. 纯 2D 工作台壳 | `GameXXKDesktopTrainingWorkbenchWidget` 建立左 4 列仓库、中栏 1.76:1/4×5、右 27 节点/三难度、顶部 3+3 站位、底部五按钮 | **几何/接口通过** | UI Master 纹理绑定、真实字体/图标、真实数据、1920/2560 截图通过 |
| C. Tab 与五按钮导航 | `InputKey(Tab)` 在 opt-in 时打开并调用 `OpenBackpack()`；仓库/编队/天赋/工具/历练的替换关系已写入壳逻辑 | **规则部分通过** | PIE 中逐个点击并截取焦点、关闭/设置分离、战斗中输入锁定的证据 |
| D. 历练进度与难度 | 27 个稳定 StageId；普通 1-1 新档通关；普通→困难→地狱顺序和锁定规则由纯规则层验证 | **规则通过** | 真实地图节点状态、挑战/游历按钮 disabled/hover、全内容后的“期待新内容”截图 |
| E. 第一章敌人编制 | 公鸡/狸猫普通，山羊/黄鼬次级精英，1-1 山羊、1-2 黄鼬、1-3 青角羊王；挑战和游历共享编制定义 | **数据/测试通过** | 每个节点真实 tooltip 显示实际编制，且路线图不把两个次级精英错误合成同一波 |
| F. 游历循环 | `Walking → Combat → Defeated/下一遭遇` runner；一只敌人一次推进；击杀结算；1-1 游历 1 HP；每个 encounter 可按同一概率表掉箱，普通/高级箱冷却分别 240/360 秒；失败重试/回退 | **确定性运行时通过** | 接真实 Actor/动画/移动、Timer/离线时间、失败弹窗和收菜入库；不能只依赖 ForTest/tick |
| G. 主动挑战 | `StartTrainingChallenge` 建立真实 `FGameXXKCardBattleAdapter`；支持一步合法卡/阶段推进；工作台可切换 ChallengeViewport | **适配器通过，产品流未闭合** | 路线图→卡牌→意图→胜负→宝箱/经验→下一遭遇的完整 PIE/MCP；pending card choice 策略明确 |
| H. 奖励与天赋 | 普通/高级 tier 与金币/经验结算入口；`ResolveChallengeReward`/`ResolveTravelReward` 的稳定 seed、配置概率、天赋 bonus 参数；Travel 普通/高级冷却 240/360 秒 | **规则/聚焦 Automation 通过** | 接真实天赋 read model、最终概率、宝箱物品、仓库/背包写入、重复结算保护和收菜 |
| I. 存档与恢复 | `CurrentSaveVersion=19`；v18→v19 为 reward seed/Travel cooldown 增量迁移；训练互斥/范围/负值校验；runner 为 transient，读档后重建 | **聚焦通过** | 未结束挑战/游历在生产 UI 中断、退出、离线时间推进、读档、恢复和失败后状态一致 |
| J. 素材/PSD | 规格已冻结 reuse→derive→new、透明单图、1:1 安全框、alpha/尺寸/hash manifest | **规范通过，资产未交付** | UI Master 实际图层绑定；新增图标逐个 manifest/hash；无概念图裁切、噪点和非等比缩放 |
| K. 性能 | 已记录 TBH 参考包络和四组采样方案 | **未验收** | 同机 Shipping 空壳/游历/挑战/3D 四组数据，包含静置 30 FPS、退出回落和长时稳定性 |
| L. 默认入口 | `bEnableDesktopTrainingWorkbench=false`，3D 城镇仍为默认 | **保护正确** | 只有全部 A–K 关闭后才允许改为默认；保留回退开关并重新做全量验收 |

## 12. 代码规范与边界复核

### 12.1 已遵守的约束

- 关卡、编制、解锁、失败策略和奖励基础字段集中在纯 `FGameXXKTrainingRules`，Widget 不直接改存档 flag。
- `StartTrainingChallenge`、`StartTrainingTravel`、`AdvanceTrainingTravelStep` 和失败处理采用 Candidate 状态后提交，初始化失败不会留下半套挑战/游历状态。
- 持久状态在 `FGameXXKTrainingProgress`；`FGameXXKTrainingTravelRuntime` 明确为 transient read model，读档后由 `RebuildTrainingTravelRuntime()` 重建，避免把动画/当前伤害帧写进存档。
- 挑战与游历使用同一 Stage/Encounter 定义，但使用两套运行状态机；游历不能驱动 CardBattle，挑战不能偷用 1 HP 游历例外。
- 运行时新增逻辑先写聚焦 Automation，再执行冷 UBT（`-NoHotReload`）；没有用 Live Coding/Hot Reload 作为验收依据。
- 默认入口继续 opt-in，用户 `L_Main.umap`、PaperZD、角色像素图、相机/HD2D 参数和未跟踪源资产没有被本轮提交覆盖。

### 12.2 仍需单独立项的代码问题

- TravelRunner 目前是确定性规则/Slate tick，不是生产 Actor、动画、碰撞移动或后台离线服务；不能称为“完整挂机”。
- `BuildChallengeReward` 仍接收调用方的 `bChestRolled`，但仅用于旧 fixture；生产挑战和游历使用稳定 seed 的 `ResolveChallengeReward`/`ResolveTravelReward`。当前调用层传入 `0.0f` 天赋 bonus，因为真实天赋树 read model 尚不存在。
- 背包标题中的 `金币 0 · 数据来自存档` 仍是程序化壳文案，当前未证明金币实际从 RuntimeState 投影到 UI；仓库只读装备实例 snapshot，转移/排序/容量/选择态还没有工作台闭环。
- `BuildTopIdleStrip` 目前只有当前 runner 槽位是真实投影，其余站位仍是等待占位；角色/怪物 Actor 和像素动画没有接入。
- ChallengeViewport 已可挂接现有 BattleBoard，但完整手牌选择、目标选择、意图、结算、退出确认和 pending choice 自动策略还没有产品级闭环。

## 13. 素材、PSD 与 UI 生产复核

### 13.1 已冻结的生产规则

生产唯一链路是：

`UI Master/PSD → reuse/derive/new 语义审计 → 独立透明切图 → alpha/尺寸/hash manifest → UE 导入 → 1920×1080/2560×1440 视觉复核`。

仓库、背包和工具框体优先复用实机 Master；概念图只冻结三栏比例、节点层级和 UX。历练节点必须使用正方形母图并等比显示；导航、挑战、游历、失败重试、锁定、精英和首领图标不得烘焙文字/数字/水印，不得把多次重绘产生的噪点切进 PSD。

### 13.2 当前实际状态

- 程序化壳可以证明命中区域、按钮路由和尺寸合同，但不能作为最终美术基准。
- 当前没有本工作台新增图标的完整语义 ID、源路径、尺寸、alpha、生成/绘制来源和 SHA256 manifest。
- 当前没有接受的 1920×1080 与 2560×1440 工作台截图；因此“节点没有被压成椭圆”“图标没有被挤扁”“字体/卡牌可读”都还不能写成通过。
- `SourceAssets/` 与 `SourceArt/` 当前存在大量未跟踪内容，必须继续按资产归属/manifest 决策处理，不能用 `git add .` 解决。

## 14. 性能与桌面用量复核

### 14.1 可引用的同机参照

TaskBarHero 参照快照：Working Set 约 513 MiB、Private Bytes 约 1418 MiB、GPU Dedicated 约 282 MiB、整机 CPU 约 2.3%、GPU Engine 约 2%。这些是整进程长时样本，不是单张贴图预算。

当前临时验收包络仍为：CPU ≤3%、GPU Engine ≤3%、GPU Dedicated ≤350 MiB、Working Set ≤650 MiB、Private ≤1.6 GiB；游历静置 30 FPS，主动挑战退出后 30 秒内回落。当前没有 GameXXK 工作台的对应测量，因此性能结论是**未验收**而不是“已经贴合”。

必须在同机、同构建、无编辑器附加开销下补齐以下四组：

1. 空 2D Shell 静置；
2. 2D 游历运行与收菜；
3. ChallengeViewport 主动 CardBattle；
4. 原 3D 城镇回退。

每组需要记录启动后稳定窗口、30 FPS、CPU/GPU/Working Set/Private/Dedicated、对象/Widget/纹理是否持续增长，以及挑战退出后的回落时间。四组数据未齐前不得切换默认入口。

## 15. 本次刷新验证证据

| 检查 | 当前证据 | 判定 |
|---|---|---:|
| harness 状态 | `python scripts/harness_state_validator.py --json`，本轮 exit 0、`findings=[]` | PASS |
| headless 脚本门禁 | `Saved/HarnessReports/20260818-011435-ai-production-loop.md`，本轮 headless 全部通过 | PASS |
| 空白检查 | `git diff --check`，exit 0；仅有 Windows LF→CRLF warning | PASS |
| 冷 UBT | `Saved/HarnessReports/20260818-015636-ai-production-loop.md`，`GameXXKEditor`、`-NoHotReload`、Result Succeeded | PASS |
| Training Automation | `Saved/HarnessReports/20260818-015846-ai-production-loop.md`、`Saved/Automation/TrainingRewardCooldownGreen-20260818-r3/index.json`，13 discovered / 13 succeeded / 0 warnings / 0 failed | PASS |
| DesktopTraining Automation | `Saved/HarnessReports/20260818-015908-ai-production-loop.md`、`Saved/Automation/DesktopTrainingRewardCooldownGreen-20260818-r2/index.json`，1 / 1 / 0 / 0 | PASS |
| SaveGame Automation | `Saved/HarnessReports/20260818-015019-ai-production-loop.md`、`Saved/Automation/SaveGameRewardCooldownGreen-20260818/index.json`，12 / 12 / 0 / 0 | PASS |
| 历史全量 Automation | `Saved/Automation/ChargeFinishSubject/index.json`，598/598，2026-08-16 | **历史参考**，非本轮全量 |
| asset-contract | `Saved/HarnessReports/20260818-012130-ai-production-loop.md`，66 项中 51 PASS / 15 FAIL | **未关闭** |
| mcp-live / PIE | 本轮没有新的 1920/2560 工作台截图、点击流、悬停或窗口内存采样 | **未运行** |

## 16. 工作区、提交和回滚核对

- 本轮运行时提交为 `1a17019`，前置 TravelRunner 为 `23aee95`、桥接为 `7881927`；提交后 `git ls-files -m` 仍只剩用户已有 `Content/GameXXK/Maps/L_Main.umap` 与既有 `scripts/test_battle_camera_framing.py`。
- 当前仍有约 10,235 个未跟踪文件，其中约 10,176 个在 `SourceAssets/`/`SourceArt/`；它们不属于本轮提交，不得通过清理、覆盖或无差别 stage 处理。
- `L_Main.umap` 没有被 reset、checkout、格式化或加入本轮提交；默认 3D 城镇仍可回退。
- 最小回滚顺序仍为：保持 `bEnableDesktopTrainingWorkbench=false` → 单独回滚 `23aee95` → 如需再回滚 `7881927`；任何回滚都不触碰用户地图和源美术。

## 17. 关闭目标的阻塞项、顺序与完成定义

### P0：必须完成后才能谈入口

1. **生产游历**：Actor/动画/移动表现、失败暂停/重试 UI、后台/离线计时、收菜奖励入库、重新打开窗口恢复。
2. **生产挑战**：路线图到真实 CardBattle 的完整胜负/结算/下一遭遇流，含自动战斗、pending choice、退出和存档恢复。
3. **真实库存与 UI**：金币、仓库 4 列多页/排序/转移、背包六槽/角色伙伴切换、工具容器、设置与关闭分离，全部来自 read model。
4. **奖励产品闭环**：把已通过的 seed/RNG Resolver 接到真实天赋 bonus、最终概率表、重复结算保护、箱内物品生成和仓库/背包写入；Travel 240/360 秒冷却还要接真实计时/离线收菜。

### P1：生产内容与证据

5. UI Master reuse/derive/new 清单和新增图标 manifest/hash；单图与整页双重视觉复核。
6. 1920×1080、2560×1440 的 PIE/MCP 点击/悬停截图；确认节点圆形、图标不挤扁、字体/卡牌/意图可读。
7. 四组性能数据与挑战退出回落；asset-contract 失败按环境/旧合同/Pillow/源归属清理或明确 SKIP。

### Complete 的硬判定

只有 P0/P1 全部有当前提交、测试、截图或采样证据，默认入口仍可回退，且以下语义可重现时，才可 `update_goal(status=complete)`：普通 1-1 默认可游历、锁定/解锁、挑战/游历双按钮、失败重试/回退、两个精英 tooltip、首领 tooltip、普通/高级双宝箱、天赋掉率、存档恢复、低耗包络和 3D 回退。当前这些条件没有全部满足。

## 18. 最终决策

**本次复核结论：`not-complete`；goal 继续 `active`，不标记 `blocked`。**

理由不是编译失败，而是产品完成判定还需要真实生产表现、完整挑战/游历闭环、奖励概率、库存 read model、PSD/manifest、PIE/MCP 和性能四组证据。当前提交应被视为“可回滚、默认关闭、规则与运行时桥接可验证的基础包”；在上述门禁关闭前，不能把 2D 历练工作台当作替代 3D 城镇的正式主入口。
