# Protagonist 36-Card Runtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the approved 36-card protagonist pool, level-based unlock migration, and all shared combat mechanics it depends on while preserving the current inventory and battle UI layout.

**Architecture:** Extend the existing data-driven card catalog with append-only serialized enums and declarative hero metadata; do not dispatch gameplay by `CardId`. Refactor active-card resolution into an origin-aware, transactional queue so automatic replays, task rewards, Heavy Arrow, reactions, and terrain listeners reuse the shared effect and damage resolvers without recursively counting as active plays. Persist only the state that can cross an existing card-choice interruption, and evaluate victory once at the end of each complete combat event queue.

**Tech Stack:** Unreal Engine 5.8, C++/USTRUCT SaveGame state, Unreal Automation Tests, UBT, existing UMG battle widgets, PowerShell, Git on `main`.

---

## Authoritative input and non-negotiable boundaries

- Implement every value and ordering rule in `docs/superpowers/specs/2026-08-10-protagonist-card-pool-design.md`.
- Work in the root project on `main`; do not create a worktree or feature branch.
- Keep the existing inventory and battle layouts. The only UI behavior change is reusing the current pending-choice panel for Mage search and replacing status/card wording in existing tooltip surfaces.
- Do not implement talent or title pages in this plan; their tabs and layouts have not been approved.
- Do not touch or stage the existing untracked `Build/`, `Packaged/`, `SourceArt/`, `SourceAssets/`, `outputs/`, or `tmp/` content.
- Append serialized enum values only. Never renumber an existing `EGameXXKCardStatus`, `EGameXXKCardEffectType`, `EGameXXKCardZone`, pending-choice kind, damage cause, or modifier trigger.
- No runtime `if (CardId == ...)` branches. Card identity belongs in catalog data; runtime behavior is selected by effect, trigger, Heavy Arrow, task-reward, or linked-role descriptors.
- No Live Coding or Hot Reload verification. Every GREEN checkpoint requires a cold UBT followed by a new `UnrealEditor-Cmd` process and an inspected `index.json` report.
- Before a cold build, run `Get-Process UnrealEditor -ErrorAction SilentlyContinue`. If an editor is running, save dirty packages through `scripts/ue_mcp_client.py`; if MCP cannot connect, stop and preserve the editor rather than force-closing it.

## File responsibility map

| File | Responsibility in this change |
| --- | --- |
| `Source/GameXXK/Public/GameXXKCardTypes.h` | Append serialized enums; add hero catalog descriptors, reaction records, active-play snapshots, automatic-resolution continuation, Mage task state, exhaust zone, and audit fields. |
| `Source/GameXXK/Public/GameXXKCardCatalog.h` | Expose the deterministic hero-unlock query used by runtime, migration, and tests. |
| `Source/GameXXK/Private/GameXXKCardCatalog.cpp` | Replace 12 legacy hero entries with the exact 36 approved entries and validate their declarative metadata. |
| `Source/GameXXK/Private/GameXXKCardQualityRules.cpp` | Remove legacy hero IDs from Rare/Epic authority and keep all 36 protagonist permanent cards Common so their approved table values are their default runtime values. |
| `Source/GameXXK/Public/GameXXKCardRules.h` | Declare exhaust movement, pending search submission, reaction-at-intent-boundary, terrain-change recording, and queue-resume APIs. |
| `Source/GameXXK/Private/GameXXKCardRules.cpp` | Own all shared effect execution, origin filtering, continuation, reactions, Heavy Arrow, Mage task, terrain benefits, and terminal-boundary behavior. |
| `Source/GameXXK/Public/GameXXKCardBattleAdapter.h` | Preserve existing callers while exposing resumed automatic results after discard, insight, and Mage-search choices. |
| `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp` | Reconcile level unlocks, initialize the eight-card hero snapshot, call the enemy-card boundary once per intent, and resume queued effects after choices. |
| `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h` | Introduce save version 12. |
| `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp` | Map all legacy hero IDs and normalize unlock/selection/runtime card references. |
| `Source/GameXXK/Private/GameXXKCardText.cpp` | Render the new declarative effects and short shared keywords. |
| `Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp` | Add Block and replace verbose/stale status tooltips without changing layout. |
| `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp` | Sort/map Block as a separate status; no geometry changes. |
| `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h` | Add only the existing-panel submission seam for Mage search. |
| `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp` | Reuse `PendingChoicePanel` for Mage search and leave all anchors/sizes unchanged. |
| `Source/GameXXK/Private/Tests/GameXXKHeroCardCatalogTest.cpp` | Exact 36-row data, enum, descriptor, target, cost, and unlock contract. |
| `Source/GameXXK/Private/Tests/GameXXKHeroCardUnlockMigrationTest.cpp` | Level gates, legacy ID migration, selection repair, active-zone migration, and round-trip stability. |
| `Source/GameXXK/Private/Tests/GameXXKCardResolutionQueueTest.cpp` | Exhaust zone, origin isolation, target fallback, interrupted replay continuation, and atomic terminal evaluation. |
| `Source/GameXXK/Private/Tests/GameXXKHeroGenericCardRuntimeTest.cpp` | Runtime behavior of the 12 generic cards. |
| `Source/GameXXK/Private/Tests/GameXXKHeroCounterBlockRuntimeTest.cpp` | Counter/Block source records and once-per-enemy-card boundary. |
| `Source/GameXXK/Private/Tests/GameXXKHeroBladeRuntimeTest.cpp` | Charge, Finish, arbitrary next-card interaction, replays, and bleed-preserving trigger. |
| `Source/GameXXK/Private/Tests/GameXXKHeroHealerRuntimeTest.cpp` | Nonlethal party loss, Medicine snapshot, healing reversal, Toxic Explosion, and group caps. |
| `Source/GameXXK/Private/Tests/GameXXKHeroHunterRuntimeTest.cpp` | Heavy Arrow ordering, Charge snapshot, extra hits/explosions/draw/energy. |
| `Source/GameXXK/Private/Tests/GameXXKHeroMageRuntimeTest.cpp` | Eight-card task, search choice, replay order, interruption, fallback target, and four rewards. |
| `Source/GameXXK/Private/Tests/GameXXKHeroFormationRuntimeTest.cpp` | Six terrain benefits, switch flag, three active-card listeners, and fixed all-terrain sequence. |
| `Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp` | Separate Counter/Block icons, concise approved status text, unchanged bar placement behavior. |
| `Source/GameXXK/Private/Tests/GameXXKCardTextTest.cpp` | Concise keyword/card text for every newly introduced descriptor. |
| `Source/GameXXK/Private/Tests/GameXXKCardBalanceObservationTest.cpp` | Extend the locked 2,400-case observation with new damage origins and queue invariants. |
| `scripts/run_card_balance_observation.py` | Aggregate the unchanged 2,400-case matrix and compare it with the recorded baseline. |

## Shared verification commands

Use these exact commands from `D:\UE5 demo\GameXXK`. Replace only the test filter and report folder named by each task.

Cold editor build:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload -NoHotReloadFromIDE
```

Fresh Automation process:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -nosplash -NullRHI "-ExecCmds=Automation RunTests <FILTER>;Quit" "-TestExit=Automation Test Queue Empty" "-ReportExportPath=D:\UE5 demo\GameXXK\Saved\Automation\<REPORT>" -log
```

Report gate:

```powershell
$report = Get-Content 'Saved/Automation/<REPORT>/index.json' -Raw | ConvertFrom-Json
$report | Select-Object succeeded, succeededWithWarnings, failed, notRun
```

Expected GREEN for every focused report: `failed = 0`, `notRun = 0`, and the log explicitly lists every intended leaf test. A zero process exit without the intended tests is not GREEN.

### Task 1: Lock the append-only data and runtime schema

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKHeroCardCatalogTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardCatalogTest.cpp`

- [ ] **Step 1: Write the schema RED test**

Create `GameXXK.Data.HeroCards.Catalog.Schema` and assert the exact terminal values plus default-safe descriptors:

```cpp
TestEqual(TEXT("Block appends after Counter"), static_cast<uint8>(EGameXXKCardStatus::Block), 27);
TestEqual(TEXT("Exhaust appends after Discard"), static_cast<uint8>(EGameXXKCardZone::ExhaustPile), 4);
TestEqual(TEXT("hero search appends after Insight"), static_cast<uint8>(EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand), 4);
TestEqual(TEXT("Block damage cause appends after Environment"), static_cast<uint8>(EGameXXKCardDamageCause::Block), 12);

FGameXXKCardDefinition Definition;
TestEqual(TEXT("generic cards have no linked role by default"), Definition.LinkedRole, EGameXXKCharacterRole::Invalid);
TestEqual(TEXT("non-hero cards have no hero unlock level"), Definition.HeroUnlockLevel, 0);
TestFalse(TEXT("cards do not exhaust by default"), Definition.bExhaustOnPlay);
TestTrue(TEXT("charge effects default empty"), Definition.ChargeEffects.IsEmpty());
TestTrue(TEXT("finish effects default empty"), Definition.FinishEffects.IsEmpty());
TestEqual(TEXT("Heavy Arrow defaults off"), Definition.HeavyArrow.Kind, EGameXXKHeavyArrowKind::None);
TestEqual(TEXT("Mage reward defaults off"), Definition.SpellTaskReward, EGameXXKHeroSpellTaskReward::None);
```

Also lock the new effect-source defaults and the append-only modifier triggers:

```cpp
FGameXXKCardEffect Effect;
TestEqual(TEXT("effects source from their card owner by default"), Effect.Source, EGameXXKCardEffectSource::CardOwner);
TestEqual(TEXT("terrain override defaults off"), Effect.TerrainOverride, EGameXXKCardTerrain::Invalid);
TestTrue(TEXT("effect result group defaults empty"), Effect.ResultGroupId.IsNone());
TestTrue(TEXT("effect result reference defaults empty"), Effect.ResultRef.IsNone());

FGameXXKCardDamageContext DamageContext;
FGameXXKCardDamageResult DamageResult;
FGameXXKCardPlayResult PlayResult;
TestEqual(TEXT("damage context origin defaults invalid"), DamageContext.ResolutionOrigin, EGameXXKCardResolutionOrigin::Invalid);
TestEqual(TEXT("damage result origin defaults invalid"), DamageResult.ResolutionOrigin, EGameXXKCardResolutionOrigin::Invalid);
TestEqual(TEXT("play result origin defaults invalid"), PlayResult.ResolutionOrigin, EGameXXKCardResolutionOrigin::Invalid);
TestEqual(TEXT("low-level agility roll defaults deterministically"), DamageContext.AgilityRollPercent, 0);
TestEqual(TEXT("damage audit starts without a roll"), DamageResult.AgilityRollPercent, INDEX_NONE);
TestEqual(TEXT("damage audit starts with no agility consumption"), DamageResult.AgilityStacksConsumed, 0);
TestFalse(TEXT("damage audit starts without a perfect dodge"), DamageResult.bPerfectAgilityDodge);

TestEqual(TEXT("before-next-active trigger appends at seven"), static_cast<uint8>(EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard), 7);
TestEqual(TEXT("after-next-active trigger appends at eight"), static_cast<uint8>(EGameXXKCardBattleModifierTrigger::AfterNextActiveCard), 8);
TestEqual(TEXT("next-round-start trigger appends at nine"), static_cast<uint8>(EGameXXKCardBattleModifierTrigger::NextPlayerRoundStart), 9);
TestEqual(TEXT("before-first-next-round trigger appends at ten"), static_cast<uint8>(EGameXXKCardBattleModifierTrigger::BeforeFirstActiveCardNextPlayerRound), 10);
TestEqual(TEXT("after-first-next-round trigger appends at eleven"), static_cast<uint8>(EGameXXKCardBattleModifierTrigger::AfterFirstActiveCardNextPlayerRound), 11);
TestEqual(TEXT("first attack against status trigger appends at twelve"), static_cast<uint8>(EGameXXKCardBattleModifierTrigger::FirstActiveAttackAgainstStatusNextPlayerRound), 12);
TestEqual(TEXT("after-each-active trigger appends at thirteen"), static_cast<uint8>(EGameXXKCardBattleModifierTrigger::AfterEachActiveCard), 13);
```

The same test must assert append-only values for these new effect types:

```cpp
const TMap<EGameXXKCardEffectType, uint8> ExpectedEffectValues = {
    {EGameXXKCardEffectType::RegisterReaction, 29},
    {EGameXXKCardEffectType::LoseHealthNonlethal, 30},
    {EGameXXKCardEffectType::Cleanse, 31},
    {EGameXXKCardEffectType::TriggerHighestDamageOverTime, 32},
    {EGameXXKCardEffectType::ResolveToxicExplosion, 33},
    {EGameXXKCardEffectType::HealOrReverseWithMedicine, 34},
    {EGameXXKCardEffectType::GainMedicineFromPartyHealthLoss, 35},
    {EGameXXKCardEffectType::DamagePercentAttackPlusArmor, 36},
    {EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor, 37},
    {EGameXXKCardEffectType::TriggerTerrainBenefit, 38},
    {EGameXXKCardEffectType::GainArmorFromCurrentManaPercent, 39},
    {EGameXXKCardEffectType::GainManaOverflowToArmor, 40},
    {EGameXXKCardEffectType::SearchUnfinishedHeroTaskCard, 41},
    {EGameXXKCardEffectType::TriggerStatus, 42},
    {EGameXXKCardEffectType::LightningPerTargetStatusSnapshot, 43},
    {EGameXXKCardEffectType::ReplayTriggeredCardBase, 44},
    {EGameXXKCardEffectType::ReplaySourceCardBase, 45}
};
```

- [ ] **Step 2: Run cold UBT and confirm RED**

Run the shared cold build command. Expected: UHT/C++ failure is limited to the missing enum members and fields named in Step 1. Save the full output to `Saved/Automation/HeroCards_Task01_Schema_RED/ubt-red.log`.

- [ ] **Step 3: Add the exact schema**

Append these declarations in `GameXXKCardTypes.h`; every new runtime field that can cross a save/choice boundary is `SaveGame`:

```cpp
UENUM(BlueprintType)
enum class EGameXXKCardResolutionOrigin : uint8
{
    Invalid = 0 UMETA(Hidden),
    ActivePlay = 1,
    AutomaticReplay = 2,
    MageTaskReplay = 3,
    HeavyArrow = 4,
    Reaction = 5,
    TerrainListener = 6,
    TaskReward = 7
};

UENUM(BlueprintType)
enum class EGameXXKHeavyArrowKind : uint8
{
    None = 0,
    ExtraAttackPerCharge = 1,
    ToxicExplosionPerCharge = 2,
    AddPrimaryAttackPercentPerCharge = 3
};

UENUM(BlueprintType)
enum class EGameXXKHeroSpellTaskReward : uint8
{
    None = 0,
    Fire = 1,
    Ice = 2,
    Lightning = 3,
    Universal = 4
};

UENUM(BlueprintType)
enum class EGameXXKCardEffectSource : uint8
{
    Invalid = 0 UMETA(Hidden),
    CardOwner = 1,
    SelectedTarget = 2,
    HighestArmorAlly = 3
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKHeavyArrowRule
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere) EGameXXKHeavyArrowKind Kind = EGameXXKHeavyArrowKind::None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) int32 MagnitudePerCharge = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) int32 DrawPerCharge = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) int32 MinimumChargeForEnergy = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) int32 EnergyGain = 0;
};
```

Append `Block`, the effect values listed in Step 1, `ExhaustPile`, `HeroTaskSearchChooseToHand`, and `Block` damage cause. Add `HighestArmorAlly = 11` to `EGameXXKCardEffectTarget`.

Append these modifier triggers without renumbering the existing six:

```cpp
BeforeNextActiveCard = 7,
AfterNextActiveCard = 8,
NextPlayerRoundStart = 9,
BeforeFirstActiveCardNextPlayerRound = 10,
AfterFirstActiveCardNextPlayerRound = 11,
FirstActiveAttackAgainstStatusNextPlayerRound = 12,
AfterEachActiveCard = 13
```

Add these fields to `FGameXXKCardEffect`:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere)
EGameXXKCardEffectSource Source = EGameXXKCardEffectSource::CardOwner;

UPROPERTY(BlueprintReadWrite, EditAnywhere)
EGameXXKCardTerrain TerrainOverride = EGameXXKCardTerrain::Invalid;

UPROPERTY(BlueprintReadWrite, EditAnywhere)
FName ResultGroupId = NAME_None;

UPROPERTY(BlueprintReadWrite, EditAnywhere)
FName ResultRef = NAME_None;
```

`ResultGroupId` stores an actual integer result such as affected allies, consumed armor, or applied Medicine; `ResultRef` lets a later effect read it without relying on array position.

Add these fields to `FGameXXKCardBattleModifier`:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere)
bool bActivePlayOnly = false;

UPROPERTY(BlueprintReadWrite, EditAnywhere)
bool bExcludeSourceUnit = false;

UPROPERTY(BlueprintReadWrite, EditAnywhere)
bool bPreserveTriggeredStatus = false;
```

Add the following definition fields exactly:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere)
EGameXXKCharacterRole LinkedRole = EGameXXKCharacterRole::Invalid;

UPROPERTY(BlueprintReadWrite, EditAnywhere)
int32 HeroUnlockLevel = 0;

UPROPERTY(BlueprintReadWrite, EditAnywhere)
bool bExhaustOnPlay = false;

UPROPERTY(BlueprintReadWrite, EditAnywhere)
TArray<FGameXXKCardEffect> ChargeEffects;

UPROPERTY(BlueprintReadWrite, EditAnywhere)
TArray<FGameXXKCardEffect> FinishEffects;

UPROPERTY(BlueprintReadWrite, EditAnywhere)
FGameXXKHeavyArrowRule HeavyArrow;

UPROPERTY(BlueprintReadWrite, EditAnywhere)
EGameXXKHeroSpellTaskReward SpellTaskReward = EGameXXKHeroSpellTaskReward::None;
```

Add `FGameXXKResolvedCardSnapshot` before `FGameXXKCardBattleModifierRuntime`, then add the remaining serializable runtime types before `FGameXXKCardBattleRuntime`:

```cpp
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKResolvedCardSnapshot
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName CardId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKCardQuality Quality = EGameXXKCardQuality::Common;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName OwnerUnitId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TArray<FName> OriginalTargetUnitIds;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKReactionRuntime
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName ReactionId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKCardStatus Status = EGameXXKCardStatus::None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName RecipientUnitId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName GrantedByUnitId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName SourceCardInstanceId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 RemainingTriggers = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 ExpireBeforePlayerRound = 0;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKAutomaticResolutionQueue
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) bool bActive = false;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKCardResolutionOrigin Origin = EGameXXKCardResolutionOrigin::Invalid;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TArray<FGameXXKResolvedCardSnapshot> PendingCards;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 NextCardIndex = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKHeroSpellTaskReward PendingReward = EGameXXKHeroSpellTaskReward::None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName RewardOwnerUnitId = NAME_None;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKHeroSpellTaskRuntime
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) bool bActive = false;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TArray<FName> LockedHeroCardIds;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TArray<FName> CompletedHeroCardIds;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TArray<FGameXXKResolvedCardSnapshot> FirstPlayOrder;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKHeroSpellTaskReward StarterReward = EGameXXKHeroSpellTaskReward::None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName StarterOwnerUnitId = NAME_None;
};
```

Add this field to `FGameXXKCardBattleModifierRuntime` so a delayed replay never depends on a discarded live instance:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
FGameXXKResolvedCardSnapshot SourceCardSnapshot;
```

Add `ExhaustPile` to the deck and these fields to the battle runtime:

```cpp
TArray<FName> EquippedHeroCardIds;
int32 ActiveCardsPlayedThisRound = 0;
FGameXXKResolvedCardSnapshot LastActiveCard;
bool bTerrainChangedThisRound = false;
int32 CombatRandomState = 0;
TArray<FGameXXKReactionRuntime> Reactions;
int32 NextReactionOrdinal = 0;
FGameXXKAutomaticResolutionQueue AutomaticResolutionQueue;
FGameXXKHeroSpellTaskRuntime HeroSpellTask;
```

Mark each with `UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)`.

Add `EGameXXKCardResolutionOrigin ResolutionOrigin = EGameXXKCardResolutionOrigin::Invalid` to `FGameXXKCardDamageContext`, `FGameXXKCardDamageResult`, and `FGameXXKCardPlayResult`. The shared resolver stamps the play result and every nested damage context/result with its explicit origin. Existing enemy/status/environment paths may keep `Invalid` until they explicitly select a more specific source, while automatic replay, Mage replay, Heavy Arrow, Reaction, TerrainListener, and TaskReward must never be ambiguous in tests or balance output.

Add `int32 AgilityRollPercent = 0` to `FGameXXKCardDamageContext`, plus `int32 AgilityRollPercent = INDEX_NONE`, `int32 AgilityStacksConsumed = 0`, and `bool bPerfectAgilityDodge = false` to `FGameXXKCardDamageResult`. Runtime attack entrypoints advance the separately saved `CombatRandomState` exactly once per direct packet and stamp `0..99`; the low-level pure packet API keeps a deterministic default for existing direct-rule fixtures.

- [ ] **Step 4: Extend existing enum regression assertions**

In `GameXXKCardCatalogTest.cpp`, retain every current numeric assertion and add assertions for the new terminal values. Do not replace old checks with only terminal checks.

- [ ] **Step 5: Build and run the schema test**

Run cold UBT, then `GameXXK.Data.HeroCards.Catalog.Schema` with report `HeroCards_Task01_Schema_GREEN`. Expected: one discovered test, one success, zero failures.

- [ ] **Step 6: Commit Task 1**

```powershell
git add Source/GameXXK/Public/GameXXKCardTypes.h Source/GameXXK/Private/Tests/GameXXKCardCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKHeroCardCatalogTest.cpp
git commit -m "test: lock protagonist card runtime schema"
```

### Task 2: Replace the hero catalog and implement level unlocks/save v12

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardCatalog.h`
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardQualityRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardBattleAdapter.h`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKHeroCardCatalogTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKHeroCardUnlockMigrationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardQualityRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardQualityResolutionTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardTextTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardCombatRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleActorHudRetirementTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCompanionRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEnemyMechanicsTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteCardEntriesSaveMigrationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteMapSummaryWidgetTest.cpp`

- [ ] **Step 1: Write catalog and unlock RED tests**

Use an explicit expected row table. At minimum it must contain these 36 unique IDs in this insertion order, with the specified linked role and unlock level:

```cpp
static const TArray<FExpectedHeroCard> Expected = {
    {TEXT("Hero.Generic.QingFengYiShi"), EGameXXKCharacterRole::Invalid, 1},
    {TEXT("Hero.Generic.HeYuZhan"), EGameXXKCharacterRole::Invalid, 1},
    {TEXT("Hero.Generic.FengShenBu"), EGameXXKCharacterRole::Invalid, 1},
    {TEXT("Hero.Generic.SuiYanJi"), EGameXXKCharacterRole::Invalid, 1},
    {TEXT("Hero.Generic.GuiYuanShu"), EGameXXKCharacterRole::Invalid, 1},
    {TEXT("Hero.Generic.HengJianShouShi"), EGameXXKCharacterRole::Invalid, 1},
    {TEXT("Hero.Generic.NingShenTuNa"), EGameXXKCharacterRole::Invalid, 1},
    {TEXT("Hero.Generic.GuanXi"), EGameXXKCharacterRole::Invalid, 1},
    {TEXT("Hero.Generic.PoYunYiShan"), EGameXXKCharacterRole::Invalid, 5},
    {TEXT("Hero.Generic.XingQiHuiHuan"), EGameXXKCharacterRole::Invalid, 10},
    {TEXT("Hero.Generic.JianYiGuanHong"), EGameXXKCharacterRole::Invalid, 15},
    {TEXT("Hero.Generic.GuiYuanFanZhao"), EGameXXKCharacterRole::Invalid, 20},
    {TEXT("Hero.Blade.TongFengYinShi"), EGameXXKCharacterRole::Blade, 1},
    {TEXT("Hero.Blade.XueLuXiangCheng"), EGameXXKCharacterRole::Blade, 1},
    {TEXT("Hero.Blade.YingFengHuanBu"), EGameXXKCharacterRole::Blade, 1},
    {TEXT("Hero.Blade.TongPaoJuShi"), EGameXXKCharacterRole::Blade, 1},
    {TEXT("Hero.Guard.TieBiTongShou"), EGameXXKCharacterRole::Guard, 1},
    {TEXT("Hero.Guard.JieJiaHuanFeng"), EGameXXKCharacterRole::Guard, 1},
    {TEXT("Hero.Guard.LieZhenChengFeng"), EGameXXKCharacterRole::Guard, 1},
    {TEXT("Hero.Guard.XuanJiaZhenYue"), EGameXXKCharacterRole::Guard, 1},
    {TEXT("Hero.Healer.YiXueCuiFang"), EGameXXKCharacterRole::Healer, 1},
    {TEXT("Hero.Healer.HuiChunNiMai"), EGameXXKCharacterRole::Healer, 1},
    {TEXT("Hero.Healer.DuHuoTongLu"), EGameXXKCharacterRole::Healer, 1},
    {TEXT("Hero.Healer.BaiCaoJiZhen"), EGameXXKCharacterRole::Healer, 1},
    {TEXT("Hero.Hunter.FengYanDingXian"), EGameXXKCharacterRole::Hunter, 1},
    {TEXT("Hero.Hunter.LieYuLianShi"), EGameXXKCharacterRole::Hunter, 1},
    {TEXT("Hero.Hunter.CuiDuChuanXin"), EGameXXKCharacterRole::Hunter, 1},
    {TEXT("Hero.Hunter.HuiFengGuanRi"), EGameXXKCharacterRole::Hunter, 1},
    {TEXT("Hero.Mage.YanXuLiaoYuan"), EGameXXKCharacterRole::Sorcerer, 1},
    {TEXT("Hero.Mage.HanXuNingChuan"), EGameXXKCharacterRole::Sorcerer, 1},
    {TEXT("Hero.Mage.LeiXuYinTing"), EGameXXKCharacterRole::Sorcerer, 1},
    {TEXT("Hero.Mage.GuiXuTongXuan"), EGameXXKCharacterRole::Sorcerer, 1},
    {TEXT("Hero.Formation.GuanShiLuoZi"), EGameXXKCharacterRole::FormationMaster, 1},
    {TEXT("Hero.Formation.YiZhenHuiXiang"), EGameXXKCharacterRole::FormationMaster, 1},
    {TEXT("Hero.Formation.LianYingBuShi"), EGameXXKCharacterRole::FormationMaster, 1},
    {TEXT("Hero.Formation.LiuHeGuiYi"), EGameXXKCharacterRole::FormationMaster, 1}
};
```

Assert hero count `36`, total catalog/visual count `198`, identity locks `60`, default level-1 unlock count `32`, and level 5/10/15/20 counts `33/34/35/36`. Assert all four Formation cards remain `SingleEnemy` for every terrain preview.

Create `GameXXK.MVP.SaveGame.HeroCardPoolV12` with v11 fixtures containing all 12 old IDs in unlock/selection arrays and card instances in draw, hand, discard, and pending candidates. Expected mapping:

```cpp
static const TMap<FName, FName> LegacyToV12 = {
    {TEXT("Hero.QingFengYiShi"), TEXT("Hero.Generic.QingFengYiShi")},
    {TEXT("Hero.HeYuZhan"), TEXT("Hero.Generic.HeYuZhan")},
    {TEXT("Hero.FengShenBu"), TEXT("Hero.Generic.FengShenBu")},
    {TEXT("Hero.SuiYanJi"), TEXT("Hero.Generic.SuiYanJi")},
    {TEXT("Hero.GuiYuanShu"), TEXT("Hero.Generic.GuiYuanShu")},
    {TEXT("Hero.HengJianShouShi"), TEXT("Hero.Generic.HengJianShouShi")},
    {TEXT("Hero.NingShenTuNa"), TEXT("Hero.Generic.NingShenTuNa")},
    {TEXT("Hero.GuanXi"), TEXT("Hero.Generic.GuanXi")},
    {TEXT("Hero.PoYunYiShan"), TEXT("Hero.Generic.PoYunYiShan")},
    {TEXT("Hero.HuiFengZhuiJian"), TEXT("Hero.Generic.XingQiHuiHuan")},
    {TEXT("Hero.JianYiGuanHong"), TEXT("Hero.Generic.JianYiGuanHong")},
    {TEXT("Hero.GuiYuanFanZhao"), TEXT("Hero.Generic.GuiYuanFanZhao")}
};
```

At level 1, mapped level-gated selections must be deterministically replaced with the first missing unlocked level-1 IDs so the result remains eight unique playable IDs. At level 20, preserve mapped selection order. A second migration/initialization pass must be byte-stable.

- [ ] **Step 2: Run RED**

Run cold UBT and then both `GameXXK.Data.HeroCards.Catalog` and `GameXXK.MVP.SaveGame.HeroCardPoolV12`. Expected: failures show the old `12/174/36` counts, missing new IDs, old unlock policy, and save version 11.

- [ ] **Step 3: Add the deterministic unlock API**

Declare and implement:

```cpp
static TArray<FName> GetHeroCardIdsUnlockedAtLevel(int32 HeroLevel);
```

The implementation clamps level to `1..20`, iterates catalog order, selects `Owner == Hero && HeroUnlockLevel <= level`, and returns unique IDs. Catalog validation requires hero entries to have `Role == Hero`, `OwnerId == Hero`, `HeroUnlockLevel` in `{1,5,10,15,20}`, and `LinkedRole` either invalid or one of the six permanent professions. Non-hero entries require `HeroUnlockLevel == 0`, invalid `LinkedRole`, empty charge/finish arrays, no Heavy Arrow rule, and no Mage reward.

Extend `FGameXXKCardCatalog::ValidateCardDefinition` to validate every new descriptor without claiming the runtime already supports it: result references must name an earlier result group in the same effect list, Heavy Arrow metadata must be internally complete, only Sorcerer-linked hero cards may declare a Mage reward, and only Hunter-linked hero cards may declare a non-empty Heavy Arrow rule. Validation must remain data-driven and must not branch on a concrete CardId.

- [ ] **Step 4: Replace `AddHeroCards` with the exact 36 rows**

Use the approved costs, targets, base effects, charge effects, finish effects, Heavy Arrow rules, and Mage reward descriptors from Sections 5-11 of the authoritative spec. Set:

Extend the existing `AddCard` helper only with optional trailing parameters, so all 162 non-hero call sites retain current behavior:

```cpp
void AddCard(
    TArray<FGameXXKCardDefinition>& Cards,
    EGameXXKCardOwner Owner,
    EGameXXKCardRarity Rarity,
    EGameXXKCharacterRole Role,
    const TCHAR* OwnerId,
    const TCHAR* NpcId,
    const TCHAR* CardId,
    const TCHAR* DisplayName,
    int32 EnergyCost,
    int32 ManaCost,
    EGameXXKCardTargetMode TargetMode,
    TArray<FGameXXKCardEffect> Effects,
    const TCHAR* FrameKey,
    const TCHAR* AcquisitionKey,
    bool bCoreProfessionCard = false,
    bool bIdentityLocked = false,
    TArray<FGameXXKCardTargetModeOverride> TargetModeOverrides = {},
    EGameXXKCharacterRole LinkedRole = EGameXXKCharacterRole::Invalid,
    int32 HeroUnlockLevel = 0,
    bool bExhaustOnPlay = false,
    TArray<FGameXXKCardEffect> ChargeEffects = {},
    TArray<FGameXXKCardEffect> FinishEffects = {},
    FGameXXKHeavyArrowRule HeavyArrow = {},
    EGameXXKHeroSpellTaskReward SpellTaskReward = EGameXXKHeroSpellTaskReward::None);
```

Move every array parameter into its matching definition field, and assign every scalar exactly once.

Use this exact catalog matrix; `E/M` means Energy/Mana, and every special column becomes declarative metadata or an effect rather than a CardId branch:

| CardId | E/M | Target | Base effects | Special descriptor |
| --- | ---: | --- | --- | --- |
| `Hero.Generic.QingFengYiShi` | 1/0 | SingleEnemy | Attack140 | next active card by another unit costs -1 |
| `Hero.Generic.HeYuZhan` | 1/3 | SingleEnemy | Attack160; trigger highest Bleed/Poison/Burn | tie order Bleed, Poison, Burn |
| `Hero.Generic.FengShenBu` | 0/0 | SingleAlly | Agility2; draw2; discard1 | exhaust |
| `Hero.Generic.SuiYanJi` | 1/3 | SingleEnemy | Attack150; Vulnerability3; Mark1 | none |
| `Hero.Generic.GuiYuanShu` | 1/0 | SingleAlly | heal12; Cleanse | target's next active card this round costs -1 |
| `Hero.Generic.HengJianShouShi` | 1/0 | SingleAlly | Mark2; Armor16; Block1 | none |
| `Hero.Generic.NingShenTuNa` | 0/0 | Self | Momentum2; Mana10 | exhaust |
| `Hero.Generic.GuanXi` | 0/0 | None | draw3; discard1 | exhaust |
| `Hero.Generic.PoYunYiShan` | 1/3 | SingleEnemy | Attack160 | consume Agility1 to append Attack100 and draw1 |
| `Hero.Generic.XingQiHuiHuan` | 0/0 | None | draw2; Energy1 | exhaust |
| `Hero.Generic.JianYiGuanHong` | 2/6 | SingleEnemy | Attack260 | consume all starting Momentum; +20 percentage points each; 3+ grants Energy1 |
| `Hero.Generic.GuiYuanFanZhao` | 2/6 | AllAllies | heal6; Armor12; Cleanse; draw2 | none |
| `Hero.Blade.TongFengYinShi` | 0/0 | SingleAlly | draw1; target Momentum2 | Charge replays next active base; Finish replays source base after next-round first active |
| `Hero.Blade.XueLuXiangCheng` | 1/3 | SingleEnemy | Attack150; Bleed8 | Charge triggers next attack target's Bleed without decay; Finish next-round first attack on Bleed draws2/Energy1 |
| `Hero.Blade.YingFengHuanBu` | 1/0 | Self | hero Mark2; Agility3; Counter2 | Charge next owner Agility2/Counter1; Finish next round start hero Mark2/Counter2 |
| `Hero.Blade.TongPaoJuShi` | 1/0 | SingleAlly | target Momentum3; next attack +10 percentage points per Momentum per segment | Charge next owner Momentum3; Finish bound target's first next-round active card costs0 |
| `Hero.Guard.TieBiTongShou` | 1/0 | SingleAlly | Armor18; Block2 | none |
| `Hero.Guard.JieJiaHuanFeng` | 1/3 | SingleEnemy | highest-Armor ally attacks for Attack100+Armor; then that ally Armor10/Block1 | stable ally order breaks ties |
| `Hero.Guard.LieZhenChengFeng` | 2/0 | AllAllies | Armor8; Block1 | none |
| `Hero.Guard.XuanJiaZhenYue` | 2/6 | SingleAlly | consume target's full Armor; all enemies receive `100% + 20% * consumed` attack | zero Armor still Attack100 |
| `Hero.Healer.YiXueCuiFang` | 0/0 | None | each ally loses1 nonlethally; hero gains Medicine2 per ally that actually lost HP; draw1 | one Medicine6 grant also gives Momentum1 |
| `Hero.Healer.HuiChunNiMai` | 1/3 | AnyLivingUnit | ally heals `10+Medicine` and Cleanses; enemy loses `10+Medicine` HP | snapshot/consume all Medicine once |
| `Hero.Healer.DuHuoTongLu` | 1/3 | SingleEnemy | Attack130; Poison6; Burn2; Toxic Explosion; Medicine6 | Medicine grant also gives Momentum1 |
| `Hero.Healer.BaiCaoJiZhen` | 2/6 | None | allies heal `6+Medicine`; enemies Poison1/Burn1 | snapshot/consume Medicine once |
| `Hero.Hunter.FengYanDingXian` | 0/3 | None | draw2; discard1; Agility2; Charge3 | no Heavy Arrow rule |
| `Hero.Hunter.LieYuLianShi` | 1/3 | SingleEnemy | Attack140; Bleed8 | each consumed Charge appends Attack50 |
| `Hero.Hunter.CuiDuChuanXin` | 1/3 | SingleEnemy | Attack130; Poison6; Toxic Explosion | each consumed Charge appends one Toxic Explosion |
| `Hero.Hunter.HuiFengGuanRi` | 1/6 | SingleEnemy | pending Attack150 | each Charge adds 40 percentage points to same hit and draws1; 3+ grants Energy1 |
| `Hero.Mage.YanXuLiaoYuan` | 1/3 | SingleEnemy | Attack100; Burn4; search1 unfinished hero card | Fire reward: all enemies Burn8 then trigger Burn twice |
| `Hero.Mage.HanXuNingChuan` | 0/0 | Self | Armor=floor(current Mana*25%); Mana6; overflow to Armor100% | Ice reward: consume all Armor; all enemies take Attack20 per point |
| `Hero.Mage.LeiXuYinTing` | 1/3 | SingleEnemy | Attack100; Mark3; search1 unfinished hero card | Lightning reward: all enemies Mark3; snapshot Mark; one Attack60 per snapshot layer |
| `Hero.Mage.GuiXuTongXuan` | 0/0 | None | draw2; discard1 | Universal reward: draw4; Energy2; hero cards cost -1 this round |
| `Hero.Formation.GuanShiLuoZi` | 0/3 | SingleEnemy | Attack80; current terrain benefit once; draw1 | none |
| `Hero.Formation.YiZhenHuiXiang` | 1/3 | SingleEnemy | current terrain benefit twice | if terrain changed this round: three times and Energy1 |
| `Hero.Formation.LianYingBuShi` | 1/0 | SingleEnemy | register three post-active terrain benefits anchored to target | reads live terrain each trigger |
| `Hero.Formation.LiuHeGuiYi` | 2/6 | SingleEnemy | Plain, Cliff, Forest, WaterShore, Village, Cave benefits; then current benefit | never changes terrain or switch flag |

```cpp
Definition.Role = EGameXXKCharacterRole::Hero;
Definition.LinkedRole = LinkedRole;
Definition.HeroUnlockLevel = HeroUnlockLevel;
Definition.AcquisitionKey = HeroUnlockLevel == 1
    ? FName(TEXT("Unlock.Initial"))
    : FName(*FString::Printf(TEXT("Unlock.Level.%02d"), HeroUnlockLevel));
Definition.bIdentityLocked = true;
```

The eight initial generic cards must be the first eight hero rows so a new save's default eight-card selection is deterministic. The four level cards follow, then the six linked-role groups in Blade, Guard, Healer, Hunter, Mage, Formation order.

Update the independent quality authority in `GameXXKCardQualityRules.cpp`: remove all legacy hero IDs from the Rare/Epic lists and classify every one of the 36 protagonist permanent cards as Common. The approved card table gives exact default combat values and defines no protagonist rarity distribution; preserving the old Rare/Epic flags would silently double or quadruple several confirmed values. Lock total quality counts at `122 Common / 47 Rare / 29 Epic = 198`; explicit Rare/Epic card instances must still exercise the generic upgrade scaler in quality-resolution tests, and an unknown ID must not silently satisfy the catalog test.

Remove every accidental legacy hero ID fixture under `Source/GameXXK/Private/Tests`. Identity, ownership, portrait, loadout, and save tests use the mapped canonical ID from `LegacyToV12` and update only assertions whose approved cost/target/data value changed. A generic engine test whose former hero fixture now includes a not-yet-implemented v12 operation must switch to an existing non-hero card that exercises the same primitive the test actually owns; the later hero-specific tests become authoritative for the new card. Do not weaken the original assertion or add a production compatibility alias. The explicitly affected files are the 14 existing tests listed in this task; after the replacement, `rg` must find old IDs only in the migration table and deliberate v11 migration fixtures.

- [ ] **Step 5: Implement save v12 migration and unlock reconciliation**

Set:

```cpp
static constexpr int32 HeroCardPoolIntroducedSaveVersion = 12;
static constexpr int32 CurrentSaveVersion = 12;
```

Add one `MigrateHeroCardId` helper using `LegacyToV12`, and visit `HeroUnlockedCardIds`, `HeroSelectedCardIds`, every active battle zone including exhaust, pending-choice candidates, route entries, legacy route IDs, Mage task snapshots, automatic queue snapshots, and `LastActiveCard`. Then rebuild the allowed unlock set from `PlayerLevel`; remove unknown/locked IDs; add all expected IDs once; repair the eight-card selection without reordering still-legal entries.

For a pre-v12 active battle, old serialized `Medicine` stacks still mean the retired hidden medicine resource. Convert each old stack to exactly six `NextHealingBonus` stacks, merge with any existing `NextHealingBonus` using its normal cap, and clear old `Medicine`. Only v12 card resolution may create `Medicine` with the new player-facing 药效 semantics. This prevents an old save from receiving an unintended healing-reversal resource. Initialize the new `CombatRandomState` deterministically from the saved deck random state and a fixed nonzero salt; assert the same v11 input always yields the same value and a second migration does not advance it.

Change `EnsureCardRunInitialized` to call the same catalog helper for every save, not only when the unlock list is empty. It must never infer linked-card availability from the active partner or partner level.

- [ ] **Step 6: Run GREEN and regression filters**

Run cold UBT. Run:

```text
GameXXK.Data.HeroCards.Catalog
GameXXK.MVP.SaveGame.HeroCardPoolV12
GameXXK.Data.CardCatalog
GameXXK.Data.CardQuality
GameXXK.Data.CardBattleRuntime
GameXXK.Integration.CardText
GameXXK.Integration.CardBattle.Board
GameXXK.Battle.EnemyIntentRules
GameXXK.Battle.EnemyMechanics
GameXXK.MVP.Battle.BoardWidget
GameXXK.MVP.SaveGame
```

Use one fresh report folder per filter: `HeroCards_Task02_<ShortFilter>_GREEN` for the two new groups and `HeroCards_Task02_<ShortFilter>_REGRESSION` for every existing group. Expect all discovered tests to pass with no not-run leaves, and inspect the logs to prove each intended test was actually discovered.

- [ ] **Step 7: Commit Task 2**

```powershell
git add Source/GameXXK/Public/GameXXKCardCatalog.h Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/GameXXKCardQualityRules.cpp Source/GameXXK/Public/GameXXKCardBattleAdapter.h Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Public/MVP/GameXXKSaveMigration.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp
git add Source/GameXXK/Private/Tests/GameXXKHeroCardCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKHeroCardUnlockMigrationTest.cpp Source/GameXXK/Private/Tests/GameXXKCardCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKCardQualityRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKCardQualityResolutionTest.cpp Source/GameXXK/Private/Tests/GameXXKCardTextTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKCardCombatRulesTest.cpp
git add Source/GameXXK/Private/Tests/GameXXKBattleActorHudRetirementTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKCompanionRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKEnemyMechanicsTest.cpp Source/GameXXK/Private/Tests/GameXXKRouteCardEntriesSaveMigrationTest.cpp Source/GameXXK/Private/Tests/GameXXKRouteMapSummaryWidgetTest.cpp
git commit -m "feat: add protagonist thirty six card catalog"
```

### Task 3: Build exhaust and origin-aware automatic resolution

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardBattleAdapter.h`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKCardResolutionQueueTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp`

- [ ] **Step 1: Write queue/exhaust RED scenarios**

Create `GameXXK.Data.HeroCards.Foundation` with these named cases:

```text
ExhaustedActiveCardMovesToExhaustAndNeverReshuffles
AutomaticReplayPaysNoCostAndMovesNoCard
AutomaticReplayDoesNotIncrementActivePlayCount
DeadOriginalEnemyFallsBackByStableOrder
NoLegalFallbackSkipsOnlyTargetDependentEffects
ForcedDiscardPausesAndResumesReplayQueue
InsightPausesAndResumesReplayQueue
TerminalPhaseWaitsForTheWholeQueuedSequence
SimultaneousEliminationChoosesPlayerVictory
```

Use a two-card automatic queue where the first replay draws two and opens forced discard, and the second deals lethal damage. Before submission, assert `AutomaticResolutionQueue.bActive`, `NextCardIndex == 1`, and no second-card damage. After submission, assert the second card resolves exactly once and the queue resets to its default value.

- [ ] **Step 2: Run RED**

Run cold UBT. Expected: compile failures only for missing public APIs and exhaust-aware validation, or runtime failures showing that cards always discard and no continuation exists.

- [ ] **Step 3: Make deck ownership exhaust-aware**

Update `ValidateDeckStateInternal`, `FindInstance`, defeated-owner cleanup, initialization ledger rebuild, and all zone visitors to include `ExhaustPile`. Replace the unconditional move with:

```cpp
bool MoveResolvedHandCard(
    FGameXXKBattleDeckState& InOutDeck,
    FName InstanceId,
    bool bExhaust,
    FString* OutError);
```

An exhausted instance remains in `ActiveInstanceIds`, appears in exactly one owning zone, never participates in discard reshuffle, and is removed with its defeated owner.

- [ ] **Step 4: Introduce one internal effect resolver with explicit origin**

Refactor the current `ResolveCurrentCardEffects` into:

```cpp
bool ResolveCardEffectsFromSnapshot(
    FGameXXKCardBattleRuntime& InOutRuntime,
    const FGameXXKResolvedCardSnapshot& Snapshot,
    EGameXXKCardResolutionOrigin Origin,
    FGameXXKCardPlayResult& InOutResult,
    FString& OutError);
```

`ActivePlay` alone may pay resources, move a hand instance, increment `ActiveCardsPlayedThisRound`, replace `LastActiveCard`, start/progress Mage task, produce/consume Charge/Finish timing, or notify active-card listeners. All other origins resolve only `Definition.Effects`, use the stored quality/owner, and resolve targets with this deterministic rule:

```cpp
original target legal -> retain it
original target illegal -> first legal same-side unit by StableSortOrder
no legal same-side unit -> omit only effects requiring that target
```

- [ ] **Step 5: Implement serializable continuation**

Add these APIs:

```cpp
GAMEXXK_API bool ResumeAutomaticResolutionQueue(
    FGameXXKCardBattleRuntime& InOutRuntime,
    TArray<FGameXXKCardPlayResult>& OutResults,
    FString* OutError = nullptr);
```

The queue loop advances `NextCardIndex` before resolving a snapshot, stops only when a pending choice becomes active, and clears itself only after all snapshots and `PendingReward` finish. Add runtime-level insight and cancel-insight overloads, and extend the existing runtime forced-discard overload, so all three call `ResumeAutomaticResolutionQueue` after committing the choice. Preserve every deck-only API unchanged for low-level tests.

Preserve existing adapter call sites by adding a final optional output parameter to `SubmitForcedDiscard`, `SubmitInsightChoice`, and `CancelInsight`:

```cpp
TArray<FGameXXKCardPlayResult>* OutResumedResults = nullptr
```

Each adapter operation commits on a temporary `FGameXXKRuntimeState`, resumes the entire queue, syncs the legacy projection once, then exposes only the newly resumed results. A failed resume rolls back both the choice and every subsequent automatic effect. Task 9 adds the equivalent Mage-search operation only after that choice type has real behavior.

- [ ] **Step 6: Move terminal evaluation to top-level event boundaries**

Do not call `RefreshCombatTerminalPhase` between effects, Heavy Arrow segments, automatic replays, task replay cards, reaction records, or terrain benefits. A queue paused by an existing card choice remains non-terminal until that choice resumes and empties the queue. Call terminal refresh only after the queue is empty, after a complete enemy intent plus reactions, or after a complete side-end DoT queue. Preserve the existing rule that simultaneous party/enemy elimination becomes Victory.

- [ ] **Step 7: Run GREEN and existing runtime regression**

Run cold UBT; run `GameXXK.Data.HeroCards.Foundation` and `GameXXK.Data.CardBattleRuntime` into `HeroCards_Task03_Foundation_GREEN` and `HeroCards_Task03_Runtime_REGRESSION`. Expected: every named case and all existing runtime leaves pass.

- [ ] **Step 8: Commit Task 3**

```powershell
git add Source/GameXXK/Public/GameXXKCardRules.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Public/GameXXKCardBattleAdapter.h Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/Tests/GameXXKCardResolutionQueueTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp
git commit -m "feat: add resumable automatic card resolution"
```

### Task 4: Implement the 12 generic protagonist cards

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKHeroGenericCardRuntimeTest.cpp`

- [ ] **Step 1: Write exact generic-card RED cases**

Create `GameXXK.Data.HeroCards.Generic` with one case per card and explicit boundary cases. Required assertions include:

```text
QingFeng: 140% packet; next active card owned by a different unit costs one less; same owner does not consume it.
HeYu: costs 1/3; chooses Bleed over Poison over Burn on ties; triggers the highest live stack once; never triggers Rot.
FengShen: costs 0; grants Agility2; draws2 then requires discard1; ends in ExhaustPile.
SuiYan: 150% packet; Vulnerability3; Mark1.
GuiYuan: heals12; clears all Bleed/Poison/Burn but not Rot/Mark/Vulnerability; target's next active card this round costs one less.
HengJian: selected ally receives Mark2, Armor16, Block1.
NingShen: Momentum2, Mana10, ExhaustPile.
GuanXi: draw3/discard1, ExhaustPile.
PoYun: base160; with Agility consumes exactly1, adds a separate100% packet and draws1; without Agility no bonus.
XingQi: draw2, energy+1, ExhaustPile.
JianYi: base260; consumes all starting Momentum; +20 percentage points per stack; 3+ stacks grant one energy; newly created Momentum survives.
GuiYuanFanZhao: every living ally heals6, Armor12, full cleanse; draw2.
```

- [ ] **Step 2: Run RED**

Run cold UBT and `GameXXK.Data.HeroCards.Generic`. Expected: failures isolate unsupported Cleanse/highest-DoT/nonlethal/exhaust/cost-recipient semantics or incorrect catalog values.

- [ ] **Step 3: Implement shared generic effects**

Implement `Cleanse` as all Bleed, Poison, and Burn with `MAX_int32`; never include `DamageOverTime`. Implement `TriggerHighestDamageOverTime` by snapshotting live Bleed/Poison/Burn, selecting largest stack and the fixed tie order, then invoking the existing status trigger with normal one-stack decay. Implement next-card cost modifiers using `OnCardPlayed` with a new data flag `bExcludeSourceUnit` and existing `RecipientUnitIds`; preview remains non-mutating and commit consumes exactly the modifier IDs used by the preview.

For Jian Yi, capture Momentum through the existing card-play snapshot before consuming it, attach `20 * captured stacks` to the same attack request, then grant energy only if captured stacks are at least three.

- [ ] **Step 4: Run GREEN plus status regression**

Run cold UBT; run `GameXXK.Data.HeroCards.Generic`, `GameXXK.Data.CombatStatusRedesign`, and `GameXXK.Data.MarkRules` into separate Task 4 report folders. Expect zero failures; the existing Mark/Momentum/DoT reports must remain unchanged.

- [ ] **Step 5: Commit Task 4**

```powershell
git add Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/Tests/GameXXKHeroGenericCardRuntimeTest.cpp
git commit -m "feat: implement generic protagonist cards"
```

### Task 5: Implement Agility and split Counter/Block at the enemy-card boundary

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKHeroCounterBlockRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardCombatRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCombatStatusRedesignTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEnemyMechanicsTest.cpp`

- [ ] **Step 1: Write reaction RED cases**

Create `GameXXK.Data.HeroCards.CounterBlock` with:

```text
CounterDealsCurrentAttackAndConsumesOneSource
BlockDealsCurrentAttackPlusCurrentArmorWithoutConsumingArmor
CounterAndBlockBothResolve
TwoSourcesEachResolveOnce
ThreeHitEnemyCardDoesNotTripleOneSource
PerfectDodgeStillAllowsRegisteredReaction
NormalAgilityDodgeStillAllowsRegisteredReaction
FailedOneStackAgilityLeavesTheStackAndTakesTheHit
AgilityAcceptsMoreThanTheLegacyTwoStackCap
GroupIntentNeverTriggers
ReactionDamageCannotTriggerReactionRecursively
UnusedReactionExpiresBeforeNextPlayerRound
LethalEnemyHitAndLethalReactionProducesVictory
RedirectUsesTheFinalSingleTargetRecipient
```

For the multi-hit case, register Counter2 and Block2, resolve a three-hit enemy intent, and expect exactly four reactive packets: two Counter packets at `Attack`, two Block packets at `Attack + Armor`; all four records are consumed once and armor is unchanged.

- [ ] **Step 2: Run RED**

Run cold UBT and the focused filter. Expected: current per-packet modifier behavior either triggers multiple times, suppresses on dodge, or cannot distinguish Block.

- [ ] **Step 3: Implement the approved Agility roll and make reaction records authoritative**

Seed `CombatRandomState` independently from the deck shuffle state during fresh battle initialization; Task 2 owns the equivalent pre-v12 migration. Remove Agility's obsolete two-stack cap so cards that grant three layers retain all three. For every direct packet, advance the combat stream once and stamp a roll `0..99`. If the resolved receiver has Agility and the roll is below 25, consume one layer, set `bPerfectAgilityDodge`, and avoid the packet. Otherwise, if at least two layers exist, consume two and avoid it normally. A failed roll with exactly one layer neither consumes that layer nor avoids the hit. Self loss, DoT, Toxic Explosion, Rot, and environment packets never roll or consume Agility. Update every existing deterministic Agility fixture to provide a known perfect/failed seed and replace the old cap-two assertions.

`RegisterReaction` creates one `FGameXXKReactionRuntime` record per requested use, with a unique `Reaction.<ordinal>` ID and `ExpireBeforePlayerRound = RoundNumber + 1`. Synchronize the unit's visible Counter/Block stack count to the sum of remaining records after registration, consumption, expiry, defeat cleanup, and save validation. Existing enemy Counter status behavior remains intact and does not create player reaction records.

- [ ] **Step 4: Resolve once after a complete enemy single-target card**

Declare:

```cpp
GAMEXXK_API bool ResolvePartyReactionsAfterEnemyCard(
    FGameXXKCardBattleRuntime& InOutRuntime,
    FName EnemySourceUnitId,
    EGameXXKCardDamageKind IntentKind,
    FName FinalRecipientUnitId,
    TArray<FGameXXKCardDamageResult>& OutReactionResults,
    FString* OutError = nullptr);
```

Call it once in `ResolveNextEnemyIntentImpl`, after all catalog or legacy intent effects and before terminal refresh. A single-target intent registers its final redirected recipient even when all direct hits were avoided. Snapshot each reaction record before reaction damage, consume the record, emit Counter cause `100% Attack` or Block cause `100% Attack + post-intent current Armor`, and tag the origin `Reaction` so no reactive chain can start. A recipient defeated by the just-completed intent is still allowed to emit its already-registered reaction snapshot; this narrow exception accepts a defeated queued source without making normal defeated units legal card attackers, enabling the approved simultaneous-death victory case.

- [ ] **Step 5: Run GREEN and the full enemy-intent group**

Run cold UBT; run `GameXXK.Data.HeroCards.CounterBlock`, `GameXXK.Data.CardCombatRules`, `GameXXK.Data.CombatStatusRedesign`, `GameXXK.Data.CardBattleRuntime`, `GameXXK.Battle.EnemyIntentRules`, and `GameXXK.Battle.EnemyMechanics` in fresh processes. Inspect that both seeded Agility branches and the multi-hit reaction leaf ran, and that existing enemy-specific warnings did not become errors.

- [ ] **Step 6: Commit Task 5**

```powershell
git add Source/GameXXK/Public/GameXXKCardRules.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/Tests/GameXXKHeroCounterBlockRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKCardCombatRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKCombatStatusRedesignTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKEnemyMechanicsTest.cpp
git commit -m "feat: separate counter and block reactions"
```

### Task 6: Implement Blade Charge and Finish

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKHeroBladeRuntimeTest.cpp`

- [ ] **Step 1: Write all Blade timing RED cases**

Create `GameXXK.Data.HeroCards.Blade` and cover all four cards plus shared boundaries:

```text
OnlyFirstActiveBladeCardTriggersCharge
ChargeAffectsAnyOwnersNextActiveCard
AutomaticReplayNeitherConsumesNorCreatesCharge
EndTurnUsesOnlyLastActiveBladeCardForFinish
NonBladeLastActiveCardSuppressesFinish
TongFengChargeReplaysNextBaseOnly
TongFengFinishReplaysSourceBaseAfterNextRoundFirstActive
XueLuChargeTriggersBleedWithoutDecay
XueLuFinishWaitsForFirstActiveAttackAgainstBleedingTarget
YingFengChargeGrantsNextOwnerAgility2Counter1
YingFengFinishAtRoundStartGrantsHeroMark2Counter2
TongPaoChargeGrantsNextOwnerMomentum3
TongPaoFinishMakesBoundTargetsFirstNextRoundCardFree
```

- [ ] **Step 2: Run RED**

Run cold UBT and the Blade filter. Expected: catalog descriptors exist but no timing resolver registers or consumes them.

- [ ] **Step 3: Add active-play timing hooks without CardId checks**

On successful `ActivePlay`, capture `bFirstActiveThisRound` before incrementing the count. After base effects finish, resolve `ChargeEffects` only when the definition has `LinkedRole == Blade` and `bFirstActiveThisRound`. Replace `LastActiveCard` only for active plays. In `EndPlayerCardPhase`, before discarding the hand, find the last snapshot's effective definition and resolve its `FinishEffects` only when linked to Blade.

Use the append-only triggers introduced in Task 1: Momentum/status grants that must affect the next card run at `BeforeNextActiveCard`; replays run at `AfterNextActiveCard`; delayed source grants run at `NextPlayerRoundStart`; free-cost effects run at `BeforeFirstActiveCardNextPlayerRound`; source replays run at `AfterFirstActiveCardNextPlayerRound`; and the bleed payoff uses `FirstActiveAttackAgainstStatusNextPlayerRound`. Modifiers receive the source snapshot and original anchor target; only `ActivePlay` notifies them.

- [ ] **Step 4: Implement replay/status/bleed actions generically**

`ReplayTriggeredCardBase` queues the just-finished active snapshot with origin `AutomaticReplay`. `ReplaySourceCardBase` queues the modifier's source snapshot. The bleed-preserving trigger snapshots Bleed, invokes the existing Bleed trigger damage, and restores the snapshot layer count only for this explicitly flagged modifier. Status grants target the triggered card owner through the played snapshot.

- [ ] **Step 5: Run GREEN and foundation regression**

Run cold UBT; run `GameXXK.Data.HeroCards.Blade` and `GameXXK.Data.HeroCards.Foundation`. Expected: all leaves pass and automatic replay interruption still resumes correctly.

- [ ] **Step 6: Commit Task 6**

```powershell
git add Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/Tests/GameXXKHeroBladeRuntimeTest.cpp
git commit -m "feat: implement blade charge and finish"
```

### Task 7: Implement Guard armor conversion and Healer Medicine

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKHeroHealerRuntimeTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKHeroGuardRuntimeTest.cpp`

- [ ] **Step 1: Write Guard and Healer RED cases**

Create `GameXXK.Data.HeroCards.Guard` for exact Armor/Block values, highest-armor stable tie choice, `100% Attack + Armor`, and Xuan Jia's `100% + 20% * consumed armor` group packet. Assert only Xuan Jia consumes armor and zero armor still produces 100%.

Create `GameXXK.Data.HeroCards.Healer` with:

```text
PartyHealthLossStopsAtOne
YiXueCountsOnlyActualLossAndAwardsMedicineOnce
SixMedicineGrantedAtOnceAlsoGrantsOneMomentum
MedicineCanExceedTheRetiredEightStackCap
FriendlyReverseCardHealsTenPlusSnapshotAndCleanses
EnemyReverseCardLosesTenPlusSnapshotIgnoringDefenseAndArmor
MedicineCreatedDuringResolutionSurvivesOldSnapshotConsumption
GroupHealAppliesSixPlusSnapshotToEveryAllyButConsumesOnce
DuHuoAppliesPoison6Burn2ThenExplodesThenGrantsMedicine6
BaiCaoAddsOnlyPoison1Burn1PerEnemy
ToxicExplosionNeverTriggersRot
```

- [ ] **Step 2: Run RED**

Run cold UBT and both focused filters. Expected: unsupported effect-type failures, missing HighestArmorAlly resolution, and old Medicine semantics.

- [ ] **Step 3: Implement Guard effects through the shared packet resolver**

Resolve `HighestArmorAlly` by descending armor and then ascending `StableSortOrder`. `DamagePercentAttackPlusArmor` uses that source's current Attack and Armor in one requested packet. `DamageAllPercentAttackPerConsumedArmor` snapshots and consumes all selected ally armor once, computes `Magnitude + SecondaryMagnitude * consumed`, and emits one group packet per living enemy through the normal direct-damage path.

- [ ] **Step 4: Implement Medicine and nonlethal loss atomically**

Rename the player-facing meaning of `Medicine` to 药效 but retain its serialized enum value, and remove the retired eight-stack cap so repeated blood-change synergies can accumulate the resource until a healing/reversal action consumes it. `LoseHealthNonlethal` applies `min(requested, HP - 1)` and reports actual loss. `GainMedicineFromPartyHealthLoss` sums actual loss events produced by the preceding action and grants `2 * affected ally count` once; if that one grant is at least six, also grant Momentum1.

`HealOrReverseWithMedicine` consumes the owner's full Medicine snapshot before changing health. For an ally, call `HealCombatUnit(base + snapshot)` and Cleanse. For an enemy, emit health-only loss of `base + snapshot`, bypassing defense/armor and not classed as direct attack. Group healing uses one owner snapshot and applies the same flat bonus to each living ally.

- [ ] **Step 5: Run GREEN plus DoT regression**

Run cold UBT; run Guard, Healer, and `GameXXK.Data.CombatStatusRedesign` reports. Expect every exact Toxic Explosion order/cause assertion to remain GREEN.

- [ ] **Step 6: Commit Task 7**

```powershell
git add Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/Tests/GameXXKHeroGuardRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKHeroHealerRuntimeTest.cpp
git commit -m "feat: add guard conversion and medicine cards"
```

### Task 8: Implement Hunter Heavy Arrow

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKHeroHunterRuntimeTest.cpp`

- [ ] **Step 1: Write Heavy Arrow RED cases**

Create `GameXXK.Data.HeroCards.Hunter` with no-Charge playability for all three Heavy Arrow attacks and exact cases:

```text
FengYan: draw2/discard1, Agility2, Charge3; no Heavy Arrow consumption.
LieYu: base140 then Bleed8; lock/consume current Charge; append one50% packet per locked layer; each append observes live Bleed.
CuiDu: base130, Poison6, one base Toxic Explosion; append one Toxic Explosion per locked layer; never add more Poison.
HuiFeng: register one pending150% attack, lock Charge, add40 percentage points per layer into that same packet, draw once per layer, and at 3+ layers gain energy1.
NewChargeCreatedAfterLockSurvivesForTheNextCard.
HeavyArrowSegmentsDoNotAdvanceActiveCountersTasksOrTerrainListeners.
ChargeCanAccumulateAtLeastThreeLayersAndHasNoLegacyOneStackCap.
```

For Charge3, assert Hui Feng emits exactly one `270%` main packet, draws three, gains one energy, and consumes exactly three Charge.

- [ ] **Step 2: Run RED**

Run cold UBT and the Hunter filter. Expected: base effects may pass, but Charge remains live and Heavy Arrow audit values are absent.

- [ ] **Step 3: Add the post-base Heavy Arrow hook**

Remove Charge's obsolete one-stack cap. Only after base effects finish, snapshot and remove all live Charge from the card owner. Dispatch by `Definition.HeavyArrow.Kind`, not ID. Queue extra attacks with origin `HeavyArrow`; queue Toxic Explosions in the existing Bleed/Poison/Burn atomic order; for the primary-attack modifier, delay only the definition's first attack packet until Charge is known, merge the percentage, then submit one packet.

Expose audit fields on `FGameXXKCardPlayResult`:

```cpp
int32 HeavyArrowChargeConsumed = 0;
int32 HeavyArrowExtraAttackCount = 0;
int32 HeavyArrowToxicExplosionCount = 0;
int32 HeavyArrowPrimaryBonusPercent = 0;
```

- [ ] **Step 4: Run GREEN and Mark/DoT regression**

Run cold UBT; run Hunter, MarkRules, and CombatStatusRedesign reports. Inspect that every extra attack consumes Mark/Bleed according to the live shared resolver and that Toxic Explosion still excludes Rot.

- [ ] **Step 5: Commit Task 8**

```powershell
git add Source/GameXXK/Public/GameXXKCardTypes.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/Tests/GameXXKHeroHunterRuntimeTest.cpp
git commit -m "feat: implement protagonist heavy arrow cards"
```

### Task 9: Implement the eight-card Mage task and existing-panel search

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardBattleAdapter.h`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKHeroMageRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`

- [ ] **Step 1: Write Mage task RED cases**

Create `GameXXK.Data.HeroCards.Mage` with:

```text
FirstActiveHeroMageCardLocksExactlyEightEquippedHeroIds
PartnerNpcRouteAndTemporaryCardsNeverEnterTask
DuplicateHeroIdDoesNotReplaceMissingId
AllEightFirstPlaysAreRecordedInOrderWithOwnerAndTarget
TaskReplaysEightBaseEffectsInFirstPlayOrder
OnlyStarterMageRewardRuns
ReplayAndRewardNeverStartOrAdvanceAnotherTask
DeadOriginalTargetFallsBackToSameSideStableFirst
MissingTargetSkipsOnlyDependentEffects
SearchOffersOnlyUnfinishedEquippedHeroCardsInDrawOrDiscard
SearchNeverCopiesCardsAlreadyInHandOrCurrentlyResolving
SearchUsesExistingPendingChoiceAndResumesQueue
ReplayForcedDiscardPausesAndResumesBeforeReward
```

Add exact reward cases: Fire gives Burn8 then triggers Burn twice per enemy; Ice consumes all owner armor and emits `20% Attack` per consumed point to all enemies; Lightning gives Mark3, locks each enemy's stack count, and emits that many 60% packets despite live Mark consumption; Universal draws4, gains energy2, and reduces hero-card energy by one for the rest of the round.

- [ ] **Step 2: Run RED**

Run cold UBT and the Mage filter. Expected: no task starts, no equipped-eight snapshot exists, and search cannot open the new choice kind.

- [ ] **Step 3: Snapshot equipped hero IDs at battle start**

Immediately after `InitializeCardBattleRuntime`, assign `Run.HeroSelectedCardIds` to `NewRuntime.EquippedHeroCardIds` and validate exactly eight unique catalog Hero IDs. Never derive this list from the materialized 18+ card battle deck.

- [ ] **Step 4: Implement active-play task bookkeeping**

After target/cost validation and moving the active instance, but before resolving its base effects, initialize an inactive task when the definition is Hero with `LinkedRole == EGameXXKCharacterRole::Sorcerer`, then record the current locked Hero CardId/snapshot once. This ordering lets a starter Mage card's own search see the task and exclude the already-completed/currently-resolving card. Resolve the active card's complete base effect list first; only afterward, when all eight IDs are complete and no choice from that base list is pending, copy the ordered snapshots and starter reward into `AutomaticResolutionQueue`. If the eighth base list opens a choice, leave the completed task persisted and have every runtime choice-submission wrapper call `TryStartCompletedHeroSpellTaskQueue` after clearing the choice but before resuming a queue. Reset the task only after the replay queue and reward complete, and reject a new task while either state is active.

- [ ] **Step 5: Implement search and four reward operations**

`SearchUnfinishedHeroTaskCard` collects real instances from DrawPile and DiscardPile whose CardId is locked but unfinished, sorts by `AcquisitionOrdinal`, and opens `HeroTaskSearchChooseToHand`. Submission moves the selected real instance to Hand if capacity permits; it never materializes a copy. Extend `PendingChoicePanel` only by changing prompt text and routing the existing candidate button to `SubmitHeroTaskSearchChoice`; do not change panel slots, anchors, padding, colors, or dimensions.

Declare the runtime rule and matching adapter operation in this task:

```cpp
GAMEXXK_API bool SubmitHeroTaskSearchChoice(
    FGameXXKCardBattleRuntime& InOutRuntime,
    FName PickedInstanceId,
    TArray<FGameXXKCardPlayResult>& OutResumedResults,
    FString* OutError = nullptr);
```

The adapter uses the Task 3 optional-output convention, commits on a temporary state, resumes the automatic queue, and rolls back the search selection if any later replay or reward fails.

For Insight, forced discard, cancel Insight, and Mage search, capture the pre-submission runtime, request resumed results from the adapter, flatten their `DamageResults`, and feed them to the existing `QueueMutationPresentation` path before refreshing. This presents post-choice automatic damage/healing in order without adding a widget or bypassing the existing animation queue.

Implement `GainArmorFromCurrentManaPercent`, `GainManaOverflowToArmor`, `TriggerStatus`, and `LightningPerTargetStatusSnapshot` as generic effect operations used by the reward resolver. The Ice base effect floors `current Mana * 25 / 100`, then restores six Mana and converts only overflow to Armor one-for-one.

- [ ] **Step 6: Add a no-layout-drift widget assertion**

In `GameXXKCardBattleBoardWidgetTest.cpp`, create a pending Mage search state, refresh the widget, and assert the same `PendingChoicePanel` instance, anchors, offsets, and candidate-card size used by Insight. Assert prompt `选择一张尚未完成任务的主角牌` and that clicking a candidate invokes the Mage submission path.

- [ ] **Step 7: Run GREEN and UI/runtime regression**

Run cold UBT; run `GameXXK.Data.HeroCards.Mage`, `GameXXK.Integration.CardBattle.Board`, and `GameXXK.Data.HeroCards.Foundation`. All leaves must run; no layout golden value may change.

- [ ] **Step 8: Commit Task 9**

```powershell
git add Source/GameXXK/Public/GameXXKCardRules.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Public/GameXXKCardBattleAdapter.h Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Private/Tests/GameXXKHeroMageRuntimeTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp
git commit -m "feat: add protagonist eight card spell task"
```

### Task 10: Implement Formation terrain benefits and listeners

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKHeroFormationRuntimeTest.cpp`

- [ ] **Step 1: Write the complete terrain matrix RED test**

Create `GameXXK.Data.HeroCards.Formation`. For each of the four cards and each terrain, assert preview and commit retain one manual enemy target. Assert one benefit execution equals:

```cpp
Plain      -> selected enemy Burn +2
Cliff      -> selected enemy Vulnerability +2 and Mark +1
Forest     -> every living ally heals 4
WaterShore -> every living ally gains Mana 3
Ferry      -> every living ally gains Mana 3
Village    -> draw 1 and every living ally gains Armor 4
Cave       -> every living ally gains Armor 8 and Block 1
```

Also assert Guan Shi `80% + benefit + draw1`; Yi Zhen two benefits, or three plus energy1 only after a real terrain change this round; Lian Ying registers exactly three active-card triggers, reads terrain at each trigger, and retargets a dead anchor by enemy stable order; Liu He executes Plain, Cliff, Forest, WaterShore, Village, Cave, then current terrain, without changing terrain or setting the switch flag.

- [ ] **Step 2: Run RED**

Run cold UBT and the Formation filter. Expected: unsupported terrain effect/listener failures; existing target-mode compatibility must remain GREEN.

- [ ] **Step 3: Implement one shared terrain-benefit resolver**

Declare:

```cpp
GAMEXXK_API bool NotifyTerrainChanged(
    FGameXXKCardBattleRuntime& InOutRuntime,
    EGameXXKCardTerrain NewTerrain,
    FString* OutError = nullptr);
```

It validates a concrete changed terrain, assigns it, and sets `bTerrainChangedThisRound = true`. Reset the flag at the next player-round boundary. `TriggerTerrainBenefit` accepts the original enemy anchor and an explicit terrain override; when no override is supplied it reads `Runtime.Terrain` at execution time.

- [ ] **Step 4: Make the three-listener rule active-play only**

Register one persistent modifier with three remaining triggers and its anchor. Only origin `ActivePlay` consumes it after that card's resolution; AutomaticReplay, MageTaskReplay, HeavyArrow, Reaction, TerrainListener, TaskReward, Toxic Explosion, and terrain benefit operations never notify it. Each trigger chooses the living original anchor or first living enemy by stable order.

- [ ] **Step 5: Run GREEN and compatibility regression**

Run cold UBT; run `GameXXK.Data.HeroCards.Formation`, `GameXXK.Integration.MarkCardCompatibility`, and `GameXXK.Data.CardBattleRuntime`. Expect all terrain combinations and existing Formation tests to pass.

- [ ] **Step 6: Commit Task 10**

```powershell
git add Source/GameXXK/Public/GameXXKCardRules.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/Tests/GameXXKHeroFormationRuntimeTest.cpp
git commit -m "feat: add protagonist formation terrain payoffs"
```

### Task 11: Replace card/status wording without changing UI layout

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardText.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardTextTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`

- [ ] **Step 1: Write concise-tooltip RED assertions**

Create/extend `GameXXK.UI.Battle.StatusTooltips` so every tooltip has a title, layer line, and at most two short rule lines. Lock these exact player-facing keyword strings:

```text
冲锋：本回合第一张主动牌时触发。
收招：作为结束回合前最后一张主动牌时触发。
重箭：消耗全部蓄力，逐层触发本牌重箭效果。
药效：下一次治疗或治疗反转每层＋1；结算时全部消耗。
反击：敌方单体攻击牌结算后，造成100%攻击并消耗1次。
格挡：敌方单体攻击牌结算后，造成100%攻击＋当前护甲并消耗1次。
当前地势收益：按当前地势触发对应效果。
法术任务：主角8张装备牌各主动打出一次后，依序重放基础效果并触发首牌奖励。
```

Lock these exact concise state rules. They retain the previously approved table except where the later protagonist specification explicitly replaced Agility, Counter/Block, and Medicine:

```text
护甲：优先抵挡直接攻击伤害；所属阵营回合开始时清空。
气势：每层使每段攻击伤害+1；仅指定牌与驱散会消耗。
灵动：25%概率消耗1层完美闪避；失败时可消耗2层闪避。
破绽：每层使下一段直接攻击伤害提高10%；结算后清空。
流血：受到直接攻击后，失去等同层数的生命并减少1层；回合结束不衰减。
中毒：回合结束时，失去等同层数的生命并减少1层。
灼烧：打出牌或执行意图后，失去等同层数的生命并减少1层；回合结束再减少1层。
标记：直接攻击伤害提高15%；每段有效命中后减少1层。
守护：下一次针对本单位的单体攻击由守护者承受；触发后减少1层。
蚀伤：流血、中毒或灼烧造成伤害时，额外失去等同层数的生命；回合结束减少1层。
药效：下一次治疗或治疗反转每层+1；结算时全部消耗。
反击：敌方单体攻击牌结算后，造成100%攻击并消耗1次。
格挡：敌方单体攻击牌结算后，造成100%攻击＋当前护甲并消耗1次。
破绽免疫：无法获得新的破绽；不会自行消耗。
追击标记：下一次攻击的首段命中施加1层标记；出手后减少1层。
破绽追击：下一次攻击的首段命中施加1层破绽；出手后减少1层。
疗愈增幅：下一次治疗中，每个目标的治疗量增加等同层数的数值；结算后清空。
地形双效：队伍下一张地形牌的地形条件效果额外结算1次；使用后减少1层。
地形免耗：队伍下一张地形牌的气力消耗变为0；使用后减少1层。
地形减耗：队伍下一张地形牌的气力消耗-1；使用后减少1层。
代挡：替队友承受下一次敌方单体攻击；触发后减少1层。
本回合地形双效：本回合队伍下一张地形牌的地形条件效果额外结算1次；使用或回合结束时清除。
虚弱：直接攻击伤害降低50%；回合结束减少1层。
财富：钱潮冲击每层伤害+15；散财疗伤最多消耗3层，每层回复6%最大生命。
狂怒：受到玩家牌的生命伤害时增加1层；怒獠每层伤害+20。
猎物：老虎锁定的目标；虎扑将攻击该单位。
蓄力：层数表示剩余蓄力回合；归零后执行已准备的意图。
```

For every visible state, lock the final shape as exactly `状态名称\n层数：真实整数\n一句规则`; values above 99 show `99+` only on the icon while the tooltip retains the full integer. No tooltip may contain the retired `效果：` or `时机：` prefixes.

- [ ] **Step 2: Run RED**

Run cold UBT; run `GameXXK.UI.Battle.StatusTooltips` and `GameXXK.Integration.CardText`. Expected: stale long `效果/时机` phrasing, 药材 naming, old DoT text, and missing Block mapping fail.

- [ ] **Step 3: Replace text construction, not geometry**

Change `MakeStyle` to accept one concise rule string and build:

```cpp
Style.Tooltip = Rule;
```

Keep `DescribeStatusTooltip` as `DisplayName + layer count + rule`. Add `BlockShield` fallback glyph and map Block separately. Preserve priority ordering and every widget slot/anchor/size. Replace the stale scene-actor assertions that read separate `Effect`/`Timing` fields with the same final tooltip contract; do not keep a second wording authority. Extend `GameXXKCardText` switch cases for all appended effect types and add a compact keyword suffix derived from definition descriptors.

- [ ] **Step 4: Run GREEN and widget layout regression**

Run cold UBT; run the two focused filters plus `GameXXK.UI.Battle.StatusEffectsWidget`, `GameXXK.MVP.Battle.SceneActors`, and `GameXXK.Integration.CardBattle.Board`. Inspect screenshots only if an existing widget visual test emits them; no new art asset is required.

- [ ] **Step 5: Commit Task 11**

```powershell
git add Source/GameXXK/Private/GameXXKCardText.cpp Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp Source/GameXXK/Private/Tests/GameXXKCardTextTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp
git commit -m "feat: simplify battle card and status tooltips"
```

### Task 12: Full integration, deterministic simulations, and acceptance record

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKHeroCardIntegrationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBalanceObservationTest.cpp`
- Modify: `scripts/run_card_balance_observation.py`
- Create: `docs/production/protagonist-card-runtime-acceptance.md`
- Modify only if a failing focused test proves a production defect: files already listed in Tasks 1-11

- [ ] **Step 1: Write cross-system integration scenarios before final fixes**

Create `GameXXK.Data.HeroCards.Integration` with deterministic decks and fixed seeds for:

```text
BladeReplayOfHunterDoesNotDoubleConsumeCharge
MageReplayOfFormationDoesNotConsumeTerrainListener
HealerToxicExplosionCanFinishMageFireQueueWithoutPartialCommit
DodgedEnemyMultiHitStillProducesOneCounterAndOneBlockPerSource
EightHeroFivePartnerThreeNpcCompositionRemainsExact
LevelOneAndLevelTwentyLoadoutsStartAndResumeBattle
SaveReloadDuringForcedDiscardContinuationIsByteStable
SaveReloadDuringMageSearchContinuationIsByteStable
BothSidesDieDuringFinalReactionAndPlayerWins
```

- [ ] **Step 2: Run the integration RED/GREEN loop**

Run cold UBT and `GameXXK.Data.HeroCards.Integration`. If a case fails, identify the first incorrect state transition, add the smallest focused assertion to the owning Task test, implement the minimal shared fix, rerun that focused filter, then rerun Integration. Do not weaken expected values or add CardId branches.

- [ ] **Step 3: Run focused protagonist simulations and the locked 2,400-case balance observation**

First add a test-only simulation loop inside the integration test using fixed seeds `1101..1200`, legal eight-card protagonist selections representing Generic, Blade, Guard, Healer, Hunter, Mage, and Formation mixes, and the existing Chapter 1 standard encounter fixtures. Record without changing balance automatically:

```text
battle outcome
round count
cards actively played
automatic-resolution count
energy spent/gained
mana spent/gained
damage by direct/DoT/reaction/task/terrain origin
healing and armor generated
unused cards stranded by target legality
maximum queue depth
```

Fail only on invariant defects: an unplayable card with a legal target, negative resources, unresolved queue, more than 20 cards in hand, invalid zone ledger, recursive queue depth above 64, or a battle that exceeds 100 rounds. Write aggregate observations into the acceptance document; do not tune approved numbers without a separate user confirmation.

Then extend `GameXXKCardBalanceObservationTest.cpp` to record the new Heavy Arrow, Reaction, TaskReward, TerrainListener, queue-depth, and stranded-target fields while retaining the exact existing 2,400-case matrix. Run:

```powershell
python scripts/run_card_balance_observation.py --once --ue-editor 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
```

Expected: exactly 2,400 rows, zero execution errors, a stable output hash on a second identical run, and an aggregate comparison against `docs/design/2026-08-08-card-balance-analysis.md`. Report victory/defeat/stalemate, median/P90 rounds, damage by origin, energy/mana flow, wasted statuses, queue depth, and stranded legal-target failures. Treat the results as evidence only; any proposed number changes require another user review.

- [ ] **Step 4: Run the complete required suite in fresh processes**

Run one cold UBT, then these filters in independent editor processes and report folders:

```text
GameXXK.Data.HeroCards
GameXXK.Data.CardBattleRuntime
GameXXK.Data.CombatStatusRedesign
GameXXK.Data.MarkRules
GameXXK.Integration.MarkCardCompatibility
GameXXK.Battle.EnemyIntentRules
GameXXK.Data.CardCatalog
GameXXK.Integration.CardText
GameXXK.UI.Battle.StatusEffectsWidget
GameXXK.Integration.CardBattle.Board
GameXXK.MVP.SaveGame
```

For each `index.json`, record test count, succeeded, succeeded-with-warnings, failed, and not-run in `docs/production/protagonist-card-runtime-acceptance.md`. Distinguish existing warning logs from new errors; any failed or not-run leaf blocks completion.

- [ ] **Step 5: Run the real PIE acceptance flow without changing layout**

If no editor is running, launch the built editor once with the project and wait for the UE MCP smoke report to pass:

```powershell
Start-Process -FilePath 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' -ArgumentList @('"D:\UE5 demo\GameXXK\GameXXK.uproject"') -WindowStyle Hidden
```

If an editor is already running, save dirty packages through MCP first and reuse that process; never force-close it. Then run:

```powershell
python scripts/ue_mcp_smoke.py --tool-timeout 60 --report 'Saved/Automation/HeroCards_UE_MCP_Smoke.json'
python scripts/gamexxk_real_play_flow_mcp.py --timeout 60 --keep-pie --report 'Saved/Automation/HeroCards_RealPlay.json'
python scripts/gamexxk_real_play_flow_mcp.py --timeout 60 --battle-hud-observation --report 'Saved/Automation/HeroCards_BattleHud.json'
```

Inspect the JSON rather than only the exit code. Require the existing main-menu → inn → quest interaction/save → route map → battle path and the current battle HUD geometry verdict to remain successful. Use the exact widget Automation assertions from Task 11 as the hover-text authority for Counter, Block, Medicine, Bleed, Poison, and Burn, and record both runtime and widget evidence in the acceptance document. Do not move, resize, recolor, or replace any existing inventory/battle layout control while addressing a failure.

- [ ] **Step 6: Verify repository scope**

Run:

```powershell
git status --short
git diff --check
git diff --name-only HEAD
```

Expected: only the planned tracked files appear; unrelated untracked art/build content remains untouched. Inspect every staged path explicitly with `git diff --cached --name-only` before committing.

- [ ] **Step 7: Commit Task 12**

```powershell
git add Source/GameXXK/Private/Tests/GameXXKHeroCardIntegrationTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBalanceObservationTest.cpp scripts/run_card_balance_observation.py docs/production/protagonist-card-runtime-acceptance.md
git add Source/GameXXK/Public/GameXXKCardTypes.h Source/GameXXK/Public/GameXXKCardCatalog.h Source/GameXXK/Public/GameXXKCardRules.h Source/GameXXK/Public/GameXXKCardBattleAdapter.h Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/GameXXKCardQualityRules.cpp Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Public/MVP/GameXXKSaveMigration.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp Source/GameXXK/Private/GameXXKCardText.cpp Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp
git commit -m "test: verify protagonist card runtime integration"
```

## Spec-to-task traceability

| Approved requirement | Owning task |
| --- | --- |
| 36 exact IDs, costs, targets, values, 12+24 grouping | Task 2 catalog table and catalog tests |
| Initial 8, levels 5/10/15/20, all 24 linked immediately available | Task 2 unlock helper and v12 migration |
| Hero8 + partner5 + NPC3; route cards separate | Tasks 2, 9, and 12 integration |
| Active play versus automatic effects | Task 3 origin-aware resolver |
| Automatic replay targeting and interrupted choices | Tasks 3 and 9 |
| Exhaust cards | Tasks 1 and 3 |
| Generic cards | Task 4 |
| Agility 25% perfect/2-stack normal dodge | Tasks 1, 2, and 5 |
| Counter/Block split, dodge, multi-hit, expiry, no recursion | Task 5 |
| Blade Charge/Finish and arbitrary next active card | Task 6 |
| Guard armor conversion | Task 7 |
| Medicine, healing reversal, nonlethal party loss, Toxic Explosion | Task 7 |
| Heavy Arrow order and Charge snapshot | Task 8 |
| Eight-card Mage task, search, four rewards | Task 9 |
| Six terrain benefits and active-card listener | Task 10 |
| Concise tooltip language and unchanged layout | Task 11 |
| Atomic terminal queue and simultaneous death victory | Tasks 3, 5, and 12 |
| Save/reload continuity | Tasks 2, 3, 9, and 12 |
| Balance data without unapproved automatic tuning | Task 12 |

## Plan self-review result

- Every section of the approved protagonist specification maps to a named task and an Automation assertion.
- Every new type used by later tasks is introduced in Task 1; every public API is declared in the same task that implements it.
- Serialized enums are append-only and the plan retains legacy numeric assertions.
- Automatic replay, Heavy Arrow, task replay, reactions, and terrain listeners share one origin-aware resolver and cannot advance one another accidentally.
- The existing pending-choice panel is reused; no inventory or battle layout geometry is authorized to change.
- Talent and title UI remain outside scope until their pages/tabs are separately discussed and confirmed.
