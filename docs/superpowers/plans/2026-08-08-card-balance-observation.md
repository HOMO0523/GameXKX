# Card Balance Observation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a read-only diagnostic matrix and deadline runner that preserve all 2,400 current route-balance outcomes and summarize repeated runs at 17:30.

**Architecture:** A UE automation test remains the only executor of gameplay rules and writes one CSV row per locked case. A Python runner owns clean commandlet launches, validates reports and CSVs, aggregates repeated observations, audits static card metadata, and writes final evidence under `Saved/`.

**Tech Stack:** Unreal Engine 5.8 C++ automation tests, UBT, Python 3 standard library, `unittest`, CSV/JSON/Markdown evidence.

---

### Task 1: Define the Python evidence contract with failing tests

**Files:**
- Create: `scripts/test_card_balance_observation.py`
- Create: `scripts/run_card_balance_observation.py`

- [ ] **Step 1: Write a failing parser test**

```python
from io import StringIO
from run_card_balance_observation import aggregate_case_rows, read_case_rows


def test_aggregate_preserves_victory_defeat_and_stalemate():
    rows = read_case_rows(StringIO(
        "cohort,quest_npc,equipment_set,equipment_quality,enhancement,chapter,node,seed_ordinal,seed,outcome,rounds,remaining_party_health,first_round_deaths,damage_by_source,healing_by_source,armor_by_source,status_produced,status_consumed,error\n"
        "NakedBaseline,Npc.TusiChief,None,Common,0,1,Battle,0,900000,Victory,4,120,0,Player=100,,,,Burn=2,Burn=1,\n"
        "NakedBaseline,Npc.TusiChief,None,Common,0,1,Battle,1,900001,Defeat,6,0,0,Player=50,,,,,,,\n"
        "NakedBaseline,Npc.TusiChief,None,Common,0,1,Elite,2,901002,Stalemate,100,80,0,,,,,,,Simulation.MaxRounds\n"
    ))
    summary = aggregate_case_rows(rows, expected_case_count=3)
    assert summary["outcomes"] == {"Victory": 1, "Defeat": 1, "Stalemate": 1}
    assert summary["status_utilization"]["Burn"] == {"produced": 2, "consumed": 1, "ratio": 0.5}
```

- [ ] **Step 2: Run the test and verify RED**

Run: `python -B -m unittest scripts.test_card_balance_observation -v`

Expected: import failure because `run_card_balance_observation.py` does not exist.

- [ ] **Step 3: Implement the minimal parser API**

Implement these public functions in `scripts/run_card_balance_observation.py`:

```python
def parse_metric_map(value: str) -> dict[str, int]:
    result = {}
    for item in filter(None, value.split(";")):
        key, separator, raw_value = item.partition("=")
        if not separator or not key:
            raise ValueError(f"invalid metric item: {item!r}")
        result[key] = int(raw_value)
    return result


def read_case_rows(source) -> list[dict[str, object]]:
    rows = []
    for row in csv.DictReader(source):
        parsed = dict(row)
        for key in ("enhancement", "chapter", "seed_ordinal", "seed", "rounds",
                    "remaining_party_health", "first_round_deaths"):
            parsed[key] = int(parsed[key])
        for key in ("damage_by_source", "healing_by_source", "armor_by_source",
                    "status_produced", "status_consumed"):
            parsed[key] = parse_metric_map(parsed[key])
        rows.append(parsed)
    return rows


def aggregate_case_rows(rows, expected_case_count: int = 2400) -> dict[str, object]:
    if len(rows) != expected_case_count:
        raise ValueError(f"expected {expected_case_count} cases, found {len(rows)}")
    identities = {(row["cohort"], row["chapter"], row["node"], row["seed"]) for row in rows}
    if len(identities) != len(rows):
        raise ValueError("case identities are not unique")
    outcomes = Counter(row["outcome"] for row in rows)
    produced = Counter()
    consumed = Counter()
    for row in rows:
        produced.update(row["status_produced"])
        consumed.update(row["status_consumed"])
    utilization = {
        key: {"produced": produced[key], "consumed": consumed[key],
              "ratio": round(consumed[key] / produced[key], 6) if produced[key] else 0.0}
        for key in sorted(produced.keys() | consumed.keys())
    }
    return {"case_count": len(rows), "outcomes": dict(outcomes),
            "status_utilization": utilization}
```

The implementation must reject duplicate `(cohort, chapter, node, seed)` identities and a count different from `expected_case_count`.

- [ ] **Step 4: Run the parser test and verify GREEN**

Run: `python -B -m unittest scripts.test_card_balance_observation -v`

Expected: all parser tests pass.

### Task 2: Add the UE continue-on-stalemate diagnostic

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKCardBalanceObservationTest.cpp`

- [ ] **Step 1: Establish RED through the missing automation path**

Run:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nopause -nosplash -nullrhi -ExecCmds='Automation RunTests GameXXK.Diagnostics.CardBalanceObservation; Quit'
```

Expected: zero matching tests/no generated `Saved/BalanceObservation/smoke/cases.csv`.

- [ ] **Step 2: Implement the automation test**

The test must:

```cpp
const FGameXXKRouteBalanceMatrix Matrix = FGameXXKRouteBalanceRules::MakeLockedFullMatrix();
TArray<FGameXXKRouteBalanceCase> Cases;
FString Error;
TestTrue(TEXT("locked matrix expands"), FGameXXKRouteBalanceRules::ExpandCases(Matrix, Cases, &Error));
TestEqual(TEXT("diagnostic attempts 2400 cases"), Cases.Num(), 2400);
for (const FGameXXKRouteBalanceCase& Case : Cases)
{
    FGameXXKRouteBalanceCaseResult Result;
    FString CaseError;
    const bool bResolved = FGameXXKRouteBalanceRules::RunCase(Case, Result, &CaseError);
    const TCHAR* Outcome = bResolved
        ? (Result.Metrics.bVictory ? TEXT("Victory") : TEXT("Defeat"))
        : (CaseError.Contains(TEXT("MaxRounds")) ? TEXT("Stalemate") : TEXT("Error"));
    Csv += BuildCaseCsvRow(Case, bResolved ? &Result.Metrics : nullptr, Outcome, CaseError);
}
TestTrue(TEXT("diagnostic CSV saves"), FFileHelper::SaveStringToFile(Csv, *OutputPath));
```

Use `FParse::Value(FCommandLine::Get(), TEXT("GameXXKBalanceObservationId="), RunId)` and reject an unsafe run ID. Sort metric-map keys before serialization so repeated hashes are deterministic.

- [ ] **Step 3: Compile through UBT**

Run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload
```

Expected: exit code 0 and `GameXXKEditor` build succeeds.

- [ ] **Step 4: Run the diagnostic smoke test**

Run the diagnostic with `-GameXXKBalanceObservationId=smoke` and a unique report directory.

Expected: one successful automation test and a CSV containing 2,400 data rows.

### Task 3: Implement the deadline runner and static catalog audit

**Files:**
- Modify: `scripts/test_card_balance_observation.py`
- Modify: `scripts/run_card_balance_observation.py`

- [ ] **Step 1: Write failing tests for catalog audit and report validation**

```python
def test_catalog_audit_finds_current_card_count_and_zero_cost_draw_risk():
    audit = audit_card_catalog(PROJECT_ROOT)
    assert audit["card_count"] == 174
    assert "Npc.JinGui.ShiJingErMu" in audit["zero_cost_draw_cards"]


def test_validate_automation_report_rejects_failed_test(tmp_path):
    path = tmp_path / "index.json"
    path.write_text('{"succeeded":0,"succeededWithWarnings":0,"failed":1,"tests":[]}', encoding="utf-8")
    with self.assertRaises(RuntimeError):
        validate_automation_report(path)
```

- [ ] **Step 2: Run tests and verify RED**

Run: `python -B -m unittest scripts.test_card_balance_observation -v`

Expected: missing `audit_card_catalog` and `validate_automation_report` failures.

- [ ] **Step 3: Implement runner orchestration**

Add:

```python
@dataclass(frozen=True)
class ObservationConfig:
    project_root: Path
    ue_editor: Path
    timeout_seconds: int = 600


def validate_automation_report(index_path: Path) -> dict[str, object]:
    report = json.loads(index_path.read_text(encoding="utf-8"))
    if int(report.get("failed", 0)) != 0:
        raise RuntimeError(f"diagnostic automation failed: {index_path}")
    if int(report.get("succeeded", 0)) + int(report.get("succeededWithWarnings", 0)) != 1:
        raise RuntimeError(f"diagnostic automation count is not one: {index_path}")
    return report


def run_until_deadline(config: ObservationConfig, deadline: datetime) -> list[dict[str, object]]:
    records = []
    iteration = 1
    while datetime.now().astimezone() < deadline:
        run_id = f"{datetime.now():%Y%m%d_%H%M%S}_{iteration:03d}"
        records.append(run_observation_once(config, run_id))
        iteration += 1
    return records
```

Build the command as a list named `command` and call `subprocess.run(command, shell=False)`. Use unique absolute paths and `-AbsLog`/`-ReportOutputPath`. Parse `index.json`; do not infer success from localized log text.

- [ ] **Step 4: Run all Python tests and verify GREEN**

Run: `python -B -m unittest scripts.test_card_balance_observation -v`

Expected: all observation tests pass with no warnings.

### Task 4: Run until 17:30 and analyze

**Files:**
- Generate: `Saved/BalanceObservation/<run-id>/cases.csv`
- Generate: `Saved/BalanceObservation/observation_runs.jsonl`
- Generate: `Saved/BalanceObservation/final_summary.json`
- Generate: `Saved/BalanceObservation/final_summary.md`

- [ ] **Step 1: Start the deadline run**

Run:

```powershell
python -B scripts/run_card_balance_observation.py --deadline '2026-08-08 17:30:00' --ue-editor 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
```

Expected: one progress line per completed iteration and continued execution until the deadline.

- [ ] **Step 2: Verify evidence integrity**

Run: `python -B scripts/run_card_balance_observation.py --summarize-existing`

Expected: exit 0, every accepted run has 2,400 unique cases, and final JSON/Markdown paths are printed.

- [ ] **Step 3: Run focused regressions**

Run:

```powershell
python -B -m unittest scripts.test_card_balance_observation scripts.test_route_balance_matrix -v
```

Expected: all tests pass.

- [ ] **Step 4: Review and report**

Compare repeated hashes, recurring stalemate identities, resolved win rates, round distributions, cohort spread, status production/consumption, and static catalog risks. Explicitly label all win rates as current-policy observations rather than human difficulty targets.
