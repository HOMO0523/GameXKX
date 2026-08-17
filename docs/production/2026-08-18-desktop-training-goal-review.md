---
status: review
owner: codex
updated_at: 2026-08-18
source_commit: 23aee95
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
3. **奖励仍是占位 Resolver**：普通箱/高级箱的概率没有真实 RNG；天赋树掉率加成没有接入；挑战奖励的金币/经验已写回，但箱子只是按占位概率选择层级。
4. **PIE/MCP 与性能证据缺失**：本轮没有新的工作台 1920×1080 / 2560×1440 截图、实际点击流、悬停视觉证据或 TaskBarHero 对照采样。

因此，本轮完成的是一个**可回滚、默认关闭、可自动化验证的 opt-in 运行时基础包**，不是可直接替代 3D 城镇的发行版本。

## 2. 工作区与保护边界

| 项目 | 当前事实 |
|---|---|
| 分支 | `main` |
| 本轮代码提交 | `23aee95 feat: run desktop training travel loop`；前置桥接提交为 `7881927` |
| 当前存档版本 | `CurrentSaveVersion=18`；v18 专门引入桌面 Training 进度 |
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
| 1-1 生命例外 | `BuildEncounterSequence(Stage, false)` 挑战生命 >1；`true` 游历序列为 1 HP，含首领；TravelRunner 与 subsystem wrapper 复用该规则；游历奖励无箱 | 规则与 runner 通过 | 没有真实 Actor/动画、离线计时和生产收菜窗口 |
| 挑战真实战斗 | `StartTrainingChallenge` 创建真实 `FGameXXKCardBattleAdapter` 会话，进入 `Battle`，投影现有敌人 catalog；BattleBoard 可挂到 ChallengeViewport | **新增且已自动化通过** | 尚未完成全路线手牌/意图/胜负/结算的 PIE 视觉闭环；pending card choice 会暂停自动步进 |
| 挑战自动战斗 | 每次调用推进一张合法卡、结束玩家阶段或解析敌方阶段；终结后结算并打开下一遭遇 | 部分通过 | 目前是运行时适配器，不是完整自动战斗 UX；需要手牌、意图、血条、tooltip 和战斗结算截图 |
| 游历循环 | `InitializeTravelRunner`/`AdvanceTravelRunner` 与 `AdvanceTrainingTravelStep` 已按 Walking→Combat→Settlement 推进 7 遭遇；失败重试/回退由 `ResolveTrainingTravelFailure` 处理；挂机条读取阶段/HP/遭遇 | **新增且 Automation 通过** | 仍缺真实 Actor/动画、后台/离线时间、失败弹窗和最终收菜 UI |
| 奖励与宝箱 | 普通/首领使用不同概率字段；普通箱/高级箱 tier 已区分；挑战金币经验会写回状态 | 占位通过 | `bChestRolled` 仍由调用方传入，未做可复现 RNG；天赋树 bonus 未接；未写真实仓库/背包掉落 |
| 存档迁移 | v18 Training 字段、旧档 Normal 1-1 初始化、challenge/travel 互斥；新增 challenge/travel encounter index 范围验证 | 通过 | 需要补实际战斗中断/恢复和最终存档 round-trip |
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
2. 挑战奖励的箱子目前是占位概率层级，不能被当作生产掉率。
3. 游历已有独立的纯规则 Runner 和 subsystem tick，但没有生产 Actor/动画、定时器后台/离线时间模型，不能声称已经实现完整挂机。
4. BattleBoard 嵌入工作台的视觉 session 目前是适配性接线，尚未在两种分辨率和真实输入下验证生命周期。

## 5. 验证证据（本轮真实运行）

| 验证 | 证据 | 结果 |
|---|---|---:|
| harness 状态 | 每次 `ai_production_loop.py` 报告中的 `harness_state_validator.py --json` | PASS，`findings=[]` |
| 代码空白 | 同上报告中的 `git diff --check` | PASS（仅 CRLF 提示） |
| headless script gate | `Saved/HarnessReports/20260818-002949-ai-production-loop.md` | 13/13 PASS；不启动 UE |
| Training rules + bridge + TravelRunner | `Saved/HarnessReports/20260818-005702-ai-production-loop.md`；`Saved/Automation/TrainingTravelRunner-20260818-r2/index.json` | 11/11 PASS；含 `RealCardBattleBridge`、`TravelRunnerLoop`、`TravelRunnerFailure`、`TravelSubsystemBridge`、`SaveValidation` |
| DesktopTraining geometry | `Saved/HarnessReports/20260818-010941-ai-production-loop.md`；`Saved/Automation/DesktopTraining-20260818-r3/index.json` | 1/1 PASS |
| SaveGame migration | `Saved/HarnessReports/20260818-003510-ai-production-loop.md`；`Saved/Automation/SaveGame-20260818-r3/index.json` | 12/12 PASS |
| Real bridge isolated rerun | `Saved/Automation/TrainingBridge-20260818-r6/index.json` | 1/1 PASS：真实 Battle 屏、1 个 authored enemy、auto step |
| Cold UBT | `Saved/HarnessReports/20260818-010554-ai-production-loop.md`，本轮变更后成功 | PASS；未用 Live Coding/Hot Reload |
| 历史全量回归 | `Saved/Automation/ChargeFinishSubject/index.json` | 598/598 是 2026-08-16 历史证据，只作为回归参考，不冒充本轮全量 |
| asset-contract | `Saved/HarnessReports/20260818-003545-ai-production-loop.md` | 51/66 PASS，15 个测试文件 FAIL，门禁未通过 |
| PIE/MCP 工作台 | 本轮仍无新的 1920×1080 / 2560×1440 工作台截图、点击流或悬停视觉证据 | 未运行 |

### 5.1 asset-contract 失败分类

本轮报告明确失败的 15 个文件是：

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
- 不能写“宝箱概率已完成”：当前概率是占位值，天赋 bonus 还没有 Resolver/RNG。
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
