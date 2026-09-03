# Overall In-Run Optimization Plan Suite

> **Execution:** Use superpowers:executing-plans in the current task, one document at a time. The user explicitly prohibits sub-agents. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver the approved 173-card, combat-scaling, monster-phase, 27-stage, equipment, settlement, and balance-certification redesign as independently testable increments.

**Architecture:** The program is split into seven dependency-ordered plans so each merge point leaves a valid, testable game. Shared integer scaling and save schema land first; data-heavy cards and enemies consume those primitives; UI, progression, settlement, and certification follow without duplicating combat authority.

**Tech Stack:** Unreal Engine 5.8 C++, USTRUCT SaveGame state, UMG/Slate, UE Automation Framework, project Python report tooling, UE MCP, cold UBT via `scripts/ue_tdd_pipeline.py`.

---

## Source of truth

- Approved design: `docs/superpowers/specs/2026-09-03-card-monster-progression-rebalance-design.md`
- Working directory: `D:\UE5 demo\GameXXK`
- Authorized branch: `codex/overall-in-run-optimization`
- Canonical runtime surface: `/Game/GameXXK/Maps/L_DesktopTrainingHUD`

**Energy cost checkpoint (confirmed):** `docs/superpowers/specs/2026-09-03-sorcerer-energy-cost-design.md` governs all 18 partner and 4 Hero Mage fees. Partner 周天/照见 stay 0, the other sixteen cost 1; Hero 炎序/寒序/雷序/归序 cost 1/1/1/0. Mana and explicit effects remain intact, including HanXu Armor 40%. Partner sequences require 3-5 Energy before refunds; 4/5-Energy damage fixtures must state that budget while real simulation cannot invent it. Plan 2 Tasks 3/6/8 and Plan 7 include payment/replay gates. This settles the fee proposal, while overall runtime remains paused and 雷走 Attack numbers stay under separate review.

**Current review checkpoint:** The user has confirmed the 10% current-Mana recovery version for all four partner Ice cards. Design sections 4.3.1/6.3 and Plan 2 Tasks 6/8 now use upward-rounded 10% recovery, level/quality-scaled overflow Armor, exact mirror doubling, and a failed-search copy of the Armor just generated. Standard Ice is 100% times starter quality plus one attack-percentage point per consumed Armor. Mirror and 六合 Ice rewards grant each living ally floor(consumed Armor/4). 六合 separately recovers fixed 8/16 caster Mana and shares only this recovery's overflow Armor, after any active cost, exactly once per living recipient. Current expected figures are in `docs/design/2026-09-03-ice-mana-armor-confirmed-step-tables.md`; earlier 20%/25% figures and Defense-derived Ice/六合 versions are historical. Numeric design approval does not complete Plan 2 or certify a compile, PIE run, or win-rate target. Runtime implementation remains paused for the remaining overall design review, with existing uncommitted code preserved.

**Lightning direction checkpoint:** The user has selected single-hit 雷走 with normal one-Mark use and remaining Marks retained. `docs/superpowers/specs/2026-09-03-lightning-single-hit-design.md` separates that approved identity from the provisional 120/180/220/240 numeric package and positions 3-4 window. The proposed 引雷 Vulnerability reward remains separate and unapproved. Plan 2 Task 6 and Plan 7 now record the one-hit, per-target Mark, replay-quality, 斗转 remaining-Mark and phase-clear boundaries. The 44-route projection is design evidence only; no runtime restart or numeric freeze follows from this direction approval.

The dirty worktree contains user-owned deletions and many untracked assets/scripts. Every task stages only its named paths, inspects `git diff --cached --name-status`, and never runs reset/checkout/clean against unrelated files.

Before Plan 1 implementation, add a newest-first entry to `docs/production/current-goal-acceptance.md` that points to this suite/spec, states “design and plans approved; runtime implementation in progress,” and preserves the historical entries below it. At each plan boundary, use a separate docs-only commit to update only that top entry with the last green commit/gates; do not mix the pointer into a gameplay task's staged paths and never rewrite an older acceptance claim. The final pointer names the certification commit and any explicitly unverified manual/package checks.

## Dependency order

1. `2026-09-03-combat-scaling-foundation-implementation.md`
   - integer quality/level/DOT/Armor math;
   - battle scaling snapshot;
   - Armor cap removal;
   - DOT reservoirs, Medicine helper, next-round Energy penalty;
   - save schema migration.
2. `2026-09-03-active-card-catalog-rebalance-implementation.md`
   - retire 25 route cards;
   - retain five Boss compatibility IDs;
   - declarative value policies;
   - all 173 player-card overrides, runtime-resolved card text, and exact audit.
3. `2026-09-03-task-formula-terrain-status-ui-implementation.md`
   - Hero/partner/NPC task counts and lifecycle;
   - Healer formulas and owner-scoped icons;
   - Formation unlocks and terrain trigger ownership;
   - unit state-badge projection.
4. `2026-09-03-enemy-phases-training-formations-implementation.md`
   - multi-phase state and migration;
   - 156 enemy intent cards / 351 difficulty cases;
   - ordered forecasts and reactions;
   - all 27 stages / 189 encounter definitions;
   - phase ink presentation.
5. `2026-09-03-equipment-gems-idle-progression-implementation.md`
   - level-135 equipment;
   - halved equipment contribution and gem curve;
   - stage-band chest/idle item levels;
   - level-100 equip compatibility.
6. `2026-09-03-training-boss-settlement-implementation.md`
   - unique pending Training settlement receipt;
   - atomic grant and idempotent reload;
   - functional settlement page and return path;
   - no Boss-card choice in single-map mode.
7. `2026-09-03-balance-telemetry-certification-implementation.md`
   - strategy puzzles and bounded lookahead;
   - per-card telemetry and review flags;
   - 351 enemy semantic cases, 189 formations, tuning/certification matrices;
   - Hell 3-3 45%-55% / 9-11-round gate.

Plans 2-6 must not start before Plan 1 is green and committed. Plan 7 starts only after Plans 2-6 are green and their save migrations compose successfully.

## Approved-spec coverage

| Design section | Owning plan | Completion evidence |
|---|---|---|
| 3 and 6: 173-card pool and semantic overrides | Plan 2 | exact ID/quality/policy tables, resolved text parity, generated catalog |
| 4: quality, level, Armor, DOT, healing, statuses | Plan 1, then Plans 2-3 | arithmetic boundaries, real transaction tests, owner/state UI tests |
| 5: equipment, gems, late benchmark | Plan 5 | ten-rank table, 135 validation, seven exact Treasure stat snapshots |
| 7: task/formula/phase/terrain presentation | Plan 3 plus Plan 4 phase ink | 3/4/5 task fixtures, owner formulas, terrain boundary, two-row badges |
| 8: single-map settlement | Plan 6 | idempotent receipt, reload recovery, functional page, 2D PIE flow |
| 9-13: difficulty, 21 enemies, phase decks, formations | Plan 4 | 351 resolved cases, 189 explicit formations, ordered opening forecasts |
| 14: implementation conflicts/migrations | Plans 1-6 | sequential save fixtures v32 through v38 and cold compile gates |
| 15-16: strategy, telemetry, acceptance/tuning | Plan 7 | nine puzzles, deterministic matrices/hashes, Hell 3-3 benchmark |

Design sections 17-18 remain decision records/deferred scope and are not silently converted into implementation tasks.

## Explicitly deferred design work

These recorded items do not enter this implementation suite until their own brainstorming/spec approval:

- in-run shop UI polish;
- event UI, event variety, and event-interest redesign;
- relic variety/strength redesign (certification holds relics fixed until approved);
- reward-card visual polish;
- enemy-intent visual polish beyond the functional final-number/phase contract;
- original route-map background and node art;
- settlement visual/UX polish beyond the functional page;
- packaged cross-machine DPI/white-edge remediation;
- 3D-town cook isolation and pure-2D tutorial redesign.

The 3D town may remain in the Unreal project but must not be introduced into the canonical main-flow acceptance for any plan in this suite.

## Common task protocol

At the start of every task:

```powershell
git branch --show-current
git status --short
git diff --cached --name-status
```

Expected branch: `codex/overall-in-run-optimization`. Expected cached diff: empty. Existing unrelated dirty paths may remain.

For C++ red/green cycles:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 240 --filter "[TDD]"
```

Do not use Live Coding or Hot Reload. If UE is open and a cold build is required, save dirty packages through UE MCP and close the editor safely first.

Before every commit:

```powershell
git diff --check
git diff --cached --name-status
git diff --cached --check
```

The cached file list must contain only paths named by that task.

## Program completion gate

The suite is complete only when:

- all seven plans are checked off and committed independently;
- cold UBT succeeds;
- focused and broad `GameXXK` Automation suites have zero failures/errors;
- save migration fixtures cover v32 through the final schema;
- 173 active CardIds, 351 monster intent/difficulty cases, and 189 formations validate exactly;
- card faces/tooltips/previews/logs agree on resolved Armor, DOT, Medicine, fixed-damage, phase, and intent numbers;
- the deterministic certification report and hash are committed as evidence;
- a real PIE flow on `L_DesktopTrainingHUD` reaches battle, resolves a multi-phase Boss, opens settlement, confirms once, and returns to the 2D workbench;
- no user-owned dirty asset is absorbed into a task commit.

Run `python scripts/harness_state_validator.py` after the final pointer update; all new production metadata must validate, while pre-existing classified findings remain reported rather than hidden.
