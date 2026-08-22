# Desktop Training Wave Health Reset Design

Date: 2026-08-22  
Status: user-approved behavior, pending written-spec review  
Scope: pure-2D Travel strip on `/Game/GameXXK/Maps/L_DesktopTrainingHUD`

## Problem and evidence

After Travel runs through several waves, party health no longer matches the intended encounter semantics and an enemy health bar can appear stale at a later wave boundary.

The ProgressBar paint path is not the root cause. A live PIE probe showed the party visual fractions and UMG bar percentages changing together with zero mismatch, and a forced percentage change repainted. The data lifecycle has two relevant problems:

1. `AdvanceTravelRunner` copies `InOutRuntime.PartyUnits` into the next encounter, intentionally preserving each member's remaining HP. This contradicts the confirmed rule that every encounter begins with both sides at full health.
2. The presented enemy slot sometimes reads the flattened `EnemyHP` compatibility mirror while the other slots read the authoritative `Enemies` array. During repeated encounter and presentation-boundary changes, those two sources can describe different snapshots.

The current spawn approach also lasts only two one-second logical steps. The confirmed pacing is five logical seconds.

## Required behavior

1. Starting Travel and every transition between encounters enter `Walking` for exactly five logical seconds before combat begins.
2. During those five seconds, the current hero walkloop and seamless background motion continue. Enemy sprites and enemy health bars remain hidden. No new companion walk assets or layout changes are introduced.
3. Every encounter materialization resets each valid party unit to `HP = MaxHP` and creates every enemy with `HP = MaxHP`.
4. The previous wave's final target may finish its existing hit/death presentation. After that presentation finishes, the strip shows only the five-second Walking interval.
5. When the next wave appears, every visible party and enemy bar begins at 100 percent and then updates only from that encounter's own authoritative snapshot.
6. Individual unit identities, MaxHP, Attack, party selection, deterministic attack cursors, rewards, cooldowns, challenge behavior, inventory state, and save semantics remain unchanged.

## Data and presentation design

### Spawn delay

Introduce one named Travel rule constant for the five-second encounter spawn delay. `InitializeTravelRunner` assigns it to `WalkStepsRequired`; the existing one-second cadence remains the clock. The first four advances remain `Walking`, and the fifth reaches `Combat`.

### Party health reset

`InitializeTravelRunner` continues accepting the selected party templates, but normalizes every accepted unit to a positive MaxHP and then sets HP to MaxHP. At a wave boundary, the existing party identities and stats can be passed forward, but remaining HP is no longer carried across encounters.

### Enemy health source

The Travel visual runtime treats `Enemies[slot]` and the immutable combat-event `EnemiesBefore/EnemiesAfter` arrays as the authoritative source for enemy HP and MaxHP. The flattened `EnemyHP`/`EnemyMaxHP` fields remain compatibility mirrors for existing callers but do not drive a per-slot health bar.

The active slot interpolation uses the active event's matching slot before/after values. Non-active slots use the same event snapshot. When there is no active event, every slot reads `LatestRuntime.Enemies[slot]`. This prevents a next-wave compatibility mirror from being paired with a previous-wave identity.

### Hidden interval

While `Walking`, all enemy bars are collapsed and their presentation cache is normalized so the next visible encounter must apply its full-health value. Party bars remain hidden with the existing strip behavior; their cached values are refreshed from the newly full party snapshot before combat becomes visible.

## Tests

Add failing tests before production changes:

1. A Travel runner initialized for an encounter reports `WalkStepsRequired == 5`; four advances remain Walking and the fifth enters Combat.
2. Damaged party templates passed into the next encounter emerge with every `HP == MaxHP`.
3. A multi-encounter runner test damages the party and enemies, settles several waves, and verifies every new encounter starts with full party and enemy arrays.
4. A visual-runtime test deliberately makes the flattened enemy compatibility mirror stale and verifies every displayed enemy fraction still follows the authoritative per-slot array.
5. A workbench test verifies the enemy bars remain hidden during all five Walking seconds and reappear at 100 percent for the next encounter.

## Verification

- Run focused Training, TravelVisualRuntime, and DesktopTraining Workbench automation.
- Run a cold UBT build with Hot Reload and Live Coding disabled.
- Run the pure-2D PIE flow for multiple complete encounter cycles, sampling authoritative HP, visual fractions, UMG percentages, and layout build count.
- Capture pre-spawn Walking and post-spawn combat screenshots and send them through the required luna visual review. Acceptance requires five seconds without enemies/bars, then full bars on both sides with unchanged layout and assets.

## Rejected alternatives

- UI-only forced full bars: hides incorrect gameplay data and fails after the first damage event.
- A new serialized `InterWave` state: clearer in isolation but unnecessary for this correction because existing `Walking` already owns the desired walkloop, hidden-enemy, and timing behavior.
- Repeated manual invalidation of every ProgressBar: the live evidence shows paint invalidation is functioning and does not correct the stale encounter data source.
