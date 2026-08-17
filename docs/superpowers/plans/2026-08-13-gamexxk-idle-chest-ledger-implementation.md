---
status: shelved
updated_at: 2026-08-17
shelved_reason: legacy migration package; do not execute
superseded_by: docs/superpowers/specs/2026-08-17-gamexxk-desktop-training-workbench-design.md
---
# GameXXK Idle Chest Ledger Implementation Plan

> 执行冻结：当前 `CurrentSaveVersion=17`，本文的 v15/v16/v17/v18 迁移边界不能直接执行。恢复历练实现必须以 `2026-08-17-gamexxk-desktop-training-workbench-design.md` 和新的 Phase 0 基线重新编排迁移编号。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 v16 历练状态上实现真实在线刷怪时间线、双冷却随机掉箱、待领奖励账本、两类宝箱 FIFO、固定种子展开和背包不足时的可恢复部分领取。

**Architecture:** 在线 scheduler 只生成稳定怪物死亡事件，`OnlineDropResolver` 只消费事件并写同一 `IdleState`；`RewardLedgerRules` 和 `ChestResolverRules` 在完整 RuntimeState 副本上完成货币、批次、背包及装备事务。宝箱批次数组是权威，背包 `Inventory` 只保存两种箱的数量镜像，所有 Subsystem 命令都通过包 1 的原子 repository 提交。

**Tech Stack:** UE 5.8 C++、定点时间线、稳定哈希/FRandomStream、GameXXK EquipmentRules/Inventory、UE Automation、UBT、统计模拟测试。

---

## 0. 文件结构

**Create:**

- `Public/Idle/GameXXKIdleEncounterScheduler.h`、`Private/Idle/GameXXKIdleEncounterScheduler.cpp`
- `Public/Idle/GameXXKIdleDropRules.h`、`Private/Idle/GameXXKIdleDropRules.cpp`
- `Public/Idle/GameXXKIdleRewardLedgerRules.h`、`Private/Idle/GameXXKIdleRewardLedgerRules.cpp`
- `Public/Idle/GameXXKIdleChestResolver.h`、`Private/Idle/GameXXKIdleChestResolver.cpp`
- `Private/Tests/GameXXKIdleOnlineDropTest.cpp`
- `Private/Tests/GameXXKIdleRewardLedgerTest.cpp`
- `Private/Tests/GameXXKIdleChestResolverTest.cpp`
- `Private/Tests/GameXXKIdleOnlineOfflineParityTest.cpp`

**Modify:**

- `Public/Idle/GameXXKIdleTypes.h` — monster stream ordinal、transaction result；scheduler 复用核心包唯一共享时间游标。
- `Public/Idle/GameXXKIdleCatalog.h`、private cpp — 四项掉落系数与 loot profile。
- `Public/MVP/GameXXKMVPSubsystem.h`、private cpp — Tick/kill/claim/open commands。
- `Public/GameXXKMVPRules.h`、private cpp — 两种箱 item defs，仅兼容镜像。
- `Public/UI/GameXXKInventoryWindowWidget.h`、private cpp — 在现有 200 格/4×5 布局中显示两类箱与打开动作，禁止改布局常量。
- `Private/Tests/GameXXKInventoryEnhancementTest.cpp`、`GameXXKFinalInventoryWidgetTest.cpp` — 布局与现有功能邻测。

新增字段：

```cpp
UPROPERTY(SaveGame) int64 NextMonsterEventOrdinal = 0;
UPROPERTY(SaveGame) int32 MonsterRandomRootSeed = 1;
```

禁止新增第二个在线时间游标。`Advance` 的输入起点和提交终点都使用 `IdleState.LastSettlementTimeUtcTicks`；切在线/离线、金币经验和宝箱 scheduler 必须在同一个 candidate transaction 中消费同一段时间。

规则结果统一：

```cpp
struct FGameXXKIdleTransactionResult
{
    bool bSucceeded = false;
    int64 GoldDelta = 0;
    int64 ExperienceDelta = 0;
    TArray<FName> CreatedChestIds;
    TArray<FName> MovedChestIds;
    TArray<FName> CompletedChestIds;
    FString Error;
};
```

## Task 1: 锁定在线稳定时间线

**Files:** scheduler 四文件、online test、IdleTypes。

- [ ] **Step 1: 写 RED**

同一状态分别以 `60×1s`、`600×0.1s`、一次60s推进，断言事件时间、ordinal、Normal/Elite 序列和最终 `LastSettlementTimeUtcTicks` 全相同；窗口最小化、UI页签和表现速度不是请求参数。

- [ ] **Step 2: 实现纯 scheduler**

```cpp
struct FGameXXKIdleMonsterDeathEvent
{
    int64 EventUtcTicks = 0;
    int64 EventOrdinal = 0;
    EGameXXKIdleMonsterKind MonsterKind = EGameXXKIdleMonsterKind::Normal;
};

static bool Advance(
    FGameXXKIdleState& InOutState,
    int64 EndUtcTicks,
    TArray<FGameXXKIdleMonsterDeathEvent>& OutEvents,
    FString* OutError = nullptr);
```

用总 kills/hour 的定点间隔计算下一事件，不按帧累计浮点；MonsterKind 用 `{MonsterRootSeed, ordinal, chapter, config}` 稳定派生。每次只推进到 End，不能改 reward ledger。Drop resolver 消费时间线时，先用“上一事件/游标→当前事件”的 elapsed 同步递减两类 cooldown，再处理本次死亡；消费完最后一个事件后再递减到 End。若某个掉落使容量刚好填满，则从该事件时刻起的剩余区间不再递减 cooldown，满仓期间不隐藏蓄积 ready 时间。

- [ ] **Step 3: 模式切换边界**

Offline→Online 先调用包1 settle 到 now，共享游标已是 now；Online→Offline 先消费所有 `<=now` 的死亡事件、经济和掉落并把共享游标保存为 now，再切模式。保存失败整体回滚。

- [ ] **Step 4: mutation + GREEN + commit**

临时用 DeltaSeconds 累加，分帧测试必须 RED；恢复后：

```powershell
git commit -m "feat: add deterministic online idle timeline"
```

## Task 2: 双冷却与四系数在线掉箱

**Files:** DropRules、Catalog、online test。

- [ ] **Step 1: 写规则 RED**

覆盖：Normal CD 240s、Boss CD 360s；同一次 kill 可掉两种；普通/精英分别使用四系数；失败保持 ready；成功只重置自己的 cooldown；每次真实尝试推进对应 ordinal；容量满时两者不尝试、不减 cooldown。

- [ ] **Step 2: 注入确定 roll**

测试不猜 seed，允许规则接受 `FGameXXKIdleDropConfig`，设置 0 或 `FixedScale` 概率精确制造失败/成功。生产目录概率范围 `[0, FixedScale]`。

- [ ] **Step 3: 实现 resolver**

```cpp
static bool ResolveDeath(
    FGameXXKIdleState& InOutState,
    const FGameXXKIdleMonsterDeathEvent& Event,
    const FGameXXKIdleDropConfig& Config,
    FGameXXKIdleTransactionResult& OutResult,
    FString* OutError = nullptr);
```

先把两类 eligible/roll 都算入候选，再按 Normal→Boss 生成箱；第一个成功填满容量时第二个不生成但其 roll ordinal 是否推进必须固定：同一死亡事件先对进入判定的 ready 类型推进尝试 ordinal，再按容量顺序提交，防止通过1空格重随机第二类。测试锁定该语义。

- [ ] **Step 4: 原子保存门禁**

Subsystem `AdvanceOnlineIdleToNow` 在 Candidate 上 scheduler→drop→Validate→Save→commit；保存失败不播放掉落、不推进 ordinal/cursor。

- [ ] **Step 5: GREEN、mutation、commit**

临时让失败重置 cooldown，测试 RED；恢复提交：

```powershell
git commit -m "feat: resolve independent online idle drops"
```

## Task 3: 待领奖励与容量恢复

**Files:** LedgerRules、Subsystem、ledger test。

- [ ] **Step 1: 写金币/经验独立领取 RED**

PendingGold/XP 都有值，箱背包拒绝：领取仍加入 PlayerGold/XP；只领取金币只重置 `GoldEligibleSecondsUsed` 与金币 claim 账，经验保持；overflow 失败原子回滚。

- [ ] **Step 2: 写箱 FIFO 领取 RED**

按 `Sequence` 转移 PendingLedger→Inventory；同类 UI 可堆叠但顺序不变；拒绝较早批次时不能跳过它转移同类型后来批次。不同类型各自 FIFO，不要求 Normal 阻塞 Boss。新增 `FGameXXKIdleBackpackAcceptanceRules::CanAcceptChestStack`：当前权威容量为200个逻辑显示栈，已存在同类箱栈只增加数量不占新格；不存在时用“非零普通物品栈 + 装备实例 + 已存在箱栈”计算是否还有1格。只在箱领取路径使用该规则，不顺手改写其他既有掉落规则。

- [ ] **Step 3: 实现镜像同步**

```cpp
static void SynchronizeChestInventoryMirrors(FGameXXKRuntimeState& State)
{
    State.Inventory.FindOrAdd(UGameXXKMVPRules::ItemNormalIdleChest()) = CountInventoryBatches(State, Normal);
    State.Inventory.FindOrAdd(UGameXXKMVPRules::ItemBossIdleChest()) = CountInventoryBatches(State, Boss);
}
```

任何 AddItem/RemoveItem 对这两个 ID 必须拒绝，避免绕过批次权威。

- [ ] **Step 4: 容量释放时间语义**

领取成功且 `PendingLedger` 数量从 full 降到 below-capacity：双 cooldown从候选事务捕获并成功写盘的 `NowUtcTicks` 恢复；共享 settlement游标同时推进到该时刻，满仓时间不补算，offline balances不变。Inventory container 中尚未开启/部分开启的批次不重新占历练容量。保存失败仍 full。

- [ ] **Step 5: GREEN、save-failure mutation、commit**

```powershell
git commit -m "feat: claim idle reward ledger atomically"
```

## Task 4: 固定种子开箱与部分领取

**Files:** ChestResolver、EquipmentRules integration、chest tests。

- [ ] **Step 1: 写同 seed 稳定 RED**

复制同一 RuntimeState 两次打开，完整 `RemainingRewards`、装备请求 seed、货币结果相同；切活动章节后打开旧箱仍使用 `SourceChapter`；旧 LootProfileVersion 用对应 profile。

- [ ] **Step 2: 定义首版奖励 entry**

只使用现有可接收资产：`Gold`、`Experience`、`Item.*`、`EquipmentRoll`。`FGameXXKIdleRewardEntry::Kind` 决定解释方式，禁止靠 RewardId 字符串前缀猜类型。装备 entry 在首次展开时固定 set/slot/quality/item-level/roll seed；仍只能使用当前三档 `EGameXXKEquipmentQuality`，十档装备不是本包范围。

- [ ] **Step 3: 实现一次展开**

```cpp
static bool EnsureExpanded(
    FGameXXKIdleChestBatch& Batch,
    const FGameXXKIdleLootProfile& Profile,
    FString* OutError);
```

只有 `bRewardsExpanded=false` 时按 FixedLootSeed 生成并保存清单；后续绝不调用 RNG。损坏/未知 profile 返回失败，不删除箱。

- [ ] **Step 4: 实现部分领取**

固定顺序遍历全部奖励：金币检查 int32 overflow；经验调用从现有 `GameXXKMVP::ApplyXP` 提取出的公共 `UGameXXKMVPRules::GrantPlayerExperience`（同一升级/上限语义），不能直接写 PlayerXP；材料走 AddItem；装备先检查 `HasWarehouseCapacity`。为 `FGameXXKEquipmentCreateRequest` 增加 `bUseRollSeedOverride/RandomSeedOverride`，`CreateRolledInstance` 在 true 时只用已保存 seed 生成 slot/affix，InstanceId仍由权威 collection ordinal 分配；这样部分领取期间即使玩家获得了别的装备，重试仍得到同一属性。某装备失败时保留该 entry，但继续尝试后续可堆叠材料/货币；不得越过未清空箱打开下一箱。

- [ ] **Step 5: 写背包不足 RED/GREEN**

仓库200满、箱内 `[equipment, gold, material]`：equipment 保留，gold/material 领取；保存后箱仍为同一个 FIFO head；腾出仓库再开只交付剩余 equipment，结果 seed/affix 不变。

- [ ] **Step 6: 读档重随机 mutation**

临时忽略 `bRewardsExpanded` 重建，精确测试必须 RED；恢复提交：

```powershell
git commit -m "feat: open seeded idle chests with partial delivery"
```

## Task 5: 接入现有背包模板但不改布局

**Files:** InventoryWindow header/cpp、inventory tests。

- [ ] **Step 1: 快照当前布局 RED 基线**

先在现有测试锁定 `BackpackStorageCapacity=200`、4列、20可见格、窗口框/tooltip/slot资源路径与关键 Canvas geometry，改动后必须完全相等。

- [ ] **Step 2: 构造两种显示栈**

`RefreshBackpackSlots` 从批次数组追加两个 `FBackpackRuntimeEntry`：数量为 Inventory container 对应批次总数，DisplayName 为“普通怪物宝箱”/“首领宝箱”，Detail 显示最早批次来源章和“按获得顺序开启”。不把各批次占200个格。

- [ ] **Step 3: 主动作打开 FIFO head**

选择箱 stack 后 `PrimaryActionText=开启`，确认调用 Subsystem `OpenNextIdleChest(type, now)`；部分领取后刷新 detail 显示剩余条目数；错误保持原显示并用现有 ActionErrorText。

- [ ] **Step 4: UI 测试**

锁定两 stack、数量、最早来源、打开后数量/余量、现有装备/材料排序与交互不回归；没有新页签或布局移动。

- [ ] **Step 5: 提交**

```powershell
git commit -m "feat: show idle chest batches in existing backpack"
```

## Task 6: 在线/离线长期评估与最终门禁

- [ ] **Step 1: Monte Carlo 测试**

每组配置至少100,000在线死亡事件，比较长期箱/小时与 offline fluid expectation；报告每章、队伍档、monster mix、两类型偏差，容差使用置信区间，不要求短期逐箱相等。

- [ ] **Step 2: 频繁切换防套利**

1h持续 offline、60次online/offline切换、每分钟登录三条路径：金币/XP、冷却、offline balance、attempt ordinals 和 ledger 无重复时段；随机结果允许不同但总期望不因切换系统性增加。

- [ ] **Step 3: 冷 UBT 与测试组**

```text
GameXXK.Idle.Online
GameXXK.Idle.Ledger
GameXXK.Idle.Chest
GameXXK.Idle.Parity
GameXXK.MVP.Inventory
GameXXK.Equipment
GameXXK.MVP.SaveGame
```

- [ ] **Step 4: production status**

创建 `docs/production/2026-08-13-idle-chest-ledger-status.md`，记录精确 RED/GREEN、统计样本和偏差，不宣称已完成十档装备或 Steam 服务。

- [ ] **Step 5: push**

```powershell
git push origin main
git fetch origin main
```

本包结束后只有规则和现有背包接入；完整历练主界面与迷你窗仍未实现。
