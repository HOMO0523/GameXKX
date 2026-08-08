# Combat Status, DoT, Momentum, and Tooltip Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the approved differentiated Bleed/Poison/Burn/Rot rules, Momentum and Weak damage math, Counter and healing behavior, ShiGu set loop, authoritative Guard/Charge HUD projection, exact concise tooltips, and player-victory simultaneous-death rule without changing the confirmed UI layout.

**Architecture:** Keep `FGameXXKCardBattleRuntime` as the sole gameplay authority. Split damage mitigation policy from damage cause, route every direct hit through one ordered hit-sequence resolver, and use explicit non-recursive follow-up events for Bleed, Rot, Counter, Burn, Toxic Explosion, and terminal evaluation. Guard and Charge remain derived HUD projections; equipment per-wearer/per-round state stays in existing equipment runtime records.

**Tech Stack:** Unreal Engine 5.8 C++, UE Automation Tests, cold UBT, Unreal commandlet reports, the existing GameXXK card runtime, equipment snapshots, battle adapter, HUD projection, and 2,400-case UE simulation.

---

## File map

- Modify `Source/GameXXK/Public/GameXXKCardTypes.h`: append damage-cause/effect enum values, add direct-hit/DoT audit fields, and add any serialized per-action fields without reordering existing values.
- Modify `Source/GameXXK/Public/GameXXKCardRules.h`: expose named percentages and focused status/damage helpers used by tests and adapters.
- Modify `Source/GameXXK/Private/GameXXKCardRules.cpp`: implement unlimited approved stacks, Momentum/Weak ordering, ordered direct-hit follow-ups, differentiated DoT, Toxic Explosion, healing bonus, cleanup, terminal precedence, and ShiGu hooks.
- Modify `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`: trigger Burn after one complete monster intent, use healing bonus for monster healing, and retain authoritative Charge state.
- Modify `Source/GameXXK/Public/GameXXKEquipmentSetCatalog.h`: replace the three ShiGu bonus identifiers while preserving values 13/14/15.
- Modify `Source/GameXXK/Private/GameXXKEquipmentSetCatalog.cpp`: install the exact approved 2/4/6-piece descriptions and hooks.
- Modify `Source/GameXXK/Private/GameXXKEquipmentRules.cpp`: materialize the renamed ShiGu runtime effects without changing other sets.
- Modify `Source/GameXXK/Private/GameXXKCardCatalog.cpp`: update the approved cards, add Toxic Explosion to 腐骨散, replace 剑意贯虹, and update cleanses.
- Modify `Source/GameXXK/Private/GameXXKEnemyCatalog.cpp`: update enemy Bleed/Poison values and Giant Toad healing setup.
- Modify `Source/GameXXK/Private/GameXXKCardText.cpp`: standardize public status names and describe Toxic Explosion/each-DoT cleanse.
- Modify `Source/GameXXK/Private/GameXXKEnemyText.cpp`: standardize status names in intent text.
- Modify `Source/GameXXK/Private/GameXXKBattlePresentation.cpp`: append read-only Guard and Charge badges to HUD views.
- Modify `Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp`: install the exact approved one-line rules.
- Modify `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp`: hide legacy Medicine and preserve the current badge row/layout.
- Modify `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`: convert active-battle Medicine stacks into healing bonus.
- Create `Source/GameXXK/Private/Tests/GameXXKCombatStatusRedesignTest.cpp`: focused status capacity, damage order, DoT, Toxic Explosion, Counter, healing, and terminal tests.
- Create `Source/GameXXK/Private/Tests/GameXXKShiGuBattleRulesTest.cpp`: real runtime tests for ShiGu 2/4/6 pieces and double explosion.
- Modify `Source/GameXXK/Private/Tests/GameXXKCardCatalogTest.cpp`: lock every approved card and cleanse value/order.
- Modify `Source/GameXXK/Private/Tests/GameXXKEnemyCatalogTest.cpp`: lock enemy DoT values and Giant Toad data.
- Modify `Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp`: prove enemy Burn, Counter, healing bonus, Charge, and simultaneous-death behavior.
- Modify `Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp`: lock exact tooltip strings, Medicine hiding, and `99+`/true-stack behavior.
- Modify `Source/GameXXK/Private/Tests/GameXXKBattleProjectedUnitHudTest.cpp`: prove Guard and Charge derive from their canonical sources.
- Modify `Source/GameXXK/Public/GameXXKCombatSimulationTypes.h`, `Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp`, and `Source/GameXXK/Private/Tests/GameXXKCardBalanceObservationTest.cpp`: record damage causes and status utilization in the existing 2,400-case observation.

## Shared verification commands

Save and close a running editor only through the project lifecycle helper. If MCP cannot save a running editor, stop instead of force-closing it:

```powershell
python -B -c "from scripts.ue_tdd_pipeline import save_running_editor_before_close, kill_editor; import sys; ok=save_running_editor_before_close(); kill_editor() if ok else None; sys.exit(0 if ok else 1)"
```

Use this cold build after each production slice:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload -NoHotReloadFromIDE
```

Run each automation path in a fresh process and inspect `index.json`, not localized log text:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\UE5 demo\GameXXK\GameXXK.uproject' `
  -unattended -nopause -nosplash -nullrhi `
  '-ExecCmds=Automation RunTests GameXXK.Data.CombatStatusRedesign; Quit' `
  '-ReportOutputPath=D:\UE5 demo\GameXXK\Saved\Automation\CombatStatusRedesign_Core'
```

Expected GREEN for every report: `failed = 0`, `notRun = 0`, and the intended concrete tests are listed individually.

### Task 0: Establish the cold baseline and protected scope

**Files:**
- Read: `AGENTS.md`
- Read: `docs/superpowers/specs/2026-08-08-combat-status-dot-momentum-tooltip-redesign.md`
- Inspect: every file in the file map

- [ ] **Step 1: Verify branch, HEAD, and overlap**

```powershell
git branch --show-current
git rev-parse HEAD
git status --short
git diff -- Source/GameXXK docs/superpowers
```

Expected: branch `main`; user-owned untracked art/build outputs may remain; no tracked overlap in the listed implementation files.

- [ ] **Step 2: Save/close the editor safely and cold-build the unchanged baseline**

Run the lifecycle helper and shared UBT command.

Expected: editor is absent or saved with `dirty_after=[]`; UBT exits 0 without Live Coding/Hot Reload.

- [ ] **Step 3: Run the unchanged focused baseline**

Run these filters in separate reports:

```text
GameXXK.Data.CardCombatRules
GameXXK.Data.CardBattleRuntime
GameXXK.Battle.EnemyIntentRules
GameXXK.UI.Battle.StatusEffectsWidget
GameXXK.Data.EquipmentSetCatalog
GameXXK.Data.MarkRules
```

Expected: capture exact counts and any existing warnings before adding RED tests.

### Task 1: Lock status capacity and simultaneous-death precedence

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKCombatStatusRedesignTest.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp:1934-1980, 770-794, 2201-2232`

- [ ] **Step 1: Write failing capacity and terminal tests**

Create the test path `GameXXK.Data.CombatStatusRedesign.CapacityAndTerminal`. Its table must assert:

```cpp
const TArray<EGameXXKCardStatus> Unlimited = {
    EGameXXKCardStatus::Momentum,
    EGameXXKCardStatus::Bleed,
    EGameXXKCardStatus::Poison,
    EGameXXKCardStatus::Burn,
    EGameXXKCardStatus::DamageOverTime};
for (const EGameXXKCardStatus Status : Unlimited)
{
    FGameXXKCardCombatUnit Unit = MakeStatusUnit(TEXT("Unit"), EGameXXKCardTargetSide::Party, 100, 1);
    TestEqual(TEXT("large approved status applies without gameplay cap"),
        GameXXKCardRules::AddCombatStatus(Unit, Status, 250), 250);
    TestEqual(TEXT("large approved status remains exact"),
        GameXXKCardRules::GetCombatStatusStacks(Unit, Status), 250);
}
```

Also build a legal transaction fixture whose complete queue leaves the last party unit and last enemy at zero HP, then assert `Phase == Victory`. Keep a separate party-only death fixture asserting `Defeat`.

- [ ] **Step 2: Cold-build and verify RED**

Run `GameXXK.Data.CombatStatusRedesign.CapacityAndTerminal`.

Expected: Momentum stops at 3, DoTs stop at 8, and the simultaneous-death fixture reports Defeat.

- [ ] **Step 3: Implement uncapped approved statuses with saturating arithmetic**

In `GetCombatStatusCap`, return `MAX_int32` for the five approved statuses. In `AddCombatStatus`, calculate available capacity with `int64`:

```cpp
const int64 Available = static_cast<int64>(GetCombatStatusCap(Status)) - CurrentStacks;
const int32 AppliedStacks = static_cast<int32>(FMath::Clamp<int64>(Amount, 0, Available));
```

Keep Agility 2, Vulnerability/Mark/Weak/Rage 5, Counter/Wealth 8, healing bonus 99, and all one-shot statuses unchanged.

- [ ] **Step 4: Give enemy elimination terminal priority**

Replace the simultaneous-loss branch with:

```cpp
if (!bHasLivingEnemy)
{
    InOutRuntime.Phase = EGameXXKCardBattlePhase::Victory;
}
else if (!bHasLivingParty)
{
    InOutRuntime.Phase = EGameXXKCardBattlePhase::Defeat;
}
```

Retain pending-hand cleanup on either terminal outcome.

- [ ] **Step 5: Cold-build, verify GREEN, and run Mark regressions**

Run the new test plus `GameXXK.Data.MarkRules` and `GameXXK.Data.CardCombatRules`.

- [ ] **Step 6: Commit the first slice**

```powershell
git add -- 'Source/GameXXK/Private/GameXXKCardRules.cpp' 'Source/GameXXK/Private/Tests/GameXXKCombatStatusRedesignTest.cpp'
git commit -m "feat: uncap core combat stacks"
```

### Task 2: Implement Momentum and Weak in the shared direct-hit pipeline

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardRules.h`
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h:938-1025`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp:2430-2588, 4121-4345`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCombatStatusRedesignTest.cpp`

- [ ] **Step 1: Add failing damage-order tests**

Add `GameXXK.Data.CombatStatusRedesign.DirectAttackOrder` covering a single hit, three hits, group targets, enemy attack, and reactive direct hit. Lock this numeric case:

```text
Attack 20, card 150%, flat 4, Momentum 6 => requested 40
Weak => 20
Defense 5 => 15
Vulnerability 2 + Mark => floor(15 * 135%) = 20
Armor 7 => 13 HP damage
```

Assert Momentum remains 6; Weak remains until owner-side end; Mark consumes one; Vulnerability clears.

- [ ] **Step 2: Verify RED**

Expected: requested damage omits Momentum, Weak has no effect, and existing downstream Mark/armor values differ.

- [ ] **Step 3: Add explicit context and audit fields**

Append to `FGameXXKCardDamageContext` and `FGameXXKCardDamageResult`:

```cpp
int32 MomentumStacksOverride = INDEX_NONE;
int32 BaseRequestedDamage = 0;
int32 MomentumDamageBonus = 0;
int32 DamageAfterWeak = 0;
int32 WeakDamageReduction = 0;
```

The override is only for a card packet that consumed Momentum while building its multiplier; `INDEX_NONE` means read the living source's current stacks.

- [ ] **Step 4: Apply the approved order before defense**

Inside `ApplyCombatDirectDamageInternal`, after Agility and before `ComputeDamageAfterDefense`:

```cpp
const FGameXXKCardCombatUnit* Source = FindCombatUnitById(NewUnits, Context.SourceUnitId);
const int32 MomentumStacks = Context.MomentumStacksOverride != INDEX_NONE
    ? FMath::Max(0, Context.MomentumStacksOverride)
    : GetCombatStatusStacksInternal(*Source, EGameXXKCardStatus::Momentum);
NewResult.BaseRequestedDamage = RequestedDamage;
NewResult.MomentumDamageBonus = MomentumStacks;
const int32 WithMomentum = SaturatingAddPositive(RequestedDamage, MomentumStacks);
NewResult.DamageAfterWeak = GetCombatStatusStacksInternal(*Source, EGameXXKCardStatus::Weak) > 0
    ? FMath::Max(1, WithMomentum / 2)
    : WithMomentum;
NewResult.WeakDamageReduction = WithMomentum - NewResult.DamageAfterWeak;
NewResult.RequestedDamage = WithMomentum;
NewResult.DamageAfterDefense = ComputeDamageAfterDefense(
    NewResult.DamageAfterWeak, *ResolvedTarget, Context.IgnoredDefense);
```

Non-direct damage leaves all new direct-hit fields at zero.

- [ ] **Step 5: Snapshot Momentum once per card attack packet**

At the start of `ResolveAttackPacket`, before conditions can consume state, capture:

```cpp
const FGameXXKCardCombatUnit* PacketOwner = FindCombatUnitById(InOutRuntime.Units, Instance.OwnerUnitId);
const int32 MomentumAtPacketStart = PacketOwner
    ? GameXXKCardRules::GetCombatStatusStacks(*PacketOwner, EGameXXKCardStatus::Momentum)
    : 0;
```

Set `Context.MomentumStacksOverride = MomentumAtPacketStart` on every hit and every group target.

- [ ] **Step 6: Cold-build and verify GREEN/regressions**

Run the new direct-order test, `GameXXK.Data.MarkRules`, `GameXXK.Data.CardBattleRuntime`, and `GameXXK.Battle.EnemyIntentRules`.

- [ ] **Step 7: Commit**

```powershell
git add -- 'Source/GameXXK/Public/GameXXKCardRules.h' 'Source/GameXXK/Public/GameXXKCardTypes.h' 'Source/GameXXK/Private/GameXXKCardRules.cpp' 'Source/GameXXK/Private/Tests/GameXXKCombatStatusRedesignTest.cpp'
git commit -m "feat: apply momentum and weak to direct attacks"
```

### Task 3: Add explicit DoT causes and Toxic Explosion

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKCardRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp:2350-2410, 2906-2913, 3523-3569`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCombatStatusRedesignTest.cpp`

- [ ] **Step 1: Write failing differentiated DoT tests**

Add test paths `...Bleed`, `...EndPhaseDot`, and `...ToxicExplosion`. Lock:

- Bleed 4 on a landed hit: HP -4 and Bleed 3; no end-phase change.
- Poison 3 at owner end: HP -3, Poison 2.
- Burn 3 at action end: HP -3, Burn 2; owner end then Burn 1 with no damage.
- Rot 2 adds 2 to each actual Bleed/Poison/Burn packet, never self-triggers, and owner end reduces Rot by one.
- Toxic Explosion at B4/P3/Burn2/Rot2 deals 15, leaves B3/P2/Burn1/Rot2.
- A six-piece preservation flag deals the same 15 but leaves all four stacks unchanged.

- [ ] **Step 2: Verify RED against the old combined end-phase formula**

Expected: old end phase deals `3*Bleed + 2*Poison + 3*Burn + 3*DamageOverTime` and consumes nothing; no Toxic Explosion API exists.

- [ ] **Step 3: Separate mitigation kind from audit cause**

Append, without changing existing enum values:

```cpp
enum class EGameXXKCardDamageCause : uint8
{
    Invalid = 0,
    DirectAttack = 1,
    Counter = 2,
    Bleed = 3,
    Poison = 4,
    Burn = 5,
    Rot = 6,
    ToxicExplosionBleed = 7,
    ToxicExplosionPoison = 8,
    ToxicExplosionBurn = 9,
    SelfLoss = 10,
    Environment = 11
};
```

Add `Cause`, `StatusStacksBefore`, `RotDamageBonus`, and `StatusStacksConsumed` to `FGameXXKCardDamageResult`.

- [ ] **Step 4: Implement one health-only status packet helper**

Use a non-recursive helper that clamps at current HP, updates `bLiving`, removes dead Guard links, and records the exact cause. It must not call the direct resolver and must never consult defense, armor, Agility, Vulnerability, Mark, Momentum, Weak, Guard, or Counter.

```cpp
bool ApplyStatusHealthLoss(
    FGameXXKCardBattleRuntime& InOutRuntime,
    FName TargetId,
    EGameXXKCardDamageCause Cause,
    int32 BaseStacks,
    bool bApplyRot,
    FGameXXKCardDamageResult& OutResult,
    FString& OutError);
```

- [ ] **Step 5: Implement atomic Toxic Explosion**

Expose:

```cpp
GAMEXXK_API bool ResolveToxicExplosion(
    FGameXXKCardBattleRuntime& InOutRuntime,
    FName SourceUnitId,
    FName TargetUnitId,
    bool bPreserveDamageOverTimeStacks,
    TArray<FGameXXKCardDamageResult>& OutResults,
    FString* OutError = nullptr);
```

Snapshot Bleed/Poison/Burn first, enqueue in that fixed order, add current Rot to each present packet, then atomically consume one from each present status unless preservation is active. Finish all snapshot packets even if HP reaches zero mid-explosion.

- [ ] **Step 6: Replace the old combined end-phase helper**

`ApplyCombatEndPhaseDot` becomes a compatibility wrapper for the owner-side end sequence: Poison damage/-1, Burn -1 only, Rot -1, Weak -1, Bleed unchanged. Higher-level phase functions collect the individual audit packets.

- [ ] **Step 7: Cold-build, verify GREEN, and commit**

Run all `GameXXK.Data.CombatStatusRedesign` tests and existing combat/runtime groups.

```powershell
git add -- 'Source/GameXXK/Public/GameXXKCardTypes.h' 'Source/GameXXK/Public/GameXXKCardRules.h' 'Source/GameXXK/Private/GameXXKCardRules.cpp' 'Source/GameXXK/Private/Tests/GameXXKCombatStatusRedesignTest.cpp'
git commit -m "feat: differentiate combat damage over time"
```

### Task 4: Enforce the per-hit Bleed/on-hit/Counter order

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp:2430-2600, 4080-4345`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCombatStatusRedesignTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp`

- [ ] **Step 1: Write failing ordered-hit tests**

Cover armor-only hits, Agility, new on-hit Bleed, old Bleed lethal, three-hit Bleed countdown, Counter with armor-only hit, multiple Counter layers, lethal Counter stopping later hits, and prohibition of Counter-on-Counter.

- [ ] **Step 2: Verify RED**

Expected: current resolver applies on-hit status immediately but never triggers Bleed or status Counter.

- [ ] **Step 3: Build one non-recursive hit sequence**

For each direct hit, capture pre-hit Bleed and Counter, call the low-level direct packet, then execute exactly:

```cpp
if (!DirectResult.bAvoidedByAgility && Target->bLiving && BleedBefore > 0)
    ResolveBleedPacketAndConsumeOne(...);
if (Target->bLiving)
    ApplyDeferredOnHitStatusesAndShiGuHooks(...);
if (Target->bLiving && CounterBefore > 0 && bPlayerCardDirectHit)
    ResolveOneCounterAndConsumeOne(...);
```

Tag Counter packets with `Cause=Counter` and `bCanTriggerCounter=false`. Continue the next hit only if source and target both remain living.

- [ ] **Step 4: Move on-hit status application out of the low-level damage tail**

The low-level packet owns only mitigation and HP. The high-level player/enemy wrappers own deferred statuses, Bleed, ShiGu hooks, Counter, and append all audit results in event order.

- [ ] **Step 5: Cold-build, verify GREEN/regressions, and commit**

```powershell
git add -- 'Source/GameXXK/Private/GameXXKCardRules.cpp' 'Source/GameXXK/Private/Tests/GameXXKCombatStatusRedesignTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp'
git commit -m "feat: order bleed and counter hit followups"
```

### Task 5: Trigger Burn at complete player-card and monster-intent boundaries

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp:5120-5240, 5300-5425`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp:2640-2790, 2910-2960`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCombatStatusRedesignTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp`

- [ ] **Step 1: Write failing complete-action tests**

Prove one player card with three hits triggers Burn once after all hits, one multi-effect enemy intent triggers Burn once after all effects, reactive Burn newly applied during the action triggers at that action end, and rejected/uncommitted actions do not trigger it.

- [ ] **Step 2: Verify RED**

Expected: Burn only participates in the old end-phase total.

- [ ] **Step 3: Add the player-card boundary**

After `ResolveCurrentCardEffects` succeeds, but before boss/terminal validation commits, call `ResolveBurnAfterCompletedAction(NewRuntime, CopiedInstance.OwnerUnitId, NewResult.DamageResults, ValidationError)`.

- [ ] **Step 4: Add the monster-intent boundary**

After every effect and hit of one catalog intent has completed and before advancing the intent cursor, call the same helper for `Intent.SourceUnitId`. Do not call it per hit or per target.

- [ ] **Step 5: Move terminal evaluation to the queue tail**

Remove intermediate terminal commits that would skip same-action Bleed, ShiGu explosion, Counter, or Burn. Evaluate boss phases and victory/defeat only after the complete action queue.

- [ ] **Step 6: Cold-build, verify GREEN, and commit**

```powershell
git add -- 'Source/GameXXK/Private/GameXXKCardRules.cpp' 'Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp' 'Source/GameXXK/Private/Tests/GameXXKCombatStatusRedesignTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp'
git commit -m "feat: resolve burn after complete actions"
```

### Task 6: Replace Medicine with symmetric healing bonus

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKEnemyCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp:1400-1565`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEnemyCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp`

- [ ] **Step 1: Write failing Giant Toad and migration tests**

Lock Inflate = Armor 14 + NextHealingBonus 6; Tongue = 100% attack then self-heal 6; setup plus Tongue heals 12 and clears the bonus. Load a battle with Medicine 2 and assert Medicine 0, NextHealingBonus 12.

- [ ] **Step 2: Verify RED**

Expected: catalog still emits Medicine 1; enemy healing ignores `NextHealingBonus`; migration leaves Medicine visible.

- [ ] **Step 3: Use one symmetric healing action helper**

Route card and enemy healing through a helper that snapshots and clears the source's `NextHealingBonus` once, then adds that fixed amount to every target in the action.

- [ ] **Step 4: Migrate old active battles**

For every runtime unit during save migration:

```cpp
const int32 Medicine = GameXXKCardRules::ConsumeCombatStatus(Unit, EGameXXKCardStatus::Medicine, MAX_int32);
if (Medicine > 0)
{
    GameXXKCardRules::AddCombatStatus(Unit, EGameXXKCardStatus::NextHealingBonus,
        static_cast<int32>(FMath::Min<int64>(99, static_cast<int64>(Medicine) * 6)));
}
```

- [ ] **Step 5: Cold-build, verify GREEN, and commit**

```powershell
git add -- 'Source/GameXXK/Private/GameXXKEnemyCatalog.cpp' 'Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp' 'Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp' 'Source/GameXXK/Private/Tests/GameXXKEnemyCatalogTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp'
git commit -m "feat: replace medicine with healing bonus"
```

### Task 7: Implement ShiGu 2/4/6-piece runtime rules

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKEquipmentSetCatalog.h`
- Modify: `Source/GameXXK/Private/GameXXKEquipmentSetCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKEquipmentRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKShiGuBattleRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentSetCatalogTest.cpp`

- [ ] **Step 1: Write failing catalog and runtime tests**

Create `GameXXK.Integration.ShiGuBattleRules` and lock: per-card/per-target Rot once, AoE each target, only Bleed/Poison/Burn triggers, nonqualifying application preserves the 4-piece chance, per-wearer counters, first explosion preservation for 6-piece, and 腐骨散 auto+explicit double explosion.

- [ ] **Step 2: Verify RED**

Expected: old 5%/80%/end-tick descriptions and no runtime triggers.

- [ ] **Step 3: Preserve enum values while replacing meanings**

Use values 13/14/15 for:

```cpp
ShiGuApplyRotPerCardTarget = 13,
ShiGuAutoToxicExplosion = 14,
ShiGuPreserveFirstToxicExplosion = 15,
```

- [ ] **Step 4: Install exact set descriptions**

Keep RequiredPieces 2/4/6 and Owner scope, and use these exact descriptions:

```text
2件：穿戴者使用一张牌时，该牌首次对一个目标施加流血、中毒或灼烧后，同时施加1层蚀伤。每张牌对每个目标最多触发一次。
4件：每名穿戴者每回合首次新施加流血、中毒或灼烧，并使目标同时具有这三种状态中的至少两种时，自动毒爆一次。
6件：每名穿戴者每回合第一次发动的毒爆不会减少目标的流血、中毒和灼烧层数。
```

- [ ] **Step 5: Track per-card/per-target and per-wearer state**

Use a temporary card-resolution set keyed by `(SourceUnitId, TargetUnitId)` for 2-piece. Use each matching `FGameXXKEquipmentBattleEffectRuntime::CurrentRoundTriggerCount` for 4/6-piece; reset counts at the existing new-round boundary. Do not add team-global booleans.

- [ ] **Step 6: Invoke hooks only after a positive Bleed/Poison/Burn application**

Apply Rot 1 first, test whether the target now has at least two of the three qualifying statuses, then auto-explode. A failed two-status check does not consume 4-piece. The first actual explosion consumes the 6-piece opportunity and passes `bPreserveDamageOverTimeStacks=true`.

- [ ] **Step 7: Cold-build, verify GREEN, and commit**

```powershell
git add -- 'Source/GameXXK/Public/GameXXKEquipmentSetCatalog.h' 'Source/GameXXK/Private/GameXXKEquipmentSetCatalog.cpp' 'Source/GameXXK/Private/GameXXKEquipmentRules.cpp' 'Source/GameXXK/Private/GameXXKCardRules.cpp' 'Source/GameXXK/Private/Tests/GameXXKShiGuBattleRulesTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKEquipmentSetCatalogTest.cpp'
git commit -m "feat: implement shigu toxic explosion set"
```

### Task 8: Update card rules, values, and each-DoT cleanse semantics

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp:3557-3569, 4121-4345`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp`

- [ ] **Step 1: Write failing catalog table tests**

Lock every exact value:

```text
Bleed: 封喉4; 残月三叠 total6 after all three hits; 断脉矢6.
Poison: 百草毒3; 苦参麻散4+破绽2; 蛊雾迷踪4+标记1; 腐叶陷阱3+标记2; 岩粉封脉/铁蒺藜/洞穴分支2.
Burn: 灵火符3; 凝焰成刃3; 炎墙/焰幕护体2; 离火印/焚脉符3; 星火燎原3; 赤霄焚星5; 焚天诀/赤焰封界4.
Cleanse: 止血草 Bleed6+Heal6; 止血散 Bleed4+Heal12; 归元术/银铃镇心/清心散/灵泉一饮 each2; 异草辨识/行气针/回气香 each1; 连翘解毒 each4; 归元返照/药王归元/虎魄镇胆 all allies each2.
```

- [ ] **Step 2: Add and verify the ToxicExplosion card effect RED**

Append `ToxicExplosion = 29` to `EGameXXKCardEffectType`. Change 腐骨散 to `Apply Poison 2`, then `ToxicExplosion`; delete Poison 5 and Vulnerability 1.

- [ ] **Step 3: Replace 剑意贯虹 and preserve Momentum snapshot**

Keep `Attack(200)` and attach `BonusDamagePercentPerConsumedStatus(20)` consuming all owner Momentum. Delete `GainManaPerConsumedStatus(4)`. The Task 2 packet-start snapshot supplies the same consumed stacks as flat Momentum damage.

- [ ] **Step 4: Keep 刀意收束 explicit consumption**

Retain maximum 3; each consumed stack gives 3 Mana and one draw. When a `DrawCards` effect has a nonempty `ConsumedStackResultRef`, calculate its draw count as:

```cpp
const int32 ReferencedConsumed = ConsumptionResults.FindRef(Effect.ConsumedStackResultRef);
const int32 DrawCount = Effect.ConsumedStackResultRef.IsNone()
    ? Effect.Magnitude
    : static_cast<int32>(FMath::Min<int64>(MAX_int32,
        static_cast<int64>(Effect.Magnitude) * ReferencedConsumed));
```

Use `DrawCount` in `DrawCards`. This keeps 破云一闪 unchanged because it consumes one Agility. Add a runtime test with 5 Momentum proving 3 consumed, 9 Mana requested subject to max, 3 cards drawn, 2 Momentum remain.

- [ ] **Step 5: Change `RemoveAnyDamageOverTime N` to each status**

Replace the shared `Remaining` budget loop with independent calls:

```cpp
int32 Removed = 0;
for (const EGameXXKCardStatus Status : {Bleed, Poison, Burn, DamageOverTime})
    Removed += GameXXKCardRules::ConsumeCombatStatus(InOutUnit, Status, Maximum);
return Removed;
```

- [ ] **Step 6: Cold-build, verify GREEN, and commit**

```powershell
git add -- 'Source/GameXXK/Public/GameXXKCardTypes.h' 'Source/GameXXK/Private/GameXXKCardCatalog.cpp' 'Source/GameXXK/Private/GameXXKCardRules.cpp' 'Source/GameXXK/Private/Tests/GameXXKCardCatalogTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp'
git commit -m "feat: rebalance dot and momentum cards"
```

### Task 9: Update enemy DoT data and symmetric runtime behavior

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKEnemyCatalog.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEnemyCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp`

- [ ] **Step 1: Write failing exact enemy-data tests**

Lock 豪猪刺毛 Bleed4, 飞刺 all Bleed3, 黑熊撕裂 Bleed6, 山猫抓挠 Bleed4, 老虎裂伤 Bleed6, 毒蛇毒牙 Poison2, 巨蟾毒雾 all Poison2, and no enemy Burn/Rot application.

- [ ] **Step 2: Verify RED, edit only the catalog values, and verify GREEN**

Do not change enemy attack percentages, targeting, intent order, charge rules, or unrelated passives.

- [ ] **Step 3: Run full enemy intent/mechanics groups and commit**

```powershell
git add -- 'Source/GameXXK/Private/GameXXKEnemyCatalog.cpp' 'Source/GameXXK/Private/Tests/GameXXKEnemyCatalogTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp'
git commit -m "balance: update enemy damage over time stacks"
```

### Task 10: Project Guard and Charge without duplicating gameplay state

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKBattlePresentation.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleProjectedUnitHudTest.cpp`

- [ ] **Step 1: Write failing projection tests**

Assert two Guard links targeting one unit with stacks 1 and 2 project Guard 3 only on the protected unit. Assert an enemy state with a pending charged intent and `ChargeRoundsRemaining=2` projects Charge 2; decrement, execution, cancellation, or death removes it. Ordinary status dispel must not affect either source.

- [ ] **Step 2: Verify RED**

Expected: `BuildUnitHudView` only copies `Unit.Statuses`, so neither badge appears.

- [ ] **Step 3: Append read-only derived stacks**

After copying real statuses, sum valid living `GuardLinks` whose `ProtectedUnitId` matches. For enemy units, read `Runtime.EnemyStates[UnitId]` and append Charge only when pending intent is nonempty and remaining rounds are positive. Never write either projection back to `Unit.Statuses`.

- [ ] **Step 4: Cold-build, verify GREEN/layout regressions, and commit**

```powershell
git add -- 'Source/GameXXK/Private/GameXXKBattlePresentation.cpp' 'Source/GameXXK/Private/Tests/GameXXKBattleProjectedUnitHudTest.cpp'
git commit -m "ui: project guard and charge status badges"
```

### Task 11: Install exact public names and concise tooltips

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardText.cpp`
- Modify: `Source/GameXXK/Private/GameXXKEnemyText.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardTextTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEnemyCatalogTest.cpp`

- [ ] **Step 1: Write failing exact-string tests**

For Armor and all 24 visible statuses, assert `DescribeStatusTooltip` equals exactly `Name + "\n层数：N\n" + Rule` using this table:

```text
护甲：优先抵挡直接攻击伤害；所属阵营回合开始时清空。
气势：每层使每段攻击伤害+1；仅指定牌与驱散会消耗。
灵动：闪避下一段直接攻击并减少1层。
破绽：每层使下一段直接攻击伤害提高10%；结算后清空。
流血：受到直接攻击后，失去等同层数的生命并减少1层；回合结束不衰减。
中毒：回合结束时，失去等同层数的生命并减少1层。
灼烧：打出牌或执行意图后，失去等同层数的生命并减少1层；回合结束再减少1层。
标记：直接攻击伤害提高15%；每段有效命中后减少1层。
守护：下一次针对本单位的单体攻击由守护者承受；触发后减少1层。
蚀伤：流血、中毒或灼烧造成伤害时，额外失去等同层数的生命；回合结束减少1层。
破绽免疫：无法获得新的破绽；不会自行消耗。
追击标记：下一次攻击的首段命中施加1层标记；出手后减少1层。
破绽追击：下一次攻击的首段命中施加1层破绽；出手后减少1层。
疗愈增幅：下一次治疗中，每个目标的治疗量增加等同层数的数值；结算后清空。
地形双效：队伍下一张地形牌的地形条件效果额外结算1次；使用后减少1层。
本回合地形双效：本回合队伍下一张地形牌的地形条件效果额外结算1次；使用或回合结束时清除。
地形免耗：队伍下一张地形牌的气力消耗变为0；使用后减少1层。
地形减耗：队伍下一张地形牌的气力消耗-1；使用后减少1层。
代挡：替队友承受下一次敌方单体攻击；触发后减少1层。
虚弱：直接攻击伤害降低50%；回合结束减少1层。
财富：钱潮冲击每层伤害+15；散财疗伤最多消耗3层，每层回复6%最大生命。
狂怒：受到玩家牌的生命伤害时增加1层；怒獠每层伤害+20。
猎物：老虎锁定的目标；虎扑将攻击该单位。
蓄力：层数表示剩余蓄力回合；归零后执行已准备的意图。
反击：受到玩家牌的直接攻击后，以100%攻击反击攻击者并减少1层。
```

Assert no tooltip contains `效果：` or `时机：`. Assert Medicine creates no badge. Assert 99 displays `99`, 100 displays `99+`, and a 250-stack tooltip contains `层数：250`.

- [ ] **Step 2: Verify RED**

Expected: long two-part tooltips, old names such as 敏捷/易伤, and visible Medicine fail.

- [ ] **Step 3: Collapse style data to one user-facing rule line**

Keep existing icon IDs, texture paths, tints, priorities, row size, hit testing, and tooltip paper. Set `Style.Tooltip` to the exact one-line rule and have `DescribeStatusTooltip` format only:

```cpp
return FString::Printf(TEXT("%s\n层数：%d\n%s"),
    *Style.DisplayName, FMath::Max(1, Stacks), *Style.Tooltip);
```

- [ ] **Step 4: Install the approved names everywhere**

Use 气势、灵动、破绽、流血、中毒、灼烧、标记、守护、蚀伤、破绽免疫、追击标记、破绽追击、疗愈增幅, and the approved terrain/monster terms. Keep internal enum identifiers and numeric values unchanged.

- [ ] **Step 5: Hide Medicine at the badge builder**

Skip `EGameXXKCardStatus::Medicine` in both `BuildStatusText` and `BuildBadgeModels`; do not delete enum value 20.

- [ ] **Step 6: Cold-build, verify GREEN and UI layout preservation, then commit**

```powershell
git add -- 'Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp' 'Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp' 'Source/GameXXK/Private/GameXXKCardText.cpp' 'Source/GameXXK/Private/GameXXKEnemyText.cpp' 'Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKCardTextTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKEnemyCatalogTest.cpp'
git commit -m "ui: simplify battle status tooltips"
```

### Task 12: Extend simulation telemetry and run the 2,400-case comparison

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCombatSimulationTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBalanceObservationTest.cpp`
- Generate: `Saved/BalanceObservation/status-redesign-*`

- [ ] **Step 1: Write compile-failing metric assertions**

Add counters/maps for damage by cause, applied/triggered/cleansed/wasted status stacks, Momentum bonus/consumption, Weak prevented damage, Counter triggers/kills, and ShiGu 2/4/6 triggers. Assert all metric keys serialize in stable lexical order.

- [ ] **Step 2: Populate metrics from audit results and before/after snapshots**

Do not recalculate combat math in the simulator. Aggregate `FGameXXKCardDamageResult` cause/audit fields and actual runtime deltas.

- [ ] **Step 3: Cold-build and run the observation**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\UE5 demo\GameXXK\GameXXK.uproject' `
  -unattended -nopause -nosplash -nullrhi `
  '-GameXXKBalanceObservationId=status-redesign-20260808' `
  '-ExecCmds=Automation RunTests GameXXK.Diagnostics.CardBalanceObservation; Quit' `
  '-ReportOutputPath=D:\UE5 demo\GameXXK\Saved\Automation\StatusRedesign_Observation'
```

Expected: exactly 2,400 cases attempted; report preserves victory/defeat/stalemate/error and round quantiles; new cause/utilization fields are populated.

- [ ] **Step 4: Compare to the accepted baseline**

Use `docs/design/2026-08-08-card-balance-analysis.md` as the reference. Report total and bucket changes; identify whether any outlier is driven by Bleed multihit, Burn action frequency, Poison duration, Toxic Explosion, Counter, or Momentum. Do not tune unrelated stats merely to hit one global win rate.

- [ ] **Step 5: Commit telemetry only after deterministic replay passes**

```powershell
git add -- 'Source/GameXXK/Public/GameXXKCombatSimulationTypes.h' 'Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp' 'Source/GameXXK/Private/Tests/GameXXKCardBalanceObservationTest.cpp'
git commit -m "test: measure redesigned combat statuses"
```

### Task 13: Final verification gate

**Files:**
- Verify: all planned source/test files
- Generate only: `Saved/Automation/StatusRedesign_Final_*`, `Saved/BalanceObservation/status-redesign-*`

- [ ] **Step 1: Save/close editor and run one fresh cold UBT**

Expected: exit 0 with both `-NoHotReload` and `-NoHotReloadFromIDE`.

- [ ] **Step 2: Run focused and related groups in separate reports**

```text
GameXXK.Data.CombatStatusRedesign
GameXXK.Integration.ShiGuBattleRules
GameXXK.Data.CardCatalog
GameXXK.Data.CardCombatRules
GameXXK.Data.CardBattleRuntime
GameXXK.Data.MarkRules
GameXXK.Integration.MarkCardCompatibility
GameXXK.Battle.EnemyIntentRules
GameXXK.Battle.EnemyMechanics
GameXXK.Data.EquipmentSetCatalog
GameXXK.UI.Battle.StatusEffectsWidget
GameXXK.UI.Battle.ProjectedUnitHud
```

Expected: every intended concrete test runs; zero failed/not-run tests; existing warnings are itemized rather than hidden.

- [ ] **Step 3: Re-run deterministic route replay and 2,400 cases**

Expected: same-process deterministic fingerprints match and the complete observation contains 2,400 rows.

- [ ] **Step 4: Verify layout and asset scope**

Run the existing Battle HUD/widget structural tests. Confirm no UMG layout, backpack layout, map, camera, sprite, PaperZD, or generated art asset was modified.

- [ ] **Step 5: Inspect final diff and commits**

```powershell
git diff --check
git status --short
git log --oneline --decorate -15
```

Expected: no tracked uncommitted changes; user-owned untracked files remain untouched; every commit contains only its declared slice.

## Self-review checklist

- [ ] Bleed triggers after each landed direct hit, consumes one, and never decays at turn end.
- [ ] Poison triggers at its owner's side end and consumes one.
- [ ] Burn triggers once after a complete card/intent, consumes one, and separately decays one at owner end.
- [ ] Rot adds its full current value to each real Bleed/Poison/Burn packet, never detonates directly, and decays at owner end.
- [ ] Toxic Explosion snapshots Bleed/Poison/Burn, applies Rot per packet, and consumes one from each unless ShiGu 6 preserves them.
- [ ] Momentum adds its full stack count per hit/target, is not normally consumed, and 剑意贯虹/刀意收束 use the approved exceptions.
- [ ] Weak halves outgoing direct attacks only, does not stack percentage, and decays one at owner end.
- [ ] Mark remains the already-tested fixed +15% landed-hit rule.
- [ ] Per-hit order is direct attack, old Bleed, on-hit statuses/ShiGu, Counter; lethal events stop later segments correctly.
- [ ] Counter is 100% current Attack, consumes one, uses the direct pipeline, and cannot chain from Counter.
- [ ] NextHealingBonus works for player and enemy actions; Medicine migrates and stays hidden.
- [ ] ShiGu counters are per wearer and per round; auto+explicit 腐骨散 explosions both run.
- [ ] Guard and Charge badges derive from canonical data and are never duplicated into ordinary statuses.
- [ ] Exact approved tooltip strings are used without changing the current layout.
- [ ] Complete-queue simultaneous death is player victory.
- [ ] The 2,400-case report is generated by UE rules and compared to the accepted baseline.
