# Balance Telemetry and Certification Implementation Plan

> **Execution:** Use superpowers:executing-plans in the current task. The user prohibits sub-agents. Steps use checkbox (`- [ ]`) syntax for tracking; current design review still pauses runtime implementation.

**Goal:** Certify all approved cards, enemy intents, formations, and representative three-person builds with a deterministic strategy that understands setup, defense, tasks, formulas, terrain, and delayed payoffs.

**Architecture:** A focused evaluator scores legal adapter mutations on copied runtime state with depth-two bounded beam search and stable tie-breaking. Simulation telemetry records opportunities and effective output per CardId. Small tactical puzzles gate policy competence before tuning and certification matrices run; reporting tools remain read-only.

**Tech Stack:** Unreal Engine 5.8 C++ simulation/Automation, existing adapter as sole mutation authority, Python JSON/CSV report tools, deterministic hashes.

---

## Preconditions and files

Complete Plans 1-6; current schema is v38. Work on `codex/overall-in-run-optimization`. Relic redesign is not approved, so the binding certification uses no route relics and labels that exclusion explicitly. Any later fixed-relic sensitivity run is provisional and outside this plan's completion gate.

Use the revised partner Ice semantics from design sections 4.3.1 and 6.3, not the superseded 40%/80%-Defense base effects. Carry Plan 2's deterministic 照见/斗转 Ice fixtures through adapter parity: initial Mana 34/34, zero Armor, Attack 495, minimum legal qualities; expected pre-reward Armor 456/816 and 100%-base standard Ice damage 1694/2981 against level-135 Defense 146 before phase protection. Record actual phase-limited HP damage separately from potential damage and overkill. Reset ordinary Armor correctly at a new player phase, account for every once-per-battle automatic-hand budget, and preserve live Mana changes/+4 capacity in replays. These conditional combo figures are not substitutes for sampled completion rates, full battles, or the approved win-rate target. Resolve the review's broader fixed-base-Mana/equipment-growth inconsistency before certifying a live loadout; never silently benchmark the old level-grown Mana pool.

The subsequent 霜镜 reward amendment grants `floor(consumed Armor / 4)` to each living ally including the caster, independent of Defense/quality/level. Add the matching starter control to adapter parity: 霜镜→寒息→玄冰→冰鉴→照见 produces 351 pre-reward Armor, potential Ice damage 1421, and 87 Armor per living ally at the same baseline with unavailable searches. Attribute each recipient's generated/absorbed/refunded Armor to 霜镜, and do not execute that reward in the existing 照见/斗转 starter cases. Conditional party protection and overkill must remain distinct metrics.

The confirmed 六合 revision uses caster-only current-Mana recovery 8/16 and one shared overflow Armor grant per living ally, then only a consumed-Armor quarter for its Ice reward. With confirmed 10% Ice recovery, the ordinary 六合 starter benchmark is 774 consumed Armor / 2782 potential damage / 193 new Armor per ally, with final party Armor 193/265/265. 照见→寒息→六合→冰鉴→霜镜 uses 16 and gives 912 / 3161 with final Armor 228/168/168; the corresponding 斗转 sequence gives 1728 / 5915 and final Armor 0/168/168. Attribute caster Mana separately from group Armor and distinguish new reward Armor from retained base grants. Use 10% for current certification fixtures; the earlier 20% and 25% comparisons are not alternate acceptance targets.

Before freezing Lightning numeric fixtures, reconcile `docs/superpowers/specs/2026-09-03-lightning-single-hit-design.md`: the approved 雷走 identity is one direct hit per enemy with ordinary at-most-one-Mark use, leaving remaining Marks for 连霆. Its 120/180/220 coefficients, positions 3-4 boost and one-240 starter reward are numeric candidates. The local `Saved/HarnessReports/2026-09-03-lightning-single-hit-projection.md` has 44 old/new same-order comparisons and separate original-order controls, including first-play quality, phase clearing and all four possible last-Lightning cards for 斗转. Candidate level-100/Attack-495 tiger potentials include 定标3520, 雷走3610 and 斗转3814/4417/4009/4347 depending on the recorded order; these are not binding win-rate or full-battle targets. Freeze coefficients through the ongoing design review, then regenerate adapter fixtures from those inputs. Telemetry must distinguish one 雷走 damage packet and its one normal Mark use from 连霆's per-Mark packets; do not credit remaining Marks as damage or reuse old empty-volley results. Add a policy case with four Marks and fourth-position 雷走 followed by fifth-position 连霆, plus a phase-transition variant that clears Marks before the second card. Keep 引雷's existing reward unless its independent proposal receives approval.

Create:

- `Source/GameXXK/Public/GameXXKCombatPolicyEvaluator.h`
- `Source/GameXXK/Private/GameXXKCombatPolicyEvaluator.cpp`
- `Source/GameXXK/Private/Tests/GameXXKCombatPolicyPuzzleTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKApprovedBalanceMatrixTest.cpp`
- `scripts/run_approved_balance_matrix.py`
- `scripts/test_approved_balance_matrix.py`
- `docs/production/2026-09-03-in-run-balance-certification.md`

Modify:

- `Source/GameXXK/Public/GameXXKCombatSimulationTypes.h`
- `Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp`
- `Source/GameXXK/Public/GameXXKRouteBalanceTypes.h`
- `Source/GameXXK/Public/GameXXKRouteBalanceRules.h`
- `Source/GameXXK/Private/GameXXKRouteBalanceRules.cpp`
- `Source/GameXXK/Private/Tests/GameXXKCombatSimulationPolicyTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKCombatSimulationFoundationTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKRouteBalanceSimulationTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKCardBalanceObservationTest.cpp`
- `scripts/propose_card_balance_tuning.py`
- `scripts/test_propose_card_balance_tuning.py`
- `scripts/README.md`

---

### Task 1: Add opportunity-aware per-card telemetry

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCombatSimulationTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCombatSimulationFoundationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBalanceObservationTest.cpp`

- [ ] **Step 1: Write red telemetry tests**

Build one hand containing a legal card, an unaffordable card, and a card with no target. End the phase and assert separate seen/legal/legal-but-rejected/played/resource-rejected/target-rejected/hand-held counters. In focused transactions, assert direct/DOT/fixed damage, overkill, healing/overheal, Armor generated/absorbed/wasted, status add/natural-change/clear, generated/unused Energy and Mana, task/formula/terrain events, and phase packets are attributed to the originating CardId.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Simulation.Foundation --automation-report InRun07_Task01_RED --json
```

- [ ] **Step 3: Add the serializable card telemetry struct**

```cpp
USTRUCT()
struct FGameXXKSimulationCardMetrics
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) int64 Seen = 0;
    UPROPERTY(SaveGame) int64 LegalOpportunities = 0;
    UPROPERTY(SaveGame) int64 LegalButRejected = 0;
    UPROPERTY(SaveGame) int64 Played = 0;
    UPROPERTY(SaveGame) int64 ResourceRejected = 0;
    UPROPERTY(SaveGame) int64 TargetRejected = 0;
    UPROPERTY(SaveGame) int64 HandHeldRounds = 0;
    UPROPERTY(SaveGame) int64 DirectDamage = 0;
    UPROPERTY(SaveGame) int64 DotDamage = 0;
    UPROPERTY(SaveGame) int64 FixedDamage = 0;
    UPROPERTY(SaveGame) int64 Healing = 0;
    UPROPERTY(SaveGame) int64 ArmorGenerated = 0;
    UPROPERTY(SaveGame) int64 ArmorAbsorbed = 0;
    UPROPERTY(SaveGame) int64 Overkill = 0;
    UPROPERTY(SaveGame) int64 Overheal = 0;
    UPROPERTY(SaveGame) int64 ArmorWasted = 0;
    UPROPERTY(SaveGame) int64 StatusesAdded = 0;
    UPROPERTY(SaveGame) int64 StatusesNaturallyChanged = 0;
    UPROPERTY(SaveGame) int64 StatusesCleared = 0;
    UPROPERTY(SaveGame) int64 EnergyGenerated = 0;
    UPROPERTY(SaveGame) int64 ManaGenerated = 0;
    UPROPERTY(SaveGame) int64 GeneratedEnergyLeftUnused = 0;
    UPROPERTY(SaveGame) int64 GeneratedManaLeftUnused = 0;
    UPROPERTY(SaveGame) int64 TaskCompletions = 0;
    UPROPERTY(SaveGame) int64 FormulaTriggers = 0;
    UPROPERTY(SaveGame) int64 TerrainTriggers = 0;
    UPROPERTY(SaveGame) int64 PhasePackets = 0;
};
```

Add `TMap<FName,FGameXXKSimulationCardMetrics> CardMetrics` to aggregate metrics. Derive counters only from adapter previews/results and before/after snapshots; do not reproduce combat formulas in simulation. Keep a simulation-only provenance ledger for generated Armor, status portions, Energy, and Mana. Spending/absorption consumes pre-existing un-attributed value first, then generated portions in stable FIFO CardId/sequence order; battle-end residue and natural decay are charged to the remaining source portions. This ledger never enters gameplay state or save migration.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Simulation.Foundation --automation-report InRun07_Task01_GREEN --json
git add Source/GameXXK/Public/GameXXKCombatSimulationTypes.h Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp Source/GameXXK/Private/Tests/GameXXKCombatSimulationFoundationTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBalanceObservationTest.cpp
git diff --cached --check
git commit -m "feat: collect per card simulation telemetry"
```

### Task 2: Add deterministic depth-two policy evaluation

**Files:**
- Create: `Source/GameXXK/Public/GameXXKCombatPolicyEvaluator.h`
- Create: `Source/GameXXK/Private/GameXXKCombatPolicyEvaluator.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCombatSimulationPolicyTest.cpp`

- [ ] **Step 1: Write red deterministic-choice tests**

Create choices where immediate damage loses to a forecast lethal, a setup card enables a larger next action, ending the phase preserves Mana for a stronger next-round play, and equal scores differ only by stable CardId/target. Assert repeated and serialized runs choose the same IDs.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Simulation.Policy --automation-report InRun07_Task02_RED --json
```

- [ ] **Step 3: Add evaluator contracts**

```cpp
struct FGameXXKPolicyScore
{
    int32 TerminalRank = 0; // defeat = 0, ongoing = 1, victory = 2
    int32 LivingPartyUnits = 0;
    int32 PredictedPartyDeaths = 0;
    int64 PartyEffectiveHealth = 0;
    int64 EnemyPhaseAndHealthProgress = 0;
    int64 TaskFormulaTerrainProgress = 0;
    int32 SharedEnergyRemaining = 0;
    int64 PartyManaRemaining = 0;
    int64 WastePenalty = 0;

    bool IsBetterThan(const FGameXXKPolicyScore& Other) const;
};

class GAMEXXK_API FGameXXKCombatPolicyEvaluator final
{
public:
    static bool ChooseDecision(
        const FGameXXKRuntimeState& State,
        FGameXXKSimulationDecision& OutDecision,
        FString* OutError = nullptr);
};
```

- [ ] **Step 4: Implement bounded search through adapter copies**

Enumerate every legal CardInstanceId/target plus end-phase. Resolve each candidate on a state copy through `FGameXXKCardBattleAdapter`; retain the best eight first-ply states; enumerate one more player action for each. An end-phase candidate executes the locked enemy plan and next player-round boundary on its copy before second-ply enumeration, allowing deliberate Mana/resource preservation to be valued. Evaluate every leaf after forecasting its next locked enemy plan. Compare `FGameXXKPolicyScore` lexicographically in the declared field order, except that fewer `PredictedPartyDeaths` and lower `WastePenalty` are better. `PartyEffectiveHealth` is the saturating sum of living-party HP plus remaining Armor after that forecast. Define `EnemyPhaseAndHealthProgress` as `removed phase bars * encounter starting total enemy HP + current-bar HP removed`. Define `TaskFormulaTerrainProgress` as the sum of newly completed distinct Sorcerer task pieces, newly opened or advanced Healer formulas, and effective Formation terrain-benefit triggers. `WastePenalty` is the sum of this line's overkill, overheal, cap overflow, unused generated Armor on defeated units, and forced discard count. Forecast enemy damage from the already locked intent plan, including Armor and phase transitions, when calculating predicted deaths and party effective health. Stable ties prefer a card play over end-phase, then compare CardId, CardInstanceId, and TargetUnitId by ordinal `ToString()` value rather than process-local FName indices.

- [ ] **Step 5: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Simulation.Policy --automation-report InRun07_Task02_GREEN --json
git add Source/GameXXK/Public/GameXXKCombatPolicyEvaluator.h Source/GameXXK/Private/GameXXKCombatPolicyEvaluator.cpp Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp Source/GameXXK/Private/Tests/GameXXKCombatSimulationPolicyTest.cpp
git diff --cached --check
git commit -m "feat: evaluate combat decisions with lookahead"
```

### Task 3: Gate policy competence with tactical puzzles

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKCombatPolicyPuzzleTest.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCombatPolicyEvaluator.cpp`

- [ ] **Step 1: Add nine exact puzzles**

Create one fixture for each required behavior:

1. Armor/Agility/Guard survives a visible lethal intent.
2. DOT setup then trigger beats immediate attack.
3. Hunter saves/builds Charge then uses Heavy Arrow.
4. Hero Sorcerer completes four cards including one Exhausted card.
5. Hero and partner Healer formulas progress independently.
6. Formation changes terrain then explicitly triggers it.
7. Mark/Prey redirection protects the low-HP target.
8. Killing Ironfeather/Graymane/White Ape setup changes later forecast and is selected.
9. Ending the phase with Mana/resources unspent enables the forecast payoff next round and beats a low-value legal play.

Each puzzle asserts an exact first CardId/target and eventual victory within its small decision limit.

- [ ] **Step 2: Run red, correct evaluator scoring only, then run green**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Simulation.Policy.Puzzle --automation-report InRun07_Task03_RED --json
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Simulation.Policy.Puzzle --automation-report InRun07_Task03_GREEN --json
```

Do not change card/enemy data to make a policy puzzle pass; correct the declared score calculation/tie-break order or the fixture's legal-state construction.

- [ ] **Step 3: Commit**

```powershell
git add Source/GameXXK/Private/Tests/GameXXKCombatPolicyPuzzleTest.cpp Source/GameXXK/Private/GameXXKCombatPolicyEvaluator.cpp
git diff --cached --check
git commit -m "test: gate skilled combat policy"
```

### Task 4: Build the approved stage/build/cohort matrix

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKRouteBalanceTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKRouteBalanceRules.h`
- Modify: `Source/GameXXK/Private/GameXXKRouteBalanceRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKApprovedBalanceMatrixTest.cpp`

- [ ] **Step 1: Write red matrix-shape tests**

Assert six Hero profession packages, every legal unordered pair from six distinct permanent partner roles (`6 * C(6,2) = 90` builds), all 27 stages, four equipment cohorts, and stable seeds. The primary cohort has no relics; Hell 3-3 Treasure has 4/4/4 gems.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.RouteBalance.ApprovedMatrix --automation-report InRun07_Task04_RED --json
```

- [ ] **Step 3: Add explicit case dimensions**

```cpp
struct FGameXXKApprovedBalanceCase
{
    FName StageId = NAME_None;
    EGameXXKCharacterRole HeroPackage = EGameXXKCharacterRole::Invalid;
    EGameXXKCharacterRole PartnerRoleA = EGameXXKCharacterRole::Invalid;
    EGameXXKCharacterRole PartnerRoleB = EGameXXKCharacterRole::Invalid;
    FName EquipmentCohortId = NAME_None;
    FName GemDistributionId = NAME_None;
    int32 Seed = 0;
};
```

Add `MakeApprovedTuningCases(20 seeds)` and `MakeApprovedCertificationCases(100 seeds)`. Use runtime legality to omit invalid duplicate/formation combinations and report the exact count instead of padding cases.

Declare four stable cohort IDs and build only legal runtime equipment:

| Cohort ID | Deterministic construction |
|---|---|
| `MinimumEligible` | Six unenhanced pieces at the stage band's lower item level, using the lowest quality currently eligible from progression; no gems. |
| `BandMean` | Six unenhanced pieces at `DivideAndRoundNearest(Lower + Upper, 2)`, using the probability-weighted modal eligible quality; no gems. |
| `ReasonableStage` | Six pieces at the band upper level, median eligible enhancement/quality, plus all gems available in the seeded progression fixture distributed Attack/Defense/HP round-robin. |
| `MaximumExpected` | Six pieces at the band upper level with the highest progression-eligible enhancement/quality and gems; Hell 3-3 is fixed to six Treasure pieces and twelve Treasure gems split 4/4/4. |

Resolve eligible quality, enhancement, and gem inventory through existing equipment-economy/progression rules, never through hard-coded stage guesses. Where a probability distribution has two equal modes or medians, choose the lower rank. The same StageId/build/seed must construct byte-identical equipment instances.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.RouteBalance.ApprovedMatrix --automation-report InRun07_Task04_GREEN --json
git add Source/GameXXK/Public/GameXXKRouteBalanceTypes.h Source/GameXXK/Public/GameXXKRouteBalanceRules.h Source/GameXXK/Private/GameXXKRouteBalanceRules.cpp Source/GameXXK/Private/Tests/GameXXKApprovedBalanceMatrixTest.cpp
git diff --cached --check
git commit -m "feat: build approved balance matrix"
```

### Task 5: Add read-only report and review flags

**Files:**
- Create: `scripts/run_approved_balance_matrix.py`
- Create: `scripts/test_approved_balance_matrix.py`
- Modify: `scripts/propose_card_balance_tuning.py`
- Modify: `scripts/test_propose_card_balance_tuning.py`
- Modify: `scripts/README.md`

- [ ] **Step 1: Write red parser/flag tests**

Feed a synthetic report and assert flags for legal play rate below 10%, zero/waste at least 80%, output above 2x role median, resource rejection above 60%, waste above 70%, non-monotonic quality, selection above 65%, and task/formula noncompletion. Assert shard merging rejects a missing or duplicate case and produces the same canonical hash for shard counts 1 and 27. Assert the tool writes no source file.

- [ ] **Step 2: Run red**

```powershell
python scripts/test_approved_balance_matrix.py
python scripts/test_propose_card_balance_tuning.py
```

- [ ] **Step 3: Implement deterministic JSON/CSV output**

`run_approved_balance_matrix.py` launches the named Automation matrix, parses `index.json`, emits sorted `cases.csv`, `cards.csv`, `aggregates.json`, and `sha256.txt` under `Saved/Automation/<report>`. Support `--shards N --resume`; partition the canonical case key by a stable project hash, write one manifest per shard, and merge only when every expected key occurs exactly once. Sort all maps by stable string key before serialization. Exclude wall-clock timing, absolute paths, report names, and timestamps from hashed payloads so shard count and resume do not alter results. `propose_card_balance_tuning.py` reads these files and emits recommendations to stdout/a separate report only; it has no write path into `Source`, `Content`, or `Config`.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/test_approved_balance_matrix.py
python scripts/test_propose_card_balance_tuning.py
git add scripts/run_approved_balance_matrix.py scripts/test_approved_balance_matrix.py scripts/propose_card_balance_tuning.py scripts/test_propose_card_balance_tuning.py scripts/README.md
git diff --cached --check
git commit -m "feat: report approved balance telemetry"
```

### Task 6: Run tuning gate and document findings

**Files:**
- Create: `docs/production/2026-09-03-in-run-balance-certification.md`

- [ ] **Step 1: Run the 20-seed gate**

```powershell
python scripts/run_approved_balance_matrix.py --mode tuning --seeds 20 --shards 27 --resume --report InRun07_Tuning_20
```

Expected maximum is 194,400 cases; the report records the actual legal-build count, terminal rate, hashes, per-stage/build/cohort aggregates, and review flags.

Create the certification document with fixed sections for source commit/schema, case dimensions and actual count, explicit no-relic scope, equipment/gem cohort definitions, canonical report hash, termination/error totals, stage/build summaries, per-card flags, mandatory observation systems, and approved follow-up changes. Link report paths as evidence without committing generated CSV files.

- [ ] **Step 2: Triage in the approved order**

For every outlier, first classify rule/preview/policy error, then opening-intent alignment, then phase payoff, then support Armor/healing, and only then base stats. Correct rule/preview/policy defects by returning to the exact owning plan task. If an authored number still needs tuning, record the one proposed variable and evidence, pause this suite, and write a focused TDD micro-plan naming its exact catalog and test paths for approval; do not make an unscripted value change inside this reporting task.

- [ ] **Step 3: Re-run the same seed set after each approved change**

```powershell
python scripts/run_approved_balance_matrix.py --mode tuning --seeds 20 --shards 27 --resume --report InRun07_Tuning_20_After
```

After any approved focused correction, record both hashes and the single changed variable in the certification document. Never auto-apply a recommendation.

- [ ] **Step 4: Commit the tuning evidence**

```powershell
git add docs/production/2026-09-03-in-run-balance-certification.md
git diff --cached --check
git commit -m "docs: record in run tuning matrix"
```

Any separately approved catalog change must already have its own red/green commit before this evidence commit.

### Task 7: Run final certification and Hell 3-3 sensitivity

**Files:**
- Modify: `docs/production/2026-09-03-in-run-balance-certification.md`

- [ ] **Step 1: Run 100-seed certification**

```powershell
python scripts/run_approved_balance_matrix.py --mode certification --seeds 100 --shards 27 --resume --report InRun07_Certification_100
```

Expected maximum is 972,000 cases, all terminal and deterministic.

- [ ] **Step 2: Run Hell 3-3 200-seed gem sensitivity**

```powershell
python scripts/run_approved_balance_matrix.py --mode hell-3-3 --seeds 200 --gem-distributions balanced,attack,defense,health --shards 18 --resume --report InRun07_Hell33_200
```

The binding balanced Treasure cohort must have 45%-55% representative win rate, winning median 9-11 rounds, and strictly more than half of its wins finish in 8-14 rounds. Report each Hero role and partner pair; do not require every matchup to equal 50%, but flag a reasonable archetype with no executable defensive line.

- [ ] **Step 3: Repeat and compare hashes**

```powershell
python scripts/run_approved_balance_matrix.py --mode certification --seeds 100 --shards 27 --report InRun07_Certification_100_Repeat
python scripts/run_approved_balance_matrix.py --mode hell-3-3 --seeds 200 --gem-distributions balanced,attack,defense,health --shards 18 --report InRun07_Hell33_200_Repeat
```

Assert case CSV and aggregate SHA-256 values match the first runs byte-for-byte.

- [ ] **Step 4: Run final engine gates**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK --automation-report InRun07_GameXXK_FINAL --fail-on-automation-warnings --json
python scripts/gamexxk_real_play_flow_mcp.py --flow desktop-training-settlement
```

Classify pre-existing accepted warnings before using `--fail-on-automation-warnings`; the final run itself must contain zero unclassified warnings, failures, not-run tests, or errors.

- [ ] **Step 5: Commit certification**

Update the certification document with both repeated hashes, the binding balanced Hell 3-3 win-rate/round distribution, per-role and partner-pair findings, all remaining human-review flags, engine/PIE gate results, and an explicit statement that relic balance remains excluded/deferred.

```powershell
git add docs/production/2026-09-03-in-run-balance-certification.md
git diff --cached --check
git commit -m "docs: certify overall in run balance"
```

Plan 7 is complete only when policy puzzles pass, reports are deterministic, every active CardId has telemetry coverage, and Hell 3-3 meets its approved benchmark without hidden intent or phase exceptions.
