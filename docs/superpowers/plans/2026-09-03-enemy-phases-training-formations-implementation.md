# Enemy Phases and Training Formations Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement all 21 enemies, two/three-phase Elite/Boss battles, ordered visible intent plans, fixed levels 5-135, and the approved 189 Training formations.

**Architecture:** Enemy definitions become data-driven arrays of phase decks with per-difficulty values. A focused phase-rules unit owns lethal clamping and transition state; a separate intent-plan unit locks card identities and non-mutatingly re-forecasts ordered effects. Training owns combat level, difficulty, formation slots, and opening intents, then passes a complete scaling/phase context into the existing card battle.

**Tech Stack:** Unreal Engine 5.8 C++, enemy/card runtime, USTRUCT SaveGame migration, UE Automation, UMG/Slate phase badges, deterministic Training/Travel rules.

---

## Preconditions and guard

- Complete Plans 1-3 first; current save schema must be v35.
- Work on `codex/overall-in-run-optimization` in the root checkout.
- Use design sections 9-16 as exact data authority.
- Do not alter enemy base HP/Attack/Defense curves, add phase stat multipliers, or limit a formation to one phase-bearing unit.
- Preserve all unrelated dirty assets; no Live Coding or 3D-town verification.

## File map

Create:

- `Source/GameXXK/Public/GameXXKEnemyPhaseRules.h`
- `Source/GameXXK/Private/GameXXKEnemyPhaseRules.cpp`
- `Source/GameXXK/Public/GameXXKEnemyIntentPlanRules.h`
- `Source/GameXXK/Private/GameXXKEnemyIntentPlanRules.cpp`
- `Source/GameXXK/Private/Tests/GameXXKEnemyPhaseRulesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKEnemyIntentPlanRulesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKApprovedEnemyCatalogTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKTrainingFormationCatalogTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKEnemyPhaseSaveMigrationTest.cpp`

Modify:

- `Source/GameXXK/Public/GameXXKEnemyTypes.h`
- `Source/GameXXK/Public/GameXXKEnemyCatalog.h`
- `Source/GameXXK/Private/GameXXKEnemyCatalog.cpp`
- `Source/GameXXK/Public/GameXXKCardTypes.h`
- `Source/GameXXK/Public/GameXXKCardRules.h`
- `Source/GameXXK/Private/GameXXKCardRules.cpp`
- `Source/GameXXK/Public/GameXXKCardBattleAdapter.h`
- `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- `Source/GameXXK/Public/GameXXKEnemyText.h`
- `Source/GameXXK/Private/GameXXKEnemyText.cpp`
- `Source/GameXXK/Public/GameXXKTrainingRules.h`
- `Source/GameXXK/Private/GameXXKTrainingRules.cpp`
- `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- `Source/GameXXK/Public/UI/GameXXKBattleStateBadgeRules.h`
- `Source/GameXXK/Private/UI/GameXXKBattleStateBadgeRules.cpp`
- `Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp`
- `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- existing enemy, Training, Board, and save tests.

---

### Task 1: Replace Boss-only phase fields with a data-driven phase model

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKEnemyTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKEnemyCatalog.h`
- Modify: `Source/GameXXK/Private/GameXXKEnemyCatalog.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKEnemyPhaseRulesTest.cpp`

- [ ] **Step 1: Write red data-contract tests**

Construct Hard and Hell Elite fixtures and assert total phases 2/3, current phase 1, remaining numbers `[2]` and `[3,2]`. Assert a Normal Boss has one total phase. Assert phase definitions are numbered contiguously and each has a nonempty deck. Persist the selected enemy difficulty in battle runtime and assert it agrees with the existing 100/125/150 damage percentage.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Battle.EnemyPhase --automation-report InRun04_Task01_RED --json
```

- [ ] **Step 3: Append shared enums and structs without renumbering existing values**

Put the difficulty enum/value wrapper in `GameXXKEnemyTypes.h`, which is already safe for `GameXXKCardTypes.h` to include:

```cpp
UENUM(BlueprintType)
enum class EGameXXKEnemyDifficulty : uint8
{
    Normal = 0,
    Hard = 1,
    Hell = 2
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEnemyDifficultyInt
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 Normal = 0;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 Hard = 0;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 Hell = 0;

    int32 Resolve(EGameXXKEnemyDifficulty Difficulty) const;
};

```

After `FGameXXKEnemyIntentDefinition` in `GameXXKEnemyCatalog.h`, add the phase definition there so UHT sees a complete intent type and no circular include is introduced:

```cpp
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEnemyPhaseDefinition
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 PhaseNumber = 1;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) TArray<FGameXXKEnemyIntentDefinition> Intents;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 ArmorRetentionPercent = 0;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 FirstStatusGuardDefensePercent = 0;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 HealMissingHealthPercentOnBleedingPrey = 0;
};
```

Add `TArray<FGameXXKEnemyPhaseDefinition> Phases` to `FGameXXKEnemyDefinition`. Keep the existing `Intents` array temporarily as a deprecated phase-one compatibility input so this first commit still compiles against the legacy catalog; a shared catalog accessor returns `Phases[CurrentPhase-1].Intents` when phases exist and otherwise returns legacy `Intents` only for definitions not yet converted. Validation rejects a converted definition whose legacy and phase-one arrays are both populated. Retain old PhaseTwo fields only as deprecated serialized migration inputs, never as live authority after a definition has `Phases`.

Add live state:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 CurrentPhase = 1;
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 TotalPhases = 1;
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 PhaseTransitionSerial = 0;
```

Add the battle-wide intent-value context to `FGameXXKCardBattleRuntime`:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
EGameXXKEnemyDifficulty EnemyDifficulty = EGameXXKEnemyDifficulty::Normal;
```

Training maps its existing `EGameXXKTrainingDifficulty` to this enum at battle creation. Runtime validation requires Normal/Hard/Hell to agree with `EnemyDifficultyDamagePercent` 100/125/150; previews, execution, and save/load read the enum rather than reconstructing difficulty from display text.

Remaining ink numbers derive from Total/Current; do not store a second mutable token array.

- [ ] **Step 4: Add phase-count resolution**

```cpp
int32 FGameXXKEnemyCatalog::ResolveTotalPhases(const EGameXXKEnemyTier Tier, const EGameXXKEnemyDifficulty Difficulty)
{
    if (Tier == EGameXXKEnemyTier::Normal) return 1;
    if (Difficulty == EGameXXKEnemyDifficulty::Hell) return 3;
    if (Difficulty == EGameXXKEnemyDifficulty::Hard) return 2;
    return 1;
}
```

- [ ] **Step 5: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Battle.EnemyPhase --automation-report InRun04_Task01_GREEN --json
git add Source/GameXXK/Public/GameXXKEnemyTypes.h Source/GameXXK/Public/GameXXKEnemyCatalog.h Source/GameXXK/Private/GameXXKEnemyCatalog.cpp Source/GameXXK/Public/GameXXKCardTypes.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKEnemyPhaseRulesTest.cpp
git diff --cached --check
git commit -m "feat: add data driven enemy phases"
```

### Task 2: Implement packet-safe enemy phase transitions

**Files:**
- Create: `Source/GameXXK/Public/GameXXKEnemyPhaseRules.h`
- Create: `Source/GameXXK/Private/GameXXKEnemyPhaseRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEnemyPhaseRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEnemyMechanicsTest.cpp`

- [ ] **Step 1: Write red transition tests**

Cover exact 1%, lethal from above 1%, one packet not overflowing, later hits of the same multi-hit card crossing phase two and phase three, several enemies transitioning from one AoE, negative clear, positive/Armor retention, 100% heal, intent/charge cancellation, no inserted enemy attack, and Deer healing unable to restore tokens.

```cpp
TestEqual(TEXT("lethal packet clamps old phase"), FirstHit.HealthDamage, 99);
TestEqual(TEXT("phase advances"), EnemyState.CurrentPhase, 2);
TestEqual(TEXT("new phase fully heals"), Enemy.HP, Enemy.MaxHP);
TestEqual(TEXT("wealth retained"), GetStatus(Enemy, EGameXXKCardStatus::Wealth), 4);
TestEqual(TEXT("poison cleared"), GetStatus(Enemy, EGameXXKCardStatus::Poison), 0);
TestTrue(TEXT("old charge cancelled"), EnemyState.PendingChargedIntentId.IsNone());
```

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Battle.EnemyPhase.Transition --automation-report InRun04_Task02_RED --json
```

- [ ] **Step 3: Implement the focused API**

```cpp
struct GAMEXXK_API FGameXXKEnemyPhaseTransitionResult
{
    bool bTransitioned = false;
    FName EnemyUnitId = NAME_None;
    int32 PreviousPhase = 1;
    int32 NewPhase = 1;
    int32 Healing = 0;
    TArray<EGameXXKCardStatus> ClearedStatuses;
};

class GAMEXXK_API FGameXXKEnemyPhaseRules final
{
public:
    static int32 ClampHealthDamageForRemainingPhase(
        const FGameXXKCardBattleRuntime& Runtime,
        const FGameXXKCardCombatUnit& Target,
        int32 RequestedHealthDamage);
    static bool ResolveAfterDamagePacket(
        FGameXXKCardBattleRuntime& InOutRuntime,
        FName EnemyUnitId,
        FGameXXKEnemyPhaseTransitionResult& OutResult,
        FString* OutError = nullptr);
};
```

Clamp to leave HP 1 while a token remains. Define the integer transition boundary as `max(1, MaxHP / 100)` so enemies below 100 maximum HP still consume a phase after lethal clamping; after the packet commits, transition when HP is at or below that boundary. Increment current phase and serial, clear all negative statuses/DOT, retain positive status/Armor, heal to MaxHP, reset intent cursor, cancel charge/locked target for that source, rearm phase-specific passives, and leave turn ownership unchanged.

During the chapter-by-chapter conversion window, definitions with an empty `Phases` array continue through the existing deprecated Boss transition path unchanged. The new path is mandatory as soon as a definition has `Phases`; Task 10 removes the fallback after proving all 21 definitions converted. This keeps every intermediate commit playable and prevents half-converted chapters from losing their current Boss behavior.

- [ ] **Step 4: Extend damage results for presentation**

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere) bool bTriggeredEnemyPhase = false;
UPROPERTY(BlueprintReadWrite, EditAnywhere) int32 EnemyPhaseBefore = 1;
UPROPERTY(BlueprintReadWrite, EditAnywhere) int32 EnemyPhaseAfter = 1;
UPROPERTY(BlueprintReadWrite, EditAnywhere) int32 EnemyPhaseHealing = 0;
```

Every player, DOT, replay, Heavy Arrow, Counter, and Block packet calls `ResolveAfterDamagePacket` after its own commit and before terminal evaluation. A later packet sees the healed phase.

- [ ] **Step 5: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Battle.EnemyPhase.Transition --automation-report InRun04_Task02_GREEN --json
git add Source/GameXXK/Public/GameXXKEnemyPhaseRules.h Source/GameXXK/Private/GameXXKEnemyPhaseRules.cpp Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKEnemyPhaseRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKEnemyMechanicsTest.cpp
git diff --cached --check
git commit -m "feat: resolve packet safe enemy phases"
```

### Task 3: Add difficulty-aware enemy intent effects

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKEnemyTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKEnemyCatalog.h`
- Modify: `Source/GameXXK/Private/GameXXKEnemyCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKApprovedEnemyCatalogTest.cpp`

- [ ] **Step 1: Write red schema validation tests**

Assert DirectDamage has positive AttackPercent by difficulty, DOT statuses have coefficients, Armor has DefensePercent, group/self targets are legal, phases are contiguous, intent IDs are unique within one enemy, and deprecated phase stat multipliers are never non-100.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.EnemyCatalog.Approved --automation-report InRun04_Task03_RED --json
```

- [ ] **Step 3: Extend effect data append-only**

Append effect types needed by the approved data:

```cpp
QueueNextRoundEnergyPenalty,
TriggerDamageOverTime,
HealMaxHealthPercent,
AddArmorDefensePercent,
RefreshHealingAmplification,
ConsumeWealthForDamage,
ConsumeWealthForHealing
```

Add difficulty values to effects:

```cpp
UPROPERTY(BlueprintReadOnly, EditAnywhere) FGameXXKEnemyDifficultyInt AttackPercentByDifficulty;
UPROPERTY(BlueprintReadOnly, EditAnywhere) FGameXXKEnemyDifficultyInt StatusAmountByDifficulty;
UPROPERTY(BlueprintReadOnly, EditAnywhere) FGameXXKEnemyDifficultyInt DefensePercentByDifficulty;
UPROPERTY(BlueprintReadOnly, EditAnywhere) FGameXXKEnemyDifficultyInt ResourceAmountByDifficulty;
```

`BuildResolvedIntent` obtains intents only through the shared current-phase catalog accessor, resolves the selected difficulty once, converts DOT coefficients through `AddDotFromCoefficient`, computes Armor from the source enemy's current Defense, and sends Energy denial to the saved next-round penalty.

Validate the saved difficulty enum against the 100/125/150 damage percentage before preview or execution so a corrupt or partially migrated battle cannot show Hard status values while dealing Normal damage.

For this schema commit, update shared enemy-effect builders to wrap every not-yet-converted scalar as `{value, value, value}` and keep its current behavior. Chapter Tasks 4-6 then replace those triples with approved Normal/Hard/Hell values and phase arrays. No intermediate commit may leave a legacy direct/status/Armor effect resolving to the new struct's zero default.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.EnemyCatalog.Approved --automation-report InRun04_Task03_GREEN --json
git add Source/GameXXK/Public/GameXXKEnemyTypes.h Source/GameXXK/Public/GameXXKEnemyCatalog.h Source/GameXXK/Private/GameXXKEnemyCatalog.cpp Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/Tests/GameXXKApprovedEnemyCatalogTest.cpp
git diff --cached --check
git commit -m "feat: add difficulty aware enemy intents"
```

### Task 4: Encode Chapter 1 enemy decks and passives

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKEnemyCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKApprovedEnemyCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEnemyMechanicsTest.cpp`

- [ ] **Step 1: Add red exact Chapter 1 rows**

Assert twelve ordinary resolved cases per difficulty, fourteen phase-one intents per difficulty, thirteen phase-two intents for Hard/Hell, and thirteen phase-three intents for Hell. Lock Ironfeather 80/115/150 x3, corrected 160 x3 and 180 x4 later-phase values, Bluehorn retention 50/75/100, and Money Rat Wealth/penalty/healing rules.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.EnemyCatalog.Approved.Chapter1 --automation-report InRun04_Task04_RED --json
```

- [ ] **Step 3: Encode design sections 10.1, 11.1, and 12.1**

Use phase arrays and difficulty structs. Temporary attack buffs affect only not-yet-acted enemies and expire at enemy-phase end. Wealth cap 8; phase transition retains it. Money Rat ally-damage Wealth triggers exclude DOT/reactions and respect the per-enemy-phase cap.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.EnemyCatalog.Approved.Chapter1 --automation-report InRun04_Task04_GREEN --json
git add Source/GameXXK/Private/GameXXKEnemyCatalog.cpp Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKApprovedEnemyCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKEnemyMechanicsTest.cpp
git diff --cached --check
git commit -m "feat: author chapter one enemy phases"
```

### Task 5: Encode Chapter 2 enemy decks and enemy reactions

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKEnemyCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKApprovedEnemyCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp`

- [ ] **Step 1: Add red Chapter 2 and reaction tests**

Lock 12/14/13/13 intent counts as in Task 4. Assert Porcupine gets visible Block only from Bristle Guard; a player multi-hit single-target card causes one post-card Block using remaining Armor; group/DOT/reactions do not trigger. Assert Redtusk gains one Rage per active card, Graymane's 1.2 Marked multiplier, and Black Bear Thick Hide after Armor.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.EnemyCatalog.Approved.Chapter2 --automation-report InRun04_Task05_RED --json
```

- [ ] **Step 3: Encode design sections 10.2, 11.2, and 12.2**

Use the approved 150x3/180x3 Graymane corrections, Rage caps/consumption, Weak/Bleed/Burn coefficients, Counter/Block counts, and no old Black Bear phase stat mutation or added hits.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.EnemyCatalog.Approved.Chapter2 --automation-report InRun04_Task05_GREEN --json
git add Source/GameXXK/Private/GameXXKEnemyCatalog.cpp Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKApprovedEnemyCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp
git diff --cached --check
git commit -m "feat: author chapter two enemy phases"
```

### Task 6: Encode Chapter 3 enemy decks and persistent Prey

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKEnemyCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKApprovedEnemyCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp`

- [ ] **Step 1: Add red Chapter 3 tests**

Lock 12/14/13/13 counts. Assert Vulture is the only Chapter 3 ordinary Burn source; Giant Toad uses one refresh-only healing amplification, not Medicine; White Ape guards first negative per eligible enemy/round; Deer cooldown fallbacks; Tiger Prey lifecycle, retargeting, redirect interaction, and once-per-card missing-HP healing.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.EnemyCatalog.Approved.Chapter3 --automation-report InRun04_Task06_RED --json
```

- [ ] **Step 3: Encode design sections 10.3, 11.3, and 12.3**

Prey attacks fall back to lowest HP when Tiger is absent. First-phase Pounce clears Prey; phase two persists and may switch via Tail Sweep; phase three persists until death. Remove Tiger-owned Prey when Tiger dies. Toad Tongue consumes its one healing amplification even on overheal.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.EnemyCatalog.Approved.Chapter3 --automation-report InRun04_Task06_GREEN --json
git add Source/GameXXK/Private/GameXXKEnemyCatalog.cpp Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKApprovedEnemyCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp
git diff --cached --check
git commit -m "feat: author chapter three enemy phases"
```

### Task 7: Lock and refresh ordered enemy intent plans

**Files:**
- Create: `Source/GameXXK/Public/GameXXKEnemyIntentPlanRules.h`
- Create: `Source/GameXXK/Private/GameXXKEnemyIntentPlanRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKEnemyIntentPlanRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardEnemyIntentPresentationTest.cpp`

- [ ] **Step 1: Write red ordered-plan tests**

Use Ironfeather/Money Rat/Bluehorn and Graymane/Redtusk/Black Bear fixtures. Assert speed order, left-to-right ties, setup statuses affecting later displayed damage, fixed intent IDs through player-state refresh, source death removing its setup, phase transition selecting the new phase's first card, and save/load preserving the same plan.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Battle.EnemyIntentPlan --automation-report InRun04_Task07_RED --json
```

- [ ] **Step 3: Add a saved lock model and pure forecaster**

```cpp
USTRUCT(BlueprintType)
struct FGameXXKEnemyIntentLock
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName SourceUnitId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName IntentId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 PhaseNumber = 1;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 RoundNumber = 0;
};

// Add to FGameXXKCardBattleRuntime.
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
TArray<FGameXXKEnemyIntentLock> LockedEnemyIntents;

class GAMEXXK_API FGameXXKEnemyIntentPlanRules final
{
public:
    static bool LockForPlayerRound(FGameXXKCardBattleRuntime& InOutRuntime, FString* OutError = nullptr);
    static bool BuildForecast(
        const FGameXXKCardBattleRuntime& Runtime,
        TArray<FGameXXKCardEnemyIntent>& OutIntents,
        FString* OutError = nullptr);
};
```

`LockForPlayerRound` selects identities once. `BuildForecast` copies runtime, orders sources by Speed then stable slot, resolves each locked effect into the copy, and emits final numbers without committing RNG, cursor, HP, status, charge, or resources. Player card commits refresh forecast values but never reroll IDs. At phase end, execution consumes the same locks and advances cursor exactly once.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Battle.EnemyIntentPlan --automation-report InRun04_Task07_GREEN --json
git add Source/GameXXK/Public/GameXXKEnemyIntentPlanRules.h Source/GameXXK/Private/GameXXKEnemyIntentPlanRules.cpp Source/GameXXK/Public/GameXXKCardTypes.h Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/Tests/GameXXKEnemyIntentPlanRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardEnemyIntentPresentationTest.cpp
git diff --cached --check
git commit -m "feat: forecast ordered enemy plans"
```

### Task 8: Author fixed Training levels, formations, and openings

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKTrainingRules.h`
- Modify: `Source/GameXXK/Private/GameXXKTrainingRules.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKTrainingFormationCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTrainingRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTrainingTravelVisualRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp`

- [ ] **Step 1: Write red 27-stage/189-formation tests**

Assert stage levels are exactly 5..135 by five, every stage has 4 Normal + 2 Elite + 1 Boss encounters, all formations have three slots and explicit opening intents, every one of 21 enemies appears, stage-end cores match the 3x3 table, and stage 3 uses both Elites plus the true Boss. Assert non-Boss Training encounters retain Plain and stage-end Boss encounters retain Cave even without a Formation partner (terrain exists, but Plan 3 forbids automatic income). Assert a party unit at level 101 is rejected, an enemy unit at level 135 is accepted, and an enemy at level 136 is rejected.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Training.FormationCatalog --automation-report InRun04_Task08_RED --json
```

- [ ] **Step 3: Replace parallel pools with explicit slots**

```cpp
USTRUCT(BlueprintType)
struct FGameXXKTrainingEnemySlotDefinition
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, EditAnywhere) FName EnemyDefinitionId = NAME_None;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) FName OpeningIntentId = NAME_None;
};

UPROPERTY(BlueprintReadOnly, EditAnywhere)
TArray<FGameXXKTrainingEnemySlotDefinition> EnemySlots;

UPROPERTY(BlueprintReadOnly, EditAnywhere)
int32 CombatLevel = 1;
```

Build the exact 27 rows from design section 13. Preserve `EnemyDefinitionIds` only as a derived compatibility projection if an existing UI still consumes it. Use the stage CombatLevel in `BuildTrainingEnemyProjection`, never `PlayerLevel`. Map difficulty to 100/125/150 damage and 1/2/3 phase totals.

In card-runtime validation, keep party units bounded by `FGameXXKCharacterStatRules::MaxCharacterLevel` (100) and validate enemy units against the new explicit enemy maximum 135. Do not raise the character-level constant.

Populate each saved `OpeningIntentId` with this deterministic authoring table:

| Enemy | Normal opening | Context/duplicate override |
|---|---|---|
| Rooster | `Peck` | use `DoublePeck` when faster Weasel supplies Mark; first of duplicate Roosters uses `Crow` |
| Goat | `Horn` | second duplicate Goat uses `Stomp` |
| Weasel | `Harass` | none |
| Civet | `Feint` | none |
| Gray Wolf | `Bite` | first duplicate Gray Wolf uses `CallPack` |
| Boar | `Tusk` | use `ArmorBreakCharge` with Gray Wolf/Graymane; second duplicate Boar uses `Bristle` |
| Macaque | `Snatch` | none |
| Porcupine | `Quill` | none |
| Venom Snake | `VenomBite` | second duplicate Snake uses `Coil` |
| Wildcat | `Rake` | first duplicate Wildcat uses `Stalk` |
| Vulture | `Gaze` | none |
| Giant Toad | `PoisonFog` | none |
| Ironfeather | `RapidPeck` | none |
| Bluehorn | `Pierce` | none |
| Money Rat | `GreedyMark` | none |
| Graymane | `HuntMark` | none |
| Redtusk | `Earthquake` | none |
| Black Bear | `Rend` | none |
| White Ape | `ThrowRock` | none |
| Deer | `TerrainBless` | none |
| Tiger | `MarkPrey` | none |

Phase transition always replaces the opening with the first listed card of the new phase deck.

- [ ] **Step 4: Keep Travel and Challenge on the same authored formation**

Update Travel runtime creation to consume `EnemySlots`. Do not downgrade three enemies to two or substitute pool indices. Initial intent IDs are ignored only by Travel's simplified numeric runner; its displayed identities/order must still match Challenge.

- [ ] **Step 5: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Training --automation-report InRun04_Task08_GREEN --json
git add Source/GameXXK/Public/GameXXKTrainingRules.h Source/GameXXK/Private/GameXXKTrainingRules.cpp Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKTrainingFormationCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKTrainingRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKTrainingTravelVisualRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp
git diff --cached --check
git commit -m "feat: author all training formations"
```

### Task 9: Project phase ink and fully resolved intent text

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleStateBadgeRules.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleStateBadgeRules.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp`
- Modify: `Source/GameXXK/Public/GameXXKEnemyText.h`
- Modify: `Source/GameXXK/Private/GameXXKEnemyText.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleStateBadgeRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardEnemyIntentPresentationTest.cpp`

- [ ] **Step 1: Write red phase badge and intent text tests**

Assert Hard phase one displays `二`; Hell phase one displays `三`,`二`; after one transition only `三`; no cleanse/removal API can select them. Assert compact ordinary/phase-one intent faces use at most two semantic rows and include target scope plus their high final number/status payload without exposing authoring percentages. Assert the full tooltip includes per-hit final damage and hit count, resolved DOT amount, Armor/heal, charge remaining, phase number, and conditional bonus. Phase-two/three cards may use longer multi-effect face text.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.UI.Battle.EnemyIntent --automation-report InRun04_Task09_RED --json
```

- [ ] **Step 3: Add phase projection models**

Derive phase badges from Total/Current. Use stable IconIds `EnemyPhase.2` and `EnemyPhase.3`, fallback glyphs `二` and `三`, and priority above ordinary combat status. Build order descending so Hell renders `三 二`, while transition consumes the rightmost `二` first.

- [ ] **Step 4: Format resolved intent cards**

`FGameXXKEnemyText` consumes only `FGameXXKCardEnemyIntent` resolved fields; it must not recalculate from catalog percentages. Provide separate compact-face and full-tooltip formatters. Multi-hit text uses `final-per-hit × count`; ordinary/phase-one compact faces combine target and the one or two primary payloads, while phase-two/three faces may list every authored effect. Tooltips always enumerate every resolved effect in execution order.

- [ ] **Step 5: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.UI.Battle.EnemyIntent --automation-report InRun04_Task09_GREEN --json
git add Source/GameXXK/Public/UI/GameXXKBattleStateBadgeRules.h Source/GameXXK/Private/UI/GameXXKBattleStateBadgeRules.cpp Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp Source/GameXXK/Public/GameXXKEnemyText.h Source/GameXXK/Private/GameXXKEnemyText.cpp Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Private/Tests/GameXXKBattleStateBadgeRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardEnemyIntentPresentationTest.cpp
git diff --cached --check
git commit -m "feat: present enemy phases and final intents"
```

### Task 10: Migrate v35 phase state and certify 351 intent cases

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Modify: `Source/GameXXK/Private/GameXXKEnemyCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKEnemyPhaseSaveMigrationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKApprovedEnemyCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEnemyCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp`

- [ ] **Step 1: Write v35 migration and exact-count tests**

Map old Boss `bPhaseTwo=true` to current phase 2 without healing/clearing. Infer total and `EnemyDifficulty` from active Training difficulty/tier; when Training context is absent, map the persisted 100/125/150 damage percentage to Normal/Hard/Hell. Legacy non-Training old Bosses retain two total phases. Invalid pending old-phase charge is cleared and visibly reforecast without execution. Assert 108 ordinary + 126 phase-one + 78 phase-two + 39 phase-three resolved cases = 351, and assert all 21 converted definitions have empty legacy `Intents` plus neutral deprecated phase-stat fields.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.SaveMigration.EnemyPhase --automation-report InRun04_Task10_RED --json
```

- [ ] **Step 3: Add v36**

```cpp
static constexpr int32 EnemyPhaseAndTrainingFormationIntroducedSaveVersion = 36;
static constexpr int32 CurrentSaveVersion = 36;
```

Migration preserves HP, Armor, positive/negative status values, cursor where valid, deck zones, rewards, and RNG. It never retroactively performs a transition or grant. After the migration fixture is green, remove the temporary live legacy-intent/Boss-transition fallback; deprecated fields remain readable only by migration code.

- [ ] **Step 4: Run cold and broad gates**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Battle.Enemy --automation-report InRun04_Enemy_GREEN --json
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.Training --automation-report InRun04_Training_GREEN --json
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.SaveMigration --automation-report InRun04_Save_GREEN --json
```

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Public/MVP/GameXXKSaveMigration.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp Source/GameXXK/Private/GameXXKEnemyCatalog.cpp Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKEnemyPhaseSaveMigrationTest.cpp Source/GameXXK/Private/Tests/GameXXKApprovedEnemyCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKEnemyCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp
git diff --cached --check
git commit -m "test: certify enemy phase catalog"
```

Plan 4 is complete only when 351 cases and 189 formations pass, three phase-bearing units can coexist, and a real player action can alter the visible later intent without rerolling its card identity.
