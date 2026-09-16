# Party slot progression: 1-1 hero / 1-2 companion / 1-3 NPC

Status: approved party-slot/main-story-entry scope implemented and verified. Root main; no worktree. Opening equipment lesson remains a separate pending design choice.

## Authorized outcome

- A new save starts with the hero only and ordinary 1-1 remains available for Travel and replayable Challenge.
- An actual 1-1 Challenge victory opens 1-2 and eligibility for the companion-slot talent. An actual 1-2 victory opens 1-3 and the main story. Completing S00-02 (retrieve scroll / meet YueBai) then makes the NPC-slot talent eligible. YueBai is the first deployable NPC; record that first deployment with the task objectives and retain it across removal/reload. Each slot talent costs 200 gold once; deployment remains a separate choice. Initial synthetic 1-1-clear eligibility must not unlock 1-2 early.
- Once unlocked, both slots remain optional: hero, hero+companion, hero+NPC, and all three are legal. Removing a deployed member preserves ownership, gear and personal cards.
- Existing card shares remain 8/5/3; only deployed members participate in Travel, battle, effects and experience.
- Challenge pauses Travel. Formation is edited outside a run, frozen during that run.
- Pre-change saves retain their existing progress, unlocked functionality and selected members. Modern empty slots must survive save normalization without automatic refill.
- No unapproved new cards, catch-up levels, tutorial gifts or tool-unlock gates in this scope.

## Milestones

1. Add focused regression tests for new defaults, actual-clear unlock order, all four party/deck shapes, and optional-slot save restoration; run RED on current implementation.
2. Add persisted progression state and versioned legacy migration; implement central formation validation and compatibility projection with optional slots.
3. Update initialization, Travel snapshots/rewards, CardBattle materialization, party APIs and formation UI. Keep fixed maximum layout capacity distinct from active member count.
4. Cold UBT; run focused and affected regression groups. Adjust obsolete fixed-three fixtures explicitly, preserving tests that exercise genuine invalid states.
5. Run isolated canonical 2D PIE/MCP behavior and visible formation checks; preserve player saves and tuned assets. Update acceptance and document remaining unrelated failures.

## Known affected components

- GameXXKPartyFormationRules / Types, GameXXKTrainingRules and TrainingProgress.
- MVPSubsystem initialization, party operations, Travel party construction/experience and persistence.
- CardBattleAdapter party/equipment/deck construction.
- SaveMigration dispatcher and current version.
- DesktopTrainingWorkbench formation controls and read model.

## Verification

Use UBT GameXXKEditor Win64 Development with NoHotReload/NoHotReloadFromIDE. Use project Automation report parsing and UE 5.8 MCP; no Live Coding/UnrealBridge. Compile requires no active editor or a prior successful MCP save and safe close. UI verification uses L_DesktopTrainingHUD in an isolated UserDir.

New regression prefix: GameXXK.PartySlots. Evidence directory: Saved/PartySlotProgression-20260915.

Latest guide scope: add 1-3 story opening, first-task start, reward claim, and scroll-task start as separate one/two-action courses. Keep authored plot, all 61 illustrations, dialogue and reward amounts.

Latest UI correction: locked slots display only the approved lock icon; purchasing a slot talent immediately offers a one-action formation navigation guide, followed by the two-action choose/deploy guide. Test the automatic handoff, not just manually opening the next lesson.

Final evidence: Saved/PartySlotProgression-20260915/final-acceptance/index.json (68/68,0 errors,2 existing missing-atlas warnings); navigation-visibility-build.log (cold UBT passed). Production record: docs/production/2026-09-15-party-slot-onboarding.md. No Shipping rebuild or commit.
