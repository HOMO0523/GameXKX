# Boss Special Mechanics Audit — 2026-07-24

## Scope

Audit the already-present three-chapter boss mechanics before treating the route-balance certification as player-facing complete. This audit covers the implementation currently loaded by the UE 5.8 editor, not a design-only review.

## Verified implementation boundaries

- Black Bear (`Enemy.Ch2.BlackBear`) enters phase two once at or below half health, keeps the transition across save/load and healing, changes its declared attack/defense values once, and gives only `Pounce` and `Rend` their extra hit.
- Tiger (`Enemy.Ch3.Tiger`) retains its stored Prey in phase two, keeps `TigerPounce` locked to that target, retargets only after the stored target dies, and changes only phase-two `TigerPounce` to 150% damage and two hits.
- Boss phase transitions are evaluated after a complete player card packet and after end-round damage-over-time, before forecasting the next enemy intent.

## Live editor evidence

UE MCP smoke succeeded against the running editor on 2026-07-24. Focused automation runs all completed successfully:

| Filter | Result | Cases |
| --- | --- | --- |
| `GameXXK.Battle.EnemyIntentRules.BlackBear` | Success | `PhaseStatsApplyOnce`, `PhasePounceAndRendGainOneHit` |
| `GameXXK.Battle.EnemyIntentRules.Tiger` | Success | `PreyLocksPhasePounceAndRetargetsAfterDeath`, `PreyExpiresAfterPhaseOnePounce`, `MarkPreyRetargetsStaleForecast`, `PredatorHealsAfterDamagingBleedingTarget` |
| `GameXXK.Battle.EnemyIntentRules.BossPhase` | Success | `CardPacketCrossesHalf`, `EndRoundDotBeforeForecast`, `SaveReloadPersistsAfterHealing` |

## Source boundaries reviewed

- `Source/GameXXK/Private/GameXXKCardRules.cpp`: phase transition, state persistence, target resolution, and effect application.
- `Source/GameXXK/Private/GameXXKEnemyCatalog.cpp`: Black Bear and Tiger intent declarations, phase multipliers, and allowed phase-only intents.
- `Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp`: executable rule coverage listed above.

## Follow-up

This only certifies combat-rule behavior. Route-node interaction, player-facing intent/tooltips, balance, and art integration remain separate acceptance work.
