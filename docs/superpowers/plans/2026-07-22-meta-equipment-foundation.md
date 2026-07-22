# Meta Equipment Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: use `superpowers:subagent-driven-development` or `superpowers:executing-plans` task by task. Use `superpowers:test-driven-development` for every gameplay change and `superpowers:verification-before-completion` before reporting success.

**Goal:** Replace the legacy three-slot static-item equipment authority with save-authoritative equipment instances, a 200-slot warehouse, six explicit loadout slots for the hero and permanent companions, deterministic affixes, enhancement/reforge/dismantle transactions, failure-safe save migration, battle-ready stat/effect projections, and a deterministic headless simulation foundation.

**Architecture:** `FGameXXKRuntimeState::EquipmentCollection` is the permanent equipment authority. It stores every instance, the ordered warehouse IDs, and six-slot loadouts. Instance owner fields are redundant integrity data and must always agree with the warehouse/loadout indexes. Immutable definitions are split into `EquipmentCatalog`, `AffixCatalog`, and `EquipmentSetCatalog`. `FGameXXKEquipmentRules` owns collection-only queries and mutations. `FGameXXKEquipmentEconomyRules` owns every transaction that crosses equipment and another permanent resource and therefore copy-validates-commits a complete `FGameXXKRuntimeState`. Character base attributes come from one shared naked-stat API; battle and simulation consume the same equipment and card rules.

**Tech stack:** Unreal Engine 5.8, C++20/UE reflection, UE Automation tests, pure UMG-independent rules, project UE MCP scripts, UBT cold builds only.

**Authoritative specification:** [`docs/superpowers/specs/2026-07-22-meta-equipment-partner-three-chapter-route-design.md`](../specs/2026-07-22-meta-equipment-partner-three-chapter-route-design.md)

---

## Execution guardrails

- Work in `D:\UE5 demo\GameXXK` on `main`. Do not create a worktree and do not use UnrealBridge.
- Before each task run `git status --short` and `git diff -- <exact-paths>`. The worktree contains extensive user-owned edits; never reset, restore, replace, or auto-format unrelated hunks.
- Prefer the new focused files listed below. Changes to `GameXXKMVPRules.*`, `GameXXKMVPSubsystem.*`, companion rules, card types, and existing tests must be isolated feature hunks.
- If the editor is running, `scripts/ue_tdd_pipeline.py` must save dirty packages through UE MCP before it closes the editor. If saving fails, stop without force-closing.
- Live Coding, Hot Reload, and `--check-only` are not compile verification.
- Do not touch character sprites, PaperZD assets, maps, cameras, HD2D transforms, manually tuned widgets, or UI textures in this plan.
- Every random result uses saved seeds and saved monotonic ordinals. Tests use fixed seeds and compare complete serialized results.
- Every mutating rule changes a candidate copy, validates the whole resulting state, and assigns it back only after success. Failure leaves resources, warehouse order, loadouts, instances, ordinals, pending previews, roster, and legacy mirrors unchanged.
- Execute this plan before the meta-shop/UI and three-chapter route plans.
- This plan owns global save version `7` and equipment schema version `1`. The older route-merchant plan must use a nested route schema or the next global version; two migrations may not assign different meanings to version 7.

## Mandatory red/green protocol for every implementation task

1. Add a focused failing automation test. Existing types may receive the smallest compile-only declaration needed for the test to build; do not implement the behavior yet.
2. Run a cold cycle. If UBT fails on an expected missing declaration, record that compile-red result and do not call MCP against the stale/closed editor; add only compile-only declarations and rerun the cold cycle. Once UBT succeeds, run the focused MCP filter and record the expected behavior assertion failure. For Task 1, the initial missing-header UBT failure is acceptable but does not replace the later MCP behavior-red result.
3. Implement the smallest complete behavior for the task.
4. Run the cold pipeline again so the editor loads the newly built DLL:

   ```powershell
   python scripts/ue_tdd_pipeline.py
   ```

5. Only after the cold pipeline succeeds, discover and run the focused tests through UE MCP:

   ```powershell
   $env:GAMEXXK_AUTOMATION_FILTER = "StartsWith:GameXXK.Equipment.Types"
   @'
   import os
   from scripts.ue_mcp_client import UnrealMCPClient

   toolset = "AutomationTestToolset.AutomationTestToolset"
   client = UnrealMCPClient(timeout=60.0)
   assert client.connect(), "UE MCP is unavailable"
   print(client.call_tool(
       "DiscoverTests",
       {"bForceRediscover": True},
       toolset_name=toolset,
       timeout=180.0,
   ))
   print(client.call_tool(
       "RunTestsByFilter",
       {"filterExpression": os.environ["GAMEXXK_AUTOMATION_FILTER"]},
       toolset_name=toolset,
       timeout=900.0,
   ))
   '@ | python -
   ```

   Before each invocation, set the first line to that step's exact `StartsWith:` filter. When a step names multiple filters, run the complete block once per filter.

6. Record the returned test count and zero failures. A successful UBT build without the MCP test result is not a green task.
7. Run `git diff --check`. Commit only when all required dependency hunks can be staged together; never create a commit that depends on an intentionally unstaged header or runtime-state hunk. Never use `git add -A`.

---

## Frozen public contracts

These names and ownership rules are dependencies for the shop/UI and route plans. Change them only by updating all dependent plans in the same review.

### Authoritative equipment state

Create `Source/GameXXK/Public/GameXXKEquipmentTypes.h` with explicit reflected enums. Each enum has `Invalid = 0`; valid values start at 1 so malformed/default data cannot masquerade as a valid saved value.

```cpp
UENUM(BlueprintType)
enum class EGameXXKEquipmentSlot : uint8
{
    Invalid = 0 UMETA(Hidden),
    Weapon = 1,
    Head = 2,
    Armor = 3,
    Belt = 4,
    Shoes = 5,
    Accessory = 6
};

UENUM(BlueprintType)
enum class EGameXXKEquipmentSet : uint8
{
    Invalid = 0 UMETA(Hidden),
    Legacy = 1,
    PoJun = 2,
    XuanJia = 3,
    QingNang = 4,
    ZhuiFeng = 5,
    ShiGu = 6,
    ShanHe = 7
};

UENUM(BlueprintType)
enum class EGameXXKEquipmentQuality : uint8
{
    Invalid = 0 UMETA(Hidden),
    Common = 1,
    Rare = 2,
    Epic = 3
};

UENUM(BlueprintType)
enum class EGameXXKAffixTier : uint8
{
    Invalid = 0 UMETA(Hidden),
    Common = 1,
    Rare = 2,
    Epic = 3
};

UENUM(BlueprintType)
enum class EGameXXKEquipmentOwnerKind : uint8
{
    Invalid = 0 UMETA(Hidden),
    Warehouse = 1,
    Hero = 2,
    PermanentCompanion = 3
};

UENUM(BlueprintType)
enum class EGameXXKEquipmentScalingRule : uint8
{
    Invalid = 0 UMETA(Hidden),
    ModernPercentBase = 1,
    LegacyFlatPerEnhancement = 2
};

UENUM(BlueprintType)
enum class EGameXXKEquipmentMagnitudeUnit : uint8
{
    Invalid = 0 UMETA(Hidden),
    BasisPoints = 1,
    FlatCount = 2
};
```

`EGameXXKEquipmentModifierKind` must contain the five universal stats plus the approved thirty set-specific families: maximum health, maximum mana, attack, defense, speed; direct/multi-hit damage, armor-break stacks, vulnerable-target damage, first-attack damage; armor gain/retention, counter damage, guard reduction, low-health protection; healing, cleanse, overheal conversion, mana recovery, emergency healing; draw, low-cost bonus, shared energy, combo count, temporary cost reduction; poison, bleed, burn, damage over time, status retention; terrain power/cost reduction, adjacent ally power, formation power, and team terrain power. Discrete values use `FlatCount`; percentages use `BasisPoints`.

Define these exact save structures:

```cpp
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCharacterStats
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 MaxHealth = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 MaxMana = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 Attack = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 Defense = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 Speed = 0;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEquipmentAffixRoll
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName AffixId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKAffixTier Tier = EGameXXKAffixTier::Invalid;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 Magnitude = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKEquipmentMagnitudeUnit Unit = EGameXXKEquipmentMagnitudeUnit::Invalid;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEquipmentInstance
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName InstanceId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName BaseEquipmentId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 ItemLevel = 1;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKEquipmentQuality Quality = EGameXXKEquipmentQuality::Invalid;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 EnhancementLevel = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TArray<FGameXXKEquipmentAffixRoll> RolledAffixes;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 AcquisitionSeed = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKEquipmentScalingRule ScalingRule = EGameXXKEquipmentScalingRule::Invalid;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FGameXXKCharacterStats LegacyBaseStatSnapshot;

    // Redundant integrity data. Rules synchronize these from WarehouseInstanceIds/CharacterLoadouts.
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKEquipmentOwnerKind OwnerKind = EGameXXKEquipmentOwnerKind::Invalid;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName OwnerCharacterId = NAME_None;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEquipmentLoadout
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName WeaponInstanceId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName HeadInstanceId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName ArmorInstanceId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName BeltInstanceId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName ShoesInstanceId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName AccessoryInstanceId = NAME_None;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPendingEquipmentReforge
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) bool bActive = false;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName InstanceId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 AffixIndex = INDEX_NONE;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FGameXXKEquipmentAffixRoll OriginalAffix;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FGameXXKEquipmentAffixRoll CandidateAffix;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 PaidRefinementSand = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 ConsumedReforgeOrdinal = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEquipmentCollectionState
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TArray<FGameXXKEquipmentInstance> EquipmentInstances;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TArray<FName> WarehouseInstanceIds;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TMap<FName, FGameXXKEquipmentLoadout> CharacterLoadouts;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 RefinementSand = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 CollectionSeed = 1;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 NextInstanceOrdinal = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 NextReforgeOrdinal = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 EquipmentSchemaVersion = 1;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) bool bLegacyWarehouseOverflow = false;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FGameXXKPendingEquipmentReforge PendingReforge;
};
```

Add exactly one authoritative runtime field:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
FGameXXKEquipmentCollectionState EquipmentCollection;
```

`FGameXXKEquipmentRules::HeroCharacterId()` returns `Player`. Permanent companions use `FGameXXKPermanentCompanion::InstanceId`. Task NPC IDs are always invalid equipment owners.

The normal warehouse capacity is the rules constant `200`; it is not a mutable saved field. New acquisitions are rejected when the ordered warehouse has 200 entries or `bLegacyWarehouseOverflow` is true.

### Transaction result contract

Define `EGameXXKEquipmentTransactionError` with: `None`, `InvalidRequest`, `InstanceMissing`, `DefinitionMissing`, `CollectionInvalid`, `WarehouseFull`, `InvalidOwner`, `SlotMismatch`, `ItemNotInWarehouse`, `ConfirmationRequired`, `InsufficientEnhancementStones`, `MaxEnhancementReached`, `InsufficientRefinementSand`, `PendingReforgeExists`, `NoPendingReforge`, `PendingReforgeStale`, `RouteLocked`, and `SaveMigrationFailed`.

`FGameXXKEquipmentTransactionResult` contains `bSucceeded`, `Error`, `Message` (`FText`), `AffectedInstanceIds`, `bConfirmationRequired`, `EnhancementStoneDelta`, and `RefinementSandDelta`. Internal validation helpers use `FString* OutError`; transaction/UI boundaries always return the reflected result and `FText`. One pure message mapper supplies the approved Chinese strings, including `装备背包已满`, `强化石不足`, `洗炼砂不足`, `当前装备实例已不存在`, `路线进行中无法更换伙伴`, and `存档迁移失败，已保留原存档。`. Widgets never invent alternate rules errors.

### Catalog split

- `FGameXXKEquipmentCatalog` owns modern and queryable legacy base definitions. `GetPackageDefinitions()` returns exactly the 36 modern definitions; `FindDefinition()` can also resolve old definitions.
- `FGameXXKAffixCatalog` owns five universal definitions, thirty current-set definitions, tier weights, units, and magnitude ranges.
- `FGameXXKEquipmentSetCatalog` owns eighteen 2/4/6-piece descriptors.

Legacy definitions are queryable so their slot and old enhancement behavior remain valid, but they are excluded from package candidates.

Rolled creation uses this frozen request type. `Set` must be one of the six modern sets, `Quality` must be Common/Rare/Epic, and `ItemLevel` must be 1–20. Normal package creation leaves `bForceSlot=false`; only deterministic tests and migration may force a valid slot.

```cpp
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEquipmentCreateRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    EGameXXKEquipmentSet Set = EGameXXKEquipmentSet::Invalid;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    EGameXXKEquipmentQuality Quality = EGameXXKEquipmentQuality::Invalid;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    int32 ItemLevel = 1;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    bool bForceSlot = false;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    EGameXXKEquipmentSlot ForcedSlot = EGameXXKEquipmentSlot::Invalid;
};
```

### Rule layer split

`FGameXXKEquipmentRules` is the only public collection-only rule facade. Its frozen methods are:

```cpp
static FName HeroCharacterId();
static const FGameXXKEquipmentInstance* FindInstance(const FGameXXKEquipmentCollectionState&, FName InstanceId);
static int32 CountWarehouseItems(const FGameXXKEquipmentCollectionState&);
static bool HasWarehouseCapacity(const FGameXXKEquipmentCollectionState&, int32 RequiredSlots = 1);
static bool ValidateCollectionState(const FGameXXKEquipmentCollectionState&, FString* OutError = nullptr);
static bool ValidateCollectionAgainstRoster(const FGameXXKEquipmentCollectionState&, const FGameXXKCompanionRosterState&, FString* OutError = nullptr);
static bool CreateRolledInstance(FGameXXKEquipmentCollectionState&, const FGameXXKEquipmentCreateRequest&, FName& OutInstanceId, FString* OutError = nullptr);
static FGameXXKEquipmentTransactionResult EquipInstance(FGameXXKEquipmentCollectionState&, const FGameXXKCompanionRosterState&, FName CharacterId, EGameXXKEquipmentSlot Slot, FName InstanceId);
static FGameXXKEquipmentTransactionResult UnequipInstance(FGameXXKEquipmentCollectionState&, FName CharacterId, EGameXXKEquipmentSlot Slot);
static FGameXXKEquipmentTransactionResult ReturnAllEquipmentToWarehouse(FGameXXKEquipmentCollectionState&, FName CharacterId);
static bool BuildLoadoutSnapshot(const FGameXXKEquipmentCollectionState&, FName CharacterId, const FGameXXKCharacterStats& BareStats, FGameXXKEquipmentLoadoutSnapshot& OutSnapshot, FString* OutError = nullptr);
static bool BuildTooltipSnapshot(const FGameXXKEquipmentCollectionState&, FName InstanceId, FName CompareCharacterId, const FGameXXKCharacterStats& CompareBareStats, FGameXXKEquipmentTooltipSnapshot& OutSnapshot, FString* OutError = nullptr);
```

`CompareBareStats` is mandatory because final percentage-derived attribute deltas cannot be reconstructed from equipment ownership alone. A direct caller, including the plan-two presenter, resolves it through `FGameXXKCharacterStatRules::GetBareHeroStats` or `GetBareCompanionStats` and passes it immediately after `CompareCharacterId`. The subsystem facade resolves the same value from its authoritative RuntimeState before delegating. The snapshot reports the selected character's current final pre-route attributes, the final pre-route attributes after replacing that character's same-slot item with `InstanceId`, and the signed delta between those complete attribute sets. It applies naked stats, the current complete six-slot loadout, the candidate swap, enhancement, universal affixes, set-specific passive modifiers, and passive set descriptors in the fixed order below; it excludes later route-event, relic, terrain, and battle-status modifiers. No caller may approximate this delta from the candidate item in isolation.

`FGameXXKEquipmentEconomyRules` accepts the complete runtime state for cross-resource atomicity:

```cpp
static bool Equip(FGameXXKRuntimeState& InOutState, FName CharacterId, EGameXXKEquipmentSlot Slot, FName InstanceId, FGameXXKEquipmentTransactionResult& OutResult);
static bool Unequip(FGameXXKRuntimeState& InOutState, FName CharacterId, EGameXXKEquipmentSlot Slot, FGameXXKEquipmentTransactionResult& OutResult);
static bool EnhanceInstance(FGameXXKRuntimeState& InOutState, FName InstanceId, FGameXXKEquipmentTransactionResult& OutResult);
static bool BeginReforge(FGameXXKRuntimeState& InOutState, FName InstanceId, int32 AffixIndex, FGameXXKEquipmentTransactionResult& OutResult);
static bool ResolvePendingReforge(FGameXXKRuntimeState& InOutState, bool bAccept, FGameXXKEquipmentTransactionResult& OutResult);
static bool DismantleBatch(FGameXXKRuntimeState& InOutState, const TArray<FName>& InstanceIds, bool bConfirmedProtected, FGameXXKEquipmentTransactionResult& OutResult);
static bool PurchaseLegacyEquipmentForCompatibility(FGameXXKRuntimeState& InOutState, FName BaseEquipmentId, FGameXXKEquipmentTransactionResult& OutResult);
```

Each method copies the complete runtime state first. `Inventory[Item.EnhancementStone]` is authoritative; `EnhancementMaterial` is synchronized only after successful commit.

---

## File map

### New production files

- `Source/GameXXK/Public/GameXXKEquipmentTypes.h`
- `Source/GameXXK/Public/GameXXKCharacterStatRules.h`
- `Source/GameXXK/Private/GameXXKCharacterStatRules.cpp`
- `Source/GameXXK/Public/GameXXKEquipmentCatalog.h`
- `Source/GameXXK/Private/GameXXKEquipmentCatalog.cpp`
- `Source/GameXXK/Public/GameXXKAffixCatalog.h`
- `Source/GameXXK/Private/GameXXKAffixCatalog.cpp`
- `Source/GameXXK/Public/GameXXKEquipmentSetCatalog.h`
- `Source/GameXXK/Private/GameXXKEquipmentSetCatalog.cpp`
- `Source/GameXXK/Public/GameXXKEquipmentRules.h`
- `Source/GameXXK/Private/GameXXKEquipmentRules.cpp`
- `Source/GameXXK/Public/GameXXKEquipmentEconomyRules.h`
- `Source/GameXXK/Private/GameXXKEquipmentEconomyRules.cpp`
- `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- `Source/GameXXK/Public/GameXXKCombatSimulationTypes.h`
- `Source/GameXXK/Public/GameXXKCombatSimulationRules.h`
- `Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp`

### Existing production files to modify

- `Source/GameXXK/Public/GameXXKMVPRules.h`
- `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- `Source/GameXXK/Public/GameXXKCompanionTypes.h`
- `Source/GameXXK/Public/GameXXKCompanionRules.h`
- `Source/GameXXK/Private/GameXXKCompanionRules.cpp`
- `Source/GameXXK/Public/GameXXKCardTypes.h`
- `Source/GameXXK/Public/GameXXKCardRules.h`
- `Source/GameXXK/Private/GameXXKCardRules.cpp`
- `Source/GameXXK/Public/GameXXKCardBattleAdapter.h`
- `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- `Source/GameXXK/Public/MVP/GameXXKSaveGame.h`
- `Source/GameXXK/Private/MVP/GameXXKSaveGame.cpp`

### Focused tests

- `Source/GameXXK/Private/Tests/GameXXKEquipmentTypesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKCharacterStatRulesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKEquipmentCatalogTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKAffixCatalogTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKEquipmentSetCatalogTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKEquipmentRulesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKEquipmentStatRulesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKEquipmentEconomyRulesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKEquipmentCompanionReplacementTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKEquipmentBattleIntegrationTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKEquipmentFacadeTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKCombatSimulationFoundationTest.cpp`

---

## Task 1: Establish equipment state, naked character stats, Speed, and level-20 caps

**Files:**

- Create `GameXXKEquipmentTypes.h`
- Create `GameXXKCharacterStatRules.h/.cpp`
- Create `GameXXKEquipmentTypesTest.cpp`
- Create `GameXXKCharacterStatRulesTest.cpp`
- Modify `GameXXKMVPRules.h/.cpp`
- Modify `GameXXKCompanionTypes.h`
- Modify `GameXXKCompanionRules.cpp`

- [ ] **Step 1: Write the failing type/stat/cap tests.** Register `GameXXK.Equipment.Types.Contract` and `GameXXK.Equipment.CharacterStats`. Assert six valid slots, six modern sets plus `Legacy`, three qualities, three tiers, default schema 1, non-zero collection seed, empty indexes, empty pending reforge, five complete stats including Speed, and a hero that cannot pass level 20.
- [ ] **Step 2: Prove red without calling MCP against a failed build.** First run `python scripts/ue_tdd_pipeline.py`. If UBT fails because the expected new header/declarations are absent, record that compile failure as the first red result and **skip MCP**, because the editor cannot relaunch with the new test DLL. Add only the minimum compile-only declarations required by the tests, rerun the cold pipeline until UBT succeeds, and only then run the shared MCP command with `StartsWith:GameXXK.Equipment.Types` and `StartsWith:GameXXK.Equipment.CharacterStats`. Expected behavior-level red: Speed/shared naked stats/level cap assertions fail; do not implement their behavior before this failure is recorded.
- [ ] **Step 3: Add the frozen state contracts.** Add the frozen structures and `FGameXXKRuntimeState::EquipmentCollection` exactly as defined above. Keep `EquippedWeapon`, `EquippedArmor`, `EquippedAccessory`, `Inventory`, `ItemEnhancementLevels`, and `EnhancementMaterial` as deprecated compatibility mirrors.
- [ ] **Step 4: Implement naked character stats.** Implement `FGameXXKCharacterStatRules::GetBareHeroStats(Level)` by moving the existing hero formulas into one public pure function. Implement `GetBareCompanionStats(Role, Level, Star)` by moving the current role/growth/star logic out of `GameXXKCompanionRules.cpp`; make the old companion API delegate to it. Add Speed to `FGameXXKCompanionAttributes`. Lock level-one role Speed to Blade 11, Guard 8, Healer 9, Hunter 13, Sorcerer 9, and FormationMaster 10; add `floor((Level-1)/5)` before applying the existing star multiplier.
- [ ] **Step 5: Implement level caps.** Add `MaxCharacterLevel = 20`. Clamp hero and permanent companion levels to 1–20. Update `ApplyXP` so level 20 never advances and stored XP becomes 0 at the cap. Do not clamp the standalone quest-NPC test helper merely because it is passed 21; clamp the actual player/companion/route inputs at their authority boundary.
- [ ] **Step 6: Cold-build the implementation.** Run `python scripts/ue_tdd_pipeline.py`. Expected: UBT succeeds and the relaunched editor contains the new DLL.
- [ ] **Step 7: Prove green.** Run the shared MCP command for both Task 1 prefixes. Expected: discovered tests are non-zero and all pass.
- [ ] **Step 8: Commit only the complete Task 1 dependency set.**

  ```powershell
  git add Source/GameXXK/Public/GameXXKEquipmentTypes.h Source/GameXXK/Public/GameXXKCharacterStatRules.h Source/GameXXK/Private/GameXXKCharacterStatRules.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentTypesTest.cpp Source/GameXXK/Private/Tests/GameXXKCharacterStatRulesTest.cpp
  git add -p -- Source/GameXXK/Public/GameXXKMVPRules.h Source/GameXXK/Private/GameXXKMVPRules.cpp Source/GameXXK/Public/GameXXKCompanionTypes.h Source/GameXXK/Private/GameXXKCompanionRules.cpp
  git diff --cached --check
  git commit -m "feat: define equipment state and naked character stats"
  ```

Expected: a default state validates structurally, bare hero values match the former formulas at levels 1 and 20, all six companion roles produce Speed, and repeated XP cannot exceed level 20.

---

## Task 2: Implement the three immutable catalogs

**Files:**

- Create `GameXXKEquipmentCatalog.h/.cpp`
- Create `GameXXKAffixCatalog.h/.cpp`
- Create `GameXXKEquipmentSetCatalog.h/.cpp`
- Create the three catalog tests

- [ ] **Step 1: Write the failing catalog tests.** `GameXXK.Equipment.Catalog` asserts exactly 36 package definitions: one modern definition for every `(six sets × six slots)`. `GameXXK.Equipment.AffixCatalog` asserts five universal plus thirty set-specific families. `GameXXK.Equipment.SetCatalog` asserts eighteen 2/4/6 descriptors. Stable IDs use `Equipment.<Set>.<Slot>` and localized names never determine identity.
- [ ] **Step 2: Prove red.** Run the cold pipeline and the three catalog MCP prefixes. Expected: the catalog headers/rows are absent or the exact count/pool/descriptor assertions fail.
- [ ] **Step 3: Implement equipment definitions and level curves.** Define `FGameXXKEquipmentStatCurve` with `LevelOne`, `GrowthNumerator`, and `GrowthDivisor`, and define `FGameXXKEquipmentBaseStatCoefficients` with one curve for each of MaxHealth, MaxMana, Attack, Defense, and Speed. Resolve a curve as `LevelOne + floor((clamp(Level,1,20)-1) * GrowthNumerator / GrowthDivisor)`; `GrowthNumerator=0` yields no growth and every non-zero numerator requires a positive divisor. Use this deterministic initial slot table for all six modern sets; plan three tunes the table through simulation without changing its schema:

   | Slot | Level-one stats | Growth curves |
   | --- | --- | --- |
   | Weapon | Attack 2 | Attack `1/1` |
   | Head | MaxHealth 8 | MaxHealth `2/1` |
   | Armor | Defense 1, MaxHealth 4 | Defense `1/3`, MaxHealth `1/1` |
   | Belt | MaxHealth 6, MaxMana 2 | MaxHealth `1/1`, MaxMana `1/1` |
   | Shoes | Speed 1 | Speed `1/5` |
   | Accessory | MaxMana 4, Attack 1 | MaxMana `1/1`, Attack `1/4` |

   Each `a/b` entry means GrowthNumerator `a` and GrowthDivisor `b`; omitted stats use zero growth. Tests lock resolution at levels 1, 5, 10, 15, and 20.
- [ ] **Step 4: Add queryable legacy definitions.** Add `Item.IronSword`, `Item.ClothArmor`, `Item.CranePatternTalisman`, `Item.InkstonePendant`, `Item.WoodenSword`, `Item.StarterClothArmor`, and `Item.ClothTalisman`. Their slots and snapshots exactly match the existing item definitions. They use `LegacyFlatPerEnhancement`, are found by `FindDefinition`, and never appear in `GetPackageDefinitions`.
- [ ] **Step 5: Implement the affix catalog.** Add five universal affixes and five current-set affixes for each modern set. Affix uniqueness is checked by stable `ModifierKind`, not merely row ID. Initial tier weights are Common `100/0/0`, Rare `70/30/0`, and Epic `50/35/15`. Initial basis-point ranges are `300–500`, `600–900`, and `1000–1400`; flat-count ranges are `1–1`, `1–2`, and `2–3`.
- [ ] **Step 6: Implement the set catalog.** Add eighteen stable IDs `Set.<Set>.2/4/6`, correct scopes/hooks, and non-zero values. Use 500 BP for passive 2-piece percentages, 800 BP or one flat unit for 4-piece effects, and 1200 BP or one flat unit for 6-piece effects; trigger effects begin with one trigger per round. Plan three changes catalog constants, not rule branches.
- [ ] **Step 7: Enforce the visual boundary.** A modern definition may have an empty icon soft path in this foundation because visual production is later. Validation rejects malformed non-empty paths but does not require loading a missing asset. Legacy definitions use their existing icon mapping when available.
- [ ] **Step 8: Cold-build the implementation.** Run `python scripts/ue_tdd_pipeline.py`. Expected: UBT succeeds with all three catalogs linked.
- [ ] **Step 9: Prove green.** Run the shared MCP command with `GameXXK.Equipment.Catalog`, `GameXXK.Equipment.AffixCatalog`, and `GameXXK.Equipment.SetCatalog`. Expected: all exact counts and invariants pass.
- [ ] **Step 10: Commit the catalog unit.**

  ```powershell
  git add Source/GameXXK/Public/GameXXKEquipmentCatalog.h Source/GameXXK/Private/GameXXKEquipmentCatalog.cpp Source/GameXXK/Public/GameXXKAffixCatalog.h Source/GameXXK/Private/GameXXKAffixCatalog.cpp Source/GameXXK/Public/GameXXKEquipmentSetCatalog.h Source/GameXXK/Private/GameXXKEquipmentSetCatalog.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKAffixCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentSetCatalogTest.cpp
  git diff --cached --check
  git commit -m "feat: add equipment affix and set catalogs"
  ```

Expected: all IDs, counts, slots, pools, ranges, units, and legacy lookup exclusions pass without loading visual assets.

---

## Task 3: Implement deterministic rolls, ordered warehouse ownership, and six-slot loadouts

**Files:**

- Create `GameXXKEquipmentRules.h/.cpp`
- Create `GameXXKEquipmentRulesTest.cpp`

- [ ] **Step 1: Write failing deterministic ownership tests.** Cover 10,000 fixed-seed rolls, save/reload equality, legal affix pools, 200/201 capacity, hero plus 12 companions, same-instance double ownership, swaps, full-warehouse rollback, task-NPC rejection, and redundant-owner corruption.
- [ ] **Step 2: Prove red.** Run the cold pipeline and `StartsWith:GameXXK.Equipment.Rules`. Expected: rule symbols are absent or deterministic/capacity/ownership assertions fail.
- [ ] **Step 3: Define the creation request and stable random source.** Define `FGameXXKEquipmentCreateRequest` with Set, Quality, ItemLevel, `bForceSlot`, and ForcedSlot. A package request leaves `bForceSlot=false`; tests and migration may force a slot. Generate a stream seed with `FCrc::StrCrc32` over the stable ASCII string `CollectionSeed|NextInstanceOrdinal|Set|Slot|Quality|ItemLevel`. Do not use `GetTypeHash(FName)` or process-local FName indexes. Stable IDs are `EquipmentInstance.<CollectionSeedHex>.<Ordinal>`.
- [ ] **Step 4: Implement rolled creation.** `CreateRolledInstance` chooses one of six slots uniformly when unforced, chooses legal affixes only from universal plus the selected set, uses catalog tier weights, prevents duplicate `ModifierKind`, stores every ID/tier/value/unit, inserts the instance and ordered warehouse ID, synchronizes owner redundancy, validates, then increments `NextInstanceOrdinal`. Failure advances nothing.
- [ ] **Step 5: Implement complete validation.** Prove unique IDs; valid warehouse/loadout references; no duplicate indexes; exactly one authoritative location per instance; catalog slot match; exact 1/2/3 affixes; legal tiers/values; owner agreement; enhancement 0–10; level 1–20; valid pending reforge; and only Player/current companion owners when a roster is supplied.
- [ ] **Step 6: Implement collection ownership mutations.** Equip, unequip, swap, and return-all use candidate copies. A swap returns the displaced instance at the replaced entry's stable warehouse position. Unequip/return-all fail over normal capacity. Migrated overflow is legal only with `bLegacyWarehouseOverflow=true` and legacy over-cap entries; new acquisition/unequip stay blocked until count returns to 200.
- [ ] **Step 7: Cold-build the implementation.** Run `python scripts/ue_tdd_pipeline.py`. Expected: UBT succeeds.
- [ ] **Step 8: Prove green.** Run `StartsWith:GameXXK.Equipment.Rules`. Expected: all deterministic and byte-for-byte rollback cases pass.
- [ ] **Step 9: Commit the rules unit.**

  ```powershell
  git add Source/GameXXK/Public/GameXXKEquipmentRules.h Source/GameXXK/Private/GameXXKEquipmentRules.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentRulesTest.cpp
  git diff --cached --check
  git commit -m "feat: add deterministic equipment ownership rules"
  ```

Expected: full serialized roll equality and byte-for-byte rollback snapshots pass.

---

## Task 4: Implement approved-order stat and set-effect projection

**Files:**

- Modify `GameXXKEquipmentTypes.h`
- Modify `GameXXKEquipmentRules.h/.cpp`
- Create `GameXXKEquipmentStatRulesTest.cpp`

- [ ] **Step 1: Write failing projection tests.** Use small integer fixtures to assert exact layer order, legacy scaling, six additive Attack affixes, flat-count preservation, mixed-quality set counts, combined team score, deterministic tie-break, and tooltip comparison deltas. For Tooltip cases, supply non-zero `CompareBareStats`, equip a complete current six-slot loadout, replace one slot with the candidate, and prove that percentage affixes and passive set changes produce the difference between the two complete final pre-route character attribute sets rather than an isolated item-stat difference.
- [ ] **Step 2: Prove red.** Run the cold pipeline and `StartsWith:GameXXK.Equipment.Stats`. Expected: snapshot/effect types are absent or calculation-order assertions fail.
- [ ] **Step 3: Define projection/read-model types.** Define `FGameXXKEquipmentActiveEffect` with EffectId, SourceCharacterId, Set, RequiredPieces, Scope, Hook, ModifierKind, Magnitude, Unit, and MaxTriggersPerRound. Define `FGameXXKEquipmentLoadoutSnapshot` with BareStats, EnhancedEquipmentBaseStats, AttributesBeforeRoute, aggregated universal/set modifier maps, active personal effects, candidate team effects, and `TeamEffectSourceScore`. Define the tooltip snapshot fields described below, including `CurrentCharacterStats`, `CandidateCharacterStats`, and signed `CharacterStatDeltas` as full five-stat `FGameXXKCharacterStats` values.
- [ ] **Step 4: Implement the fixed calculation order.**
   1. naked character stats;
   2. sum each resolved equipment base;
   3. for each item, multiply its base only by `(10000 + 1000 * EnhancementLevel) / 10000`, flooring each integer stat after that item's calculation;
   4. add same-kind universal basis points and apply them once to the subtotal `bare + enhanced equipment base`;
   5. aggregate set-specific affixes by modifier kind; base-stat kinds apply once and event kinds emit declarative effects;
   6. apply passive 2/4/6 descriptors and emit trigger descriptors;
   7. leave route event, relic, terrain, and battle-status modifiers to `ApplyPostEquipmentModifiers`, called explicitly after this snapshot.
- [ ] **Step 5: Implement additive modifiers and team selection.** Six Attack +10% affixes produce +60%; no equipment percentages compound. Flat-count modifiers remain integers. Mixed qualities count together. Personal 2/4 effects coexist. Resolve one same-name team six-piece source with `sum((QualityNumeric * 10) + EnhancementLevel)`; ties use stable CharacterId lexical order and different sets coexist.
- [ ] **Step 6: Implement full-loadout tooltip snapshots.** Validate `CompareBareStats`, build the requested character's current loadout snapshot, make a local collection candidate that replaces only the candidate item's matching slot using the same equip/swap legality rules, and build the candidate loadout snapshot from the same naked stats. Return identity, slot, quality, level, enhancement, item base/current stats, affixes with units, current/candidate set counts, both complete final pre-route character attribute sets, their signed five-stat delta, and the transaction error that would block equipping. If the instance is already in that character's matching slot, current and candidate attributes are identical and every delta is zero. Never mutate the supplied collection.
- [ ] **Step 7: Cold-build the implementation.** Run `python scripts/ue_tdd_pipeline.py`. Expected: UBT succeeds.
- [ ] **Step 8: Prove green.** Run `StartsWith:GameXXK.Equipment.Stats`. Expected: all order, score, and full-loadout Tooltip cases pass, including percentage-derived deltas that change when `CompareBareStats` changes.
- [ ] **Step 9: Commit the projection unit.**

  ```powershell
  git add -p -- Source/GameXXK/Public/GameXXKEquipmentTypes.h Source/GameXXK/Public/GameXXKEquipmentRules.h Source/GameXXK/Private/GameXXKEquipmentRules.cpp
  git add Source/GameXXK/Private/Tests/GameXXKEquipmentStatRulesTest.cpp
  git diff --cached --check
  git commit -m "feat: project equipment stats and set effects"
  ```

Expected: exact order, legacy scaling, additive percentages, mixed-quality counts, combined team score, deterministic tie-break, and complete current-versus-candidate final character Tooltip deltas pass.

---

## Task 5: Implement enhancement, paid reforge preview, and dismantling as runtime transactions

**Files:**

- Create `GameXXKEquipmentEconomyRules.h/.cpp`
- Create `GameXXKEquipmentEconomyRulesTest.cpp`
- Modify `GameXXKEquipmentCatalog.h/.cpp`

- [ ] **Step 1: Write failing economy transaction tests.** Cover all +0–+10 transitions, insufficient resources, modern/legacy scaling, per-instance 80% rounding, reforge begin/accept/cancel/reload, sequence advancement, batch duplicate rejection, protected confirmation, equipped dismantle, overflow reduction, hero stat synchronization, and complete rollback.
- [ ] **Step 2: Prove red.** Run the cold pipeline and `StartsWith:GameXXK.Equipment.Economy`. Expected: economy symbols are absent or stone/sand/rollback assertions fail.
- [ ] **Step 3: Implement enhancement.** Lock costs to `{1,1,1,1,1,1,1,1,1,1}`. `EnhanceInstance` copies full RuntimeState, reads authoritative `Inventory[Item.EnhancementStone]`, deducts one step, increments to +10, synchronizes `EnhancementMaterial`, recalculates hero if relevant, validates, then commits. Modern +10 doubles equipment base only; legacy weapon/armor/accessory retain +1 Attack/Defense/Speed per level.
- [ ] **Step 4: Implement deterministic paid reforge.** `BeginReforge` validates one affix and sand, rejects a second preview, derives `CollectionSeed|NextReforgeOrdinal|InstanceId|AffixIndex`, saves a complete legal candidate, deducts sand, increments the ordinal, validates, then commits. Accept verifies the original and applies the saved candidate; cancel clears without refund; failure advances nothing.
- [ ] **Step 5: Implement RuntimeState equip/unequip wrappers.** Call collection rules on a candidate, recalculate hero mirrors when needed, synchronize compatibility mirrors, validate against roster, and commit only on success.
- [ ] **Step 6: Implement protected batch dismantling.** Costs/sand are Common `10/5`, Rare `30/15`, Epic `90/45`. Aggregate `floor(each instance's spent stones * 0.8)`. Rare/Epic, enhanced, or equipped instances require `bConfirmedProtected`; false returns `ConfirmationRequired` and exact deltas without mutation. Confirmed execution clears equipped slots directly, rejects pending-reforge references, deletes all selected instances/indexes, adds resources, recalculates hero, synchronizes mirrors, validates, and commits with no gold.
- [ ] **Step 7: Cold-build the implementation.** Run `python scripts/ue_tdd_pipeline.py`. Expected: UBT succeeds.
- [ ] **Step 8: Prove green.** Run `StartsWith:GameXXK.Equipment.Economy`. Expected: all transaction and rollback cases pass.
- [ ] **Step 9: Commit the economy unit.**

  ```powershell
  git add Source/GameXXK/Public/GameXXKEquipmentEconomyRules.h Source/GameXXK/Private/GameXXKEquipmentEconomyRules.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentEconomyRulesTest.cpp
  git add -p -- Source/GameXXK/Public/GameXXKEquipmentCatalog.h Source/GameXXK/Private/GameXXKEquipmentCatalog.cpp
  git diff --cached --check
  git commit -m "feat: add equipment economy transactions"
  ```

Expected: cross-resource failure snapshots are identical and all resource deltas match the result payload.

---

## Task 6: Migrate all supported saves to version 7 and keep legacy facades coherent

**Files:**

- Create `MVP/GameXXKSaveMigration.h/.cpp`
- Modify `GameXXKMVPRules.h/.cpp`
- Modify `GameXXKMVPSubsystem.h/.cpp`
- Modify `GameXXKSaveGame.h/.cpp`
- Modify `GameXXKCompanionTypes.h`
- Create `GameXXKEquipmentSaveMigrationTest.cpp`
- Modify `GameXXKSaveGameTest.cpp`
- Modify `GameXXKInventoryEnhancementTest.cpp`
- Modify `GameXXKCompanionCodexPersistenceTest.cpp`

- [ ] **Step 1: Capture failing migration fixtures.** Add source-version fixtures for 0/2/3/4/5/6/7; hero equipment quantities and mirrors; 12 companions with old equipped IDs; active route/card/codex state; pending replacement; zero-count equipped corruption; 201 legacy warehouse copies; real-slot backup/write failures; and new-game starter compatibility.
- [ ] **Step 2: Prove red.** Run the cold pipeline and `StartsWith:GameXXK.Equipment.SaveMigration`, `StartsWith:GameXXK.MVP.SaveGame`, `StartsWith:GameXXK.MVP.Codex.SaveMigration`, and `StartsWith:GameXXK.MVP.Inventory`. Expected: version remains 6, collection migration/backups are absent, and the new assertions fail.
- [ ] **Step 3: Define the typed migration report and dispatcher.**

   ```cpp
   struct FGameXXKSaveMigrationReport
   {
       bool bSucceeded = false;
       int32 SourceVersion = 0;
       int32 TargetVersion = 7;
       bool bCreatedLegacyOverflow = false;
       TArray<FString> Warnings;
       FString Error;
   };

   static bool FGameXXKSaveMigration::MigrateToCurrent(
       const FGameXXKSaveState& Source,
       FGameXXKSaveState& OutMigrated,
       FGameXXKSaveMigrationReport& OutReport);
   ```

- [ ] **Step 4: Preserve the entire old migration chain.** Move the current version-2 through version-6 inventory/codex compatibility branches behind this dispatcher without changing results. Reject versions greater than 7. Running migration twice must produce byte-identical IDs, indexes, ordinals, resources, codex, roster, cards, and route state.
- [ ] **Step 5: Implement deterministic version 6→7 equipment conversion.**
   - Iterate old equipment Inventory keys in stable lexical order.
   - For a hero equipped mirror, assign one of that ItemId's quantity to the matching hero slot. If the saved quantity is zero but the mirror exists, synthesize exactly one legacy instance so equipped progress is not lost.
   - Convert the remaining quantity to warehouse instances in lexical key then ascending copy order.
   - Treat each companion `EquippedItemIds` entry as a separately owned old item, because the current dismissal API returns those IDs into inventory only after dismissal. Assign one instance per entry to that companion's matching slot; reject duplicate same-slot entries with a migration warning and place the later copy in warehouse.
   - Every copy inherits the old per-definition enhancement level, the queryable legacy definition, its exact base snapshot, Common quality, no random affixes, and `LegacyFlatPerEnhancement`.
   - Remove successfully converted equipment keys from legacy Inventory as authority, then rebuild those legacy equipment counts as a read-only compatibility mirror of all matching instances so the current inventory widget remains usable until plan two replaces it. Remove authoritative `ItemEnhancementLevels` entries after their derived compatibility values are rebuilt. Preserve consumables, task items, materials, gold, cards, companions, stars, active partner, quest NPC, route map, route resources, and battle state.
- [ ] **Step 6: Preserve over-cap saves without loss.** If converted warehouse count exceeds 200, migrate every item, set `bLegacyWarehouseOverflow=true`, and preserve full order. New acquisition and unequip remain blocked until equipping/dismantling reduces count to 200; migration never deletes items or makes the save unloadable because of old quantity.
- [ ] **Step 7: Implement coherent legacy mirrors and adapters.** `SynchronizeLegacyEquipmentMirrors` derives hero slots, equipped enhancement levels, and legacy BaseEquipmentId counts. Inventory equipment counts are presentation mirrors only; modern instances never enter them. `GetItemCount`, `EquipItem`, `EnhanceItem`, `DecomposeItem`, and equipment `BuyItem` delegate deterministically to collection/economy rules as frozen above.
- [ ] **Step 8: Replace player stat recalculation.** `RecalculatePlayerStatsFromEquipment` uses `GetBareHeroStats` plus `BuildLoadoutSnapshot`, preserves former missing-health/missing-mana behavior, and never rereads static equipment bonuses directly.
- [ ] **Step 9: Upgrade current version and new-game initialization.** Set `CurrentSaveVersion=7`. New games initialize schema 1, a non-zero collection seed, ten authoritative stones, and wooden sword/cloth armor/cloth talisman as unequipped legacy warehouse instances in current order, with derived legacy Inventory counts.
- [ ] **Step 10: Implement failure-safe disk migration.** `UGameXXKMVPSubsystem::LoadGameFromSlot` follows this exact sequence:
   1. Load the original object without changing RuntimeState.
   2. Save that unmodified object to `<SlotName>.PreV7Backup` with the same user index; do not overwrite an existing successful pre-v7 backup.
   3. Migrate an in-memory copy and validate the complete runtime state.
   4. Save a new `UGameXXKSaveGame` containing the migrated state to the main slot.
   5. Assign live RuntimeState only after the upgraded write succeeds.
   6. If backup, migration, validation, or upgraded write fails, keep live state unchanged, retain the original main file, restore it from the backup if the main write was attempted, and surface `存档迁移失败，已保留原存档。`.
- [ ] **Step 11: Update all version-dependent regressions.** Update `GameXXKInventoryEnhancementTest.cpp` and `GameXXKCompanionCodexPersistenceTest.cpp`; keep existing version-2/4/5 expectations. Real-slot tests delete temporary main/backup slots before and after each case.
- [ ] **Step 12: Cold-build the implementation.** Run `python scripts/ue_tdd_pipeline.py`. Expected: UBT succeeds.
- [ ] **Step 13: Prove green.** Run all four Task 6 MCP prefixes. Expected: all migrations, backups, current inventory compatibility, and version assertions pass.
- [ ] **Step 14: Commit the complete migration unit.**

  ```powershell
  git add Source/GameXXK/Public/MVP/GameXXKSaveMigration.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp
  git add -p -- Source/GameXXK/Public/GameXXKMVPRules.h Source/GameXXK/Private/GameXXKMVPRules.cpp Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp Source/GameXXK/Public/MVP/GameXXKSaveGame.h Source/GameXXK/Private/MVP/GameXXKSaveGame.cpp Source/GameXXK/Public/GameXXKCompanionTypes.h Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp Source/GameXXK/Private/Tests/GameXXKInventoryEnhancementTest.cpp Source/GameXXK/Private/Tests/GameXXKCompanionCodexPersistenceTest.cpp
  git diff --cached --check
  git commit -m "feat: migrate equipment saves to version seven"
  ```

Expected: no progress loss, exact legacy effective stats, exact copy counts, safe over-cap migration, idempotency, backup recovery, and current-version assertions all pass.

---

## Task 7: Make companion replacement atomic with central equipment

**Files:**

- Modify `GameXXKCompanionRules.h/.cpp`
- Modify `GameXXKCompanionTypes.h`
- Modify `GameXXKMVPSubsystem.h/.cpp`
- Create `GameXXKEquipmentCompanionReplacementTest.cpp`

Freeze this subsystem overload before plan two begins:

```cpp
// Authoritative C++ transaction used by the new UI.
bool ResolvePendingPermanentCompanionReplacement(
    FName DismissedInstanceId,
    FName ActivePermanentCompanionInstanceIdAfterReplacement,
    FGameXXKEquipmentTransactionResult& OutResult);

// Existing Blueprint-compatible wrapper; retained only as a compatibility delegate.
bool ResolvePendingPermanentCompanionReplacement(
    FName DismissedInstanceId,
    FName ActivePermanentCompanionInstanceIdAfterReplacement = NAME_None);
```

The three-argument overload is the only new-UI mutation path. It initializes `OutResult` on every call and returns the same success boolean stored in `OutResult.bSucceeded`. `WarehouseFull`, `InvalidOwner`, and `RouteLocked` are returned unchanged through `OutResult.Error` and the central `FText` message mapper. Every failure preserves the pending candidate and leaves the complete RuntimeState byte-identical. The existing two-argument bool function constructs a local result, delegates once to the three-argument overload, discards only the result payload, and returns its boolean; it contains no replacement logic of its own. Because Unreal reflection does not expose overloaded `UFUNCTION` names safely, retain the existing two-argument function as the Blueprint compatibility surface and keep the authoritative three-argument overload as the C++ function called by the plan-two UMG code.

- [ ] **Step 1: Write the failing companion-replacement transaction tests.** Register `GameXXK.Equipment.CompanionReplacement`. Compile against both overloads. Cover zero to six equipped items, warehouse counts 194–200, active and inactive companion replacement, candidate discard, route lock, stale dismissed/active companion IDs, and byte-identical failure snapshots. Assert exact `WarehouseFull`, `InvalidOwner`, and `RouteLocked` result codes/messages, `bSucceeded=false`, and an unchanged pending candidate plus complete RuntimeState on every failure. At warehouse 200 with one equipped item, replacement must fail without changing roster or equipment; with sufficient space, all six IDs must return in deterministic Weapon/Head/Armor/Belt/Shoes/Accessory order. Assert the old two-argument wrapper delegates to and returns the same boolean outcome as the new transaction overload.
- [ ] **Step 2: Prove red.** Run `python scripts/ue_tdd_pipeline.py`, then the shared MCP command with `StartsWith:GameXXK.Equipment.CompanionReplacement`. Expected: equipped instances remain attached to the dismissed companion, the central capacity check is absent, or rollback/order assertions fail. Record at least one behavior assertion failure before implementation.
- [ ] **Step 3: Retire the old companion equipment write path.** Keep `FGameXXKPermanentCompanion::EquippedItemIds` readable and deprecated only as a pre-v7 migration source. New-game, recruitment, dismissal, replacement, and equipment gameplay never write it.
- [ ] **Step 4: Implement one full-state replacement transaction behind the new overload.** Keep roster-only `FGameXXKCompanionRules::ResolvePendingRecruitment` free of equipment mutation. In the three-argument `UGameXXKMVPSubsystem::ResolvePendingPermanentCompanionReplacement`, initialize `OutResult`, reject route/battle configuration with `RouteLocked`, copy the complete RuntimeState, validate the selected dismissed companion and, when non-`None`, the requested post-replacement active companion with `InvalidOwner`, call `ReturnAllEquipmentToWarehouse` and forward `WarehouseFull` without translation, call the roster rule, validate the collection against the resulting roster, validate card-run consistency, and commit the complete candidate only after every check succeeds. `NAME_None` retains the roster rule's existing behavior: preserve the surviving active companion when replacing an inactive companion, otherwise permit no active companion. On all failures, assign only the typed result and leave the saved pending candidate, roster, equipment, resources, and live RuntimeState unchanged. Replace the old two-argument implementation body with the one-call compatibility delegation described above.
- [ ] **Step 5: Make dismissal-refund detection schema-aware.** Replace the current `HasUnclaimedDismissalRefund` equipment check with a central collection lookup while preserving experience-material and active-companion behavior.
- [ ] **Step 6: Cold-build the implementation.** Run `python scripts/ue_tdd_pipeline.py`. Expected: UBT succeeds and the editor relaunches with the new transaction code.
- [ ] **Step 7: Prove green and preserve adjacent companion behavior.** Run the shared MCP command with `StartsWith:GameXXK.Equipment.CompanionReplacement` and `StartsWith:GameXXK.Data.Companion`. Expected: both prefixes discover non-zero tests and report zero failures; the new UI overload returns typed success/failure, the legacy bool wrapper remains behavior-compatible, and every typed failure preserves the pending candidate and complete RuntimeState.
- [ ] **Step 8: Commit the complete companion-replacement unit.**

  ```powershell
  git add Source/GameXXK/Private/Tests/GameXXKEquipmentCompanionReplacementTest.cpp
  git add -p -- Source/GameXXK/Public/GameXXKCompanionRules.h Source/GameXXK/Private/GameXXKCompanionRules.cpp Source/GameXXK/Public/GameXXKCompanionTypes.h Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp
  git diff --cached -- Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentCompanionReplacementTest.cpp
  git diff --cached --check
  git commit -m "feat: make companion replacement equipment-safe"
  ```

Expected: no equipped instance is orphaned, copied, or lost during full-roster replacement; the plan-two UI has one typed transaction gate, and all capacity/owner/route failures preserve the pending candidate and complete state.

---

## Task 8: Feed hero/partner equipment and set descriptors into the actual card battle

**Files:**

- Modify `GameXXKEquipmentTypes.h`
- Modify `GameXXKCardTypes.h`
- Modify `GameXXKCardRules.h/.cpp`
- Modify `GameXXKCardBattleAdapter.h/.cpp`
- Modify `GameXXKMVPRules.h/.cpp`
- Create `GameXXKEquipmentBattleIntegrationTest.cpp`

- [ ] **Step 1: Write the failing battle-projection tests.** Register `GameXXK.Equipment.BattleIntegration`. Build hero + permanent companion + task NPC fixtures; give the two permanent characters six pieces each, include mixed same-set team effects, and retain no-equipment controls. Assert HP, MP, Attack, Defense, Speed, descriptor count, source score, task-NPC exclusion, save/reload, and no double application.
- [ ] **Step 2: Prove red.** Run `python scripts/ue_tdd_pipeline.py`, then `StartsWith:GameXXK.Equipment.BattleIntegration`. Expected: Speed is absent from the card unit, equipment descriptors are absent/duplicated, or projected permanent-character stats disagree. Record the focused behavior failure before implementation.
- [ ] **Step 3: Carry authoritative Speed into card combat.** Add Speed to `FGameXXKCardCombatUnit` and map it from `FGameXXKBattleRuntimeUnit`; do not retain fixed companion or task-NPC speed in the adapter.
- [ ] **Step 4: Add saved battle-effect runtime state and validation.** Define `FGameXXKEquipmentBattleEffectRuntime` with active descriptor, source character, current-round trigger count, and last-trigger round. Add `TArray<FGameXXKEquipmentBattleEffectRuntime> EquipmentEffects` to `FGameXXKCardBattleRuntime`. Extend `FGameXXKCardRules::ValidateCardBattleRuntime` to reject unknown effect IDs, stale source units, negative counters, duplicate `(EffectId, SourceCharacterId)`, and last-trigger rounds beyond the current round. This is the one persistent location for later card/damage/heal/status hooks.
- [ ] **Step 5: Project each party member through exactly one stat path.** In `BuildRoutePartyProjection`, calculate hero as bare hero stats → hero loadout snapshot → existing hero route-event bonuses; calculate the active permanent companion as bare role/level/star stats → that companion's loadout snapshot; calculate the temporary task NPC from the existing quest-NPC level snapshot only and never query it as an equipment owner.
- [ ] **Step 6: Materialize set descriptors once.** Resolve personal and unique team effects across hero and active permanent companion, add each descriptor exactly once to card battle runtime with a stable source ID, and do not apply equipment again in `FGameXXKCardRules` or the legacy scene projection. Preserve current HP/MP by missing amount when maxima change. No-equipment fixtures remain numerically identical except for explicit Speed.
- [ ] **Step 7: Cold-build the implementation.** Run `python scripts/ue_tdd_pipeline.py`. Expected: UBT succeeds and the editor loads the updated card runtime.
- [ ] **Step 8: Prove green with adjacent real-rule tests.** Run the shared MCP command with `StartsWith:GameXXK.Equipment.BattleIntegration`, `StartsWith:GameXXK.Integration.CardBattleAdapter`, `StartsWith:GameXXK.Integration.CardBattle.Adapter`, `StartsWith:GameXXK.Data.CardRules`, and `StartsWith:GameXXK.Data.CardCombatRules`. Expected: every discovered test passes and equipment projections remain single-applied.
- [ ] **Step 9: Commit the complete battle-integration unit.**

  ```powershell
  git add Source/GameXXK/Private/Tests/GameXXKEquipmentBattleIntegrationTest.cpp
  git add -p -- Source/GameXXK/Public/GameXXKEquipmentTypes.h Source/GameXXK/Public/GameXXKCardTypes.h Source/GameXXK/Public/GameXXKCardRules.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Public/GameXXKCardBattleAdapter.h Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Public/GameXXKMVPRules.h Source/GameXXK/Private/GameXXKMVPRules.cpp
  git diff --cached --check
  git commit -m "feat: project equipment into card battles"
  ```

Expected: battle authority, legacy HUD projection, and equipment snapshots agree exactly.

---

## Task 9: Expose exact read models and transaction facades for plan two

**Files:**

- Modify `GameXXKMVPSubsystem.h/.cpp`
- Create `GameXXKEquipmentFacadeTest.cpp`

Add these thin methods; Blueprint-facing variants may use output references but retain the same names and semantics:

```cpp
bool GetEquipmentWarehouseSnapshot(TArray<FName>& OutOrderedInstanceIds) const;
bool GetEquipmentLoadoutSnapshot(FName CharacterId, FGameXXKEquipmentLoadoutSnapshot& OutSnapshot) const;
bool GetEquipmentTooltipSnapshot(FName InstanceId, FName CompareCharacterId, FGameXXKEquipmentTooltipSnapshot& OutSnapshot) const;
bool EquipEquipmentInstance(FName CharacterId, EGameXXKEquipmentSlot Slot, FName InstanceId, FGameXXKEquipmentTransactionResult& OutResult);
bool UnequipEquipmentSlot(FName CharacterId, EGameXXKEquipmentSlot Slot, FGameXXKEquipmentTransactionResult& OutResult);
bool EnhanceEquipmentInstance(FName InstanceId, FGameXXKEquipmentTransactionResult& OutResult);
bool BeginEquipmentReforge(FName InstanceId, int32 AffixIndex, FGameXXKEquipmentTransactionResult& OutResult);
bool ResolveEquipmentReforge(bool bAccept, FGameXXKEquipmentTransactionResult& OutResult);
bool DismantleEquipmentInstances(const TArray<FName>& InstanceIds, bool bConfirmedProtected, FGameXXKEquipmentTransactionResult& OutResult);
```

- [ ] **Step 1: Write failing facade contract tests.** Register `GameXXK.Equipment.Facade`. Compile against every frozen method above and assert success/error codes, approved Chinese messages, exact rollback, 200-capacity behavior, task-NPC rejection, route lock, pending-reforge visibility, ordered warehouse IDs, and full-loadout Tooltip comparison values. Use hero and companion fixtures with different naked stats so the test fails if the facade omits or fabricates `CompareBareStats`.
- [ ] **Step 2: Prove red.** Add only minimum declarations if needed, run `python scripts/ue_tdd_pipeline.py`, then `StartsWith:GameXXK.Equipment.Facade`. Expected: facade methods are absent or their read/transaction semantics fail. If a missing declaration first causes UBT red, record it, add compile-only declarations, rerun the cold pipeline, and obtain at least one MCP behavior assertion failure before implementation.
- [ ] **Step 3: Implement pure read facades.** Queries expose ordered saved IDs and snapshots built by `FGameXXKEquipmentRules`; they never calculate UI prices, filter enums, or mutate RuntimeState. `GetEquipmentTooltipSnapshot` resolves `CompareCharacterId` to the hero or a current permanent companion, obtains authoritative naked stats from `FGameXXKCharacterStatRules`, and calls `BuildTooltipSnapshot(Collection, InstanceId, CompareCharacterId, CompareBareStats, OutSnapshot, OutError)`. Task NPCs and stale companion IDs fail without producing a comparison.
- [ ] **Step 4: Implement full-state mutation facades.** Equip, unequip, enhance, reforge, and dismantle call the complete-RuntimeState `FGameXXKEquipmentEconomyRules` wrappers so hero stat mirrors, route locks, roster validation, resources, and compatibility mirrors commit atomically. Collection-only rules remain lower-level building blocks for economy rules and pure tests.
- [ ] **Step 5: Enforce configuration-state boundaries.** Mutations are permitted only in the existing town configuration state. Route/battle lock returns `RouteLocked` with no state change. Read-only tooltip and warehouse queries remain available while locked.
- [ ] **Step 6: Cold-build the implementation.** Run `python scripts/ue_tdd_pipeline.py`. Expected: UBT succeeds.
- [ ] **Step 7: Prove green.** Run `StartsWith:GameXXK.Equipment.Facade`. Expected: a non-zero test count and zero failures.
- [ ] **Step 8: Commit the complete subsystem facade unit.**

  ```powershell
  git add Source/GameXXK/Private/Tests/GameXXKEquipmentFacadeTest.cpp
  git add -p -- Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp
  git diff --cached --check
  git commit -m "feat: expose equipment progression facade"
  ```

Expected: plan two can build all widgets without scanning structs or duplicating gameplay calculations.

---

## Task 10: Establish a deterministic simulation runner over the real battle rules

**Files:**

- Create `GameXXKCombatSimulationTypes.h`
- Create `GameXXKCombatSimulationRules.h/.cpp`
- Create `GameXXKCombatSimulationFoundationTest.cpp`

Define these contracts:

```cpp
UENUM()
enum class EGameXXKSimulationPolicy : uint8
{
    Invalid = 0,
    Skilled = 1
};

struct FGameXXKSimulationScenario
{
    int32 Seed = 0;
    FGameXXKRuntimeState InitialRuntimeState;
    EGameXXKNodeKind NodeKind = EGameXXKNodeKind::Battle;
    EGameXXKCardTerrain Terrain = EGameXXKCardTerrain::Plain;
    EGameXXKSimulationPolicy Policy = EGameXXKSimulationPolicy::Skilled;
    int32 MaxRounds = 100;
    int32 MaxDecisions = 2000;
};

struct FGameXXKSimulationDecision
{
    FName CardInstanceId = NAME_None;
    FName TargetUnitId = NAME_None;
    bool bEndPlayerPhase = false;
};

struct FGameXXKSimulationTraceEntry
{
    int32 Round = 0;
    FName Action = NAME_None;
    FName SourceUnitId = NAME_None;
    FName CardOrIntentId = NAME_None;
    FName TargetUnitId = NAME_None;
    int32 HealthDelta = 0;
    int32 ManaDelta = 0;
    int32 ArmorDelta = 0;
};

struct FGameXXKSimulationMetrics
{
    bool bVictory = false;
    int32 Rounds = 0;
    int32 RemainingPartyHealth = 0;
    int32 FirstRoundDeaths = 0;
    TMap<FName, int64> DamageBySource;
    TMap<FName, int64> HealingBySource;
    TMap<FName, int64> ArmorBySource;
    TMap<FName, int64> StatusProduced;
    TMap<FName, int64> StatusConsumed;
    FName FailureReason = NAME_None;
};

static bool FGameXXKCombatSimulationRules::RunScenario(
    const FGameXXKSimulationScenario& Scenario,
    FGameXXKSimulationMetrics& OutMetrics,
    TArray<FGameXXKSimulationTraceEntry>& OutTrace,
    FString* OutError = nullptr);
```

- [ ] **Step 1: Write the failing deterministic simulation tests.** Register `GameXXK.Simulation.Foundation`. Use naked hero and all six full sets at Common/Rare/Epic and +10 at levels 1, 5, 10, 15, and 20. Require the same serialized scenario and seed to emit byte-identical metrics and trace; assert finite termination, no negative resources, valid owners, and no duplicate card instances.
- [ ] **Step 2: Prove red.** Add the compile-only contracts above, run `python scripts/ue_tdd_pipeline.py`, then `StartsWith:GameXXK.Simulation.Foundation`. Expected: runner output is unimplemented or determinism/real-rule execution assertions fail. Record at least one MCP behavior failure.
- [ ] **Step 3: Implement the real-rule scenario loop.** `RunScenario` copies InitialRuntimeState, calls `FGameXXKCardBattleAdapter::BeginCardBattle`, then uses only `BuildCardPlayPreview`, `ResolveCardPlay`, pending-choice APIs, `EndPlayerCardPhase`, `ResolveNextEnemyIntent`/`ResolveEnemyPhase`, and `CompleteEnemyCardPhase`. Do not implement a second damage, defense, status, draw, or enemy formula.
- [ ] **Step 4: Implement the skilled deterministic policy.** Enumerate legal hand cards and stable legal targets on candidate copies; resolve candidates through the real adapter; score immediate enemy health damage, ally healing/armor, status production/consumption, resource cost, lethal prevention, and draw. Tie-break by card acquisition ordinal then stable target ID. Score forced discard and insight choices deterministically; end the phase when no positive legal play remains.
- [ ] **Step 5: Derive trace and metrics from authoritative deltas.** Compare before/after state snapshots plus returned damage results. Enforce `MaxRounds` and `MaxDecisions` with explicit failure reasons. Record raw damage, survival, resource, and status components only; plan three owns the approved 100-seed enemy matrix and tuning against +60/+150/+300% targets.
- [ ] **Step 6: Cold-build the implementation.** Run `python scripts/ue_tdd_pipeline.py`. Expected: UBT succeeds.
- [ ] **Step 7: Prove green.** Run `StartsWith:GameXXK.Simulation.Foundation`. Expected: every fixture terminates and repeated serialized inputs produce byte-identical output.
- [ ] **Step 8: Commit the standalone simulation foundation.**

  ```powershell
  git add Source/GameXXK/Public/GameXXKCombatSimulationTypes.h Source/GameXXK/Public/GameXXKCombatSimulationRules.h Source/GameXXK/Private/GameXXKCombatSimulationRules.cpp Source/GameXXK/Private/Tests/GameXXKCombatSimulationFoundationTest.cpp
  git diff --cached --check
  git commit -m "test: add deterministic combat simulation foundation"
  ```

Expected: finite, repeatable real-rule battles with no negative resources, invalid owners, duplicate cards, or UI/UObject dependency.

---

## Task 11: Full cold-build, migration, and adjacent regression gate

**Files:** verification only.

- [ ] **Step 1: Inspect the exact working set.**

   ```powershell
   git status --short
   git diff --check
   git diff --stat -- Source/GameXXK/Public/GameXXKEquipmentTypes.h Source/GameXXK/Public/GameXXKEquipmentRules.h Source/GameXXK/Public/GameXXKEquipmentEconomyRules.h Source/GameXXK/Public/MVP/GameXXKSaveMigration.h
   ```

- [ ] **Step 2: Run the final cold cycle.**

   ```powershell
   python scripts/ue_tdd_pipeline.py --pie-duration 8 --filter "[TDD]"
   ```

   Expected: UE MCP saves dirty packages, editor closes, UBT succeeds, the project opens through `GameXXK.uproject`, PIE starts and stops, and no crash/assert appears.

- [ ] **Step 3: Rediscover and run all required prefixes, recording non-zero test counts and zero failures.**

   - `StartsWith:GameXXK.Equipment`
   - `StartsWith:GameXXK.Simulation.Foundation`
   - `StartsWith:GameXXK.Data.Companion`
   - `StartsWith:GameXXK.MVP.Save`
   - `StartsWith:GameXXK.MVP.Codex.SaveMigration`
   - `StartsWith:GameXXK.MVP.Inventory`
   - `StartsWith:GameXXK.Integration.CardBattleAdapter`
   - `StartsWith:GameXXK.Integration.CardBattle.Adapter`
   - `StartsWith:GameXXK.Data.CardRules`
   - `StartsWith:GameXXK.Data.CardCombatRules`

- [ ] **Step 4: Execute the real-slot round trip.** Run the `GameXXK.Equipment.SaveMigration` real-slot case with hero and companion six-piece loadouts, 200 warehouse items, a paid pending reforge, legacy instances, route state, and codex state. Verify stable IDs/order, exact resources, no duplicate owner, exact partner cards/names, exact battle descriptors, and clean deletion of temporary main/backup slots.
- [ ] **Step 5: Audit the ten task commits and the scoped working tree.**

  ```powershell
  git log --oneline -10
  git status --short -- Source/GameXXK
  git diff --check
  git diff --cached --check
  ```

  Expected: Tasks 1–10 each have their named commit, no required equipment dependency remains unstaged, and no unrelated user hunk was staged. Task 11 is verification-only and intentionally creates no empty commit. If a legitimate integration hunk remains, return to the task that owns it, stage it with that task's exact `git add`/`git add -p` command, rerun its cold/MCP green gate, and amend that task commit only with explicit repository-owner approval.

---

## Completion gate

- `EquipmentCollection` is the sole equipment authority and validates its instance array, ordered warehouse IDs, six-slot loadouts, and redundant owner data.
- All 36 modern definitions, 35 affix families, eighteen set descriptors, and seven queryable legacy definitions validate in three separate catalogs.
- Every obtained item is one stable serialized instance; fixed seeds reproduce complete rolls and reforge previews.
- Normal warehouse capacity is 200; migration overflow preserves old items while blocking new acquisitions until resolved.
- Hero and up to 12 permanent companions save six independent slots; task NPCs cannot own permanent equipment.
- Hero and companion naked stats, including Speed, are calculated once; player and permanent companion levels cannot exceed 20.
- Enhancement reaches +10, doubles modern base stats only, preserves legacy flat behavior, and dismantle returns exactly 80% of spent stones rounded down.
- Reforge consumes sand when the preview is created, advances a saved ordinal, survives reload, and cannot be rerolled by cancellation.
- Stat calculation follows the approved order, percentages add by kind, and team six-piece source selection uses the locked combined score.
- Versions 0/2/3/4/5/6 migrate through one dispatcher to version 7 without lost equipment, companions, cards, codex, route state, or resources; disk backup and rollback are verified.
- Current static-item facade calls remain coherent until plan two replaces their presentation; no second equipment authority remains active.
- Actual card battle state carries Speed and saved equipment effect descriptors exactly once.
- The deterministic simulation runner uses the real adapter/rules and emits repeatable trace and raw balance metrics.
- Cold UBT, focused automation, migration round trip, adjacent regressions, editor launch, and PIE all pass without Live Coding, Hot Reload, UnrealBridge, or asset modification.
