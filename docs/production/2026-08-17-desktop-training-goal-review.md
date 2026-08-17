---
status: review
owner: codex
updated_at: 2026-08-17
source_commit: 8475ba8
decision: not-complete
goal_status: active
---

# 桌面历练工作台目标完成后复核

> 这是一份“完成后复核”格式的诚实状态记录。它把用户已经确认的桌面布局、背包/仓库约束、挑战/游历语义和项目级优化门禁，与当前工作区实际存在的代码、自动化报告和保护边界逐项对照。结论不是“功能已完成”：当前只完成了规则/存档的第一版和默认关闭的程序化 UI 壳，生产 PSD、真实战斗接入、挂机结算、性能和 PIE/MCP 验收仍未完成。

## 1. 最终判定

**当前 goal 不通过完成判定，保持 `active`。**

可以确认的成果：

- Phase 0 的真源文档、旧历练计划的 shelved/superseded 标识、脚本标签边界、GBK 输出和个人路径参数化已经落地；
- Training 规则对象、v18 存档字段/迁移、程序化工作台和 PlayerController 的 opt-in 接线已经编译并通过聚焦 Automation；
- 默认入口仍保持 3D 城镇，`bEnableDesktopTrainingWorkbench=false`，所以没有把未验收的 2D 工作台强行切成主入口；
- 用户已有 `Content/GameXXK/Maps/L_Main.umap` 修改以及 `SourceAssets/`、`SourceArt/` 的大批未跟踪资产没有被回滚或加入工作提交。

不能宣称完成的原因：

1. 运行时工作台目前是 `UBorder/UTextBlock/UButton` 程序化壳，不是经过 PSD/MasterV2 复用、透明图标 manifest/hash 和 1920/2560 双分辨率校准的生产 UI。
2. 挑战界面只有规则 façade；尚未接入现有 CardBattle/RouteMap 的真实路线、手牌、意图、血条、战斗胜负和结算写回。
3. 游历没有真实走动/遭遇计时/自动攻击/阵亡暂停/金币经验入账/离线收益循环；失败重试目前只验证了规则状态，不是实机挂机。
4. 掉箱概率仍是占位值，没有真实 RNG、天赋树掉率 Resolver 或奖励写回；1-1 的“一滴血只用于游历”尚未与挑战生命值严格分离。
5. 第一章“4 个普通候选中哪些作为普通、哪些作为次级精英”的最终映射仍需产品冻结；当前代码直接取 catalog 的 4 Normal + 2 Elite，存在口径差异风险。
6. 没有新的 1920×1080/2560×1440 PIE 截图、TaskBarHero 对照包络或 Shipping 静置/局内性能数据；asset-contract 也没有全绿。

因此，当前正确的发布策略是：**保留 3D 城镇默认入口，继续以 opt-in/测试入口推进；不启用主入口替换。**

## 2. 需求真源与冻结内容

### 2.1 桌面壳布局

| 区域 | 用户冻结要求 | 当前代码/状态 | 判定 |
|---|---|---|---|
| 左侧 | 全高仓库、4 列；不显示“小侠客 Lv1”身份卡；仓库有多页/排序 | `BuildWarehousePanel()` 有 4 列、20 个占位格，读取仓库快照；未完成真实分页、真实图标和页签交互 | 部分 |
| 中间 | 保持实机背包比例约 1.76:1；角色/伙伴在背包内切换；金币显示在背包；只留排序 | `BackpackAspectRatio` 与几何测试已锁定；标题/金币/六装备槽/4×5 格为占位文本；真实背包数据和图标尚未绑定 | 部分 |
| 右侧 | 历练地图或工具；点击工具替换右侧；历练显示 9 节点/三难度 | 程序化地图生成当前难度 9 个节点和三难度页签；工具只改变标题，未接真实工具容器 | 部分 |
| 顶部 | 3 个敌方站位 + 3 个角色站位；游历条只有失败重试 | 有 3+3 文本占位与失败重试按钮；没有角色像素图、行动/攻击/受击演出和真实挂机状态 | 部分 |
| 底部 | 仓库/编队/天赋/工具/历练五按钮；Tab 打开角色背包 | 五个程序化按钮存在；Tab 仅在 opt-in 且 Town 时打开壳并切到编队；真实页面替换和菜单状态未完成 | 部分 |
| 设置 | 与关闭分离并放在背包上 | 当前新壳尚未实现设置按钮；旧 Town HUD 的生产设置/关闭没有迁入工作台 | 未完成 |

### 2.2 历练玩法

| 需求 | 期望语义 | 当前状态 |
|---|---|---|
| 难度 | 普通/困难/地狱，每档 1-1 至 3-3 共 9 关；普通九关解锁困难，困难九关解锁地狱 | 规则层有 27 个稳定 StageId；`DifficultyUnlocks` Automation 通过 |
| 默认 | 普通 1-1 新档默认通关，可游历 | `InitializeNewGame()` 写入 Normal 1-1 cleared/current/travel active；规则测试通过 |
| 挑战/游历 | 挑战首次通关解锁；游历只能选已通关；按钮固定分离 | `CanChallenge/CanTravel/StartChallenge/StartTravel` 已有；UI façade 已有两个按钮 |
| 全部通关 | 挑战按钮置灰，悬停“期待新内容” | 已对已通关节点设置 disabled/tooltip，但未在真实 Slate/PIE 上复核悬停样式 |
| 失败 | 重试开：当前关；关：前一关；1-1 仍为 1-1 | `ResolveTravelFailure()` 与 Automation 覆盖；没有实际挂机阵亡触发 |
| 游历循环 | 走动→遭遇→自动攻击→击杀→单场结算→继续循环 | 只有描述文本和状态 façade；没有定时器、敌人 Actor、伤害/HUD/奖励写回 |
| 局内挑战 | 路线图/卡牌战斗/自动战斗只在挑战局内 | ChallengeViewport 只展示 Encounter 文本，自动按钮推进规则索引；没有真实 CardBattle |

### 2.3 第一章敌人和奖励

当前代码事实：

- `BuildStages()` 从 `FGameXXKEnemyCatalog` 取每章 4 个 Normal、2 个 Elite；
- 当前第一章 Boss 映射为 1-1 `Enemy.Ch1.BluehornGoatKing`、1-2 `Enemy.Ch1.IronfeatherRooster`、1-3 `Enemy.Ch1.MoneyRat`；
- `BuildEncounterSequence()` 生成 4 个普通遭遇、2 个次级精英遭遇、1 个 Boss 遭遇，并保证路径有两个 Elite；
- 1-1 的 `bOneHealthTravelException` 当前被 encounter builder 直接用于整条 encounter sequence，尚未仅限制在游历运行时；
- `BuildChallengeReward()` 只依据占位概率和传入的 `bChestRolled` 选择 NormalChest/AdvancedChest，未执行真实 RNG，也未接天赋树。

这意味着“存在两个精英节点”和“Boss tooltip 可见”已有数据基础，但以下项目必须在产品冻结后再实现：

1. 明确“公鸡/狸猫为普通、山羊/黄鼬为次级精英”与现有 catalog Elite 变体的映射，不允许用名字相近就默默替换；
2. 明确 1-1/1-2 Boss 是现有精英当 Boss，还是用户后来指定的另一组 Boss；
3. 将游历 1 HP 例外从挑战路线的基础生命值计算中拆出；
4. 将普通/精英普通箱、Boss 高级箱、天赋掉率加成写入配置和可复现 RNG，并由奖励 Resolver 统一结算。

## 3. 已落地文件与工作包

### 3.1 已提交的 Phase 0 基础

- `ba90810 docs: freeze desktop training workbench design`
- `c419b7b docs: add phase 0 source-of-truth plan`
- `6300a92 docs: record phase 0 project baseline`
- `9313325 docs: mark legacy idle migration plans shelved`
- `3bcb039 test: separate headless and environment script gates`
- `c4762be chore: parameterize external asset migration paths`

相关真源：

- `docs/production/current-goal-acceptance.md`
- `docs/production/2026-08-17-phase0-baseline.md`
- `docs/superpowers/specs/2026-08-17-gamexxk-desktop-training-workbench-design.md`
- `docs/superpowers/plans/2026-08-17-gamexxk-phase0-source-of-truth-and-gates.md`
- `scripts/script-test-manifest.json`

### 3.2 当前未提交的 opt-in 运行时工作

| 文件 | 内容 | 当前边界 |
|---|---|---|
| `GameXXKTrainingRules.h/.cpp` | 难度、StageId、遭遇、挑战/游历状态、失败策略、奖励层级 | 规则层，不是完整战斗/挂机执行器 |
| `GameXXKMVPRules.h/.cpp` | SaveState 增加 Training，创建新档初始化 | 只完成字段/默认值 |
| `GameXXKSaveMigration.h/.cpp` | v17→v18 Training 初始化/归一化/校验 | 未做旧历练真实数据迁移矩阵 |
| `GameXXKMVPSubsystem.h/.cpp` | 查询、选择、挑战推进、游历启动、失败策略 API | 真实货币/奖励/战斗写回未接 |
| `GameXXKDesktopTrainingWorkbenchWidget.h/.cpp` | 程序化壳、几何契约、节点 tooltip、ChallengeViewport façade | 无 PSD、无真实图标、无卡牌战斗 |
| `GameXXKMVPPlayerController.h/.cpp` | opt-in class/flag、Tab、打开/关闭、离开 Town 自动收壳 | 默认 false；无默认入口迁移 |
| `GameXXK*Training*Test.cpp` | 规则和几何合同 | 只证明纯函数/壳合同，不证明实机视觉 |

## 4. 验证证据

| 验证 | 报告/命令 | 结果 | 解释 |
|---|---|---:|---|
| 状态验证 | `python scripts/harness_state_validator.py --json` | PASS | `findings=[]` |
| headless 脚本 | `Saved/HarnessReports/20260817-233157-ai-production-loop.md` | 13/13 | `--script-tests all` 当前只运行 headless，不拉起 UE |
| asset-contract | `Saved/HarnessReports/20260817-233751-ai-production-loop.md` | 51/66 | 15 项失败，涉及外部源、旧 hash/manifest、Pillow API、受保护地图合同等；Phase 0 总门禁不通过 |
| 工作台几何 | `20260817-234829-ai-production-loop.md` / `GameXXK.DesktopTraining` | 1/1 | 4 列、1.76 比例、27 StageId 等程序化合同 |
| Training 规则 | `20260817-234854-ai-production-loop.md` / `GameXXK.Training` | 4/4 | 新档、难度解锁、失败策略、第一章构成/奖励占位 |
| SaveGame | `20260817-234916-ai-production-loop.md` / `GameXXK.MVP.SaveGame` | 12/12 | 修正 v18 断言后迁移测试全绿 |
| 冷 UBT | `GameXXKEditor Win64 Development -NoHotReload` | PASS | 编辑器安全关闭后冷编译；未用 Live Coding/Hot Reload |
| 历史全量 Automation | `Saved/Automation/ChargeFinishSubject/index.json` | 598/598 | 2026-08-16 历史证据，不是当前工作区的新全量运行 |
| 真实 PIE/MCP | 本轮无新工作台截图/流转记录 | 未运行 | 不能证明字体、比例、点击流、悬停和实际战斗 |
| 性能 | 无当前工作台 Shipping/PIE 采样 | 未测 | 不能对 TaskBarHero 包络作结论 |

### 4.1 asset-contract 失败分类

最新独立报告的 15 项失败不是被隐藏的“环境跳过”，包括：Pillow `get_flattened_data` API 不兼容；terrain/PSD/portrait 外部源或 manifest 缺失；PartyDeck 资源合同未 ready；UI Master 预览数量/Task icon 约束；以及 `L_QingshanInn.umap` raw SHA 与旧合同不一致。后两类与当前用户保护的手调地图/源资产策略有关，不能通过回滚用户资产来“修绿”。正确处理方式是逐项更新合同或明确 `SKIP/外部源`，再重新生成报告。

## 5. 性能与素材规范复核

### 5.1 目标包络（尚未测量）

同机 TaskBarHero 参照曾记录约 `Working Set 513 MiB / Private 1418 MiB / GPU Dedicated 282 MiB / 整机 CPU 2.3% / GPU Engine 2%`。目标工作台需要至少给出以下四组数据：

1. 空壳桌面 2D 静置；
2. 2D 工作台游历静置/收取；
3. ChallengeViewport 主动战斗；
4. 原 3D 城镇回退。

在用户批准上限前，目标参考仍是 `CPU≤3%`、`GPU Engine≤3%`、`GPU Dedicated≤350 MiB`、`Working Set≤650 MiB`、`Private≤1.6 GiB`；当前没有证据证明达到或超出这些值。

### 5.2 PSD/图标生产规范

必须以 `GameXXK_UI_Master_V1.psd` 和 UI Master manifest 为唯一美术真源：

- reuse/derive/new 先登记语义 ID，再出图；
- 复用面板、按钮、槽位、页签、tooltip、资源条和导航，不重绘已有基元；
- 只有仓库、编队、天赋、工具、历练、节点状态、挑战/游历/重试等真正缺失图标才进入 new；
- 新图标透明、无文字/数字/水印、1:1 安全框、像素边缘干净，禁止非等比拉伸和概念图直接切片；
- PSD 保持可编辑图层，manifest 记录语义 ID、来源、尺寸、alpha 检查和 SHA256；
- 1920×1080 与 2560×1440 必须用同一等比映射验证，节点保持圆形，字体/卡牌/意图可读。

当前工作区还没有为本工作台建立 production icon manifest/hash，也没有完成 MasterV2 绑定；程序化橙色/棕色面板只能用作交互和几何验证，不得进入最终 PSD/UE 资产。

## 6. 工作区保护和回滚点

### 6.1 保护结论

- `Content/GameXXK/Maps/L_Main.umap` 仍是用户/编辑器已有 tracked 修改；不得 reset、checkout、格式化或加入工作提交。
- `SourceAssets/`、`SourceArt/`、`Content/Python/` 和 `scripts/` 下大量未跟踪探针/生成资产保持原样；未执行 `git add .`，没有把源美术归属擅自改成仓库资产。
- 未修改 `.uasset`、`.umap`、PaperZD、角色像素图、相机变换或 HD2D 平面参数。

### 6.2 回滚点

1. **入口回滚**：`bEnableDesktopTrainingWorkbench=false`，继续使用 3D Town；这是当前默认值。
2. **运行时回滚**：只删除/回滚本轮新增的 `GameXXKTrainingRules.*`、`UI/GameXXKDesktopTrainingWorkbenchWidget.*`、Subsystem/Controller Training 接线及对应测试；不要碰 `L_Main.umap` 或未跟踪资产。
3. **存档回滚**：v18 迁移仅增加 Training 字段；若产品否决，可在独立提交中移除 v18 变更并保持旧存档版本，不可复用已 shelved 的 v16/v17/v18 历练字段。
4. **美术回滚**：所有新图标必须先放候选目录并留 manifest/hash；未通过视觉验收不得覆盖 MasterV2 或现有 Town HUD。

## 7. 必须完成的下一阶段顺序

1. 先冻结第一章四类敌人、1-1/1-2 Boss、四普通/两精英遭遇次数，以及“1 HP 只作用于游历”的数据模型，并为每个 tooltip 写失败测试。
2. 把 `AdvanceTrainingChallengeEncounter` 换成现有 CardBattle/RouteMap 的真实适配层：同一份编制快照，不共用挑战和游历运行状态；战斗胜利后统一结算并保存。
3. 实现 TravelRunner：走动/遭遇/攻击/受击/击杀/单场结算/循环、阵亡暂停、重试开关、1-1 回退规则、金币经验和箱子概率；接天赋 Reward Resolver。
4. 做 PSD reuse/derive/new 审计，输出 UI Master 页面和新增图标 manifest/hash；把背包真实比例、金币、仓库 4 列、多页、排序、设置/关闭分离接入实际纹理。
5. 用 NullRHI/PIE 验证 Tab→背包、历练→节点→挑战/游历、节点 hover、难度解锁、挑战完成、失败重试和回到 3D Town；补 1920×1080/2560×1440 视觉证据。
6. 采集四组性能数据并与 TaskBarHero 对照；若 UE 固定开销达不到上限，先做空壳/静置/局内/3D 四组报告，再由用户批准新的包络。
7. 逐项清理 asset-contract 15 项失败；涉及外部个人路径的测试只能标 `SKIP` 并记录源资产，不得伪造通过。
8. 所有上述项通过后，才允许将 opt-in flag 改为默认入口候选，并保留一键回退到 3D 城镇的开关。

## 8. Goal 状态更新规则

本复核不调用 `update_goal(status=complete)`，因为“PSD/真实战斗/挂机/性能/PIE/默认入口验收全部通过”的完成条件尚未满足。若后续连续三轮因同一外部条件（例如用户未提供源 PSD 或未决定敌人映射）无法推进，才按 goal 规则标记 `blocked`；在那之前继续保持 `active`，每个工作包用新报告和独立提交推进。
