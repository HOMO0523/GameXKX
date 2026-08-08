# Card Balance Observation Design

## Objective

Collect reproducible, read-only evidence about the current card battle balance until 2026-08-08 17:30 Asia/Shanghai without changing gameplay rules, card definitions, assets, maps, or saves.

The observation must preserve every one of the 2,400 locked route-balance cases even when an individual case reaches `Simulation.MaxRounds`, then aggregate results by chapter, node kind, equipment cohort, quest NPC, and repeated run.

## Chosen Approach

Use a diagnostic UE automation test plus a Python deadline runner.

- The UE test calls the existing `FGameXXKRouteBalanceRules::RunCase` for every locked case. Unlike `RunFullMatrix`, it records an error row and continues after a stalemate.
- The Python runner launches the diagnostic test in clean commandlet processes, validates UE's `index.json`, parses the generated case CSV, hashes each run, and repeats until the deadline.
- Static catalog auditing runs in Python against the authoritative C++ card catalog and quality classification. It does not reproduce combat rules.

Rejected alternatives:

1. Repeating the existing `FullMatrixExecution` test would continue stopping at the first stalemate and produce no usable matrix.
2. Reimplementing combat in Python would create a second rule set and make numerical conclusions untrustworthy.

## Components

### UE diagnostic test

Create `Source/GameXXK/Private/Tests/GameXXKCardBalanceObservationTest.cpp` with automation path `GameXXK.Diagnostics.CardBalanceObservation`.

For each expanded case it records:

- stable case identity: cohort, quest NPC, equipment, chapter, node, seed;
- outcome: `Victory`, `Defeat`, or `Stalemate/Error`;
- rounds, remaining party health, first-round deaths;
- damage, healing, armor, statuses produced, and statuses consumed by source;
- the exact simulator error when the case cannot resolve.

The test writes `Saved/BalanceObservation/<run-id>/cases.csv`. `<run-id>` comes from `-GameXXKBalanceObservationId=` and is restricted to letters, digits, dash, and underscore. The test passes when all 2,400 rows were attempted and the CSV was saved; stalemates are observations, not automation infrastructure failures.

### Python runner and analyzer

Create `scripts/run_card_balance_observation.py`.

Responsibilities:

- launch `UnrealEditor-Cmd.exe` without Live Coding or Hot Reload;
- create unique report, log, and evidence directories for each iteration;
- validate the diagnostic automation state from `index.json`;
- require exactly 2,400 unique case rows;
- calculate outcome counts, win rates, median/P90 rounds, status utilization, cohort spread, repeated-run hashes, and recurring stalemate case IDs;
- audit the static card catalog for energy, quality, zero-cost draw/resource cards, and status producer/consumer coverage;
- repeat while another clean run can start before 17:30;
- write `final_summary.json` and `final_summary.md` after the deadline.

The runner never edits tracked files. Runtime evidence lives under `Saved/BalanceObservation` and `Saved/Automation/CardBalanceObservation`.

## Failure Handling

- A missing report, nonzero commandlet exit, failed diagnostic automation test, malformed CSV, duplicate case, or case-count mismatch is recorded as a failed iteration.
- Failed iterations do not delete earlier evidence.
- A simulator `MaxRounds` result remains a case-level stalemate and does not fail the iteration.
- If UE startup or compilation leaves no time for another iteration, the runner stops launching work and summarizes completed runs.
- The runner never kills a normal editor session. It only owns the commandlet process it starts.

## Analysis Boundaries

The existing `Skilled` policy is known to undervalue statuses, draw, resource generation, and future intent. Therefore observed win rates describe the current policy, not human-player difficulty. The analysis may compare cohorts and identify systematic blind spots, but it must not present these rates as final tuning targets.

## Formation-Master Targeting Addendum

The observation pass also diagnoses the reported formation-master cards that appear unable to target or remain stuck in hand. This remains test-only:

- enumerate all 18 formation-master definitions across all seven concrete terrains;
- build a real preview and resolve each of the 126 card/terrain pairs with living self, ally, and enemy candidates;
- validate `SingleAlly`/`SingleEnemy` sides, terrain overrides to `AllAllies`, exact hand removal after successful commits, and atomic hand preservation after rejected commits;
- exercise the Board target-proxy click for a friendly target and the automatic water-terrain override;
- prove that `GuanShi` and `BaMenLunZhuan` intentionally block later cards only while their insight/forced-discard choice is pending, then unblock after submission;
- statically report same-pool cards that have identical quality, mana, target, and effects but a strictly worse energy cost.

The pass does not repair any confirmed targeting or presentation defect. A rule-layer failure, Board/proxy failure, and an interaction-clarity problem are reported separately so a later implementation can use a focused failing test.

## Acceptance

- No tracked gameplay or asset file changes.
- The new Python unit tests pass.
- The UE module compiles through UBT with the editor closed.
- One smoke run produces exactly 2,400 unique case rows.
- At least one repeated run completes before or across the 17:30 deadline.
- The final summary contains deterministic hashes, per-bucket outcomes, round distributions, status utilization, and recurring stalemate cases.
