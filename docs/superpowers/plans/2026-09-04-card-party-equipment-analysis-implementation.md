# Card, Party, and Equipment Analysis Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` after `2026-09-04-xuanjia-shanhe-runtime-implementation.md` is complete. The user explicitly prohibited subagents, so execute inline with checkpoints. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the hardcoded six-combo HTML with a deterministic, production-rule-backed Hell 3-1 analyzer that searches legal decks, party composition, equipment sets, gems, and trigger chains.

**Architecture:** C++ owns legality, card resolution, equipment descriptors, beam policy, combat state, telemetry, and the design-only Hell 3-1 phase overlay. A UE Automation exporter writes one versioned JSON report. Python only validates and renders that JSON into the review HTML; it never recomputes card damage. Candidate search is staged so 216 party directions remain auditable without evaluating the full raw Cartesian product.

**Tech Stack:** Unreal Engine 5.8 C++, UE Automation Tests, `FJsonObjectConverter`, Python 3, openpyxl, static HTML/CSS/JavaScript.

---

## File map

- Create `Source/GameXXK/Public/GameXXKBuildAnalysisTypes.h`: versioned candidate, trace, metric, and report structs.
- Create `Source/GameXXK/Public/GameXXKBuildAnalysisRules.h`: enumeration, fingerprint, staged search, and report API.
- Create `Source/GameXXK/Private/GameXXKBuildAnalysisRules.cpp`: implementation using production adapters/rules.
- Modify `Source/GameXXK/Public/GameXXKCombatSimulationTypes.h`: build-analysis policy and missing telemetry.
- Modify `Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp`: deterministic decision enumeration and depth-two beam policy.
- Create `Source/GameXXK/Private/Tests/GameXXKBuildAnalysisRulesTest.cpp`: legality, search, trigger, equipment, determinism tests.
- Create `Source/GameXXK/Private/Tests/GameXXKBuildAnalysisExportTest.cpp`: writes the final JSON report.
- Modify `scripts/export_game_enemy_design_table.py`: emit structured Hell 3-1 design JSON beside the workbook.
- Rewrite `scripts/export_game_analysis_html.py`: pure JSON-to-HTML renderer.
- Replace `docs/design/2026-09-04-project-design-tables/GameXXK_职业配队与伤害期望分析_2026-09-04.json` and `.html`.
- Modify `docs/production/2026-09-04-design-tables-party-analysis-acceptance.md` and `docs/production/current-goal-acceptance.md`.

## Execution preflight

Before the first C++ edit or cold build, save and close only this project's interactive editor:

```powershell
python -c "import sys; sys.path.insert(0,'scripts'); from ue_tdd_pipeline import save_running_editor_before_close,kill_editor; ok=save_running_editor_before_close(); ok=ok and kill_editor(); raise SystemExit(0 if ok else 1)"
```

Keep the interactive editor closed through C++ and Automation work. Reopen the visible editor only for the final pure-2D HUD/BattleBoard review.

### Task 1: Define the versioned analysis schema

**Files:**
- Create: `Source/GameXXK/Public/GameXXKBuildAnalysisTypes.h`
- Create: `Source/GameXXK/Public/GameXXKBuildAnalysisRules.h`
- Create: `Source/GameXXK/Private/GameXXKBuildAnalysisRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKBuildAnalysisRulesTest.cpp`

- [ ] **Step 1: Write a RED schema test**

Register `GameXXK.Diagnostics.BuildAnalysis.Schema` and require schema1 defaults:

```cpp
FGameXXKBuildAnalysisReport Report;
TestEqual(TEXT("report schema"), Report.SchemaVersion, 1);
TestEqual(TEXT("stage id"), Report.StageId, FName(TEXT("Hell.3-1.End")));
TestEqual(TEXT("party level"), Report.PartyLevel, 100);
TestEqual(TEXT("enemy level"), Report.EnemyLevel, 125);
```

- [ ] **Step 2: Run and verify RED**

```powershell
python scripts/ai_production_loop.py --run-ubt
```

Expected: compile FAIL because the report types do not exist. Do not use a stale editor binary as the RED result.

- [ ] **Step 3: Add focused USTRUCTs**

Define the complete report graph:

```cpp
USTRUCT()
struct FGameXXKBuildCardLoadout
{
    GENERATED_BODY()
    UPROPERTY() TArray<FName> HeroCards;
    UPROPERTY() TArray<FName> CompanionCards;
    UPROPERTY() TArray<FName> NpcCards;
};

USTRUCT()
struct FGameXXKBuildEquipmentPattern
{
    GENERATED_BODY()
    UPROPERTY() FName OwnerUnitId;
    UPROPERTY() TMap<EGameXXKEquipmentSet, int32> PiecesBySet;
    UPROPERTY() int32 AttackGems = 4;
    UPROPERTY() int32 DefenseGems = 4;
    UPROPERTY() int32 HealthGems = 4;
};

USTRUCT()
struct FGameXXKBuildTriggerEdge
{
    GENERATED_BODY()
    UPROPERTY() FName ProducerCardId;
    UPROPERTY() FName ConsumerCardId;
    UPROPERTY() FName OwnerUnitId;
    UPROPERTY() FName MechanicId;
    UPROPERTY() int32 TriggerCount = 0;
    UPROPERTY() int32 WastedCount = 0;
    UPROPERTY() double MeanDelayRounds = 0.0;
};

USTRUCT()
struct FGameXXKBuildAffixSensitivity
{
    GENERATED_BODY()
    UPROPERTY() FName OwnerUnitId;
    UPROPERTY() FName AffixId;
    UPROPERTY() int32 MinimumMagnitude = 0;
    UPROPERTY() int32 MedianMagnitude = 0;
    UPROPERTY() int32 MaximumMagnitude = 0;
    UPROPERTY() double MinimumDprDelta = 0.0;
    UPROPERTY() double MedianDprDelta = 0.0;
    UPROPERTY() double MaximumDprDelta = 0.0;
};

USTRUCT()
struct FGameXXKBuildMetricSummary
{
    GENERATED_BODY()
    UPROPERTY() double FirstTurnDamage = 0.0;
    UPROPERTY() double ThreeRoundDpr = 0.0;
    UPROPERTY() double TotalDpr = 0.0;
    UPROPERTY() double StableDpr = 0.0;
    UPROPERTY() double CycleCompletionRate = 0.0;
    UPROPERTY() double MeanRemainingPartyHealth = 0.0;
    UPROPERTY() double MeanArmorGenerated = 0.0;
    UPROPERTY() double MeanEffectiveHealing = 0.0;
    UPROPERTY() int32 SeedCount = 0;
};

USTRUCT()
struct FGameXXKBuildCandidateResult
{
    GENERATED_BODY()
    UPROPERTY() FName CandidateId;
    UPROPERTY() EGameXXKCharacterRole HeroProfession = EGameXXKCharacterRole::Invalid;
    UPROPERTY() EGameXXKCharacterRole CompanionRole = EGameXXKCharacterRole::Invalid;
    UPROPERTY() FName NpcId;
    UPROPERTY() FGameXXKBuildCardLoadout Cards;
    UPROPERTY() TArray<FGameXXKBuildEquipmentPattern> Equipment;
    UPROPERTY() FGameXXKBuildMetricSummary Metrics;
    UPROPERTY() TArray<FGameXXKBuildTriggerEdge> TriggerEdges;
    UPROPERTY() TArray<FGameXXKBuildAffixSensitivity> AffixSensitivity;
    UPROPERTY() TArray<FGameXXKSimulationTraceEntry> MedianSeedTrace;
};

USTRUCT()
struct FGameXXKBuildDirectionResult
{
    GENERATED_BODY()
    UPROPERTY() FName DirectionId;
    UPROPERTY() EGameXXKCharacterRole HeroProfession = EGameXXKCharacterRole::Invalid;
    UPROPERTY() EGameXXKCharacterRole CompanionRole = EGameXXKCharacterRole::Invalid;
    UPROPERTY() FName NpcId;
    UPROPERTY() int32 LegalCandidateCount = 0;
    UPROPERTY() TArray<FString> RejectionReasons;
    UPROPERTY() TArray<FGameXXKBuildCandidateResult> Finalists;
};

USTRUCT()
struct FGameXXKBuildRecommendation
{
    GENERATED_BODY()
    UPROPERTY() FName CategoryId;
    UPROPERTY() FName CandidateId;
    UPROPERTY() FString Rationale;
};

USTRUCT()
struct FGameXXKBuildAnalysisReport
{
    GENERATED_BODY()
    UPROPERTY() int32 SchemaVersion = 1;
    UPROPERTY() FName StageId = TEXT("Hell.3-1.End");
    UPROPERTY() int32 PartyLevel = 100;
    UPROPERTY() int32 EnemyLevel = 125;
    UPROPERTY() TArray<FGameXXKBuildDirectionResult> Directions;
    UPROPERTY() TArray<FGameXXKBuildRecommendation> Recommendations;
    UPROPERTY() TArray<FString> Limitations;
};
```

Keep report-only structs out of `GameXXKCardTypes.h` so save schemas do not grow with diagnostics.

- [ ] **Step 4: Implement report validation**

Add:

```cpp
static bool ValidateReport(const FGameXXKBuildAnalysisReport& Report, FString* OutError);
```

Validate schema, levels, exactly16 unique core cards per candidate, 12 gems per owner, nonnegative metrics, unique direction IDs, and stable trace ordinals.

- [ ] **Step 5: Run schema test and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Diagnostics.BuildAnalysis.Schema
git add Source/GameXXK/Public/GameXXKBuildAnalysisTypes.h Source/GameXXK/Public/GameXXKBuildAnalysisRules.h Source/GameXXK/Private/GameXXKBuildAnalysisRules.cpp Source/GameXXK/Private/Tests/GameXXKBuildAnalysisRulesTest.cpp
git commit -m "feat: define build analysis schema"
```

### Task 2: Export a structured Hell 3-1 design fixture

**Files:**
- Modify: `scripts/export_game_enemy_design_table.py`
- Create: `docs/design/2026-09-04-project-design-tables/GameXXK_怪物与阶段数值设计总表_2026-09-04.json`
- Test: `Source/GameXXK/Private/Tests/GameXXKBuildAnalysisRulesTest.cpp`

- [ ] **Step 1: Add Python assertions for the stage-end fixture**

Extend `main()` to assert:

```python
design = build_design_data()
end = next(x for x in design["hell31"] if x["category"] == "end")
assert [x["id"] for x in end["units"]] == [
    "Enemy.Ch3.Vulture", "Enemy.Ch3.WhiteApe", "Enemy.Ch3.GiantToad"]
assert [x["phases"] for x in end["units"]] == [1, 3, 1]
assert end["raw_phase_hp"] == 11178
```

- [ ] **Step 2: Add exact structured Hell intent rows**

Emit `hell31_stage_end.intent_decks` with these Hell cards:

```python
HELL31_INTENTS = {
  "Enemy.Ch3.Vulture": {"phases": [[
    {"id":"Gaze","target":"LowestHP","mark":5,"burn_coefficient":6},
    {"id":"Dive","target":"MarkedFirst","attack_percent":340,"trigger_burn":1},
    {"id":"WingCut","target":"AllParty","attack_percent":210,"burn_coefficient":3}
  ]]},
  "Enemy.Ch3.WhiteApe": {"phases": [
    [
      {"id":"ThrowRock","target":"PreyThenLowestHP","attack_percent":160},
      {"id":"Disturb","next_card_energy_surcharge":1},
      {"id":"BoulderCharge","target":"PreyThenLowestHP","charge_rounds":1,"attack_percent":260},
      {"id":"WideSweep","target":"AllParty","attack_percent":115}
    ],
    [
      {"id":"ChaoticRocksSealMeridians","target":"PreyThenLowestHP","attack_percent":160,"remove_positive":1,"next_card_energy_surcharge":1},
      {"id":"ApeHowlDisruptsFormation","target":"AllParty","attack_percent":100,"weak":3},
      {"id":"FlyingRocksGuardGroup","target":"AllEnemies","armor_defense_percent":120,"self_agility":1},
      {"id":"GiantRockCharge","target":"PreyThenLowestHP","charge_rounds":1,"attack_percent":270,"trigger_bleed":1}
    ],
    [
      {"id":"ApeKingSealsMeridians","target":"PreyThenLowestHP","attack_percent":200,"remove_positive":2,"next_card_energy_surcharge":1,"trigger_bleed":1},
      {"id":"ChaoticRocksFallHeaven","target":"AllParty","attack_percent":125,"weak":3,"next_card_energy_surcharge":1},
      {"id":"TenThousandStonesGuard","target":"AllEnemies","armor_defense_percent":180,"all_agility":1},
      {"id":"MountainCrushingBoulder","target":"PreyThenLowestHP","charge_rounds":1,"attack_percent":310,"trigger_bleed":1}
    ]
  ]},
  "Enemy.Ch3.GiantToad": {"phases": [[
    {"id":"Tongue","target":"LowestHP","attack_percent":300,"poisoned_attack_percent":400,"heal_max_hp_percent":6},
    {"id":"PoisonFog","target":"AllParty","attack_percent":190,"poison_coefficient":6},
    {"id":"Inflate","target":"Self","armor_defense_percent":360,"tongue_heal_bonus_percent":6}
  ]]}
}
```

Add:

```python
def build_hell31_stage_end_fixture(design):
    end = next(x for x in design["hell31"] if x["category"] == "end")
    return {
        "enemy_level": 125,
        "raw_phase_hp": end["raw_phase_hp"],
        "units": end["units"],
        "openings": {
            "Enemy.Ch3.Vulture": "Gaze",
            "Enemy.Ch3.WhiteApe": "ThrowRock",
            "Enemy.Ch3.GiantToad": "PoisonFog",
        },
        "white_ape_first_negative_armor_percent_by_phase": [120, 100, 160],
        "intent_decks": HELL31_INTENTS,
    }
```

Set openings to `Gaze`, `ThrowRock`, `PoisonFog`. Add White Ape passive Armor percentages120/100/160 by phase.

- [ ] **Step 3: Write JSON and verify determinism**

```python
json_path = OUT / "GameXXK_怪物与阶段数值设计总表_2026-09-04.json"
design["hell31_stage_end"] = build_hell31_stage_end_fixture(design)
json_path.write_text(json.dumps(design, ensure_ascii=False, indent=2, sort_keys=True)+"\n", encoding="utf-8")
```

Run the exporter twice and assert identical SHA256.

- [ ] **Step 4: Add a C++ fixture-loader test**

Define loader-only structs in `GameXXKBuildAnalysisTypes.h`:

```cpp
USTRUCT()
struct FGameXXKDesignIntent
{
    GENERATED_BODY()
    UPROPERTY() FName Id;
    UPROPERTY() FName TargetRule;
    UPROPERTY() int32 AttackPercent = 0;
    UPROPERTY() int32 PoisonedAttackPercent = 0;
    UPROPERTY() int32 BurnCoefficient = 0;
    UPROPERTY() int32 PoisonCoefficient = 0;
    UPROPERTY() int32 Mark = 0;
    UPROPERTY() int32 Weak = 0;
    UPROPERTY() int32 ArmorDefensePercent = 0;
    UPROPERTY() int32 ChargeRounds = 0;
    UPROPERTY() int32 RemovePositive = 0;
    UPROPERTY() int32 NextCardEnergySurcharge = 0;
    UPROPERTY() int32 HealMaxHpPercent = 0;
    UPROPERTY() int32 TongueHealBonusPercent = 0;
    UPROPERTY() int32 SelfAgility = 0;
    UPROPERTY() int32 AllAgility = 0;
    UPROPERTY() int32 TriggerBleed = 0;
    UPROPERTY() int32 TriggerBurn = 0;
};

USTRUCT()
struct FGameXXKDesignIntentDeck
{
    GENERATED_BODY()
    UPROPERTY() TArray<FGameXXKDesignIntent> Intents;
};

USTRUCT()
struct FGameXXKDesignEnemyFixture
{
    GENERATED_BODY()
    UPROPERTY() FName EnemyId;
    UPROPERTY() int32 TotalPhases = 1;
    UPROPERTY() FName OpeningIntentId;
    UPROPERTY() TArray<int32> FirstNegativeArmorPercentByPhase;
    UPROPERTY() TArray<FGameXXKDesignIntentDeck> PhaseDecks;
};

USTRUCT()
struct FGameXXKHell31Fixture
{
    GENERATED_BODY()
    UPROPERTY() int32 EnemyLevel = 125;
    UPROPERTY() int32 RawPhaseHp = 11178;
    UPROPERTY() TArray<FGameXXKDesignEnemyFixture> Enemies;
};
```

Load with `FJsonSerializer`, assert level125, IDs, phases, openings,11178 total phase HP, and all intent arrays nonempty. Reject unknown effect keys rather than ignoring them.

- [ ] **Step 5: Run and commit**

```powershell
python scripts/export_game_enemy_design_table.py
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.Diagnostics.BuildAnalysis.Hell31Fixture
git add scripts/export_game_enemy_design_table.py docs/design/2026-09-04-project-design-tables/GameXXK_怪物与阶段数值设计总表_2026-09-04.json Source/GameXXK/Private/Tests/GameXXKBuildAnalysisRulesTest.cpp
git commit -m "docs: export structured Hell 3-1 fixture"
```

### Task 3: Enumerate legal 216 party directions and 16-card decks

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKBuildAnalysisTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKBuildAnalysisRules.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKBuildAnalysisRulesTest.cpp`

- [ ] **Step 1: Add RED cardinality tests**

Require six Hero profession packages, six partner roles, six NPCs, and216 direction IDs. At level100 require18 unlocked cards and8568 legal five-card combinations for every partner role. For every concrete loadout require8/5/3 unique cards and production validator acceptance.

- [ ] **Step 2: Run and verify RED**

Expected: enumerator absent.

- [ ] **Step 3: Enumerate Hero packages deterministically**

For each Hero profession direction, include its four profession cards and choose four of the twelve Hero generic cards:

```cpp
static void ChooseExactlyFour(
    const TArray<FName>& SortedCards,
    int32 StartIndex,
    TArray<FName>& Working,
    TArray<TArray<FName>>& OutChoices);
static TArray<FName> GetFourHeroProfessionCards(EGameXXKCharacterRole Role);
static TArray<FName> GetHeroGenericCards();
struct FHeroPackage
{
    EGameXXKCharacterRole Role = EGameXXKCharacterRole::Invalid;
    TArray<FName> Cards;
};
TArray<FHeroPackage> OutHeroPackages;

for (EGameXXKCharacterRole Role : SixProfessionRoles)
{
    const TArray<FName> Anchors = GetFourHeroProfessionCards(Role);
    TArray<TArray<FName>> GenericChoices;
    TArray<FName> Working;
    ChooseExactlyFour(GetHeroGenericCards(), 0, Working, GenericChoices);
    for (const TArray<FName>& GenericFour : GenericChoices)
    {
        TArray<FName> Package = Anchors;
        Package.Append(GenericFour);
        Package.Sort([](const FName Left, const FName Right)
        {
            return Left.LexicalLess(Right);
        });
        OutHeroPackages.Add({Role, MoveTemp(Package)});
    }
}
```

This yields `6 * C(12,4) = 2970` pre-pruning Hero packages and preserves the approved “six profession packages” meaning.

- [ ] **Step 4: Enumerate level-100 partner and NPC loadouts**

For each partner role, construct a level-100 companion, call `BuildFullProfessionCardPool` and `RefreshUnlockedPersonalCards`, assert18 unique unlocked cards, sort them by CardId, and enumerate all `C(18,5)=8568` five-card selections. Validate every emitted selection through `ValidateSelectedPersonalCards`. Build the pool with two different positive card seeds and assert the sorted18-card set is identical; only order/early unlock prefix may differ at level100. Generate all four exact `4 choose 3` NPC loadouts and pass each through `ValidateQuestNpcCardSelection`.

- [ ] **Step 5: Validate through production setup**

For each emitted candidate, call `SetHeroSelectedCards`, `SetSelectedPersonalCards`, `SetQuestNpcForCurrentRun`, then materialize starting instances through the adapter test hook. Assert exactly16 instances and owner counts8/5/3.

Keep `BossCardSlots` and route relics empty in every primary scenario. Assert the materialized deck remains16 so current single-map Training cannot silently inherit future multi-map rewards.

Reject `Profession.Sorcerer.RanLingHuanYuan` and `Npc.JinGui.HouXiangTuoShen` before ranking, record `PendingCardSemantic` as the rejection reason, and keep them visible only in the report limitation list.

- [ ] **Step 6: Run and commit**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.Diagnostics.BuildAnalysis.Legality
git add Source/GameXXK/Public/GameXXKBuildAnalysisTypes.h Source/GameXXK/Private/GameXXKBuildAnalysisRules.cpp Source/GameXXK/Private/Tests/GameXXKBuildAnalysisRulesTest.cpp
git commit -m "feat: enumerate legal build directions"
```

### Task 4: Build owner-scoped mechanic fingerprints and deck pruning

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKBuildAnalysisTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKBuildAnalysisRules.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKBuildAnalysisRulesTest.cpp`

- [ ] **Step 1: Add RED known-chain tests**

Assert the scanner recognizes:

```cpp
// Qiong Meier GuWu: Bleed + Poison producer.
// Healer LianQiao: DOT consumer/explosion.
// Guard Armor cards: Armor producer and Block registration.
// Hunter cards: Charge producer and Heavy Arrow consumer.
// Sorcerer cards: owner-scoped task pieces.
// Formation/Yue Bai cards: terrain card and terrain trigger.
// Blade cards: Charge/Finish rather than Hunter Charge.
```

- [ ] **Step 2: Implement typed features**

Define booleans/counts per owner, never a shared string-only tag:

```cpp
USTRUCT()
struct FGameXXKBuildMechanicFingerprint
{
    GENERATED_BODY()
    UPROPERTY() int32 ArmorProducerCount = 0;
    UPROPERTY() int32 ArmorConsumerCount = 0;
    UPROPERTY() int32 BlockSourceCount = 0;
    UPROPERTY() int32 BladeChargeProducerCount = 0;
    UPROPERTY() int32 BladeFinishCount = 0;
    UPROPERTY() int32 HunterChargeProducerCount = 0;
    UPROPERTY() int32 HeavyArrowConsumerCount = 0;
    UPROPERTY() int32 DotTypeMask = 0;
    UPROPERTY() int32 ToxicExplosionCount = 0;
    UPROPERTY() int32 TerrainCardCount = 0;
    UPROPERTY() int32 HighCostCardCount = 0;
    UPROPERTY() int32 DrawCount = 0;
    UPROPERTY() int32 TaskPieceCount = 0;
};
```

Read structured definitions (`Effects`, `BladeSequence`, `HeavyArrow`, `HealerRule`, spell-task metadata), never compact text.

- [ ] **Step 3: Prune Hero and partner selections before party combination**

Rank the495 Hero packages per profession and8568 partner packages per role by:

1. count of consumer mechanics with at least one party producer;
2. count of task/formula requirements fully present;
3. reachable printed Energy/Mana budget;
4. summed printed direct/DOT coefficients;
5. sorted CardId tuple.

For each profession/role, take the best four packages in each of five categories—direct, DOT, Armor/reaction, resource, and profession mechanic—then add the best four overall. Deduplicate by sorted CardId tuple and fill from the global order until exactly24 remain or the legal pool is exhausted. For each of216 party directions, combine those retained Hero/partner packages with four NPC loadouts, apply the same producer-consumer tuple across the full party, and keep at most eight complete16-card candidates. Store every rejection reason.

- [ ] **Step 4: Run fingerprint/pruning tests and commit**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Diagnostics.BuildAnalysis.Fingerprint+GameXXK.Diagnostics.BuildAnalysis.Pruning"
git add Source/GameXXK/Public/GameXXKBuildAnalysisTypes.h Source/GameXXK/Private/GameXXKBuildAnalysisRules.cpp Source/GameXXK/Private/Tests/GameXXKBuildAnalysisRulesTest.cpp
git commit -m "feat: prune decks by typed trigger chains"
```

### Task 5: Enumerate set structures and gem allocations by owner

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKBuildAnalysisTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKBuildAnalysisRules.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKBuildAnalysisRulesTest.cpp`

- [ ] **Step 1: Add RED combinatoric tests**

Assert56 set structures and91 gem allocations:

```cpp
TestEqual(TEXT("six pure plus thirty 4+2 plus twenty 2+2+2"), Patterns.Num(), 56);
TestEqual(TEXT("nonnegative triples summing to twelve"), GemAllocations.Num(), 91);
```

- [ ] **Step 2: Generate exact structures**

```cpp
// 6A: six choices.
// 4A+2B: 6*5 choices.
// 2A+2B+2C: choose(6,3) choices.
```

Sort by `(piece counts PoJun..ShanHe)` and deduplicate. Generate gems with three nested nonnegative loops whose sum is12.

- [ ] **Step 3: Apply owner-scoped eligibility**

Use the fingerprint:

```cpp
PoJun   => BladeChargeProducerCount > 0 && BladeFinishCount > 0;
XuanJia => ArmorProducerCount > 0 && (BlockSourceCount > 0 || ArmorConsumerCount > 0);
QingNang=> HighCostCardCount > 0;
ZhuiFeng=> affordable active-card chain can reach at least four plays;
ShiGu   => (DotTypeMask & (DotTypeMask - 1)) != 0 || ToxicExplosionCount > 0;
ShanHe  => TerrainCardCount > 0;
```

Do this independently for Hero, partner, and NPC; a Qiong Meier DOT card only qualifies Qiong Meier's ShiGu set.

- [ ] **Step 4: Apply approved stat overlays without double-counting gems**

Start from the frozen balanced attributes, subtract `+80 Attack/+80 Defense/+400 HP`, then add `20*AttackGems`, `20*DefenseGems`, `100*HealthGems`. NPC rows use their level-100 naked curve plus their actual selected equipment snapshot and remain labeled as an unfrozen projection.

- [ ] **Step 5: Build single-variable affix sensitivity cases**

For every affix whose mechanic is reachable in a finalist fingerprint, query `FGameXXKAffixCatalog::GetMagnitudeRange` at Treasure tier. Evaluate minimum, integer midpoint `(Minimum + Maximum) / 2`, and maximum with all other affixes held neutral on the same seeds. Store the three marginal DPR values in `FGameXXKBuildAffixSensitivity`; never combine all maximum rolls into the primary candidate.

- [ ] **Step 6: Run and commit**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.Diagnostics.BuildAnalysis.EquipmentCandidates
git add Source/GameXXK/Public/GameXXKBuildAnalysisTypes.h Source/GameXXK/Private/GameXXKBuildAnalysisRules.cpp Source/GameXXK/Private/Tests/GameXXKBuildAnalysisRulesTest.cpp
git commit -m "feat: enumerate build equipment candidates"
```

### Task 6: Add a deterministic depth-two beam policy

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCombatSimulationTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKCombatSimulationRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKBuildAnalysisRulesTest.cpp`

- [ ] **Step 1: Add RED setup/payoff puzzles**

Create fixtures where greedy immediate damage chooses incorrectly but two-ply chooses Armor→consume, DOT→explosion, Charge→Heavy Arrow, terrain setup→payoff, and task search→completion.

- [ ] **Step 2: Append a policy enum and legal decision enumerator**

```cpp
enum class EGameXXKSimulationPolicy : uint8
{
    Invalid = 0,
    Skilled = 1,
    BuildAnalysisBeam = 2
};

static bool EnumerateLegalDecisions(
    const FGameXXKRuntimeState& State,
    TArray<FGameXXKSimulationDecision>& OutDecisions,
    FString* OutError);
```

Enumerate every playable CardInstanceId/target pair plus EndPlayerPhase in stable order.

- [ ] **Step 3: Implement lexicographic leaf evaluation**

Represent the approved order explicitly:

```cpp
struct FBeamValue
{
    int32 TerminalRank = 0;       // victory > live > defeat/invalid
    int32 LivingParty = 0;
    int64 EffectiveEnemyHpLoss = 0;
    int64 PartyHpAndArmor = 0;
    int32 CompletedTaskPieces = 0;
    int32 ConsumableState = 0;
    int32 FutureEnergyMana = 0;
    FString StableTieBreak;
};
```

Compare fields in this order; do not collapse them into weighted points.

- [ ] **Step 4: Search depth two with beam width twelve**

Apply each candidate to a copied state through `FGameXXKCardBattleAdapter`. Keep the best12 first-ply states, enumerate one more decision, and return the first decision of the best leaf. For pending discard/search choices, try every legal choice and use the same leaf comparator rather than always taking index0.

- [ ] **Step 5: Run old policy and new puzzle tests**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Diagnostics.BuildAnalysis.BeamPolicy+GameXXK.Data.CombatSimulation"
```

Expected: new puzzles pass; legacy `Skilled` outputs remain unchanged.

- [ ] **Step 6: Commit**

```powershell
git add Source/GameXXK/Public/GameXXKCombatSimulationTypes.h Source/GameXXK/Public/GameXXKCombatSimulationRules.h Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp Source/GameXXK/Private/Tests/GameXXKBuildAnalysisRulesTest.cpp
git commit -m "feat: add build analysis beam policy"
```

### Task 7: Execute the design-only Hell 3-1 phase overlay

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKBuildAnalysisTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKBuildAnalysisRules.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKBuildAnalysisRulesTest.cpp`

- [ ] **Step 1: Add RED phase and opening tests**

Require openings `Gaze/ThrowRock/PoisonFog`, White Ape phase sequence1→2→3, lethal clamping, full heal at transition, negative clear, positive/Armor retention, and no packet overflow.

- [ ] **Step 2: Build one analysis-owned encounter state**

```cpp
struct FDesignEnemyPhaseState
{
    FName UnitId;
    int32 CurrentPhase = 1;
    int32 TotalPhases = 1;
    int32 IntentCursor = 0;
    bool bCharging = false;
    FName ChargedIntentId;
    int32 ChargeRoundsRemaining = 0;
};
```

Initialize Vulture/WhiteApe/GiantToad from the structured JSON and inject their level125 stats into the production card runtime.

- [ ] **Step 3: Wrap production player actions, not card effects**

After each production-resolved damage queue, inspect defeated analysis enemies. If phases remain, replace terminal state with the next phase transaction: clamp the lethal packet, restore that enemy to full phase HP, clear all negative statuses, preserve positives/Armor, cancel charged intent, increment phase, and reset cursor to0. Never reimplement player-card effects.

Implement the boundary behind:

```cpp
static bool AdvanceDesignPhasesAfterResolvedQueue(
    FGameXXKRuntimeState& InOutState,
    TMap<FName, FDesignEnemyPhaseState>& InOutPhases,
    const FGameXXKHell31Fixture& Fixture,
    FString& OutError);
```

- [ ] **Step 4: Resolve exact Hell enemy intents**

Translate the structured effects into existing public enemy/card primitives: direct attack, group attack, status/DOT coefficient, Armor, heal, surcharge, positive removal, charge, and DOT trigger. Apply White Ape passive120/100/160 by phase and Giant Toad's one-use6% Tongue heal amplification.

Use one exhaustive dispatcher that rejects unknown fields:

```cpp
static bool ResolveDesignEnemyIntent(
    FGameXXKRuntimeState& InOutState,
    FDesignEnemyPhaseState& InOutEnemy,
    const FGameXXKDesignIntent& Intent,
    TArray<FGameXXKCardDamageResult>& OutDamage,
    FString& OutError);
```

- [ ] **Step 5: Run phase, passive, charge, and determinism tests**

Run the Hell31 analysis filter twice. Expected: identical state/trace hashes and total raw phase HP11178.

- [ ] **Step 6: Commit**

```powershell
git add Source/GameXXK/Public/GameXXKBuildAnalysisTypes.h Source/GameXXK/Private/GameXXKBuildAnalysisRules.cpp Source/GameXXK/Private/Tests/GameXXKBuildAnalysisRulesTest.cpp
git commit -m "feat: simulate approved Hell 3-1 phases"
```

### Task 8: Capture card, trigger, equipment, and cycle telemetry

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCombatSimulationTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKBuildAnalysisTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKBuildAnalysisRules.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKBuildAnalysisRulesTest.cpp`

- [ ] **Step 1: Add RED attribution tests**

Use one controlled build and require direct, task replay, Heavy Arrow, DOT, terrain, Block, Xuanjia, and set-trigger counts to sum to total effective damage without counting overkill.

- [ ] **Step 2: Extend raw metrics**

Add maps/fields:

```cpp
TMap<FName, int64> DamageByCause;
TMap<FName, int64> DamageByEquipmentEffectId;
TMap<FName, int64> EquipmentTriggerCountById;
TMap<FName, int64> TerrainTriggerCountBySource;
TMap<FName, int64> TaskCompletionCountByOwner;
TMap<FName, int64> FormulaCompletionCountByOwner;
int64 ArmorAbsorbed = 0;
int64 ArmorClearedUnused = 0;
int64 EffectiveHealing = 0;
int64 DotCapWaste = 0;
```

Record from resolved results and before/after state; never infer damage by parsing log text.

- [ ] **Step 3: Build producer-consumer edges**

Attach a stable mechanic event ID when status/resource is produced. When a later card consumes or triggers it, close the edge and record delay. Unconsumed events at battle end increment `WastedCount`.

- [ ] **Step 4: Calculate time windows**

For each seed calculate first-turn damage, rounds1-3 damage/round, total effective damage/actual player rounds, and rounds4-10 stable DPR. Early victory stops both numerator and denominator.

- [ ] **Step 5: Run attribution tests and commit**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.Diagnostics.BuildAnalysis.Telemetry
git add Source/GameXXK/Public/GameXXKCombatSimulationTypes.h Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp Source/GameXXK/Public/GameXXKBuildAnalysisTypes.h Source/GameXXK/Private/GameXXKBuildAnalysisRules.cpp Source/GameXXK/Private/Tests/GameXXKBuildAnalysisRulesTest.cpp
git commit -m "feat: trace build trigger telemetry"
```

### Task 9: Run staged deck/equipment search and export JSON

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKBuildAnalysisRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKBuildAnalysisExportTest.cpp`
- Create: `docs/design/2026-09-04-project-design-tables/GameXXK_职业配队与伤害期望分析_2026-09-04.json`

- [ ] **Step 1: Add RED staged-count and determinism tests**

Require20 seeds in screening,100 in verification,1000 for finalists, no more than eight deck candidates per direction, and identical report hash on repeated fixed input.

- [ ] **Step 2: Implement three stages**

```cpp
// Stage A: fingerprint legality; retain <=8 decks per direction.
// Stage B: 20 seeds over three rounds; retain mechanic representatives per Hero profession.
// Stage C: 100 seeds over ten rounds; finalists rerun at1000 seeds.
```

For equipment, evaluate each owner's eligible patterns and91 gem splits against the same neutral baseline/seed, keep the union of top damage/survival/stability marginal candidates, then evaluate their team combinations.

Treat each NPC `4 choose 3` loadout as a player-selectable candidate. Use the best legal three-card loadout in the primary recommendation and emit the other three omissions as separate sensitivity rows; never average the four loadouts into one DPR.

For partners, primary rows name the exact selected five from the level-100 full18-card pool. Emit the next three legal five-card alternatives and their same-seed marginal changes; do not show an early-level “free-card dependency” in the level-100 report.

- [ ] **Step 3: Select recommendations without an opaque total score**

Emit category winners for active damage, DOT, reaction, stability, survival, and Pareto-balanced. Keep at least one executable recommendation for every Hero profession. A balanced recommendation must not be dominated simultaneously on effective damage, survival, and cycle completion.

- [ ] **Step 4: Write canonical JSON atomically**

The Automation test serializes with `FJsonObjectConverter`, writes a temporary file, validates it, then moves it to:

```text
docs/design/2026-09-04-project-design-tables/GameXXK_职业配队与伤害期望分析_2026-09-04.json
```

Include source Git commit, spec hashes, enemy JSON hash, seed schedule, runtime implementation flags, and unresolved-card exclusions.

- [ ] **Step 5: Run export twice and compare hashes**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.Diagnostics.BuildAnalysis.ExportHell31
Get-FileHash 'docs/design/2026-09-04-project-design-tables/GameXXK_职业配队与伤害期望分析_2026-09-04.json' -Algorithm SHA256
```

Expected: both runs produce the same SHA256.

- [ ] **Step 6: Commit**

```powershell
git add Source/GameXXK/Private/GameXXKBuildAnalysisRules.cpp Source/GameXXK/Private/Tests/GameXXKBuildAnalysisExportTest.cpp docs/design/2026-09-04-project-design-tables/GameXXK_职业配队与伤害期望分析_2026-09-04.json
git commit -m "feat: export Hell 3-1 build analysis"
```

### Task 10: Rewrite the HTML exporter as a pure renderer

**Files:**
- Rewrite: `scripts/export_game_analysis_html.py`
- Replace: `docs/design/2026-09-04-project-design-tables/GameXXK_职业配队与伤害期望分析_2026-09-04.html`

- [ ] **Step 1: Add renderer schema assertions**

At startup require schema1,216 direction rows, nonempty six recommendation categories, exact8/5/3 decks, 12 gems per owner, and traceable source IDs. Delete `BUILDS`, `simulate_rounds`, `variant_sequence`, and `cycle_damage`.

Resolve permanent-partner display labels through `FGameXXKCompanionRules::GetCompanionDisplayName`; JSON and HTML must contain the six canonical profession labels and no seed-generated personal names.

- [ ] **Step 2: Render the approved information hierarchy**

Generate sections for authority/limitations, category winners, six Hero profession entries, 216-direction filters, exact decks, three-owner equipment, trigger graph, median seed round ledger, damage/defense/resource charts, and sensitivity rows.

Use escaped JSON values only:

```python
def esc(value):
    return html.escape(str(value), quote=True)

data_json = json.dumps(report, ensure_ascii=False).replace("</", "<\\/")
```

- [ ] **Step 3: Make equipment causality visible**

For every equipped pattern show neutral-vs-equipped deltas for effective damage, Armor/healing, resource, trigger rate, and completion. Label NPC level-100 attributes as a projection; never label the design overlay as PIE-measured win rate.

- [ ] **Step 4: Generate HTML and validate embedded JavaScript**

```powershell
python scripts/export_game_analysis_html.py
python -m py_compile scripts/export_game_analysis_html.py
```

Extract the inline script to `Saved/Temp/build-analysis-inline.js` and run:

```powershell
node --check Saved/Temp/build-analysis-inline.js
```

Expected: no Python or JavaScript errors.

- [ ] **Step 5: Commit**

```powershell
git add scripts/export_game_analysis_html.py docs/design/2026-09-04-project-design-tables/GameXXK_职业配队与伤害期望分析_2026-09-04.html
git commit -m "docs: render trigger-aware build analysis"
```

### Task 11: Verify artifacts and record acceptance

**Files:**
- Modify: `docs/production/2026-09-04-design-tables-party-analysis-acceptance.md`
- Modify: `docs/production/current-goal-acceptance.md`

- [ ] **Step 1: Run cold build and focused analysis tests**

Save dirty packages through MCP before any editor close, then run:

```powershell
python -c "import sys; sys.path.insert(0,'scripts'); from ue_tdd_pipeline import save_running_editor_before_close,kill_editor; ok=save_running_editor_before_close(); ok=ok and kill_editor(); raise SystemExit(0 if ok else 1)"
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.Diagnostics.BuildAnalysis+GameXXK.Equipment.XuanJia+GameXXK.Equipment.ShanHe"
```

Expected: cold UBT succeeds; all focused tests pass.

- [ ] **Step 2: Validate every design artifact**

```powershell
python scripts/export_game_enemy_design_table.py
python scripts/export_game_equipment_design_table.py
python scripts/export_game_analysis_html.py
python -m py_compile scripts/export_game_enemy_design_table.py scripts/export_game_equipment_design_table.py scripts/export_game_analysis_html.py
git diff --check
```

Expected: workbook reload assertions, report schema assertions, Python compilation, and diff check all pass.

- [ ] **Step 3: Browser-review the generated page**

Open the local HTML in the in-app browser. Verify no console errors; exactly216 direction entries; six recommendation categories; exact Hero8/partner5/NPC3 sections; equipment for all three owners; trigger chains; per-round ledger; and projection labels.

- [ ] **Step 4: Record evidence and limitations**

Update the acceptance document with commits, test counts, JSON/HTML SHA256, seed counts, runtime/spec hashes, top recommendations, and explicit design-overlay limitations. Remove the old text describing a five-card burst or manual six-build `cycle_damage` model.

- [ ] **Step 5: Commit and push only owned files**

```powershell
git add docs/production/2026-09-04-design-tables-party-analysis-acceptance.md docs/production/current-goal-acceptance.md
git commit -m "docs: accept trigger-aware party analysis"
git push
```

Do not stage unrelated map, font, scrollbar, generated art, temporary probes, or `docs/design/2026-09-04-project-design-tables.zip` changes.
