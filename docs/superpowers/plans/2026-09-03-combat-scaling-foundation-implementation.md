# Combat Scaling Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Provide one integer-safe authority for card quality, level differences, DOT reservoirs, Defense-derived Armor, Medicine, enemy Energy denial, and persisted battle scaling context.

**Architecture:** A new pure `FGameXXKCombatScalingRules` class owns arithmetic only. Existing `GameXXKCardRules` remains the transaction authority and calls the pure helper with a battle-start snapshot; save migration adds the snapshot without retroactively re-scaling old state.

**Tech Stack:** Unreal Engine 5.8 C++, USTRUCT SaveGame data, UE Automation Framework, cold UBT and UnrealEditor-Cmd through project scripts.

---

## Repository guard

- Work in `D:\UE5 demo\GameXXK` on `codex/overall-in-run-optimization`; do not create a worktree.
- Preserve every unrelated tracked deletion and untracked asset. Stage only paths listed by the active task.
- Do not use Live Coding, Hot Reload, UnrealBridge, or a 3D-town map.
- The approved specification is `docs/superpowers/specs/2026-09-03-card-monster-progression-rebalance-design.md`, especially sections 4, 5, 14, and 15.

## File map

### New focused files

- `Source/GameXXK/Public/GameXXKCombatScalingRules.h` — pure public integer scaling API.
- `Source/GameXXK/Private/GameXXKCombatScalingRules.cpp` — overflow-safe arithmetic implementation.
- `Source/GameXXK/Private/Tests/GameXXKCombatScalingRulesTest.cpp` — exact arithmetic boundaries.
- `Source/GameXXK/Private/Tests/GameXXKCardBattleScalingIntegrationTest.cpp` — real transaction integration for direct damage, Armor, DOT, Medicine, and Energy penalty.

### Existing files to modify

- `Source/GameXXK/Public/GameXXKCardTypes.h` — persisted battle scaling snapshot and next-round Energy penalty.
- `Source/GameXXK/Public/GameXXKCardQualityRules.h`
- `Source/GameXXK/Private/GameXXKCardQualityRules.cpp` — 100/120/140 continuous scaling and `史诗` display.
- `Source/GameXXK/Public/GameXXKCardRules.h`
- `Source/GameXXK/Private/GameXXKCardRules.cpp` — transaction integration, no-99 Armor, DOT reservoirs, Medicine, refill penalty.
- `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp` — battle-start snapshot and legacy Shield projection.
- `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp` — schema v33 migration.
- Existing focused tests named by each task.

---

### Task 1: Add the pure integer scaling authority

**Files:**
- Create: `Source/GameXXK/Public/GameXXKCombatScalingRules.h`
- Create: `Source/GameXXK/Private/GameXXKCombatScalingRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKCombatScalingRulesTest.cpp`

- [ ] **Step 1: Write the failing arithmetic tests**

Create `GameXXK.Data.CombatScaling.Arithmetic` with these exact assertions:

```cpp
#include "GameXXKCombatScalingRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameXXKCombatScalingArithmeticTest,
    "GameXXK.Data.CombatScaling.Arithmetic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCombatScalingArithmeticTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Common 101"), FGameXXKCombatScalingRules::ScaleContinuousCeil(101, EGameXXKCardQuality::Common), 101);
    TestEqual(TEXT("Rare 101"), FGameXXKCombatScalingRules::ScaleContinuousCeil(101, EGameXXKCardQuality::Rare), 122);
    TestEqual(TEXT("Epic 101"), FGameXXKCombatScalingRules::ScaleContinuousCeil(101, EGameXXKCardQuality::Epic), 142);

    TestEqual(TEXT("level 10 DOT"), FGameXXKCombatScalingRules::ResolveDotAddition(6, EGameXXKCardQuality::Common, 10), 9);
    TestEqual(TEXT("level 100 common DOT"), FGameXXKCombatScalingRules::ResolveDotAddition(6, EGameXXKCardQuality::Common, 100), 30);
    TestEqual(TEXT("level 100 rare DOT"), FGameXXKCombatScalingRules::ResolveDotAddition(6, EGameXXKCardQuality::Rare, 100), 36);
    TestEqual(TEXT("level 100 epic DOT"), FGameXXKCombatScalingRules::ResolveDotAddition(6, EGameXXKCardQuality::Epic, 100), 42);

    TestEqual(TEXT("DOT cap 25"), FGameXXKCombatScalingRules::ResolveDotCap(25), 25);
    TestEqual(TEXT("DOT cap 26"), FGameXXKCombatScalingRules::ResolveDotCap(26), 50);
    TestEqual(TEXT("DOT cap 100"), FGameXXKCombatScalingRules::ResolveDotCap(100), 100);
    TestEqual(TEXT("DOT cap 135"), FGameXXKCombatScalingRules::ResolveDotCap(135), 150);

    TestEqual(TEXT("cost zero armor"), FGameXXKCombatScalingRules::ResolvePrintedCostArmor(358, 0, EGameXXKCardQuality::Common), 144);
    TestEqual(TEXT("cost one armor"), FGameXXKCombatScalingRules::ResolvePrintedCostArmor(358, 1, EGameXXKCardQuality::Common), 287);
    TestEqual(TEXT("cost two armor"), FGameXXKCombatScalingRules::ResolvePrintedCostArmor(358, 2, EGameXXKCardQuality::Common), 502);
    TestEqual(TEXT("cost three armor"), FGameXXKCombatScalingRules::ResolvePrintedCostArmor(358, 3, EGameXXKCardQuality::Common), 716);
    TestEqual(TEXT("rare cost two armor"), FGameXXKCombatScalingRules::ResolvePrintedCostArmor(358, 2, EGameXXKCardQuality::Rare), 602);
    TestEqual(TEXT("epic cost three armor"), FGameXXKCombatScalingRules::ResolvePrintedCostArmor(358, 3, EGameXXKCardQuality::Epic), 1003);

    TestEqual(TEXT("plus thirty five levels"), FGameXXKCombatScalingRules::ApplyLevelDifferenceCeil(100, 135, 100), 135);
    TestEqual(TEXT("minus thirty five levels"), FGameXXKCombatScalingRules::ApplyLevelDifferenceCeil(100, 100, 135), 65);
    TestEqual(TEXT("upper clamp"), FGameXXKCombatScalingRules::ApplyLevelDifferenceCeil(100, 200, 1), 150);
    TestEqual(TEXT("lower clamp"), FGameXXKCombatScalingRules::ApplyLevelDifferenceCeil(100, 1, 200), 50);
    return true;
}
#endif
```

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CombatScaling.Arithmetic --automation-report InRun01_Task01_RED --json
```

Expected: UBT fails because `GameXXKCombatScalingRules.h` and its methods do not exist.

- [ ] **Step 3: Add the public API**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"

class GAMEXXK_API FGameXXKCombatScalingRules final
{
public:
    static int32 GetQualityPercent(EGameXXKCardQuality Quality);
    static int32 ScaleContinuousCeil(int32 BaseValue, EGameXXKCardQuality Quality);
    static int32 ScaleByPercentCeil(int32 BaseValue, int32 Percent);
    static int32 ResolveDotAddition(int32 BaseCoefficient, EGameXXKCardQuality Quality, int32 TeamMaxLevel);
    static int32 ResolveDotCap(int32 TeamMaxLevel);
    static int32 ResolvePrintedCostArmor(int32 CasterDefense, int32 PrintedEnergyCost, EGameXXKCardQuality Quality);
    static int32 ApplyLevelDifferenceCeil(int32 Damage, int32 SourceLevel, int32 TargetLevel);
};
```

- [ ] **Step 4: Implement overflow-safe integer math**

```cpp
#include "GameXXKCombatScalingRules.h"

namespace
{
    int32 CeilPositiveRatio(const int64 Numerator, const int64 Denominator)
    {
        if (Numerator <= 0 || Denominator <= 0) return 0;
        return static_cast<int32>(FMath::Min<int64>(MAX_int32, (Numerator + Denominator - 1) / Denominator));
    }
}

int32 FGameXXKCombatScalingRules::GetQualityPercent(const EGameXXKCardQuality Quality)
{
    switch (Quality)
    {
    case EGameXXKCardQuality::Rare: return 120;
    case EGameXXKCardQuality::Epic: return 140;
    case EGameXXKCardQuality::Common: return 100;
    default: return 0;
    }
}

int32 FGameXXKCombatScalingRules::ScaleContinuousCeil(const int32 BaseValue, const EGameXXKCardQuality Quality)
{
    return ScaleByPercentCeil(BaseValue, GetQualityPercent(Quality));
}

int32 FGameXXKCombatScalingRules::ScaleByPercentCeil(const int32 BaseValue, const int32 Percent)
{
    return CeilPositiveRatio(static_cast<int64>(FMath::Max(0, BaseValue)) * FMath::Max(0, Percent), 100);
}

int32 FGameXXKCombatScalingRules::ResolveDotAddition(const int32 BaseCoefficient, const EGameXXKCardQuality Quality, const int32 TeamMaxLevel)
{
    const int64 Numerator = static_cast<int64>(FMath::Max(0, BaseCoefficient))
        * GetQualityPercent(Quality) * (FMath::Clamp(TeamMaxLevel, 1, 135) + 25);
    return CeilPositiveRatio(Numerator, 2500);
}

int32 FGameXXKCombatScalingRules::ResolveDotCap(const int32 TeamMaxLevel)
{
    const int32 Level = FMath::Clamp(TeamMaxLevel, 1, 135);
    return FMath::Max(25, 25 * FMath::DivideAndRoundUp(Level, 25));
}

int32 FGameXXKCombatScalingRules::ResolvePrintedCostArmor(const int32 CasterDefense, const int32 PrintedEnergyCost, const EGameXXKCardQuality Quality)
{
    const int32 CostPercent = PrintedEnergyCost <= 0 ? 40 : PrintedEnergyCost == 1 ? 80 : PrintedEnergyCost == 2 ? 140 : 200;
    const int64 Numerator = static_cast<int64>(FMath::Max(0, CasterDefense)) * CostPercent * GetQualityPercent(Quality);
    return CeilPositiveRatio(Numerator, 10000);
}

int32 FGameXXKCombatScalingRules::ApplyLevelDifferenceCeil(const int32 Damage, const int32 SourceLevel, const int32 TargetLevel)
{
    const int32 Percent = 100 + FMath::Clamp(SourceLevel - TargetLevel, -50, 50);
    return ScaleByPercentCeil(Damage, Percent);
}
```

- [ ] **Step 5: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CombatScaling.Arithmetic --automation-report InRun01_Task01_GREEN --json
git add Source/GameXXK/Public/GameXXKCombatScalingRules.h Source/GameXXK/Private/GameXXKCombatScalingRules.cpp Source/GameXXK/Private/Tests/GameXXKCombatScalingRulesTest.cpp
git diff --cached --check
git commit -m "feat: add combat scaling arithmetic"
```

Expected: one Automation success, zero failures/errors.

### Task 2: Replace legacy card-quality multiplication

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardQualityRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardQualityRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardQualityResolutionTest.cpp`

- [ ] **Step 1: Change tests to the approved quality contract**

Replace x2/x4 expectations with:

```cpp
TestEqual(TEXT("Rare scales 101 upward"),
    FGameXXKCombatScalingRules::ScaleContinuousCeil(101, EGameXXKCardQuality::Rare), 122);
TestEqual(TEXT("Epic scales 101 upward"),
    FGameXXKCombatScalingRules::ScaleContinuousCeil(101, EGameXXKCardQuality::Epic), 142);
TestEqual(TEXT("Epic display name"),
    FGameXXKCardQualityRules::GetDisplayName(EGameXXKCardQuality::Epic).ToString(), FString(TEXT("史诗")));
```

For `BuildEffectiveDefinition`, assert damage/heal/fixed/ally-attack effects use 120/140 percent and Draw/Status/Energy/Mana/count effects retain their catalog magnitude unless an explicit card override later changes them.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CardQuality --automation-report InRun01_Task02_RED --json
```

Expected: failures still report Rare doubling, Epic quadrupling, and `珍稀`.

- [ ] **Step 3: Replace `ScaleMagnitude`**

```cpp
int32 ScaleMagnitude(
    const EGameXXKCardEffectType EffectType,
    const int32 BaseMagnitude,
    const EGameXXKCardQuality Quality)
{
    switch (EffectType)
    {
    case EGameXXKCardEffectType::DamagePercentAttack:
    case EGameXXKCardEffectType::DamageFlat:
    case EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget:
    case EGameXXKCardEffectType::Heal:
    case EGameXXKCardEffectType::AddArmor:
        return FGameXXKCombatScalingRules::ScaleContinuousCeil(BaseMagnitude, Quality);
    default:
        return BaseMagnitude;
    }
}
```

Set Epic display text to `史诗`. Do not change catalog classification counts in this task; Plan 2 retires the 25 route cards atomically.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CardQuality --automation-report InRun01_Task02_GREEN --json
git add Source/GameXXK/Private/GameXXKCardQualityRules.cpp Source/GameXXK/Private/Tests/GameXXKCardQualityRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKCardQualityResolutionTest.cpp
git diff --cached --check
git commit -m "feat: adopt 120 and 140 percent card quality"
```

### Task 3: Persist battle scaling context and apply direct-damage levels

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKCardRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardBattleAdapter.h`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKCardBattleScalingIntegrationTest.cpp`

- [ ] **Step 1: Write red integration tests**

Add fixtures with source level 135, target level 100, target Defense 100, no Armor. Store `EnemyDifficultyDamagePercent=150`. Resolve an enemy requested packet of 200 and assert the ordered result is `(200*150%-100)*135%=270`. Add the inverse player level-100 versus enemy level-135 fixture and assert `(200-100)*65%=65`. Add a player fixed-damage packet of 100 against level-135/Defense-500 and assert 65, proving fixed damage bypasses Defense but still receives level difference. Add a DOT fixture asserting visible pool 40 remains 40 rather than 54.

```cpp
Runtime.TeamMaxLevelSnapshot = 100;
Runtime.EnemyDifficultyDamagePercent = 150;
Enemy.CombatLevel = 135;
Target.CombatLevel = 100;
TestEqual(TEXT("difficulty then defense then level"), Result.HealthDamage, 270);
```

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CardBattleRuntime.Scaling --automation-report InRun01_Task03_RED --json
```

- [ ] **Step 3: Add persisted fields and validation**

Add to `FGameXXKCardBattleRuntime`:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
int32 TeamMaxLevelSnapshot = 1;

UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
int32 EnemyDifficultyDamagePercent = 100;

UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
int32 PendingNextRoundEnergyPenalty = 0;
```

Validation accepts TeamMaxLevel 1-135, enemy percentage exactly 100/125/150, and Energy penalty 0-99. `InitializeCardBattleRuntime` computes TeamMaxLevel from living party units and defaults difficulty to 100.

Extend `BeginCardBattle` with an optional trailing difficulty percentage while preserving every existing caller:

```cpp
static bool BeginCardBattle(
    FGameXXKRuntimeState& InOutState,
    EGameXXKNodeKind NodeKind,
    EGameXXKCardTerrain Terrain,
    int32 InitialRandomSeed,
    FString* OutError = nullptr,
    int32 EnemyDifficultyDamagePercent = 100);
```

- [ ] **Step 4: Integrate the ordered damage pipeline**

In `ResolveEnemyDirectAttack`, scale `RequestedDamage` by `EnemyDifficultyDamagePercent` before calling the shared internal resolver. In `ApplyCombatDirectDamageInternal`, after Defense and Mark/Vulnerability but before Armor, call `ApplyLevelDifferenceCeil` using the resolved source and redirected target `CombatLevel`. Fixed-damage effects take the same level-difference branch while preserving their existing Defense bypass. Do not call it for `DamageOverTime`, self loss, or environmental loss.

Record the pre/post level values in `FGameXXKCardDamageResult`:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere)
int32 DamageBeforeLevelDifference = 0;

UPROPERTY(BlueprintReadWrite, EditAnywhere)
int32 DamageAfterLevelDifference = 0;
```

- [ ] **Step 5: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CardBattleRuntime.Scaling --automation-report InRun01_Task03_GREEN --json
git add Source/GameXXK/Public/GameXXKCardTypes.h Source/GameXXK/Public/GameXXKCardRules.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Public/GameXXKCardBattleAdapter.h Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleScalingIntegrationTest.cpp
git diff --cached --check
git commit -m "feat: persist battle scaling context"
```

### Task 4: Remove the Armor-99 cap and add Defense-derived helpers

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCombatStatusRedesignTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroCounterBlockRuntimeTest.cpp`

- [ ] **Step 1: Write red Armor and post-card Block assertions**

```cpp
FGameXXKCardCombatUnit Unit = MakeStatusCapacityUnit();
TestEqual(TEXT("apply 1003 armor"), GameXXKCardRules::AddCombatArmor(Unit, 1003), 1003);
TestEqual(TEXT("armor is not capped at 99"), Unit.Armor, 1003);
```

Add a three-hit enemy-card fixture that leaves 400 Armor after the complete card and assert one Block reaction uses `Attack + 400`, not pre-card Armor and not three reactions.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.HeroCards.CounterBlock --automation-report InRun01_Task04_RED --json
```

- [ ] **Step 3: Remove all gameplay clamps**

Delete `MaxCombatArmor = 99`. Validate `Armor >= 0`; add with int64 and clamp only to `MAX_int32`:

```cpp
const int32 OriginalArmor = FMath::Max(0, InOutUnit.Armor);
InOutUnit.Armor = static_cast<int32>(FMath::Min<int64>(
    MAX_int32,
    static_cast<int64>(OriginalArmor) + Amount));
return InOutUnit.Armor - OriginalArmor;
```

Replace legacy projection `FMath::Clamp(LegacyUnit.Shield, 0, 99)` with `FMath::Max(0, LegacyUnit.Shield)`. Expose one helper that calls `ResolvePrintedCostArmor` from the card owner, printed catalog cost, and instance quality; Plan 2 uses it for Armor effects.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.HeroCards.CounterBlock --automation-report InRun01_Task04_GREEN --json
git add Source/GameXXK/Public/GameXXKCardRules.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/Tests/GameXXKCombatStatusRedesignTest.cpp Source/GameXXK/Private/Tests/GameXXKHeroCounterBlockRuntimeTest.cpp
git diff --cached --check
git commit -m "feat: remove combat armor cap"
```

### Task 5: Convert DOT layers into capped resolved reservoirs

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCombatStatusRedesignTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardOutcomeAuditTest.cpp`

- [ ] **Step 1: Write red reservoir tests**

Cover level 100 coefficient 6 -> 30, adding 80 then 30 -> actual 20/cap 100, trigger damage 100 without consumption, toxic explosion preserving all reservoirs, full Bleed clear, and all-DOT clear.

```cpp
TestEqual(TEXT("first add"), GameXXKCardRules::AddDotFromCoefficient(Runtime, TargetId, EGameXXKCardStatus::Poison, 6, EGameXXKCardQuality::Common), 30);
TestEqual(TEXT("seed eighty poison"),
    GameXXKCardRules::AddCombatStatus(Target, EGameXXKCardStatus::Poison, 80), 80);
TestEqual(TEXT("only cap remainder applies"), GameXXKCardRules::AddDotFromCoefficient(Runtime, TargetId, EGameXXKCardStatus::Poison, 6, EGameXXKCardQuality::Common), 20);
TestEqual(TEXT("trigger does not consume"), TriggeredDamage, 100);
TestEqual(TEXT("reservoir remains"), GameXXKCardRules::GetCombatStatusStacks(Target, EGameXXKCardStatus::Poison), 100);
```

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CombatStatusRedesign --automation-report InRun01_Task05_RED --json
```

- [ ] **Step 3: Add the runtime-aware DOT API**

```cpp
GAMEXXK_API int32 AddDotFromCoefficient(
    FGameXXKCardBattleRuntime& InOutRuntime,
    FName TargetUnitId,
    EGameXXKCardStatus Status,
    int32 BaseCoefficient,
    EGameXXKCardQuality Quality);

GAMEXXK_API int32 ClearDotReservoir(
    FGameXXKCardCombatUnit& InOutUnit,
    EGameXXKCardStatus Status);

GAMEXXK_API int32 ClearAllDotReservoirs(FGameXXKCardCombatUnit& InOutUnit);
```

Accept only Bleed, Poison, Burn, and `DamageOverTime` (Rot). Clamp each to `ResolveDotCap(TeamMaxLevelSnapshot)`. Replace end-phase and explicit trigger code so damage equals the visible reservoir and does not apply quality, difficulty, Defense, Mark, Vulnerability, or level difference again. Remove legacy automatic one-layer consumption from toxic explosion and trigger paths.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CombatStatusRedesign --automation-report InRun01_Task05_GREEN --json
git add Source/GameXXK/Public/GameXXKCardRules.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKCombatStatusRedesignTest.cpp Source/GameXXK/Private/Tests/GameXXKCardOutcomeAuditTest.cpp
git diff --cached --check
git commit -m "feat: add capped dot reservoirs"
```

### Task 6: Resolve Medicine healing once per owner transaction

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCombatScalingRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCombatScalingRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHealerPartnerFormulaRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTaskNpcHealerFormationRuntimeTest.cpp`

- [ ] **Step 1: Add red formula and group-consumption tests**

At TeamMaxLevel 100, coefficient 25 plus Medicine 6 resolves Common healing 155, Rare 186, Epic 217. A three-target group heal consumes Medicine once and gives the full result to all three. Cumulative gains 4+4 grant one Momentum with remainder 2; spending Medicine leaves remainder 2.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.Healer --automation-report InRun01_Task06_RED --json
```

- [ ] **Step 3: Implement the composite helper and transaction snapshot**

```cpp
static int32 ResolveMedicineHealing(
    int32 BaseCoefficient,
    int32 Medicine,
    EGameXXKCardQuality Quality,
    int32 TeamMaxLevel);
```

Implement it by returning `ResolveDotAddition(FMath::Max(0, BaseCoefficient) + FMath::Max(0, Medicine), Quality, TeamMaxLevel)`. In `HealOrReverseWithMedicine`, snapshot and consume owner Medicine once before iterating targets; all recipients use that snapshot. Keep `MedicineGainRemainderByOwner` independent of live Medicine consumption and grant one Momentum for every complete six gained.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.Healer --automation-report InRun01_Task06_GREEN --json
git add Source/GameXXK/Public/GameXXKCombatScalingRules.h Source/GameXXK/Private/GameXXKCombatScalingRules.cpp Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKHealerPartnerFormulaRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKTaskNpcHealerFormationRuntimeTest.cpp
git diff --cached --check
git commit -m "feat: resolve owner scoped medicine healing"
```

### Task 7: Queue next-round enemy Energy penalties

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleScalingIntegrationTest.cpp`

- [ ] **Step 1: Write red refill tests**

Queue penalties 1 and 2 in the same enemy phase. Assert the next player round refills to zero from a base of three, clears the penalty, and the following round refills normally. Assert a +1 hand surcharge remains a separate non-stacking value.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CardBattleRuntime.Scaling.EnergyPenalty --automation-report InRun01_Task07_RED --json
```

- [ ] **Step 3: Add queue and refill logic**

```cpp
bool GameXXKCardRules::QueueNextPlayerRoundEnergyPenalty(
    FGameXXKCardBattleRuntime& InOutRuntime,
    const int32 Amount,
    FString* OutError)
{
    if (Amount <= 0) return SetFailure(OutError, TEXT("Energy penalty must be positive."));
    InOutRuntime.PendingNextRoundEnergyPenalty = FMath::Min(
        99,
        InOutRuntime.PendingNextRoundEnergyPenalty + Amount);
    return true;
}
```

At round start:

```cpp
const int32 Refill = FMath::Max(
    0,
    3 + NewRuntime.BonusSharedEnergyCap + NewRuntime.PendingNextRoundEnergyBonus
        - NewRuntime.PendingNextRoundEnergyPenalty);
NewRuntime.Deck.SharedEnergy = FMath::Min(MaxCardBattleEnergy, Refill);
NewRuntime.PendingNextRoundEnergyBonus = 0;
NewRuntime.PendingNextRoundEnergyPenalty = 0;
```

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CardBattleRuntime.Scaling.EnergyPenalty --automation-report InRun01_Task07_GREEN --json
git add Source/GameXXK/Public/GameXXKCardRules.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleScalingIntegrationTest.cpp
git diff --cached --check
git commit -m "feat: queue enemy energy penalties"
```

### Task 8: Migrate save schema v32 to v33 and run the foundation gate

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKCombatScalingSaveMigrationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp`

- [ ] **Step 1: Write a v32 migration fixture**

Serialize a v32 active battle with party levels 100/80/75, old DOT values, Armor 99, and no scaling snapshot. Assert migration produces v33, TeamMaxLevel 100, enemy damage 100, penalty 0, preserves numeric DOT/Armor exactly, and validates without granting healing or resources.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.SaveMigration --automation-report InRun01_Task08_RED --json
```

- [ ] **Step 3: Add the schema gate**

```cpp
static constexpr int32 CombatScalingFoundationIntroducedSaveVersion = 33;
static constexpr int32 CurrentSaveVersion = 33;
```

For source versions below 33, derive TeamMaxLevel from party units, set enemy percent 100 and penalty 0, preserve existing Armor and DOT integers without re-scaling, then run normal validation. Do not mutate currency, rewards, phase state, deck order, or random state.

- [ ] **Step 4: Run focused and broad green**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.CombatScaling --automation-report InRun01_Foundation_GREEN --json
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.Data.CardBattleRuntime --automation-report InRun01_CardRuntime_GREEN --json
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.SaveMigration --automation-report InRun01_Save_GREEN --json
```

Expected: zero failed/not-run/error tests. Existing classified warnings must be listed, not silently ignored.

- [ ] **Step 5: Commit the migration and record handoff**

```powershell
git add Source/GameXXK/Public/MVP/GameXXKSaveMigration.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp Source/GameXXK/Private/Tests/GameXXKCombatScalingSaveMigrationTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp
git diff --cached --check
git commit -m "feat: migrate combat scaling state"
```

Plan 1 is complete only when `git status --short` contains no new plan-owned modifications and Plan 2 can consume the v33 runtime without altering these formulas.
