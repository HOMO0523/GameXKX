# Three-Chapter Route, Enemy Catalog, NPC Loops, and Balance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the current single-map, single-enemy route into a deterministic three-chapter run with 21 data-driven enemies, complete enemy intents and phases, six closed-loop task-NPC kits, idempotent route settlement, approved versioned visuals, and a real-rules 100-seed balance system.

**Architecture:** Preserve the shipped seven-layer random map generator and run it three times from chapter-derived seeds. Put immutable enemy/intent data in a catalog, saved per-enemy phase and cooldown state in the card battle runtime, deterministic encounter construction in pure rules, and all route settlement behind a copy-validate-commit receipt. Extend the equipment plan's existing `FGameXXKCombatSimulationRules` so both PIE and headless balance use `FGameXXKCardBattleAdapter`/`GameXXKCardRules` rather than parallel damage formulas.

**Tech Stack:** Unreal Engine 5.8, C++20/UE reflection, Paper2D, UMG/Slate, UE Automation, UE 5.8 MCP, Python 3, JSON/CSV reports, Pillow, built-in `image_gen` through the `imagegen` skill, project `apply_patch`, UBT cold builds only.

**Authoritative specification:** [`docs/superpowers/specs/2026-07-22-meta-equipment-partner-three-chapter-route-design.md`](../specs/2026-07-22-meta-equipment-partner-three-chapter-route-design.md)

---

## Dependency order and implementation guardrails

- Execute [`2026-07-22-meta-equipment-foundation.md`](2026-07-22-meta-equipment-foundation.md) and [`2026-07-22-meta-shop-partner-character-ui.md`](2026-07-22-meta-shop-partner-character-ui.md) first. Rebase [`2026-07-22-route-merchant-card-quality-upgrade.md`](2026-07-22-route-merchant-card-quality-upgrade.md) onto their resulting schema before this plan, because this plan consumes route-card entries, card qualities, `RouteTravelMoney`, equipment projections, and the combat-simulation foundation.
- Global save versions are sequential. At execution time, read the integer currently assigned to `GameXXKMVP::CurrentSaveVersion` after all dependencies, increment it exactly once, and store that resulting value as `ThreeChapterRouteIntroducedSaveVersion`. Do not assign a second meaning to version 7 or pre-reserve a literal before dependency integration.
- Work in `D:\UE5 demo\GameXXK` on `main`; do not create a worktree and do not use UnrealBridge.
- The worktree already contains user-owned modifications. Before every task run `git status --short` and `git diff -- <paths from that task>`. Never reset, restore, replace, or bulk-format unrelated hunks.
- Do not replace user-tuned sprites, PaperZD/Paper2D assets, placed levels, camera transforms, HD2D plane values, PSD cuts, or existing Money Rat/Black Bear/Tiger packages.
- If the editor is running, save dirty packages through UE MCP before closing it. If MCP cannot save, stop; do not force-close. Live Coding, Hot Reload, and `--check-only` are not verification.
- Every gameplay transaction uses validate-copy-commit. A failed chapter transition, intent, settlement, card acquisition, or save migration leaves the complete input state unchanged.
- Keep the established 18-card opening contract: hero 8 + permanent partner 5 + task NPC 3 + travel base 2. This later-approved battle contract recorded in this plan takes precedence over the shorthand “fixed 12-card deck” in authoritative specification §8. For a task NPC, `12` means only a display/collection personal pool made from four unique existing IDs repeated three times; only its locked three-card default loadout enters the shared battle deck, so battle initialization is always `8 + 5 + 3 + 2 = 18`. Never append all 12 personal-pool entries to the shared route deck.
- Keep the global card definition count at 174. Rewrite the existing 24 task-NPC definitions in place; do not add or remove card IDs.
- The approved visual inventory is exactly 30 identities plus seven status glyphs: 18 new normal/elite monsters, six profession-shared permanent-partner identities, six identity-locked task NPCs, and seven appended statuses. Reuse the three existing Boss assets unless an explicitly approved V1 gap-fill is required.
- The simulator may produce tuning recommendations and before/after reports. It must never modify `GameXXKEnemyCatalog.cpp`, card catalog data, equipment data, or any approved asset automatically.

## File map

### New gameplay files

- `Source/GameXXK/Public/GameXXKEnemyTypes.h` — reflected enemy tier, target/effect, passive/phase, resolved-intent, and saved enemy runtime types.
- `Source/GameXXK/Public/GameXXKEnemyCatalog.h`
- `Source/GameXXK/Private/GameXXKEnemyCatalog.cpp` — the exact 21-enemy catalog, chapter pools, stats, intent lists, passives, phases, Codex metadata, and versioned visual soft paths.
- `Source/GameXXK/Public/GameXXKEncounterRules.h`
- `Source/GameXXK/Private/GameXXKEncounterRules.cpp` — chapter seed derivation, no-replacement formation sampling, combat levels, stable unit IDs, and explicit 1P/2P/3P assignment.
- `Source/GameXXK/Public/GameXXKEnemyIntentRules.h`
- `Source/GameXXK/Private/GameXXKEnemyIntentRules.cpp` — atomic forecast, target locking, charge, effect execution, passives, cooldowns, and 50-percent phase transitions.
- `Source/GameXXK/Public/GameXXKEnemyText.h`
- `Source/GameXXK/Private/GameXXKEnemyText.cpp` — one shared card-body/Tooltip/status/passive/phase text formatter used by UMG and tests.
- `Source/GameXXK/Public/GameXXKCharacterVisualCatalog.h`
- `Source/GameXXK/Private/GameXXKCharacterVisualCatalog.cpp` — versioned semantic-key mapping for six profession-shared partner identities and six identity-locked task NPCs across sheet, battle Idle, portrait, card art, and PaperZD data.
- `Source/GameXXK/Public/GameXXKRouteSettlementRules.h`
- `Source/GameXXK/Private/GameXXKRouteSettlementRules.cpp` — unique receipt generation, successful/failed/abandoned conversion, idempotent permanent award, and route-local clear.
- `Source/GameXXK/Public/GameXXKRouteBalanceCommandlet.h`
- `Source/GameXXK/Private/GameXXKRouteBalanceCommandlet.cpp` — no-render sharded matrix entry point and JSON/CSV output.

### Existing gameplay files to modify

- `Source/GameXXK/Public/GameXXKCardTypes.h` — append statuses and persist explicit enemy definition/slot/state without renumbering existing enum values.
- `Source/GameXXK/Public/GameXXKCardRunTypes.h` — store resolved multi-effect enemy intents, chapter progress, acquisition count, and pending settlement.
- `Source/GameXXK/Public/GameXXKMVPRules.h`
- `Source/GameXXK/Private/GameXXKMVPRules.cpp` — route entry snapshot, encounter creation, chapter transition, settlement calls, Codex delegation, migration, and route cleanup.
- `Source/GameXXK/Public/GameXXKCardRules.h`
- `Source/GameXXK/Private/GameXXKCardRules.cpp` — expose the minimal atomic heal/status/modifier helpers needed by enemy rules and apply the appended status caps.
- `Source/GameXXK/Public/GameXXKCardBattleAdapter.h`
- `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp` — project explicit slots/definitions, build and resolve catalog intents, preserve 18 opening cards, use `RouteCombatLevel` for task NPCs, and notify passives/phases after card plays.
- `Source/GameXXK/Private/GameXXKBattlePresentation.cpp` — prefer saved explicit enemy slots, retaining legacy `StableSortOrder + 1` only as migration fallback.
- `Source/GameXXK/Private/GameXXKCardCatalog.cpp` — rewrite the 24 existing task-NPC cards without changing IDs or total count.
- `Source/GameXXK/Public/GameXXKCompanionTypes.h`
- `Source/GameXXK/Private/GameXXKCompanionCatalog.cpp`
- `Source/GameXXK/Private/GameXXKCompanionRules.cpp` — add the 12-entry personal-pool view and lock exactly three default battle cards.
- `Source/GameXXK/Private/GameXXKCardText.cpp`
- `Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp`
- `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp`
- `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp` — render all saved intent effects, charge, attacker slot, passive, and phase details.
- `Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp`
- `Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp` — resolve approved V1 character/card/Codex art through semantic keys instead of legacy hardcoded PartyDeck paths.
- `Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp` — resolve enemy flipbooks from the catalog rather than adding 21 branches.
- `Source/GameXXK/Public/GameXXKCombatSimulationTypes.h`
- `Source/GameXXK/Public/GameXXKCombatSimulationRules.h`
- `Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp` — skilled policy, route progression, contributions, and status-loop metrics.
- `Source/GameXXK/Private/GameXXKEquipmentCatalog.cpp`
- `Source/GameXXK/Private/GameXXKAffixCatalog.cpp`
- `Source/GameXXK/Private/GameXXKEquipmentSetCatalog.cpp` — only manually reviewed scalar tuning derived from paired-seed reports; never commandlet-written.
- `Source/GameXXK/GameXXK.Build.cs` — add private `Json` and `JsonUtilities` dependencies for balance reports.

### New C++ automation tests

- `Source/GameXXK/Private/Tests/GameXXKEnemyCatalogTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKEncounterFormationTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKThreeChapterRouteTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKRouteSettlementTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKEnemyMechanicsTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKQuestNpcLoopTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKRouteBalanceSimulationTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKRouteEnemyVisualMappingTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKCharacterVisualMappingTest.cpp`

### Existing tests to extend

- `Source/GameXXK/Private/Tests/GameXXKBattleEncounterRulesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKRouteMapSeedRulesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardEnemyIntentPresentationTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKQuestNpcDefaultLoadoutTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKQingshanTaskNpcRouteTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKCardRouteLifecycleTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKCompanionCodexPersistenceTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKPartyDeckBattleSceneUnitActorTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKTaskNpcCodexWidgetTest.cpp`

### New balance and PIE scripts

- `SourceAssets/Balance/route-balance-matrix-v1.json` — versioned levels, gear fixtures, formations, 100 fixed seeds, threshold cohorts, and report schema.
- `scripts/run_route_balance_matrix.py` — shard orchestration, commandlet invocation, schema validation, and aggregate report.
- `scripts/test_route_balance_matrix.py` — command construction, shard partition, merge, threshold, and no-source-write tests.
- `Content/Python/gamexxk_probe_three_chapter_route.py` — read-only live state/UMG/actor probe plus player-facing actions only.
- `scripts/gamexxk_three_chapter_route_acceptance.py` — PIE scenarios for formations, intent cards, phases, chapter jumps, settlement, and click recovery.
- `scripts/test_gamexxk_three_chapter_route_acceptance.py` — offline report and command contract tests.
- `docs/verification/2026-07-22-three-chapter-route-enemies-balance.md` — final commands, hashes, test totals, balance gates, PIE evidence, and remaining non-blocking observations.

### New visual source and import files

- `SourceAssets/RouteEnemies/route-enemy-manifest.json` — identities, prompt records, checksums, candidate/approved state, required views, and versioned UE destinations.
- `SourceAssets/RouteEnemies/prompts/route-enemy-prompts-v1.json` — exact built-in `image_gen` prompt specs for 18 new monsters and appended status icons.
- `scripts/verify_route_enemy_visual_sources.py`
- `scripts/prepare_route_enemy_sprite_atlas.py`
- `scripts/test_route_enemy_visual_manifest.py`
- `scripts/test_route_enemy_sprite_atlas.py`
- `Content/Python/gamexxk_import_route_enemy_sprite_atlases.py`
- `Content/Python/gamexxk_assemble_route_enemy_characters.py`
- `Content/Python/gamexxk_import_route_enemy_portraits.py`
- `Content/Python/gamexxk_import_route_status_icons.py`
- `scripts/test_route_enemy_sprite_import_pipeline.py`
- `scripts/test_route_enemy_character_assembly.py`
- `SourceAssets/CharacterVisuals/character-visual-manifest.json`
- `SourceAssets/CharacterVisuals/prompts/character-visual-prompts-v1.json`
- `scripts/verify_character_visual_sources.py`
- `scripts/prepare_character_visual_atlases.py`
- `scripts/test_character_visual_manifest.py`
- `scripts/test_character_visual_atlases.py`
- `Content/Python/gamexxk_import_character_visuals.py`
- `Content/Python/gamexxk_assemble_character_paperzd.py`
- `scripts/test_character_visual_import_pipeline.py`
- `scripts/test_character_paperzd_assembly.py`

Candidate raster output lives only under `SourceAssets/RouteEnemies/candidates/v1/`. Human-approved alpha PNGs move to `SourceAssets/RouteEnemies/approved/v1/`. New UE assets live under `/Game/GameXXK/Sprites/Generated/RouteEnemies/V1`, `/Game/GameXXK/Characters/RouteEnemies/V1`, `/Game/GameXXK/UI/Codex/RouteEnemies/V1`, and `/Game/GameXXK/UI/Battle/Status/V1`. Existing `/Game/GameXXK/Characters/Enemies`, PartyDeck, maps, and camera packages are read-only inputs.

Partner/task-NPC candidates live only under `SourceAssets/CharacterVisuals/candidates/v1/`; approved images live under `SourceAssets/CharacterVisuals/approved/v1/`. Their new UE packages live only beneath `/Game/GameXXK/Characters/Generated/Partners/V1`, `/Game/GameXXK/Characters/Generated/TaskNpc/V1`, `/Game/GameXXK/Animations/Generated/CharacterVisuals/V1`, and `/Game/GameXXK/UI/Generated/CharacterVisuals/V1`. Existing PartyDeck card art, existing character/PaperZD packages, original NPC art, placed actors, and tuned scene transforms remain read-only references.

## Shared verification helpers

Use the project MCP automation toolset after a successful cold cycle:

```powershell
@'
from scripts.ue_mcp_client import UnrealMCPClient

client = UnrealMCPClient(timeout=60.0)
assert client.connect(), "UE MCP is unavailable"
toolset = "AutomationTestToolset.AutomationTestToolset"
print(client.call_tool("DiscoverTests", {"bForceRediscover": True}, toolset_name=toolset, timeout=180.0))
print(client.call_tool("RunTestsByFilter", {"filterExpression": "StartsWith:GameXXK.Route"}, toolset_name=toolset, timeout=1800.0))
'@ | python -
```

Expected final result: the requested test filter reports zero failures. Before any C++ milestone, run:

```powershell
python scripts/ue_tdd_pipeline.py
```

Expected: MCP saves all dirty packages, the editor closes, UBT performs a non-Live-Coding build, the editor relaunches from `D:\UE5 demo\GameXXK\GameXXK.uproject`, PIE starts, and the pipeline exits successfully. `--check-only` is never accepted as compile proof.

For every C++ red step, add the test first and run the cold pipeline immediately. A missing contract should fail UBT; a test against an existing contract should cold-build, relaunch, then fail through MCP for the stated assertion. For every green step, implement first, run the cold pipeline, and only then run MCP tests against the newly loaded DLL.

---

## Task 1: Add save-authoritative route/enemy schema without breaking older saves

**Files:**

- Create: `Source/GameXXK/Public/GameXXKEnemyTypes.h`
- Create: `Source/GameXXK/Private/Tests/GameXXKThreeChapterRouteTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKCardRunTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveGame.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveGame.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp`

- [ ] **Step 1: Write the failing reflected-type and in-memory dispatcher tests**

Register `GameXXK.Route.ThreeChapter.Schema` and `GameXXK.MVP.SaveGame.ThreeChapterVersionMigration`. Assert default route progress is inactive, chapter is `0`, combat level is `0`, enemy state maps are empty, no receipt is pending, and old saves load safely. Construct a save at the actual dependency version with an active route and pass it through `FGameXXKSaveMigration::MigrateToCurrent`; assert the report's target equals `UGameXXKMVPRules::GetCurrentSaveVersion()`, migration preserves the current map and route resources, assigns chapter `1`, snapshots `PlayerLevel`, sets root seed from the saved route seed, and does not regenerate nodes. `RestoreFromSaveState` must delegate to the same dispatcher and produce serialization-equal state, never implement a second in-memory chain.

```cpp
TestEqual(TEXT("inactive chapter"), State.CardRun.RouteProgress.CurrentChapter, 0);
TestEqual(TEXT("inactive combat level"), State.CardRun.RouteProgress.RouteCombatLevel, 0);
TestTrue(TEXT("no enemy state"), State.CardRun.ActiveBattle.EnemyStates.IsEmpty());

FGameXXKSaveState Legacy = UGameXXKMVPRules::MakeSaveState(ActiveLegacyRoute);
Legacy.SaveVersion = UGameXXKMVPRules::GetCurrentSaveVersion() - 1;
FGameXXKSaveState MigratedSave;
FGameXXKSaveMigrationReport Report;
TestTrue(TEXT("dispatcher migrates dependency save"), FGameXXKSaveMigration::MigrateToCurrent(Legacy, MigratedSave, Report));
TestEqual(TEXT("dynamic report target"), Report.TargetVersion, UGameXXKMVPRules::GetCurrentSaveVersion());
TestEqual(TEXT("old active route becomes chapter one"), MigratedSave.RuntimeState.CardRun.RouteProgress.CurrentChapter, 1);
TestEqual(TEXT("snapshot uses saved level"), MigratedSave.RuntimeState.CardRun.RouteProgress.RouteCombatLevel, ActiveLegacyRoute.PlayerLevel);
TestEqual(TEXT("existing map remains byte-stable"), BuildRouteSignature(MigratedSave.RuntimeState), BuildRouteSignature(ActiveLegacyRoute));
```

- [ ] **Step 2: Write failing real-slot backup, verification, and rollback tests**

Extend the existing unique temporary-slot fixture and failure-injection seams. Use the actual target version from `GetCurrentSaveVersion()` and derive the base backup label as `<SlotName>.PreV<TargetVersion>Backup`; never hardcode `PreV9` or another literal. Cover:

1. success stores the unmodified dependency-version object in the dynamic backup slot, reloads it, verifies its canonical checksum equals the original main-slot source checksum, writes the migrated current-version object to the main slot, reload-verifies serialization equality, and only then updates live state;
2. an existing dynamic backup is reused only when it reloads and its canonical source checksum matches the current main object; it is never overwritten;
3. a stale/mismatched dynamic backup causes allocation of the first free `.001`, `.002`, and so on attempt slot, whose checksum must match the current source; older `PreV7`/`PreV8` or same-label backups remain untouched;
4. backup write/reload failure, in-memory dispatcher failure, full-state validation failure, upgraded-main write failure, and post-write reload/equality failure all return false with the existing migration error text;
5. every failure leaves live `RuntimeState` byte-identical; failures before main write leave the main slot checksum unchanged, and failures after a main write restore only from this attempt's checksum-matched backup, then reload-verify the original main checksum before returning;
6. reloading an already-current slot is idempotent and creates no new backup; every test removes only its unique temporary main/attempt slots before and after execution.

Record the canonical source checksum, actual backup slot name, source version, and dynamic target version in `FGameXXKSaveMigrationReport`. A mismatched pre-existing backup is evidence for a numbered non-overwriting attempt, never permission to replace that backup.

- [ ] **Step 3: Cold-build the red tests, then run focused MCP filters**

```powershell
python scripts/ue_tdd_pipeline.py
```

If UBT first fails because `GameXXKEnemyTypes.h`, route fields, report fields, or the next dispatcher-stage declaration does not exist, record that compile red and do not call MCP. Add only the minimum compile-only declarations, rerun the cold pipeline until the editor loads the new test DLL, and only then run MCP with `StartsWith:GameXXK.Route.ThreeChapter.Schema`, `StartsWith:GameXXK.MVP.SaveGame.ThreeChapterVersionMigration`, and the next-version cases under `StartsWith:GameXXK.Equipment.SaveMigration`. Expected behavior red: the dispatcher still targets the dependency version, the dynamic backup chain is absent, or an injected failure changes the main slot/live state. Record at least one focused behavior assertion before implementation.

- [ ] **Step 4: Define explicit append-only types**

Add the following public contract. Existing serialized enum values remain unchanged; every new `EGameXXKCardStatus` value is appended after `TerrainBonusDoubleThisRound = 19`.

```cpp
UENUM(BlueprintType)
enum class EGameXXKEnemyTier : uint8
{
    Normal = 0, Elite = 1, Boss = 2
};

UENUM(BlueprintType)
enum class EGameXXKEnemyIntentTargetRule : uint8
{
    None = 0, Self = 1, LowestHealthParty = 2, RandomLivingParty = 3,
    AllLivingParty = 4, AllEnemyAllies = 5, LowestHealthEnemyAlly = 6,
    MarkedParty = 7, PreyTarget = 8
};

UENUM(BlueprintType)
enum class EGameXXKEnemyIntentEffectType : uint8
{
    DirectDamage = 0, AddArmor = 1, Heal = 2, ApplyStatus = 3,
    ConsumeSharedQi = 4, ModifyAttack = 5, ModifyDefense = 6,
    ModifySpeed = 7, RemovePositiveStatus = 8, IncreaseNextCardEnergy = 9,
    SetCounter = 10, SetCharge = 11
};

UENUM(BlueprintType)
enum class EGameXXKEnemyPassiveTrigger : uint8
{
    BattleStart = 0, RoundStart = 1, FirstDirectDamageReceived = 2,
    DirectDamageReceived = 3, FirstStatusReceivedThisRound = 4,
    DirectAttackReceived = 5, TargetDefeated = 6
};

UENUM(BlueprintType)
enum class EGameXXKEnemyPassiveId : uint8
{
    None = 0, IronfeatherFirstHit = 1, BluehornArmorRetention = 2,
    MoneyRatWealth = 3, PorcupineCounter = 4, GraymaneMarkedHunt = 5,
    RedtuskRage = 6, BlackBearThickHide = 7, WhiteApeStatusGuard = 8,
    DeerHealCooldown = 9, TigerPredator = 10
};

UENUM(BlueprintType)
enum class EGameXXKEnemyPhaseId : uint8
{
    None = 0, MoneyRatMadHoard = 1, BlackBearEnraged = 2, TigerDread = 3
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKResolvedEnemyIntentEffect
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKEnemyIntentEffectType Type = EGameXXKEnemyIntentEffectType::DirectDamage;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TArray<FName> TargetUnitIds;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 Magnitude = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 HitCount = 1;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKCardStatus Status = EGameXXKCardStatus::None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 StatusStacks = 0;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEnemyBattleState
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName DefinitionId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 IntentCursor = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) bool bPhaseTwo = false;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName PendingChargedIntentId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 ChargeRoundsRemaining = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 HealingCooldownRounds = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 InitiativeBonus = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) bool bFirstHitPassiveAvailable = true;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) bool bFirstStatusPassiveAvailable = true;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKRouteProgress
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 SchemaVersion = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 RootSeed = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TArray<int32> ChapterSeeds;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 CurrentChapter = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 RouteCombatLevel = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 ActualRouteCardAcquisitionCount = 0;
};
```

Append `Medicine = 20`, `Weak = 21`, `Wealth = 22`, `Rage = 23`, `Prey = 24`, `Charge = 25`, and `Counter = 26` to `EGameXXKCardStatus`. Add `EnemyDefinitionId`, `BattleSlotNumber`, and `CombatLevel` to both `FGameXXKBattleRuntimeUnit` and `FGameXXKCardCombatUnit`. Add `TMap<FName, FGameXXKEnemyBattleState> EnemyStates` to `FGameXXKCardBattleRuntime`. Add `FGameXXKRouteProgress RouteProgress` to `FGameXXKCardRunState`.

- [ ] **Step 5: Extend the saved intent without deleting legacy fields**

Keep `Damage`, `Kind`, and `OnHitStatuses` for old-save migration, and append:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName IntentDefinitionId = NAME_None;
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKEnemyIntentTargetRule TargetRule = EGameXXKEnemyIntentTargetRule::None;
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TArray<FGameXXKResolvedEnemyIntentEffect> Effects;
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 ResolutionOrder = INDEX_NONE;
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) bool bCharging = false;
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 ChargeRounds = 0;
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FString PhaseLabel;
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TArray<FString> TooltipLines;
```

- [ ] **Step 6: Extend the unified dispatcher and real-slot load chain to the dynamic target version**

Expose `UGameXXKMVPRules::GetCurrentSaveVersion()` for tests/probes. After dependency integration, increment the actual current global version once and bind `ThreeChapterRouteIntroducedSaveVersion` to it. Extend the existing `FGameXXKSaveMigration` dispatcher with exactly one dependency-version-to-three-chapter-version stage; all older versions must first traverse the frozen plan-one, plan-two, and rebased route-merchant stages. Reject a source newer than the dynamic target, accept a current source without mutation, and make `RestoreFromSaveState` delegate to this dispatcher.

The new stage migrates on a candidate: an active old route becomes chapter 1 with `RootSeed = RouteSeed`, `ChapterSeeds = {RouteSeed, 0, 0}`, and `RouteCombatLevel = Clamp(PlayerLevel, 1, 20)`; Task 4 deterministically fills only the two zero future seeds without regenerating the current map. Inactive saves keep zero route progress. Convert legacy damage-only intents into one resolved `DirectDamage` effect without changing their saved source/target IDs. Validate the full runtime/card/route/enemy/equipment/meta-shop state and commit the candidate only after every validator succeeds.

Extend `UGameXXKMVPSubsystem::LoadGameFromSlot` rather than adding a parallel loader. It must load the original object locally without changing live state; canonically serialize and checksum it; select or create the non-overwriting `<SlotName>.PreV<TargetVersion>Backup[.NNN]` whose reloaded checksum matches that exact source; migrate and fully validate a copy; write the migrated main slot; reload and compare the current-version object; and only then cross the normal runtime mutation boundary. If failure occurs after main write, restore from the exact checksum-matched backup recorded for this attempt and reload-verify the original checksum. On every failure, both the main slot and live state equal their pre-call values. Successful backups persist and are never overwritten or automatically deleted.

- [ ] **Step 7: Cold compile, run green in-memory and real-slot migration tests, and commit**

Run `python scripts/ue_tdd_pipeline.py` first. Expected: a successful non-Live-Coding build and editor relaunch with the new DLL. Then use MCP to run `StartsWith:GameXXK.Route.ThreeChapter.Schema`, `StartsWith:GameXXK.MVP.SaveGame.ThreeChapterVersionMigration`, `StartsWith:GameXXK.MVP.SaveGame.SlotRoundTrip`, and the next-version cases under `StartsWith:GameXXK.Equipment.SaveMigration`. Expected: new saves round-trip route/enemy fields; every supported prior version traverses the single dispatcher; active routes migrate once without regenerating their map; current saves are idempotent; dynamic checksum-matched backups, numbered mismatch attempts, verified main writes, and injected rollback cases pass; and every failure preserves both the original main slot and live state.

```powershell
git add Source/GameXXK/Public/GameXXKEnemyTypes.h Source/GameXXK/Public/GameXXKCardTypes.h Source/GameXXK/Public/GameXXKCardRunTypes.h Source/GameXXK/Public/GameXXKMVPRules.h Source/GameXXK/Private/GameXXKMVPRules.cpp Source/GameXXK/Public/MVP/GameXXKSaveMigration.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp Source/GameXXK/Public/MVP/GameXXKSaveGame.h Source/GameXXK/Private/MVP/GameXXKSaveGame.cpp Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp Source/GameXXK/Private/Tests/GameXXKThreeChapterRouteTest.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp
git diff --check
git commit -m "feat: add three-chapter route save schema"
```

---

## Task 2: Author and validate the exact 21-enemy catalog

**Files:**

- Create: `Source/GameXXK/Public/GameXXKEnemyCatalog.h`
- Create: `Source/GameXXK/Private/GameXXKEnemyCatalog.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKEnemyCatalogTest.cpp`

- [ ] **Step 1: Write the failing catalog-integrity test**

Register `GameXXK.Data.EnemyCatalog`. Assert exactly 21 definitions, exactly four normal/two elite/one Boss per chapter, globally unique IDs, no cross-chapter pool membership, exact intent counts `3/4/6`, elite/Boss passives, Boss phase-two definitions at 50 percent, positive stats/growth, valid Codex IDs, and non-empty soft paths. Iterate every ID in the tables below rather than checking counts alone.

```cpp
TestEqual(TEXT("enemy count"), FGameXXKEnemyCatalog::GetAllDefinitions().Num(), 21);
for (int32 Chapter = 1; Chapter <= 3; ++Chapter)
{
    TestEqual(TEXT("normal pool"), FGameXXKEnemyCatalog::GetPool(Chapter, EGameXXKEnemyTier::Normal).Num(), 4);
    TestEqual(TEXT("elite pool"), FGameXXKEnemyCatalog::GetPool(Chapter, EGameXXKEnemyTier::Elite).Num(), 2);
    TestEqual(TEXT("boss pool"), FGameXXKEnemyCatalog::GetPool(Chapter, EGameXXKEnemyTier::Boss).Num(), 1);
}
```

- [ ] **Step 2: Run the red test**

Run `python scripts/ue_tdd_pipeline.py`. Expected red result: UBT fails because the catalog API referenced by the new test does not exist; no stale editor DLL is used as evidence.

- [ ] **Step 3: Define immutable catalog structs and API**

```cpp
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEnemyIntentEffectDefinition
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, EditAnywhere) EGameXXKEnemyIntentEffectType Type = EGameXXKEnemyIntentEffectType::DirectDamage;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) EGameXXKEnemyIntentTargetRule Target = EGameXXKEnemyIntentTargetRule::LowestHealthParty;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 FlatMagnitude = 0;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 AttackPercent = 0;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 HitCount = 1;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) EGameXXKCardStatus Status = EGameXXKCardStatus::None;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 StatusStacks = 0;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) EGameXXKCardStatus ConsumedStatus = EGameXXKCardStatus::None;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 MaxConsumedStacks = 0;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 MagnitudePerConsumedStack = 0;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEnemyIntentDefinition
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, EditAnywhere) FName Id = NAME_None;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) FText DisplayName;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) TArray<FGameXXKEnemyIntentEffectDefinition> Effects;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 ChargeRounds = 0;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 Weight = 1;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) bool bPhaseTwoOnly = false;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) bool bRequiresSourceBelowHalf = false;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) EGameXXKCardStatus RequiredTargetStatus = EGameXXKCardStatus::None;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 CooldownRounds = 0;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEnemyComputedStats
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 MaxHP = 1;
    UPROPERTY(BlueprintReadOnly) int32 Attack = 1;
    UPROPERTY(BlueprintReadOnly) int32 Defense = 0;
    UPROPERTY(BlueprintReadOnly) int32 Speed = 1;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEnemyDefinition
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, EditAnywhere) FName Id = NAME_None;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) FText DisplayName;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 Chapter = 0;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) EGameXXKEnemyTier Tier = EGameXXKEnemyTier::Normal;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 BaseHP = 1;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) float HPPerLevel = 0.0f;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 BaseAttack = 1;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) float AttackPerLevel = 0.0f;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 BaseDefense = 0;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) float DefensePerLevel = 0.0f;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 Speed = 1;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) TArray<FGameXXKEnemyIntentDefinition> Intents;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) EGameXXKEnemyPassiveId PassiveId = EGameXXKEnemyPassiveId::None;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) EGameXXKEnemyPhaseId PhaseId = EGameXXKEnemyPhaseId::None;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) int32 PhaseThresholdPercent = 0;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) FName CodexId = NAME_None;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) FSoftObjectPath PortraitSoftPath;
    UPROPERTY(BlueprintReadOnly, EditAnywhere) FSoftObjectPath BattleVisualSoftPath;
};

class GAMEXXK_API FGameXXKEnemyCatalog final
{
public:
    static const TArray<FGameXXKEnemyDefinition>& GetAllDefinitions();
    static const FGameXXKEnemyDefinition* Find(FName DefinitionId);
    static TArray<FName> GetPool(int32 Chapter, EGameXXKEnemyTier Tier);
    static bool Validate(FString* OutError = nullptr);
    static FGameXXKEnemyComputedStats ComputeStats(FName DefinitionId, int32 CombatLevel);
};
```

- [ ] **Step 4: Add exact identity and baseline-stat rows**

Use `HP/HP growth`, `attack/attack growth`, `defense/defense growth`, and speed as follows. Growth is applied for `CombatLevel - 1`, rounded half away from zero, and then clamped to at least `1/1/0/1`.

| Chapter | Tier | ID | Display | HP/growth | ATK/growth | DEF/growth | SPD |
| ---: | --- | --- | --- | ---: | ---: | ---: | ---: |
| 1 | Normal | `Enemy.Ch1.Rooster` | 公鸡 | 46/7 | 8/1.1 | 1/0.25 | 10 |
| 1 | Normal | `Enemy.Ch1.Goat` | 山羊 | 58/8 | 7/1.0 | 3/0.35 | 6 |
| 1 | Normal | `Enemy.Ch1.Weasel` | 黄鼬 | 42/6 | 9/1.2 | 1/0.20 | 11 |
| 1 | Normal | `Enemy.Ch1.Civet` | 狸猫 | 48/7 | 8/1.1 | 2/0.25 | 9 |
| 1 | Elite | `Enemy.Ch1.IronfeatherRooster` | 铁羽斗鸡 | 118/15 | 14/1.7 | 5/0.50 | 11 |
| 1 | Elite | `Enemy.Ch1.BluehornGoatKing` | 青角羊王 | 138/17 | 13/1.6 | 7/0.65 | 7 |
| 1 | Boss | `Enemy.Ch1.MoneyRat` | 金钱鼠 | 240/24 | 17/2.0 | 8/0.75 | 10 |
| 2 | Normal | `Enemy.Ch2.GrayWolf` | 灰狼 | 62/9 | 11/1.3 | 2/0.30 | 12 |
| 2 | Normal | `Enemy.Ch2.Boar` | 野猪 | 76/10 | 10/1.2 | 5/0.45 | 7 |
| 2 | Normal | `Enemy.Ch2.Macaque` | 猕猴 | 58/8 | 10/1.3 | 2/0.25 | 13 |
| 2 | Normal | `Enemy.Ch2.Porcupine` | 豪猪 | 70/9 | 9/1.1 | 5/0.50 | 8 |
| 2 | Elite | `Enemy.Ch2.GraymaneWolfKing` | 苍鬃狼王 | 158/18 | 18/2.0 | 6/0.55 | 13 |
| 2 | Elite | `Enemy.Ch2.RedtuskBoarKing` | 赤獠猪王 | 188/20 | 17/1.9 | 9/0.75 | 8 |
| 2 | Boss | `Enemy.Ch2.BlackBear` | 黑熊 | 320/30 | 23/2.4 | 11/0.90 | 7 |
| 3 | Normal | `Enemy.Ch3.VenomSnake` | 毒蛇 | 72/9 | 12/1.35 | 2/0.25 | 14 |
| 3 | Normal | `Enemy.Ch3.Wildcat` | 山猫 | 70/9 | 14/1.50 | 3/0.30 | 14 |
| 3 | Normal | `Enemy.Ch3.Vulture` | 秃鹫 | 74/9 | 13/1.45 | 3/0.30 | 15 |
| 3 | Normal | `Enemy.Ch3.GiantToad` | 巨蟾 | 94/12 | 11/1.25 | 7/0.60 | 6 |
| 3 | Elite | `Enemy.Ch3.WhiteApe` | 白猿 | 198/21 | 21/2.20 | 8/0.65 | 12 |
| 3 | Elite | `Enemy.Ch3.SpiralHornDeer` | 盘角鹿 | 210/22 | 20/2.10 | 9/0.75 | 11 |
| 3 | Boss | `Enemy.Ch3.Tiger` | 老虎 | 380/34 | 28/2.70 | 12/1.00 | 14 |

These are simulation starting values, not self-applying tuning output. Later reports may recommend different values, but an engineer changes them only after human approval.

- [ ] **Step 5: Add the exact intent/passive rows**

The notation is data, not a second resolver: `D%` means direct damage at that attack percentage, `A` armor, `H%Max` heal percent max HP, `Sx` status stacks, `Qi-` shared energy subtraction, and `C1` one-round charge. Encode these rows into `FGameXXKEnemyIntentDefinition` arrays in the listed order; a saved seed chooses the starting cursor, then the cursor cycles deterministically while respecting phase/cooldown rules.

| Enemy | Intent rows | Passive / phase |
| --- | --- | --- |
| 公鸡 | `Peck D100`; `DoublePeck D55x2`; `Crow AllEnemyAllies ATK+2 one round` | none |
| 山羊 | `Horn D90 + Weak1`; `Stomp A10`; `Charge C1 then D170` | none |
| 黄鼬 | `Harass D80 + Mark1`; `StinkFog AllLivingParty Weak1`; `Escape Agility1 + A5` | none |
| 狸猫 | `Claw D95`; `Feint Mark2`; `Pickpocket D70 + Qi-1 + A8` | none |
| 铁羽斗鸡 | `RapidPeck D50x3`; `IronGuard A16`; `BattleCry allies ATK+3 one round`; `BloodFight D170 below 50% HP` | first direct hit received is reduced by 50% |
| 青角羊王 | `Pierce D120 + Weak2`; `HerdStomp AllLivingParty D70`; `GuardHerd allies A8`; `RageCharge C1 then D210` | retains 50% armor at its phase start |
| 金钱鼠 | `CoinVolley AllLivingParty D70`; `Hoard A18 + Wealth2`; `GreedyMark Mark2`; `Pickpocket D90 + Qi-1 + Wealth1`; `BreakWealth consume up to 3 Wealth, H6%Max each`; `CoinCrash D100 + 15 flat per Wealth` | round start Wealth+1; at <=50% enter 守财癫狂, round Wealth gain becomes 2 and direct damage +25% |
| 灰狼 | `Bite D100 + Mark1`; `Pursuit D130 when target Marked`; `CallPack allies ATK+2 one round` | none |
| 野猪 | `Tusk D110`; `Bristle A12`; `ArmorBreakCharge D145 + Weak1` | none |
| 猕猴 | `ThrowStone D90 random`; `Snatch remove one positive status`; `Hasten allies SPD+3 next enemy phase` | none |
| 豪猪 | `Quill D85 + Bleed1`; `BristleGuard A8 + Counter1`; `QuillVolley AllLivingParty D55 + Bleed1` | Counter retaliates D40 only against the next direct attack |
| 苍鬃狼王 | `HuntMark D90 + Mark2`; `ContinuousHunt D70x3 against Marked`; `PackOrder allies ATK+3 and target Mark1`; `Sidestep Agility1 + A6` | Marked party targets take 20% more direct damage from this enemy |
| 赤獠猪王 | `HeavyArmor A18`; `Earthquake AllLivingParty D80 + Weak1`; `RageStrike D140 + 20 flat per Rage`; `RedCharge C1 then D190` | each received direct attack grants Rage1, cap5 |
| 黑熊 | `Sweep AllLivingParty D80`; `Pounce D150`; `WeakRoar AllLivingParty Weak1`; `Rend D110 + Bleed2`; `CounterPosture Counter1`; `Quake AllLivingParty D100 + Weak1` | 厚皮 reduces incoming direct damage 15%; at <=50% 狂怒 lowers base defense 25%, raises attack 30%, and adds one hit to Pounce/Rend |
| 毒蛇 | `VenomBite D70 + Poison2`; `Coil Agility1 + A5`; `ToxicPursuit D90 + 12 flat per target Poison` | none |
| 山猫 | `Rake D95 + Bleed1`; `Stalk Mark2 + Agility1`; `BloodPursuit D140 against Bleeding` | none |
| 秃鹫 | `Gaze mark lowest-health party`; `Dive D120, +50% below 50% HP`; `WingCut AllLivingParty D60` | none |
| 巨蟾 | `Tongue D100`; `PoisonFog AllLivingParty Poison1`; `Inflate A14 and next PoisonFog +1 Poison` | none |
| 白猿 | `ThrowRock D120`; `Disturb increase one playable hand card energy by 1`; `BoulderCharge C1 then D220`; `WideSweep AllLivingParty D85` | first status received each round grants A8 |
| 盘角鹿 | `Horn D125`; `TerrainBless allies direct damage +20% one round`; `HerdArmor allies A10`; `SpringHeal lowest-health ally H12%Max, cooldown2` | heal intent is ineligible while cooldown is positive |
| 老虎 | `MarkPrey Prey1 on lowest-health`; `TigerPounce D160, +50% against Prey`; `TailSweep AllLivingParty D95`; `BleedingRend D120 + Bleed2`; `DreadRoar AllLivingParty Weak1`; `Ambush C1 then D240` | after damaging a Bleeding target heal 8% missing HP; at <=50% enter 百兽震惶, Prey no longer expires, TigerPounce becomes two hits, and defeating Prey immediately marks the next lowest-health living party member |

- [ ] **Step 6: Cold compile, run catalog tests, and commit**

Run `python scripts/ue_tdd_pipeline.py` first. Expected: a successful non-Live-Coding build and editor relaunch with the new DLL. Then use MCP to run `StartsWith:GameXXK.Data.EnemyCatalog`. Expected: all IDs, stats, counts, pools, intent rows, passives, phase thresholds, and soft-path syntax pass.

```powershell
git add Source/GameXXK/Public/GameXXKEnemyCatalog.h Source/GameXXK/Private/GameXXKEnemyCatalog.cpp Source/GameXXK/Private/Tests/GameXXKEnemyCatalogTest.cpp
git diff --check
git commit -m "feat: add twenty-one enemy catalog"
```

---

## Task 3: Build deterministic chapter formations with explicit 1P/2P/3P slots

**Files:**

- Create: `Source/GameXXK/Public/GameXXKEncounterRules.h`
- Create: `Source/GameXXK/Private/GameXXKEncounterRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKEncounterFormationTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Private/GameXXKBattlePresentation.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleEncounterRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp`

- [ ] **Step 1: Write failing formation-shape tests**

Add automation cases under `GameXXK.Route.EncounterFormation` that call a pure encounter builder for every chapter and node tier. Assert exactly these shapes:

| Node | 1P | 2P | 3P |
| --- | --- | --- | --- |
| Normal | distinct chapter normal | empty | distinct chapter normal |
| Elite | distinct chapter normal | chapter elite | distinct chapter normal |
| Boss | chapter elite A | chapter boss | chapter elite B |

Also assert normal/elite sampling is without replacement, chapter pools never leak across chapters, the same `(RootSeed, Chapter, NodeId)` returns byte-identical definitions/slots, a changed node ID can produce another deterministic selection, and the stronger unit is always 2P. Run `StartsWith:GameXXK.Route.EncounterFormation`. Expected red result: the encounter-rule API and explicit slot fields do not exist.

- [ ] **Step 2: Write failing combat-level and route-snapshot tests**

For route snapshots `1, 5, 10, 15, 20`, assert normal level is `Snapshot`, elite is `Min(Snapshot + 1, 20)`, boss is `Min(Snapshot + 2, 20)`, and task NPC projection uses the saved `RouteCombatLevel`, not a later changed `PlayerLevel`. Add negative cases for chapter outside `1..3`, duplicate catalog IDs, missing 2P boss, and an invalid snapshot; each must return `false` without mutating the output arrays.

- [ ] **Step 3: Add the pure formation contract**

Implement this public surface and keep all random streams local:

```cpp
struct FGameXXKEncounterSlot
{
    FName EnemyDefinitionId = NAME_None;
    int32 BattleSlotNumber = INDEX_NONE;
    int32 CombatLevel = 1;
};

class FGameXXKEncounterRules
{
public:
    static int32 DeriveChapterSeed(int32 RootSeed, int32 Chapter);
    static int32 GetCombatLevel(EGameXXKEnemyTier Tier, int32 RouteCombatLevel);
    static bool BuildFormation(int32 Chapter, EGameXXKNodeKind NodeKind,
        int32 ChapterSeed, int32 NodeId,
        TArray<FGameXXKEncounterSlot>& OutSlots, FString* OutError = nullptr);
};
```

Use `FRandomStream` initialized from a hash of chapter seed, node ID, and tier; do not consume `FMath::Rand` or global random state. Sort the returned array by `BattleSlotNumber` after assigning slots. `BuildFormation` first validates into a local array and moves it into `OutSlots` only on success.

- [ ] **Step 4: Persist explicit enemy identity, level, and slot**

Add `EnemyDefinitionId`, `BattleSlotNumber`, and `CombatLevel` to the legacy battle-unit and card-combat-unit projections. Runtime unit IDs must be stable and readable: `Enemy.<DefinitionLeaf>.P<Slot>`. Preserve saved IDs on reload; do not generate a new ID from array position. In `BuildCardCombatUnits`, copy the explicit values into the card runtime and reject duplicate occupied slots before committing.

- [ ] **Step 5: Replace the hardcoded single-enemy battle entry**

In `GameXXKMVP::BeginBattle`, call `BuildFormation` with the saved chapter seed, node kind, and node ID, then create all returned enemies. Keep reward/event/shop node behavior unchanged. In `GetUnitSlotNumber`, return `BattleSlotNumber` when it is `1..3`; retain `StableSortOrder + 1` only for a migrated pre-feature save lacking an explicit slot.

- [ ] **Step 6: Cold compile, run focused tests, and commit**

Run `python scripts/ue_tdd_pipeline.py` first. Expected: a successful non-Live-Coding build and editor relaunch with the new DLL. Then use MCP to run `StartsWith:GameXXK.Route.EncounterFormation`, `StartsWith:GameXXK.Battle.Encounter`, and the adapter slot tests. Expected green result: all formations, caps, task-NPC snapshot projection, explicit slots, stable IDs, and legacy fallback pass.

```powershell
git add Source/GameXXK/Public/GameXXKEncounterRules.h Source/GameXXK/Private/GameXXKEncounterRules.cpp Source/GameXXK/Private/Tests/GameXXKEncounterFormationTest.cpp Source/GameXXK/Public/GameXXKMVPRules.h Source/GameXXK/Private/GameXXKMVPRules.cpp Source/GameXXK/Public/GameXXKCardTypes.h Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/GameXXKBattlePresentation.cpp Source/GameXXK/Private/Tests/GameXXKBattleEncounterRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp
git diff --check
git commit -m "feat: build deterministic route enemy formations"
```

---

## Task 4: Run the existing random map topology as three saved chapters

**Files:**

- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKThreeChapterRouteTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteMapSeedRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardRouteLifecycleTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKQingshanTaskNpcRouteTest.cpp`

- [ ] **Step 1: Freeze the existing seven-layer topology in failing regression tests**

Capture the current `GenerateRouteMapForSeed` contract: start layer, five random intermediate layers, and one boss layer, including current edge connectivity and random node-kind distribution. Assert a chapter map has exactly the same topology contract as today. Expected red only for the new chapter assertions; the legacy seed/topology assertions remain green.

- [ ] **Step 2: Write failing route-entry and chapter-seed tests**

Assert `EnterDungeon` snapshots `PlayerLevel` once into `RouteCombatLevel`, stores root seed, sets chapter `1`, fills `ChapterSeeds` with exactly three different deterministic values, and generates chapter 1 through the unchanged map generator. Changing `PlayerLevel` after entry must not alter this snapshot. A migrated active route with `{RouteSeed, 0, 0}` fills only indices 1 and 2 and must not regenerate its current chapter-1 map.

- [ ] **Step 3: Write failing boss-transition preservation tests**

Drive a state through chapter 1 and chapter 2 boss reward completion. Each transition must:

- fully restore current/max HP and MP for hero, carried permanent partner, and active task NPC;
- clear battle armor, statuses, temporary card modifiers, enemy intents, and active battle state;
- preserve the route deck/card qualities, relics, travel money, event attribute gains, actual route-card acquisition count, carried partner, task NPC, and route-level snapshot;
- increment chapter once and generate a fresh current-structure map from that chapter seed;
- map bosses exactly as chapter 1 Money Rat, chapter 2 Black Bear, chapter 3 Tiger.

Add a forced transition failure and assert the complete pre-call state remains byte-equivalent. Expected red: current `ResolveBossClear` ends the run instead of advancing chapters.

- [ ] **Step 4: Implement deterministic chapter progress without changing map generation**

Keep `GenerateRouteMapForSeed` intact. Add a private transition helper that copies state, validates the completed boss and pending rewards, clears only battle-local fields, reads `ChapterSeeds[NextChapter - 1]`, calls `GenerateRouteMapForSeed(Copy, SelectedChapterSeed)`, then commits. Derive every new chapter seed with a stable integer mix rather than platform hash behavior:

```cpp
uint32 Mixed = static_cast<uint32>(RootSeed) ^ (0x9E3779B9u * static_cast<uint32>(Chapter));
Mixed ^= Mixed >> 16;
Mixed *= 0x7FEB352Du;
Mixed ^= Mixed >> 15;
Mixed *= 0x846CA68Bu;
Mixed ^= Mixed >> 16;
return static_cast<int32>(Mixed);
```

Store all three derived seeds in the route progress so reload does not depend on future implementation changes.

- [ ] **Step 5: Make only chapter 3 boss invoke final settlement**

Route boss victory through the chapter-transition helper for chapters 1 and 2. Chapter 3 records a successful terminal outcome and hands control to the settlement rules introduced in Task 5. Keep the current reward gate: a boss cannot transition or settle until its saved reward choice is resolved.

- [ ] **Step 6: Cold compile, run lifecycle regressions, and commit**

Run `python scripts/ue_tdd_pipeline.py` first. Expected: a successful non-Live-Coding build and editor relaunch with the new DLL. Then use MCP to run `StartsWith:GameXXK.Route.MapSeed`, `StartsWith:GameXXK.Route.ThreeChapter`, `StartsWith:GameXXK.Card.RouteLifecycle`, and the task-NPC route filter. Expected green result: topology is unchanged; all three seeds, boss order, heals, cleanup, preservation, migration, reward gating, and atomic failures pass.

```powershell
git add Source/GameXXK/Public/GameXXKMVPRules.h Source/GameXXK/Private/GameXXKMVPRules.cpp Source/GameXXK/Private/Tests/GameXXKThreeChapterRouteTest.cpp Source/GameXXK/Private/Tests/GameXXKRouteMapSeedRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKCardRouteLifecycleTest.cpp Source/GameXXK/Private/Tests/GameXXKQingshanTaskNpcRouteTest.cpp
git diff --check
git commit -m "feat: add three deterministic route chapters"
```

---

## Task 5: Settle route currencies once with an idempotent receipt

**Files:**

- Create: `Source/GameXXK/Public/GameXXKRouteSettlementRules.h`
- Create: `Source/GameXXK/Private/GameXXKRouteSettlementRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKRouteSettlementTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardRunTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardRouteLifecycleTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp`

- [ ] **Step 1: Write failing conversion-table tests**

For travel money values `0, 9, 10, 19, 20, 101` and actual route-card acquisition counts `0, 4, 5, 9, 10, 51`, assert integer-floor conversions:

| Outcome | Permanent gold | Enhancement stones |
| --- | --- | --- |
| Chapter 3 clear | `RouteTravelMoney / 10` | `ActualRouteCardAcquisitionCount / 5` |
| Defeat | `RouteTravelMoney / 20` | `ActualRouteCardAcquisitionCount / 10` |
| Active abandon | `RouteTravelMoney / 20` | `ActualRouteCardAcquisitionCount / 10` |

Zero-value settlement remains a valid receipt. Expected red: the receipt API and acquisition counter do not exist.

- [ ] **Step 2: Write failing acquisition-accounting tests**

Increment `ActualRouteCardAcquisitionCount` exactly once when a card is actually accepted from battle reward, route merchant purchase, or positive event grant. Count the acquisition before same-name quality merge or replacement, so a merge still contributes one. Do not count the 18 opening cards, a skipped reward, a failed purchase, an unaffordable purchase, a canceled replacement, or a load/migration. Assert every failure leaves the count unchanged.

- [ ] **Step 3: Write failing receipt replay and crash-window tests**

Create one terminal receipt, save before applying it, apply and save, then attempt to apply the same ID repeatedly and after reload. Permanent currency may change only once. Simulate a save containing a pending unapplied receipt and one containing its `LastAppliedSettlementId`; the first applies once, the second only clears stale route-local data. Assert a rejected receipt cannot partially award or clear the run.

- [ ] **Step 4: Implement preview and atomic apply**

Use this contract:

```cpp
UENUM()
enum class EGameXXKRouteTerminalOutcome : uint8 { Cleared, Defeated, Abandoned };

USTRUCT()
struct FGameXXKRouteSettlementReceipt
{
    GENERATED_BODY()
    FGuid SettlementId;
    EGameXXKRouteTerminalOutcome Outcome = EGameXXKRouteTerminalOutcome::Defeated;
    int32 SourceTravelMoney = 0;
    int32 SourceCardAcquisitionCount = 0;
    int32 PermanentGoldAward = 0;
    int32 EnhancementStoneAward = 0;
};

class FGameXXKRouteSettlementRules
{
public:
    static bool Preview(const FGameXXKRuntimeState& State,
        EGameXXKRouteTerminalOutcome Outcome,
        FGameXXKRouteSettlementReceipt& OutReceipt, FString* OutError = nullptr);
    static bool Apply(FGameXXKRuntimeState& State,
        const FGameXXKRouteSettlementReceipt& Receipt, FString* OutError = nullptr);
};
```

Generate the GUID only when a new terminal transition is committed, persist pending receipt plus last applied ID, validate the receipt's recorded inputs against the saved terminal snapshot, award on a copied state, record the ID, clear all route-local fields, then move the copy into the caller. Never derive a second receipt during replay.

- [ ] **Step 5: Route every terminal path through settlement**

Call the same rules from chapter 3 boss clear, `FailDungeonToTown`, and the existing player-confirmed abandon path. Defeat and abandon use the same rates but retain distinct outcome values for analytics. Do not clear cards, relics, money, NPC, or route progress before `Apply` succeeds.

- [ ] **Step 6: Cold compile, run settlement/lifecycle/save tests, and commit**

Run `python scripts/ue_tdd_pipeline.py` first. Expected: a successful non-Live-Coding build and editor relaunch with the new DLL. Then use MCP to run `StartsWith:GameXXK.Route.Settlement`, `StartsWith:GameXXK.Card.RouteLifecycle`, and the route cases in `GameXXK.Save`. Expected green result: conversion boundaries, three acquisition sources, exclusions, replay, reload, terminal routing, and atomic clear all pass.

```powershell
git add Source/GameXXK/Public/GameXXKRouteSettlementRules.h Source/GameXXK/Private/GameXXKRouteSettlementRules.cpp Source/GameXXK/Private/Tests/GameXXKRouteSettlementTest.cpp Source/GameXXK/Public/GameXXKCardRunTypes.h Source/GameXXK/Public/GameXXKMVPRules.h Source/GameXXK/Private/GameXXKMVPRules.cpp Source/GameXXK/Private/Tests/GameXXKCardRouteLifecycleTest.cpp Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp
git diff --check
git commit -m "feat: settle route rewards idempotently"
```

---

## Task 6: Forecast and resolve every living enemy intent through shared card rules

**Files:**

- Create: `Source/GameXXK/Public/GameXXKEnemyIntentRules.h`
- Create: `Source/GameXXK/Private/GameXXKEnemyIntentRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardBattleAdapter.h`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp`

- [ ] **Step 1: Write failing simultaneous-forecast tests**

Construct one-, two-, and three-enemy battles. Assert `BuildEnemyIntents` produces one saved intent for every living enemy before the player acts, dead enemies produce none, all target IDs/effect magnitudes are locked into the forecast, and reloading does not reroll them. Forecast order must be initiative descending, then explicit slot ascending. Expected red: the legacy intent only stores one damage action and cannot express the catalog rows.

- [ ] **Step 2: Write failing target and effect tests**

Cover single lowest-health, single marked/prey, deterministic random single, all living party, self, lowest-health ally, and all living enemy allies. Cover direct damage, multi-hit damage, armor, healing, Poison/Bleed/Weak/Mark/Agility/Counter/Wealth/Rage/Prey, Qi reduction clamped at zero, one-round attack/speed modifiers, positive-status removal, and one-card energy increase. Expected red: each missing target/effect kind is reported explicitly.

- [ ] **Step 3: Write failing charge, cursor, and atomicity tests**

A charge row forecasts a visible charge effect and stores its locked target; it deals no hit until its following eligible enemy phase. Resolution advances the intent cursor exactly once, decrements eligible cooldowns once, and creates the next forecast only after all living enemies finish. Invalid targets, impossible effects, and failed shared-resource updates must not consume charge, cursor, initiative, cooldown, or target locks.

- [ ] **Step 4: Expose only atomic card-rule primitives**

Promote the smallest existing `GameXXKCardRules` helpers needed for clamped damage, armor, healing, status add/remove, temporary modifier add/remove, and shared Qi updates. Each helper validates a copied runtime and commits only on success. The enemy resolver must call these helpers; it must not copy damage, defense, status-cap, or death-cleanup arithmetic into `GameXXKEnemyIntentRules.cpp`.

- [ ] **Step 5: Implement pure forecast plus ordered resolution**

Use this public surface:

```cpp
class FGameXXKEnemyIntentRules
{
public:
    static bool ForecastAll(FGameXXKCardBattleRuntime& Runtime,
        TConstArrayView<FGameXXKEnemyDefinition> Definitions, FString* OutError = nullptr);
    static bool ResolveNext(FGameXXKCardBattleRuntime& Runtime,
        TConstArrayView<FGameXXKEnemyDefinition> Definitions,
        FGameXXKCardEnemyIntent& OutResolved, FString* OutError = nullptr);
    static bool FinishEnemyPhase(FGameXXKCardBattleRuntime& Runtime,
        TConstArrayView<FGameXXKEnemyDefinition> Definitions, FString* OutError = nullptr);
};
```

`ForecastAll` resolves percentages against saved combat stats once and persists an ordered array of explicit effects. `ResolveNext` consumes only the front eligible saved intent. `FinishEnemyPhase` expires one-round enemy modifiers, decrements cooldowns, resets initiative flags, and prepares the next player phase; it does not draw cards itself.

- [ ] **Step 6: Wire the adapter lifecycle**

At battle start and after a complete enemy phase, forecast all living enemies. At End Turn, resolve the entire saved enemy-intent queue in order, emit one resolution record per enemy card for presentation, then call the existing single hand-refresh/draw-to-full flow exactly once after the last enemy finishes. Do not hide the pre-existing intent cards while the player can act.

- [ ] **Step 7: Cold compile, run intent and adapter tests, and commit**

Run `python scripts/ue_tdd_pipeline.py` first. Expected: a successful non-Live-Coding build and editor relaunch with the new DLL. Then use MCP to run `StartsWith:GameXXK.Battle.EnemyIntentRules` and the enemy-phase cases under `GameXXK.Card.BattleAdapter`. Expected green result: target locks, every effect type, simultaneous forecasts, charge, ordering, cursor/cooldown advancement, one refresh, and atomic failures pass.

```powershell
git add Source/GameXXK/Public/GameXXKEnemyIntentRules.h Source/GameXXK/Private/GameXXKEnemyIntentRules.cpp Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp Source/GameXXK/Public/GameXXKCardRules.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Public/GameXXKCardBattleAdapter.h Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp
git diff --check
git commit -m "feat: resolve data-driven enemy intents"
```

---

## Task 7: Implement elite passives, boss phases, and special-case ordering

**Files:**

- Create: `Source/GameXXK/Private/Tests/GameXXKEnemyMechanicsTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKEnemyIntentRules.h`
- Modify: `Source/GameXXK/Private/GameXXKEnemyIntentRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp`

- [ ] **Step 1: Write failing passive-boundary tests**

Assert Ironfeather reduces only the first direct hit it receives in the entire battle by 50%; Bluehorn retains exactly 50% of end-phase armor; Graymane's own direct damage gets the marked multiplier; Redtusk gains one Rage per received direct card hit up to five; Black Bear reduces incoming direct damage 15%; Porcupine Counter triggers once; White Ape gains armor from only the first received status each round; Deer cannot choose its heal during cooldown. Poison, Burn, Bleed, reflected damage, and passive damage must not count as direct hits for first-hit, Rage, or Counter triggers.

- [ ] **Step 2: Write failing boss phase-transition tests**

For Money Rat, Black Bear, and Tiger, cross 50% HP with a card hit and separately with end-phase damage. Assert each phase activates once, persists through save/reload, changes only the catalog-declared effects, and never replays its entry trigger after healing above then falling below 50%. Evaluate the threshold after the complete card/effect packet or damage-over-time tick and before forecasting the next enemy intent.

- [ ] **Step 3: Write failing special-mechanic tests**

Cover all declared special cases: Wealth healing/flat damage and cap; charge target lock; direct-only Rage/Counter; Black Bear added phase hits; White Ape's one-card cost increase chooses a currently playable card and applies at most once per enemy round; Deer heal selects the lowest-health living ally and starts cooldown two; Tiger Prey selects the lowest-health living party member, persists in phase two, and retargets only after that unit dies. Assert no enemy mechanic summons a fourth unit or occupies a second instance of 1P/2P/3P.

- [ ] **Step 4: Define one explicit hook order**

Implement and test this order for each resolved effect packet:

1. validate locked source and targets;
2. apply base card-rule effects atomically;
3. run direct-hit receiver passives;
4. run direct-hit attacker passives;
5. remove defeated units and cancel their unresolved intents;
6. evaluate one-time phase transitions;
7. retarget Tiger Prey if its holder died;
8. publish the resolution record.

At phase end, resolve damage-over-time, remove deaths, evaluate phases, process cooldowns/armor retention/status expiry, then forecast. Reflected/passive damage cannot recursively invoke another Counter or Rage chain.

- [ ] **Step 5: Implement stateful passives behind catalog IDs**

Dispatch through the catalog's passive and phase enums and saved `FGameXXKEnemyBattleState`; do not compare localized names. Store first-hit-used, first-status-this-round, retained-armor remainder, Rage/Wealth, cooldowns, phase-entered, Prey target, and locked charge data explicitly so replay and load are deterministic.

- [ ] **Step 6: Cold compile, run mechanics tests, and commit**

Run `python scripts/ue_tdd_pipeline.py` first. Expected: successful non-Live-Coding build and relaunch with the new DLL. Then use MCP to run `StartsWith:GameXXK.Battle.EnemyMechanics` and `StartsWith:GameXXK.Battle.EnemyIntentRules`. Expected green result: passive boundaries, three phases, charge, cooldown, direct-hit classification, no recursive triggers, target recovery, and save/reload determinism pass.

```powershell
git add Source/GameXXK/Private/Tests/GameXXKEnemyMechanicsTest.cpp Source/GameXXK/Public/GameXXKEnemyIntentRules.h Source/GameXXK/Private/GameXXKEnemyIntentRules.cpp Source/GameXXK/Public/GameXXKCardRules.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp
git diff --check
git commit -m "feat: add enemy passives and boss phases"
```

---

## Task 8: Present all enemy intents and appended statuses with shared text

**Files:**

- Create: `Source/GameXXK/Public/GameXXKEnemyText.h`
- Create: `Source/GameXXK/Private/GameXXKEnemyText.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardText.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardEnemyIntentPresentationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp`

- [ ] **Step 1: Write failing shared-text tests**

For every catalog intent, format a card body and Tooltip from the saved resolved effects. Assert both contain attacker slot (`敌 1P`, `敌 2P`, or `敌 3P`), target scope, hit count, final forecast number, status name/stacks, charge delay, and any active passive/phase qualifier. A multi-target forecast must say `我方全体`; a locked single target must use its display name and player slot. Expected red: current card body is damage-only and UI-specific strings diverge.

- [ ] **Step 2: Write failing simultaneous-intent layout tests**

Build three living enemies and assert the top region creates three horizontal intent cards at once, ordered 1P/2P/3P, with no hidden card before End Turn. Hovering any card returns its own source/effects; resolving one marks only that card resolving/complete while the remaining forecasts stay visible. Defeating an enemy removes only its card and reflows the survivors without changing their locked data.

- [ ] **Step 3: Write failing status-icon tests**

For Medicine, Weak, Wealth, Rage, Prey, Charge, and Counter, assert a nonempty style key, localized display name, exact mechanical Tooltip, stack/counter text when applicable, and the existing icon-background contract. Layer text is drawn over the icon and the foreground glyph fills the available center without a white fringe. Missing approved V1 art uses the existing neutral ink fallback until Task 14 imports the versioned asset; it never resolves to a user-tuned icon package.

- [ ] **Step 4: Implement one formatter for card body and Tooltip**

`FGameXXKEnemyText::FormatIntentCard` and `FormatIntentTooltip` accept only the saved resolved intent plus read-only unit/catalog lookup. Make `GameXXKCardText`, battle board, Tooltip, and automation tests call this formatter. Localized display text is presentation only; resolution never parses it.

- [ ] **Step 5: Bind the top row and status widgets**

Replace the single-intent assumption with a panel keyed by source runtime ID. Rebuild only when the saved intent revision changes. Preserve mouse hover during unrelated status refreshes. Bind appended status styles by enum and show stack count/cooldown on the icon; keep the icon Tooltip interactive anywhere cards or combatants are shown.

- [ ] **Step 6: Cold compile, run presentation tests, and commit**

Run `python scripts/ue_tdd_pipeline.py` first. Expected: successful non-Live-Coding build and relaunch. Then use MCP to run `StartsWith:GameXXK.UI.Battle.EnemyIntent`, the enemy-intent adapter presentation cases, and status-widget cases. Expected green result: all living intent cards, shared text, hover details, source labels, reflow, and seven appended status presentations pass.

```powershell
git add Source/GameXXK/Public/GameXXKEnemyText.h Source/GameXXK/Private/GameXXKEnemyText.cpp Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Private/GameXXKCardText.cpp Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardEnemyIntentPresentationTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp
git diff --check
git commit -m "feat: present complete enemy intents and statuses"
```

---

## Task 9: Drive Codex and battle actors from the enemy catalog

**Files:**

- Create: `Source/GameXXK/Private/Tests/GameXXKRouteEnemyVisualMappingTest.cpp`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCompanionCodexPersistenceTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPartyDeckBattleSceneUnitActorTest.cpp`

- [ ] **Step 1: Write failing catalog-to-Codex tests**

For all 21 enemy IDs, assert Codex lookup returns the catalog's localized name, chapter, tier, intent summary, passive/phase summary, portrait soft path, and discovered/undiscovered state. Legacy Money Rat/Black Bear/Tiger IDs must migrate to the corresponding catalog ID without duplicating a Codex entry. Expected red: the current Codex path knows only hardcoded legacy enemies.

- [ ] **Step 2: Write failing actor visual-resolution tests**

Instantiate a battle actor for each catalog ID and assert it asks the catalog for approved flipbook/sprite soft paths, keeps the explicit slot, and uses the existing neutral missing-visual fallback only when a soft path cannot load. Assert the implementation contains no switch/if chain with 21 enemy names and does not read from PartyDeck generated roots.

- [ ] **Step 3: Extend catalog visual metadata**

Each enemy entry owns `PortraitSoftPath`, directional idle/attack/hit/death soft paths or flipbook set path, visual scale, foot anchor, and Codex description keys. Point the three existing bosses at their current read-only packages. Point the 18 new enemies at `/Game/GameXXK/Characters/RouteEnemies/V1` and `/Game/GameXXK/UI/Codex/RouteEnemies/V1` destinations declared in the visual manifest; unresolved paths remain valid soft references before import.

- [ ] **Step 4: Replace hardcoded lookup with catalog delegation**

Make MVP Codex queries and `GameXXKBattleSceneUnitActor` resolve by `EnemyDefinitionId`. Preserve the existing boss sprites and tuned transforms. Visual load failure logs definition ID and attempted path once, uses the neutral fallback, and never mutates catalog/runtime state.

- [ ] **Step 5: Cold compile, run mapping tests, and commit**

Run `python scripts/ue_tdd_pipeline.py` first. Expected: successful cold build/relaunch. Then use MCP to run `StartsWith:GameXXK.Data.EnemyVisualMapping`, Codex persistence cases, and battle-scene actor cases. Expected green result: 21 Codex entries, three legacy migrations, catalog path delegation, fallback, and explicit slots pass.

```powershell
git add Source/GameXXK/Private/Tests/GameXXKRouteEnemyVisualMappingTest.cpp Source/GameXXK/Private/GameXXKMVPRules.cpp Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp Source/GameXXK/Private/Tests/GameXXKCompanionCodexPersistenceTest.cpp Source/GameXXK/Private/Tests/GameXXKPartyDeckBattleSceneUnitActorTest.cpp
git diff --check
git commit -m "feat: map route enemies through the catalog"
```

---

## Task 10: Give each task NPC a 12-entry personal pool and a locked three-card closed loop

**Files:**

- Modify: `Source/GameXXK/Public/GameXXKCompanionTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCompanionCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCompanionRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKQuestNpcLoopTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKQuestNpcDefaultLoadoutTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKQingshanTaskNpcRouteTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp`

- [ ] **Step 1: Write failing count and ownership tests**

Assert the global catalog remains exactly 174 unique definitions and every task NPC still owns exactly four unique IDs. Add `PersonalPoolCardIds` with exactly 12 entries: each of those four IDs appears exactly three times. Add `DefaultRouteCardIds` with exactly three unique IDs drawn from the same four. The adapter must still materialize exactly hero 8 + permanent partner 5 + task NPC 3 + travel 2 = 18 opening instances. Expected red: no personal-pool field and no loop validator exist.

- [ ] **Step 2: Write failing producer/consumer and default-loop tests**

Tag each of the existing 24 definitions with its produced and consumed loop resources. For each NPC assert at least two of its four definitions produce resources, at least two consume resources, every resource produced by the locked default three has a consumer in that same three, and every default consumer has a producer in that same three. Simulate the default producer before consumer and assert the consumer receives a nonzero benefit. Do not require all 12 repeated personal-pool entries to enter combat.

- [ ] **Step 3: Lock the exact defaults and rewrite the existing four definitions in place**

Use this table; costs not listed as changed stay at their current values. `Produce`/`Consume` are validator metadata backed by the listed real effects, not UI-only labels.

| NPC | Card | Role | Exact loop effect |
| --- | --- | --- | --- |
| 土司首领 | 寨主号令 | Produce Momentum | all allies gain Momentum 1 and armor 3 |
| 土司首领 | 石门守势 | Consume Momentum | selected ally consumes up to 2 Momentum; armor 8 + 6 per stack; draw 1 if 2 consumed |
| 土司首领 | 土司军令 | Produce Momentum | selected ally gains Momentum 2 and MP 3 |
| 土司首领 | 盟寨誓约 | Consume Momentum | owner consumes up to 3 Momentum; all allies gain armor 8 + 3 per stack and MP 2 + 1 per stack |
| 宋金宝 | 赏钱鼓舞 | Produce Momentum | all allies gain Momentum 1 and MP 2 |
| 宋金宝 | 耳目密报 | Produce Momentum | reveal all current enemy intents; owner gains Momentum 1; draw 1 |
| 宋金宝 | 贵客令 | Consume Momentum | owner consumes up to 2 Momentum; all allies gain armor 4 + 3 per stack |
| 宋金宝 | 一诺千金 | Consume Momentum | owner consumes up to 2 Momentum; the next `1 + consumed` shared-deck cards cost 1 less this round |
| 月白 | 青焰点灯 | Produce Burn/Mark | selected enemy gains Burn 3 and Mark 1 |
| 月白 | 残卷批注 | Produce Mark | selected enemy gains Mark 2; reveal its intent; draw 1 |
| 月白 | 月白照夜 | Consume Burn/Mark | attack 140%; consume up to Mark 2 for +35% damage each and Burn 3 for MP 2 each |
| 月白 | 山河残图 | Consume Burn/Mark | selected enemy consumes up to Mark 2 and Burn 4; all allies gain armor 5 per Mark and heal 3 per Burn |
| 周光祖 | 异草辨识 | Produce Medicine | owner gains Medicine 2 and Insight 1 |
| 周光祖 | 黄山敷治 | Produce Medicine | heal selected ally 16; owner gains Medicine 1; terrain bonus remains additive |
| 周光祖 | 地志摹图 | Consume Medicine | owner consumes Medicine 1; draw 2 and double terrain bonus this round |
| 周光祖 | 岩粉封脉 | Consume Medicine | attack 100%; consume owner Medicine up to 2; apply Vulnerability 1 and Poison 2 per stack |
| 金贵 | 市井耳目 | Produce Wealth | all enemies gain Mark 1; owner gains Wealth 1; draw 1 |
| 金贵 | 巧言周旋 | Produce Wealth | selected enemy gains Mark 2; owner gains Wealth 1; all allies gain armor 3 |
| 金贵 | 杂役筹备 | Consume Wealth | owner consumes Wealth up to 2; draw `1 + consumed`, discard 1, gain MP `3 + 2 * consumed` |
| 金贵 | 后巷脱身 | Consume Wealth | owner consumes Wealth up to 2; all allies gain Agility 1 and armor `4 + 3 * consumed` |
| 琼么儿 | 藤桥飞渡 | Produce Agility | preserve terrain targeting; selected/all allies gain Agility 1 and MP 3 |
| 琼么儿 | 蛊雾迷踪 | Produce Poison | selected enemy gains Poison 4 and Mark 1 |
| 琼么儿 | 银铃镇心 | Consume Poison/Agility | consume selected enemy Poison up to 3 and owner Agility 1; lowest-health ally heals 4 per Poison and gains armor 8 + 4 if Agility consumed |
| 琼么儿 | 山歌唤灵 | Consume Poison/Agility | consume selected enemy Poison up to 5 and owner Agility up to 2; all allies heal `8 + 2 * Poison` and gain MP equal to consumed Agility |

Locked default IDs remain compatible with current shipped choices:

| NPC | Locked default three |
| --- | --- |
| 土司首领 | 寨主号令、石门守势、土司军令 |
| 宋金宝 | 耳目密报、赏钱鼓舞、一诺千金 |
| 月白 | 青焰点灯、残卷批注、月白照夜 |
| 周光祖 | 异草辨识、黄山敷治、岩粉封脉 |
| 金贵 | 市井耳目、巧言周旋、杂役筹备 |
| 琼么儿 | 藤桥飞渡、蛊雾迷踪、银铃镇心 |

- [ ] **Step 4: Build the personal pool without changing battle assembly**

At catalog construction, sort the four unique fixed IDs, append each exactly three times to `PersonalPoolCardIds`, and retain the explicit locked default order above. Personal-pool UI may show repeated instances and future randomization may draw from it; `BuildStartingCardInstances` reads only `DefaultRouteCardIds`. Reject an invalid pool/default without modifying the NPC definition.

- [ ] **Step 5: Add static loop validation and save behavior**

`FGameXXKCompanionRules::ValidateQuestNpcLoop` checks ownership, `4/12/3` counts, repetition multiplicity, producer/consumer coverage, and resolvable effect consumption. Old saves retaining one of the shipped defaults load unchanged; a save with an invalid or missing task-NPC selection receives that NPC's locked default, never all 12 entries.

- [ ] **Step 6: Cold compile, run NPC/deck tests, and commit**

Run `python scripts/ue_tdd_pipeline.py` first. Expected: successful cold build/relaunch. Then use MCP to run `StartsWith:GameXXK.Card.QuestNpcLoop`, `StartsWith:GameXXK.Companion.QuestNpcDefaultLoadout`, the Qingshan task-NPC route tests, and adapter deck-count tests. Expected green result: 174 definitions, six `4/12/3` contracts, six closed defaults, real consumption benefits, save migration, and the exact 18-card opening deck pass.

```powershell
git add Source/GameXXK/Public/GameXXKCompanionTypes.h Source/GameXXK/Private/GameXXKCompanionCatalog.cpp Source/GameXXK/Private/GameXXKCompanionRules.cpp Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/Tests/GameXXKQuestNpcLoopTest.cpp Source/GameXXK/Private/Tests/GameXXKQuestNpcDefaultLoadoutTest.cpp Source/GameXXK/Private/Tests/GameXXKQingshanTaskNpcRouteTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp
git diff --check
git commit -m "feat: close task npc card loops"
```

---

## Task 11: Extend the real-rules simulator with a deterministic skilled policy

**Files:**

- Modify: `Source/GameXXK/Public/GameXXKCombatSimulationTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKCombatSimulationRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteBalanceSimulationTest.cpp`

- [ ] **Step 1: Write failing three-unit and multi-enemy simulation tests**

Build a hero + one permanent partner + one task NPC against normal, elite, and boss formations. Assert `RunScenario` begins through `FGameXXKCardBattleAdapter`, sees all saved enemy intents, finishes without a UObject/widget dependency, and produces the same serialized metrics/trace for the same seed. A different seed may change draws/choices but must preserve legal units, explicit slots, resource bounds, and finite round/decision guards. Expected red: the foundation simulator lacks route-enemy metrics and threat-aware decisions.

- [ ] **Step 2: Write failing skilled-policy choice tests**

Create minimal hands with exactly one expected best decision for each priority: prevent forecast lethal with armor/heal; kill an acting enemy to cancel its intent; focus 2P elite/boss when threats are otherwise equal; consume an available NPC loop status; exploit Mark/Burn/Poison/Bleed; avoid overheal and capped status overflow; preserve Qi/MP when a zero-value play is legal; use a beneficial relic/terrain/set interaction; and end phase when no positive play remains. Ties must resolve by acquisition ordinal then stable target ID.

- [ ] **Step 3: Implement one deterministic one-enemy-phase lookahead score**

Enumerate legal `(CardInstanceId, TargetUnitId)` pairs, apply each to a candidate copy through adapter preview/resolve APIs, then estimate the already-forecast enemy phase without mutating the authoritative candidate. Use signed 64-bit score components in this order:

```text
prevent forecast lethal       +1,000,000,000 per saved party member
kill an intent source           +100,000,000 per enemy
reduce forecast party damage         +50,000 per HP
effective enemy health damage         +10,000 per HP
effective ally healing                 +8,000 per HP
armor usable this enemy phase          +6,000 per point
consume a loop/status stack             +4,000 per stack
produce a consumable status             +1,500 per stack
draw / Insight choice                    +1,000 per card
gain MP                                    +500 per point
spend shared Qi                            -750 per point
overheal / capped overflow              -10,000 per point
leave an unconsumed generated loop       -25,000 per stack at battle end
```

Apply a `+5,000` threat-focus tie bonus to the living source with greatest forecast party damage, preferring 2P only when threat is equal. This is a policy heuristic, not combat arithmetic; all actual outcomes still come from the adapter.

- [ ] **Step 4: Resolve pending choices and complete real battle phases**

Use existing forced-discard, Insight, Discover, target selection, End Player Phase, enemy-intent resolution, and hand-refresh APIs. Choice scoring uses the same candidate-copy rule and stable tie breaks. Never reach into private arrays to force a draw, damage a unit, grant armor, or skip a pending choice.

- [ ] **Step 5: Add route and contribution metrics**

Extend metrics/trace with chapter, node kind/ID, enemy definition/slot, party death chapter/node, shared Qi spent/wasted, MP spent/wasted by source, status overflow, loop status produced/consumed, damage-over-time share, and hero/partner/task-NPC damage/healing/armor contributions. Derive deltas from before/after authoritative snapshots and returned resolution records. `FailureReason` distinguishes policy stall, round guard, decision guard, invalid catalog, invalid saved state, and party defeat.

- [ ] **Step 6: Cold compile, run simulator tests, and commit**

Run `python scripts/ue_tdd_pipeline.py` first. Expected: successful non-Live-Coding build and editor relaunch. Then use MCP to run `StartsWith:GameXXK.Simulation.Foundation` and `StartsWith:GameXXK.Simulation.RouteBalance`. Expected green result: deterministic traces, unique best choices, all pending-choice paths, finite guards, contribution accounting, and no direct combat formula duplication pass.

```powershell
git add Source/GameXXK/Public/GameXXKCombatSimulationTypes.h Source/GameXXK/Public/GameXXKCombatSimulationRules.h Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp Source/GameXXK/Private/Tests/GameXXKRouteBalanceSimulationTest.cpp
git diff --check
git commit -m "feat: extend skilled route combat simulation"
```

---

## Task 12: Run the sharded 100-seed balance matrix and emit recommendations only

**Files:**

- Create: `SourceAssets/Balance/route-balance-matrix-v1.json`
- Create: `Source/GameXXK/Public/GameXXKRouteBalanceCommandlet.h`
- Create: `Source/GameXXK/Private/GameXXKRouteBalanceCommandlet.cpp`
- Create: `scripts/run_route_balance_matrix.py`
- Create: `scripts/test_route_balance_matrix.py`
- Modify: `Source/GameXXK/GameXXK.Build.cs`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteBalanceSimulationTest.cpp`

- [ ] **Step 1: Write failing matrix-schema and cardinality tests**

The JSON config must declare schema `1`, levels `[1,5,10,15,20]`, fixed seeds `0..99` under a stable namespace, all six equipment sets, gear fixtures `Naked`, `Common+0`, `Rare+0`, `Epic+0`, `Epic+10`, all six partner roles, all six task NPCs, and all legal formations. Assert formation expansion is exactly 18 normal pairs (`3 * C(4,2)`), 36 elite formations (`3 * 2 * C(4,2)`), and 3 boss formations. Each non-naked band expands to all six sets; naked is one fixture, producing 25 gear fixtures total.

Do **not** form the universal Cartesian product `25 gear * 5 levels * 57 formations * 6 roles * 6 NPCs * 100 seeds`: that would be 25,650,000 real-rule battles before full-route and tuning reruns. Instead, store four explicit versioned cohort schedules in the JSON. A schedule row has a stable row ID and one of the fixed seeds; schedules are data, not runtime random sampling.

| Cohort | Fixed cells | Seed/arm rule | Exact baseline work |
| --- | --- | --- | ---: |
| `BasePressure` | `57 formations * 5 levels = 285` | each cell uses seeds `0..99` once; a checked 100-row orthogonal schedule assigns partner role, task NPC, and one of six `Common+0` sets | `28,500` single-encounter simulations |
| `EquipmentPower` | `24 non-naked fixtures * 5 levels = 120` | each cell uses seeds `0..99` once; every row runs the equipped arm and its exact same-seed naked arm; a checked schedule assigns formation, role, and NPC | `24,000` single-encounter simulations (`12,000` pairs) |
| `NpcContribution` | `6 NPCs * 5 levels = 30` | each cell uses seeds `0..99` once; every row runs that NPC arm and the exact same-seed Common+0 permanent-partner reference arm; a checked schedule assigns formation, role, and Common set | `6,000` single-encounter simulations (`3,000` pairs) |
| `FullRouteGrowth` | one route cohort | exactly one three-chapter route for each seed `0..99`; the 100-row schedule assigns level, role, NPC, and Common set | exactly `100` full-route simulations |

The baseline therefore contains exactly `58,500` single-encounter simulations plus `100` full routes. A current-topology route can contain at most 18 battles across three chapters, so the baseline upper bound is `60,300` effective battle simulations. Catalog integrity, all 21 enemy definitions, and all 57 legal formation shapes are additionally exhaustive pure-data tests and do not multiply the simulation matrix.

Freeze the schedule coverage contract in schema tests:

- every declared cohort row has a unique stable ID and every fixed seed `0..99` appears exactly once in each fixed cell;
- each `BasePressure` cell uses every role, NPC, and Common set either 16 or 17 times, and every role/NPC, role/set, and NPC/set pair either two or three times;
- each `EquipmentPower` fixture/level cell uses every formation once or twice, every role and NPC 16 or 17 times, and every role/NPC pair two or three times; the naked/equipped arms differ only by loadout;
- each `NpcContribution` NPC/level cell uses every formation once or twice, every role and Common set 16 or 17 times, and every role/set pair two or three times; the two arms differ only by the compared support slot;
- the 100 `FullRouteGrowth` rows use each level exactly 20 times, every role/NPC/Common-set value 16 or 17 times, every pair among the three six-value factors two or three times, and every level-to-six-value pair three or four times;
- coverage rows and their SHA-256 are checked in tests. Changing the schedule is a reviewed data change, never an incidental result of iteration order.

- [ ] **Step 2: Write failing process-safety, shard/merge, and no-write Python tests**

Test `--shard-count 16` assigns every stable scenario ID or inseparable paired-arm work item to exactly one shard through a deterministic cost-balanced partition (stable work-weight descending, then stable row ID; ties choose the lowest current projected cost, then shard index). Restart skips only a shard whose config hash and output checksum match, aggregate merge is independent of shard completion order, duplicate/missing scenarios fail, and malformed metrics fail with a precise ID. Assert the full baseline expands to exactly `58,500` single encounters and `100` routes; any attempt to request the universal five-factor Cartesian product fails schema validation. Assert every coverage invariant in Step 1 and a maximum 10-percent projected-cost spread across the 16 frozen shards before a commandlet can launch.

If any `UnrealEditor.exe` or another GameXXK `UnrealEditor-Cmd.exe` is running, the runner must launch zero commandlets and print `GameXXK editor is running; save and close it before balance commandlets`. `--jobs` accepts only `1`; a larger value fails before launch so Zen/D3D/DotNet processes are never shared by concurrent UE instances. The runner creates a short-lived orchestration token only after the process-exit proof; the commandlet refuses a missing, expired, wrong-parent, or config-hash-mismatched token, so the raw commandlet cannot be used as a supported bypass around mutual exclusion. Snapshot SHA-256 for catalog/card/equipment `.h/.cpp/.json` inputs before and after a fake commandlet run; any source mutation fails the runner.

- [ ] **Step 3: Define acceptance cohorts and exact thresholds**

The config separates diagnostics from gates:

- `BasePressure`: Common+0 compatible six-piece set, starting 18-card deck, no route relics/event gains/shop cards, every legal formation/level cell, skilled policy, with role/NPC/set assigned by the approved orthogonal rows. Aggregate by level/chapter/partner role/NPC and require normal `55%..70%`, elite `35%..50%`, boss `15%..35%`.
- `FullRouteGrowth`: actual random maps, reward cards/quality merge, relics, route shop, positive events, three chapter heals, and current settlement-independent combat state. For Common+0 entry equipment require three-chapter clear rate `82%..88%`, whose midpoint is the locked 85% target.
- `EquipmentPower`: compare every non-naked fixture to its exact same-seed, same-level, same-formation, same-role, same-NPC naked arm using the simulator composite of damage, survival, resource efficiency, and status contribution. Require Common `+50%..+70%`, Rare `+135%..+165%`, Epic `+270%..+330%`; Epic+10 is reported separately and must be monotonic above Epic+0.
- `NpcContribution`: compare each task NPC with the exact same-seed, same-level, same-formation, same-role, same-Common-set permanent-partner reference arm. Total contribution ratio must be `85%..115%`, its declared specialty component must be at least `110%` of the reference component, key loop consumption rate `60%..85%`, status overflow below `15%`, and no generated key status may remain unconsumed for the entire battle.

All declared schedule marginals remain diagnostic even when not named as a gate; schema, coverage, legality, pairing, or determinism failures always fail the run. No unlisted high-order Cartesian cell is implied.

- [ ] **Step 4: Implement the no-render commandlet**

Register `UGameXXKRouteBalanceCommandlet`. The following is the runner's internal command template, not a manual verification command:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -run=GameXXKRouteBalance -unattended -nop4 -nosplash -nullrhi -Config='SourceAssets/Balance/route-balance-matrix-v1.json' -ShardIndex=0 -ShardCount=16 -OrchestratedRun='<runner-issued-token>' -Out='Saved/BalanceReports/RouteEnemies/V1/shard-00.json'
```

Expected exit `0`: token, config, coverage, and all assigned scenarios are valid and the shard JSON/CSV is written. Exit nonzero for absent/invalid orchestration token, invalid content, nondeterminism, missing scenario, simulator guard, or write outside the output root. A threshold miss remains a completed measurement and is recorded for the aggregate recommendation phase rather than mutating source data.

- [ ] **Step 5: Implement resumable orchestration and report aggregation**

`scripts/run_route_balance_matrix.py` accepts `--shard-count`, `--jobs 1`, `--resume`, `--mode smoke|full|cohort`, `--cohort`, `--estimate-only`, `--max-estimated-minutes` (default `120`), `--wall-budget-minutes` (default `120`), `--close-editor-safely`, and `--out-dir`. `full` expands only the four exact schedules in Step 1. `smoke` is fixed at 648 single encounters plus two full routes: 432 BasePressure cases (`3` representative tiers * `2` levels * `36` role/NPC pairs * seeds `0,1`), 72 paired EquipmentPower simulations (`3` non-naked representative bands * `3` tiers * `2` levels * `2` seeds * `2` arms), and 144 paired NpcContribution simulations (`6` NPCs * `3` tiers * `2` levels * `2` seeds * `2` arms).

With `--close-editor-safely`, connect through `scripts/ue_mcp_client.py`, call `save_dirty_packages`, require `dirty_after` empty, issue `QUIT_EDITOR`, and poll until both `UnrealEditor.exe` and GameXXK `UnrealEditor-Cmd.exe` are absent. If MCP save/quit or the 120-second process-exit proof fails, stop without force-killing or launching a commandlet. Execute shard commandlets sequentially, one UE process at a time. Write partial outputs only beneath `Saved/BalanceReports/RouteEnemies/V1/`, then aggregate to `summary.json`, `summary.csv`, `failures.csv`, `npc-loops.csv`, `recommendations.json`, and `performance.json`.

Smoke records warm-start, median, p95, and maximum duration separately for single encounters and full routes. Before `full`, project the exact Step-1 cardinality; if projected wall time exceeds 120 minutes, fail before launching the first full shard and print the two measured rates and projected minutes. During a full run the commandlet checks the 120-minute wall budget between scenarios, writes a resumable checkpoint, and exits cleanly with `BudgetExceeded`; the runner never terminates a UE process. The deterministic partition must keep projected shard costs within 10 percent. A completed baseline must remain within the 120-minute budget on the current machine.

- [ ] **Step 6: Generate bounded in-memory tuning recommendations**

For each missed cohort, rerun only that cohort's paired/orthogonal rows with temporary in-memory tier/stat/NPC-effect overrides and perform a deterministic bounded search toward normal `62.5%`, elite `42.5%`, boss `25%`, full route `85%`, equipment `+60/+150/+300%`, and NPC contribution `100%`. Preserve the same schedule and seed IDs; do not expand unrelated dimensions. `recommendations.json` records current value, suggested value, confidence interval, affected cohort, and paired before/after metrics. It never writes C++/JSON catalogs or imports assets; applying a recommendation requires a separately reviewed manual patch and another measured run.

- [ ] **Step 7: Cold compile and run smoke green**

Run `python scripts/ue_tdd_pipeline.py` first. Expected: successful cold build/relaunch with `Json` and `JsonUtilities`. Then use MCP to run `StartsWith:GameXXK.Simulation.RouteBalance`; expected green. Finally run the orchestrator with the safe-close flag. It must save through MCP, request normal editor quit, prove the GUI process exited, and only then start sequential `-nullrhi` commandlets:

```powershell
python scripts/test_route_balance_matrix.py
python scripts/run_route_balance_matrix.py --mode smoke --shard-count 2 --jobs 1 --close-editor-safely --out-dir Saved/BalanceReports/RouteEnemies/V1/Smoke
```

Expected: Python tests pass; exactly 648 smoke single encounters and two smoke routes are present once; report schemas validate; repeated run is byte-identical; source hashes are unchanged; and `performance.json` projects the full baseline at no more than 120 minutes.

- [ ] **Step 8: Run the first full 100-seed matrix and commit the measured infrastructure**

```powershell
python scripts/run_route_balance_matrix.py --mode full --shard-count 16 --jobs 1 --resume --close-editor-safely --max-estimated-minutes 120 --wall-budget-minutes 120 --out-dir Saved/BalanceReports/RouteEnemies/V1/Full
```

Expected: exactly 58,500 baseline single encounters and 100 full routes are present; every fixed cell has the exact 100-seed and coverage guarantees from Step 1; aggregate gates and confidence intervals are present; no simulator/schema/coverage/pairing/determinism failure exists; all threshold misses have recommendation rows; and measured wall time is at most 120 minutes. The editor remains closed throughout all commandlet shards.

```powershell
git add SourceAssets/Balance/route-balance-matrix-v1.json Source/GameXXK/Public/GameXXKRouteBalanceCommandlet.h Source/GameXXK/Private/GameXXKRouteBalanceCommandlet.cpp scripts/run_route_balance_matrix.py scripts/test_route_balance_matrix.py Source/GameXXK/GameXXK.Build.cs Source/GameXXK/Private/Tests/GameXXKRouteBalanceSimulationTest.cpp
git diff --check
git commit -m "test: add sharded route balance matrix"
```

- [ ] **Step 9: Apply reviewed recommendation values manually and close every gate**

Read `recommendations.json` in stable `(Cohort, DefinitionId, Field)` order. For each recommendation with paired-seed confidence at least `0.95`, patch its exact `SuggestedValue` manually with `apply_patch` in `GameXXKEnemyCatalog.cpp`, the six NPC rows in `GameXXKCardCatalog.cpp`, or the planned equipment catalog/affix/set scalar files. Do not change mechanics, IDs, pools, target modes, or visual paths during this tuning step. For a lower-confidence row, expand only the affected paired cohort row from 100 to at most 500 fixed seeds, regenerate its suggestion, and then apply the resulting `>=0.95` value; never expand the universal Cartesian product.

After each reviewed batch, run `python scripts/ue_tdd_pipeline.py`, use MCP to run enemy catalog, NPC loop, equipment, and simulation filters, then invoke only the affected `--mode cohort --cohort <Name>` schedule with `--close-editor-safely`; this saves and normally closes the editor before sequential commandlets. Give each targeted tuning iteration a 30-minute wall budget. Repeat affected cohorts until their locked gates pass and equipment quality remains monotonic, then run the exact full baseline once as the final cross-cohort regression. Store iterations as `Saved/BalanceReports/RouteEnemies/V1/Tuning-01`, then `Tuning-02`, incrementing the two-digit suffix without reuse; never overwrite a prior report.

```powershell
git add Source/GameXXK/Private/GameXXKEnemyCatalog.cpp Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/GameXXKEquipmentCatalog.cpp Source/GameXXK/Private/GameXXKAffixCatalog.cpp Source/GameXXK/Private/GameXXKEquipmentSetCatalog.cpp
git diff --check
git commit -m "balance: tune route enemies npc loops and equipment"
```

Expected: normal `55%..70%`, elite `35%..50%`, boss `15%..35%`, full route `82%..88%`, equipment bands, and NPC contribution/consumption/overflow gates all pass in the final scheduled 100-seed report; source changes came only from reviewed `apply_patch` edits; and no tuning or final run exceeds its declared time budget.

---

## Task 13: Define a tested, non-overwriting visual manifest and import pipeline

**Files:**

- Create: `SourceAssets/RouteEnemies/route-enemy-manifest.json`
- Create: `SourceAssets/RouteEnemies/prompts/route-enemy-prompts-v1.json`
- Create: `scripts/verify_route_enemy_visual_sources.py`
- Create: `scripts/prepare_route_enemy_sprite_atlas.py`
- Create: `scripts/test_route_enemy_visual_manifest.py`
- Create: `scripts/test_route_enemy_sprite_atlas.py`
- Create: `Content/Python/gamexxk_import_route_enemy_sprite_atlases.py`
- Create: `Content/Python/gamexxk_assemble_route_enemy_characters.py`
- Create: `Content/Python/gamexxk_import_route_enemy_portraits.py`
- Create: `Content/Python/gamexxk_import_route_status_icons.py`
- Create: `scripts/test_route_enemy_sprite_import_pipeline.py`
- Create: `scripts/test_route_enemy_character_assembly.py`

- [ ] **Step 1: Write failing manifest contract tests**

Require exactly 18 new monster IDs, the three existing boss reuse records, and seven appended status IDs. Each new monster requires one generated master, one generated battle-facing source, and one derived Codex portrait; each status requires one generated foreground glyph. Therefore the manifest has 43 creative-generation records and 18 derived portrait records. Every generated record contains prompt ID, reference checksums, candidate path, approved path, generation result ID, dimensions, alpha metrics, subject bounds, SHA-256, approval state, and versioned UE destination. Expected red: neither manifest nor validator exists.

- [ ] **Step 2: Lock the exact asset inventory and identity prompts**

Use these prompt identity clauses after the shared style clause; do not substitute another species:

| ID | Identity clause |
| --- | --- |
| Rooster | compact proud farm rooster, cream body, muted red comb and tail, alert martial stance |
| Goat | stocky cream mountain goat, short beard, rounded gray horns, stubborn stance |
| Weasel | slim ochre weasel, dark paws and tail tip, sly low stance |
| Civet | round gray-brown civet, simple dark eye mask and ringed tail |
| Ironfeather | large black-gray fighting rooster, iron-like wing feathers, scarred comb, elite silhouette |
| Bluehorn | large pale goat king, oversized blue-gray curled horns, elite silhouette |
| GrayWolf | lean gray wolf, pale belly, sharp ears, hunting stance |
| Boar | squat brown wild boar, short ivory tusks, braced stance |
| Macaque | small tan macaque, expressive face, one smooth stone in hand |
| Porcupine | round muted-brown porcupine, broad readable quill fan |
| Graymane | large gray wolf king, dark mane, one ear nick, elite silhouette |
| Redtusk | massive dark boar, muted rust-red mane and tusk wraps, elite silhouette |
| Snake | coiled desaturated green-gray snake, triangular head, clear venom fangs |
| Wildcat | lean tawny mountain cat, dark ear tips, low stalking stance |
| Vulture | hunched ash-brown vulture, pale neck ruff, broad readable wings |
| Toad | large round moss-gray toad, pale belly, wide mouth, simple poison sacs |
| WhiteApe | broad white-gray ape, long arms, one stone, elite silhouette |
| Deer | tall pale deer, oversized dark branching pan antlers, elite silhouette |

The seven status clauses are: Medicine `single herb bundle`; Weak `single drooping broken blade`; Wealth `single square-hole coin`; Rage `single flaring horn-shaped flame`; Prey `single brush-ring target eye`; Charge `single tightening spiral horn`; Counter `single returning hooked blade stroke`. Each is one muted blue-gray ink color, flat simplified silhouette, no internal text, no number, no frame; UMG supplies the existing icon base and stack number.

- [ ] **Step 3: Lock the shared image-generation prompt**

Store this base prompt in JSON and concatenate the identity clause and view clause:

```text
Match the approved GameXXK PSD, hero, task-NPC, Money Rat, Black Bear, and Tiger references: simplified cute Q-version Chinese ink-and-light-watercolor game art, low saturation and low contrast, clean readable outer silhouette, large head and compact body, minimal folds and accessories, restrained paper grain, one consistent soft key light, no photorealism, no 3D render, no pixel art, no UI, no card frame, no logo, no writing. Keep the complete subject inside the canvas and fill 78-90% of the usable square. Render on a perfectly flat #ff00ff chroma background with no shadow touching the canvas edge.
```

Master view clause: `neutral three-quarter character master, identity readable at icon size`. Battle view clause: `left-side combat placement, facing right toward the player party, feet/ground anchor level, same identity, proportions, palette and accessories as the approved master`. Status clause replaces character proportions with `single centered flat ink glyph, 82-92% square fill`.

- [ ] **Step 4: Write failing alpha, fill, consistency, and overwrite tests**

Create synthetic good/bad PNG fixtures in the Python test temp directory. Assert approved images are RGBA, all four corner alpha values are `0`, no opaque #ff00ff spill remains, nontransparent bounds fill `78%..92%` of width/height for monsters and `82%..94%` for status glyphs, no edge is clipped, and derived portraits reference an approved master checksum. Compare master/battle palette histograms and perceptual identity hashes within manifest tolerances. Reject any approved record missing human approval metadata or any UE destination under existing boss/PartyDeck roots.

- [ ] **Step 5: Implement deterministic preparation without creative redraw**

`verify_route_enemy_visual_sources.py` validates manifest/checksums/QC and writes a read-only contact-sheet report. `prepare_route_enemy_sprite_atlas.py` only trims transparent padding, centers to fixed canvas, normalizes ground anchors, derives the portrait crop from the approved master, and writes import metadata; it never paints, inpaints, or changes identity. The generated battle-facing source is the authoritative combat image.

- [ ] **Step 6: Make UE import versioned and fail closed**

Import scripts accept manifest path plus `--dry-run`. Dry run lists exact source/destination pairs and fails if a destination exists with a different checksum, if approval is false, or if any source is outside `approved/v1`. Exact matching existing V1 packages are skipped; no package is overwritten. A changed approved image requires a new untouched `V2` manifest/root. Assembly creates sprites/flipbooks/character data only beneath the declared `/Game/GameXXK/.../V1` roots and saves through UE MCP editor Python.

- [ ] **Step 7: Run Python red-to-green tests and commit tooling**

```powershell
python scripts/test_route_enemy_visual_manifest.py
python scripts/test_route_enemy_sprite_atlas.py
python scripts/test_route_enemy_sprite_import_pipeline.py
python scripts/test_route_enemy_character_assembly.py
```

Expected green result: exact inventory, prompt IDs, approval gates, alpha/fill/identity checks, deterministic portrait derivation, version roots, dry-run output, and overwrite refusal pass. No raster candidate or UE asset is created by this task.

```powershell
git add SourceAssets/RouteEnemies/route-enemy-manifest.json SourceAssets/RouteEnemies/prompts/route-enemy-prompts-v1.json scripts/verify_route_enemy_visual_sources.py scripts/prepare_route_enemy_sprite_atlas.py scripts/test_route_enemy_visual_manifest.py scripts/test_route_enemy_sprite_atlas.py Content/Python/gamexxk_import_route_enemy_sprite_atlases.py Content/Python/gamexxk_assemble_route_enemy_characters.py Content/Python/gamexxk_import_route_enemy_portraits.py Content/Python/gamexxk_import_route_status_icons.py scripts/test_route_enemy_sprite_import_pipeline.py scripts/test_route_enemy_character_assembly.py
git diff --check
git commit -m "build: add guarded route enemy visual pipeline"
```

---

## Task 14: Generate, approve, and import the V1 monster/status art

**Files:**

- Create after approval: `SourceAssets/RouteEnemies/candidates/v1/*`
- Create after approval: `SourceAssets/RouteEnemies/approved/v1/*`
- Modify after approval: `SourceAssets/RouteEnemies/route-enemy-manifest.json`
- Create through UE import: assets only under `/Game/GameXXK/Sprites/Generated/RouteEnemies/V1`, `/Game/GameXXK/Characters/RouteEnemies/V1`, `/Game/GameXXK/UI/Codex/RouteEnemies/V1`, and `/Game/GameXXK/UI/Battle/Status/V1`

- [ ] **Step 1: Re-read the `imagegen` skill and inspect all visual references**

Before any generation, read `C:\Users\shxuw\.codex\skills\.system\imagegen\SKILL.md` completely. Use `view_image` on the current PSD preview/cuts and approved hero, six task NPC, Money Rat, Black Bear, and Tiger source images. Record exact local reference paths and SHA-256 values in the manifest; do not infer style only from prose.

- [ ] **Step 2: Generate 18 master candidates one asset at a time**

Call built-in `image_gen` once for each monster master using the shared prompt, identity clause, and only the minimum relevant local reference paths. Do not use `num_last_images_to_include` when all references have local paths. Save each returned bitmap immediately to its unique `SourceAssets/RouteEnemies/candidates/v1/<id>_master_chroma.png`; if that path exists, stop and allocate another candidate revision instead of overwriting it. Present each result through `generatedImage(result)` during the production session.

- [ ] **Step 3: Generate 18 battle-facing candidates as reference edits**

For each monster, call built-in `image_gen` once with its generated master plus the approved style references and the battle-facing clause. Identity, colors, proportions, horn/ear/tail details, and elite silhouette must match its master. Save to a unique `<id>_battle_chroma.png`. This is a distinct image-editing call for each distinct battle asset; no Python redraw or horizontal mirroring substitutes for generation.

- [ ] **Step 4: Generate seven status foreground glyphs**

Call built-in `image_gen` once per status clause. Use the approved V4 one-color, low-saturation, simplified ink status style as the primary reference. Save unique chroma candidates. The result contains only the foreground glyph; the middle remains free of text/number and UMG supplies the icon base.

- [ ] **Step 5: Remove chroma and run automated QC without overwriting**

For every candidate, write a new `_alpha.png` path with the installed helper:

```powershell
python 'C:\Users\shxuw\.codex\skills\.system\imagegen\scripts\remove_chroma_key.py' --input '<candidate>_chroma.png' --out '<candidate>_alpha.png' --key-color '#ff00ff' --soft-matte --transparent-threshold 18 --opaque-threshold 72 --edge-feather 1 --edge-contract 1 --spill-cleanup
python scripts/verify_route_enemy_visual_sources.py --manifest SourceAssets/RouteEnemies/route-enemy-manifest.json --candidate-root SourceAssets/RouteEnemies/candidates/v1
```

Expected: transparent corners, no fringe, fill/clip bounds pass, and master/battle identity report is within tolerance. If fur, feathers, antlers, or quills fail chroma extraction, show the failure and obtain explicit user approval before using the optional `gpt-image-1.5` CLI transparency fallback described by the skill.

- [ ] **Step 6: Present contact sheets and obtain explicit approval**

Create deterministic contact sheets grouped by chapter (`master | battle | derived portrait`) and one status sheet. Use `view_image` to inspect at original detail, then show the sheets to the user. Record per-asset `approved/revise/reject`, approver, timestamp, prompt ID, generation result ID, and checksum. Stop here until the user explicitly approves; an unapproved candidate cannot enter `approved/v1` or UE.

- [ ] **Step 7: Promote only approved checksums into the V1 source root**

Copy each approved alpha image to its exact manifest `approved/v1` path, derive portraits through `prepare_route_enemy_sprite_atlas.py`, and update the manifest checksums/approval metadata. Refuse a different file at an existing approved path. The three boss records continue pointing to existing read-only assets; if validation exposes a missing boss portrait/orientation/phase image, create a separate candidate and obtain separate approval before filling only that missing V1 destination.

- [ ] **Step 8: Dry-run, import through UE MCP, and save new packages**

With the editor running and MCP available, execute each import/assembly script first with `--dry-run`; expected: 18 monster sources, 18 portraits, seven glyphs, and no overwrite. Execute the real scripts through focused editor Python via UE MCP, save only the new V1 packages, and rerun the import scripts; expected: exact-checksum assets are skipped and no package changes.

- [ ] **Step 9: Cold cycle, run mapping/visual tests, and commit approved art**

Run `python scripts/ue_tdd_pipeline.py` first so the editor reloads all code and packages. Then use MCP to run `StartsWith:GameXXK.Data.EnemyVisualMapping` and the route-enemy import/assembly filters. Expected green result: every new monster loads its V1 battle visual and portrait, every appended status loads its V1 glyph, three bosses retain existing references, bounds/anchors are valid, and no pre-existing package checksum changed.

```powershell
git add SourceAssets/RouteEnemies/candidates/v1 SourceAssets/RouteEnemies/approved/v1 SourceAssets/RouteEnemies/route-enemy-manifest.json Content/GameXXK/Sprites/Generated/RouteEnemies/V1 Content/GameXXK/Characters/RouteEnemies/V1 Content/GameXXK/UI/Codex/RouteEnemies/V1 Content/GameXXK/UI/Battle/Status/V1
git diff --check
git commit -m "art: import approved route enemy visuals"
```

---

## Task 15: Generate and bind six profession partners and six identity-locked task NPC visual suites

**Files:**

- Create: `SourceAssets/CharacterVisuals/character-visual-manifest.json`
- Create: `SourceAssets/CharacterVisuals/prompts/character-visual-prompts-v1.json`
- Create: `scripts/verify_character_visual_sources.py`
- Create: `scripts/prepare_character_visual_atlases.py`
- Create: `scripts/test_character_visual_manifest.py`
- Create: `scripts/test_character_visual_atlases.py`
- Create: `Content/Python/gamexxk_import_character_visuals.py`
- Create: `Content/Python/gamexxk_assemble_character_paperzd.py`
- Create: `scripts/test_character_visual_import_pipeline.py`
- Create: `scripts/test_character_paperzd_assembly.py`
- Create: `Source/GameXXK/Public/GameXXKCharacterVisualCatalog.h`
- Create: `Source/GameXXK/Private/GameXXKCharacterVisualCatalog.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKCharacterVisualMappingTest.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCompanionCatalog.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPartyDeckBattleSceneUnitActorTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTaskNpcCodexWidgetTest.cpp`

- [ ] **Step 1: Write failing 12-identity manifest and atlas tests**

Require six profession-shared identities (`Blade`, `Guard`, `Healer`, `Hunter`, `Sorcerer`, `FormationMaster`) and six named task-NPC identities (`TusiChief`, `SongJinBao`, `YueBai`, `ZhouGuangZu`, `JinGui`, `QiongMeiEr`). Every identity has three creative records: standing master, 4-by-2 eight-direction sheet, and four-frame battle Idle sheet, for exactly 36 image-generation records. Each also has a derived half-body card/Codex portrait, eight directional frame records, four Idle frame records, PaperZD destination, and semantic keys. Expected red: manifest and character pipeline do not exist.

- [ ] **Step 2: Lock directions, canvas, identity, and profession sharing**

Direction cells read left-to-right, top-to-bottom as `South, SouthWest, West, NorthWest, North, NorthEast, East, SouthEast`; each cell normalizes to `512x512`. Battle Idle is four horizontal `512x512` frames facing left because party units stand on the right. Standing master and derived card portrait normalize to `2048x2048` and `1024x1024`. Partner manifest rows bind every one of the 12 possible roster members of a profession to the same suite; names, level, star, stats, and random personal deck never choose another visual.

Use these profession identity clauses with a youthful, compact, visually androgynous Q character so the shared identity does not imply a name-specific biography:

| Profession | Locked visual identity |
| --- | --- |
| Blade | agile dao wanderer, short teal-gray coat, cloth waist sash, one simple single-edged saber |
| Guard | sturdy village guard, muted ochre padded coat, small round shield and short spear |
| Healer | gentle herbal physician, pale sage robe, compact herb satchel, one medicine gourd |
| Hunter | nimble archer, muted brown-green short cloak, small bow and compact quiver |
| Sorcerer | talisman caster, desaturated blue-gray robe, one wooden wand and two plain paper charms |
| FormationMaster | young scholar tactician, warm gray robe, folded fan and one compact formation flag |

Task-NPC prompts do not invent a generic face. Each uses that named NPC's current approved original/card art as the primary local reference and explicitly preserves face, hair/headwear, clothing palette, body proportion, and signature prop while simplifying excess folds/accessories into the shared Q ink style.

- [ ] **Step 3: Implement manifest/QC/import tests to green before generation**

The same alpha, chroma-spill, clipping, checksum, approval, and no-overwrite rules from Task 13 apply. Add per-cell occupancy checks: direction/Idle subject height `72%..92%`, no transparent cell, common ground anchor within 3% of canvas height, master/card identity perceptual match, and all frames of a sheet within the approved palette tolerance. Import dry run must list exactly 12 masters, 12 direction atlases, 12 Idle atlases, 12 card/Codex portraits, 96 direction sprites, 48 Idle sprites, 12 PaperZD animation/character sets, and no legacy package writes.

```powershell
python scripts/test_character_visual_manifest.py
python scripts/test_character_visual_atlases.py
python scripts/test_character_visual_import_pipeline.py
python scripts/test_character_paperzd_assembly.py
```

Expected: all synthetic fixture tests pass while the manifest's real assets remain `candidate-missing` and therefore not importable.

- [ ] **Step 4: Re-read `imagegen`, inspect references, and generate 12 standing masters**

Read `C:\Users\shxuw\.codex\skills\.system\imagegen\SKILL.md` completely again at production time. Inspect PSD cuts, hero, current six profession art, and each named NPC original with `view_image`. Call built-in `image_gen` once per standing master on flat #ff00ff, using the shared low-saturation simplified Q ink-watercolor prompt and the appropriate identity references. Save unique candidates under `SourceAssets/CharacterVisuals/candidates/v1`; never overwrite a revision.

- [ ] **Step 5: Generate 12 eight-direction sheets and 12 battle Idle sheets**

For each approved-shape master candidate, make one distinct reference-edit call for its direction sheet and one for its Idle sheet. Direction prompt requires the exact 4-by-2 order, unchanged face/clothes/prop/proportions, consistent orthographic scale, separated cells, flat chroma, and no labels. Idle prompt requires four subtle breathing/cloth-shift frames facing left, identical feet anchor, no translation, no attack pose, no text. If a cell duplicates a direction, changes identity, clips a prop, or crosses a grid boundary, reject the full sheet and generate a new candidate revision through `image_gen`; do not repair it by painting or mirroring.

- [ ] **Step 6: Remove chroma, derive card portraits, and obtain explicit per-suite approval**

Run the installed `remove_chroma_key.py` with the Task 14 soft-matte parameters into unique alpha paths, then run all character visual validators. Derive half-body card/Codex portraits only by deterministic crop/resize from the alpha-approved standing master. Present two contact sheets: six professions and six named NPCs, with `master | eight directions | Idle | card portrait`. Record approval for all three generated sources and the derived portrait per identity. Stop until the user explicitly approves; approval of one identity does not approve another.

- [ ] **Step 7: Promote approved suites and assemble new V1 PaperZD/Paper2D assets**

Copy only approved checksums into `SourceAssets/CharacterVisuals/approved/v1`, dry-run both UE scripts, then execute them through focused UE MCP editor Python. Create all textures, sprites, flipbooks, PaperZD animation sources/sequences/character data, standing portraits, and card textures only beneath the V1 roots. Match existing character pixel/unit scale and foot pivots without changing placed actors, camera transforms, HD2D planes, or legacy PaperZD assets. A destination mismatch aborts; a future revision uses V2.

- [ ] **Step 8: Write failing semantic binding tests**

Before changing resolvers, assert all six `Portrait.Companion.<Role>` keys, all six `Source.Role.Profession.<Role>` keys, and all six `Source.Identity.Npc.<Name>` keys resolve to V1 assets. Every same-role recruitment template resolves the same standing/PaperZD/card suite; every named NPC resolves only its own suite. Hero retains its current fixed art. Battle actor, character sheet/roster, task-NPC Codex, town HUD, and every profession/NPC card must resolve through the catalog, not the legacy hardcoded PartyDeck path. Expected red: current widgets and battle actor still contain hardcoded legacy constants.

- [ ] **Step 9: Implement the character visual catalog and bind all consumers atomically**

`FGameXXKCharacterVisualDefinition` stores identity key, standing portrait, card/Codex texture, direction PaperZD character data, battle Idle flipbook/sequence, scale, and pivot. `FGameXXKCharacterVisualCatalog` exposes lookup by companion role, task-NPC ID, semantic source-art key, and portrait key. Replace widget/actor hardcoded maps with catalog calls only after `ValidateAllApprovedV1()` proves all 12 suites load; otherwise retain the complete legacy mapping for that session and log one validation error, never mix old/new art within one identity.

- [ ] **Step 10: Cold compile, run mapping/assembly tests, inspect PIE, and commit**

Run `python scripts/ue_tdd_pipeline.py` first. Expected: successful cold build/relaunch with all new V1 packages. Then use MCP to run `StartsWith:GameXXK.Data.CharacterVisualMapping`, PartyDeck scene-actor, card-board portrait, companion roster, and task-NPC Codex filters. In PIE inspect one recruited partner of each role and all six task NPCs in battle/card/Codex presentation; expected: approved Q identity, correct 8-direction source/PaperZD data, Idle animation, card portrait, transparent clean edges, fixed feet, no role variant drift, and unchanged hero/legacy assets.

```powershell
git add SourceAssets/CharacterVisuals/character-visual-manifest.json SourceAssets/CharacterVisuals/prompts/character-visual-prompts-v1.json SourceAssets/CharacterVisuals/candidates/v1 SourceAssets/CharacterVisuals/approved/v1 scripts/verify_character_visual_sources.py scripts/prepare_character_visual_atlases.py scripts/test_character_visual_manifest.py scripts/test_character_visual_atlases.py Content/Python/gamexxk_import_character_visuals.py Content/Python/gamexxk_assemble_character_paperzd.py scripts/test_character_visual_import_pipeline.py scripts/test_character_paperzd_assembly.py Source/GameXXK/Public/GameXXKCharacterVisualCatalog.h Source/GameXXK/Private/GameXXKCharacterVisualCatalog.cpp Source/GameXXK/Private/Tests/GameXXKCharacterVisualMappingTest.cpp Source/GameXXK/Private/GameXXKCompanionCatalog.cpp Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp Source/GameXXK/Private/Tests/GameXXKPartyDeckBattleSceneUnitActorTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKTaskNpcCodexWidgetTest.cpp Content/GameXXK/Characters/Generated/Partners/V1 Content/GameXXK/Characters/Generated/TaskNpc/V1 Content/GameXXK/Animations/Generated/CharacterVisuals/V1 Content/GameXXK/UI/Generated/CharacterVisuals/V1
git diff --check
git commit -m "art: bind approved partner and task npc suites"
```

---

## Task 16: Prove the complete three-chapter flow through player-facing PIE actions

**Files:**

- Create: `Content/Python/gamexxk_probe_three_chapter_route.py`
- Create: `scripts/gamexxk_three_chapter_route_acceptance.py`
- Create: `scripts/test_gamexxk_three_chapter_route_acceptance.py`
- Modify: `scripts/gamexxk_real_play_flow_mcp.py`

- [ ] **Step 1: Write failing offline acceptance-contract tests**

Assert the runner refuses a missing MCP connection, stale PIE world, active modal from another scenario, missing screenshot/report, direct runtime-write action, or report whose saved chapter/node/slot/intent IDs disagree. Assert timeout/error cleanup releases mouse focus and never clicks a hidden command. Expected red: three-chapter probe/runner do not exist.

- [ ] **Step 2: Implement a read-only probe and player-action adapter**

The probe returns JSON for screen, chapter/root/chapter seeds, route snapshot, visible/reachable nodes, route resources, relics, deck count/qualities, party units/slots/HP/MP/statuses, enemies/slots/definitions/levels/states, all forecast intents and Tooltip text, pending card/reward/target/modal state, current mouse-focus owner, and current visual asset paths. It may call public player controller/widget methods equivalent to visible mouse/key actions: Start/New Game, `F` quest interaction, town/route button, reachable node button, card click, target click, End Turn, reward choice, event choice, merchant exit, and modal close. It must not obtain a mutable runtime reference, call `BeginBattle`/`ResolveBattleVictory`/`ResolveBossClear`, grant damage/currency/cards, teleport actors, or write a save field.

- [ ] **Step 3: Drive three deterministic presentation fixtures through the normal UI**

Use project test maps/fixtures that initialize only their documented starting save before PIE and then expose the same player UI, never a mid-scenario mutation hook:

1. normal chapter fixture: two distinct normal enemies at 1P/3P and empty 2P;
2. elite fixture: elite 2P plus distinct normals 1P/3P;
3. chapter boss fixtures: Money Rat/Black Bear/Tiger at 2P with that chapter's two elites at 1P/3P.

For each, use visible card/target/end-turn actions to play at least two player rounds. Assert HP/MP/Qi/status data in the HUD equals probe state; all living enemy intents remain visible before End Turn; hovering each card yields source slot/target/hits/final effect/passive/phase; each enemy card presents before its real effects resolve; and a dead source's unresolved intent disappears.

- [ ] **Step 4: Drive one complete route without bypassing nodes or battles**

Starting from `L_Main`, click New Game, enter `L_QingshanInn`, press the public `F` interaction to accept the task NPC, enter the random route, and select only currently reachable node buttons. For every battle, the runner selects cards/targets and ends phases through the same UI using the deterministic skilled policy; it resolves rewards/events/shop exits only through visible buttons. It cannot mark a node complete or invoke victory directly. Continue through all three bosses.

Assert chapter 1 and 2 boss rewards lead to a fresh current-topology map, full hero/partner/NPC HP+MP, cleared battle status/armor, and preserved deck/relics/travel money/event attributes/acquisition count. Assert chapter 3 produces exactly one settlement receipt and the locked success conversions. Capture node type, formation, battle rounds, reward, and chapter transition in the report.

- [ ] **Step 5: Exercise phases, NPC loops, hover, and click recovery**

During fixture battles, drive each boss across 50% and capture before/after screenshots and probe records proving a one-time phase change. Play one producer then consumer for each of the six task NPCs and prove actual stack consumption/benefit. Hover enemy intent, player card, status icon, Codex portrait, and card reward. After card reward, event choice, merchant exit, boss transition, and final settlement, click an exposed next action; assert no transparent full-screen layer remains and mouse-focus owner is either the expected widget or empty.

- [ ] **Step 6: Save exact evidence and make the runner self-verifying**

Write only beneath `Saved/Automation/ThreeChapterRoute/<RunId>/`: `report.json`, `actions.jsonl`, `state-snapshots.jsonl`, and numbered PNGs for formation, intent row, status Tooltip, each boss phase, chapter transitions, and settlement. Every screenshot record stores SHA-256 plus state revision. The runner exits nonzero on any mismatch and prints the failed scenario/action/state IDs.

- [ ] **Step 7: Cold cycle, run offline tests, then run PIE acceptance**

Run `python scripts/ue_tdd_pipeline.py` first. Expected: successful cold build/relaunch and PIE. Then run:

```powershell
python scripts/test_gamexxk_three_chapter_route_acceptance.py
python scripts/gamexxk_three_chapter_route_acceptance.py --project 'D:\UE5 demo\GameXXK\GameXXK.uproject' --output-root Saved/Automation/ThreeChapterRoute
```

Expected: offline tests pass; the live runner reports all formation, UI/rule parity, task-NPC loop, boss phase, chapter transition, full-route, settlement, hover, and click-recovery scenarios passed without runtime-write actions.

- [ ] **Step 8: Commit the reusable acceptance harness**

```powershell
git add Content/Python/gamexxk_probe_three_chapter_route.py scripts/gamexxk_three_chapter_route_acceptance.py scripts/test_gamexxk_three_chapter_route_acceptance.py scripts/gamexxk_real_play_flow_mcp.py
git diff --check
git commit -m "test: add three-chapter player flow acceptance"
```

---

## Task 17: Run the final cold regression, balance, visual-integrity, and evidence gate

**Files:**

- Create: `docs/verification/2026-07-22-three-chapter-route-enemies-balance.md`

- [ ] **Step 1: Audit the exact working set before verification**

```powershell
git status --short
git diff --check
git diff --stat -- Source/GameXXK SourceAssets/Balance SourceAssets/RouteEnemies SourceAssets/CharacterVisuals Content/Python scripts docs/superpowers/plans/2026-07-22-three-chapter-route-enemies-balance.md
```

Expected: only files named by this plan plus pre-existing user changes appear; no existing sprite, PaperZD, map, camera, HD2D plane, PartyDeck art, or boss package is modified. Stop and separate any unrelated hunk before continuing.

Before Step 2, treat the rebased route-merchant implementation as a hard prerequisite. Verify all three conditions in the final code and focused tests: it no longer assigns global save version `7`; its base route recipe uses exactly hero 8 + permanent partner 5 + task NPC 3 + travel 2 and never appends a companion's 12-card personal pool; and only the chapter-3 terminal path settles/clears route-local state while chapter-1/2 Boss clears transition and preserve growth. If any condition is missing, ambiguous, or failing, stop verification and return to the route-merchant rebase.

- [ ] **Step 2: Run all offline Python/tooling tests**

```powershell
python scripts/test_route_balance_matrix.py
python scripts/test_route_enemy_visual_manifest.py
python scripts/test_route_enemy_sprite_atlas.py
python scripts/test_route_enemy_sprite_import_pipeline.py
python scripts/test_route_enemy_character_assembly.py
python scripts/test_character_visual_manifest.py
python scripts/test_character_visual_atlases.py
python scripts/test_character_visual_import_pipeline.py
python scripts/test_character_paperzd_assembly.py
python scripts/test_gamexxk_three_chapter_route_acceptance.py
python scripts/harness_state_validator.py
```

Expected: every process exits `0`, candidate/approved checksums agree, versioned imports are idempotent skips, protected assets retain pre-plan hashes, and production-unit files validate.

- [ ] **Step 3: Perform the final cold compile before any MCP automation**

```powershell
python scripts/ue_tdd_pipeline.py
```

Expected: MCP saves dirty packages, editor closes, UBT builds without Live Coding/Hot Reload, editor opens directly from `D:\UE5 demo\GameXXK\GameXXK.uproject`, PIE starts, and the script exits `0`.

- [ ] **Step 4: Run the complete loaded-DLL automation suite through MCP**

Run the shared MCP helper for these filters after the cold editor is live:

```text
StartsWith:GameXXK.Route
StartsWith:GameXXK.Card.RouteLifecycle
StartsWith:GameXXK.Integration.CardBattleAdapter
StartsWith:GameXXK.Battle.Enemy
StartsWith:GameXXK.Card.QuestNpcLoop
StartsWith:GameXXK.Simulation
StartsWith:GameXXK.Data.Enemy
StartsWith:GameXXK.Data.CharacterVisualMapping
StartsWith:GameXXK.Equipment
StartsWith:GameXXK.MVP.SaveGame
StartsWith:GameXXK.MVP.SaveGame.ThreeChapterVersionMigration
StartsWith:GameXXK.Equipment.SaveMigration
```

Expected: zero failed tests and no ensure/assert/crash in the current session log. Save migration proves route merchant no longer owns global version 7 and exercises a real unique temporary main slot through the dynamic `<SlotName>.PreV<TargetVersion>Backup[.NNN]` chain: the selected attempt backup reloads with the original source checksum, pre-existing mismatched backups remain byte-identical and force a numbered non-overwriting attempt, the migrated main slot passes write/reload serialization equality, and an already-current slot is idempotent without a new backup. Failure injection at backup, dispatcher, validation, main-write, and post-write reload/equality boundaries must prove the main slot and live `RuntimeState` remain equal to their pre-call values; after a main write, rollback must use only that attempt's checksum-matched backup and reload-verify the original main checksum. Route lifecycle/card-adapter tests prove the exact `8 + 5 + 3 + 2 = 18` base recipe; chapter-transition tests prove only chapter 3 settles or clears the run. Any missing real-slot proof is a hard stop.

- [ ] **Step 5: Save and normally close the editor, then rerun the final full balance matrix alone**

Invoke the safe-close orchestrator; it must prove `UnrealEditor.exe` exited before launching a single sequential commandlet process:

```powershell
python scripts/run_route_balance_matrix.py --mode full --shard-count 16 --jobs 1 --resume --close-editor-safely --max-estimated-minutes 120 --wall-budget-minutes 120 --out-dir Saved/BalanceReports/RouteEnemies/V1/Final
```

Expected: no GUI/commandlet overlap; exactly 58,500 single encounters and 100 full routes satisfy the frozen coverage schedules; measured wall time is at most 120 minutes; no invalid/nondeterministic/coverage/pairing/guard failure exists; every locked win-rate/equipment/NPC gate passes; source hashes remain unchanged; and repeated aggregate output is byte-identical.

- [ ] **Step 6: Reopen through the cold pipeline and rerun player-facing PIE acceptance**

```powershell
python scripts/ue_tdd_pipeline.py
python scripts/gamexxk_three_chapter_route_acceptance.py --project 'D:\UE5 demo\GameXXK\GameXXK.uproject' --output-root Saved/Automation/ThreeChapterRoute/Final
```

Expected: successful cold reopen and every Task 16 scenario passes against the final tuned catalogs/assets. Inspect the numbered screenshots at original resolution before recording visual acceptance.

- [ ] **Step 7: Write the verification record from actual outputs**

Create the verification markdown with exact commit hashes, global save version, cold-build result, MCP filter totals, final balance report checksum and gate table, PIE report checksum/scenario table, approved visual manifest checksums, protected-asset before/after hashes, screenshot links, and any non-blocking observation explicitly separated from acceptance. Do not copy expected values into the result column unless the corresponding command/report proves them.

- [ ] **Step 8: Commit the final evidence and run one final consistency check**

```powershell
git add docs/verification/2026-07-22-three-chapter-route-enemies-balance.md
git diff --check
git commit -m "docs: verify three-chapter route balance"
git status --short
```

Expected: verification is committed, no named plan file is missing, and remaining dirty entries are only previously identified user-owned changes.

## Final acceptance checklist

- [ ] Existing random route-map topology runs as three deterministic chapters with Money Rat, Black Bear, then Tiger.
- [ ] Normal/elite/boss formations use explicit 1P/2P/3P, correct snapshot levels, unique chapter pools, and stable reload identity.
- [ ] All 21 enemies have validated stats, intents, passives/phases, Codex data, battle visuals, and truthful all-live intent Tooltips.
- [ ] Opening deck remains exactly `8 + 5 + 3 + 2 = 18`; global definitions remain 174; each NPC exposes `4 unique / 12 repeated personal / 3 locked battle` and closes its loop.
- [ ] Chapter transitions heal hero/partner/NPC, clear battle-local effects, and preserve every route-growth field.
- [ ] Clear/fail/abandon settlements convert at the locked ratios exactly once and survive replay/reload safely.
- [ ] Base-pressure, full-route 85% target band, equipment `+60/+150/+300%`, and NPC contribution/consumption gates pass the final paired 100-seed matrix.
- [ ] Six profession partners share six approved simplified-Q V1 identities; six task NPCs retain six distinct approved identities; all have standing art, eight directions, battle Idle, card/Codex art, and new PaperZD assembly.
- [ ] Eighteen new monster suites and seven one-color low-saturation ink status glyphs are approved/imported; existing three bosses and all protected user assets remain unchanged.
- [ ] Player-facing PIE proves formation, intents, target selection, damage/status state, phases, chapters, rewards, settlement, hover Tooltips, and post-modal mouse recovery.
