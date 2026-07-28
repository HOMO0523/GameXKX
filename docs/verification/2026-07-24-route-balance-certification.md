# Three-Chapter Route Balance Certification — Final

## Scope and status

- Certification date: 2026-07-24
- Status: **certified — repeatable across clean editor restarts**
- Matrix: `SourceAssets/Balance/route-balance-matrix-v1.json`
- Profile: 8 fixed player/NPC/equipment cohorts × 3 node kinds × 100 fixed seeds = **2,400 real-rule battles**.
- Resolver: `FGameXXKCombatSimulationRules` through the existing card-battle adapter. No alternate damage, healing, status, or intent resolver was introduced.
- Build discipline: cold UBT using `scripts/ue_tdd_pipeline.py`; `TEMP` and `TMP` point to `D:\GameXXKBuildTemp`.
- Runtime ownership: `FGameXXKEncounterRules::GetAuthoredStatScale` is the single authored difficulty table used by both normal battle entry and the default matrix projection. Catalog base stats, route topology, save data, UI assets, and encounter selection remain unchanged.

## Approved authored encounter scales

All percentages apply to every enemy in the relevant encounter after its catalog stat and route-level snapshot are resolved. Values are `(HP / attack / defense)`.

| Chapter | Normal | Elite | Boss |
| --- | --- | --- | --- |
| 1 | 140 / 850 / 100 | 160 / 270 / 100 | 120 / 120 / 100 |
| 2 | 140 / 540 / 100 | 160 / 170 / 105 | 100 / 100 / 100 |
| 3 | 140 / 540 / 100 | 160 / 180 / 110 | 80 / 90 / 100 |

The table is deliberately encounter-based: normal, elite, and Boss nodes must occupy different pressure bands, while their monster identities and 1P/2P/3P formation rules stay intact.

## Verification commands

```powershell
$env:TEMP='D:\GameXXKBuildTemp'; $env:TMP='D:\GameXXKBuildTemp'
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 36

# UE MCP console commands, run after the cold pipeline restarts the editor
Automation RunTests GameXXK.Route.EncounterFormation.AuthoredStatScales
Automation RunTests GameXXK.MVP.Battle.EncounterRules
Automation RunTests GameXXK.Integration.CardBattleAdapter
Automation RunTests GameXXK.RouteBalance.CalibrationProfileProjectsExactEnemyStats
Automation RunTests GameXXK.RouteBalance.Determinism.ChapterTwoNormalReplay
Automation RunTests GameXXK.RouteBalance.FinalCandidateTargets
Automation RunTests GameXXK.RouteBalance.FullMatrixExecution
```

The final normal-authored-rules matrix completed in **27.892 seconds**, well under the 30-minute operational target and 60-minute hard cap. Two independent clean-editor `FinalCandidateTargets` runs produced identical nine-bucket results. The chapter-two normal replay regression also produced an identical `0x1A497B31` fingerprint across a clean editor restart.

### Repeatability correction

The drift was not a tuning issue. `RandomLivingParty` target selection derived its seed from `GetTypeHash(Source.UnitId)`. `SourceUnitId` is an `FName`, whose comparison index depends on process startup and asset-load order. Consequently a fixed battle could select a different player-side target after an editor restart, changing later decisions and aggregate results.

`FGameXXKCardBattleAdapter::MakeStableEnemyIntentTargetSeed` now uses `FCrc::StrCrc32(SourceUnitId.ToString())` combined with the round number. The function is covered by `GameXXK.Integration.CardBattleAdapter`; it makes target selection depend only on the saved textual unit ID and battle round.

## Final full-matrix result

| Chapter | Node | Samples | Wins | Win rate | Locked target | Result |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| 1 | Normal | 267 | 168 | 62.92% | 55–70% | Pass |
| 1 | Elite | 267 | 127 | 47.57% | 35–50% | Pass |
| 1 | Boss | 267 | 69 | 25.84% | 15–35% | Pass |
| 2 | Normal | 267 | 153 | 57.30% | 55–70% | Pass |
| 2 | Elite | 267 | 104 | 38.95% | 35–50% | Pass |
| 2 | Boss | 267 | 64 | 23.97% | 15–35% | Pass |
| 3 | Normal | 266 | 149 | 56.02% | 55–70% | Pass |
| 3 | Elite | 266 | 99 | 37.22% | 35–50% | Pass |
| 3 | Boss | 266 | 57 | 21.43% | 15–35% | Pass |

## Guardrails retained

- `GameXXK.Route.EncounterFormation.AuthoredStatScales` locks all nine scale triples.
- `GameXXK.MVP.Battle.EncounterRules` proves real battle entry applies the same authored scale to the formal enemy projection. Its focused action checks neutralize enemy damage only after the projection assertion, so they remain unit tests rather than accidental difficulty checks.
- `GameXXK.RouteBalance.CalibrationProfileProjectsExactEnemyStats` proves a diagnostic override can change only initial projected values, never enemy identity or combat level.
- `GameXXK.RouteBalance.Determinism.ChapterTwoNormalReplay` executes all 267 fixed chapter-two normal fixtures twice in one process and fingerprints every semantic result. The same aggregate fingerprint also matched after a clean editor restart.
- `GameXXK.RouteBalance.FinalCandidateTargets` asserts all nine target ranges on 2,400 fixed fixtures. It passed identically in two clean editor processes.
- `GameXXK.RouteBalance.FullMatrixExecution` repeats the same workload on the normal game-rule path, so a test-only profile cannot mask a runtime mismatch. It passed after the correction in 27.892 seconds with the certified values above.
- The Tiger `Mark Prey` stale-target path retargets a living party member rather than failing the battle after a forecast target dies during end-of-player-phase effects.

## Follow-up scope

The practical commandlet/CSV exporter described in the original plan is not required for this certified pass and remains optional follow-up tooling. Any future numeric change must first update the authored-table test, run a cold build, then repeat the final-candidate and full-matrix automation runs.
