# Equipment, Gems, and Idle Progression Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend equipment to item level 135, halve approved equipment/HP-gem contributions, implement the post-Immortal gem curve, and make Training/idle drops use the previous+1-to-current stage band.

**Architecture:** Equipment catalog curves remain deterministic rational curves; only approved HP/Attack/Defense coefficients change. Gem values use one explicit ten-rank table. Training resolves a stable item level from StageId plus reward seed before creating a chest token, so online and offline rewards share the same level authority.

**Tech Stack:** Unreal Engine 5.8 C++, equipment/gem catalogs, Training reward rules, SaveGame migration, UE Automation.

---

## Preconditions and files

Complete Plans 1-4 first; current schema is v36 and Training stages expose `CombatLevel`. Work on `codex/overall-in-run-optimization` and preserve unrelated dirty files.

Create:

- `Source/GameXXK/Private/Tests/GameXXKApprovedEquipmentBudgetTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKTrainingEquipmentLevelBandTest.cpp`

Modify:

- `Source/GameXXK/Public/GameXXKEquipmentTypes.h`
- `Source/GameXXK/Public/GameXXKEquipmentRules.h`
- `Source/GameXXK/Private/GameXXKEquipmentRules.cpp`
- `Source/GameXXK/Private/GameXXKEquipmentCatalog.cpp`
- `Source/GameXXK/Public/GameXXKGemRules.h`
- `Source/GameXXK/Private/GameXXKGemRules.cpp`
- `Source/GameXXK/Public/GameXXKTrainingRules.h`
- `Source/GameXXK/Private/GameXXKTrainingRules.cpp`
- `Source/GameXXK/Private/GameXXKTrainingChestRules.cpp`
- `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- existing equipment, gem, Training reward, and save tests.

---

### Task 1: Lock the ten-rank gem curve

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKGemRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKGemRulesTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKApprovedEquipmentBudgetTest.cpp`

- [ ] **Step 1: Write red exact-rank tests**

```cpp
const int32 ExpectedAttackDefense[] = {1, 2, 4, 8, 16, 20, 25, 32, 40, 50};
const int32 ExpectedHealth[] = {5, 10, 20, 40, 80, 100, 125, 160, 200, 250};
for (int32 Rank = 1; Rank <= 10; ++Rank)
{
    const EGameXXKGemQuality Quality = FGameXXKGemRules::QualityFromRank(Rank);
    TestEqual(TEXT("Attack gem"), FGameXXKGemRules::GetStatBonus(EGameXXKGemType::Attack, Quality), ExpectedAttackDefense[Rank - 1]);
    TestEqual(TEXT("Defense gem"), FGameXXKGemRules::GetStatBonus(EGameXXKGemType::Defense, Quality), ExpectedAttackDefense[Rank - 1]);
    TestEqual(TEXT("Health gem"), FGameXXKGemRules::GetStatBonus(EGameXXKGemType::MaxHealth, Quality), ExpectedHealth[Rank - 1]);
}
```

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.Gems --automation-report InRun05_Task01_RED --json
```

- [ ] **Step 3: Replace bit shifting with the exact table**

```cpp
static constexpr int32 AttackDefenseByRank[] = {0, 1, 2, 4, 8, 16, 20, 25, 32, 40, 50};
const int32 Base = AttackDefenseByRank[Rank];
return Type == EGameXXKGemType::MaxHealth ? Base * 5 : Base;
```

Rank 6 is Treasure: Attack/Defense 20 and HP 100. Do not label rank 10 as Treasure.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.Gems --automation-report InRun05_Task01_GREEN --json
git add Source/GameXXK/Private/GameXXKGemRules.cpp Source/GameXXK/Private/Tests/GameXXKGemRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKApprovedEquipmentBudgetTest.cpp
git diff --cached --check
git commit -m "feat: rebalance gem growth curve"
```

### Task 2: Extend item levels and halve equipment curves

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKEquipmentTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKEquipmentRules.h`
- Modify: `Source/GameXXK/Private/GameXXKEquipmentCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKEquipmentRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKApprovedEquipmentBudgetTest.cpp`

- [ ] **Step 1: Write red level-135 and coefficient tests**

Create item levels 100, 101, 134, and 135; assert validation succeeds and 136 fails. Equip a level-135 item on the level-100 Hero fixture. Assert Mana/Speed curves are unchanged and the approved slot curves are:

```cpp
// LevelOne, GrowthNumerator, GrowthDivisor
Weapon.Attack = {1, 1, 2};
Head.MaxHealth = {4, 1, 1};
Armor.MaxHealth = {2, 1, 2};
Armor.Defense = {1, 1, 6};
Belt.MaxHealth = {3, 1, 2};
Accessory.Attack = {1, 1, 8};
```

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.Equipment --automation-report InRun05_Task02_RED --json
```

- [ ] **Step 3: Raise only item-level limits and replace curves**

Set `MaxEquipmentLevel` and `FGameXXKEquipmentRules::MaxItemLevel` to 135. Do not raise character level. Replace `MakeSlotCoefficients` with the exact approved values above; preserve MaxMana and Speed curves. Keep modern enhancement percentages operating on the reduced base.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.Equipment --automation-report InRun05_Task02_GREEN --json
git add Source/GameXXK/Public/GameXXKEquipmentTypes.h Source/GameXXK/Public/GameXXKEquipmentRules.h Source/GameXXK/Private/GameXXKEquipmentCatalog.cpp Source/GameXXK/Private/GameXXKEquipmentRules.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKApprovedEquipmentBudgetTest.cpp
git diff --cached --check
git commit -m "feat: extend and rebalance equipment levels"
```

### Task 3: Resolve deterministic stage equipment bands

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKTrainingRules.h`
- Modify: `Source/GameXXK/Private/GameXXKTrainingRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKTrainingChestRules.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKTrainingEquipmentLevelBandTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTrainingChestRulesTest.cpp`

- [ ] **Step 1: Write red band tests**

Assert Normal 1-1 resolves interval 1-5; Normal 1-2 6-10; Hard 1-1 46-50; Hell 3-3 131-135. For 1,000 stable seeds, every rolled level lies in the interval and repeating a seed returns the same value. Online Challenge and offline Travel use the same resolver, and a Hell 3-3 chest token preserves a rolled value above 100 without clamping.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Training.EquipmentLevelBand --automation-report InRun05_Task03_RED --json
```

- [ ] **Step 3: Add the pure interval API**

```cpp
static FInt32Interval GetEquipmentLevelBand(FName StageId);
static int32 ResolveEquipmentItemLevel(FName StageId, int32 RewardSeed);
```

Compute `Upper = Stage.CombatLevel`; `Lower = Stage.CombatLevel == 5 ? 1 : Stage.CombatLevel - 4`. Use a local `FRandomStream` mixed from StageId and RewardSeed; never consume the battle random stream. Chest tokens store the resolved item level, not `PlayerLevel`. Replace their hard-coded 100 clamp/validation with `FGameXXKEquipmentRules::MaxItemLevel` in this task so the 131-135 band works before commit.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Training.EquipmentLevelBand --automation-report InRun05_Task03_GREEN --json
git add Source/GameXXK/Public/GameXXKTrainingRules.h Source/GameXXK/Private/GameXXKTrainingRules.cpp Source/GameXXK/Private/GameXXKTrainingChestRules.cpp Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp Source/GameXXK/Private/Tests/GameXXKTrainingEquipmentLevelBandTest.cpp Source/GameXXK/Private/Tests/GameXXKTrainingChestRulesTest.cpp
git diff --cached --check
git commit -m "feat: roll stage band equipment levels"
```

### Task 4: Migrate v36 equipment and chest validation to v37

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleRewardTieringSaveMigrationTest.cpp`

- [ ] **Step 1: Write red migration tests**

Load v36 saves containing item/token levels 100 and verify unchanged. Create a current-state fixture with level 135 and assert validation succeeds. Assert level 136 is rejected. Migration must not recompute existing equipment stats, sockets, affixes, or stored token item levels.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.Equipment.SaveMigration --automation-report InRun05_Task04_RED --json
```

- [ ] **Step 3: Add v37 and replace token clamps**

```cpp
static constexpr int32 EquipmentLevel135IntroducedSaveVersion = 37;
static constexpr int32 CurrentSaveVersion = 37;
```

Confirm migration/current-state validation uses `FGameXXKEquipmentRules::MaxItemLevel` rather than introducing another numeric clamp. Do not mutate old values during migration.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.Equipment --automation-report InRun05_Task04_GREEN --json
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.SaveMigration --automation-report InRun05_Save_GREEN --json
git add Source/GameXXK/Public/MVP/GameXXKSaveMigration.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleRewardTieringSaveMigrationTest.cpp
git diff --cached --check
git commit -m "feat: migrate level 135 equipment"
```

### Task 5: Certify final level-100 Treasure loadouts

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKApprovedEquipmentBudgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentBattleIntegrationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentBudgetObservationTest.cpp`

- [ ] **Step 1: Build the exact six-piece/twelve-gem fixture**

Equip six Treasure pieces and socket four Treasure Attack, four Defense, and four HP gems. Assert socket contribution exactly +80/+80/+400 and the final role checkpoints from design section 5:

```cpp
TestStats(TEXT("Hero"), Snapshot, 2659, 592, 392);
TestStats(TEXT("Blade"), Snapshot, 2371, 563, 285);
TestStats(TEXT("Guard"), Snapshot, 2800, 425, 358);
TestStats(TEXT("Healer"), Snapshot, 2238, 423, 286);
TestStats(TEXT("Hunter"), Snapshot, 2232, 562, 272);
TestStats(TEXT("Sorcerer"), Snapshot, 2094, 495, 257);
TestStats(TEXT("Formation"), Snapshot, 2374, 452, 301);
```

- [ ] **Step 2: Run the budget gate**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Data.Equipment.Budget --automation-report InRun05_Budget_GREEN --json
```

Expected: exact stats and zero failures/errors. If current innate/set/affix inputs make a checkpoint differ, identify the contributing layer and correct the implementation to the approved final checkpoint; do not add the gem package twice.

- [ ] **Step 3: Commit the certification fixture**

```powershell
git add Source/GameXXK/Private/Tests/GameXXKApprovedEquipmentBudgetTest.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentBattleIntegrationTest.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentBudgetObservationTest.cpp
git diff --cached --check
git commit -m "test: certify treasure equipment budget"
```

Plan 5 is complete when level 135 validates end-to-end, first/last reward bands are 1-5 and 131-135, and the balanced Treasure fixture matches all seven role checkpoints without double-counting gems.
