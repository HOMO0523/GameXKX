# Three-Chapter Route Balance Certification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a reproducible, real-combat-rules certification run that executes exactly 2,400 fixed-seed battles across the current three chapters, emits an auditable report, and only applies reviewed, explicit balance data.

**Architecture:** Keep the existing `FGameXXKCombatSimulationRules` as the only source of combat resolution. A versioned JSON matrix describes 8 fixed player/NPC/equipment cohorts and 3 node kinds; each cohort/node pair gets 100 fixed seeds, with chapters assigned round-robin per seed. A no-render commandlet validates the matrix, creates each scenario through current route/encounter rules, runs it through the existing battle adapter, and writes JSON/CSV evidence under `Saved/Reports/RouteBalance/`.

**Tech Stack:** Unreal Engine 5.8 C++20, existing `FGameXXKCombatSimulationRules`, UE Automation, UBT cold build, Python 3 standard-library `unittest`, Json/JsonUtilities, PowerShell.

---

## Locked certification contract

- The Full profile is exactly `8 cohorts × 3 node kinds × 100 seeds = 2,400` battles.
- Each 100-seed cohort/node group distributes chapters with `Chapter = 1 + ((SeedOrdinal + CohortIndex) % 3)`. Therefore every chapter/node group has either 266 or 267 samples across the 8 cohorts; it never silently omits a chapter.
- Node kinds are `Battle`, `Elite`, and `Boss`. All scenarios use the existing encounter builder, so battle means two normal monsters, elite means normal/elite/normal with elite in 2P, and boss means elite/boss/elite with boss in 2P.
- Route level is derived from chapter: chapter 1 = level 5, chapter 2 = level 10, chapter 3 = level 15. Equipment item level equals that route level; this preserves the route-level snapshot rule rather than inventing a second scaling model.
- The eight cohorts are fixed: naked baseline; 破军 + 宋金宝; 玄甲 + 月白; 青囊 + 周光祖; 追风 + 金贵; 蚀骨 + 琼么儿; 山河 + 土司首领; and a max-enhancement mixed regression cohort. The six task NPCs and six equipment sets are all represented. Every cohort uses the existing three-person party contract: hero + one permanent partner + one task NPC.
- Win-rate gates are measured per `(chapter, node kind)` and must remain: normal 55–70%, elite 35–50%, boss 15–35%. The report records the unrounded rate, sample count, wins, losses, failure reasons, mean rounds, and cohort breakdown.
- A single scenario must remain below one second. Full certification has a 30-minute target and a hard 60-minute cutoff including commandlet process time. Existing cold build time is recorded separately and never included as simulated combat time.
- The commandlet and Python scripts are diagnostic only: they do not write `Source/`, `SourceAssets/`, UE assets, card data, enemy data, or equipment data. Human review owns every numeric tuning change.

## File map

- Create: `SourceAssets/Balance/route-balance-matrix-v1.json` — immutable matrix, cohort definitions, fixed seed bases, profile, and thresholds.
- Create: `scripts/test_route_balance_matrix.py` — offline schema/count/distribution/gate-contract tests.
- Create: `scripts/run_route_balance_matrix.py` — validates matrix, invokes a persistent no-render commandlet, and validates returned JSON/CSV reports.
- Create: `scripts/test_route_balance_certification.py` — offline command construction, deadline, profile, and report integrity tests.
- Create: `scripts/run_route_balance_certification.py` — cold-build + one-battle preflight + selected full-profile orchestration.
- Create: `Source/GameXXK/Public/GameXXKRouteBalanceTypes.h` — serializable matrix-case, aggregate, and report structures.
- Create: `Source/GameXXK/Public/GameXXKRouteBalanceRules.h` — scenario construction and aggregate/gate evaluation only.
- Create: `Source/GameXXK/Private/GameXXKRouteBalanceRules.cpp` — real-rule fixture creation, fixed case expansion, and report aggregation.
- Create: `Source/GameXXK/Public/GameXXKRouteBalanceCommandlet.h` — no-render commandlet declaration.
- Create: `Source/GameXXK/Private/GameXXKRouteBalanceCommandlet.cpp` — matrix load, simulation execution, JSON/CSV output, and nonzero failure policy.
- Create: `Source/GameXXK/Private/Tests/GameXXKRouteBalanceSimulationTest.cpp` — C++ contract tests for deterministic case expansion and aggregation.
- Modify: `Source/GameXXK/GameXXK.Build.cs` — add `Json` and `JsonUtilities` as private dependencies.
- Create: `docs/verification/2026-07-24-route-balance-certification.md` — command lines, artifact paths, result totals, and manual tuning review template.

## Task 1: Lock the matrix and validate it offline

**Files:**
- Create: `SourceAssets/Balance/route-balance-matrix-v1.json`
- Create: `scripts/test_route_balance_matrix.py`

- [ ] **Step 1: Write the failing offline matrix contract test.**

```python
class RouteBalanceMatrixTests(unittest.TestCase):
    def test_full_profile_expands_to_exactly_2400_cases(self) -> None:
        matrix = load_matrix(MATRIX_PATH)
        cases = expand_full_cases(matrix)
        self.assertEqual(len(cases), 2400)
        self.assertEqual({case["node_kind"] for case in cases}, {"Battle", "Elite", "Boss"})
        self.assertEqual({case["cohort_id"] for case in cases}, {
            "NakedBaseline", "PoJunSong", "XuanJiaYueBai", "QingNangZhou",
            "ZhuiFengJinGui", "ShiGuQiong", "ShanHeTusi", "MixedMaxRegression"})

    def test_chapter_assignment_covers_every_chapter_node_gate(self) -> None:
        cases = expand_full_cases(load_matrix(MATRIX_PATH))
        buckets = Counter((case["chapter"], case["node_kind"]) for case in cases)
        self.assertEqual(set(buckets), {(chapter, kind) for chapter in (1, 2, 3)
                                       for kind in ("Battle", "Elite", "Boss")})
        self.assertTrue(all(count >= 264 for count in buckets.values()))
```

- [ ] **Step 2: Verify the test is red because the matrix loader and source JSON do not exist.**

Run from the scripts directory: `Push-Location scripts; python -m unittest test_route_balance_matrix -v; Pop-Location`

Expected: `ImportError` or `FileNotFoundError` naming `route-balance-matrix-v1.json`; do not create production C++ yet.

- [ ] **Step 3: Add the JSON matrix and minimal pure-Python loader/expander.**

Use this Full profile shape; `seed_ordinal` is 0–99 and `chapter` uses the locked round-robin formula.

```json
{
  "schema_version": 1,
  "profiles": {"Full": {"seed_count": 100, "node_kinds": ["Battle", "Elite", "Boss"]}},
  "route_levels": {"1": 5, "2": 10, "3": 15},
  "cohorts": [
    {"id": "NakedBaseline", "quest_npc": "Npc.TusiChief", "set": "None", "quality": "Common", "enhancement": 0},
    {"id": "PoJunSong", "quest_npc": "Npc.SongJinBao", "set": "PoJun", "quality": "Rare", "enhancement": 0},
    {"id": "XuanJiaYueBai", "quest_npc": "Npc.YueBai", "set": "XuanJia", "quality": "Rare", "enhancement": 0},
    {"id": "QingNangZhou", "quest_npc": "Npc.ZhouGuangZu", "set": "QingNang", "quality": "Rare", "enhancement": 0},
    {"id": "ZhuiFengJinGui", "quest_npc": "Npc.JinGui", "set": "ZhuiFeng", "quality": "Rare", "enhancement": 0},
    {"id": "ShiGuQiong", "quest_npc": "Npc.QiongMeiEr", "set": "ShiGu", "quality": "Rare", "enhancement": 0},
    {"id": "ShanHeTusi", "quest_npc": "Npc.TusiChief", "set": "ShanHe", "quality": "Epic", "enhancement": 5},
    {"id": "MixedMaxRegression", "quest_npc": "Npc.SongJinBao", "set": "PoJun", "quality": "Epic", "enhancement": 10}
  ],
  "gates": {"Battle": [0.55, 0.70], "Elite": [0.35, 0.50], "Boss": [0.15, 0.35]}
}
```

```python
def expand_full_cases(matrix: dict) -> list[dict]:
    profile = matrix["profiles"]["Full"]
    cases: list[dict] = []
    for cohort_index, cohort in enumerate(matrix["cohorts"]):
        for node_index, node_kind in enumerate(profile["node_kinds"]):
            for seed_ordinal in range(profile["seed_count"]):
                chapter = 1 + ((seed_ordinal + cohort_index) % 3)
                cases.append({"cohort_id": cohort["id"], "node_kind": node_kind,
                              "chapter": chapter, "seed": 900000 + cohort_index * 10000
                              + node_index * 1000 + seed_ordinal})
    return cases
```

- [ ] **Step 4: Verify the offline matrix suite is green.**

Run from the scripts directory: `Push-Location scripts; python -m unittest test_route_balance_matrix -v; Pop-Location`

Expected: all tests pass, including exact 2,400 count and all nine chapter/node buckets.

## Task 2: Add the real-rules C++ balance runner

**Files:**
- Create: `Source/GameXXK/Public/GameXXKRouteBalanceTypes.h`
- Create: `Source/GameXXK/Public/GameXXKRouteBalanceRules.h`
- Create: `Source/GameXXK/Private/GameXXKRouteBalanceRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKRouteBalanceSimulationTest.cpp`

- [ ] **Step 1: Write a failing deterministic expansion test before the runner exists.**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameXXKRouteBalanceSimulationTest,
    "GameXXK.RouteBalance.FullMatrixContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteBalanceSimulationTest::RunTest(const FString& Parameters)
{
    FGameXXKRouteBalanceMatrix Matrix = FGameXXKRouteBalanceRules::MakeLockedFullMatrix();
    TArray<FGameXXKRouteBalanceCase> Cases;
    FString Error;
    TestTrue(TEXT("full matrix expands"), FGameXXKRouteBalanceRules::ExpandCases(Matrix, Cases, &Error));
    TestEqual(TEXT("full matrix has exact case count"), Cases.Num(), 2400);
    TestTrue(TEXT("all cases are fixed-seed real-rule fixtures"),
        Algo::AllOf(Cases, [](const FGameXXKRouteBalanceCase& Item)
        { return Item.Seed > 0 && Item.RouteLevel == (Item.Chapter == 1 ? 5 : Item.Chapter == 2 ? 10 : 15); }));
    return true;
}
```

- [ ] **Step 2: Cold-build the red test and record the missing-contract failure.**

Run with D temporary storage: `$env:TEMP='D:\GameXXKBuildTemp'; $env:TMP='D:\GameXXKBuildTemp'; python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 30`

Expected: UBT fails because `GameXXKRouteBalanceRules` is absent. Save all packages through MCP first; never use Live Coding or Hot Reload.

- [ ] **Step 3: Implement case expansion and scenario construction using only existing rules.**

`BuildScenario` must call `UGameXXKMVPRules::CreateNewGame`, initialize the fixed hero/permanent-companion/task-NPC loadout through existing public card-run rules, equip fixtures through `FGameXXKEquipmentEconomyRules`, and call `FGameXXKEncounterRules::BuildFormation`. It then passes the resulting runtime state to `FGameXXKCombatSimulationRules::RunScenario`. It must not calculate health, damage, status, intent, or healing independently.

```cpp
bool FGameXXKRouteBalanceRules::RunCase(const FGameXXKRouteBalanceCase& Case,
    FGameXXKRouteBalanceCaseResult& OutResult, FString* OutError)
{
    FGameXXKSimulationScenario Scenario;
    if (!BuildScenario(Case, Scenario, OutError)) return false;
    TArray<FGameXXKSimulationTraceEntry> Trace;
    if (!FGameXXKCombatSimulationRules::RunScenario(Scenario, OutResult.Metrics, Trace, OutError)) return false;
    OutResult.Case = Case;
    return true;
}
```

- [ ] **Step 4: Cold-build, then run the focused automation test through MCP.**

Run cold build command from Step 2, then run `Automation RunTests GameXXK.RouteBalance.FullMatrixContract` through `scripts/ue_mcp_client.py`.

Expected: zero failures; no source data or assets modified by the test.

## Task 3: Add a no-render commandlet and report gate evaluation

**Files:**
- Create: `Source/GameXXK/Public/GameXXKRouteBalanceCommandlet.h`
- Create: `Source/GameXXK/Private/GameXXKRouteBalanceCommandlet.cpp`
- Modify: `Source/GameXXK/GameXXK.Build.cs`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteBalanceSimulationTest.cpp`

- [ ] **Step 1: Add a failing aggregate/gate automation assertion.**

```cpp
FGameXXKRouteBalanceReport Report;
Report.Results = MakeSyntheticResults(/* Battle wins */ 60, /* Elite wins */ 40, /* Boss wins */ 25);
TestTrue(TEXT("within locked tier gates"), FGameXXKRouteBalanceRules::EvaluateGates(Matrix, Report));
Report.Results[0].Metrics.bVictory = false;
TestFalse(TEXT("out-of-range normal rate fails its gate"), FGameXXKRouteBalanceRules::EvaluateGates(Matrix, Report));
```

- [ ] **Step 2: Verify it fails for the expected missing aggregation implementation.**

Run the cold pipeline from Task 2. Expected: an assertion failure naming `EvaluateGates`, not a build or editor-lifecycle failure.

- [ ] **Step 3: Implement report aggregation and commandlet write policy.**

The commandlet accepts `-Matrix=<absolute-json-path> -Profile=Full -Output=<absolute-report-directory>`. It writes `route-balance-full.json` and `route-balance-full.csv`; every JSON result contains `schema_version`, input matrix SHA-256, command line, UTC timestamp, case count, elapsed seconds, case results, nine aggregates, and gates. A malformed matrix, scenario runner error, count other than 2,400, elapsed time over 3,600 seconds, or gate failure returns nonzero. A gate failure still writes the complete diagnostic report.

```cpp
int32 UGameXXKRouteBalanceCommandlet::Main(const FString& Params)
{
    FGameXXKRouteBalanceMatrix Matrix;
    FString Profile, OutputDirectory, Error;
    if (!LoadAndValidateMatrix(Params, Matrix, Profile, OutputDirectory, Error)) return 2;
    FGameXXKRouteBalanceReport Report;
    const bool bRan = FGameXXKRouteBalanceRules::RunProfile(Matrix, Profile, Report, &Error);
    WriteReports(OutputDirectory, Report, Error);
    return bRan && Report.bWithinAllGates && Report.ElapsedSeconds <= 3600.0 ? 0 : 1;
}
```

- [ ] **Step 4: Cold-build and run both route-balance automation tests.**

Run the cold pipeline, then `Automation RunTests GameXXK.RouteBalance`.

Expected: all route-balance tests pass; commandlet-only report files remain under `Saved/Reports/RouteBalance/`.

## Task 4: Add repeatable Python orchestration and certify safely

**Files:**
- Create: `scripts/test_route_balance_certification.py`
- Create: `scripts/run_route_balance_matrix.py`
- Create: `scripts/run_route_balance_certification.py`
- Create: `docs/verification/2026-07-24-route-balance-certification.md`

- [ ] **Step 1: Write failing orchestrator tests.**

```python
class RouteBalanceCertificationTests(unittest.TestCase):
    def test_full_command_uses_no_render_and_d_temp(self) -> None:
        command, env = build_command(PROJECT, MATRIX, REPORT_DIR, "Full")
        self.assertIn("-run=GameXXKRouteBalance", command)
        self.assertIn("-nullrhi", command)
        self.assertEqual(env["TEMP"], r"D:\\GameXXKBuildTemp")
        self.assertEqual(env["TMP"], r"D:\\GameXXKBuildTemp")

    def test_report_rejects_wrong_case_count(self) -> None:
        with self.assertRaisesRegex(ValueError, "2400"):
            validate_full_report({"case_count": 2399, "elapsed_seconds": 1.0})
```

- [ ] **Step 2: Verify the tests fail before scripts exist.**

Run: `python -m unittest scripts.test_route_balance_certification -v`

Expected: import error for `run_route_balance_certification`; do not start a build in this red step.

- [ ] **Step 3: Implement the two scripts.**

`run_route_balance_matrix.py` performs only matrix/report validation and one commandlet invocation. `run_route_balance_certification.py` first runs `ue_tdd_pipeline.py --pie-duration 0`, then runs one explicit `--profile Preflight` battle and rejects it if it takes one second or more, then runs Full with a 60-minute subprocess timeout. Both scripts set `TEMP` and `TMP` to `D:\GameXXKBuildTemp`, write reports under the project `Saved` directory, and print JSON summaries.

- [ ] **Step 4: Verify script tests and record one full certification report.**

Run:

```powershell
Push-Location scripts
python -m unittest test_route_balance_matrix test_route_balance_certification -v
Pop-Location
$env:TEMP='D:\GameXXKBuildTemp'; $env:TMP='D:\GameXXKBuildTemp'
python scripts/run_route_balance_certification.py --profile Full --report-dir Saved/Reports/RouteBalance
```

Expected: all offline tests pass; the command returns `0` only if all gates and time limits pass. Store the command, report SHA-256, total duration, nine win-rate rows, and any manual tuning proposal in `docs/verification/2026-07-24-route-balance-certification.md`. A failed gate is evidence for a human-reviewed tuning patch, never an automatic content rewrite.

## Plan self-review

- Spec coverage: exact 2,400 count, 100-seed definition, 3 chapters, 3 node types, task NPC/equipment coverage, skilled real-rule simulation, fixed win-rate bands, single-battle and 30/60-minute limits, reporting, and no automatic tuning all have an implementation task.
- No scope expansion: route topology, enemy catalog, art assets, PSD/UI, player camera, and save migration are untouched.
- TDD order: every new production surface has an offline or C++ test written and observed red before implementation; every C++ task uses a cold build and focused MCP automation afterward.
- Project constraint: this plan executes on the root `main` project only, with no worktree, and keeps all temporary build storage on D.

---

## 2026-07-24 execution outcome

- Completed in the root project: the 2,400-case C++ real-rule matrix, deterministic route-level formation parity, a Tiger stale-target fix, temporary calibration projection support, and an authored route difficulty table shared by formal battle entry and the simulator.
- The previously planned no-render commandlet/CSV report runner remains a follow-up production convenience; certification for this pass is recorded through UE Automation output in `docs/verification/2026-07-24-route-balance-certification.md`.
- Human-approved authored scales now live in `FGameXXKEncounterRules::GetAuthoredStatScale`. They are intentionally keyed by chapter and node kind, apply to every enemy in the encounter, and do not mutate the base enemy catalog, save data, or the route map structure.
- The temporary `FGameXXKRouteBalanceCalibrationProfile` is diagnostic only. Its explicit values can override an encounter projection for experiments, but normal game play and default matrix runs use the authored table.
- The earlier cross-restart drift was traced to `GetTypeHash(FName)` being used as the random-living-party target seed. `FName` comparison indices are process-local, so the same battle could target a different party member after restarting the editor. The resolver now uses `FGameXXKCardBattleAdapter::MakeStableEnemyIntentTargetSeed`, a CRC of the stable textual unit ID plus the round number.
- The regression suite now includes `GameXXK.Integration.CardBattleAdapter` for the stable seed contract and `GameXXK.RouteBalance.Determinism.ChapterTwoNormalReplay` for 267 fixed chapter-two normal fixtures. The latter passed both in-process replay and a clean editor restart with the same fingerprint: `0x1A497B31`.
- Final certification is complete: two clean-editor 2,400-case target-gate runs and one normal authored-rules matrix run produced the same nine buckets. The normal rules matrix completed its 2,400 real-rule cases in `27.892` seconds. Exact results and commands are recorded in the verification document.
