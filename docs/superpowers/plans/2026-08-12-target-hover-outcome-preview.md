# GameXXK Target Hover Outcome Preview Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不移动任何已确认战斗 UI 的前提下，让手动单位牌悬停合法目标时显示该目标的真实伤害、毒爆、药效、治疗或护甲；让纯敌方群攻牌悬停手牌时按 1P→2P→3P 显示每个现存怪物自己的实际承伤，并保证预演与随后权威出牌完全同源、198 张卡无未分类项。

**Architecture:** 规则层先把一次出牌产生的伤害种类、药效/遗物来源、治疗尝试和护甲尝试补成完整审计；新的 `FGameXXKCardOutcomePreviewRules` 复制完整 `FGameXXKRuntimeState`，在副本上调用现有 `FGameXXKCardBattleAdapter::ResolveCardPlay`，再只从真实结果包聚合展示模型。Board 只负责在单位 Hover 或手牌 Hover 时调用该只读入口、缓存同一状态的结果，并把结构化文本段交给输入穿透的程序化 Widget；真实点击仍重新验证并沿现有结算/演出链提交。

**Tech Stack:** Unreal Engine 5.8、C++ USTRUCT/UMG、GameXXK CardRules/CardBattleAdapter/RelicRules、UE Automation Framework、UBT、`UnrealEditor-Cmd.exe`、项目 UE MCP 脚本、Python `unittest`、Win32 PIE 输入与截图探针。

---

## 0. 基线、文件结构与不可变边界

实现从 `main` 的规格提交 `5349ab3aad8254e4aeb894400c5fcbe744174e89` 开始。禁止创建 worktree，禁止 UnrealBridge，禁止 Live Coding/Hot Reload，禁止暂存共享工作区内的未跟踪美术、角色、背景、PSD、测试输出或无关脚本。

现有文件职责：

| 文件 | 当前职责 | 本功能中的变化 |
|---|---|---|
| `Source/GameXXK/Public/GameXXKCardTypes.h` | 卡牌伤害、治疗、出牌结果协议 | 追加伤害来源，补 `DamageResult.Kind`，新增护甲结果 |
| `Source/GameXXK/Public/GameXXKCardRules.h` | 共享卡牌结算入口 | 不新增 UI 公式；保留现有结算 API |
| `Source/GameXXK/Private/GameXXKCardRules.cpp` | 单包伤害、DOT、药效、治疗、护甲、地形、任务、自动重放 | 为一次出牌补齐真实审计，不改变数值或触发顺序 |
| `Source/GameXXK/Public/GameXXKRelicRules.h`、`Private/GameXXKRelicRules.cpp` | 遗物触发 | 让震山鼓坠走共享伤害并回传 `Relic` 包，禁止递归触发 |
| `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp` | 完整 RuntimeState 的权威出牌门面 | 把出牌后遗物附伤追加进本次 `DamageResults` |
| `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`、`Private/UI/GameXXKBattleBoardWidget.cpp` | 手卡、目标代理、布局、演出队列 | 接入 Hover、缓存、预演层和清理生命周期 |
| `Source/GameXXK/Private/Tests/GameXXKAllCardPlayabilityAuditTest.cpp` | 198 卡×7 地形可出牌审计 | 抽取只供测试复用的标准运行时夹具 |
| `Content/Python/gamexxk_probe_real_play_flow.py` | PIE 运行时与 Board 探针 | 输出预演行、目标、位置、可见性、计算次数和单位 HP/护甲 |
| `scripts/gamexxk_real_play_flow_mcp.py` | 真实 PIE 输入、截图、报告、可恢复存档清理 | 新增预演验收模式与只移动鼠标的 Win32 输入 |
| `scripts/test_gamexxk_real_play_flow_mcp.py` | 真实 PIE harness 单元测试 | 锁定预演 verdict、截图和 cleanup 失败转红 |

新增文件职责：

| 文件 | 职责 |
|---|---|
| `Source/GameXXK/Public/GameXXKCardOutcomePreview.h` | 预演分类、目标聚合、彩色文本段、只读 Build API |
| `Source/GameXXK/Private/GameXXKCardOutcomePreview.cpp` | 完整状态副本结算、结果归类、1P/2P/3P 聚合与短文本格式化 |
| `Source/GameXXK/Public/UI/GameXXKCardOutcomePreviewWidget.h` | 输入穿透的预演行 Widget 公共接口 |
| `Source/GameXXK/Private/UI/GameXXKCardOutcomePreviewWidget.cpp` | 每行/每语义段 UMG 构造、颜色与行数限制 |
| `Source/GameXXK/Private/Tests/GameXXKCardOutcomeAuditTest.cpp` | Damage/Healing/Armor/Relic 审计门禁 |
| `Source/GameXXK/Private/Tests/GameXXKCardOutcomePreviewTest.cpp` | 副本不可变、真实提交一致、聚合和文本门禁 |
| `Source/GameXXK/Private/Tests/GameXXKCardOutcomePreviewCatalogTest.cpp` | 198 卡三分类和每卡合法预演覆盖 |
| `Source/GameXXK/Private/Tests/GameXXKCardOutcomePreviewWidgetTest.cpp` | 预演 Widget 颜色、行数、输入穿透门禁 |
| `Source/GameXXK/Private/Tests/GameXXKAllCardRuntimeTestUtils.h` | 198 卡运行时、完整状态投影、自动队列排空的测试夹具 |
| `docs/production/2026-08-12-target-hover-outcome-preview-status.md` | 最终 RED/GREEN、Automation、PIE 与复审证据 |

`GameXXKCardOutcomePreview.cpp` 的私有命名空间完整定义并只在该文件使用下列 helper，实施者不得假设它们已存在：

```cpp
struct FEnemySlotSnapshot
{
    FName UnitId = NAME_None;
    int32 SlotNumber = INDEX_NONE;
};

const FGameXXKCardCombatUnit* FindCombatUnitByStableId(
    const TArray<FGameXXKCardCombatUnit>& Units,
    FName UnitId)
{
    return Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
    {
        return Unit.UnitId == UnitId;
    });
}

bool SnapshotLivingEnemySlots(
    const FGameXXKCardBattleRuntime& Runtime,
    TArray<FEnemySlotSnapshot>& OutSlots,
    FString* OutError);
```

`SnapshotLivingEnemySlots` 的完整行为规则在 Task 4 Step 3 锁定；失败通过 `OutError` 返回，不使用一个无法表示错误的纯 `TArray` 返回值。

统一预演 API 与类型名称，后续任务不得另造同义模型：

```cpp
enum class EGameXXKCardOutcomePreviewClass : uint8
{
    None,
    ManualUnit,
    PureEnemyGroup
};

enum class EGameXXKCardOutcomeTone : uint8
{
    Neutral,
    Damage,
    Dot,
    Medicine,
    Healing,
    Armor,
    Lethal
};

struct FGameXXKCardOutcomeTextSegment
{
    FText Text;
    EGameXXKCardOutcomeTone Tone = EGameXXKCardOutcomeTone::Neutral;
};

struct FGameXXKCardOutcomeTextLine
{
    TArray<FGameXXKCardOutcomeTextSegment> Segments;
};

struct FGameXXKCardOutcomeTarget
{
    FName UnitId = NAME_None;
    EGameXXKCardTargetSide Side = EGameXXKCardTargetSide::Invalid;
    int32 SlotNumber = INDEX_NONE;
    int32 DirectDamage = 0;
    int32 GroupDamage = 0;
    int32 BleedDamage = 0;
    int32 PoisonDamage = 0;
    int32 BurnDamage = 0;
    int32 ToxicExplosionDamage = 0;
    int32 MedicineDamage = 0;
    int32 LinkedDamage = 0;
    int32 EffectiveHealing = 0;
    int32 EffectiveArmor = 0;
    bool bLethal = false;
    bool bAvoided = false;
    bool bRedirected = false;
};

struct FGameXXKCardOutcomePreview
{
    FName CardInstanceId = NAME_None;
    FName HoveredTargetUnitId = NAME_None;
    EGameXXKCardOutcomePreviewClass Classification = EGameXXKCardOutcomePreviewClass::None;
    bool bSuccess = false;
    bool bUsesEnemyPositionList = false;
    FString FailureText;
    TOptional<FGameXXKCardOutcomeTarget> FocusedTarget;
    TArray<FGameXXKCardOutcomeTarget> EnemyPositionTargets;
    TArray<FGameXXKCardOutcomeTextLine> FocusedLines;
    TArray<FGameXXKCardOutcomeTextLine> EnemyPositionLines;
};

class GAMEXXK_API FGameXXKCardOutcomePreviewRules final
{
public:
    static bool Build(
        const FGameXXKRuntimeState& State,
        FName CardInstanceId,
        FName HoveredTargetUnitId,
        FGameXXKCardOutcomePreview& OutPreview,
        FString* OutError = nullptr);
};
```

全任务统一使用下面两个 PowerShell 变量；每个命令都从项目根执行：

```powershell
$Project = 'D:\UE5 demo\GameXXK\GameXXK.uproject'
$ReportRoot = 'D:\UE5 demo\GameXXK\Saved\Automation'
```

每次 C++ 变更后的冷编译命令固定为：

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReload -NoHotReloadFromIDE
```

预期成功尾部必须含 `Result: Succeeded` 且进程退出码为 0；`--check-only`、Live Coding 和 Hot Reload 均不算编译证据。

## Task 1: 锁定并补齐伤害语义审计

**Files:**

- Create: `Source/GameXXK/Private/Tests/GameXXKCardOutcomeAuditTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroCardCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroHealerRuntimeTest.cpp`

- [ ] **Step 1: 先写伤害审计 RED**

在 `GameXXKCardOutcomeAuditTest.cpp` 新增以下精确测试路径：

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameXXKCardOutcomeDamageAuditTest,
    "GameXXK.Data.CardOutcomePreview.Audit.DamageKindAndCause",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
```

夹具必须逐项断言：

```cpp
TestEqual(TEXT("direct single keeps kind"), Single.Kind, EGameXXKCardDamageKind::SingleTargetAttack);
TestEqual(TEXT("group packet keeps kind"), Group.Kind, EGameXXKCardDamageKind::GroupAttack);
TestEqual(TEXT("ordinary poison keeps dot kind"), Poison.Kind, EGameXXKCardDamageKind::DamageOverTime);
TestEqual(TEXT("medicine reverse has medicine cause"), Reverse.Cause, EGameXXKCardDamageCause::Medicine);
TestEqual(TEXT("medicine source remains the card owner"), Reverse.SourceUnitId, OwnerUnitId);
```

同时在 `GameXXKCardCatalogTest.cpp` 与已有重复枚举门禁的 `GameXXKHeroCardCatalogTest.cpp` 锁定既有序列化值不动且只向末尾追加：

```cpp
TestEqual(TEXT("Environment remains 11"), static_cast<uint8>(EGameXXKCardDamageCause::Environment), uint8(11));
TestEqual(TEXT("Block remains 12"), static_cast<uint8>(EGameXXKCardDamageCause::Block), uint8(12));
TestEqual(TEXT("Medicine appends at 13"), static_cast<uint8>(EGameXXKCardDamageCause::Medicine), uint8(13));
TestEqual(TEXT("Relic appends at 14"), static_cast<uint8>(EGameXXKCardDamageCause::Relic), uint8(14));
```

更新 `GameXXKHeroHealerRuntimeTest.cpp` 的敌方逆疗断言：来源从 `Environment` 改为 `Medicine`，数值、忽略防御/护甲、药效快照和消费断言全部原样保留。

- [ ] **Step 2: 冷 UBT，亲眼确认精确 RED**

运行冷编译。预期失败只允许是 `FGameXXKCardDamageResult` 缺 `Kind`、枚举缺 `Medicine/Relic` 或新增断言失败；出现其他编译错先修测试夹具，不进入生产修改。

- [ ] **Step 3: 追加协议字段，绝不改旧枚举数值**

在 `EGameXXKCardDamageCause` 末尾追加：

```cpp
Block = 12,
Medicine = 13,
Relic = 14
```

在 `FGameXXKCardDamageResult` 的 `Cause` 前增加：

```cpp
/** Mitigation policy that produced this packet; never inferred from target mode or card name. */
UPROPERTY(BlueprintReadWrite, EditAnywhere)
EGameXXKCardDamageKind Kind = EGameXXKCardDamageKind::Invalid;
```

- [ ] **Step 4: 在所有真实伤害入口填 Kind/Cause**

在 `ApplyCombatDirectDamageInternal` 初始化新结果时直接复制：

```cpp
NewResult.SourceUnitId = Context.SourceUnitId;
NewResult.Kind = Context.Kind;
NewResult.Cause = IsDirectAttackDamageKind(Context.Kind)
    ? EGameXXKCardDamageCause::DirectAttack
    : Context.Kind == EGameXXKCardDamageKind::SelfHealthLoss
        ? EGameXXKCardDamageCause::SelfLoss
        : EGameXXKCardDamageCause::Environment;
```

在 `ApplyStatusHealthLoss` 和回合末手工 DOT 结果创建处显式写：

```cpp
DamageResult.Kind = EGameXXKCardDamageKind::DamageOverTime;
```

药效两个敌方分支在共享环境扣血返回后，统一覆盖：

```cpp
DamageResult.SourceUnitId = OwnerUnitId;
DamageResult.Cause = EGameXXKCardDamageCause::Medicine;
```

新增文件内谓词并替换 `ResolveOpenedHealerFormulasAfterActiveCard` 中两处旧 `Environment` 检查，避免药方触发回归：

```cpp
bool IsMedicineReverseDamage(const FGameXXKCardDamageResult& Result, const FName OwnerUnitId)
{
    return Result.SourceUnitId == OwnerUnitId
        && Result.Cause == EGameXXKCardDamageCause::Medicine
        && Result.HealthDamage > 0;
}
```

`FirstHealingMedicine` 使用该谓词；`LargeHealingArmorOrVulnerability` 还必须保留 `RequestedDamage >= 10` 门槛。

- [ ] **Step 5: 冷 UBT 后跑精确与药师邻接组**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $Project -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Data.CardOutcomePreview.Audit.DamageKindAndCause;Quit' '-TestExit=Automation Test Queue Empty' "-ReportOutputPath=$ReportRoot\TargetOutcome_Task1_Audit_GREEN" -log -stdout -FullStdOutLogOutput
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $Project -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Data.HeroCards.Healer;Quit' '-TestExit=Automation Test Queue Empty' "-ReportOutputPath=$ReportRoot\TargetOutcome_Task1_Healer_GREEN" -log -stdout -FullStdOutLogOutput
```

两个 `index.json` 均须 `failed=0`、`notRun=0`；药师组必须继续通过原有药效快照、毒爆和药方断言。

- [ ] **Step 6: 仅提交本任务文件**

```powershell
git add -- 'Source/GameXXK/Public/GameXXKCardTypes.h' 'Source/GameXXK/Private/GameXXKCardRules.cpp' 'Source/GameXXK/Private/Tests/GameXXKCardCatalogTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKHeroCardCatalogTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKHeroHealerRuntimeTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKCardOutcomeAuditTest.cpp'
git diff --cached --check
git commit -m 'feat: expose card outcome damage semantics'
git push origin main
```

## Task 2: 锁定并补齐治疗与护甲审计

**Files:**

- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardOutcomeAuditTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKFormationMasterPartnerRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBladePartnerBloodEdgeRuntimeTest.cpp`

- [ ] **Step 1: 为每个当前出牌分支写 Healing/Armor RED**

新增路径：

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameXXKCardOutcomePositiveAuditTest,
    "GameXXK.Data.CardOutcomePreview.Audit.HealingAndArmor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
```

使用目录中的真实卡/规则夹具逐项覆盖：普通治疗、药效友方治疗、药效全体治疗、地形 Forest 治疗、药方群体回血、流血触发刀客治疗、普通 `AddArmor`、当前内力转甲、溢出转甲、Village/Cave 地形护甲、药方护甲、寒冰任务护甲。每个分支都断言 SourceUnitId、TargetUnitId、Requested、Effective。

满血与满甲的硬门禁必须是：

```cpp
TestEqual(TEXT("full-health attempt is still audited"), Result.HealingResults.Num(), 1);
TestEqual(TEXT("full-health effective heal is zero"), Result.HealingResults[0].EffectiveHealing, 0);
TestEqual(TEXT("armor-cap attempt is still audited"), Result.ArmorResults.Num(), 1);
TestEqual(TEXT("armor-cap effective armor is zero"), Result.ArmorResults[0].EffectiveArmor, 0);
```

对白猿首次受负面状态获得护甲也加一个敌方 `ArmorResults` 断言，证明所有当前出牌关联的可见护甲尝试都有审计。`RestoreConsumedStatusesAndArmor` 的内部回滚不得生成 `ArmorResults`，另写断言锁住“内部恢复不是新护甲收益”。

- [ ] **Step 2: 冷 UBT，确认 RED 只来自缺少 ArmorResults 与漏审计分支**

期望新增测试编译时缺 `FGameXXKCardArmorResult`/`ArmorResults`，或运行时只在逐项列出的漏审计分支失败。

- [ ] **Step 3: 新增护甲协议和统一记录助手**

在 `FGameXXKCardHealingResult` 后新增：

```cpp
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCardArmorResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
    FName SourceUnitId = NAME_None;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
    FName TargetUnitId = NAME_None;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
    int32 RequestedArmor = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
    int32 EffectiveArmor = 0;
};
```

在 `FGameXXKCardPlayResult::HealingResults` 后新增：

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
TArray<FGameXXKCardArmorResult> ArmorResults;
```

在 `GameXXKCardRules.cpp` 的私有命名空间增加两个只包装现有原语的助手：

```cpp
int32 ApplyAndRecordHealing(
    FGameXXKCardPlayResult& Result,
    const FName SourceUnitId,
    FGameXXKCardCombatUnit& Target,
    const int32 RequestedHealing)
{
    FGameXXKCardHealingResult& Audit = Result.HealingResults.AddDefaulted_GetRef();
    Audit.SourceUnitId = SourceUnitId;
    Audit.TargetUnitId = Target.UnitId;
    Audit.RequestedHealing = FMath::Max(0, RequestedHealing);
    Audit.EffectiveHealing = GameXXKCardRules::HealCombatUnit(Target, Audit.RequestedHealing);
    return Audit.EffectiveHealing;
}

int32 ApplyAndRecordArmor(
    FGameXXKCardPlayResult& Result,
    const FName SourceUnitId,
    FGameXXKCardCombatUnit& Target,
    const int32 RequestedArmor)
{
    FGameXXKCardArmorResult& Audit = Result.ArmorResults.AddDefaulted_GetRef();
    Audit.SourceUnitId = SourceUnitId;
    Audit.TargetUnitId = Target.UnitId;
    Audit.RequestedArmor = FMath::Max(0, RequestedArmor);
    Audit.EffectiveArmor = GameXXKCardRules::AddCombatArmor(Target, Audit.RequestedArmor);
    return Audit.EffectiveArmor;
}
```

只有合法的正数效果尝试调用助手；因此满血/满甲记录 `Effective=0`，非法或条件未命中的效果不制造假包。

- [ ] **Step 4: 把当前出牌事务中的全部正向数值分支接到审计**

逐个改造以下现有位置，来源统一为触发这次效果的稳定卡主人/公式主人：

1. 已有套装触发的 2 点群体回血保持现有审计，并统一改用记录助手；`ResolveOpenedHealerFormulasAfterActiveCard` 的药方 2 点群体回血、2 点群体护甲、4 点目标护甲；
2. `ResolveTriggeredStatusLayerConsumption` 增加可空 `FGameXXKCardPlayResult* InOutPlayResult`，流血收招治疗在当前出牌链传结果，敌方回合/独立触发传空；
3. 刀客 `HealFromTriggeredBleed` 基础效果；
4. `ResolveTerrainBenefit` 增加 `FGameXXKCardPlayResult& InOutResult`，Forest/Village/Cave 记录全部目标；
5. `Heal`、`HealOrReverseWithMedicine`、`HealOrReverseFlat` 无论 Effective 是否为 0 都记录；
6. `AddArmor`、`GainArmorFromCurrentManaPercent`、`GainManaOverflowToArmor`；
7. 寒冰任务奖励当前出牌结算产生的护甲；
8. `GrantStatusFromCardEffect` 增加可空结果指针，只有从当前卡效果/当前公式调用时把白猿被动护甲记入审计；非卡牌状态入口传空。

所有 helper 签名沿调用栈传递同一个 `FGameXXKCardPlayResult`；不得在 helper 内创建局部结果后丢弃。`RequestedHealing/RequestedArmor` 保留效果尝试值，`Effective*` 只取现有 Clamp 原语返回值，因审计而不得改变治疗、护甲上限或触发顺序。

自动重放/任务奖励合并 `NestedResult` 时，紧邻既有 Damage/Status/Healing append 追加：

```cpp
InOutRewardResult.ArmorResults.Append(MoveTemp(NestedResult.ArmorResults));
```

自动队列的三处 `NewResult.HealingResults.Append(MoveTemp(NestedResult.HealingResults))` 紧邻下方同样追加 `NewResult.ArmorResults.Append(MoveTemp(NestedResult.ArmorResults))`，确保预演包含一次出牌链的自动收益。自动重放的 DamageResults 本来已合并；预演会把这些真实包按 Cause/Kind 归类到当前一次出牌结果，不解析 AutomaticResolutionCount 猜数值。

明确保留：

```cpp
GameXXKCardRules::AddCombatArmor(Target, RestoredArmor);
```

只在 `RestoreConsumedStatusesAndArmor` 使用，不写审计，因为它撤销内部临时消耗，不是玩家看到的护甲增益。

- [ ] **Step 5: 静态扫描所有原始正向写入，逐项解释剩余调用**

```powershell
rg -n 'HealCombatUnit\(|AddCombatArmor\(' Source/GameXXK/Private/GameXXKCardRules.cpp
rg -n 'HealingResults.Append|ArmorResults.Append' Source/GameXXK/Private/GameXXKCardRules.cpp
```

允许直接调用只剩：两个原语定义、非出牌阶段/敌方独立事件、`RestoreConsumedStatusesAndArmor` 内部回滚。上述任务清单中的每个出牌分支必须使用记录助手或显式生成结果。

- [ ] **Step 6: 冷 UBT，跑精确与三组邻接**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $Project -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Data.CardOutcomePreview.Audit.HealingAndArmor;Quit' '-TestExit=Automation Test Queue Empty' "-ReportOutputPath=$ReportRoot\TargetOutcome_Task2_Positive_GREEN" -log -stdout -FullStdOutLogOutput
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $Project -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Data.CardBattleRuntime;Quit' '-TestExit=Automation Test Queue Empty' "-ReportOutputPath=$ReportRoot\TargetOutcome_Task2_Runtime_GREEN" -log -stdout -FullStdOutLogOutput
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $Project -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Data.PartnerCards;Quit' '-TestExit=Automation Test Queue Empty' "-ReportOutputPath=$ReportRoot\TargetOutcome_Task2_Partners_GREEN" -log -stdout -FullStdOutLogOutput
```

- [ ] **Step 7: 提交并推送**

```powershell
git add -- 'Source/GameXXK/Public/GameXXKCardTypes.h' 'Source/GameXXK/Private/GameXXKCardRules.cpp' 'Source/GameXXK/Private/Tests/GameXXKCardOutcomeAuditTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKFormationMasterPartnerRuntimeTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKBladePartnerBloodEdgeRuntimeTest.cpp'
git diff --cached --check
git commit -m 'feat: audit card healing and armor outcomes'
git push origin main
```

## Task 3: 让出牌后遗物的伤害、治疗和护甲进入同一审计且不递归

**Files:**

- Modify: `Source/GameXXK/Public/GameXXKRelicRules.h`
- Modify: `Source/GameXXK/Private/GameXXKRelicRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardOutcomeAuditTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRelicSystemTest.cpp`

- [ ] **Step 1: 写震山鼓坠 RED**

新增 `GameXXK.Data.CardOutcomePreview.Audit.RelicLinkedDamage`：装备 `Relic.DrumCharm`，放置 1P/2P/3P 三个存活敌人并打出一张无直接伤害牌，断言三个敌人各失去 1 HP、结果恰好三个包、每包满足：

```cpp
TestEqual(TEXT("drum uses environmental mitigation kind"), Packet.Kind, EGameXXKCardDamageKind::EnvironmentalHealthLoss);
TestEqual(TEXT("drum has relic semantic cause"), Packet.Cause, EGameXXKCardDamageCause::Relic);
TestEqual(TEXT("drum preserves card owner source"), Packet.SourceUnitId, OwnerUnitId);
TestEqual(TEXT("drum deals exactly one health"), Packet.HealthDamage, 1);
```

同一测试再装备 `Relic.ChessStone` 与 `Relic.SwordGuard`，分别断言牌主人 1 点护甲和全队 1 点护甲进入 `ArmorResults`，包括目标已到 99 时的 `EffectiveArmor=0`。另放置 PineCone/RiverPearl/EnemyDefeated 遗物，锁定原始卡牌伤害触发的 1 点治疗/2 点护甲也进入本次 Healing/Armor 审计；震山鼓坠新包不会再次触发本次 `DamageTaken`，也不会让它击杀产生额外 `EnemyDefeated` 触发。当前触发时机和一次性行为保持原样。

- [ ] **Step 2: 运行精确测试并确认直接改 HP 导致 RED**

旧实现应出现敌人 HP 已变但 `DamageResults.Num()==0`。若不是该形态，先检查夹具是否真的经过 `FGameXXKCardBattleAdapter::ResolveCardPlay`。

- [ ] **Step 3: 修改遗物接口，分离 Primary 与 Linked**

公共签名改为：

```cpp
static bool ApplyCardPlayed(
    FGameXXKRuntimeState& InOutState,
    FName OwnerUnitId,
    const TArray<FGameXXKCardDamageResult>& PrimaryDamageResults,
    FGameXXKCardPlayResult& InOutCardPlayResult,
    FString* OutError = nullptr);
```

私有 `ApplyTrigger`/`ApplyCombatEffect` 增加可空 `FGameXXKCardPlayResult* InOutCardPlayResult` 和错误返回。只有 `CardPlayed`、由本次 Primary 包触发的 `DamageTaken`/`EnemyDefeated` 传该指针；BattleStart、RoundStart、RoundEnd 传空。`GainHeroArmor`、`GainPartyArmor`、`HealParty`、`HealDamagedUnit`、`ArmorDamagedUnit` 在结果指针非空时记录 Requested/Effective，在空时保持原非出牌行为。`DamageAllEnemies` 先拷贝三个存活敌人的稳定 UnitId，再逐个调用共享入口：

```cpp
FGameXXKCardDamageContext Context;
Context.SourceUnitId = NAME_None;
Context.Kind = EGameXXKCardDamageKind::EnvironmentalHealthLoss;
Context.ResolutionOrigin = EGameXXKCardResolutionOrigin::ActivePlay;
FGameXXKCardDamageResult Packet;
if (!GameXXKCardRules::ApplyCombatDirectDamage(
        Battle.Units, Battle.GuardLinks, Context, EnemyUnitId, Magnitude, Packet, OutError))
{
    return false;
}
Packet.SourceUnitId = OwnerUnitId;
Packet.Cause = EGameXXKCardDamageCause::Relic;
InOutCardPlayResult->DamageResults.Add(MoveTemp(Packet));
```

Adapter 在调用前先建立 `const TArray<FGameXXKCardDamageResult> PrimaryDamageResults = NewResult.DamageResults;`，再把该独立副本与 `NewResult` 分别传入 `ApplyCardPlayed`，避免 `InOutCardPlayResult.DamageResults.Add` 导致同一 TArray 读写别名/扩容失效。`ApplyCardPlayed` 的 CardPlayed 触发可以向 NewResult 追加包；之后 `ApplyDamageTaken` 和击杀扫描只读取 Primary 副本。CardPlayed 触发产生的 Linked 包绝不回灌这两个触发，从而保持现有非递归语义。治疗/护甲遗物只追加审计，不影响用于反应判定的 Primary 包。

- [ ] **Step 4: Adapter 原子追加 linked 包**

在同步 legacy 投影前：

```cpp
const TArray<FGameXXKCardDamageResult> PrimaryDamageResults = NewResult.DamageResults;
if (!FGameXXKRelicRules::ApplyCardPlayed(
        NewState,
        NewResult.OwnerUnitId,
        PrimaryDamageResults,
        NewResult,
        OutError))
{
    return false;
}
```

任何遗物伤害失败都保留 Adapter 现有副本原子性，不能提交半个状态。

- [ ] **Step 5: 冷 UBT，跑 audit 与完整遗物组**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $Project -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Data.CardOutcomePreview.Audit.RelicLinkedDamage;Quit' '-TestExit=Automation Test Queue Empty' "-ReportOutputPath=$ReportRoot\TargetOutcome_Task3_RelicAudit_GREEN" -log -stdout -FullStdOutLogOutput
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $Project -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Route.Relics;Quit' '-TestExit=Automation Test Queue Empty' "-ReportOutputPath=$ReportRoot\TargetOutcome_Task3_Relics_GREEN" -log -stdout -FullStdOutLogOutput
```

- [ ] **Step 6: 提交并推送**

```powershell
git add -- 'Source/GameXXK/Public/GameXXKRelicRules.h' 'Source/GameXXK/Private/GameXXKRelicRules.cpp' 'Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp' 'Source/GameXXK/Private/Tests/GameXXKCardOutcomeAuditTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKRelicSystemTest.cpp'
git diff --cached --check
git commit -m 'feat: audit card linked relic damage'
git push origin main
```

## Task 4: 实现完整状态副本的只读预演与短文本聚合

**Files:**

- Create: `Source/GameXXK/Public/GameXXKCardOutcomePreview.h`
- Create: `Source/GameXXK/Private/GameXXKCardOutcomePreview.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKCardOutcomePreviewTest.cpp`

- [ ] **Step 1: 先写规则层 RED 矩阵**

新增以下测试：

```cpp
"GameXXK.Data.CardOutcomePreview.Rules.InputIsImmutable"
"GameXXK.Data.CardOutcomePreview.Rules.MatchesCommittedCopy"
"GameXXK.Data.CardOutcomePreview.Rules.ManualDamageAndRedirect"
"GameXXK.Data.CardOutcomePreview.Rules.GroupPositions"
"GameXXK.Data.CardOutcomePreview.Rules.DotToxicMedicineRelic"
"GameXXK.Data.CardOutcomePreview.Rules.HealingArmorAndZero"
"GameXXK.Data.CardOutcomePreview.Rules.HeavyArrowPassiveAndLethal"
"GameXXK.Data.CardOutcomePreview.Rules.PendingChoiceDoesNotAutoSelect"
"GameXXK.Data.CardOutcomePreview.Rules.FailureClearsStaleOutput"
```

每个测试使用稳定 CardInstanceId/UnitId，且至少锁定：气势、虚弱、护甲吸收、破绽、标记、灵动闪避、守护改向、白猿首次状态护甲、厚皮、重箭多段、毒爆三包、蚀伤加成、蚀骨保层、药效快照、新药效保留、致死后后续段取消。

输入不可变断言使用完整结构比较：

```cpp
const FGameXXKRuntimeState Before = State;
FGameXXKCardOutcomePreview Preview;
TestTrue(TEXT("preview succeeds"), FGameXXKCardOutcomePreviewRules::Build(State, CardId, TargetId, Preview, &Error));
TestTrue(TEXT("preview never mutates source state"),
    FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State, &Before, PPF_None));
```

一致性测试在另一个副本调用 Adapter，逐包比对 `DamageResults`、`HealingResults`、`ArmorResults` 与预演聚合；RandomState 只在副本前进，原状态不变。

群体门禁精确断言：只有 1P 与 3P 存活时恰好两行、顺序 1P/3P、每行含自己的 `群体伤害`、毒爆/DOT/药效/联动、同位置合计、致死；不含 2P，不含全场总计。

- [ ] **Step 2: 冷 UBT，确认缺新 API 的 RED**

预期只因 `GameXXKCardOutcomePreview.h`/类型/API 不存在而编译失败。

- [ ] **Step 3: 实现严格只读 Build 骨架**

`Build` 顺序固定：

```cpp
OutPreview = FGameXXKCardOutcomePreview();
OutPreview.CardInstanceId = CardInstanceId;
OutPreview.HoveredTargetUnitId = HoveredTargetUnitId;

FGameXXKCardPlayPreview Playability;
if (!FGameXXKCardBattleAdapter::BuildCardPlayPreview(State, CardInstanceId, Playability, OutError)
    || !Playability.bCanPlay)
{
    OutPreview.FailureText = TEXT("无法预演");
    return false;
}

FGameXXKRuntimeState WorkingState = State;
TArray<FEnemySlotSnapshot> EnemySlots;
if (!SnapshotLivingEnemySlots(State.CardRun.ActiveBattle, EnemySlots, OutError))
{
    OutPreview.FailureText = TEXT("无法预演");
    return false;
}
const FGameXXKCardCombatUnit* HoveredBefore = HoveredTargetUnitId.IsNone()
    ? nullptr
    : FindCombatUnitByStableId(State.CardRun.ActiveBattle.Units, HoveredTargetUnitId);
FGameXXKCardPlayResult PlayResult;
if (!FGameXXKCardBattleAdapter::ResolveCardPlay(
        WorkingState, CardInstanceId, HoveredTargetUnitId, PlayResult, OutError))
{
    OutPreview.FailureText = TEXT("无法预演");
    return false;
}

OutPreview.bSuccess = true;
```

`FEnemySlotSnapshot` 只收预演开始前 `bLiving` 的敌方，Slot 必须来自 `FGameXXKBattlePresentation::GetSlotNumber`，按 1→3 排序。若存活敌方 Slot 不在 1..3 或两个单位占同一 Slot，Build 失败并显示 `无法预演`，不能错配数字。空位、开始前死亡或移除的单位不进快照；副本中死亡仍保留行。上述任何失败均保持 `bSuccess=false`；只有 Adapter 成功返回后才设 `bSuccess=true`。

若 `ResolveCardPlay` 设置 `PlayResult.bOpenedPendingChoice=true`，Build 不得调用任何 `ResolvePending*`、不得选默认候选项，只聚合本次 `ResolveCardPlay` 已返回的即时审计包。`PendingChoiceDoesNotAutoSelect` 在独立副本中确认待选择状态已打开、手牌/候选项与直接 Adapter 结果一致，同时原始 RuntimeState 逐字段不变；悬停层绝不预测玩家尚未作出的选择。

- [ ] **Step 4: 按真实包分类，不按卡名或 TargetMode 猜测**

分类规则精确为：

```cpp
const bool bHasGroupPacket = PlayResult.DamageResults.ContainsByPredicate(
    [](const FGameXXKCardDamageResult& Packet)
    {
        return Packet.Kind == EGameXXKCardDamageKind::GroupAttack;
    });

if (Playability.TargetRequest.bRequiresManualSelection)
{
    OutPreview.Classification = EGameXXKCardOutcomePreviewClass::ManualUnit;
}
else if (bHasGroupPacket
    && Playability.TargetRequest.EffectiveMode == EGameXXKCardTargetMode::AllEnemies)
{
    OutPreview.Classification = EGameXXKCardOutcomePreviewClass::PureEnemyGroup;
}
else
{
    OutPreview.Classification = EGameXXKCardOutcomePreviewClass::None;
}
OutPreview.bUsesEnemyPositionList = bHasGroupPacket;
```

非手动 AllEnemies 状态牌但没有 GroupAttack 包属于 `None`；RandomEnemy 等自动单位卡即使结算内部产生 GroupAttack，也不属于“纯敌方群攻手牌”入口，保持现有直接结算且不在手牌 Hover 展开。纯群攻必须同时是 EffectiveMode==AllEnemies 且实际产生 GroupAttack，由手牌 Hover 传 `NAME_None`，不能进入单位选择。手动单位牌只要实际产生 GroupAttack，仍通过 `bUsesEnemyPositionList` 展开敌方位置，但 Classification 保持 ManualUnit。

聚合表：

| Packet 条件 | 字段 |
|---|---|
| `Kind == SingleTargetAttack && Cause == DirectAttack` | `DirectDamage` |
| `Kind == GroupAttack && Cause == DirectAttack` | `GroupDamage` |
| `Cause == Bleed/Poison/Burn` | 对应普通 DOT |
| `Cause == ToxicExplosionBleed/Poison/Burn` | 全部合进 `ToxicExplosionDamage` |
| `Cause == Medicine` | `MedicineDamage` |
| `Cause == Relic/Counter/Block` 或其他明确出牌附伤 | `LinkedDamage` |

每个值累加 `HealthDamage`，不得使用 RequestedDamage。蚀伤只已体现在实际 DOT 包里，不生成单独类别。

为保证零伤害仍能显示，manual 敌方 FocusedTarget 在聚合任何包前就由 `HoveredBefore` 建立；闪避/护甲全挡时把所有 `OriginalTargetUnitId==HoveredTargetUnitId` 的攻击包（包括 Redirect 前）汇总为伤害类别 0，并从 `bAvoidedByAgility/bRedirected` 取短标记。敌方位置清单同样先按 EnemySlots 建三行，再叠加包，因此某位置群体伤害为 0 也显示 `群体伤害 0`。治疗/护甲的零值来自对应审计尝试，不从状态差猜测。

手动且没有 GroupAttack 时只建立 `FocusedTarget/FocusedLines`：伤害改向后原目标显示 0 且 `bRedirected=true`，不展开守护者；遗物命中其他敌人也不展开。手动且有 GroupAttack、悬停敌方时，只建立 `EnemyPositionTargets/EnemyPositionLines`，悬停目标自己的单体效果合并进实际承受单位那一行，不能再复制一个 Focused 框。手动悬停我方时，`FocusedTarget` 只汇总该 UnitId 的 HealingResults/ArmorResults；若同次真实产生 GroupAttack，另外建立敌方位置数组与清单，因此两个输入穿透表面可以同时存在。纯群攻只建立敌方位置清单。

手动状态牌即使没有即时数值仍归 `ManualUnit` 并成功返回，但两个 Lines 数组为空，Board 不显示空面板；`None` 代表无需本功能预览的自动/自身/无目标结果。合法攻击、治疗或护甲尝试即使实际为 0 仍必须有对应行。

- [ ] **Step 5: 实现确定性短文本**

格式顺序固定：前缀位置、伤害、群体伤害、流血、中毒、灼烧、毒爆、药效伤害、联动伤害、治疗、护甲、合计、已改向、致死。同一行用 ` · `，不输出层数或公式。

规则：

```text
单体一个伤害类别：伤害 48
单体两个以上伤害类别：第一行列类别；第二行 合计 66 · 致死
群体位置：1P 群体伤害 48 · 毒爆 18 · 合计 66 · 致死
满血/满甲合法尝试：治疗 +0 / 护甲 +0
全挡或闪避：伤害 0
守护改向：伤害 0 · 已改向
失败：`bSuccess=false`、`FailureText=无法预演`，FocusedLines/EnemyPositionLines 都为空
```

FocusedLines 最多两行；EnemyPositionLines 每个现存 Slot 恰好一行、最多三行；群体不得生成全场合计行，`GroupDamage` 的标签必须是 `群体伤害`。`FailureClearsStaleOutput` 先用成功结果填充两组行，再用非法 CardInstanceId 调用同一 OutPreview，断言两个数组和两个 Target 容器已清空、只留 `FailureText`，防止模型层残留旧数字。

- [ ] **Step 6: 冷 UBT 并跑完整规则组**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $Project -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Data.CardOutcomePreview.Rules;Quit' '-TestExit=Automation Test Queue Empty' "-ReportOutputPath=$ReportRoot\TargetOutcome_Task4_Rules_GREEN" -log -stdout -FullStdOutLogOutput
```

报告要求所有九个叶测试真实出现且 `failed=0/notRun=0`。

- [ ] **Step 7: 提交并推送**

```powershell
git add -- 'Source/GameXXK/Public/GameXXKCardOutcomePreview.h' 'Source/GameXXK/Private/GameXXKCardOutcomePreview.cpp' 'Source/GameXXK/Private/Tests/GameXXKCardOutcomePreviewTest.cpp'
git diff --cached --check
git commit -m 'feat: add read only card outcome preview'
git push origin main
```

## Task 5: 对 198 张卡做三分类和逐卡预演认证

**Files:**

- Create: `Source/GameXXK/Private/Tests/GameXXKAllCardRuntimeTestUtils.h`
- Create: `Source/GameXXK/Private/Tests/GameXXKCardOutcomePreviewCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKAllCardPlayabilityAuditTest.cpp`

- [ ] **Step 1: 抽取既有全卡夹具，不改变原断言**

把 `EveryTerrain`、`AddStatus`、`PrimeRepresentativeStatuses`、`MakeUnit`、`MakeCard`、`RequiredOwnerCohortSize`、`BuildRuntime`、`DrainChoicesAndAutomaticQueue` 移到 `GameXXKAllCardRuntimeTestUtils.h` 的同名 namespace。原 `All198EveryTerrain` 改用该 header，先运行并确认仍是 198×7 全绿；不得删除 condition-hit/condition-miss、稳定 CardId、队列排空或 runtime validation 断言。

- [ ] **Step 2: 在测试工具新增完整状态包装器**

新增：

```cpp
bool BuildRuntimeState(
    FAutomationTestBase& Test,
    const FGameXXKCardDefinition& Definition,
    EGameXXKCardTerrain Terrain,
    int32 Seed,
    FGameXXKRuntimeState& OutState,
    FName& OutPlayedInstanceId,
    FString& OutError);
```

实现先调用现有 `BuildRuntime`；它当前建立两个敌方，包装器按稳定顺序写 `BattleSlotNumber=1,2`。三位置/缺 2P 的专门规则与 UI 测试另建三敌夹具，不改变原 198 可出牌拓扑。随后建立：

```cpp
OutState = UGameXXKMVPRules::CreateNewGame();
OutState.Screen = EGameXXKScreen::Battle;
OutState.bHasActiveBattle = true;
OutState.ActiveBattleNodeId = 198;
OutState.CardRun.bHasActiveCardBattle = true;
OutState.CardRun.ActiveBattleSourceNodeId = 198;
OutState.CardRun.ActiveBattle = MoveTemp(Runtime);
```

将每个 `FGameXXKCardCombatUnit` 映射到 `ActiveBattleParty/ActiveBattleEnemies` 的稳定 legacy Id、HP/MaxHP、MP/MaxMP、Attack、Defense、Speed、Shield、bEnemy，然后调用 `FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection`。包装器失败必须报具体 CardId/Terrain，禁止跳过。

- [ ] **Step 3: 写 198 分类 RED**

新增：

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameXXKCardOutcomePreviewCatalogCoverageTest,
    "GameXXK.Data.CardOutcomePreview.Catalog.All198ClassifiedAndPlayable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
```

算法遍历 `GetAllCardDefinitions()`，先断言数量恰好 198 且 Id 唯一。每张卡跑七种 Terrain：若 `TargetRequest.bRequiresManualSelection`，必须收集并按 `CandidateViews` 稳定顺序遍历全部 `bCanSelect` 的 UnitId，为每个合法目标用新的 RuntimeState 副本单独 Build；若非手动选择，只传一次 `NAME_None`。这会同时覆盖同卡对敌/对友分支，不让“第一个合法目标”掩盖另一侧的毒爆、药效、治疗或护甲。按所有实际预演观察聚合最终分类：

1. 任一合法场景是 `ManualUnit`，该 CardId 最终为 Manual；
2. 否则任一合法场景是 `PureEnemyGroup`（EffectiveMode==AllEnemies 且实际有 GroupAttack），最终为 PureEnemyGroup；
3. 否则为 None；
4. 同一 CardId 在七地形不得同时出现 Manual 与 PureEnemyGroup；出现即列出 CardId/Terrain 并失败，要求人工明确交互而非静默按优先级吞掉；
5. 另记录“非手动、非 AllEnemies、但实际产生 GroupAttack”的自动单位卡为 `AutomaticNonPreviewGroup` 诊断集合，这些卡最终仍属于 None，测试锁定手牌 Hover 不展开，避免把随机/自动目标牌误改成交互预览。

最终断言：

```cpp
TestEqual(TEXT("all catalog cards are classified"), Manual.Num() + Group.Num() + None.Num(), 198);
TestTrue(TEXT("manual category is exercised"), Manual.Num() > 0);
TestTrue(TEXT("group category is exercised"), Group.Num() > 0);
TestEqual(TEXT("classification conflicts"), Conflicts.Num(), 0);
TestEqual(TEXT("manual target attempts that failed preview"), ManualTargetFailures.Num(), 0);
TestEqual(TEXT("group cards without a successful preview"), GroupFailures.Num(), 0);
```

每个 Manual CardId 在每个 `bCanPlay` 的 Terrain 上都必须至少有一个 `bCanSelect` 目标，而且每个列出的合法目标都必须 Build 成功；每个 Group CardId 至少一个 Terrain 必须实际 Build 成功。AutomaticNonPreviewGroup 的每张卡必须 Build 成功但返回 None 且两个展示行数组均为空；未知 CardId、夹具失败、手动卡无合法目标都直接红，不能 `continue` 后算通过。诊断串必须包含 CardId/Terrain/TargetUnitId，便于直接定位指向问题。

- [ ] **Step 4: 先运行并确认任何未分类/失败场景 RED，再只修夹具或预演通用逻辑**

禁止为单张卡在 UI 写 CardId 特判。若某卡因标准夹具资源不足，统一提高夹具共享资源；若合法目标缺失，补通用第三敌人/队友；若结算器漏审计，回到 Task 1–3 的共享规则修复。

- [ ] **Step 5: 冷 UBT，跑新认证与原全卡审计**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $Project -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Data.CardOutcomePreview.Catalog;Quit' '-TestExit=Automation Test Queue Empty' "-ReportOutputPath=$ReportRoot\TargetOutcome_Task5_Catalog_GREEN" -log -stdout -FullStdOutLogOutput
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $Project -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Data.AllCards.Playability;Quit' '-TestExit=Automation Test Queue Empty' "-ReportOutputPath=$ReportRoot\TargetOutcome_Task5_All198_GREEN" -log -stdout -FullStdOutLogOutput
```

- [ ] **Step 6: 提交并推送**

```powershell
git add -- 'Source/GameXXK/Private/Tests/GameXXKAllCardRuntimeTestUtils.h' 'Source/GameXXK/Private/Tests/GameXXKCardOutcomePreviewCatalogTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKAllCardPlayabilityAuditTest.cpp'
git diff --cached --check
git commit -m 'test: certify 198 card outcome preview coverage'
git push origin main
```

## Task 6: 构造简洁、输入穿透的分段文本 Widget

**Files:**

- Create: `Source/GameXXK/Public/UI/GameXXKCardOutcomePreviewWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKCardOutcomePreviewWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKCardOutcomePreviewWidgetTest.cpp`

- [ ] **Step 1: 写 Widget RED**

新增 `GameXXK.UI.Battle.CardOutcomePreviewWidget`，断言：单体两行上限、群体三行上限、每行各类别顺序、根可见性 `HitTestInvisible`、Clear 后 Collapsed、重建不保留旧行、每个 Tone 颜色准确。

颜色固定为低饱和水墨值：

```cpp
Damage  = FLinearColor(0.66f, 0.24f, 0.20f, 1.0f);
Dot     = FLinearColor(0.25f, 0.48f, 0.31f, 1.0f);
Medicine= FLinearColor(0.58f, 0.39f, 0.20f, 1.0f);
Healing = FLinearColor(0.24f, 0.55f, 0.46f, 1.0f);
Armor   = FLinearColor(0.34f, 0.45f, 0.55f, 1.0f);
Neutral = FLinearColor(0.79f, 0.75f, 0.66f, 1.0f);
Lethal  = FLinearColor(0.82f, 0.34f, 0.26f, 1.0f);
```

- [ ] **Step 2: 冷 UBT，确认新 Widget API 缺失 RED**

- [ ] **Step 3: 实现程序化 Widget，不新建蓝图或资产**

公共接口：

```cpp
UCLASS()
class GAMEXXK_API UGameXXKCardOutcomePreviewWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetLines(const TArray<FGameXXKCardOutcomeTextLine>& InLines);
    void Clear();
    int32 GetVisibleLineCountForTest() const;
    FString GetPlainLineForTest(int32 LineIndex) const;
    FLinearColor GetSegmentColorForTest(int32 LineIndex, int32 SegmentIndex) const;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> LineBox;
};
```

每个 `FGameXXKCardOutcomeTextLine` 构造一个 `UHorizontalBox`，每个 Segment 构造一个 `UTextBlock`，沿用 Board 字体/描边，字号 18；不增加背景大面板、不动画、不闪屏。`SetLines` 非空时根设 `HitTestInvisible`，`Clear` 清子项并设 `Collapsed`。

- [ ] **Step 4: 冷 UBT，跑 Widget 叶测试**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $Project -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.UI.Battle.CardOutcomePreviewWidget;Quit' '-TestExit=Automation Test Queue Empty' "-ReportOutputPath=$ReportRoot\TargetOutcome_Task6_Widget_GREEN" -log -stdout -FullStdOutLogOutput
```

- [ ] **Step 5: 提交并推送**

```powershell
git add -- 'Source/GameXXK/Public/UI/GameXXKCardOutcomePreviewWidget.h' 'Source/GameXXK/Private/UI/GameXXKCardOutcomePreviewWidget.cpp' 'Source/GameXXK/Private/Tests/GameXXKCardOutcomePreviewWidgetTest.cpp'
git diff --cached --check
git commit -m 'feat: render concise card outcome rows'
git push origin main
```

## Task 7: 接入 Board Hover、缓存、位置与完整清理生命周期

**Files:**

- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardOutcomePreviewWidgetTest.cpp`

`GameXXKBattleBoardWidget.h` 需直接 `#include "GameXXKMVPRules.h"`，因为它将持有 `TOptional<FGameXXKRuntimeState>` 完整值；不能只前向声明不完整 USTRUCT。

- [ ] **Step 1: 写 Board 生命周期 RED**

在现有 `GameXXK.Integration.CardBattle.BoardTargeting` 附近增加独立路径：

```cpp
"GameXXK.Integration.CardBattle.TargetOutcomePreview.ManualHover"
"GameXXK.Integration.CardBattle.TargetOutcomePreview.GroupHandHover"
"GameXXK.Integration.CardBattle.TargetOutcomePreview.CacheAndClear"
"GameXXK.Integration.CardBattle.TargetOutcomePreview.LayoutInvariant"
```

使用现有 `BuildManualTargetCardFixture` 扩展单体场景，新增三敌群攻夹具。断言：

- 非目标模式 Hover 不显示；非法目标 Hover 不显示；合法目标显示；
- 单体位置只跟当前目标 HUD，最多两行；
- 群攻手牌 Hover 产生 1P/2P/3P 三行，空位去掉；点击仍一次结算且不进入 TargetingCard；
- 同卡+同目标+完整状态相等的连续 Hover 只 Build 一次，换目标 Build 一次；
- Unhover、换卡、取消、提交成功、提交失败、Refresh 权威变化、QueuePresentation、CancelBattleVisualSession、死亡/移除、NativeDestruct 清空；
- 预演失败先清旧行再显示 `无法预演`；
- 原有 ProjectedUnitHuds、手牌、意图、按钮的 anchors/offset/size/z 在功能前后逐个相等。

- [ ] **Step 2: 冷 UBT，确认缺 Board seam/hover 转发导致 RED**

- [ ] **Step 3: 给透明目标代理增加 Hover 转发**

在 `UGameXXKBattleUnitTargetProxyButton` 增加 `HandleHovered/HandleUnhovered`，Configure 时：

```cpp
OnClicked.Clear();
OnHovered.Clear();
OnUnhovered.Clear();
OnClicked.AddDynamic(this, &UGameXXKBattleUnitTargetProxyButton::HandleClicked);
OnHovered.AddDynamic(this, &UGameXXKBattleUnitTargetProxyButton::HandleHovered);
OnUnhovered.AddDynamic(this, &UGameXXKBattleUnitTargetProxyButton::HandleUnhovered);
```

转发到 Board：

```cpp
void HandleUnitTargetProxyHoverChanged(FName UnitId, bool bHovered);
```

Board 仅在 `IsCardTargetingActive()` 且 `LegalCardTargetUnitIds.Contains(UnitId)` 时 Build；移开只清该目标当前预演。

- [ ] **Step 4: 在 RootCanvas 增加输入穿透层，不改现有布局**

新增 `BattleOutcomePreviewLayer`，作为 `RootCanvas` 子项，铺满 1920×1080，`HitTestInvisible`，Z=1；现有 `BattleProjectedUnitHudLayer` 保持 Z=0，RootCanvas 在 `BattleDesignStage` 的 Z=20 保持不变。

单体 Widget 使用目标现有 `TryResolveFixedUnitHudLayout` 锚点，精确 Slot：

```cpp
PreviewSlot->SetAnchors(FixedLayout.Anchors);
PreviewSlot->SetAlignment(FVector2D(0.5f, 1.0f));
PreviewSlot->SetOffsets(FMargin(0.0f, -8.0f, 272.0f, 56.0f));
```

这会把 272×56 的结果框放在目标 HUD 顶缘上方，不改变 HUD 本身。敌方群体清单固定在敌方 HUD 区域：

```cpp
GroupSlot->SetAnchors(FAnchors(0.245f, 0.34f));
GroupSlot->SetAlignment(FVector2D(0.5f, 1.0f));
GroupSlot->SetOffsets(FMargin(0.0f, 0.0f, 620.0f, 108.0f));
```

只显示一个单体 Widget 或一个群体 Widget；不得为三个怪各建遮挡角色的浮框。

- [ ] **Step 5: 接入手牌 Hover，但不改变点击路径**

`SetHandCardHoverState(SlotIndex, true)` 在原 `RefreshCardTooltip()` 后：先以 `FGameXXKCardBattleAdapter::BuildCardPlayPreview` 确认该实例 `bCanPlay` 且 `TargetRequest.EffectiveMode==AllEnemies`，满足后才调用 outcome Build(`NAME_None`)；只有 Classification==PureEnemyGroup 才显示群体行。这样普通手牌 Hover 不运行完整副本结算。Unhover 清群体预演。

Build 失败时不从模型复用任何旧行。Board 用一个 Neutral segment 构造当次错误行；手动目标请求放入当前目标旁的 Single Widget，`NAME_None` 的纯群攻请求放入 Group Widget 的固定敌方区域：

```cpp
FGameXXKCardOutcomeTextLine FailureLine;
FGameXXKCardOutcomeTextSegment& Segment = FailureLine.Segments.AddDefaulted_GetRef();
Segment.Text = FText::FromString(Preview.FailureText.IsEmpty() ? TEXT("无法预演") : Preview.FailureText);
Segment.Tone = EGameXXKCardOutcomeTone::Neutral;
const TArray<FGameXXKCardOutcomeTextLine> FailureLines{FailureLine};
(RequestedTargetUnitId.IsNone() ? GroupOutcomeWidget : SingleOutcomeWidget)->SetLines(FailureLines);
```

这两条失败路径都在 SetLines 前调用 `ClearCardOutcomePreview()`；Board 叶测试分别锁定单体错误位置和群攻错误位置，且另一 Widget 必须 Collapsed。

`ClickCardInHand` 保留现有逻辑：手动卡 `BeginCardTargeting`，自动卡 `ResolveAutomaticCardPlay`。不得让群攻卡进入目标模式，不得新增确认按钮。

- [ ] **Step 6: 使用完整状态副本做无碰撞缓存**

Board 保存：

```cpp
FName CachedOutcomeCardInstanceId = NAME_None;
FName CachedOutcomeTargetUnitId = NAME_None;
TOptional<FGameXXKRuntimeState> CachedOutcomeSourceState;
FGameXXKCardOutcomePreview CachedOutcomePreview;
int32 OutcomePreviewBuildCountForTest = 0;
```

缓存命中条件必须同时满足 CardId、TargetId 和：

```cpp
FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
    &CurrentState, &CachedOutcomeSourceState.GetValue(), PPF_None)
```

不使用局部字段哈希，不允许碰撞或漏掉 RandomState/药效/套装次数。只在 Hover 事件计算，NativeTick 不调用 Build。

- [ ] **Step 7: 把清理集中到一个函数并覆盖全部出口**

```cpp
void ClearCardOutcomePreview()
{
    CachedOutcomeCardInstanceId = NAME_None;
    CachedOutcomeTargetUnitId = NAME_None;
    CachedOutcomeSourceState.Reset();
    CachedOutcomePreview = FGameXXKCardOutcomePreview();
    if (SingleOutcomeWidget) SingleOutcomeWidget->Clear();
    if (GroupOutcomeWidget) GroupOutcomeWidget->Clear();
}
```

从以下真实出口调用：`ClearCardTargetingState`、自动/手动 Resolve 成功前后、Resolve 失败、`RefreshPendingCardTargetingPreview` 检测变化、`RefreshProgrammaticLayout` 检测完整 RuntimeState 与缓存不一致、`QueuePresentationInternal` 第一个事件入队、`CancelBattleVisualSession`、`RemoveUnitVisual`、战斗终止、`NativeDestruct`、手牌/单位 Unhover。

真实 PIE 要通过 Python 取到真实控件几何，因此把已有 `GetUnitTargetProxyForTest` 改为反射可读 seam，并增加手牌按钮与 outcome 诊断。类型只返回可反射的基础类型，不直接暴露非 UENUM 的 `EGameXXKCardOutcomePreviewClass`：

```cpp
UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
UButton* GetUnitTargetProxyForTest(FName UnitId) const;

UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
UButton* GetHandCardButtonForTest(int32 SlotIndex) const;

UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
bool IsCardOutcomePreviewVisibleForTest() const;

UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
FString GetCardOutcomePreviewClassForTest() const; // None / ManualUnit / PureEnemyGroup

UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
FName GetCardOutcomePreviewCardInstanceIdForTest() const;

UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
FName GetCardOutcomePreviewTargetUnitIdForTest() const;

UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
TArray<FString> GetCardOutcomePreviewLinesForTest() const;

UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
int32 GetCardOutcomePreviewBuildCountForTest() const;

UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
FVector2D GetSingleOutcomePreviewAnchorForTest() const;

UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
FVector2D GetGroupOutcomePreviewAnchorForTest() const;
```

`GetHandCardButtonForTest` 只在 `HandCardButtons.IsValidIndex(SlotIndex)` 时返回对应按钮，否则返回 `nullptr`；文本 getter 按屏幕顺序返回 Focused 行后再返回 1P→2P→3P 行。这些 seam 只读可见性、文本行、当前卡/目标、BuildCount、单体/群体 Canvas anchor；不得暴露写入生产状态的测试后门。OutcomeLayer Z 仅在 C++ automation seam 读取。

- [ ] **Step 8: 冷 UBT，跑四个 Board 叶和 UI 邻接**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $Project -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Integration.CardBattle.TargetOutcomePreview;Quit' '-TestExit=Automation Test Queue Empty' "-ReportOutputPath=$ReportRoot\TargetOutcome_Task7_Board_GREEN" -log -stdout -FullStdOutLogOutput
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $Project -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.Integration.CardBattle;Quit' '-TestExit=Automation Test Queue Empty' "-ReportOutputPath=$ReportRoot\TargetOutcome_Task7_CardBattle_GREEN" -log -stdout -FullStdOutLogOutput
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $Project -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.UI.Battle;Quit' '-TestExit=Automation Test Queue Empty' "-ReportOutputPath=$ReportRoot\TargetOutcome_Task7_UI_GREEN" -log -stdout -FullStdOutLogOutput
```

- [ ] **Step 9: 提交并推送**

```powershell
git add -- 'Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h' 'Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp' 'Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKCardOutcomePreviewWidgetTest.cpp'
git diff --cached --check
git commit -m 'feat: show concise target outcome preview'
git push origin main
```

## Task 8: 真实 PIE 逐类验收、广组回归和交付证据

**Files:**

- Modify: `Content/Python/gamexxk_probe_real_play_flow.py`
- Modify: `scripts/gamexxk_real_play_flow_mcp.py`
- Modify: `scripts/test_gamexxk_real_play_flow_mcp.py`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleHudFixtureTest.cpp`
- Create: `docs/production/2026-08-12-target-hover-outcome-preview-status.md`
- Modify: `docs/superpowers/specs/2026-08-12-target-hover-outcome-preview-design.md`

- [ ] **Step 1: 先写 Python harness RED**

新增单元测试覆盖：

```python
TARGET_OUTCOME_SCENARIOS = (
    "Outcome.Single", "Outcome.HeavyArrow", "Outcome.GroupThree",
    "Outcome.GroupMissing2P", "Outcome.ToxicExplosion", "Outcome.MedicineEnemy",
    "Outcome.Healing", "Outcome.Armor", "Outcome.AgilityDodge",
    "Outcome.ArmorBlocked", "Outcome.GuardRedirect", "Outcome.Lethal",
)

def test_target_outcome_requires_all_twelve_scenarios_and_parity(self) -> None:
    report = self._valid_target_outcome_report()
    self.assertTrue(flow._target_outcome_preview_verdict(report)["ok"])
    del report["scenarios"]["Outcome.HeavyArrow"]
    verdict = flow._target_outcome_preview_verdict(report)
    self.assertFalse(verdict["ok"])
    self.assertIn("scenario_missing:Outcome.HeavyArrow", verdict["errors"])

def test_target_outcome_group_rows_are_ordered_and_omit_empty_slots(self) -> None:
    report = self._valid_target_outcome_report()
    self.assertEqual(
        ["1P", "2P", "3P"],
        [row.split(" ", 1)[0] for row in report["scenarios"]["Outcome.GroupThree"]["preview_lines"]],
    )
    self.assertEqual(
        ["1P", "3P"],
        [row.split(" ", 1)[0] for row in report["scenarios"]["Outcome.GroupMissing2P"]["preview_lines"]],
    )
    self.assertTrue(all(
        "群体伤害" in row
        for row in report["scenarios"]["Outcome.GroupThree"]["preview_lines"]
    ))

def test_target_outcome_failure_and_stale_text_turn_verdict_red(self) -> None:
    report = self._valid_target_outcome_report()
    report["scenarios"]["Outcome.Single"]["after_unhover"]["visible"] = True
    report["scenarios"]["Outcome.Single"]["after_unhover"]["lines"] = ["伤害 48"]
    verdict = flow._target_outcome_preview_verdict(report)
    self.assertFalse(verdict["ok"])
    self.assertIn("stale_preview_after_unhover:Outcome.Single", verdict["errors"])

def test_target_outcome_missing_screenshot_turns_verdict_red(self) -> None:
    report = self._valid_target_outcome_report()
    report["scenarios"]["Outcome.ToxicExplosion"]["screenshot"] = ""
    verdict = flow._target_outcome_preview_verdict(report)
    self.assertFalse(verdict["ok"])
    self.assertIn("screenshot_missing:Outcome.ToxicExplosion", verdict["errors"])

def test_target_outcome_cleanup_stop_or_delete_failure_turns_verdict_red(self) -> None:
    report = self._valid_target_outcome_report()
    report["cleanup"] = {
        "ok": False,
        "errors": ["wait_for_pie_stop_after_real_flow", "delete_default_save_after_real_flow"],
    }
    verdict = flow._target_outcome_preview_verdict(report)
    self.assertFalse(verdict["ok"])
    self.assertIn("cleanup_failed", verdict["errors"])
```

`_valid_target_outcome_report()` 必须在该 test class 内返回十二个键齐全场景：每个场景含 `ok=true`、非空 `screenshot`、`preview_lines`、`predicted`、`committed_delta`、`after_unhover={visible:false,lines:[]}`；`predicted` 与 `committed_delta` 为按 UnitId 键名的 `health_damage/healing/armor`整数字典，整体完全相等。三位群体行是 `1P 群体伤害 10`、`2P 群体伤害 11`、`3P 群体伤害 12`；缺 2P 时只保留 1P/3P。`_target_outcome_preview_verdict` 必须同时校验场景全、每场景 `ok`、数值字典相等、截图路径存在、群体行顺序/缺位/文案、Unhover 无旧文本与 cleanup 成功；任一缺失以上述稳定 error key 转红。

十二个场景固定为：单体、多段/重箭、纯群体三位置、群体缺 2P、毒爆、药效对敌、治疗、护甲、灵动闪避、护甲全挡、守护改向、致死。多段/重箭可以和单体同一个夹具报告，但 verdict 中必须有独立键。

扩展 `GameXXKBattleHudFixtureTest.cpp` 的反射 seam 契约：`ApplyTargetOutcomeFixtureForTest(FName ScenarioId, FString& OutError)` 只接受以下稳定 Id，并为每种场景建立可真实点击、可恢复的临时 RuntimeState：

```text
Outcome.Single
Outcome.HeavyArrow
Outcome.GroupThree
Outcome.GroupMissing2P
Outcome.ToxicExplosion
Outcome.MedicineEnemy
Outcome.Healing
Outcome.Armor
Outcome.AgilityDodge
Outcome.ArmorBlocked
Outcome.GuardRedirect
Outcome.Lethal
```

该 seam 与现有只读 `BattleHudFixtureView` 严格分离：现有 fixture 会被 Board 的 `RejectBattleHudFixtureMutation` 拦截，不能用于真实点击。新增 `TOptional<FGameXXKRuntimeState> TargetOutcomeFixtureBackup`，Apply 时先保存原 RuntimeState、再把可点击夹具移入真实 RuntimeState；`ClearTargetOutcomeFixtureForTest` 将原状态逐字段恢复。测试锁定未知 ScenarioId 原子失败、重复 Apply 原子失败、清理恢复原状态、每个夹具存在恰好一张待测手牌及稳定 1P/2P/3P。

- [ ] **Step 2: 运行 Python 和冷 UBT，确认新模式与 C++ seam 都是 RED**

```powershell
python -B -m unittest scripts.test_gamexxk_real_play_flow_mcp
$Project = 'D:\UE5 demo\GameXXK\GameXXK.uproject'
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReload -NoHotReloadFromIDE
```

Python 必须因 `--target-outcome-preview`/十二场景尚未实现而失败；冷 UBT 必须只因 `ApplyTargetOutcomeFixtureForTest`/`ClearTargetOutcomeFixtureForTest`/`IsTargetOutcomeFixtureActiveForTest` 缺失而失败。保留两份完整 RED 日志，不在测试中用 SFINAE 吞掉缺失 API。

- [ ] **Step 3: 实现确定性 outcome fixture，并扩展 PIE 探针**

在 `UGameXXKMVPSubsystem` 增加 DevelopmentOnly：

```cpp
UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP|Development", meta = (DevelopmentOnly))
bool ApplyTargetOutcomeFixtureForTest(FName ScenarioId, FString& OutError);

UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP|Development", meta = (DevelopmentOnly))
bool ClearTargetOutcomeFixtureForTest(FString& OutError);

UFUNCTION(BlueprintPure, Category = "GameXXK|MVP|Development", meta = (DevelopmentOnly))
bool IsTargetOutcomeFixtureActiveForTest() const;
```

实现先 `BeginRuntimeStateMutation(BattleHudFixtureView)` 清除只读 overlay，再把原 RuntimeState 放入 `TargetOutcomeFixtureBackup`。每种场景以真实卡目录、稳定单位/状态/遗物和固定 CombatRandomState 构造；必须经过 `ValidateCardBattleRuntime` 和 `SyncCardBattleToLegacyProjection` 后才替换 RuntimeState。构造或验证失败时清空 backup 并保留原状态。群体三位置/缺 2P、药效、毒爆、灵动、护甲、改向和致死都由运行时状态产生，不把预期数字硬写进 Board。Clear 无 backup 时返回成功且不改状态，有 backup 时原子恢复并清空 backup。

`gamexxk_probe_real_play_flow.py` 新增 `--apply-target-outcome-fixture ScenarioId` 与 `--clear-target-outcome-fixture`，调用对应 seam 后刷新 PlayerFlow Widgets；失败写出 `{"ok": false, "reason": str(error)}`。

在 `gamexxk_probe_real_play_flow.py::_battle_board_summary` 增加：

```python
result["outcome_preview"] = {
    "visible": bool(board.is_card_outcome_preview_visible_for_test()),
    "classification": str(board.get_card_outcome_preview_class_for_test()),
    "card_instance_id": str(board.get_card_outcome_preview_card_instance_id_for_test()),
    "target_unit_id": str(board.get_card_outcome_preview_target_unit_id_for_test()),
    "lines": [str(line) for line in board.get_card_outcome_preview_lines_for_test()],
    "build_count": int(board.get_card_outcome_preview_build_count_for_test()),
    "single_anchor": _vector2d_to_dict(board.get_single_outcome_preview_anchor_for_test()),
    "group_anchor": _vector2d_to_dict(board.get_group_outcome_preview_anchor_for_test()),
}
```

同一 probe 已输出每个 UnitId 的 HP/Shield/Slot；点击前后报告用这些权威值验证预演数字。再把 `get_unit_target_proxy_for_test(UnitId)` 与每张可见手牌按钮的 `_widget_screen_summary` 加入结果，供 Win32 使用真实 screen rect，不用猜 1920 设计坐标。任何 getter 异常写入 `battle_board.errors` 并让 harness verdict 转红。

- [ ] **Step 4: 新增 Win32 move-only 输入和 `--target-outcome-preview` 模式**

在 `PreviewWindowController` 增加：

```python
def move_absolute_point(self, window, screen_x, screen_y):
    self.focus(window)
    self.user32.SetCursorPos(int(screen_x), int(screen_y))
    time.sleep(0.12)
    return {"x": int(screen_x), "y": int(screen_y)}
```

移动不发送 mouse down/up。新模式复用已存在的可恢复默认存档 sidecar、PIE 启停、Battle HUD fixture、截图与 cleanup；不得绕过 `close()`。

对每个场景：调用 `--apply-target-outcome-fixture`→探针取得待测手牌/目标 screen rect→移动或点击手牌→移动到合法目标→探针读预演→截图→点击目标或自动群攻卡→等待演出完成→探针读 HP/Shield→比较每包/合计。自动群攻卡点击后不进入目标模式；手动卡点击手牌后再 Hover/点击目标。每个场景的 `finally` 先调用 `--clear-target-outcome-fixture` 并验证 backup 已清空，再进入下一场景。群体报告必须是：

```python
assert [row.split(" ", 1)[0] for row in rows] == ["1P", "2P", "3P"]
assert all("群体伤害" in row for row in rows)
assert "全场" not in "".join(rows)
```

空 2P 场景必须满足 `[row.split(" ", 1)[0] for row in rows] == ["1P", "3P"]`。Unhover、取消和失败场景要求 `visible=False` 或唯一 `无法预演`，且不出现上一目标文本。

CLI：

```python
parser.add_argument("--target-outcome-preview", action="store_true")
```

该模式允许保留 PIE 仅用于人工复核时使用显式 `--keep-pie`；即使保留 PIE 也必须先恢复 target-outcome fixture。自动验收默认恢复 fixture、停止 PIE、删除临时槽并恢复 sidecar；fixture restore、PIE stop 或 save cleanup 任一失败都让顶层 verdict 转红。

- [ ] **Step 5: Python GREEN 后冷 UBT**

```powershell
python -B -m unittest scripts.test_gamexxk_real_play_flow_mcp
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReload -NoHotReloadFromIDE
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $Project -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.Battle.HudFixture;Quit' '-TestExit=Automation Test Queue Empty' "-ReportOutputPath=$ReportRoot\TargetOutcome_Task8_Fixture_GREEN" -log -stdout -FullStdOutLogOutput
```

记录 Python 实际 test count、UBT 退出码和 fixture 叶报告；fixture 必须真实发现 1 项、`failed=0/notRun=0/errors=0`，不能沿用旧报告。

- [ ] **Step 6: 用独立 Editor-Cmd 进程跑最终精确与邻接矩阵**

每行是一个新鲜进程：

```powershell
$Filters = @(
  'GameXXK.Data.CardOutcomePreview',
  'GameXXK.Data.AllCards.Playability',
  'GameXXK.Data.CardBattleRuntime',
  'GameXXK.Data.CardCombatRules',
  'GameXXK.Data.MarkRules',
  'GameXXK.Integration.MarkCardCompatibility',
  'GameXXK.Data.HeroCards.Healer',
  'GameXXK.Data.HeroCards.Hunter',
  'GameXXK.Data.PartnerCards.Guard',
  'GameXXK.Data.PartnerCards.Hunter',
  'GameXXK.Data.PartnerCards.Healer',
  'GameXXK.Route.Relics',
  'GameXXK.Integration.CardBattle',
  'GameXXK.UI.Battle.StatusEffectsWidget',
  'GameXXK.Battle'
)
foreach ($Filter in $Filters) {
  $SafeName = $Filter -replace '[^A-Za-z0-9]+','_'
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' $Project -Unattended -NoSplash -NoSound -NullRHI "-ExecCmds=Automation RunTests $Filter;Quit" '-TestExit=Automation Test Queue Empty' "-ReportOutputPath=$ReportRoot\TargetOutcome_Final_$SafeName" -log -stdout -FullStdOutLogOutput
  if ($LASTEXITCODE -ne 0) { throw "Automation process failed: $Filter" }
}
```

逐个解析 `index.json`，确认发现数大于 0、`failed=0`、`notRun=0`、`errors=0`；warning 逐条分类，不能只看目录名或退出码。任何新失败按 systematic-debugging 回到最小叶，不放宽断言。

- [ ] **Step 7: 通过 UE MCP harness 跑真实 PIE**

先让 UE MCP 保存所有 dirty package；若编辑器必须重启，确认保存成功后再关。随后：

```powershell
python -B scripts/gamexxk_real_play_flow_mcp.py --target-outcome-preview --report 'Saved/Automation/TargetOutcome_RealPIE_Final.json'
```

报告门禁：顶层 `ok=true`、十二场景全部 `ok=true`、每个预演总值等于点击后权威 HP/Shield 变化、单体/重箭/群体三位置/群体缺位/毒爆/药效/治疗/护甲/闪避/全挡/改向/致死均有截图、群体空位门禁通过、缓存同目标不重算、无布局跳动、cleanup `ok=true`、默认槽与 sidecar 均恢复。

人工视觉复核截图：低饱和色、现有字体/描边、水墨风格、单体血条附近、群体固定敌方区域、无遮挡手牌/意图/状态/角色、无高饱和面板、Hover 无明显卡顿。

- [ ] **Step 8: 写生产证据并更新规格状态**

`docs/production/2026-08-12-target-hover-outcome-preview-status.md` 写入：基线/最终 SHA、每个 RED 根因、每个 GREEN 报告绝对路径与 test count、warning 分类、198 分类计数与失败数、PIE 十二场景/截图/数值 parity、缓存次数、布局不变量、剩余风险。规格头状态改成“已实现并由生产证据验收”，链接该状态文件；不得把旧报告当当前证据。

- [ ] **Step 9: 请求独立代码复审并修完所有 Critical/Important**

复审重点：完整状态不可变、预演/真实同源、Medicine enum 兼容、药方触发未回归、Relic 不递归、Healing/Armor 零值审计、198 无漏卡、群体每位置、缓存失效、UI 布局、PIE cleanup。每个 finding 先复现再修；修后重新跑受影响叶、邻接组和最终矩阵。

- [ ] **Step 10: 最终暂存边界、提交与推送**

```powershell
git status --short --branch
git diff --check
git add -- 'Content/Python/gamexxk_probe_real_play_flow.py' 'scripts/gamexxk_real_play_flow_mcp.py' 'scripts/test_gamexxk_real_play_flow_mcp.py' 'Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h' 'Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp' 'Source/GameXXK/Private/Tests/GameXXKBattleHudFixtureTest.cpp' 'docs/production/2026-08-12-target-hover-outcome-preview-status.md' 'docs/superpowers/specs/2026-08-12-target-hover-outcome-preview-design.md'
git diff --cached --name-only
git diff --cached --check
git commit -m 'test: certify target outcome preview parity'
git push origin main
git rev-parse HEAD
git rev-parse origin/main
```

只在两个 SHA 完全一致且 tracked 工作树 clean 后交付；未跟踪美术和其他用户文件保持原样。

## 完成定义

以下全部成立才允许宣称完成：

- 伤害每包有正确 Kind/Cause；Medicine/Relic 只追加不重编号；
- 当前出牌关联的伤害、治疗和护甲尝试全部有审计，`+0` 也保留，内部回滚不伪装成收益；
- 震山鼓坠走共享伤害并进入本次结果，但不递归触发 DamageTaken/EnemyDefeated；
- 预演复制完整 RuntimeState，原状态逐字段不变，结果与独立副本真实提交一致；
- 单体只看当前目标；实际含 GroupAttack 或纯群攻手牌时按 1P→2P→3P 每个现存位置一行，空位删除、群体文案明确、无全场总计；
- 毒爆、普通 DOT、药效、联动、治疗、护甲、闪避、全挡、改向、致死均为真实实际值；
- 198 个 CardId 全部且只属于三类之一，Manual/Group 每卡至少一个合法成功预演；
- UI 输入穿透、行数/颜色/位置符合规格，现有 anchors/offset/size/z 完全不变；
- 所有失效出口清旧文本，同状态 Hover 有缓存且不在 Tick 重算；
- 冷 UBT、精确/邻接/广组 Automation、Python 单测、真实 PIE、独立复审全部为当前 SHA 的新鲜证据；
- `main` 已推送到 `origin/main`，并有可恢复 Git 历史。
