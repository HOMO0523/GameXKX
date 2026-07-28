# Equipment Foundation Task 6 Save-Migration Preflight

Date: 2026-07-22 (Asia/Shanghai)

Status: **READ-ONLY PREFLIGHT — implementation must not start until the decisions in this record are resolved and Task 5 has stabilized.**

Scope: Task 6 in `docs/superpowers/plans/2026-07-22-meta-equipment-foundation.md`, lines 547–613, checked against `docs/superpowers/specs/2026-07-22-meta-equipment-partner-three-chapter-route-design.md` and the current source tree on `main`.

This record was produced without modifying source code, opening or closing Unreal Editor, running build/automation commands, or staging/committing files. It is a preflight and test-design record, not implementation or passing-test evidence.

## Outcome

The current save path can be migrated to version 7, but Task 6 is not safe to start as a blind implementation of the written file list. The production load path, menu preview path, legacy direct-restore facade, `UGameXXKSaveGame` construction defaults, modern equipment mirror ownership, and disk-failure reporting need one coherent design.

Task 5 dependencies were still changing during this audit. `Source/GameXXK/Private/GameXXKEquipmentEconomyRules.cpp` changed from compile-only failure stubs to a full implementation while the preflight was in progress. Its last observed SHA-256 prefix was `77EDA16CDA5B`. **After Task 5 stabilizes, Task 6 must re-read `GameXXKEquipmentEconomyRules.h/.cpp`, its tests, and all touched Task 6 files before making any edit.** This hash is only an observation, not a baseline to restore.

## Authoritative requirements

The specification makes progress preservation the primary migration rule:

- preserve hero level, gold, companions, stars, cards, route state, route resources, codex state, quest NPC state, and active battle state;
- convert old weapon/armor/accessory ownership into queryable legacy equipment instances with exact old base effects and enhancement;
- map the old three hero slots to Weapon, Armor, and Accessory while leaving Head, Belt, and Shoes empty;
- keep an in-progress old route loadable and unchanged as the current first-chapter route state;
- use safe defaults for new permanent-equipment fields;
- back up the original save before migration, write the upgraded main slot only after successful migration and validation, and retain/restore the original on failure.

## Current version chain

The current global save version is `6`, defined by `GameXXKMVP::CurrentSaveVersion` in `Source/GameXXK/Private/GameXXKMVPRules.cpp`.

`UGameXXKMVPRules::MakeSaveState` always writes version 6. `UGameXXKMVPRules::RestoreFromSaveState` currently implements the complete compatibility chain:

| Source version | Current behavior |
|---|---|
| `0`, `1`, or another value below `2` | Starts from `CreateNewGame()`, overlays the old top-level facade fields, clears Inventory/enhancement/three hero equipment mirrors, then runs inventory/codex/progression normalization. |
| `2` | Copies `RuntimeState`, forces ten enhancement materials, clears unsupported `ItemEnhancementLevels`, restores the top-level player-location mirror, then runs inventory/codex/progression normalization. |
| `3` or `4` | Copies `RuntimeState`, restores the player-location mirror, migrates inventory categories, infers Guide discovery for an accepted/completed quest, migrates legacy enemy codex IDs, and normalizes progression. |
| `5` | Same broad `>=3` path, but does not infer Guide discovery; legacy enemy codex IDs are still migrated because the enemy migration threshold is 6. |
| `6` | Same broad `>=3` path; inventory-category synchronization and progression normalization run, while both codex thresholds are no-ops. |
| `>6` | Currently accepted by the same `>=3` branch. This is incorrect for Task 6, which requires rejecting versions greater than 7. |

The codex implementation is threshold-based, not a set of discrete v3/v4/v5/v6 functions:

- `SaveVersion < 5`: discover `Codex.Guide` when the quest is accepted or completed;
- `SaveVersion < 6`: map Bandit/Wolf to Money Rat, Elite Bandit to Black Bear, and Boss to Tiger.

## Real restore and load entry points

The disk-authoritative production entry is:

- `UGameXXKMVPSubsystem::LoadGameFromSlot` in `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`.

It currently loads a `UGameXXKSaveGame`, immediately clears the development HUD fixture through `BeginRuntimeStateMutation`, assigns `RuntimeState = UGameXXKMVPRules::RestoreFromSaveState(...)`, and returns true. It has no backup, typed migration result, full-state validation, upgraded main-slot write, rollback, or player-facing migration error.

There is also a production bypass:

- `UGameXXKMainMenuWidget::BuildSaveSlotRow` in `Source/GameXXK/Private/UI/GameXXKMainMenuWidget.cpp` loads the save object directly and calls `UGameXXKMVPRules::RestoreFromSaveState` to render the slot preview.

Tests and other compatibility callers also invoke `RestoreFromSaveState` directly. Therefore, adding the new dispatcher only to `LoadGameFromSlot` does not make it the canonical migration entry. The compatibility facade and menu preview behavior must be explicitly decided.

`UGameXXKSaveGame::UGameXXKSaveGame` currently initializes `SaveState` by calling `MakeSaveState(CreateNewGame())`. Once Task 6 makes `CreateNewGame()` contain three starter equipment instances, loading an older object that lacks new serialized fields can inherit v7 constructor defaults before old fields are overlaid. The constructor must use a neutral serialization default; the actual new-game path belongs in the subsystem/rules new-game flow.

## Exact Task 6 modification surface

### Steps 1–2: RED fixtures

- Create `Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp` and register tests below `GameXXK.Equipment.SaveMigration`.
- Modify `FGameXXKSaveGameSlotRoundTripTest::RunTest` in `GameXXKSaveGameTest.cpp` for version assertions, backup lifecycle, rollback, and v7 round trips.
- Modify `FGameXXKInventoryEnhancementTest::RunTest` in `GameXXKInventoryEnhancementTest.cpp` for version 7 and instance-backed legacy compatibility.
- Modify `FGameXXKCompanionCodexPersistenceTest::RunTest` in `GameXXKCompanionCodexPersistenceTest.cpp`, retaining the existing v4/v5 expectations while updating the current-version assertion.

### Step 3: typed dispatcher

- Create `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`.
- Define `FGameXXKSaveMigrationReport` with `bSucceeded`, `SourceVersion`, `TargetVersion`, `bCreatedLegacyOverflow`, `Warnings`, and `Error`.
- Define `FGameXXKSaveMigration::MigrateToCurrent(const FGameXXKSaveState&, FGameXXKSaveState&, FGameXXKSaveMigrationReport&)`.
- Create `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp` for the implementation.
- Reset both outputs on every call; do not expose partially migrated state after failure.

### Step 4: preserve the old chain

Move or faithfully reuse these current private helpers from `GameXXKMVPRules.cpp` behind the dispatcher:

- `MigrateLegacyCodexEntryIds`;
- `MigrateInventoryCategoryItems`;
- `MigrateCodexState`;
- `NormalizeLoadedCharacterProgression`;
- all three branches currently embedded in `UGameXXKMVPRules::RestoreFromSaveState`.

`UGameXXKMVPRules::RestoreFromSaveState` should become a compatibility delegate rather than a second migration implementation. Its failure behavior must be decided before implementation because its current return type cannot carry the migration report.

### Steps 5–6: deterministic 6-to-7 conversion and overflow

Implement in `FGameXXKSaveMigration::MigrateToCurrent`/private migration helpers:

- identify old equipment through `FGameXXKEquipmentCatalog::FindDefinition`;
- sort old equipment Inventory keys lexically, independent of `TMap` iteration order;
- create copies in ascending per-definition copy order;
- consume one matching Inventory quantity for each valid hero mirror;
- synthesize exactly one instance when a valid hero mirror exists at saved quantity zero;
- treat every companion `EquippedItemIds` entry as separately owned, without consuming Inventory quantities;
- for a companion's duplicate same-slot entries, equip the first, put later copies in the warehouse, and add a warning;
- preserve roster array order and each companion's old item order;
- set Common quality, item level 1, no affixes, `LegacyFlatPerEnhancement`, the exact catalog base snapshot, and the old per-definition enhancement level;
- assign deterministic instance IDs and deterministic acquisition seeds while advancing `NextInstanceOrdinal` exactly once per created copy;
- rebuild warehouse order without deleting over-cap copies;
- set `bLegacyWarehouseOverflow=true` only when the converted warehouse exceeds 200;
- validate with `FGameXXKEquipmentRules::ValidateCollectionAgainstRoster` before success;
- preserve all non-equipment state byte-for-byte at the field/order level.

`Source/GameXXK/Public/GameXXKCompanionTypes.h` must retain `FGameXXKPermanentCompanion::EquippedItemIds` under the same serialized property name. Task 6 should only mark/document it as a deprecated pre-v7 migration source.

### Step 7: legacy mirrors and adapters

The following `UGameXXKMVPRules` methods must stop treating equipment Inventory counts as authority:

- `GetItemCount`;
- equipment branch of `BuyItem`;
- `GetItemEnhancementLevel` and `CanEnhanceItem`;
- `EnhanceItem`;
- `CanDecomposeItem` and `DecomposeItem`;
- `EquipItem` and `UnequipItem`.

The current Task 5 implementation already contains a private anonymous-namespace `SynchronizeLegacyEquipmentMirrors` in `GameXXKEquipmentEconomyRules.cpp`. It removes catalog equipment keys from Inventory, rebuilds counts for legacy instances, derives the hero's three old equipped-item mirrors, and rebuilds equipped enhancement mirrors. Task 6 must choose one authoritative implementation and make the economy and MVP facades delegate to it. Duplicating this helper in `GameXXKMVPRules.cpp` would create drift.

The plan also needs an explicit decision for `SellItem`, `CanSellItem`, `AddItem`, and `RemoveItem`: these public APIs can otherwise continue mutating equipment presentation mirrors as if they were authoritative.

### Step 8: hero stat recalculation

- Replace `UGameXXKMVPRules::RecalculatePlayerStatsFromEquipment` with `FGameXXKCharacterStatRules::GetBareHeroStats` plus `FGameXXKEquipmentRules::BuildLoadoutSnapshot`.
- Preserve absolute missing HP and missing MP, not resource percentages.
- Audit the private `GameXXKMVP::RecalculatePlayerStats` call sites in legacy buy/sell/enhance/decompose/equip/new-game/restore paths so they cannot bypass the new implementation.
- The current Task 5 Economy cpp has a second private implementation named `RecalculateHeroMirrors`; it should delegate to the same authoritative recalculation path.

### Step 9: version and new game

- Set `GameXXKMVP::CurrentSaveVersion` to 7.
- Update `UGameXXKMVPRules::CreateNewGame` to initialize schema 1, a non-zero collection seed, ten authoritative stones, and three unequipped legacy instances in this warehouse order: Wooden Sword, Starter Cloth Armor, Cloth Talisman.
- Advance `NextInstanceOrdinal` consistently with the three starter instances.
- Derive the three legacy Inventory counts; do not store a second authoritative equipment quantity.
- Change `UGameXXKSaveGame::UGameXXKSaveGame` to a neutral default so old deserialization cannot inherit v7 starter equipment.
- `GameXXKSaveGame.h` needs modification only if a factory/initialization or test contract is added; the current plan lists it without specifying a required header-level API.

### Step 10: failure-safe disk migration

Rewrite `UGameXXKMVPSubsystem::LoadGameFromSlot` to:

1. load the original save object without changing `RuntimeState` or clearing the HUD fixture;
2. for a pre-v7 source, create or validate `<ResolvedSlotName>.PreV7Backup` using the same user index, without overwriting an existing successful backup;
3. migrate an in-memory copy and validate the resulting runtime state;
4. save a new `UGameXXKSaveGame` containing the migrated v7 state to the main slot;
5. only after the upgraded write succeeds, call `BeginRuntimeStateMutation` and assign live `RuntimeState`;
6. on backup, migration, validation, or upgraded-write failure, leave live state unchanged and restore the original main slot from the backup if a main write was attempted.

The subsystem header needs a defined error contract or test seam. No current property/getter can surface `存档迁移失败，已保留原存档。`, and the main-menu load flow only consumes a bool.

### Steps 11–14

- Step 11 updates the three adjacent regression files while retaining v2/v4/v5 behavior and cleaning both main and backup test slots before and after each real-slot case.
- Steps 12–13 are cold-build and four-prefix MCP verification, not source edits.
- Step 14 is the final selective stage/commit operation and must occur only after green verification; it is outside this preflight.

## Dirty-worktree preservation risk

At the audit snapshot, these Task 6 targets were already modified or untracked:

- modified: `GameXXKMVPRules.h/.cpp`;
- modified: `GameXXKMVPSubsystem.h/.cpp`;
- modified: `GameXXKCompanionTypes.h`;
- modified: `GameXXKInventoryEnhancementTest.cpp`;
- untracked: `GameXXKCompanionCodexPersistenceTest.cpp`.

These Task 6 targets were clean:

- `GameXXKSaveGame.h/.cpp`;
- `GameXXKSaveGameTest.cpp`.

The new migration header, cpp, and test did not yet exist.

Preservation rules:

- `GameXXKMVPRules.cpp` already contains a large inventory/codex/route/character-stat change set. Task 6 touches its most conflict-prone functions and must use narrow hunks.
- `GameXXKMVPSubsystem.cpp` already contains a large companion/card/HUD-fixture change set. Rewriting `LoadGameFromSlot` must preserve the HUD fixture invalidation behavior, but delay invalidation until the migrated write succeeds.
- The inventory test's version assertions, v2 fixture, category behavior, and real-slot test are already dirty; do not replace the test body wholesale.
- The codex test is wholly untracked user work. Never recreate, delete, checkout, or overwrite it.
- `GameXXKCompanionTypes.h` has unrelated seed, speed, and quest-NPC changes. Limit the Task 6 change to the legacy equipment property documentation/deprecation.
- All equipment type/catalog/rules/economy production files and focused tests were untracked at the audit snapshot. Task 6 depends on them and must not assume their current contents are committed or stable.

## Recommended RED matrix

### Source-version coverage

| Fixture | Minimum assertions |
|---|---|
| v0 | Top-level legacy quest/location/level/gold restoration; no inherited v7 starter collection. |
| v1 | Freeze the current fact that version 1 follows the old facade path, or explicitly reject it after a written decision. |
| v2 | Ten stones, unsupported enhancement reset, inventory-category migration, old codex behavior. |
| v3 | Enhancement preservation, progression normalization, exact old equipped stats. |
| v4 | Accepted/completed quest discovers Guide; legacy enemy codex IDs migrate. |
| v5 | Empty codex does not infer Guide; legacy enemy codex IDs still migrate. |
| v6 | Complete deterministic equipment conversion and preservation of all adjacent state. |
| v7 | Valid state passes through unchanged; a second migration produces identical IDs, indexes, ordinals, resources, roster, cards, codex, route, battle, and mirrors. |
| v8 | Dispatcher returns false with SourceVersion 8, TargetVersion 7, a non-empty error, and no partially usable output. |

### Minimal 6-to-7 conversion fixture

Build one valid v6 state with:

- equipment, consumable, task, and material Inventory entries inserted in deliberately non-lexical order;
- all three hero mirrors, including one valid equipped item whose saved quantity is zero;
- multiple copies of at least one equipped definition, proving one copy goes to the hero and the remainder enter the warehouse;
- different old enhancement levels per equipment definition;
- twelve valid permanent companions in a stable roster order;
- old equipped IDs on companions, including one companion with two different same-slot legacy definitions;
- one active companion and one saved pending full-roster replacement candidate;
- non-empty cards, codex discovery/read sets, route nodes/edges/resources, quest NPC state, relic/event state, and active battle state;
- sentinel gold, stars, experience, seeds, and ordinals.

Assert:

- Inventory definition keys are processed lexically and copies in ascending copy order;
- companion entries are separately owned rather than consuming Inventory copies;
- the first same-slot companion entry is equipped and later entries move to the warehouse with a warning;
- every instance has the exact catalog snapshot, Common quality, no affixes, and legacy scaling;
- only derived legacy mirrors remain in Inventory/`ItemEnhancementLevels`;
- no adjacent state changes.

Use separate minimal fixtures for:

- 201 unequipped legacy copies, which must all survive with `bLegacyWarehouseOverflow=true`;
- an unknown old equipment ID;
- a hero mirror whose definition does not match that mirror's slot;
- a duplicate/corrupt companion owner or otherwise invalid final roster;
- a version 7 collection with modern instances and an active pending reforge, proving true pass-through idempotency.

### New-game and facade matrix

- Three starter legacy instances, exact warehouse order, none equipped, schema 1, non-zero seed, ordinal 3.
- Ten authoritative `Item.EnhancementStone` and synchronized `EnhancementMaterial`.
- Legacy equipment Inventory counts equal matching legacy instance counts and are not separately mutable.
- Deterministic `EquipItem`, `EnhanceItem`, and `DecomposeItem` behavior when multiple instances share one BaseEquipmentId.
- Equipment `BuyItem` is atomic for its quantity and respects capacity/overflow.
- New acquisition and unequip fail while overflow is active; dismantling down to 200 clears the overflow flag.
- Modern equipment instances never appear in Inventory.
- Hero HP/MP preserve their prior absolute missing amounts after loadout recalculation.

### Real-slot transaction matrix

Use unique temporary main slots and the exact `.PreV7Backup` companion slot. Delete both before and after every case.

1. Successful v6 load: backup contains the unmodified v6 object, main becomes v7, live state commits last.
2. Existing successful backup: preserve a sentinel backup without overwriting it.
3. Backup-write failure: return false; main and live state remain unchanged.
4. Migration failure: return false; main, backup, and live state remain safe.
5. Final validation failure: same rollback guarantees.
6. Upgraded-main-write failure: restore original main from backup and keep live state unchanged.
7. Current v7 load: do not create a PreV7Backup and do not rewrite the main slot.

A narrow injectable save-slot I/O seam is preferable for the two write-failure tests. Invalid paths, permissions, and read-only files are platform-dependent and do not provide deterministic automation.

## Plan/code mismatches and unresolved decisions

The following ten items must be resolved before Task 6 implementation begins:

1. **Direct-restore failure contract.** `RestoreFromSaveState` returns only `FGameXXKRuntimeState`; it cannot report/reject migration failure safely. Decide whether to add a `TryRestore` API, change direct callers, or define an explicit compatibility fallback.
2. **Menu bypass.** `BuildSaveSlotRow` bypasses `LoadGameFromSlot`. Decide whether previews call a non-writing migration preview, show an unloadable/future-version state, or remain on a separately documented path. The current Task 6 file list omits the menu file.
3. **Neutral SaveGame construction.** Decide and document that `UGameXXKSaveGame` construction is serialization-neutral and that only `CreateNewGame`/`StartNewGame` creates starter content.
4. **Version 1 support.** The code accepts v1 but the plan's source-version fixtures omit it. Freeze support with a test or explicitly reject it.
5. **Helper ownership after Task 5.** Decide where `SynchronizeLegacyEquipmentMirrors` and hero mirror recalculation live. The observed Task 5 implementation keeps both private in Economy cpp, while Task 6 requires them from migration/MVP code and does not list EconomyRules as a modified file.
6. **Deterministic legacy-instance selection.** Define which same-BaseEquipmentId instance legacy `EquipItem`, `EnhanceItem`, and `DecomposeItem` select: equipped-first, warehouse-order-first, lowest ordinal, or another frozen rule.
7. **Unlisted mirror mutators.** Decide equipment behavior for `SellItem`, `CanSellItem`, `AddItem`, and `RemoveItem`; otherwise the supposedly read-only Inventory mirror remains writable authority.
8. **Player-visible error surface.** Decide the subsystem/UI contract that exposes `存档迁移失败，已保留原存档。`. Returning false or logging alone does not satisfy a player-visible “surface” requirement.
9. **Pure complete-state validation.** There is no single public pure runtime validator. `EnsureCardRunInitialized` mutates card data and can break old-chain preservation/idempotency. Decide which pure validators compose the migration gate or extend the relevant rules API and file list.
10. **Legacy/new-game and backup edge semantics.** Explicitly reset equipment collection in the v0 facade path so it cannot inherit v7 starters; define what qualifies as an “existing successful backup,” what happens if restoration itself fails, and whether v7 loads are strictly read-only.

## Task 6 start gate

Task 6 may begin only after all of the following are true:

- Task 5 is stable and its EconomyRules production/test files have been re-read from the latest workspace state;
- the ten decisions above are recorded in the plan or an approved follow-up;
- current dirty hunks in every overlapping Task 6 file have been re-inspected and preserved;
- RED fixtures exist for v0 through v8, including v1, deterministic 6-to-7 conversion, 201 overflow, idempotency, constructor contamination, and transactional disk failures;
- the chosen failure-injection seam can deterministically prove backup and upgraded-write rollback;
- no UE assets, levels, PaperZD data, character sprites, camera transforms, or HD2D values enter the Task 6 working set.

