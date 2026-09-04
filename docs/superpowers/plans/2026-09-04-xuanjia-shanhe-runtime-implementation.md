# Xuanjia and Shanhe Runtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to implement this plan task-by-task. The user explicitly prohibited subagents, so execute inline and stop at the listed checkpoints. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the approved terrain payloads, NPC battle equipment projection, and complete Xuanjia/Shanhe 2/4/6 runtime semantics with save-safe previews, logs, tooltips, and tests.

**Architecture:** Keep equipment loadouts authoritative in `FGameXXKEquipmentRules`, materialize all three deployed party snapshots in `FGameXXKCardBattleAdapter`, and consume immutable descriptors in `GameXXKCardRules`. Add one secondary descriptor magnitude so the approved dual-effect tiers remain data-driven. Route every newly generated Armor packet through one source-aware helper; keep copies, retention, restoration, and refunds on the existing unscaled path.

**Tech Stack:** Unreal Engine 5.8 C++, USTRUCT SaveGame data, UE Automation Tests, project MCP/UBT scripts, Python workbook exporters.

---

## File map

- `Source/GameXXK/Public/GameXXKEquipmentSetCatalog.h`: stable 2/4/6 catalog schema and secondary value.
- `Source/GameXXK/Private/GameXXKEquipmentSetCatalog.cpp`: approved player text, hooks, primary/secondary values.
- `Source/GameXXK/Public/GameXXKCardTypes.h`: persisted active-effect secondary magnitude and per-effect counters.
- `Source/GameXXK/Private/GameXXKEquipmentRules.cpp`: descriptor projection, known-effect validation, team-source selection.
- `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`: Hero/partner/NPC stat and equipment snapshots; enemy-card health-loss handoff.
- `Source/GameXXK/Public/GameXXKCardRules.h`: reaction-boundary input and focused test hooks.
- `Source/GameXXK/Private/GameXXKCardRules.cpp`: approved terrain payloads and Xuanjia/Shanhe consumers.
- `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`, `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`: active-battle descriptor migration.
- `Source/GameXXK/Private/Tests/GameXXKEquipmentBattleIntegrationTest.cpp`: NPC battle equipment projection.
- `Source/GameXXK/Private/Tests/GameXXKApprovedTerrainPayloadTest.cpp`: six terrain payloads and round-start order.
- `Source/GameXXK/Private/Tests/GameXXKXuanJiaSetRuntimeTest.cpp`: Xuanjia 2/4/6 behavior.
- `Source/GameXXK/Private/Tests/GameXXKShanHeSetRuntimeTest.cpp`: Shanhe 2/4/6 behavior.
- `Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp`: v34-to-v35 active-effect migration.
- `scripts/export_game_design_tables.py`, `scripts/export_game_equipment_design_table.py`: approved spreadsheet rows.
- `docs/production/2026-09-04-xuanjia-shanhe-runtime-acceptance.md`: implementation evidence.

## Execution preflight

Before the first C++ edit or cold build, save and close only this project's interactive editor:

```powershell
python -c "import sys; sys.path.insert(0,'scripts'); from ue_tdd_pipeline import save_running_editor_before_close,kill_editor; ok=save_running_editor_before_close(); ok=ok and kill_editor(); raise SystemExit(0 if ok else 1)"
```

Keep the interactive editor closed during the TDD tasks. UnrealEditor-Cmd processes started by Automation are disposable verification processes and do not replace the final visible PIE check.

### Task 1: Replace legacy descriptors with approved data

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKEquipmentSetCatalog.h`
- Modify: `Source/GameXXK/Private/GameXXKEquipmentSetCatalog.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKEquipmentRules.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKEquipmentStatRulesTest.cpp`

- [ ] **Step 1: Write a failing catalog contract test**

Add assertions beside the existing set-definition checks:

```cpp
const FGameXXKEquipmentSetBonusDefinition* Xuan2 =
    FGameXXKEquipmentSetCatalog::FindDefinition(TEXT("Set.XuanJia.2"));
const FGameXXKEquipmentSetBonusDefinition* Xuan4 =
    FGameXXKEquipmentSetCatalog::FindDefinition(TEXT("Set.XuanJia.4"));
const FGameXXKEquipmentSetBonusDefinition* Xuan6 =
    FGameXXKEquipmentSetCatalog::FindDefinition(TEXT("Set.XuanJia.6"));
const FGameXXKEquipmentSetBonusDefinition* Shan2 =
    FGameXXKEquipmentSetCatalog::FindDefinition(TEXT("Set.ShanHe.2"));
const FGameXXKEquipmentSetBonusDefinition* Shan4 =
    FGameXXKEquipmentSetCatalog::FindDefinition(TEXT("Set.ShanHe.4"));
const FGameXXKEquipmentSetBonusDefinition* Shan6 =
    FGameXXKEquipmentSetCatalog::FindDefinition(TEXT("Set.ShanHe.6"));
TestEqual(TEXT("Xuanjia two-piece uses ten percent"), Xuan2->Value, 1000);
TestEqual(TEXT("Xuanjia four-piece retains fifty percent"), Xuan4->Value, 5000);
TestEqual(TEXT("Xuanjia four-piece adds eighty-percent attack"), Xuan4->SecondaryValue, 80);
TestEqual(TEXT("Xuanjia six-piece grants forty-percent Defense Armor"), Xuan6->Value, 4000);
TestEqual(TEXT("Xuanjia six-piece grants one Guard link"), Xuan6->SecondaryValue, 1);
TestEqual(TEXT("Shanhe two-piece draws one"), Shan2->Value, 1);
TestEqual(TEXT("Shanhe four-piece discounts one"), Shan4->Value, 1);
TestEqual(TEXT("Shanhe four-piece restores two Mana"), Shan4->SecondaryValue, 2);
TestEqual(TEXT("Shanhe six-piece triggers once"), Shan6->Value, 1);
```

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```powershell
python scripts/ai_production_loop.py --run-ubt
```

Expected: compile FAIL because `SecondaryValue` does not exist. This is the RED gate; do not run the stale pre-change editor binary.

- [ ] **Step 3: Add the data fields and approved rows**

Add to `FGameXXKEquipmentSetBonusDefinition` and `FGameXXKEquipmentActiveEffect`:

```cpp
UPROPERTY(BlueprintReadOnly, EditAnywhere)
int32 SecondaryValue = 0;

UPROPERTY(BlueprintReadOnly, VisibleAnywhere, SaveGame)
int32 SecondaryMagnitude = 0;
```

Append `Equipment = 10` to `EGameXXKCardResolutionOrigin` so equipment damage and terrain have a stable telemetry origin without changing existing serialized values.

Extend `MakeBonus` with `const int32 SecondaryValue = 0`, copy it in `MakeSetEffect`, and validate it in `IsKnownActiveEffect`. Replace the six rows with:

```cpp
MakeBonus(TEXT("Set.XuanJia.2"), TEXT("穿戴者产生的护甲提高10%。"),
    EGameXXKEquipmentSet::XuanJia, 2, K::XuanJiaArmorGain, S::Owner, H::Passive, U::BasisPoints, 1000),
MakeBonus(TEXT("Set.XuanJia.4"), TEXT("我方回合开始时保留50%护甲；每个敌方回合首次格挡后，追加80%攻击伤害。"),
    EGameXXKEquipmentSet::XuanJia, 4, K::XuanJiaArmorRetentionCounter, S::Owner, H::RoundStart, U::BasisPoints, 5000, 1, 80),
MakeBonus(TEXT("Set.XuanJia.6"), TEXT("全队唯一。敌方回合首次有友方因攻击损失气血后，全体获得穿戴者40%防御的护甲；穿戴者援护其他友方各1次。"),
    EGameXXKEquipmentSet::XuanJia, 6, K::XuanJiaTeamGuard, S::Team, H::FirstAllyHealthDamagePerRound, U::BasisPoints, 4000, 1, 1),
MakeBonus(TEXT("Set.ShanHe.2"), TEXT("每回合首次打出地势牌后，抽1张。"),
    EGameXXKEquipmentSet::ShanHe, 2, K::ShanHeTerrainPower, S::Owner, H::TerrainSynergyCard, U::FlatCount, 1),
MakeBonus(TEXT("Set.ShanHe.4"), TEXT("每回合首张地势牌少耗1气；结算后，其他友方各回复2内力。"),
    EGameXXKEquipmentSet::ShanHe, 4, K::ShanHeTerrainCardFormation, S::Owner, H::TerrainSynergyCard, U::FlatCount, 1, 1, 2),
MakeBonus(TEXT("Set.ShanHe.6"), TEXT("全队唯一。每个我方回合开始时，由穿戴者触发1次当前地势。"),
    EGameXXKEquipmentSet::ShanHe, 6, K::ShanHeTeamFormationCore, S::Team, H::RoundStart, U::FlatCount, 1),
```

Place `SecondaryValue` after `TriggersPerRound` in the helper signature so all unchanged rows retain default zero.

- [ ] **Step 4: Run the focused test and descriptor validation**

Run:

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Equipment.Stats
```

Expected: cold build and tests PASS, with all six exact descriptions and values recognized by `IsKnownActiveEffect`.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Public/GameXXKEquipmentSetCatalog.h Source/GameXXK/Private/GameXXKEquipmentSetCatalog.cpp Source/GameXXK/Public/GameXXKCardTypes.h Source/GameXXK/Private/GameXXKEquipmentRules.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentStatRulesTest.cpp
git commit -m "feat: encode approved Xuanjia and Shanhe descriptors"
```

### Task 2: Project the deployed NPC's equipment into card battles

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKEquipmentBattleIntegrationTest.cpp`

- [ ] **Step 1: Change the integration fixture to equip the named NPC**

After selecting `Npc.TusiChief`, equip a full Xuanjia set and build its expected snapshot:

```cpp
if (!AddAndEquipFullSet(*this, State, TEXT("Npc.TusiChief"), EGameXXKEquipmentSet::XuanJia))
{
    return false;
}
FGameXXKCharacterStats NpcBareStats;
NpcBareStats.MaxHealth = TaskNpcAttributes.Health;
NpcBareStats.MaxMana = TaskNpcAttributes.Mana;
NpcBareStats.Attack = TaskNpcAttributes.Attack;
NpcBareStats.Defense = TaskNpcAttributes.Defense;
NpcBareStats.Speed = TaskNpcAttributes.Speed;
FGameXXKEquipmentLoadoutSnapshot NpcSnapshot;
TestTrue(TEXT("builds NPC equipment snapshot"),
    FGameXXKEquipmentRules::BuildLoadoutSnapshot(
        State.EquipmentCollection, TEXT("Npc.TusiChief"), NpcBareStats, NpcSnapshot, &Error));
```

Replace the old “ignores player equipment” assertions with exact `NpcSnapshot.AttributesBeforeRoute` assertions, and require at least one `EquipmentEffects` source matching the NPC ID.

- [ ] **Step 2: Run the integration test and verify RED**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.Equipment.BattleIntegration
```

Expected: FAIL because `BuildActiveBattleParty` and `MaterializeEquipmentBattleEffects` omit the NPC snapshot.

- [ ] **Step 3: Build one shared NPC snapshot in the adapter**

Add an internal helper and use it in both call sites:

```cpp
bool BuildQuestNpcEquipmentSnapshot(
    const FGameXXKRuntimeState& State,
    const FName QuestNpcId,
    const int32 QuestNpcLevel,
    FGameXXKCompanionAttributes& OutAttributes,
    FGameXXKEquipmentLoadoutSnapshot& OutSnapshot,
    FString* OutError)
{
    if (!FGameXXKCompanionRules::GetQuestNpcAttributes(
            QuestNpcId, QuestNpcLevel, OutAttributes, OutError)) return false;
    FGameXXKCharacterStats Bare;
    Bare.MaxHealth = OutAttributes.Health;
    Bare.MaxMana = OutAttributes.Mana;
    Bare.Attack = OutAttributes.Attack;
    Bare.Defense = OutAttributes.Defense;
    Bare.Speed = OutAttributes.Speed;
    return FGameXXKEquipmentRules::BuildLoadoutSnapshot(
        State.EquipmentCollection, QuestNpcId, Bare, OutSnapshot, OutError);
}
```

Use `OutSnapshot.AttributesBeforeRoute` for NPC HP/MP/Attack/Defense/Speed, add the snapshot to `Snapshots`, and let `ResolveTeamEffects` consider all three deployed owners.

- [ ] **Step 4: Run integration and owner-validation tests**

Run:

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Equipment.BattleIntegration+GameXXK.Equipment.QuestNpcOwner"
```

Expected: PASS; Hero, partner, and NPC descriptors are unique and survive save/load.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentBattleIntegrationTest.cpp
git commit -m "fix: project NPC equipment into card battles"
```

### Task 3: Migrate the six approved terrain payloads

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKApprovedTerrainPayloadTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKFormationMasterPartnerRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroFormationRuntimeTest.cpp`

- [ ] **Step 1: Add a test-only terrain entry point and RED fixtures**

Declare under `#if WITH_DEV_AUTOMATION_TESTS`:

```cpp
static bool ResolveTerrainBenefitForTest(
    FGameXXKCardBattleRuntime& InOutRuntime,
    FName SourceUnitId,
    EGameXXKCardTerrain Terrain,
    int32 Repetitions,
    FGameXXKCardPlayResult& OutResult,
    FString* OutError = nullptr);
```

Create a level-100 fixture with source Defense301, two allies, and three enemies. Assert one repetition produces:

```cpp
// Plain: Burn 10 on every enemy.
// Cliff: Vulnerability 2 and Mark 1 on every enemy.
// Forest: 50 requested healing per ally.
// WaterShore/Ferry: +3 Mana per ally.
// Village: draw 1 and 61 Armor per ally.
// Cave: 121 Armor and one Block registration per ally.
```

Add the internal audited DOT helper used by the implementation snippet:

```cpp
bool GrantDotCoefficientAndRecord(
    FGameXXKCardBattleRuntime& Runtime,
    FName SourceUnitId,
    FGameXXKCardCombatUnit& Target,
    EGameXXKCardStatus Status,
    int32 BaseCoefficient,
    EGameXXKCardQuality Quality,
    FGameXXKCardPlayResult* Result,
    FString& Error);
```

- [ ] **Step 2: Run the terrain tests and verify RED**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.Data.Terrain.ApprovedPayloads
```

Expected: FAIL on old single-target Plain/Cliff, healing4, Armor4/8.

- [ ] **Step 3: Replace the payload switch without changing trigger-count logic**

Inside `ResolveTerrainBenefit`, keep `EffectiveRepetitions` and modifier consumption unchanged. Replace only the terrain payload:

```cpp
case EGameXXKCardTerrain::Plain:
    for (FGameXXKCardCombatUnit& Enemy : InOutRuntime.Units)
        if (Enemy.bLiving && Enemy.Side != Owner->Side)
            GrantDotCoefficientAndRecord(InOutRuntime, SourceInstance.OwnerUnitId,
                Enemy, EGameXXKCardStatus::Burn, 2, EGameXXKCardQuality::Common,
                InOutResult, OutError);
    break;
case EGameXXKCardTerrain::Forest:
    ApplyAndRecordHealing(*InOutResult, Owner->UnitId, Ally,
        FGameXXKCombatScalingRules::ResolveMedicineHealing(
            10, 0, EGameXXKCardQuality::Common, InOutRuntime.TeamMaxLevelSnapshot));
    break;
case EGameXXKCardTerrain::Village:
    if (!GameXXKCardRules::DrawCards(InOutRuntime.Deck, 1, 0, &OutError)) return false;
    GrantGeneratedArmor(InOutRuntime, InOutResult, Owner->UnitId, Ally,
        ResolveDefensePercentArmorAmount(*Owner, 20, EGameXXKCardQuality::Common), true);
    break;
case EGameXXKCardTerrain::Cave:
    GrantGeneratedArmor(InOutRuntime, InOutResult, Owner->UnitId, Ally,
        ResolveDefensePercentArmorAmount(*Owner, 40, EGameXXKCardQuality::Common), true);
    if (!RegisterPartyReactionUses(InOutRuntime, SourceInstance, Ally.UnitId,
            EGameXXKCardStatus::Block, 1, OutError)) return false;
    break;
```

Implement Cliff as a stable loop over all living enemies. Reuse DOT caps and status audit results; do not call `AddCombatStatus` directly for Burn.

- [ ] **Step 4: Update old exact-number fixtures and run all formation tests**

Replace assertions that expect healing4/Armor4/8 with coefficient-derived values at their fixture level/Defense. Run:

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Data.Terrain+GameXXK.Data.HeroCards.Formation+GameXXK.Data.PartnerCards.Formation"
```

Expected: PASS for all seven code terrains and six conceptual payloads.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Public/GameXXKCardRules.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKApprovedTerrainPayloadTest.cpp Source/GameXXK/Private/Tests/GameXXKFormationMasterPartnerRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKHeroFormationRuntimeTest.cpp
git commit -m "feat: apply approved terrain payloads"
```

### Task 4: Add the ordinary round-start terrain event

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKApprovedTerrainPayloadTest.cpp`

- [ ] **Step 1: Add RED tests for first and later rounds**

Build a battle with a living permanent Formation Master partner and Village terrain. Assert:

```cpp
TestEqual(TEXT("initial hand is five before terrain"), HandBeforeTerrain, 5);
TestEqual(TEXT("Village automatic trigger grows hand to six"), Runtime.Deck.Hand.Num(), 6);
TestEqual(TEXT("later round refills to five before Village draws"), Runtime.Deck.Hand.Num(), 6);
TestEqual(TEXT("dead Formation partner disables ordinary automatic terrain"), DrawDelta, 0);
```

- [ ] **Step 2: Run and verify RED**

Run the terrain filter. Expected: FAIL because no ordinary automatic round-start terrain consumer exists.

- [ ] **Step 3: Implement one ordered round-start helper**

Add:

```cpp
bool ResolveRoundStartTerrainBenefits(
    FGameXXKCardBattleRuntime& InOutRuntime,
    FGameXXKCardPlayResult& OutResult,
    FString& OutError);
```

It must find the living party unit with Role `FormationMaster`, build a stable synthetic source instance, and resolve one current-terrain repetition after normal hand refill. Call it:

- in `BeginCardBattle`, after initial hand and bonus draw are complete;
- in `BeginNextPlayerCardRound`, after Armor clearing/retention, Mana/Energy refill, and normal hand refill.

- [ ] **Step 4: Run terrain and card-battle round tests**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Data.Terrain.ApprovedPayloads+GameXXK.Data.CardBattleRuntime"
```

Expected: PASS; Village draw is not swallowed by the hand-limit refill.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Public/GameXXKCardRules.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/Tests/GameXXKApprovedTerrainPayloadTest.cpp
git commit -m "feat: trigger terrain at player round start"
```

### Task 5: Implement Xuanjia two-piece source Armor amplification

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKXuanJiaSetRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`

- [ ] **Step 1: Write RED source/copy tests**

Use a Defense358 wearer and assert:

```cpp
TestEqual(TEXT("80 percent source Armor becomes 316"), GeneratedArmor, 316);
TestEqual(TEXT("140 percent source Armor becomes 553"), GroupArmor, 553);
TestEqual(TEXT("another caster's Armor is not amplified"), ForeignArmor, BaseForeignArmor);
TestEqual(TEXT("PriorEffectResult copy is not amplified twice"), CopiedArmor, OriginalResolvedArmor);
TestEqual(TEXT("retained Armor is not amplified"), RetainedArmor, ExpectedRetainedArmor);
```

- [ ] **Step 2: Run Xuanjia tests and verify RED**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.Equipment.XuanJia
```

Expected: FAIL because generated Armor ignores the descriptor.

- [ ] **Step 3: Centralize generated Armor scaling**

Replace the internal helper with a source-aware form:

```cpp
int32 ResolveGeneratedArmorAmount(
    const FGameXXKCardBattleRuntime& Runtime,
    const FName SourceUnitId,
    const int32 BaseArmor)
{
    int32 BasisPoints = 0;
    for (const FGameXXKEquipmentBattleEffectRuntime& Effect : Runtime.EquipmentEffects)
        if (Effect.SourceCharacterId == SourceUnitId
            && Effect.ActiveEffect.ModifierKind == EGameXXKEquipmentModifierKind::ArmorGain
            && Effect.ActiveEffect.Unit == EGameXXKEquipmentMagnitudeUnit::BasisPoints)
            BasisPoints += Effect.ActiveEffect.Magnitude;
    const int64 Numerator = static_cast<int64>(BaseArmor) * (10000 + BasisPoints);
    return static_cast<int32>(FMath::Min<int64>(MAX_int32, (Numerator + 9999) / 10000));
}
```

Add the single mutation/audit entry point used by later tasks:

```cpp
int32 GrantGeneratedArmor(
    FGameXXKCardBattleRuntime& Runtime,
    FGameXXKCardPlayResult* Result,
    FName SourceUnitId,
    FGameXXKCardCombatUnit& Target,
    int32 BaseArmor,
    bool bApplyEquipmentAmplification)
{
    const int32 Requested = bApplyEquipmentAmplification
        ? ResolveGeneratedArmorAmount(Runtime, SourceUnitId, BaseArmor)
        : FMath::Max(0, BaseArmor);
    return Result
        ? ApplyAndRecordArmor(*Result, SourceUnitId, Target, Requested)
        : GameXXKCardRules::AddCombatArmor(Target, Requested);
}
```

Pass `Runtime` into `ApplyAndRecordArmor`. Add `bGeneratedArmor` to call sites: true for newly calculated card/formula/terrain/overflow/set packets; false for `PriorEffectResult`, current-Armor copies, fixed refunds, restoration, doubling, and retention.

- [ ] **Step 4: Run Xuanjia plus existing Armor/Ice tests**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Equipment.XuanJia+GameXXK.Data.Combat.Armor+GameXXK.Data.PartnerCards.Sorcerer"
```

Expected: PASS with no double scaling of Mana-overflow copies or consumed-Armor refunds.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKXuanJiaSetRuntimeTest.cpp
git commit -m "feat: amplify Xuanjia Armor generation"
```

### Task 6: Implement Xuanjia four-piece retention

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKXuanJiaSetRuntimeTest.cpp`

- [ ] **Step 1: Add RED odd/even and explicit-retention cases**

```cpp
TestEqual(TEXT("300 Armor retains 150"), BeginNextRoundWithArmor(300), 150);
TestEqual(TEXT("301 Armor floors to 150"), BeginNextRoundWithArmor(301), 150);
TestEqual(TEXT("explicit full retention wins"), BeginNextRoundWithFullRetain(301), 301);
TestEqual(TEXT("non-wearer clears Armor"), NonWearerArmorAfterBoundary, 0);
```

- [ ] **Step 2: Run and verify RED**

Run `GameXXK.Equipment.XuanJia`. Expected: old boundary clears all Armor.

- [ ] **Step 3: Apply retention at the single Armor-clear boundary**

Inside `ClearArmorAtSidePhaseStart`, after checking explicit full-retain IDs and before `BeginCombatUnitPhase`, locate the wearer's `Set.XuanJia.4` descriptor:

```cpp
FGameXXKEquipmentBattleEffectRuntime* FindEquipmentEffectById(
    FGameXXKCardBattleRuntime& Runtime, FName SourceUnitId, FName EffectId);
```

```cpp
if (Side == EGameXXKCardTargetSide::Party
    && FindEquipmentEffectById(InOutRuntime, Unit.UnitId, TEXT("Set.XuanJia.4")))
{
    Unit.Armor = static_cast<int32>(static_cast<int64>(Unit.Armor) * 5000 / 10000);
    continue;
}
```

Use the descriptor magnitude rather than literal5000 in production code.

- [ ] **Step 4: Run Xuanjia and ordinary retention tests**

Expected: PASS; `RetainArmorNextRound` remains full retention.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKXuanJiaSetRuntimeTest.cpp
git commit -m "feat: retain half Armor with Xuanjia"
```

### Task 7: Implement Xuanjia four-piece Block follow-up

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKXuanJiaSetRuntimeTest.cpp`

- [ ] **Step 1: Add RED reaction-boundary tests**

Cover one Block, multiple Block registrations in one enemy card, a second enemy card in the same phase, a group card, and next enemy phase reset. Assert exactly one equipment-origin 80% packet per enemy phase.

```cpp
TestEqual(TEXT("first blocked card adds one Xuanjia packet"), XuanDamageCount, 1);
TestEqual(TEXT("multiple Block sources still add one packet"), XuanDamageCount, 1);
TestEqual(TEXT("second enemy card adds no second packet"), XuanDamageCount, 1);
TestEqual(TEXT("next enemy phase rearms"), NextPhaseXuanDamageCount, 1);
```

- [ ] **Step 2: Run and verify RED**

Expected: only ordinary Block damage is present.

- [ ] **Step 3: Append one nonrecursive equipment reaction**

In `ResolvePartyReactionsAfterEnemyCard`, collect whether a Block reaction from the Xuanjia wearer actually resolved. Add these focused helpers:

```cpp
bool TryConsumeEquipmentRoundUse(
    FGameXXKCardBattleRuntime& Runtime,
    FGameXXKEquipmentBattleEffectRuntime& Effect,
    FString& Error);
bool ResolveEquipmentReactionDamage(
    FGameXXKCardBattleRuntime& Runtime,
    FName SourceUnitId,
    FName TargetUnitId,
    int32 RequestedDamage,
    FName EffectId,
    TArray<FGameXXKCardDamageResult>& OutResults,
    FString& Error);
```

After all ordinary reactions:

```cpp
if (bWearerBlocked && TryConsumeEquipmentRoundUse(Runtime, Xuan4Effect, OutError))
{
    const int32 Requested = FGameXXKCombatScalingRules::ScaleByPercentCeil(
        Wearer->Attack, Xuan4Effect.ActiveEffect.SecondaryMagnitude);
    ResolveEquipmentReactionDamage(Runtime, Wearer->UnitId, EnemySourceUnitId,
        Requested, TEXT("Set.XuanJia.4"), NewResults, OutError);
}
```

Set the resolution origin to equipment/reaction, apply normal player Defense/status/level rules, and never reopen `ResolvePartyReactionsAfterEnemyCard`.

- [ ] **Step 4: Run Xuanjia, Guard, and Blade counterflow tests**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Equipment.XuanJia+GameXXK.Data.PartnerCards.Guard+GameXXK.Data.PartnerCards.Blade"
```

Expected: PASS with unchanged existing reaction counts.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKXuanJiaSetRuntimeTest.cpp
git commit -m "feat: add Xuanjia Block follow-up"
```

### Task 8: Implement Xuanjia six-piece team rescue

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKXuanJiaSetRuntimeTest.cpp`

- [ ] **Step 1: Add RED eligibility and ordering tests**

Test single-target, group, multi-hit, full Armor absorption, Poison, QingNang self-loss, source death, and existing Guard link. Verify rescue happens after ordinary Block damage and gives159 Armor per ally for Defense358 with the cumulative two-piece.

- [ ] **Step 2: Run and verify RED**

Expected: no team rescue is produced.

- [ ] **Step 3: Pass actual health loss to the completed-card boundary**

Extend the reaction function with a defaulted final parameter:

```cpp
bool ResolvePartyReactionsAfterEnemyCard(
    FGameXXKCardBattleRuntime& InOutRuntime,
    FName EnemySourceUnitId,
    EGameXXKCardDamageKind CompletedCardKind,
    FName FinalRecipientUnitId,
    TArray<FGameXXKCardDamageResult>& OutReactionDamageResults,
    FString* OutError = nullptr,
    bool bAnyPartyHealthLostFromCompletedDirectAttack = false);
```

In `ResolveNextEnemyIntent`, compute the bool only from direct-attack results whose source is the current intent and `HealthDamage > 0`; do not include reactive, DOT, or self-loss results.

- [ ] **Step 4: Resolve rescue after ordinary reactions**

After Xuanjia4 and existing reactions, find the team-unique `Set.XuanJia.6` source. If eligible and unused this enemy phase:

```cpp
bool AddOrIncrementGuardLink(
    FGameXXKCardBattleRuntime& Runtime,
    FName GuardianUnitId,
    FName ProtectedUnitId,
    int32 Uses,
    FString& Error);
```

```cpp
const int32 BaseArmor = static_cast<int32>(FMath::Min<int64>(
    MAX_int32,
    (static_cast<int64>(Wearer->Defense) * Xuan6Effect.ActiveEffect.Magnitude + 9999) / 10000));
for (FGameXXKCardCombatUnit& Ally : Runtime.Units)
    if (Ally.bLiving && Ally.Side == EGameXXKCardTargetSide::Party)
        GrantGeneratedArmor(Runtime, nullptr, Wearer->UnitId, Ally, BaseArmor, true);
for (const FGameXXKCardCombatUnit& Ally : Runtime.Units)
    if (Ally.bLiving && Ally.Side == EGameXXKCardTargetSide::Party
        && Ally.UnitId != Wearer->UnitId)
        AddOrIncrementGuardLink(Runtime, Wearer->UnitId, Ally.UnitId,
            Xuan6Effect.ActiveEffect.SecondaryMagnitude, OutError);
```

Commit the round counter only after all grants succeed so failure is atomic.

- [ ] **Step 5: Run Xuanjia and enemy-intent tests**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Equipment.XuanJia+GameXXK.Data.EnemyIntents"
```

Expected: PASS; no trigger from DOT/self-loss and no mid-volley protection.

- [ ] **Step 6: Commit**

```powershell
git add Source/GameXXK/Public/GameXXKCardRules.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/Tests/GameXXKXuanJiaSetRuntimeTest.cpp
git commit -m "feat: add Xuanjia team rescue"
```

### Task 9: Classify terrain cards and preview Shanhe four-piece cost

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKShanHeSetRuntimeTest.cpp`

- [ ] **Step 1: Create RED classification and preview tests**

Test Hero terrain, partner switch, Yue Bai terrain, an ordinary attack, an automatic replay, and a temporary actively played copy. Require the first wearer terrain card to preview one less Energy and the second to use printed/other-modifier cost.

- [ ] **Step 2: Run and verify RED**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.Equipment.ShanHe
```

Expected: no catalog-based Shanhe discount exists.

- [ ] **Step 3: Add one stable classifier**

```cpp
bool IsTerrainActiveCard(const FGameXXKCardDefinition& Definition)
{
    return Definition.Effects.ContainsByPredicate([](const FGameXXKCardEffect& Effect)
    {
        return Effect.Type == EGameXXKCardEffectType::ChangeTerrain
            || Effect.Type == EGameXXKCardEffectType::TriggerTerrainBenefit;
    });
}
```

In `BuildEffectiveCardEnergyCost`, after existing free/reduction modifiers, locate the owner's unused `Set.ShanHe.4`; subtract `Magnitude`, clamp to zero, and return the effect ID in this non-persisted preview field. Do not mutate runtime during preview.

```cpp
UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
FName AppliedShanHeFourPieceEffectId = NAME_None;
```

- [ ] **Step 4: Recompute and consume only after successful play**

At commit, rebuild the same cost and mark the exact Shanhe4 descriptor used only after the active transaction succeeds. Failed preview, target, or resolution leaves `LastTriggerRound` unchanged.

- [ ] **Step 5: Run Shanhe plus cost-preview tests and commit**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Equipment.ShanHe+GameXXK.Data.CardOutcomePreview"
git add Source/GameXXK/Public/GameXXKCardTypes.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKShanHeSetRuntimeTest.cpp
git commit -m "feat: discount the first Shanhe terrain card"
```

### Task 10: Resolve Shanhe two/four-piece post-card rewards

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKShanHeSetRuntimeTest.cpp`

- [ ] **Step 1: Add RED post-transaction tests**

Use a terrain card with forced discard/search. Assert the base choice resolves first, then draw1, then other allies gain2 Mana; owner gains none. Add automatic replay and failed-play controls.

- [ ] **Step 2: Run and verify RED**

Expected: discount may work from Task9, but no post-card rewards occur.

- [ ] **Step 3: Add a persisted pending equipment reward**

```cpp
USTRUCT(BlueprintType)
struct FGameXXKPendingEquipmentRewardRuntime
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName SourceUnitId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName DrawEffectId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName ManaEffectId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 TriggerRound = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 DrawCount = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 OtherAllyMana = 0;
};
```

Store a stable `PendingEquipmentRewards` array on `FGameXXKCardBattleRuntime`. Validate source, exact two/four-piece effect IDs, current round, and positive payload. Entries materialize only when no pending choice or automatic queue remains.

Add these helpers:

```cpp
FName FindUnusedShanHeTwoEffectId(
    const FGameXXKCardBattleRuntime& Runtime, FName SourceUnitId);
bool QueuePendingEquipmentReward(
    FGameXXKCardBattleRuntime& Runtime,
    const FGameXXKPendingEquipmentRewardRuntime& Reward,
    FString& Error);
bool MaterializePendingEquipmentRewards(
    FGameXXKCardBattleRuntime& Runtime,
    FString& Error);
```

- [ ] **Step 4: Add a single ordered resolver**

```cpp
bool ResolveShanHeAfterSuccessfulTerrainActive(
    FGameXXKCardBattleRuntime& Runtime,
    const FGameXXKCardDefinition& Definition,
    FName OwnerUnitId,
    FName AppliedFourPieceEffectId,
    FGameXXKCardPlayResult& Result,
    FString& Error)
{
    if (!IsTerrainActiveCard(Definition)) return true;
    FGameXXKPendingEquipmentRewardRuntime Reward;
    Reward.SourceUnitId = OwnerUnitId;
    Reward.TriggerRound = Runtime.RoundNumber;
    Reward.DrawEffectId = FindUnusedShanHeTwoEffectId(Runtime, OwnerUnitId);
    Reward.ManaEffectId = AppliedFourPieceEffectId;
    Reward.DrawCount = Reward.DrawEffectId.IsNone() ? 0 : 1;
    Reward.OtherAllyMana = Reward.ManaEffectId.IsNone() ? 0 : 2;
    return QueuePendingEquipmentReward(Runtime, Reward, Error);
}
```

Queue it after the active card commits. `MaterializePendingEquipmentRewards` runs after choices/replays, draws first, then restores Mana to other living allies, and finally marks the exact two/four-piece descriptors used. Failure leaves the candidate transaction unchanged.

- [ ] **Step 5: Run Shanhe, queue, and task-search tests**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Equipment.ShanHe+GameXXK.Data.HeroCards.Foundation+GameXXK.Data.TaskNpcCards"
```

Expected: PASS; automatic replays do not consume or retrigger the set.

- [ ] **Step 6: Commit**

```powershell
git add Source/GameXXK/Public/GameXXKCardTypes.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKShanHeSetRuntimeTest.cpp
git commit -m "feat: resolve Shanhe terrain-card rewards"
```

### Task 11: Implement Shanhe six-piece round-start formation core

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKShanHeSetRuntimeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKApprovedTerrainPayloadTest.cpp`

- [ ] **Step 1: Add RED first/later-round tests**

Cover no Formation partner, a living Formation partner, a dead six-piece source, duplicate six-piece wearers, Village draw order, and Cave Armor source Defense.

- [ ] **Step 2: Run and verify RED**

Expected: only the ordinary Formation partner trigger from Task4 runs.

- [ ] **Step 3: Extend the round-start helper**

After the ordinary partner trigger, find the already team-unique `Set.ShanHe.6` descriptor and resolve exactly `Magnitude` current-terrain repetitions using its living wearer as source. Use equipment origin and bypass active-card-only terrain count modifiers.

Add helpers with stable-ID selection:

```cpp
const FGameXXKEquipmentBattleEffectRuntime* FindTeamEquipmentEffectById(
    const FGameXXKCardBattleRuntime& Runtime, FName EffectId);
FGameXXKCardInstance MakeEquipmentSourceInstance(
    const FGameXXKEquipmentBattleEffectRuntime& Effect, int32 RoundNumber);
```

```cpp
if (const FGameXXKEquipmentBattleEffectRuntime* Shan6 = FindTeamEquipmentEffectById(Runtime, TEXT("Set.ShanHe.6")))
{
    if (const FGameXXKCardCombatUnit* Wearer = FindCombatUnitById(Runtime.Units, Shan6->SourceCharacterId);
        Wearer && Wearer->bLiving && Wearer->Side == EGameXXKCardTargetSide::Party)
        ResolveTerrainBenefit(Runtime, MakeEquipmentSourceInstance(*Shan6, Runtime.RoundNumber), NAME_None,
            Runtime.Terrain, Shan6->ActiveEffect.Magnitude,
            EGameXXKCardResolutionOrigin::Equipment, &OutResult, Error);
}
```

- [ ] **Step 4: Run all terrain/Shanhe tests**

Expected: Village begins control with six cards; Cave uses each source's own Defense; duplicate team sources do not stack.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/Tests/GameXXKShanHeSetRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKApprovedTerrainPayloadTest.cpp
git commit -m "feat: trigger the Shanhe formation core"
```

### Task 12: Migrate active-battle descriptors safely

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Modify: `Source/GameXXK/Private/GameXXKEquipmentRules.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKBladePartnerCardMigrationTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKBattleRewardTieringSaveMigrationTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCompanionBirthPoolMigrationTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKHeroCardUnlockMigrationTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCombatScalingSaveMigrationTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKNarrativeGuideSaveMigrationTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKRouteEconomySaveMigrationTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKPermanentNpcSaveMigrationTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKRetiredRouteCardMigrationTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKRetiredLegacyNarrativeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKTalentSaveMigrationTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKTutorialMapItemTest.cpp`

- [ ] **Step 1: Add a v34 RED save fixture**

Create an active battle with old Xuanjia/Shanhe effects (`SecondaryMagnitude=0`, old magnitudes), serialize as save version34, migrate, and assert:

```cpp
TestEqual(TEXT("current schema is v35"), FGameXXKSaveMigration::CurrentSaveVersion, 35);
TestEqual(TEXT("Xuan4 retention normalized"), Xuan4.ActiveEffect.Magnitude, 5000);
TestEqual(TEXT("Xuan4 attack normalized"), Xuan4.ActiveEffect.SecondaryMagnitude, 80);
TestEqual(TEXT("Shan4 Mana normalized"), Shan4.ActiveEffect.SecondaryMagnitude, 2);
TestEqual(TEXT("no past trigger is backfilled"), Shan4.CurrentRoundTriggerCount, 0);
```

- [ ] **Step 2: Run migration test and verify RED**

Expected: current version34 and old snapshots fail new known-effect validation.

- [ ] **Step 3: Add schema35 normalization**

Append `EquipmentSetRuntimeSemanticsSaveVersion = 35`. Add `FGameXXKEquipmentRules::NormalizeKnownSetEffectDescriptor` that copies catalog Set/Scope/Hook/Modifier/Magnitude/Secondary/Unit/MaxTriggers while preserving source identity. During v34 migration, normalize only Xuanjia/Shanhe descriptors and clear their never-before-used counters; do not alter PoJun/QingNang/ZhuiFeng/ShiGu progress.

Update every direct `CurrentSaveVersion == 34`, “writes v34”, and current-schema text assertion in the files listed above to35. Keep `ActiveCardPool173IntroducedSaveVersion == 34` unchanged because it remains the historical introduction boundary.

- [ ] **Step 4: Run migration and active-battle save tests**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Equipment.SaveMigration+GameXXK.Equipment.BattleIntegration"
```

Expected: PASS; no round-start event is executed by migration.

- [ ] **Step 5: Commit**

```powershell
git add Source/GameXXK/Public/MVP/GameXXKSaveMigration.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp Source/GameXXK/Private/GameXXKEquipmentRules.cpp Source/GameXXK/Private/Tests/GameXXK*MigrationTest.cpp Source/GameXXK/Private/Tests/GameXXKTutorialMapItemTest.cpp
git commit -m "feat: migrate approved equipment set runtime"
```

### Task 13: Refresh tooltip/export text and spreadsheets

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `scripts/export_game_design_tables.py`
- Modify: `scripts/export_game_equipment_design_table.py`
- Modify: `docs/design/2026-09-04-project-design-tables/GameXXK_装备设计总表_2026-09-04.xlsx`
- Test: `Source/GameXXK/Private/Tests/GameXXKEquipmentStatRulesTest.cpp`

- [ ] **Step 1: Add exact display-text assertions**

Assert the six catalog descriptions equal the player text in the approved spec and no old `5%`, `12%`, or `相邻队友` text remains.

- [ ] **Step 2: Update exporter rows**

Replace the six `SET_BONUSES` rows with the approved text and status `已批准；消费者已实装`. Add shared glossary rows:

```python
("地势牌", "会改变或触发地势的主动牌。"),
("全队唯一", "同名团队套装只生效1份。"),
```

Add one stable verbose audit at each runtime trigger:

```cpp
UE_LOG(LogTemp, Verbose,
    TEXT("[EquipmentSet] effect=%s source=%s round=%d primary=%d secondary=%d"),
    *Effect.ActiveEffect.EffectId.ToString(),
    *Effect.SourceCharacterId.ToString(),
    Runtime.RoundNumber,
    ResolvedPrimary,
    ResolvedSecondary);
```

Use resolved Armor/damage/draw/Mana/terrain counts for the two numeric fields. Do not log failed previews as successful triggers.

- [ ] **Step 3: Regenerate and reload the workbook**

```powershell
python scripts/export_game_equipment_design_table.py
python -c "from openpyxl import load_workbook; p=r'docs/design/2026-09-04-project-design-tables/GameXXK_装备设计总表_2026-09-04.xlsx'; w=load_workbook(p,read_only=True); assert '05_套装效果' in w.sheetnames; print('PASS')"
```

Expected: `PASS`; workbook contains all18 set thresholds and approved six rows.

- [ ] **Step 4: Run descriptor tests and commit**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.Equipment.Stats
git add Source/GameXXK/Private/GameXXKCardRules.cpp scripts/export_game_design_tables.py scripts/export_game_equipment_design_table.py docs/design/2026-09-04-project-design-tables/GameXXK_装备设计总表_2026-09-04.xlsx Source/GameXXK/Private/Tests/GameXXKEquipmentStatRulesTest.cpp
git commit -m "docs: export approved Xuanjia and Shanhe sets"
```

### Task 14: Cold verification and acceptance record

**Files:**
- Create: `docs/production/2026-09-04-xuanjia-shanhe-runtime-acceptance.md`
- Modify: `docs/production/current-goal-acceptance.md`

- [ ] **Step 1: Save the running editor through MCP if it is open**

```powershell
python -c "import sys; sys.path.insert(0,'scripts'); from ue_tdd_pipeline import save_running_editor_before_close,kill_editor; ok=save_running_editor_before_close(); ok=ok and kill_editor(); raise SystemExit(0 if ok else 1)"
```

Expected: either no editor is running, or MCP reports `save_result=True`, no dirty packages remain, and only this project's editor closes. If MCP is unavailable while the editor may contain dirty packages, the command fails and execution stops.

- [ ] **Step 2: Run cold UBT and focused Automation**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.Equipment.XuanJia+GameXXK.Equipment.ShanHe+GameXXK.Data.Terrain.ApprovedPayloads+GameXXK.Equipment.BattleIntegration+GameXXK.Equipment.SaveMigration"
```

Expected: cold `GameXXKEditor Win64 Development` build succeeds; all focused tests pass with zero failure/error/notRun.

- [ ] **Step 3: Run the adjacent regression floor**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Data.HeroCards.Formation+GameXXK.Data.PartnerCards.Formation+GameXXK.Data.PartnerCards.Guard+GameXXK.Data.EnemyIntents+GameXXK.Equipment"
```

Expected: all relevant existing tests pass. Do not broaden to unrelated UI suites unless this run identifies a concrete shared regression.

- [ ] **Step 4: Record exact evidence**

Write commit, UBT result, test filters/counts, save migration version, approved player text, NPC equipment projection, terrain values, and any remaining design-overlay limitation in the acceptance file. Point `current-goal-acceptance.md` to it.

- [ ] **Step 5: Final diff check and commit**

```powershell
git diff --check
git add docs/production/2026-09-04-xuanjia-shanhe-runtime-acceptance.md docs/production/current-goal-acceptance.md
git commit -m "docs: accept Xuanjia and Shanhe runtime"
```
