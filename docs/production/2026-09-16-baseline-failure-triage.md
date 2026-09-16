# 2026-09-16 全量回归失败分诊（170 Fail）

只读分析。本轮**没有修改任何 `Source/`、`Content/`、`Config/` 文件**，没有构建，没有启动编辑器。唯一新增文件就是本报告。

## 0. 证据来源与方法

| 来源 | 用途 |
|---|---|
| `Saved/Baseline-20260916/baseline-full2/index.json` | 权威结果：1258 发现 / 1088 成功 / **170 失败** / 606 警告。每个失败测试带 `entries[]`（`event.message` + `filename` + `lineNumber`），比日志更好用 |
| `Saved/Baseline-20260916/baseline-full2/Editor.log` | 交叉核对（13764 行，2 MB）。**该 log 不含任何 value-bearing TestEqual 文本**（全 1258 项里 `actual=` 出现 0 次） |
| `Saved/Automation/FontRollout20260912/**/index.json` | **2026-09-12 的上一份基线**（9 个前缀分卷）：覆盖 682 项，746 Success / 19 Fail。用来把失败切成“窗口内新增”和“早已红” |
| `Saved/Automation/**`、`Saved/*2026091*/**` 共 3369 份历史 `index.json` | 每项失败测试的“上一次绿”时间 |
| git | 最后提交 `02917d62`（2026-09-16 21:12，“snapshot 2026-09-13~16”）。该提交**就是**用户所说的“4 天未提交工作”的快照；`git status` 只剩 7 个 21:26–21:43 的改动，全部发生在本轮运行（21:28–21:41）**之后或之中** |
| 引擎 | `D:\UE_5.8\...\Misc\AutomationTest.h:2196` 与 `AutomationTest.cpp:2065/2145` 确认 `TestEqual(What, Actual, Expected)` 按 **“to be `<Expected>`, but it was `<Actual>`”** 打印 |

### 关键时间校正

`Editor.log` 用 UTC，`index.json` 用本地时。本轮实际运行时间 = **本地 2026-09-16 21:28–21:41**。因此：

- `GameXXKEnemyManaSiphonTest.cpp` 的崩溃修复（mtime 21:26:32）**在**本轮之前 → 本轮跑完了。
- `MVP/GameXXKAcademyStateBuilder.cpp`（mtime 21:43:03）、`Guide/GameXXKAcademyRules.*`、`Guide/GameXXKAcademySubsystem.cpp`、`UI/GameXXKDesktopTrainingWorkbenchWidget.*` 的改动**在**本轮之后 → **工作区已经部分修掉了本报告中的 A1 组**，报告描述的仍是 21:28 那一次运行的代码状态。
- 这些改动随后被提交为 **`b7a89fef`“fix: restore regression ability, gate academy courses, and read Shift from Slate”（2026-09-16 21:52:52）**，即 `02917d62` 之后的新 HEAD。因此本报告引用 `02917d62` 的 git 证据仍然成立（它就是 09-13~16 那批工作的快照），但 **A1 组中 Academy 那一支（`GameXXKAcademyStateBuilder.cpp:20` 置 `bProgressivePartySlots=false`）已经在 `b7a89fef` 里落地**，其余 A1/A2 夹具问题未修。

### 最重要的框架性结论

**170 项失败不是同一个窗口造成的。** 用 09-12 基线与 3369 份历史报告回查：

- **16 项在 2026-09-12 那次基线里就已经是 Fail**（当时覆盖到它们）：CardDocumentation、CardOutcomePreview×3、DesktopTraining.Workbench.NoticeRail/ParentCloseStack、Development.AuthoredBattleAndSimulation、Integration.CardBattle.Board*×8。
- **74 项在任何一份历史报告里都没有成功记录**（多为 09-05/09-07 起就红）。
- **93 项上一次为 Success**，其中 09-12 15:43/15:47/15:37/15:16 那一批（DesktopTraining / Data / Localization / Training / ToolsRedesign）**确定是 09-13~16 窗口引入的**。
- 另外，**动画节奏（B 组）、HUD 锚点（C 组）、`气血→HP`（D 组）、敌方意图卡尺寸（I 组）、状态/护甲 tooltip 文案（M 组）都没有被 09-12 那次基线覆盖**（它的 9 个前缀里没有 `GameXXK.MVP.Battle.*`、`GameXXK.Presentation.*`、`GameXXK.UI.*`、`GameXXK.Interaction.*`），所以“09-12 是绿的”不能推出这些也是绿的；git 证据显示它们分别由 09-01 / 09-07 / 09-10 / 09-11 的提交引入。

---

## 1. 汇总表

| # | 根因组 | 失败测试数 | 分类 | 建议批次 |
|---|---|---|---|---|
| **A1** | **渐进出战槽：NPC 槽门禁**（`bProgressivePartySlots` + `Talent.Party.NpcSlot`） | **36** | `FIXTURE-ISSUE`（设计是有意的；夹具过时） | 必修批 1 |
| **A2** | **渐进出战槽：编队塌缩成只有主角**（同一门禁经 `Validate`/`BuildLegacyProjection`） | **36** | `FIXTURE-ISSUE` | 必修批 1（与 A1 同一刀） |
| B | 战斗动画节奏回退（0.82/0.30/0.45/0.26/Death clip） | 3 | `REAL-REGRESSION`（含 1 处待决资产模型） | 必修批 2（需小心） |
| C | HUD 归一化锚点 float/double 过精确比较 | 1 | `STALE-EXPECTATION` | 机械批 |
| D | 本地化表 `气血/内力` 的 `zh-Hans` 列写成英文 | 1 | `REAL-REGRESSION`（数据） | 机械批（改 JSON） |
| E | 教学宝箱账本新不变量（`配套普通宝箱缺失/不一致`） | 6 | `FIXTURE-ISSUE` | 机械批 |
| F | 存档 schema 硬编码 36（现为 47） | 4 | `STALE-EXPECTATION` | 机械批 |
| G | 天赋图新增 2 节点导致揭示数 +1 | 4 | `STALE-EXPECTATION` | 机械批 |
| H | MetaShop 购买被 `ValidateRuntimeState` 拒绝（A 组下游） | 2 | `FIXTURE-ISSUE`（推断） | 必修批 1 |
| I | Boss/奖励卡牌槽 + 卡牌 UI 尺寸/图标（09-12 前就红） | 13 | 混合：`REAL-REGRESSION` 2 项、其余 `UNKNOWN`/早已红 | 单独排查 |
| J | 旧 3D 城镇壳层：横版 flipbook / 镜像 / 相机 pitch | 1 | `STALE-EXPECTATION` + `UNKNOWN`（用户手调值，需决策） | 需决策 |
| K | NPC 叙事序列 ID 解析为 None | 6 | `UNKNOWN` | 需排查 |
| L | Codex 发现/持久化 | 2 | `UNKNOWN` | 需排查 |
| M | 战斗表现文案/读数/布局常量 | 7 | 混合（大多早已红） | 单独排查 |
| N | 卡牌结算：Drum 联动伤害 / 流血层数 | 4 | `REAL-REGRESSION` | 需小心 |
| O | 营地治疗与已退休护符 | 2 | `REAL-REGRESSION`（可能半迁移） | 需小心 |
| P | 普通宝箱材料池概率表 | 1 | `STALE-EXPECTATION`（09-11 宝箱表改版） | 机械批 |
| Q | 模拟策略/自动选目标前置条件 | 3 | 混合 `UNKNOWN` | 需排查 |
| R | 装备强化/分解/存档镜像 | 3 | 混合 `UNKNOWN` | 需排查 |
| S | 训练工作台（旅行/图鉴/背包/名单） | 18 | 混合：A 组下游 + 若干 `UNKNOWN` | 必修批 1 + 排查 |
| T | 同伴名册/背包分页 UI | 6 | 混合：A 组下游 + `UNKNOWN` | 必修批 1 + 排查 |
| U | 队伍成长/等级门禁/天赋经济 | 5 | 混合：A 组下游 + `STALE-EXPECTATION` | 必修批 1 |
| V | 主菜单存档槽 / PlayableRoot 全流程 | 3 | 混合：`STALE-EXPECTATION` + A 组下游 | 机械批 + 必修批 1 |
| W | 其余单点（3 项） | 3 | 混合 | 单独 |

合计 **170 / 170**，无遗漏（已用脚本核对：`assigned = 170 / 170`）。

---

## 2. 逐组详情

### A1 —— NPC 槽门禁（36 项，`FIXTURE-ISSUE`）

**用户线索 1 已验证并扩展。** 门禁链条（全部已核对）：

| 位置 | 内容 |
|---|---|
| `Source/GameXXK/Private/GameXXKTrainingRules.cpp:648` | `InitializeNewGame()`：`Progress.bProgressivePartySlots = true;` → **每个新档都是渐进档** |
| `Source/GameXXK/Public/GameXXKTrainingRules.h:362` | 结构体默认值是 `false`，只有 `InitializeNewGame` 置 true |
| `Source/GameXXK/Private/GameXXKPartyFormationRules.cpp:217-222` | `IsSlotUnlocked`：`bProgressivePartySlots` 为真时还要求 `State.Talents.NodeRanks.FindRef(SlotTalentId(Kind)) > 0` |
| `…:192-197` | `SlotTalentId`：`PermanentCompanion→Talent.Party.CompanionSlot`、`QuestNpc→Talent.Party.NpcSlot` |
| `…:318-323` | `SetQuestNpc` 拒绝并报 **`Selected NPC is not one of the six owned definitions.`** |
| `Source/GameXXK/Private/GameXXKMVPRules.cpp:1789-1801` | `CreateNewGame()` → `InitializeNewGame`；`CurrentMapId = TEXT("MainMenu")`（:1794） |
| `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp:5138-5152` | `EnsureQingshanTownRuntimeForDirectMap()` → `StartNewGame()`（不是轻量预览） |

**门禁是有意设计，不是回归。** 决定性反证：

1. `Source/GameXXK/Private/Tests/GameXXKPartySlotProgressionTest.cpp:215-216` 与 `:251-252` 专门测新规则（买天赋才解锁 / 旧档免费解锁）——**通过**。
2. `Source/GameXXK/Private/Tests/GameXXKTravelFirstChallengeTest.cpp:28`：`TestFalse(TEXT("Companion slot still requires its purchase"), …)`——**通过**。
3. `docs/production/2026-09-15-party-slot-onboarding.md` + 提交信息“progression-based party slots, 1-1/1-2 challenge-vs-rules”（`02917d62`）。
4. `git show 02917d62 -- Source/GameXXK/Private/GameXXKPartyFormationRules.cpp` 就是这次改动本身（新增 `SlotTalentId`/`IsSlotEligible`/`IsFirstNpcPending`/`IsSlotUnlocked`）。

**已存在的正确夹具**：`Source/GameXXK/Private/Tests/GameXXKPermanentPartyTestFixtures.h:27-40` 的 `MakeStartedState()` 第 36 行显式 `State.Training.bProgressivePartySlots=false;`，注释写着 “This fixture exercises the established full-party systems, not onboarding.”

**逐项判定（按状态构造方式二分）**

| 构造方式 | 测试 | 判定 |
|---|---|---|
| `UGameXXKMVPSubsystem::BuildAcademyBattleState` → `GameXXKAcademyStateBuilder::BuildLoadout` (`MVP/GameXXKAcademyStateBuilder.cpp:14,46`) | `GameXXK.Academy.CatalogAndLoadouts`、`GameXXK.Academy.GuidedObjectives`、`GameXXK.Academy.PlayableObjectives` | `FIXTURE-ISSUE`（**工作区 21:43 已修**：`GameXXKAcademyStateBuilder.cpp:20` 现在置 `bProgressivePartySlots=false`） |
| `Tutorial01*` 系列 → `GameXXKMVPSubsystem.cpp:5151 StartNewGame()`（`GameXXKTutorial01SessionTest.cpp:27` 等） | `Tutorial01.BattleFlow`、`Tutorial01.Session`、`Tutorial01.PlayerFlow`、`Tutorial01.BattleGuide.RealTargets`、`Tutorial01.BattleGuide.AuthoritativeActionChain` | `FIXTURE-ISSUE` |
| `UGameXXKMVPSubsystem::StartGame()` 直建 | `Development.BenchmarkTelemetry`、`Development.ResumedPhaseRegression`、`UI.CompanionRoster.PersonalDeck`、`UI.CompanionRoster.ProfileExperienceAndTownOnlyClear`、`UI.CompanionRoster.RouteLock`、`MVP.Companion.RecruitmentFlow.TownFacadeAndPersistence` | `FIXTURE-ISSUE` |
| `GameXXKRouteBalanceSimulationTest.cpp` 系列（无资金/天赋种子） | `RouteBalance.SingleCaseRealRules`、`RouteBalance.CalibrationCoarseSweep`、`RouteBalance.CalibrationProfileProjectsExactEnemyStats`、`RouteBalance.Determinism.ChapterTwoNormalReplay`、`RouteBalance.FullMatrixExecution`、`RouteBalance.Diagnostics.*`（9 项） | `FIXTURE-ISSUE`（**单项失败数最多**：FullMatrixExecution 把 2400 例全废） |
| `GameXXKQuestNpcBalanceTuningTest.cpp`（每例 90 seed） | `RouteBalance.NpcTuning.YueBaiSurvivalBudget`（90 条）、`…ZhouGuangZuDamageBudget`（90 条）、`…YueBaiTaskLoopSeed1300216` | `FIXTURE-ISSUE`（`GameXXKQuestNpcBalanceTuningTest.cpp:94` / `:184`，断言数最多） |
| `GameXXKEquipmentBudgetObservationTest.cpp:56` / `GameXXKOrthogonalBalanceObservationTest.cpp:177` / `GameXXKCardBalanceObservationTest.cpp:324` | `Diagnostics.EquipmentBudgetObservation`、`Diagnostics.OrthogonalBalanceObservation`、`Diagnostics.CardBalanceObservation` | `FIXTURE-ISSUE`（观测跑批全 2400 例 error） |

**⚠ 需要产品决策的一点（不是本组测试的错）**：`Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp:5147-5150` 的注释仍写着直建地图“the six starter partners, six always-owned named NPC loadouts, and **exact three-member formation** cannot diverge by launch surface”，但 `StartNewGame()` 现在产出的是单人队伍。注释与行为已矛盾，应随 A 组一起更新，否则下一个读者还会照旧写夹具。相关测试 `GameXXK.MVP.PlayableShell.HUDCommandsDriveFullLoop:213-217`、`GameXXK.MVP.PlayableShell.DirectQingshanEntryParty`、`GameXXKMVPPlayableShellTest.cpp:213-217` 直接把这个注释当契约。

---

### A2 —— 编队塌缩（36 项，`FIXTURE-ISSUE`）

同一门禁的**第二个症状面**：`FGameXXKPartyFormationRules::Validate` 第 **397-401** 行对每个成员调 `IsSlotUnlocked`，失败即整表无效；`ResolveEffective:257-277` / `Normalize:465-480` 于是回退到 `BuildLegacyProjection:224-255`，而该函数在 `:235` 和 `:243` 也对槽位加锁 → **只剩英雄一人**。

这解释了整族的三种表象：

1. `… to be 3, but it was 1`（`Validate` 拒 3 人表 → legacy 投影只给英雄）。
2. `Expected '…' to be Npc.<X>, but it was None`（`ResolveQuestNpcId` 找不到 NPC 槽；`ProjectCompatibility` 把 `PartySelection.QuestNpc.NpcId` 写成 `NAME_None`）。
3. `Expected '…' to be not null` / “The fixture has no active companion to retag.”（同伴槽同样被锁，`PartySelection.ActivePermanentCompanionInstanceId` 为 None）。

代表性决定证据（每组一条）：

| 测试 | 决定证据（file:line） |
|---|---|
| `Integration.CardRoute.Lifecycle` | `Tests/GameXXKCardRouteLifecycleTest.cpp:30` 选 NPC 失败 → 后续 :39/:42/:71/:74 全 None |
| `Integration.CardRoute.QuestNpc` | `GameXXKCardRouteQuestNpcTest.cpp:28` |
| `Integration.CardRoute.QingshanTaskNpc` | `GameXXKQingshanTaskNpcRouteTest.cpp:39` |
| `Integration.CardBattleAdapter.Core` | `GameXXKCardBattleAdapterTest.cpp:130,131,147,149,154`（16 张开局牌组变 8 张） |
| `MVP.Companion.Facade.*`（8 项） | `GameXXKCompanionFacadeTest.cpp:687,587,486,483`（“three members → 1”）、`:373,392`、`:147` |
| `MVP.StarterCompanion` | `GameXXKStarterCompanionTest.cpp:47,57,78,84,89,105` |
| `MVP.FullFlow` | `GameXXKMVPFlowTest.cpp:184,186,198,199,201,230,278,291,293` |
| `MVP.Town.NpcInteractionRules` | `GameXXKTownNpcInteractionRulesTest.cpp:102,105,108,112,115` |
| `MVP.UI.PlayerControllerOwnsFlowWidgets` | `GameXXKPlayerFlowWidgetTest.cpp:930,934,1005,1048,1061-1063` |
| `MVP.UI.WidgetBasesDriveRules` | `GameXXKMVPUIWidgetTest.cpp:97,101,225,261,274,276` |
| `MVP.PlayableShell.*Party` / `HUDCommandsDriveFullLoop` | `GameXXKMVPPlayableShellTest.cpp:216,218,361`；`:216,306-329` |
| `Route.Settlement.FormationPreservation.*`（3 项） | `GameXXKRouteSettlementFormationTest.cpp:42,107,154`（“enters a legal route”本身失败） |
| `Route.Reward.ChoiceSeed.*`（2 项） | `GameXXKRouteRewardChoiceSeedTest.cpp:70,155` |
| `Route.Relics.*`（2 项） | `GameXXKRelicSystemTest.cpp:543,553,557`；`:519,520` |
| `MVP.RouteMerchant.*`（3 项） | `GameXXKRouteMerchantRulesTest.cpp:1075,1223`；`GameXXKRouteMerchantWidgetTest.cpp:363` |
| `MVP.RouteSettlement.WorkbenchHealthRegion…` | `GameXXKRouteSettlementTest.cpp:540,547,559`（fixture 非法 → `Persistent player resources are invalid.`） |
| `MVP.OwnedQuestNpcLoadouts` | `GameXXKOwnedQuestNpcLoadoutTest.cpp:55,59,60,84,87,89,91` |
| `Integration.CardRoute.CompanionBattleProgression` | `GameXXKCompanionBattleProgressionRewardTest.cpp:110,112,125,127,142,145` |
| `MVP.UI.DeckInteraction.*`（2 项） | `GameXXKFormationDeckInteractionTest.cpp:76`、`:140` |
| `MVP.Companion.Facade.RouteLock` / `RecruitAndRead` | `GameXXKCompanionFacadeTest.cpp:66`、`:44` |

**修法（一刀两用）**：把 A1/A2 的夹具统一改为二选一——(a) 调 `GameXXKPermanentPartyTestFixtures::MakeStartedState()`；或 (b) 保留 `StartGame()` 但在夹具里补 `State.Talents.NodeRanks.Add(TEXT("Talent.Party.NpcSlot"),1)` + `Talent.Party.CompanionSlot`，并把 `Training.PartyProgressionStep` / `NarrativeProgress.TaskProgressById["S00-02"].ObjectiveCounts["Party.FirstNpcDeployed"]` 置为满足 `IsSlotEligible`（`GameXXKPartyFormationRules.cpp:199-208`）。(b) 更贴近“真实中后期存档”。

---

### 3. 用户线索 2 —— Academy map identity：**不是真实代码问题**（1 项，29 条断言）

`Source/GameXXK/Private/Tests/GameXXKAcademyRulesTest.cpp:111` 断言 `State.CurrentMapId==TEXT("AcademyBattle")`，29 课全部失败。

**结论：没有任何一行覆盖 `CurrentMapId`。** `Source/GameXXK/Private/MVP/GameXXKAcademyStateBuilder.cpp:65` 的 `State.CurrentMapId=TEXT("AcademyBattle")` **根本没有执行到**：

```
GameXXKAcademyRulesTest.cpp:109  UGameXXKMVPSubsystem::BuildAcademyBattleState(...)
  → GameXXKMVPSubsystem.cpp:6297   if (!GameXXKAcademyStateBuilder::BuildLoadout(...)) return false;   ← 在这里返回
      → GameXXKAcademyStateBuilder.cpp:14  State = UGameXXKMVPRules::CreateNewGame();  // CurrentMapId = "MainMenu" (GameXXKMVPRules.cpp:1794)
      → GameXXKAcademyStateBuilder.cpp:46  SetQuestNpcForCurrentRun(...) → 失败（A1 门禁）
  → GameXXKAcademyStateBuilder.cpp:56 BuildEncounter 从未被调用 → :65 从未执行
```

决定性计数证据：`index.json` 里该测试 **109 行失败 29 次、110 行 29 次、112 行 29 次**——完全一一对应，即 map identity 是**构建失败的下游症状**，不是独立的第二个 bug。（`GameXXK.MVP.Battle.BoardWidget`、`GameXXKAcademyStateBuilder.cpp:65` 之外全仓库只有测试自己写 `CurrentMapId`。）

**注意**：工作区 21:43 已在 `GameXXKAcademyStateBuilder.cpp:20` 修好（`State.Training.bProgressivePartySlots=false;`），`Saved/Baseline-20260916/academy-b2/` 是之后的验证跑。本报告仍按 21:28 那次运行记录事实。

---

### 4. 用户线索 3 —— 开箱事务（6 项，`FIXTURE-ISSUE`）

`配套普通宝箱缺失` / `配套普通宝箱与课程进度不一致` 的来源是 **`Source/GameXXK/Private/GameXXKTeachingChestRules.cpp:137-147`**（`Validate`）：

```cpp
141:  if(P.bEnabled&&!P.bOpened)Expected.Add(StageName(P.Stage));
144:  for(const auto& Token:S.Training.OwnedChestTokens)
145:      if(!Token.FixedDropId.IsNone()&&(Token.Tier!=EGameXXKTrainingRewardTier::NormalChest||!Expected.Remove(Token.FixedDropId)))
146:          return Fail(Error,TEXT("配套普通宝箱与课程进度不一致"));
147:  if(!Expected.IsEmpty())return Fail(Error,TEXT("配套普通宝箱缺失"));
```

注意第 144-146 行**不看 `P.bEnabled`**：只要存在一个带 `FixedDropId` 的箱子 token 而它不在本阶段 `Expected` 里，就报“不一致”。这是 2026-09-15 教学宝箱批次引入的新不变量。

| 测试 | 决定证据 | 判定 |
|---|---|---|
| `ToolsRedesign.ChestMaterialsInStorage`（12 条） | `Tests/GameXXKToolInteractionRedesignTest.cpp:517` `State.Training.OwnedChestTokens.Reset();` 然后 `:519` 只追加一个**无 `FixedDropId`** 的普通箱 → `Validate` 的 `Expected`（`TeachingChest.Stage.1`）无人满足 → `:529` 保真失败 | `FIXTURE-ISSUE`。修法：夹具里调用既有的 `GameXXKPermanentPartyTestFixtures::SkipTeachingChests(State)`（`GameXXKPermanentPartyTestFixtures.h:13-26`，已被 `GameXXKChestStoredStackTest.cpp:20`、`GameXXKChestReceiptsTest.cpp:19`、`GameXXKSaveGameTest.cpp:55/123/166` 等使用） |
| `Dialogue.SaveMigration.V28` | `Tests/GameXXKDialogueSaveMigrationTest.cpp:16-23`：拿 `StartGame()` 的**现代**状态，只把 `Save.SaveVersion` 改成 27 就当 v27 存档；真实 v27 存档里 `GuideProgress.TeachingChests` 根本不存在（默认 `bEnabled=false`，`Source/GameXXK/Public/GameXXKTeachingChestRules.h`），不会带 `FixedDropId` token | `FIXTURE-ISSUE`（合成了一份现实中不可能存在的旧档） |
| `Narrative.SaveMigration.V29` / `.V29Validation` | 同上（`GameXXKNarrativeGuideSaveMigrationTest.cpp:64` / `:113`），另加 F 组的 36→47 | `FIXTURE-ISSUE` |
| `Route.BattleRetreat.LegacySaveMigration.UniqueParent` / `.AmbiguousParent` | `GameXXKBattleRetreatTest.cpp:274-275`：`MakeSaveState(BattleState)` 后只改 `SaveVersion=22` | `FIXTURE-ISSUE` |

**次要发现（值得单独确认，非本组测试的错）**：`Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp:2263-2264` 只在 `Source.SaveVersion==TeachingChestsIntroducedSaveVersion`(=44) 时调用 `RestoreOrdinaryChestTokens`。v<44 的旧档走通用分支，仅由 `:2293 ValidateRuntimeState`（→ `:2784 TeachingChestRules::Validate`）兜底报错。真实旧档不会带 `FixedDropId`，所以现在不会炸；但若将来迁移要“补发”教学箱，这条路径需要一起改。

---

### 5. 用户线索 4 —— HUD 归一化锚点（1 项 / 33 条断言，`STALE-EXPECTATION`）

**测试**：`Source/GameXXK/Private/Tests/GameXXKBattleProjectedUnitHudTest.cpp`（不是 `AutomationTest.h`；`AutomationTest.h:2199` 只是 `TestEqual(FString&, …)` 转发壳）。

- 断言：helper `AssertFixedHudSlot` 的 **L173（minimum）/ L174（maximum）/ L175-177（HUD seam）**。
- **L157 `ExpectedAnchor.Y += 0.025f;`** 是关键。
- 生产：`Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp:232-272` `TryResolveFixedUnitHudLayout`，**L267 `Anchor.Y += 0.025f;`**，**L268 `OutLayout.Anchors = FAnchors(Anchor.X, Anchor.Y, Anchor.X, Anchor.Y);`**；seam getter 在 `:6227-6232` 返回 `CanvasSlot->GetAnchors().Minimum`。

**期望 vs 实际（从源码 + IEEE754 反推，报告里没有数值）**：

| 单位 | 槽 | 期望 Y（double） | 实际 Y（`FAnchors` 的 float） | ΔY |
|---|---|---|---|---|
| Partner.Blade / Enemy.MoneyRat | 1 | 0.6250000242143869 | **0.625** | +2.42e-08 |
| Player / Enemy.BlackBear | 2 | 0.5449999812990427 | **0.5449999570846558** | +2.42e-08 |
| Npc.TusiChief / Enemy.Tiger | 3 | 0.46499999798834324 | **0.4650000035762787** | −5.59e-09 |

原因：`FAnchors` 的所有构造函数只接受 `float`（`D:\UE_5.8\Engine\Source\Runtime\Slate\Public\Widgets\Layout\Anchors.h:38/44/50-53`），而 `FVector2D` 在 UE5.8 是 `double`。生产先把 `0.60f(double)+0.025f(double)` 算成 0.6250000242143869，再被 `FAnchors` 量化回 0.625；测试的期望值留在 double。X 分量没有加法，float↔double 无损，所以 X 断言全过——这也解释了为什么恰好每个单位 3 条（min/max/seam 同值）而不是更多。

**判定依据**：`Saved/Automation/GameXXK_Full_Probe2`（09-05）锚点错误 = 0；`Saved/Automation/ThreeTables_BattleRevision_20260907`（09-07 00:32）起恰好 33 条，之后 `PartyLeft/*`（09-14）与 `baseline-full2`（09-16）保持不变。引入提交 `aea5194c`（09-07）同时加了生产的 L244 与测试的 L157。

**修法（只改测试）**：把 L157 改成 `ExpectedAnchor = FVector2D((float)ExpectedAnchor.X, (float)(ExpectedAnchor.Y + 0.025f));`，或把 L173/174/175 换成容差比较（先例：`GameXXKBattleBoardWidgetTest.cpp:476-478` 用 `FMath::IsNearlyEqual(..., 0.01f)`）。**不要改生产**——0.625 就是设计值（675 设计像素）。

**同族但不同因（不要一起批）**：
- `GameXXK.MVP.Battle.AnimationLayerWidget` 在 `AutomationTest.h:2190` 的 2 条 —— `UTexture2D*` **指针同一性**（`GameXXKBattleAnimationLayerWidgetTest.cpp:462/465`），属 B 组。
- `GameXXK.MVP.Town.ShellInputInteractionFollower` 在 `:2190` 的多条 —— 旧 3D 城镇壳层的资产引用与**枚举**（`GameXXKTownShellTest.cpp:416/485`），属 J 组。
- `GameXXK.Integration.CardBattle.BoardTargeting` 的 178/202 vs 206/285 —— 见 I 组，是**真实生产漂移**。

---

### 6. 用户线索 5 —— 动画节奏（3 项，`REAL-REGRESSION`）

由提交 **`9b60b0a2`“chore: checkpoint pre-clear shipping build”（2026-09-01）** 引入，且**没有同时改这两个测试文件**：

| 生产位置 | 改动 |
|---|---|
| `Source/GameXXK/Private/UI/GameXXKBattleAnimationPresentation.cpp:705-742` | 删掉 `bFirstHit = Event.HitOrdinal <= 0` 分支，`Duration/Impact` 变成固定的 `0.30/0.10`（设计：首次命中 `0.82/0.30`，后续 `0.30/0.10`） |
| 同上 `:711-712` | 闪避从 `0.45/0.16` 改成 `0.30/0.10` |
| 同上 `:261` | 致命一级 `ShakeDurationSeconds` 从 `0.26f` 改成 `0.20f` |
| 同上 `:330-334` | 新增 `if (Action == Hit \|\| Action == Death) return {};` → Hit/Death 图集被退休 |
| `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp:2899-2914` | 删掉 `DeathEntry.TargetClip = FitClipToDuration(ResolveUnitAnimationClip(…, Death), …)` → 死亡特写退化成播放 **Idle** 图集，且永不被预取 |

**算式的精确复原**：英雄攻击 clip 帧数 60（`GameXXKBattleAnimationPresentation.cpp:149`），`SourceFramesPerSecond = 46.153846`（`:120`），`FitClipToDuration` 在 `:760-788` 算 `FrameCount/(FPS×TargetDuration)`：

- 测试期望 `1.585366` = `60/(46.153846×0.82)`
- 生产实际 `4.333333` = `60/(46.153846×0.30)`
- 帧号 29 vs 期望 10 = `floor(0.15×rate×46.153846)`
- 其余 26↔50、0↔55、3↔1、2.5↔1.3、5.0↔0.0、impact 速率 1.5 = `9/(30×0.20)` 全部逐一吻合

**两个测试的期望值都能在冻结设计文档里找到**：`docs/superpowers/specs/2026-08-12-balance-and-battle-rhythm-tuning-design.md:143-146,149,151,163`（0.82/0.30、0.30/0.10、0.45/0.16、死亡 0.90 s 播完整死亡图集、致命 0.26 s、播放率 = `FrameCount/(FPS×本段时长)`），`:30-31` 明确把“12 fps / 2× / 2.5 s / 5 s”列为**修改前的基线问题**。

**子块分类**：

| 子族 | 判定 |
|---|---|
| 时长拟合（0.82/0.30）、闪避 0.45/0.16、致命 0.26 s、Death clip 退休 | **`REAL-REGRESSION`** |
| Death epoch / 死亡图集请求 / Death 帧号 0 vs 6 / ±0.52 s 错位 | `REAL-REGRESSION` 的**级联**（死亡提前 0.52 s 结束） |
| shake/读数/五连击（`GameXXKBattleAnimationLayerWidgetTest.cpp:442-779`、`:1326-1411`） | 上述的**级联**（采样越过 0.30 s 边界；读数在条目完成时被清空，`GameXXKBattleBoardWidget.cpp:2860-2864`） |
| `12 fps` / `2×` / 死亡 5 s / 帧号 26、0、3 / `_attack_punch_atlas` 路径 | **`STALE-EXPECTATION`（有条件）**：`9b60b0a2` 同时新增了 `T_character_00_hero_2k_attack_punch/kick_atlas` 与实测 fps 表 `CorrectedClipTimings`（`:87-165`）。要么承认新资产模型并改测试，要么回退——**这是一个需要人决策的岔路口** |
| `GameXXK.MVP.Battle.AnimationPresentation` 的 “Hit playback fits…” 断言**没有出现在失败列表** | 因为它拿 `ResolveClip` 返回的空描述符（NSDMI `PlaybackRate=1.0f`，`Public/UI/GameXXKBattleAnimationPresentation.h:25`）恰好等于 Idle 回退速率而“错误地通过”。**不要把它当成有效覆盖** |

---

### 7. D 组 —— `气血 / 内力` 本地化表数据缺陷（1 项，`REAL-REGRESSION`）

生产：`Source/GameXXK/Private/UI/GameXXKBattleUnitResourceWidget.cpp:371`
```cpp
HealthText->SetText(GameXXKLocalization::Source(FString::Printf(TEXT("气血 %d / %d"), CurrentHP, MaxHP)));
```
（兄弟行 `:381` 是 `内力 %d / %d`）

数据：**`Content/Localization/GameXXK/strings.json:4128-4147`** 两条 `Legacy.*` 条目的 **`zh-Hans` 列本身就是英文**：
```json
{ "key": "Legacy.f18bd547c99ce4c1", "nativePattern": "气血 %d / %d",
  "zh-Hans": "HP {0} / {1}", "en": "HP {0} / {1}", "nativeSource": "气血 {0} / {1}" },
{ "key": "Legacy.dced2b0761834f10", "nativePattern": "内力 %d / %d",
  "zh-Hans": "MP {0} / {1}", "en": "MP {0} / {1}", "nativeSource": "内力 {0} / {1}" }
```
由提交 `9c17e499`（2026-09-11）加入；在此之前 `气血 %d / %d` 不在表里，`ResolveDisplay`（`Source/GameXXK/Private/UI/GameXXKLocalization.cpp:140-197`）直接回退返回中文，所以测试是绿的。全表 4512 条中**只有这 2 条**中文源配了非中文 `zh-Hans`。语言偏好文件是 `zh-Hans`，所以不是语言环境问题。

影响：`GameXXK.MVP.Battle.AnimationLayerWidget`（`:504`、`:534`）+ `GameXXK.Integration.CardBattle.BoardPresentationGate`（断言 `GameXXKBattleBoardWidgetTest.cpp:2374`）。`内力/MP` 无失败测试但同一缺陷。

**附带发现（真实但本报告未列入失败项）**：`GameXXKLocalization::Source` 的 `DisplayCache`（`GameXXKLocalization.cpp:146/163/178/191`）以中文源为 key 缓存了**按语言渲染后的** FText，而 `SetLanguage`（`:319-334`）从不清理它（只有 `Initialize():279` 清）。玩家在设置里切语言后，已缓存过的格式化串会保持旧语言。本次运行的失败不是它造成的（catalogue 数据才是），但建议一并修。

---

### 8. E–H 组（机械批）

**F 组——存档 schema 硬编码 36（4 项，`STALE-EXPECTATION`）**
`GameXXKSaveMigration.h:73` `EnemyPhaseAndTrainingFormationIntroducedSaveVersion = 36`，而 `:92` `FirstBattleGuidanceIntroducedSaveVersion = 47` 才是 `CurrentSaveVersion`。
`SaveMigration.CombatScalingFoundationV33`（`GameXXKCombatScalingSaveMigrationTest.cpp:63,152,155`）、`SaveMigration.RetiredRouteCardsV34`（`GameXXKRetiredRouteCardMigrationTest.cpp:194,207`）、`Prologue.Aftermath.TutorialMapItem`（`GameXXKTutorialMapItemTest.cpp:25,154`）、`Prologue.RetiredLegacyNarrative`（`GameXXKRetiredLegacyNarrativeTest.cpp:23,64`）。修法：断言改比 `FGameXXKSaveMigration::CurrentSaveVersion`。

**G 组——天赋图新增 2 节点（4 项，`STALE-EXPECTATION`）**
`Source/GameXXK/Private/GameXXKTalentCatalog.cpp:440`（`Talent.Party.CompanionSlot` “结伴同行”）与 `:444`（`Talent.Party.NpcSlot` “知己同行”）是 09-15 批次新增的，所以揭示数各 +1：
- `Talents.Facade.AuthoritativePurchaseAndViews`：`GameXXKTalentFacadeTest.cpp:29`（1→2）、`:43`（5→6）
- `Talents.Widget.GraphIconsSelectionAndPurchase`：`GameXXKTalentTreeWidgetTest.cpp:35`（1→2）、`:103`（5→6）、`:137`（8→9）
- `Talents.Widget.CrossPanelToolBonusRefresh`：`GameXXKTalentTreeWidgetTest.cpp:204`（图标路径）
- `Localization.Surfaces.FullTalentNamesAndEffects`：`GameXXKLocalizationSurfaceRegressionTest.cpp:142` 报 `Talent effect Talent.Party.CompanionSlot` / `Talent.Party.NpcSlot` 未翻译 → **新节点的 `DescribeEffect` 缺英文词条**，是真实但很小的缺口（修词条即可）

**H 组——MetaShop 购买被拒（2 项，`FIXTURE-ISSUE`，推断）**
`GameXXKMetaShopRulesTest.cpp:176` 用局部夹具 `MakeMinimumValidState()`（`:18-42`）= `Subsystem->StartGame()`，然后 `:30-39` 只保留 `ActivePermanentCompanionInstanceId` 指向的同伴 + 一个替补。渐进档下该 ID 为 **None**（A2 症状），于是夹具留下一个“有同伴但无激活同伴”的非法状态；`PreviewPurchase` 在 `GameXXKMetaShopRules.cpp:190` 调 `FGameXXKSaveMigration::ValidateRuntimeState` 失败 → `:192 InvalidRuntimeState` → 后续 58 条（装备包）/11 条（同伴包）断言全崩。仓库容量 200（`GameXXKEquipmentRules.h:202`）无关。**推断依据充分但未跑测**，修 A 组后应复测。

**C/P/V 组混合项**
- P：`RouteTravelMoney.OrdinaryChestMaterialPool`（`GameXXKTravelMoneyLifecycleTest.cpp:115,133,134`）：断言桶概率 0.5/0.3/0.0667，实际 1.0/0/0 —— 09-11 `3f916ecd feat: revise chest loot table and tool combine curve` 改过表 → `STALE-EXPECTATION`。
- V：`MVP.UI.MainMenuPlayerFlow.SaveMigration` 的 `GameXXKMainMenuPlayerFlowTest.cpp:299`（期望 `L_Qingshan_AsianVillage_Demo`，实际 `L_DesktopTrainingHUD`）→ **`STALE-EXPECTATION`**（AGENTS.md 已把 `L_DesktopTrainingHUD` 定为默认面）；同测试的槽位种子失败（`:159,160,257-267`）需排查存档写入。`MVP.PIE.PlayableRoot*` 的多数断言是 A 组下游。

---

## 3. 推荐修复顺序

### 第 1 刀（机械、零风险、收益最大）：**A1 + A2 + H，共 74 项失败测试**

这是**解锁最多的一项**（74 / 170 = 44%），而且修的是夹具不是产品。统一做法：所有需要满编队的测试改用 `GameXXKPermanentPartyTestFixtures::MakeStartedState()`，或保留 `StartGame()` 但补上 `Talent.Party.CompanionSlot` / `Talent.Party.NpcSlot` 与 `S00-02` 的 `Party.FirstNpcDeployed`。同一批顺手更新 `GameXXKMVPSubsystem.cpp:5147-5150` 的过时注释。

连带收益：`GameXXK.Academy.*`（3 项，含 29 条 map identity）会**自动变绿**——那 29 条不需要任何单独改动（用户线索 2 的答案是“没有覆盖行”）。工作区 21:43 已修 Academy 那一支。

### 第 2 刀（机械、可在 1 次编辑内完成）：C + D + E + F + G + P + V（部分）

- **C**（1 项 / 33 条）：`GameXXKBattleProjectedUnitHudTest.cpp:157` 一行。
- **D**（1 项）：`Content/Localization/GameXXK/strings.json` 两条 `Legacy.*` 的 `zh-Hans` 改回 `气血 {0} / {1}` / `内力 {0} / {1}`（建议用 `scripts/export_localization_master.py` 重新生成，避免手改被覆盖）。
- **E**（6 项）：迁移测试的合成旧档补 `SkipTeachingChests`，或让夹具真正构造 v27/v22 时代的默认 `TeachingChests`。
- **F**（4 项）：断言改用 `CurrentSaveVersion`。
- **G**（4 项）：计数 +1；另外补 2 条天赋效果英文词条。
- **P**（1 项）、**V** 的默认地图断言（1 条）。

合计约 **18 项测试**，全是“更新期望值/数据/夹具”，不改产品行为。

### 第 3 刀（需要小心，真实回归）：**B（3 项 / 121 条断言）**

按冻结设计文档恢复：`GameXXKBattleAnimationPresentation.cpp:705-742` 的 `HitOrdinal` 分支（0.82/0.30 与 0.30/0.10）、`:711-712` 的 0.45/0.16、`:261` 的 0.26；以及 `GameXXKBattleBoardWidget.cpp:2899-2914` 的 `DeathEntry.TargetClip` 与 `:330-334` 的 Hit/Death 早退。**然后必须在“保留 09-01 的 punch/kick + 实测 fps 资产模型”与“回到 `_attack_atlas` + 12 fps + 2×”之间做一次显式决策**，并据此同步 `GameXXKBattleAnimationPresentationTest.cpp:242-303,348-353`。这一组单点断言数最多（110+11+7），也是唯一有“用户可见后果”的（死亡特写现在播 Idle 图集）。

### 第 4 刀（真实回归、范围小）：N（卡牌结算 4 项）、O（营地 2 项）、I 中的尺寸漂移（1 项）

- N：`Data.CardOutcomePreview.Audit.RelicLinkedDamage`（`GameXXKCardOutcomeAuditTest.cpp:1439,1441`，Drum 联动伤害 3 包变 0）、`Rules.DotToxicMedicineRelic`（`GameXXKCardOutcomePreviewTest.cpp:813,821`）、`Rules.GroupPositions`（`:604,606`）、`Integration.MarkCardCompatibility`（`GameXXKMarkCardCompatibilityTest.cpp:181,183`，流血 8→7 期望却得到 9）。**注意这 3 项在 09-12 就是红的**，属更早的窗口。
- O：`GameXXKRouteEncounterPanelTest.cpp:739` 与 `:1366`（营地应回 30% 血：期望 63、实际 33 = 完全没治），同时 `:745`/`:1371` 断言“已退休护符不再发放”却失败 → 看着像**营地结算只迁移了一半**，需要一次产品确认。
- I 的尺寸漂移：`GameXXKBattleBoardWidget.cpp:7253-7255` 用的 `EnemyIntentCardSize(206,285)`（`:188`）已等于 `PlayerHandCardSize`（`:123`），而 `GameXXKBattleBoardWidgetTest.cpp:2589/2591` 断言 178/202 的“compact”意图。历史：150×171（07-28）→ 178×202（09-07 `aea5194c`）→ 206×285（09-10 `e6619fc1`）。**是真实漂移，但“206×285 是否有意”需要人决策。**

### 第 5 刀（先排查再决定）：J、K、L、M、Q、R、S、T、U、W

这些请按“需要额外证据”处理（见第 4 节）。其中最值得优先的不是 bug，而是 **J 的 3 个数值**：`GameXXKTownShellTest.cpp:347`（HD2D roll 期望 −30、实际 0）、`:349`（scale X 期望 +1、实际 −1）、`:389`（相机 pitch 期望 −30、实际 −20）。横版翻页书与镜像（`:349-351,409-412`）是 09-14 “cast-right-facing / town-horizontal” 美术批次的**有意**改动，但 `−30 / −30` 这一类数值在 `AGENTS.md` 里被列为**用户手调、不得擅自回退**（相机变换 / HD2D 平面值）。翻转角色朝向时是否该连带改相机俯角与 roll，必须由人确认后再动。

---

## 4. 无法分类 / 需要额外证据

| 项 | 现状 | 需要什么证据 |
|---|---|---|
| **K 组 6 项**（`Interaction.Router.NpcNarrative*`） | `GetNarrativeSequenceId()` 全解析成 `None`；生产入口是 `Source/GameXXK/Private/Interaction/GameXXKInteractableComponent.cpp:12-46` `DefaultSequenceIdForCharacter`。测试用 `StartGame()`，且断言的是 3D 城镇的 NPC 资产（`GameXXKInteractionRouterTest.cpp:35/92/149/222/262`） | 需要一次单测跑 + `LogTemp` 打印 `DefaultSequenceIdForCharacter(CharacterId)` 的入参与出参；也可能是 A 组 NPC 门禁的下游（S00-02 未完成时 NPC 不可用）。**未验证** |
| **L 组 2 项**（`MVP.Codex.*`） | 写入/回读 codex 状态全失败（`GameXXKCompanionCodexPersistenceTest.cpp:39-42,110-119`、`GameXXKCompanionCodexRulesTest.cpp:146-170`） | 需要确认 codex 是否被新存档版本（v47）的新字段影响；报告只有布尔断言，无字段名 |
| **I 组剩余 12 项**（Board* 卡牌槽/奖励/自动战斗/意图） | 8 项在 09-12 就红、2 项 09-07 就红。`BoardAutoPlayToggle` 的 `自动战斗：关 → 自动` 有**通过的同族证据**：`GameXXKLocalizationSurfaceRegressionTest.cpp:34-35` 断言 `Compact(Source("自动战斗：关"))=="Auto"` 且通过 → 说明紧凑标签是有意的 → 该 2 条是 `STALE-EXPECTATION` | Boss 卡槽 5 项（`GameXXKRouteRewardEntryAcquisitionTest.cpp:136,164,197,270,314`）需要确认“boss 奖励选项”是否被 09-13~16 的奖励门槛改动有意移除 |
| **M 组 7 项** | `StatusEffectsWidget` 与 `SceneActors` 的 tooltip 文案：测试期望“层数：/ 受到直接攻击后…”（08-11 `aba2a2eb`），生产 `GameXXKCardPillText.cpp:42` 是“数值：/ 被攻击命中后…”（**09-03 `f31ca578`，更新**），且 `02917d62` 没动过这两侧 → **早于本窗口的既有失败**（与 09-12 基线一致：`BoardPresentationGate` 等当时已红）。`OverlayCoordinator` 的 1434→1384（50 px）与 1169→1168（1 px）是布局常量，属 09-14 party-left 布局改动 | 需要产品确认最终文案（“层数/数值”与“所属阵营回合/本方回合”哪一版为准）；布局常量需要 09-14 布局批次的验收记录 |
| **Q 组 3 项** | `CombatSimulationPolicyTest.cpp:140` 报新不变量 `Living enemy intents require unique supported enemy presentation slots.`（`Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp:821`），`:157` 选不出 setup 卡；`FormationMasterTargeting*` 报 `Automatic card targeting cannot accept a submitted stable target ID.` | 需要确认这是**新的合法不变量**（→ 夹具应提供唯一合法槽位）还是**过严的校验**（→ 生产 bug） |
| **R 组 3 项** | `Inventory.EnhancementAndStorage` 23 条（强化石 10→11、分解少给材料、`SaveGame` 回读强化等级 −1）；`Equipment.SaveMigration.InventoryLocksV25`；`MVP.SaveGame.RouteEconomyV9.Migration:232` | 强化/分解链路 09-16 “starter equipment gate”批次动过 `EquipmentEconomyRules::SynchronizeRuntimeMirrors`；需要一次单测 + 打印 `EnhancementMaterial` 与 `WarehouseItems` 的实际值。`RouteEconomyV9` 的断言是“除 3 个字段外完整保留”，很可能只是**新增字段未被排除**（→ `STALE-EXPECTATION`），但需要看到具体差异字段名 |
| **S 组 18 项** | 已确认下游：`TravelPartyAtlasAsyncFallback:5187`、`TravelCombatPresentation:5866`、`TravelRuntimeLevelUpSync`（“有 active Blade 吗”）= A 组。未确认：`ParentCloseStack:4568/4593/4701`（Tab 箭头 `▼/▲` 得空串）、`NoticeRailStateMachine:2946`、`IdleStripControlRailAndRetryStates:2711`、`TravelVisualLoop:5838/5840`、`FormationNpcPortraitPermanent:6297-6301`、`CharacterRoster.*:6613/7033/7274`（“owns six NPCs → 1”）、`BackpackAttributes.*`（13 → 8）、`BackpackPicker.*` | “six NPCs → 1”与“13 → 8”强烈提示是 A 组（六个 NPC 在渐进档下不再全部拥有），但**未逐条验证**；箭头空串与 `GetVisibility` 需要看 `GameXXKDesktopTrainingWorkbenchWidget.cpp` 的 Tab 构建代码 |
| **T 组 6 项** | `RosterUsesShortContextualEnglish:144`（六个角色标签有一个缺失）、`HeroDeckEditor:503/545`（八张上限文案）、`FinalBackpackPagingAndActiveSelection:137`（4 → 6）、`DuplicateRecruitmentFeedback:807`、`LayoutAndProfile:65`、`TaskNpcFixedDeckReadOnly`（**没有任何 error 就被判 Fail**） | 最后一项需要看 `index.json` 该测试的 `entries` 为空但 `state=Fail` 的原因（可能是 `AddError` 之外的通路，或测试目标缺失） |
| **U 组 5 项** | `CompanionLockedCardPresentation:449`（18 → 36 张）、`:462/465/476`；`DeployedTrioExperience:170/171/211/226`；`EquipmentCharacterLevelGate:549/587`；`TravelRuntimeLevelUpSync:261`；`Training.Economy.Gold105AndFortyFiveDayTalents:47`（`calibration cannot buy Talent.Party.NpcSlot: 进入1-3阶段，在主线拾回卷轴并认识幽白后可解锁`） | 最后一项文案直接证明 **NPC 槽天赋有剧情前置**，是**有意设计** → `STALE-EXPECTATION`（校准脚本不该买这个天赋）；前三项的 18→36 与“resolvable → null”疑似 A 组下游，需复用第 1 刀后复测 |
| **W 组 3 项** | `Academy.IndependentEncounter`（无 error 却 Fail）、`Data.CardDocumentation:961/962`（**09-12 已红**）、`Development.AuthoredBattleAndSimulation:98`（Hell 150% → 100%，**09-12 已红**） | 前两项需要 `entries` 为空的原因；`AuthoredBattleAndSimulation` 需要确认 dev workbench 是否该套用难度倍率 |

**方法学限制（请一并知悉）**：
1. 本报告的数值型“期望 vs 实际”全部来自 `index.json` 的消息文本 + 源码公式反推。**本 build 的 `TestEqual` 对无 `ToString` 的类型只输出 “The two values are not equal.”**（`D:\UE_5.8\Engine\Source\Runtime\Core\Public\Misc\AutomationTest.h:2157-2166`），所以 C 组（锚点）与部分 J/K/I 项的**具体数值无法从报告读出**——C 组的 Δ 值是反推结果，已在第 5 节标明。
2. 09-12 基线只覆盖 9 个前缀（682/1258 项），**不能**用它证明 `MVP.Battle.* / Presentation.* / UI.* / Interaction.*` 当时是绿的。
3. H、S、T、U 组中标注“A 组下游”的条目是**基于夹具代码的推断**，未单独跑测验证；建议第 1 刀后先复跑这 4 组再决定是否还需要单独修。
4. 本报告没有修改任何文件，也没有修复任何问题。
