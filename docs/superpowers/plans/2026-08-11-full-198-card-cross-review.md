# GameXXK 198-Card Cross-Review Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Project rules require working directly on `main`; do not create a worktree. The user deferred staging and commits until the complete 198-card review, so do not stage or commit during this plan.

**Goal:** Prove that all 198 catalog cards, six permanent-partner pools, six NPC pools, route cards, shared statuses, target resolution, save paths, and redesigned equipment sets match the approved rules and execute without stranded cards, stale text, partial transactions, or cross-owner task leakage.

**Architecture:** Treat the live 198-card catalog as the implementation snapshot, the approved per-class tables/specifications as the target, and Automation reports as status evidence. Audit in independent partitions (`36 Hero + 108 permanent partners + 24 NPC + 30 route`) before running cross-partition interactions. Any observed defect follows strict RED → minimal production fix → exact GREEN → neighboring regression; tests are never weakened to accept a production mismatch.

**Tech Stack:** Unreal Engine 5.8 C++, UE Automation, cold UBT, `FGameXXKCardCatalog`, `FGameXXKCardRules`, `FGameXXKCompanionRules`, equipment-set runtime, save migration, deterministic Python balance harness, generated MD/TXT documentation.

---

## 0. Non-negotiable review gates

- [ ] Work on root `main`; preserve the dirty worktree and every unrelated user asset.
- [ ] Do not change confirmed UI hierarchy, slots, anchors, sizes, tabs, layouts, sprites, backgrounds, camera values, or manually tuned assets.
- [ ] Do not stage or commit until all review partitions and final regressions are complete and the user explicitly approves submission.
- [ ] Use only cold builds with `-NoHotReload -NoHotReloadFromIDE`.
- [ ] Every Automation invocation gets its own report directory and must discover at least one concrete test.
- [ ] A report passes only with `failed=0`, `notRun=0`, and every warning named and classified.
- [ ] Stable CardIds may remain for migration; runtime mechanics must dispatch through typed metadata, not display names.
- [ ] “198 definitions exist” is not evidence that 198 behaviors are correct.

## Task 1: Freeze the live manifest and global schema

**Files:**

- Verify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Verify: `Source/GameXXK/Private/GameXXKCardQualityRules.cpp`
- Verify: `Source/GameXXK/Private/GameXXKCardText.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCardCatalogTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCardQualityRulesTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCardDocumentationTest.cpp`
- Create after evidence is collected: `docs/production/2026-08-11-198-card-review-status.md`

- [x] **Step 1: Record the entry boundary**

Run:

```powershell
git branch --show-current
git status --short
git diff --name-only
```

Require branch `main`. Record pre-existing tracked/untracked paths; never clean or revert them.

- [x] **Step 2: Run one cold compile**

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload -NoHotReloadFromIDE
```

Require exit 0 and `Result: Succeeded`.

- [x] **Step 3: Run independent schema filters**

```text
GameXXK.Data.CardCatalog
GameXXK.Data.CardQuality
GameXXK.Data.CardDocumentation
GameXXK.Integration.CardText
```

Require exactly 198 unique definitions, `36/108/24/30` owner counts, 36 Hero visual/unlock entries, no invalid target/effect/status enum, no unresolved text fallback, and MD/TXT equality with the live generator.

- [x] **Step 4: Create the review ledger**

Create a Markdown table with one row for Hero, Blade, Guard, Healer, Hunter, Sorcerer, Formation, NPC, Route, shared statuses, equipment sets, text/targeting, birth/migration, and balance. Columns are `expected count`, `target source`, `catalog report`, `runtime report`, `warnings`, `open defects`, and `status`. Initialize only from observed reports; do not pre-mark later rows green.

## Task 2: Verify shared combat, targeting, queues, and status semantics

**Files:**

- Verify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Verify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Test: `Source/GameXXK/Private/Tests/GameXXKCombatStatusRedesignTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCardCombatRulesTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCardResolutionQueueTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKHeroCounterBlockRuntimeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKMarkRulesTest.cpp`

- [x] **Step 1: Run the shared-rule filters independently**

```text
GameXXK.Data.CombatStatusRedesign
GameXXK.Data.CardCombatRules
GameXXK.Data.MarkRules
GameXXK.Data.CardBattleRuntime
GameXXK.Data.HeroCards.Foundation
GameXXK.Data.HeroCards.CounterBlock
```

Require the confirmed rules: Mark `+15%` and consumes per hit; Momentum adds its stack count to each attack packet without normal consumption; Bleed triggers and loses one on being hit but does not decay at round end; Poison ticks and loses one at end phase; Burn triggers once per active card/enemy intent and loses one; toxic explosion resolves Bleed/Poison/Burn once each, excludes Rot, and only a preserving source may prevent decrement; simultaneous death resolves player victory.

- [x] **Step 2: Audit transaction and target invariants**

Use targeted `rg` and the reports to confirm group/self cards never require a clicked unit, optional enhancement targets never make the base illegal, insufficient resources change no zone/resource, automatic effects do not count active plays, and a choice pause preserves the queue cursor and runtime bytes.

- [x] **Step 3: Remediate any observed shared defect with TDD**

Add one smallest reproduction to the closest existing file above, run its exact leaf path to observe the intended failure, make one production change in `GameXXKCardRules.cpp`/`GameXXKCardTypes.h`, then rerun the leaf and every filter in Step 1. Do not combine unrelated status fixes.

## Task 3: Review all 36 Hero cards

**Files:**

- Test: `Source/GameXXK/Private/Tests/GameXXKHeroCardCatalogTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKHeroGenericCardRuntimeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKHeroBladeRuntimeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKHeroGuardRuntimeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKHeroHealerRuntimeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKHeroHunterRuntimeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKHeroMageRuntimeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKHeroFormationRuntimeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKHeroCardIntegrationTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKHeroCardUnlockMigrationTest.cpp`

- [x] **Step 1: Run every Hero partition**

```text
GameXXK.Data.HeroCards.Catalog
GameXXK.Data.HeroCards.Generic
GameXXK.Data.HeroCards.Blade
GameXXK.Data.HeroCards.Guard
GameXXK.Data.HeroCards.Healer
GameXXK.Data.HeroCards.Hunter
GameXXK.Data.HeroCards.Mage
GameXXK.Data.HeroCards.Formation
GameXXK.Data.HeroCards.Integration
GameXXK.MVP.SaveGame.HeroCardPoolV12
```

Require 12 generic + 4 cards for each of six linked roles; eight generic cards start unlocked and levels 5/10/15/20 unlock one each; all 24 role cards are selectable without partner-level locks; Hero loadout remains exactly eight.

- [x] **Step 2: Check exact mechanic boundaries**

Lock Jian Yi as `Attack × (260% + 20% × Momentum) + Momentum`, then consume all Momentum and refund at most one Energy at three stacks; Hero mage task requires exactly eight equipped Hero IDs; Hero Formation keeps fixed target modes; Hero Hunter locks and consumes Charge once; Counter and Block remain separate.

- [x] **Step 3: Fix only reproducible Hero mismatches with leaf tests and rerun all ten filters**

## Task 4: Review all 108 permanent-partner cards and birth rules

**Files:**

- Test: `Source/GameXXK/Private/Tests/GameXXKBladePartner*Test.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKGuardPartner*Test.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKHealerPartner*Test.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKHunterPartner*Test.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKSorcererPartner*Test.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKFormationMasterPartnerRuntimeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCompanionBirthArchetypeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCompanionBirthPoolMigrationTest.cpp`
- Verify: `Source/GameXXK/Private/GameXXKCompanionRules.cpp`

- [x] **Step 1: Run six independent profession prefixes**

```text
GameXXK.Data.PartnerCards.BladeCatalog
GameXXK.Data.PartnerCards.BladeRuntime
GameXXK.MVP.SaveGame.BladePartnerCardsV14
GameXXK.Data.PartnerCards.Guard
GameXXK.Data.PartnerCards.Healer
GameXXK.Data.PartnerCards.Hunter
GameXXK.Data.PartnerCards.Sorcerer
GameXXK.Data.PartnerCards.Formation
```

Each profession must contain exactly 18 stable IDs. Blade/Guard/Healer/Hunter/Sorcerer each have exactly two core cards; Formation has six switch cards and twelve benefit cards rather than two cores.

- [x] **Step 2: Run birth, configuration, and migration filters**

```text
GameXXK.Data.CompanionBirth
GameXXK.Data.CompanionBirthArchetypes
GameXXK.MVP.SaveGame.CompanionBirthPoolV13
GameXXK.UI.CompanionRoster.FinalEquipmentAndCardTabs
```

Require deterministic six-card birth, five-card configuration, no upgrade unlocks, ordinary `2 core + 3 main-archetype + 1 equal-probability free slot`, and Formation `2 switches + 3 main-benefit + 1 all-benefit free slot`. No test may change the approved 8/5/3 UI layout.

- [x] **Step 3: Compare all six exact catalog tables against their approved specifications**

Use CardId as the join key and compare name, quality, Energy, Mana, fixed target, core flag, archetype, base effects, Charge/Finish/HeavyArrow/Sorcerer metadata, and exhaust flag. A display-name match without metadata/runtime support fails review.

- [x] **Step 4: For any missing table cell, write a RED assertion in that profession's catalog/runtime file before changing production**

After each GREEN, rerun that entire profession prefix plus `CardCatalog`, `CardText`, `CompanionBirth`, and the relevant save migration.

## Task 5: Review all 24 NPC cards and every 4-select-3 composition

**Files:**

- Test: `Source/GameXXK/Private/Tests/GameXXKTaskNpcCatalogTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKTaskNpcAllCardsRuntimeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKTaskNpcBladeRuntimeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKTaskNpcSupportRuntimeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKTaskNpcHealerFormationRuntimeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKTaskNpcSpellTaskRuntimeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKQuestNpcCardSelectionTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKQuestNpcDefaultLoadoutTest.cpp`

- [x] **Step 1: Run catalog and all-card base resolution**

```text
GameXXK.Data.TaskNpcCards.Catalog
GameXXK.Data.TaskNpcCards.Runtime.All24Cards
GameXXK.Data.TaskNpcCards.Runtime.BladeTiming
GameXXK.Data.TaskNpcCards.Runtime.Support
GameXXK.Data.TaskNpcCards.Runtime.HealerFormation
GameXXK.Data.TaskNpcCards.Runtime.SpellTask
```

Require six NPC owners × four cards, all 24 preview and resolve, group/self targets do not prompt, Guard-derived reactions use Block, Blade-derived reactions use Counter, and spell NPCs complete only their own three-card task.

- [x] **Step 2: Run deterministic 4-select-3 and missing-card matrices**

For each NPC, execute all four omitted-card cases and preview/resolve every selected card in that exact carried-three context. A card that consumes Heavy Arrow, Medicine, DOT, or a named status must itself establish the prerequisite earlier in its own resolution chain; both spell NPC trios must complete their three-card task. The omitted fourth card must not appear in hand/draw/discard/exhaust, pending queues, search offers, task locks, completion history, or task snapshots.

- [x] **Step 3: Fix each NPC defect test-first and rerun all six filters plus shared queue/target tests**

## Task 6: Review all 30 route cards

**Files:**

- Test: `Source/GameXXK/Private/Tests/GameXXKRouteCardRecipeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKRouteCardCanonicalIntegrationTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKRouteCardEntriesSaveMigrationTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKRouteMerchantRulesTest.cpp`
- Modify only if coverage is absent: `Source/GameXXK/Private/Tests/GameXXKRouteCardRecipeTest.cpp`

- [x] **Step 1: Run route catalog, recipe, acquisition, merchant, and migration filters**

Use the exact paths declared in the four test files. Require exactly 30 route definitions, stable IDs, valid target modes, deterministic reward/acquisition entries, and save round trips.

- [x] **Step 2: Ensure every one of the 30 CardIds is asserted by the route recipe table**

If any CardId is absent, add it to the table first and observe the expected RED. The table must assert quality, costs, target, effects, terrain/status conditions, and exhaust/temporary behavior.

- [x] **Step 3: Add base-resolution cases for any route effect type not already executed, then rerun route and global catalog/text suites**

The registered global text path is `GameXXK.Integration.CardText` (there is no `GameXXK.Data.CardText`). The final route runtime gate executes all 30 CardIds, all seven terrains for each of the ten terrain cards, Plain for the other twenty, and the low-health rare branch: 91 successful resolutions total.

## Task 7: Review all six equipment sets against the confirmed 2/4/6 rules

**Files:**

- Verify: `Source/GameXXK/Private/GameXXKEquipmentSetCatalog.cpp`
- Verify: `Source/GameXXK/Private/GameXXKEquipmentRules.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKEquipmentSetCatalogTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKPoJunSetRuntimeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKQingNangSetRuntimeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKZhuiFengSetRuntimeTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKShiGuSetRuntimeTest.cpp`

- [x] **Step 1: Run set catalog and four redesigned runtime prefixes**

```text
GameXXK.Equipment.SetCatalog
GameXXK.Equipment.PoJunRuntime
GameXXK.Equipment.QingNang
GameXXK.Equipment.ZhuiFeng
GameXXK.Equipment.ShiGu
```

Require Blade-set Charge/Finish loops at 2/4/6, QingNang team-unique first `Energy>=2` thresholds, ZhuiFeng team-unique card-count thresholds, and ShiGu Rot/dual-DOT/toxic-explosion preservation rules.

- [ ] **Step 2: Prove XuanJia and ShanHe runtime behavior**

XuanJia must retain exactly 50% Armor at the normal decay boundary where the confirmed set tier applies; ShanHe must preserve its approved terrain modifiers. If either lacks a runtime leaf test, add one RED case to a focused new `GameXXKXuanJiaShanHeSetRuntimeTest.cpp` before touching production.

Audit note: neither set currently has a battle-runtime consumer. XuanJia's 50% retention tier and its other two tiers are not frozen in the approved design source; ShanHe's per-terrain payload and integer rounding are also unspecified. Do not invent those contracts or count descriptor-only tests as runtime evidence.

- [ ] **Step 3: Rerun set filters with Hero/partner Charge, Finish, DOT, terrain, and save regressions**

## Task 8: Add one all-source playability and composition gate

**Files:**

- Create: `Source/GameXXK/Private/Tests/GameXXKAllCardPlayabilityAuditTest.cpp`
- Reuse fixture patterns from: `Source/GameXXK/Private/Tests/GameXXKTaskNpcAllCardsRuntimeTest.cpp`
- Reuse target setup from: `Source/GameXXK/Private/Tests/GameXXKFormationMasterTargetingDiagnosticTest.cpp`

- [x] **Step 1: Write the RED coverage test**

The test must iterate all 198 definitions and maintain a `TSet<FName>` of executed CardIds. For each definition it constructs a legal owner, two allies, two enemies, maximum resources, representative statuses, Armor, Charge, and every terrain. It must:

```cpp
TestEqual(TEXT("all-card audit sees 198 definitions"), Definitions.Num(), 198);
TestTrue(TEXT("preview succeeds with a legal fixed target"), bPreviewed);
TestTrue(TEXT("active play resolves atomically"), bResolved);
TestFalse(TEXT("successful card leaves an impossible pending target"), Runtime.Deck.PendingChoice.Kind == EGameXXKCardChoiceKind::Invalid && !Runtime.Deck.PendingChoice.CandidateInstanceIds.IsEmpty());
ExecutedIds.Add(Definition.Id);
TestEqual(TEXT("all 198 stable IDs executed"), ExecutedIds.Num(), 198);
```

Cards with legitimate search/discard choices must submit the first legal candidate and continue the automatic queue. A condition miss must still resolve its base; a missing required selected target must fail before payment and leave serialized runtime bytes unchanged.

- [x] **Step 2: Run the new test and classify every failure**

Fixture insufficiency is fixed only in the fixture. A genuine preview/resolution mismatch gets a smaller profession/route RED leaf test before production changes; the all-card test is the final gate, not the sole regression.

- [x] **Step 3: Add exact 8+5+3 mixed-loadout scenarios**

Run at least one loadout for each permanent profession and each NPC, including task overlap, automatic replay, full hand, death fallback, and save/resume. Require no cross-owner search candidate, no double resource payment, and no automatic action counted as active.

## Task 9: Final cold verification, balance observation, and document synchronization

**Files:**

- Update via generator: `docs/design/2026-08-11-full-card-catalog.md`
- Update via generator: `docs/design/2026-08-11-full-card-catalog.txt`
- Update: `docs/design/2026-08-11-gamexxk-project-master-plan.md`
- Update: `docs/design/2026-08-11-gamexxk-project-master-plan.txt`
- Update: `docs/design/2026-08-11-gamexxk-project-plan/03-rosters-deckbuilding-and-archetypes.md`
- Update: `docs/design/2026-08-11-gamexxk-project-plan/04-card-catalog-and-numeric-index.md`
- Update: `docs/design/2026-08-11-gamexxk-project-plan/10-implementation-testing-and-change-log.md`
- Regenerate: `docs/design/2026-08-11-gamexxk-full-project-plan-all-in-one.md`
- Regenerate: `docs/design/2026-08-11-gamexxk-full-project-plan-all-in-one.txt`

- [x] **Step 1: Run a final cold UBT after all code/test changes**

- [x] **Step 2: Run every prefix from Tasks 1–8 in fresh, independent commandlets**

Parse every `index.json`; record discovered, success, warning-success, failed, not-run, errors, and warning messages in the review ledger.

- [x] **Step 3: Run the deterministic card-balance observation harness**

Run the existing 2,400-scenario schema-2 profile three times with the same seed/config. Require identical CSV hashes, `stranded=0`, and no impossible target/resource/queue audit. Treat win-rate output as diagnostic until every strategy cohort is orthogonal; do not call it final balance certification.

- [ ] **Step 4: Regenerate and verify documents**

Run `GameXXK.Data.CardDocumentation` once with `-GameXXKUpdateCardDocs`, once without it, then run:

```powershell
& 'scripts\build_gamexxk_full_project_docs.ps1'
```

Require repeated generation to be hash-stable; A MD/TXT identical; B master MD/TXT identical; exactly one target-final section, one current-snapshot section, and one implementation-status matrix. No stale old card row may appear under a current heading.

- [ ] **Step 5: Perform final static review**

Search production for display-name dispatch, unexplained direct CardIds, stale status terms, debug logs, skipped/disabled tests, and UI/layout modifications. Inspect `git diff --check`, the complete diff stat, and every changed production/test/document file in scope.

- [ ] **Step 6: Stop before staging**

Report exact changed files, test counts/report paths, warning classifications, balance hashes, unresolved product decisions, and whether all 198 rows are proven. Do not stage or commit until the user confirms the final review.
