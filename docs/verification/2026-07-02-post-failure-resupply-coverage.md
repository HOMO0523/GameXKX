# 2026-07-02 Post-Failure Resupply Coverage Verification

## Scope

This slice closes the weak evidence gap for the active MVP goal's "failure -> return to town -> resupply -> retry" requirement.

Changes:

- `GameXXK.MVP.PIE.PlayableRootCommandsDriveFullLoop` now drives a post-failure town resupply through the real UMG root command path.
- After `FailBattle`, the test buys healing powder, verifies gold and inventory changes, damages the player, uses the healing powder, verifies HP and inventory changes, then retries the dungeon and clears through Boss.
- `GameXXK.MVP.PIE.PlayableRootPostFailureResupplyRetry` is a focused automation test for the same failure-return-resupply-retry requirement, so the coverage gate no longer depends on the broad full-loop test name alone.
- `scripts/gamexxk_mvp_playtest.py` now emits `flow_evidence`, parses Unreal Automation results into `automation_tests`, and reports `flow_coverage.ok`.

## TDD Evidence

Script coverage red check:

```powershell
python -c "import scripts.gamexxk_mvp_playtest as p; assert hasattr(p, 'FLOW_EVIDENCE'), 'missing FLOW_EVIDENCE'; assert any('post-failure resupply' in item.get('requirement', '') for item in p.FLOW_EVIDENCE), 'missing post-failure resupply coverage'"
```

Observed result: exit `1`, `AssertionError: missing FLOW_EVIDENCE`.

Script coverage green check:

```powershell
python -c "import scripts.gamexxk_mvp_playtest as p; assert hasattr(p, 'FLOW_EVIDENCE'), 'missing FLOW_EVIDENCE'; assert any('post-failure resupply' in item.get('requirement', '') for item in p.FLOW_EVIDENCE), 'missing post-failure resupply coverage'"
```

Observed result: exit `0`.

Dedicated coverage red playtest:

```powershell
python scripts\gamexxk_mvp_playtest.py --build-timeout 900 --test-timeout 300 --report Saved\HarnessReports\post-failure-resupply-dedicated-red-playtest.json
```

Observed result: build exit `0`, automation exit `0`, report `"ok": false`, and `flow_coverage.missing` contains `GameXXK.MVP.PIE.PlayableRootPostFailureResupplyRetry`.

Dedicated coverage green playtest:

```powershell
python scripts\gamexxk_mvp_playtest.py --build-timeout 900 --test-timeout 300 --report Saved\HarnessReports\post-failure-resupply-dedicated-green-playtest.json
```

Observed result: build exit `0`, automation exit `0`, report `"ok": true`, and report `flow_coverage.ok` is `true` with `10` mapped evidence requirements. The parsed automation results include successful `GameXXK.MVP.PIE.PlayableRootPostFailureResupplyRetry`.

## Current Boundary

This remains a command-surface proof rather than a full hand-played mouse/keyboard recording. It strengthens the scripted MVP proof that the flow exists and is enforced by automation.
