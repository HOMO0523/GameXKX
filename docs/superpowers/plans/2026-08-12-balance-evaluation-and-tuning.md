# GameXXK 数值评估与自动调优实施计划

> **执行要求：** 使用 `executing-plans` 逐任务实施；每个生产改动先使用 `test-driven-development` 建立 RED，提交前使用 `verification-before-completion`。禁止使用 worktree、UnrealBridge、Live Coding 或 Hot Reload。

**目标：** 把现有混合 cohort、偏即时收益的 2,400 场诊断升级为可追溯的 schema v3 与正交对照系统，并依据可信数据分批调优卡牌、NPC、套装和敌人，使不同流派体验不同但强度处于各成长档目标区间。

**架构：** 战斗模拟仍只通过真实 `FGameXXKCardBattleAdapter` 变更状态；C++ 提供确定性策略、原始指标和两套矩阵，Python 只聚合、比较、计算区间并提出有界候选。生产数值永远由明确的 TDD 改动提交，运行时不自动改难度。

**技术栈：** UE 5.8 C++、Unreal Automation、Python 3 `unittest`/CSV/JSON、UBT、Git main。

**权威规格：** `docs/superpowers/specs/2026-08-12-balance-and-battle-rhythm-tuning-design.md`

---

## Task 1：锁定 setup-aware 决策策略

**文件：**

- 新增：`Source/GameXXK/Private/Tests/GameXXKCombatSimulationPolicyTest.cpp`
- 修改：`Source/GameXXK/Public/GameXXKCombatSimulationRules.h`
- 修改：`Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp`
- 邻接：`Source/GameXXK/Private/Tests/GameXXKCombatSimulationFoundationTest.cpp`

### 1.1 写失败测试

为策略增加只在 `WITH_DEV_AUTOMATION_TESTS` 暴露的稳定决策 seam，测试直接输入真实 battle runtime，不复制卡牌规则。至少建立六个小型谜题：

1. 气力不足时选择能抽牌并回气的资源牌，而不是直接结束回合。
2. 守卫面对下一击致死时选择护甲/格挡，再选择护甲转伤。
3. 药师先建立药效/药方，再使用消耗药效的治疗/对敌结算。
4. 弓手先获得蓄力，再让重箭牌消费蓄力。
5. 法师在五牌任务尚未完成时选择未主动打出的来源牌。
6. 阵师在当前地形收益不足时选择换场，并使下一张收益牌优于无关即时小伤害。

每题同时断言：稳定 CardInstanceId、稳定目标、相同输入重复决定完全一致。

### 1.2 亲眼确认 RED

运行冷 UBT：

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload -NoHotReloadFromIDE
```

然后只运行：

```text
GameXXK.Simulation.Policy
```

RED 必须来自旧评分结束回合或选择即时小收益，不能来自非法夹具、卡牌目标失败或构建错误。

### 1.3 最小实现

在 `GameXXKCombatSimulationRules.cpp` 中拆出纯评分组件：

- 有效伤害、击杀、胜利。
- 我方有效治疗、护甲、非致死失血与死亡。
- 新进入手牌、气力/内力净变化。
- 正负状态按阵营估值。
- 地形变化后的手牌条件收益。
- modifier、任务和自动队列的保守价值。

明确的权重集中在一个结构/函数中，禁止散落 magic number。保持稳定排序：分数 → AcquisitionOrdinal → TargetUnitId。暂不增加无界 2-ply。

### 1.4 GREEN 与回归

运行：

```text
GameXXK.Simulation.Policy
GameXXK.Simulation.Foundation
GameXXK.Data.CardBattleRuntime
```

要求全部 0 failed/0 error，相同种子序列化完全一致。

### 1.5 提交

```powershell
git add Source/GameXXK/Private/Tests/GameXXKCombatSimulationPolicyTest.cpp Source/GameXXK/Public/GameXXKCombatSimulationRules.h Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp Source/GameXXK/Private/Tests/GameXXKCombatSimulationFoundationTest.cpp
git commit -m "test: lock setup-aware simulation policy"
git push origin main
```

---

## Task 2：观测 schema v3 与逐卡指标

**文件：**

- 修改：`Source/GameXXK/Public/GameXXKCombatSimulationTypes.h`
- 修改：`Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp`
- 修改：`Source/GameXXK/Private/Tests/GameXXKCardBalanceObservationTest.cpp`
- 修改：`Source/GameXXK/Private/Tests/GameXXKCombatSimulationFoundationTest.cpp`
- 修改：`scripts/run_card_balance_observation.py`
- 修改：`scripts/test_card_balance_observation.py`

### 2.1 Python 与 C++ RED

先把 Python 测试期望升级到 schema 3，并加入以下失败用例：

- 缺失伙伴身份、terrain、逐卡字段时拒绝 CSV。
- metric map 重复键、非法数值、未知 schema 时拒绝。
- seen/played 汇总与出牌率正确。
- overkill/overhealing、回合末未用资源正确求和。

在 C++ Foundation 测试加入一场固定 battle，断言：

- 初始手牌和新抽牌进入 `CardsSeenById`。
- 主动打牌进入 `CardsPlayedById`，自动重放不冒充主动打牌。
- 伤害/治疗/护甲归因到 CardId。
- `RequestedHealing - EffectiveHealing` 进入 overhealing。
- 最终有效伤害以上的剩余伤害进入 overkill。
- 主动结束玩家阶段时记录未用气力与活着成员未用内力。

### 2.2 RED 命令

```powershell
python -B -m unittest scripts.test_card_balance_observation -q
```

冷 UBT 后运行：

```text
GameXXK.Simulation.Foundation
GameXXK.Diagnostics.CardBalanceObservation
```

### 2.3 实现指标

扩展 `FGameXXKSimulationMetrics`：

- 身份：模板、角色、主流派、出生牌、选择牌、地形。
- map：seen、played、damage/healing/armor by CardId。
- total：unused energy/mana、overkill、overhealing。

记录规则：

- 比较动作前后 Hand 的 InstanceId，只为新进入者增加 seen。
- `PlayCard` 成功后增加 played。
- 使用 `FGameXXKCardPlayResult` 的 DamageResults/HealingResults 做精确过量归因。
- 初始场景从 active permanent companion 读取身份，并用 `BuildPersonalCardPool(..., OutPrimaryArchetypeId)` 复算主流派；复算失败则整个诊断失败，不静默写空。

升级 CSV 头、解析器、聚合 JSON 和 Markdown 摘要；保留 schema 2 读取仅用于显式 `--legacy-read`，当前运行默认拒绝旧 schema。

### 2.4 GREEN

```powershell
python -B -m unittest scripts.test_card_balance_observation -q
```

冷 UBT 后：

```text
GameXXK.Simulation.Foundation
GameXXK.Diagnostics.CardBalanceObservation
```

核对输出 2,400 行、198 CardId 审计仍通过、`schema_version=3`、身份字段非空。

### 2.5 提交

```powershell
git add Source/GameXXK/Public/GameXXKCombatSimulationTypes.h Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp Source/GameXXK/Private/Tests/GameXXKCardBalanceObservationTest.cpp Source/GameXXK/Private/Tests/GameXXKCombatSimulationFoundationTest.cpp scripts/run_card_balance_observation.py scripts/test_card_balance_observation.py
git commit -m "feat: add traceable balance observation v3"
git push origin main
```

---

## Task 3：新增正交控制矩阵

**文件：**

- 修改：`Source/GameXXK/Public/GameXXKRouteBalanceTypes.h`
- 修改：`Source/GameXXK/Public/GameXXKRouteBalanceRules.h`
- 修改：`Source/GameXXK/Private/GameXXKRouteBalanceRules.cpp`
- 新增：`Source/GameXXK/Private/Tests/GameXXKOrthogonalBalanceObservationTest.cpp`
- 修改：`scripts/run_card_balance_observation.py`
- 修改：`scripts/test_card_balance_observation.py`

### 3.1 RED

新增 `GameXXK.Diagnostics.OrthogonalBalanceObservation`，测试要求总计 2,520 个唯一 case：

- 职业：6×3 节点×30 种子 = 540。
- 套装：无套装+6 套×3×30 = 630。
- NPC：6×3×30 = 540。
- 地形：6×3×30 = 540。
- 成长档：3×3×30 = 270。

每个 case 带 `dimension`、`variant`、显式 CompanionTemplateId/CardSeed/Terrain。测试逐组比较除目标变量之外的字段完全一致，并验证相同 case 重复序列化一致。

### 3.2 最小生产 seam

为 `FGameXXKRouteBalanceCase` 增加只供诊断的可选字段：

- `CompanionTemplateId`
- `CompanionCardSeed`
- `Terrain`
- `DimensionId`
- `VariantId`

`BuildScenario` 在字段为空时保持现有 locked matrix 行为；字段非空时用真实 `RecruitPermanentCompanion` 构建指定职业。现有 `MakeLockedFullMatrix()` 展开结果和 2,400 场哈希语义不得改变。

### 3.3 GREEN

冷 UBT 后运行：

```text
GameXXK.Diagnostics.OrthogonalBalanceObservation
GameXXK.RouteBalance.FullMatrixExecution
GameXXK.RouteBalance.AuthoredProfilePolicy
```

核对正交 2,520 场与锁定 2,400 场均 0 failed、0 stalemate、0 stranded。

### 3.4 提交

```powershell
git add Source/GameXXK/Public/GameXXKRouteBalanceTypes.h Source/GameXXK/Public/GameXXKRouteBalanceRules.h Source/GameXXK/Private/GameXXKRouteBalanceRules.cpp Source/GameXXK/Private/Tests/GameXXKOrthogonalBalanceObservationTest.cpp scripts/run_card_balance_observation.py scripts/test_card_balance_observation.py
git commit -m "feat: add orthogonal balance diagnostics"
git push origin main
```

---

## Task 4：只读自动候选生成器

**文件：**

- 新增：`scripts/propose_card_balance_tuning.py`
- 新增：`scripts/test_propose_card_balance_tuning.py`
- 新增：`docs/production/2026-08-12-balance-tuning-ledger.md`

### 4.1 RED 单元测试

覆盖：

- 两个输入运行 case 身份或 SHA 不一致时拒绝。
- Wilson 95% 区间边界正确。
- 按成长档/节点应用正确目标区间。
- 逐卡 seen=0、played=0、低出牌率、高贡献与低贡献分别分类。
- 单次建议不超过规格的攻击±15pp、固定值±2/15%、气力±1、内力±3、敌方属性±10%。
- 输出不包含任何写 C++/修改目录的动作。

### 4.2 实现与 GREEN

命令：

```powershell
python -B -m unittest scripts.test_propose_card_balance_tuning -q
```

生成器输出 `proposal.json` 与 `proposal.md`，每条候选包含：证据切片、异常方向、建议范围、需要新增的固定种子 A/B 测试、受影响系统和禁止同时修改的变量。

### 4.3 提交

```powershell
git add scripts/propose_card_balance_tuning.py scripts/test_propose_card_balance_tuning.py docs/production/2026-08-12-balance-tuning-ledger.md
git commit -m "feat: generate bounded balance tuning proposals"
git push origin main
```

---

## Task 5：建立可信基线

**文件：**

- 生成证据：`Saved/BalanceObservation/*`（不提交原始大文件）
- 修改：`docs/production/2026-08-12-balance-tuning-ledger.md`

### 5.1 连续运行两次

分别运行 locked 与 orthogonal 两套 schema v3，使用不同 run id；第二次必须在同一二进制、同一 HEAD 下执行。

```powershell
python scripts/run_card_balance_observation.py --run-id tuned-baseline-a
python scripts/run_card_balance_observation.py --run-id tuned-baseline-b
```

正交测试通过脚本对应参数运行；若脚本尚未暴露参数，在 Task 3 中补 `--matrix locked|orthogonal|both`。

### 5.2 门禁

- 两次 cases CSV 和聚合 JSON 哈希一致。
- locked=2,400、orthogonal=2,520。
- 0 error、0 stalemate、0 stranded。
- 六职业 setup 代表牌实际 played>0。
- 记录每个成长档、职业、流派、套装、NPC、地形的胜率/中位回合/剩余血量和逐卡数据。

### 5.3 候选报告

```powershell
python scripts/propose_card_balance_tuning.py --first <run-a> --second <run-b> --output Saved/BalanceObservation/tuned-baseline-proposal
```

只把摘要和哈希写入 ledger，不提交 Saved 原始报告。

---

## Task 6：按归因簇持续调优

本任务循环执行，直到保护线满足或证据显示目标区间需要用户重新定义。每次只处理一个归因簇。

### 6.1 选择顺序

1. 决策器仍不使用的牌：先修策略/夹具，不改生产数值。
2. 同成长档职业或主流派极差 >20pp：优先改单卡预算/循环资源。
3. 固定职业后套装极差 >20pp：再改套装 2/4/6 件数值。
4. 固定职业/套装后 NPC 极差 >20pp：再改 NPC。
5. 所有玩家变量收敛后，才改敌人 HP/攻击/防御或意图频率。

### 6.2 每轮 TDD 模板

1. 在对应 Catalog/Runtime 测试增加固定 CardId 数值 RED。
2. 新增同种子 A/B 测试，只替换该参数并断言方向、终局和无规则回归。
3. 冷 UBT，运行精确 RED。
4. 最小改生产目录/规则数据。
5. 跑职业/NPC/套装精确 GREEN、`GameXXK.Data.Card`、Simulation Policy。
6. 跑受影响的正交切片；效果方向错误则回到步骤 1，不叠加第二个变量。
7. 跑 locked 2,400，确认 0 stalemate/stranded。
8. 更新 ledger：旧值、新值、证据、风险、是否保留。

### 6.3 提交模板

```powershell
git add <exact test files> <exact production files> docs/production/2026-08-12-balance-tuning-ledger.md
git commit -m "balance: tune <single attribution cluster>"
git push origin main
```

重大调值前建立 annotated tag：

```powershell
git tag -a backup/pre-balance-iteration-<n>-20260812 -m "Before balance iteration <n>"
git push origin backup/pre-balance-iteration-<n>-20260812
```

---

## Task 7：最终数值认证

### 7.1 全验证

1. 冷 UBT。
2. `GameXXK.Simulation`。
3. `GameXXK.Diagnostics.CardBalanceObservation` 与 Orthogonal。
4. `GameXXK.RouteBalance`。
5. `GameXXK.Data.Card`、六职业伙伴、NPC、六套装。
6. `Automation RunTests GameXXK` 全前缀。
7. Python 三组单元测试。
8. `scripts/harness_state_validator.py --require-units --json`。

### 7.2 最终数据

同 HEAD 连续跑两次 locked+orthogonal，要求哈希一致；把目标区间、未解决异常、极端种子和卡牌出牌率写入：

- `docs/design/2026-08-08-card-balance-analysis.md`
- `docs/design/2026-08-11-gamexxk-project-plan/06-enemies-route-and-balance.md`
- `docs/design/2026-08-11-gamexxk-project-plan/10-implementation-testing-and-change-log.md`
- `docs/production/2026-08-12-balance-tuning-ledger.md`

不满足的区间必须标为未决，不得用总胜率或目录名伪装完成。

