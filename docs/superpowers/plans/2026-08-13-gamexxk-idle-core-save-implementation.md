---
status: shelved
updated_at: 2026-08-17
shelved_reason: legacy migration package; do not execute
superseded_by: docs/superpowers/specs/2026-08-17-gamexxk-desktop-training-workbench-design.md
---
# GameXXK Idle Core And Save Implementation Plan

> 执行冻结：本文以 `CurrentSaveVersion=17` 为历史前置基线；当前工作区已进入 `CurrentSaveVersion=18`，本文的 v15/v16/v17/v18 迁移边界不能直接执行。恢复历练实现必须以 `2026-08-17-gamexxk-desktop-training-workbench-design.md` 和新的 Phase 0 基线从 v19 重新编排迁移编号。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立可持久化的历练状态、可替换 UTC 时间源、章节/效率快照和确定性离线结算，使 v15 存档安全迁移到 v16，且不依赖任何 UI。

**Architecture:** `FGameXXKIdleState` 是唯一历练权威；`FGameXXKIdleSettlementRules` 只在状态副本上使用定点整数推进金币、经验、两类宝箱流体事件和共享时间游标。`UGameXXKMVPSubsystem` 只负责编排 repository 的“复制→计算→校验→写盘→提交”，规则层不引用 SaveGame、UMG 或 Slate。

**Tech Stack:** Unreal Engine 5.8、C++ USTRUCT/SaveGame、`FDateTime` UTC ticks、MSVC 可编译的有界 `int64` 定点运算、GameXXK SaveMigration/MVPSubsystem、UE Automation、UBT。

---

## 0. 文件结构与固定协议

**Create:**

- `Source/GameXXK/Public/Idle/GameXXKIdleTypes.h` — 所有可保存历练值类型。
- `Source/GameXXK/Public/Idle/GameXXKIdleTimeSource.h` — 可替换 UTC 接口和本机实现。
- `Source/GameXXK/Private/Idle/GameXXKIdleTimeSource.cpp` — 唯一 `FDateTime::UtcNow()` 调用点。
- `Source/GameXXK/Public/Idle/GameXXKIdleCatalog.h` — 三章效率、怪物构成与掉落/奖励配置读取。
- `Source/GameXXK/Private/Idle/GameXXKIdleCatalog.cpp` — 首版显式配置，不含角色特例。
- `Source/GameXXK/Public/Idle/GameXXKIdleSettlementRules.h` — 纯结算、章节切换和校验 API。
- `Source/GameXXK/Private/Idle/GameXXKIdleSettlementRules.cpp` — 离线 fluid solver。
- `Source/GameXXK/Public/Idle/GameXXKIdleRepository.h` — 候选状态保存边界。
- `Source/GameXXK/Private/Idle/GameXXKIdleRepository.cpp` — 通过 MVPSubsystem 的 slot 写入 seam 保存。
- `Source/GameXXK/Private/Tests/GameXXKIdleSettlementRulesTest.cpp` — 分段一致、容量/时间/7 天门禁。
- `Source/GameXXK/Private/Tests/GameXXKIdleSaveMigrationTest.cpp` — v15→v16 与往返门禁。

**Modify:**

- `Source/GameXXK/Public/GameXXKMVPRules.h` — include `GameXXKIdleTypes.h`，在 RuntimeState 末尾加 `IdleState`。
- `Source/GameXXK/Private/GameXXKMVPRules.cpp` — 新局初始化、Boss 通关解锁、MakeSaveState。
- `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h` — `IdleStateIntroducedSaveVersion=16`、`CurrentSaveVersion=16`。
- `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp` — 初始化/迁移/校验历练状态。
- `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h` — 时间源注入、Settlement/Switch/Capacity API。
- `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp` — 原子保存编排；执行前必须先合并上一任务对本文件的修改。
- `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp` — 版本/磁盘事务邻测。
- `Source/GameXXK/Private/Tests/GameXXKHeroCardUnlockMigrationTest.cpp`、`GameXXKRouteCardEntriesSaveMigrationTest.cpp`、`GameXXKRouteEconomySaveMigrationTest.cpp` — 把硬编码 v15 更新为 v16 或动态 current。

固定点比例：

```cpp
static constexpr int64 GameXXKIdleFixedScale = 1'000'000;
static constexpr int64 GameXXKIdleTicksPerSecond = ETimespan::TicksPerSecond;
static constexpr int64 GameXXKIdleMaxEconomySeconds = 7LL * 24LL * 60LL * 60LL;
static constexpr int32 GameXXKIdleInitialChestCapacity = 20;
static constexpr int32 GameXXKIdleMaximumChestCapacity = 80;
static constexpr int32 GameXXKIdleCapacityPerTalentLevel = 5;
static constexpr int32 GameXXKIdleMaximumCapacityTalentLevel = 12;
```

使用下列公共模型，字段名后续计划不得改变：

```cpp
UENUM(BlueprintType)
enum class EGameXXKIdleChestType : uint8 { Normal, Boss };

UENUM(BlueprintType)
enum class EGameXXKIdleChestContainer : uint8 { PendingLedger, Inventory };

UENUM(BlueprintType)
enum class EGameXXKIdleMonsterKind : uint8 { Normal, Elite };

UENUM(BlueprintType)
enum class EGameXXKIdleMode : uint8 { Offline, Online };

UENUM(BlueprintType)
enum class EGameXXKIdleRewardKind : uint8 { Gold, Experience, Item, EquipmentRoll };

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKIdleEfficiencySnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 Chapter = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 ConfigVersion = 1;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 NormalKillsPerHourFixed = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 EliteKillsPerHourFixed = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 GoldPerHourFixed = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 ExperiencePerHourFixed = 0;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKIdleRewardEntry
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKIdleRewardKind Kind = EGameXXKIdleRewardKind::Item;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName RewardId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 Quantity = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 EquipmentSeed = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKEquipmentSet EquipmentSet = EGameXXKEquipmentSet::Invalid;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKEquipmentSlot EquipmentSlot = EGameXXKEquipmentSlot::Weapon;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKEquipmentQuality EquipmentQuality = EGameXXKEquipmentQuality::Common;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 EquipmentItemLevel = 1;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKIdleChestBatch
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName ChestId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKIdleChestType Type = EGameXXKIdleChestType::Normal;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKIdleChestContainer Container = EGameXXKIdleChestContainer::PendingLedger;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 SourceChapter = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 Sequence = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 ObtainedTimeUtcTicks = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 FixedLootSeed = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 LootProfileVersion = 1;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) bool bRewardsExpanded = false;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TArray<FGameXXKIdleRewardEntry> RemainingRewards;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKIdleState
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 SchemaVersion = 1;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKIdleMode Mode = EGameXXKIdleMode::Offline;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 ActiveChapter = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TSet<int32> UnlockedChapters;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FGameXXKIdleEfficiencySnapshot Efficiency;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) bool bSettlementClockInitialized = false;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 LastSettlementTimeUtcTicks = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 LastClaimTimeUtcTicks = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 GoldEligibleSecondsUsed = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 ExperienceEligibleSecondsUsed = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 GoldAccrualRemainderNumerator = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 ExperienceAccrualRemainderNumerator = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 NormalChestCooldownRemainingTicks = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 BossChestCooldownRemainingTicks = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 NormalOfflineChestBalanceFixed = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 BossOfflineChestBalanceFixed = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 CapacityTalentLevel = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 ChestCapacity = 20;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 PendingGold = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 PendingExperience = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TArray<FGameXXKIdleChestBatch> ChestBatches;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 NextChestSequence = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 ChestRandomRootSeed = 1;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 NormalOnlineAttemptOrdinal = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 BossOnlineAttemptOrdinal = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) bool bClockRollbackObserved = false;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 DropConfigVersion = 1;
};
```

`ActiveChapter=0` 表示尚无已通关章节，不能开始历练。新运行中只在 Boss 胜利时写入 `UnlockedChapters`；v15→v16 迁移则必须从已经持久化的三章路线进度/Completed 状态恢复此前确实完成的章节，不能让老玩家丢失通关资格，也不能把“正在第 N 章”误算成第 N 章已完成。

## Task 1: 先锁类型默认值与 v16 迁移 RED

**Files:** `GameXXKIdleTypes.h`、`GameXXKIdleSaveMigrationTest.cpp`、SaveMigration 三文件、版本邻测。

- [ ] **Step 1: 写缺类型/版本的编译 RED**

新增 `GameXXK.MVP.Idle.SaveMigration.V15ToV16`，直接构造 `SaveVersion=15`：Accepted/follower 保持 v15 现状，本任务只迁移历练数据。至少覆盖新局、正在第2章、正在第3章、三章已完成四种来源：分别恢复 `{}`、`{1}`、`{1,2}`、`{1,2,3}`，活动章取最高已完成章；全部 `bSettlementClockInitialized=false`、奖励为空，禁止从 Unix epoch 补发奖励。

```cpp
TestEqual(TEXT("idle migration raises current save version"), FGameXXKSaveMigration::CurrentSaveVersion, 16);
TestEqual(TEXT("v15 creates schema one"), Migrated.RuntimeState.IdleState.SchemaVersion, 1);
TestEqual(TEXT("v15 fresh save has no active idle chapter"), Fresh.RuntimeState.IdleState.ActiveChapter, 0);
TestEqual(TEXT("v15 chapter three progress restores chapters one and two"), ChapterThree.RuntimeState.IdleState.UnlockedChapters.Num(), 2);
TestFalse(TEXT("migration waits for a trusted load-time baseline"), ChapterThree.RuntimeState.IdleState.bSettlementClockInitialized);
TestEqual(TEXT("v15 starts with twenty slots"), Migrated.RuntimeState.IdleState.ChestCapacity, 20);
TestEqual(TEXT("migration does not manufacture rewards"), Migrated.RuntimeState.IdleState.ChestBatches.Num(), 0);
```

- [ ] **Step 2: 冷 UBT 亲眼确认 RED**

预期：仅缺 `GameXXKIdleTypes.h`、`IdleState` 或 v16 常量；生产代码未改。

- [ ] **Step 3: 加类型、v16 分支和验证**

在 `ValidateRuntimeState` 追加精确约束：schema=1；chapter 为 0 或 unlocked；容量=`20+level*5` 且 20..80；`PendingLedger` 批次数不大于容量（Inventory 批次不占历练容量）；余额/冷却/游标/序号非负；ChestId/Sequence 唯一；批次来源章有效；时间游标不小于零；Pending 值不溢出 int32 可领取边界；两项 accrual remainder 均在 `[0, 3600*FixedScale)`。v15 迁移按上面的已完成章映射恢复资格；v16 current save 不做隐式修复，只验证。加载一个 `bSettlementClockInitialized=false` 的已解锁状态时，Subsystem 用注入 TimeSource 把唯一共享 `LastSettlementTimeUtcTicks` 原子设为 now、刷新效率快照并保存，首次基线收益严格为0；写盘失败则仍未初始化。

- [ ] **Step 4: 冷 UBT + 精确 GREEN**

```powershell
& $EditorCmd $Project -unattended -nop4 -nosplash -NullRHI -ExecCmds="Automation RunTests GameXXK.MVP.Idle.SaveMigration;Quit" -ReportExportPath="$ReportRoot\IdleCore_Task1_GREEN"
```

预期：新叶与所有迁移邻测成功、0 failed/notRun/errors。

- [ ] **Step 5: 提交**

```powershell
git add Source/GameXXK/Public/Idle/GameXXKIdleTypes.h Source/GameXXK/Public/GameXXKMVPRules.h Source/GameXXK/Public/MVP/GameXXKSaveMigration.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp Source/GameXXK/Private/Tests/GameXXKIdleSaveMigrationTest.cpp Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp Source/GameXXK/Private/Tests/GameXXKHeroCardUnlockMigrationTest.cpp Source/GameXXK/Private/Tests/GameXXKRouteCardEntriesSaveMigrationTest.cpp Source/GameXXK/Private/Tests/GameXXKRouteEconomySaveMigrationTest.cpp
git commit -m "feat: add idle save state version sixteen"
```

## Task 2: 时间源、目录与离线经济 RED/GREEN

**Files:** IdleTimeSource、IdleCatalog、IdleSettlementRules、`GameXXKIdleSettlementRulesTest.cpp`。

- [ ] **Step 1: 写 fake time 与基础经济 RED**

测试使用固定 ticks，不等待真实时间：

```cpp
FGameXXKIdleSettlementRequest Request;
Request.NowUtcTicks = FDateTime(2026, 8, 13, 12).GetTicks();
Request.State = MakeIdleStateAt(Request.NowUtcTicks - ETimespan::FromHours(2).GetTicks());
Request.State.Efficiency.GoldPerHourFixed = 10 * GameXXKIdleFixedScale;
Request.State.Efficiency.ExperiencePerHourFixed = 7 * GameXXKIdleFixedScale + 500000;
```

断言 2 小时得到金币20、经验15，经验余量0；倒退时收益0、游标不后退、`bClockRollbackObserved=true`；超过7天只消费604800秒。

- [ ] **Step 2: 精确 RED**

预期：缺 `FGameXXKIdleSettlementRules::SettleOffline`。

- [ ] **Step 3: 实现时间源与经济定点算法**

```cpp
class GAMEXXK_API IGameXXKIdleTimeSource
{
public:
    virtual ~IGameXXKIdleTimeSource() = default;
    virtual int64 NowUtcTicks() const = 0;
};

class GAMEXXK_API FGameXXKLocalUtcTimeSource final : public IGameXXKIdleTimeSource
{
public:
    virtual int64 NowUtcTicks() const override;
};
```

MSVC/Win64 不依赖编译器私有 128 位整数。新增 `CheckedAccumulatePerHourFixed`：先验证 `RateFixed <= (MAX_int64-PriorRemainder)/EligibleSeconds`，再计算 `Numerator=RateFixed*EligibleSeconds+PriorRemainder`、`WholeReward=Numerator/(3600*FixedScale)`、`Remainder=Numerator%(3600*FixedScale)`；配置加载和 current-save validation 同时锁定该乘法上界。先用总 elapsed 推进共享游标，再分别按各自 `EligibleSecondsUsed` 裁到7天；分段测试逐项锁定 numerator remainder，不能以浮点近似代替。

- [ ] **Step 4: 实现三章目录**

`FGameXXKIdleCatalog::TryGetChapterConfig(int32, FGameXXKIdleChapterConfig&)` 只接受 1..3。首版数字集中在目录并明确标注“初始观测值”，规则测试用测试注入 config，不把平衡数字锁死在 solver。

- [ ] **Step 5: 精确 GREEN 与分段 mutation**

先跑 `GameXXK.Idle.Settlement.EconomyAndClock`。然后临时丢弃余量，确认 `SegmentEquivalence` 失败，再恢复。

- [ ] **Step 6: 提交**

```powershell
git add Source/GameXXK/Public/Idle Source/GameXXK/Private/Idle Source/GameXXK/Private/Tests/GameXXKIdleSettlementRulesTest.cpp
git commit -m "feat: add deterministic idle settlement core"
```

## Task 3: 两类离线宝箱 fluid solver

**Files:** IdleSettlementRules、IdleTypes、Idle tests。

- [ ] **Step 1: 写双冷却和分段等价 RED**

测试注入 normal CD=240s、boss CD=360s、两类期望成功速率；对同一 12 小时输入比较一次结算与 144×5分钟，逐字段比较：箱序列、类型、来源章、固定 seed、双余额、双冷却、NextSequence、经济余量和游标。

- [ ] **Step 2: 写容量中途填满 RED**

从19/20开始，构造两个同刻事件，断言固定优先级 Normal 入队、容量达到20；Boss 冷却/余额冻结在精确填满时刻，而金币经验结算到终点。再次结算不得补发 Boss。

- [ ] **Step 3: 实现事件时间合并**

不要逐秒循环。为每种箱计算“冷却结束→余额到1”的下一事件 ticks，选择最早事件；同刻 Normal 优先；最多循环 `RemainingCapacity<=80` 次。箱生成调用统一 helper：

```cpp
static bool AppendChest(
    FGameXXKIdleState& State,
    EGameXXKIdleChestType Type,
    int32 SourceChapter,
    int64 EventUtcTicks,
    FString* OutError);
```

ChestId/seed 使用固定哈希 `{RootSeed, Sequence, Type, Chapter, LootProfileVersion}`；生成后立即递增 sequence。

- [ ] **Step 4: 时间复杂度门禁**

7天极高速输入最多创建80个实例，测试使用计数 seam 断言 solver event iterations `<=80+4`，禁止按秒/怪逐个循环。

- [ ] **Step 5: GREEN、mutation、提交**

临时把同刻顺序改为 Boss 优先，精确测试必须 RED；恢复后提交：

```powershell
git commit -am "feat: settle offline idle chest expectations"
```

## Task 4: 通关解锁、章节/效率切换

**Files:** GameXXKMVPRules.cpp、Idle rules/catalog、route tests、idle tests。

- [ ] **Step 1: 写 Boss 胜利解锁 RED**

章1/2 Boss clear 分别加入 `UnlockedChapters`；失败/放弃不加入；章3终局保留三章。历练不能解锁下一章。

- [ ] **Step 2: 写切章先结旧快照 RED**

旧章 gold=10/h，新章=100/h，在 t=1h 切换、t=2h结算，结果必须110而非200；旧章生成的箱 `SourceChapter=old`。

- [ ] **Step 3: 实现纯切换 API**

```cpp
static bool SwitchChapter(
    FGameXXKIdleState& InOutState,
    int32 NewChapter,
    int64 NowUtcTicks,
    const FGameXXKIdleEfficiencySnapshot& NewEfficiency,
    FGameXXKIdleSettlementResult& OutResult,
    FString* OutError = nullptr);
```

未解锁章直接失败且输入不变；效率/装备/队伍变化使用同一 `SettleThenReplaceEfficiency`，不能另写捷径。

- [ ] **Step 4: 运行邻接 Route 测试并提交**

```powershell
git add Source/GameXXK/Private/GameXXKMVPRules.cpp Source/GameXXK/Private/Idle Source/GameXXK/Private/Tests/GameXXKIdleSettlementRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKThreeChapterRouteTest.cpp Source/GameXXK/Private/Tests/GameXXKRouteSettlementTest.cpp
git commit -m "feat: unlock idle chapters from route clears"
```

## Task 5: Repository 原子事务与 Subsystem 门面

**Files:** IdleRepository、MVPSubsystem header/cpp、SaveGame tests、idle transaction tests。

- [ ] **Step 1: 写保存失败 RED**

通过现有 `FGameXXKSaveSlotWriteDelegate` 强制 write=false。调用 `SettleIdleToNow` 后断言整个 `FGameXXKRuntimeState` 用 `CompareScriptStruct` 字节语义不变，游标/奖励/箱也不变。

- [ ] **Step 2: 增加当前 slot 上下文**

Subsystem 成功 Start/Load 后记录 `ActiveSaveSlotName`、`ActiveSaveUserIndex`；只在当前会话有效，不额外保存。历练事务如果没有活跃 slot 返回明确失败，禁止悄悄只改内存。`IGameXXKIdleRepository` 暴露 `Commit(const FGameXXKRuntimeState& Candidate, FString& OutError)`；本包的 `FGameXXKLocalIdleRepository` 只包现有 slot write delegate，未来远端实现只能替换 repository，不能旁路规则。

- [ ] **Step 3: 实现统一事务 helper**

```cpp
bool UGameXXKMVPSubsystem::CommitRuntimeCandidate(
    FGameXXKRuntimeState&& Candidate,
    const FString& OperationName)
{
    UGameXXKSaveGame* Save = Cast<UGameXXKSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UGameXXKSaveGame::StaticClass()));
    if (!Save) return false;
    Save->SaveState = UGameXXKMVPRules::MakeSaveState(Candidate);
    if (!WriteSaveGameToSlot(Save, ActiveSaveSlotName, ActiveSaveUserIndex)) return false;
    BeginRuntimeStateMutation(BattleHudFixtureView);
    RuntimeState = MoveTemp(Candidate);
    return true;
}
```

公开门面：`SettleIdleToNow`、`SwitchIdleChapter`、`RefreshIdleEfficiency`、`UpgradeIdleCapacity`。升级成本从 `FGameXXKIdleCatalog::GetCapacityUpgradeGoldCost(level)` 读取；验证成功才扣金并保存。

- [ ] **Step 4: 失败/成功 GREEN**

覆盖：创建 SaveGame 失败 seam、write false、非法状态、成功写盘再重载；成功后磁盘与内存 `IdleState` 相等。

- [ ] **Step 5: 提交**

```powershell
git add Source/GameXXK/Public/Idle/GameXXKIdleRepository.h Source/GameXXK/Private/Idle/GameXXKIdleRepository.cpp Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp Source/GameXXK/Private/Tests/GameXXKIdleSettlementRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp
git commit -m "feat: commit idle settlement atomically"
```

## Task 6: 本包最终验证

- [ ] **Step 1: 冷 UBT** — 退出0、`Result: Succeeded`。
- [ ] **Step 2: 精确组**

```powershell
GameXXK.Idle.Settlement
GameXXK.MVP.Idle.SaveMigration
GameXXK.MVP.SaveGame
GameXXK.Equipment.SaveMigration
GameXXK.Route
```

- [ ] **Step 3: 长时确定性**

执行 7 天一次、10080×1分钟、时间回退、19/20/80容量、章切换、overflow边界；输出 JSON 字段必须逐项相等。

- [ ] **Step 4: 文档证据**

创建 `docs/production/2026-08-13-idle-core-save-status.md`，记录 RED、mutation、最终报告、v15→v16 矩阵和已知非失败 warning。

- [ ] **Step 5: 最终提交与 push**

```powershell
git add docs/production/2026-08-13-idle-core-save-status.md
git commit -m "docs: certify idle core save migration"
git push origin main
```

本包完成后仍不新增界面、不生成在线随机掉落、不打开箱子、不切默认入口。
