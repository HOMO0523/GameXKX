# Sorcerer Partner Runtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. In this project, work directly on `main`; do not create a worktree. The user has deferred every commit until the complete 198-card review, so do not stage or commit during these tasks.

**Goal:** Replace the permanent Sorcerer partner's old 18-card catalog with the approved five-card sequencing system, make every base/sequence/reward effect execute deterministically, and prove search, 20-card overflow, replay, save/resume, targeting, and regression behavior without changing the confirmed UI layout.

**Architecture:** Keep all existing `Profession.Sorcerer.*` CardIds for save compatibility. Add Sorcerer-specific declarative rule metadata to `FGameXXKCardDefinition`, one owner-scoped persisted five-card task runtime, immutable first-play sequence metadata on replay snapshots, and a generic saved overflow-hand zone. Extend the existing automatic resolution queue rather than creating a second resolver: active plays alone record progress; automatic actions resolve saved snapshots, can pause on choices, and clear only the task that owns the completed queue. Runtime dispatch selects data enums, never display names or UI indices.

**Tech Stack:** Unreal Engine 5.8 C++, USTRUCT/UPROPERTY SaveGame state, UE Automation tests, UBT command-line builds, existing `FGameXXKCardCatalog` and `GameXXKCardRules` pure runtime.

**Authoritative specification:** `docs/superpowers/specs/2026-08-11-sorcerer-partner-card-pool-design.md`

---

## 0. Non-negotiable invariants

- [ ] Do not change widget hierarchy, anchors, slots, page layout, target-selection UI, or art assets.
- [ ] Preserve all 18 current `Profession.Sorcerer.*` CardIds and four existing archetype tags.
- [ ] Keep protagonist eight-card and named-NPC three-card task behavior byte-for-byte compatible except where shared queue plumbing must be generalized.
- [ ] Only `ActivePlay` may pay costs, count a played card, advance a task, trigger equipment card-count listeners, or consume active-play modifiers.
- [ ] Every automatic replay uses the first-play snapshot's quality, owner, target fallback, sequence position, previous family, actual paid Mana, and locked branch.
- [ ] Enemy-directed Sorcerer effects use `AllEnemies`; self resource/armor effects use `CardOwner`. No new manual target page is introduced.
- [ ] Hand capacity remains 20. Overflowed automatic additions are moved into a persisted queue zone and are neither duplicated nor lost.
- [ ] Use cold UBT only: `-NoHotReload -NoHotReloadFromIDE`.
- [ ] Do not stage or commit until the later 198-card audit is complete.

## Task 1: Lock the exact 18-card catalog in a failing test

**Files:**

- Create: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerCatalogTest.cpp`
- Modify after RED: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Modify after RED: `Source/GameXXK/Public/GameXXKCardTypes.h`

### 1.1 Write the RED table test

Create one row per stable CardId with exact display name, base Energy, base Mana, target mode, core flag, family, sequence rule, reward rule, and archetype. The row type must make omissions a compile-time-visible failure:

```cpp
struct FSorcererExpectedCard
{
    const TCHAR* CardId;
    const TCHAR* DisplayName;
    int32 Energy;
    int32 Mana;
    EGameXXKCardTargetMode TargetMode;
    bool bCore;
    EGameXXKSorcererCardFamily Family;
    EGameXXKSorcererSequenceRule SequenceRule;
    EGameXXKSorcererRewardRule RewardRule;
    const TCHAR* ArchetypeId;
};
```

The 18 rows must match the authoritative spec, including Fire Mana `1/2/4/2`, Lightning Mana `1/2/3/4`, all four Universal Energy costs `0`, Ice base cards with no direct-damage effect, and all enemy offense using `AllEnemies`.

Also assert:

```cpp
TestEqual(TEXT("exact Sorcerer count"), SorcererCards.Num(), 18);
TestEqual(TEXT("exact core count"), CoreCount, 2);
TestEqual(TEXT("four Fire cards"), FireCount, 4);
TestEqual(TEXT("four Ice cards"), IceCount, 4);
TestEqual(TEXT("four Lightning cards"), LightningCount, 4);
TestEqual(TEXT("four Universal cards"), UniversalCount, 4);
```

### 1.2 Observe the intended RED

Run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload -NoHotReloadFromIDE
```

Expected first RED: compile errors only for missing `EGameXXKSorcererCardFamily`, `EGameXXKSorcererSequenceRule`, `EGameXXKSorcererRewardRule`, and Sorcerer rule fields. Record the full log under `Saved/Automation/SorcererPartnerCatalog_RED_20260811/`.

### 1.3 Add the minimum declarative schema

Add these data selectors to `GameXXKCardTypes.h`:

```cpp
UENUM(BlueprintType)
enum class EGameXXKSorcererCardFamily : uint8
{
    None = 0,
    Core = 1,
    Fire = 2,
    Ice = 3,
    Lightning = 4,
    Universal = 5
};

UENUM(BlueprintType)
enum class EGameXXKSorcererSequenceRule : uint8
{
    None = 0,
    CoreSearch = 1,
    CoreManaEcho = 2,
    FireLamp = 3,
    FireSpread = 4,
    FireBurst = 5,
    FireSearch = 6,
    IceCurrentManaRestore = 7,
    IceMaxMana = 8,
    IceArmorDouble = 9,
    IceSearch = 10,
    LightningMark = 11,
    LightningSearch = 12,
    LightningMarkHits = 13,
    LightningStorm = 14,
    UniversalScalingAttack = 15,
    UniversalDraw = 16,
    UniversalPartyArmor = 17,
    UniversalSearch = 18
};

UENUM(BlueprintType)
enum class EGameXXKSorcererRewardRule : uint8
{
    None = 0,
    CoreSearch = 1,
    CoreManaEcho = 2,
    FireLamp = 3,
    FireSpread = 4,
    FireBurst = 5,
    FireSearch = 6,
    IceCurrentManaRestore = 7,
    IceMaxMana = 8,
    IceArmorDouble = 9,
    IceSearch = 10,
    LightningMark = 11,
    LightningSearch = 12,
    LightningMarkHits = 13,
    LightningStorm = 14,
    UniversalScalingAttack = 15,
    UniversalDraw = 16,
    UniversalPartyArmor = 17,
    UniversalSearch = 18
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKSorcererCardRule
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    EGameXXKSorcererCardFamily Family = EGameXXKSorcererCardFamily::None;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    EGameXXKSorcererSequenceRule SequenceRule = EGameXXKSorcererSequenceRule::None;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    EGameXXKSorcererRewardRule RewardRule = EGameXXKSorcererRewardRule::None;
};
```

Add `FGameXXKSorcererCardRule SorcererRule;` to `FGameXXKCardDefinition`, extend the `AddCard` helper with one trailing default argument, and replace only `AddSorcererCards` using the stable-ID map in the specification.

### 1.4 Verify catalog GREEN

Run cold UBT, then exact Automation prefix:

```text
GameXXK.Data.PartnerCards.Sorcerer.Catalog
```

Require 1/1 Success, 0 failed, 0 errors. Existing unrelated warnings are not counted as pass evidence.

## Task 2: Persist a five-card task and immutable replay metadata

**Files:**

- Create: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerTaskStateTest.cpp`
- Modify after RED: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify after RED: `Source/GameXXK/Private/GameXXKCardRules.cpp`

### 2.1 Write structural and validation RED tests

The tests construct an active task, validate it, serialize with `FGameXXKCardBattleRuntime::StaticStruct()->SerializeItem`, deserialize, and compare every field. Required snapshot additions:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
int32 PaidManaCost = 0;

UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
int32 SorcererSequencePosition = 0;

UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
EGameXXKSorcererCardFamily PreviousSorcererFamily = EGameXXKSorcererCardFamily::None;

UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
EGameXXKSorcererTaskBranch SorcererTaskBranch = EGameXXKSorcererTaskBranch::None;
```

Use a dedicated branch enum so the protagonist reward enum is never overloaded:

```cpp
UENUM(BlueprintType)
enum class EGameXXKSorcererTaskBranch : uint8
{
    None = 0,
    Normal = 1,
    Fire = 2,
    Ice = 3,
    Lightning = 4
};
```

The starter reward remains the per-card `EGameXXKSorcererRewardRule`.

Add an owner-scoped state:

```cpp
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKSorcererPartnerTaskRuntime
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
    bool bActive = false;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
    FName OwnerUnitId = NAME_None;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
    TArray<FName> LockedCardIds;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
    TArray<FName> CompletedCardIds;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
    TArray<FGameXXKResolvedCardSnapshot> FirstPlayOrder;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
    EGameXXKSorcererRewardRule StarterReward = EGameXXKSorcererRewardRule::None;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
    EGameXXKSorcererTaskBranch LockedBranch = EGameXXKSorcererTaskBranch::None;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
    TArray<FName> AutoHandedUniversalCardIds;
};
```

Store an array on `FGameXXKCardBattleRuntime` so owner isolation is explicit. Inactive entries may retain only `OwnerUnitId` and the per-battle universal auto-hand history; all progress fields must be empty/default.

Validation RED cases must reject duplicate locked IDs, anything other than exactly five locked IDs while active, a completed ID outside the lock, duplicate first snapshots, non-Sorcerer definitions, owner mismatch, sequence positions not exactly `1..N`, impossible previous-family links, branch before the universal starter's second card, and a completed task without exactly five snapshots.

### 2.2 Implement validation only after RED

Add `ValidateSorcererPartnerTaskRuntimes` adjacent to existing Hero/NPC validators. It must never infer the five-card loadout from draw/discard order; task creation collects the owner's five distinct non-temporary active deck instances.

### 2.3 Verify GREEN

Require cold UBT plus:

```text
GameXXK.Data.PartnerCards.Sorcerer.TaskState
```

The prefix must include valid round-trip, owner isolation, and all malformed-state rejections.

## Task 3: Record active plays and complete the five-card replay lifecycle

**Files:**

- Create: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerTaskLifecycleTest.cpp`
- Modify after RED: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify after RED: `Source/GameXXK/Private/GameXXKCardRules.cpp`

### 3.1 Write lifecycle RED tests

Use five carried Sorcerer instances plus unrelated Hero/NPC/temporary copies. Assert:

1. First active carried Sorcerer card starts and records position 1.
2. Duplicate active play resolves base but does not advance and receives no sequence enhancement.
3. Temporary copies, other owners, automatic replay, task reward, Heavy Arrow, reaction, and terrain origins do not advance.
4. Actual paid Mana is recorded after preview modifiers, never the catalog base cost.
5. Five distinct active cards enqueue exactly five base replays followed by one starter reward.
6. Replays use the recorded order, quality, target fallback, paid Mana, position, and prior family.
7. Completion clears only this Sorcerer owner's progress; it does not clear Hero or NPC tasks.
8. The same Sorcerer can start a second task in the same battle.
9. If a replay opens a choice, the queue remains valid and resumes exactly once after submission.

### 3.2 Extend automatic resolution ownership

Add a distinct origin:

```cpp
PartnerSorcererTaskReplay = 9
```

Extend `FGameXXKAutomaticResolutionQueue` with a Sorcerer pending reward payload and owner. Do not reinterpret `MageTaskReplay`, which already means the protagonist eight-card task. Queue cleanup branches on origin and resets only the owning task state while preserving its `AutoHandedUniversalCardIds`.

Before base resolution in `ResolveCardPlay`, call `RecordSorcererPartnerTaskActivePlay` after payment and after the immutable active snapshot is created. Pass `Preview.EffectiveManaCost` explicitly. Set the branch before the second card's base resolves when the starter is Universal.

### 3.3 Verify lifecycle GREEN and regressions

Run:

```text
GameXXK.Data.PartnerCards.Sorcerer.TaskLifecycle
GameXXK.Data.HeroSpellTask
GameXXK.Data.TaskNpcSpellTask
GameXXK.Data.CardResolutionQueue
```

All four prefixes must have 0 failed and 0 errors.

## Task 4: Add a real 20-card overflow queue and generalized task search

**Files:**

- Create: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerHandQueueTest.cpp`
- Modify after RED: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify after RED: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify after RED only if a public test seam is required: `Source/GameXXK/Public/GameXXKCardRules.h`

### 4.1 RED cases

Assert all of these from full runtime calls:

- Nonstarter Universal moves itself from draw or discard to hand once per battle when a task starts.
- Universal starter resolves its own base, then brings the other four unfinished carried cards to hand.
- Existing hand instances are not copied; already-completed IDs are not moved.
- At hand size 20, requested instances leave draw/discard and enter `PendingAutomaticHandCards`, sorted by acquisition ordinal then instance ID.
- Playing/discarding a card materializes queued cards in stable order until the hand is full.
- A save/load while two instances are queued produces the same zones and subsequent order.
- Search offers only unfinished locked cards belonging to the same owner; submitting a Hero/NPC/Sorcerer stale candidate fails transactionally.
- If no legal search candidate exists, the card's specified fallback effect executes once.

### 4.2 Add the persisted queue zone

Add to `FGameXXKBattleDeckState`:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
TArray<FGameXXKCardInstance> PendingAutomaticHandCards;
```

Treat this as a fifth exclusive deck zone in validation and `ActiveInstanceIds` conservation. Add two internal helpers:

```cpp
bool QueueInstanceForAutomaticHand(FGameXXKBattleDeckState&, FName InstanceId, FString&);
bool MaterializePendingAutomaticHandCards(FGameXXKBattleDeckState&, FString&);
```

The queue helper removes the exact instance from draw/discard, inserts it by `(AcquisitionOrdinal, InstanceId)`, and never accepts a duplicate. Materialization runs after any successful operation that frees a hand slot and before terminal validation.

Generalize the existing `HeroTaskSearchChooseToHand` implementation without changing the pending-choice UI enum: candidate validation may match an active Hero task, active named-NPC task, or active Sorcerer-partner task, but never crosses owner or locked-loadout boundaries.

### 4.3 GREEN and regression prefixes

```text
GameXXK.Data.PartnerCards.Sorcerer.HandQueue
GameXXK.Data.DeckRules
GameXXK.Data.CardResolutionQueue
GameXXK.Data.TaskNpcSpellTask
```

## Task 5: Implement the 18 base effects and locked sequence enhancements

**Files:**

- Create: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerCoreFireRuntimeTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerIceLightningRuntimeTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerUniversalRuntimeTest.cpp`
- Modify after each RED: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify after each RED if the catalog payload is incomplete: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`

### 5.1 Core and Fire RED/GREEN

Test every ordinary and enhanced position boundary, including positions just outside the range:

- `灵枢引法`: group 70%, search; fallback second 70%; position 1/2 searched card Mana -3 this round.
- `周天归元`: +3 Mana plus floor(50% of the previous snapshot's actual paid Mana); replay uses the saved cost.
- `灵火点灯`: group 60% + Burn 2; position 1/2 Burn 4.
- `流焰传薪`: group Burn 1; immediately previous Fire makes it 3.
- `焚脉爆炎`: group 80%; position 3/4/5 adds 10 percentage points per target Burn without consuming Burn.
- `燎原寻诀`: group 40% + search, fallback second 40%; position 4/5 changes both packets to 70%.

### 5.2 Ice RED/GREEN

Ice base cards never deal direct damage. Assert:

- `寒息回流`: restore floor(current Mana ×25%); only an active Ice branch converts overflow 100% to owner armor.
- `玄冰拓脉`: max Mana +4, current Mana unchanged.
- `霜镜叠甲`: armor 0 becomes 4; otherwise doubles, capped at 99.
- `冰鉴索法`: armor floor(current Mana ×25%) and search; no candidate grants the same armor again.
- Universal starter followed by Ice sets the branch before the Ice base, so second-card overflow already becomes armor.

### 5.3 Lightning RED/GREEN

For two enemies with different Mark stacks, assert hit counts, packet multipliers, per-hit +15% Mark bonus, one Mark consumed per hit, and death stopping remaining hits:

- `引雷定标`: group 50% then Mark 2; position 1/2 Mark 3.
- `雷符索敌`: group 70% + search then Mark 1; fallback second 70%; position 1/2 Mark 3.
- `连霆穿云`: schedule one 50% hit per pre-effect target Mark; position 4/5 uses 65%.
- `雷走八方`: schedule one 30% hit per pre-effect target Mark; position 4/5 uses 45%.

New Mark gained by the same card must not increase scheduled hits.

### 5.4 Universal base RED/GREEN

- `万法归一`: group `60/85/110/135/160%` at positions 1..5.
- `照见五蕴`: draw 1; positions 3..5 also +5 Mana.
- `六合护法`: party armor 3; if previous recorded card dealt no direct damage, armor 6.
- `斗转星移`: group 65% + search, fallback second 65%; positions 4/5 make both packets 90%.

Run the three exact prefixes after every small GREEN, not only after the entire task.

## Task 6: Implement starter rewards, including all 16 Universal branches

**Files:**

- Create: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerRewardRuntimeTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerUniversalRewardMatrixTest.cpp`
- Modify after RED: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify after RED: `Source/GameXXK/Private/GameXXKCardRules.cpp`

### 6.1 Direct starter reward matrix

Write one scenario for each of the 14 Core/Fire/Ice/Lightning starters and compare exact HP, armor, Mana, Energy, hand delta, statuses, automatic count, and replay count against the spec.

Critical assertions:

- Fire double/equalize/trigger effects preserve their stated decay semantics.
- Standard Ice reward snapshots and consumes all owner armor, then deals group `100% + 20 percentage points × consumed armor`.
- Ice card-specific rewards occur after the standard Ice group damage in the documented order.
- Lightning reward hit counts use the Mark snapshot after the reward's Mark application and still consume one Mark per hit.
- Dead targets stop receiving scheduled Lightning hits.

### 6.2 Universal 4×4 matrix

For each Universal starter, complete the same five-card loadout in Normal, Fire, Ice, and Lightning branches. Lock all 16 cells in the specification.

`斗转星移` rewards that replay the fifth or last matching-family snapshot must enqueue another automatic replay action with its already-locked sequence metadata. If that replay opens search, save the queue cursor, pause, submit, and resume before running the reward tail. Do not recursively advance the task.

### 6.3 GREEN

```text
GameXXK.Data.PartnerCards.Sorcerer.Rewards
GameXXK.Data.PartnerCards.Sorcerer.UniversalRewards
```

Require every concrete child test to execute; a passing parent prefix that discovers zero matrix children is a failure.

## Task 7: Card text, preview, targeting, and save/resume acceptance

**Files:**

- Create: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerTextTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKSorcererPartnerSaveResumeTest.cpp`
- Modify after RED: `Source/GameXXK/Private/GameXXKCardText.cpp`
- Modify after RED only as required: `Source/GameXXK/Private/GameXXKCardRules.cpp`

### 7.1 Text acceptance

Every one of the 18 cards must render concise text with four ordered clauses when applicable:

1. target and base effect;
2. sequence condition and enhanced effect;
3. starter reward;
4. Universal branch table reference in expanded detail, not a single ambiguous sentence.

Text must call the status `灼烧`, not `燃烧`; call shared resource `气力`; state group enemy/self explicitly; and never expose enum names, stale old card names, or UI implementation terms.

### 7.2 Preview/target acceptance

For every offensive card, `BuildCardPlayPreview` must succeed with no selected enemy ID and return all living enemies in stable order. Self-only cards must auto-lock the owner. Search and fallback must not create an enemy-target prompt.

### 7.3 Mid-task and mid-queue save/resume

Round-trip these states and continue to identical final hashes:

- two of five completed;
- Universal starter played, branch not yet selected;
- branch selected after second card;
- replay paused on search choice;
- 20-card hand with two overflow instances;
- reward extra replay paused on search.

Compare serialized bytes after load/save normalization and compare final units, deck zones, queue cursor, tasks, modifiers, results, and terminal phase.

## Task 8: Full verification and documentation synchronization

**Files:**

- Modify after runtime GREEN: `docs/design/2026-08-11-full-card-catalog.md`
- Modify after runtime GREEN: `docs/design/2026-08-11-full-card-catalog.txt`
- Modify after runtime GREEN: `docs/design/2026-08-11-gamexxk-full-project-plan-all-in-one.md`
- Modify after runtime GREEN: `docs/design/2026-08-11-gamexxk-full-project-plan-all-in-one.txt`
- Modify as required: `Source/GameXXK/Private/Tests/GameXXKCardDocumentationTest.cpp`

### 8.1 Cold compile and exact suites

Run one fresh cold UBT, then these independent prefixes:

```text
GameXXK.Data.PartnerCards.Sorcerer
GameXXK.Data.HeroSpellTask
GameXXK.Data.TaskNpcSpellTask
GameXXK.Data.CardResolutionQueue
GameXXK.Data.CardCatalog
GameXXK.Data.CardText
GameXXK.Data.CompanionBirth
```

Record report directories and concrete discovered counts. Every report must show 0 failed, 0 errors, and 0 not-run. Existing warning-bearing tests must be named and distinguished from new Sorcerer warnings.

### 8.2 Focused static review

Run targeted searches and fail the review if any result is unexplained:

```powershell
rg -n '灵火符|聚灵|离火印|炎墙|爆炎术|星火燎原|摄灵火|焚脉符|灵焰连弹|护灵幕|赤霄焚星|焚天诀|凝焰成刃|燃灵换元|焰幕护体|裂符|星火回收|赤焰封界' Source docs/design
rg -n 'Profession\.Sorcerer\.' Source/GameXXK/Private/GameXXKCardRules.cpp
rg -n 'DisplayName.*Sorcerer|ToString\(\).*Sorcerer' Source/GameXXK
```

The runtime may inspect Sorcerer metadata and stable owner/role but must not dispatch mechanics by display name. Any direct CardId occurrence outside catalog, migrations, tests, or documentation must be justified and removed when metadata can express it.

### 8.3 Regenerate and correctly label the 198-card documents

The standalone catalog must state that it is the current verified code snapshot. The all-in-one document must separate:

- approved target-final 198-card design;
- current verified code snapshot;
- implementation/test status.

Do not paste a stale current-code table under a target-final heading. Update the Sorcerer 18 rows from the now-green catalog and change only genuinely verified status entries.

### 8.4 Handoff boundary

Report changed files, exact Automation counts, report paths, remaining warnings, and unresolved 198-card groups. Do not stage or commit. The next phase is the user's requested full 198-card cross-review, not a Sorcerer-only completion claim.
