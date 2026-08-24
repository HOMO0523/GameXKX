# Equipment Quality, Gems, and Tools Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend equipment and gems to ten append-only qualities and make Dismantle, Combine, Enhance, Reforge, and Socket complete atomic Tool transactions with level/experience progression.

**Architecture:** Keep equipment-instance validation in Equipment Rules, permanent-resource changes in Equipment Tool/Economy Rules, and Workbench reservations/presentation in UI. Introduce typed gem and tool-progression data rather than encoding socket state in text or widget arrays. Every batch is one candidate-copy rule call.

**Tech Stack:** Unreal Engine 5.8 C++, USTRUCT SaveGame state, deterministic `FRandomStream`, UMG, UE Automation Tests.

---

## Source specification

`docs/superpowers/specs/2026-08-24-shared-inventory-equipment-tools-chests-design.md`

Prerequisite: `docs/superpowers/plans/2026-08-24-inventory-character-carry-locks.md` is complete and save version 25 exists.

### Task 1: RED — ten equipment and affix qualities

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentCatalogTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentEconomyRulesTest.cpp`

- [ ] **Step 1: assert serialized ordinals and names**

Add exact assertions for ranks 1–10 without changing Common/Rare/Epic values:

```cpp
TestEqual(TEXT("Legendary appends at four"),
	static_cast<uint8>(EGameXXKEquipmentQuality::Legendary), uint8(4));
TestEqual(TEXT("Cosmic appends at ten"),
	static_cast<uint8>(EGameXXKEquipmentQuality::Cosmic), uint8(10));
```

Repeat for Affix Tier and Gem Quality.

- [ ] **Step 2: add table-driven affix RED**

Assert affix counts `{1,2,3,4,5,5,5,5,5,5}` for ranks 1–10. Create and validate one instance at every quality.

- [ ] **Step 3: add exact affix magnitude RED**

For tier rank `r`, assert:

```cpp
const int32 BasisMin = 100 * (r + 1) * (r + 2) / 2;
const int32 BasisMax = BasisMin + 100 * (r + 1);
const int32 FlatMin = (r + 1) / 2;
const int32 FlatMax = r;
```

Quality rank 1/2/3 must remain the existing `300–500`, `600–900`, `1000–1400` basis-point ranges.

- [ ] **Step 4: run RED**

Run Equipment Catalog/Rules/Economy. Expected failures: missing enum values, quality validation, affix counts, weights, and magnitude ranges.

### Task 2: append qualities and deterministic affix rules

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKEquipmentTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKAffixCatalog.h`
- Modify: `Source/GameXXK/Private/GameXXKAffixCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKEquipmentRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKEquipmentCatalog.cpp`
- Modify: related Equipment tests.

- [ ] **Step 1: append enums**

Append to both Equipment Quality and Affix Tier:

```cpp
Legendary = 4,
Immortal = 5,
Treasure = 6,
Transcendent = 7,
Celestial = 8,
Ascendant = 9,
Cosmic = 10
```

Add one shared rank/display/next-quality helper; do not scatter range comparisons through UI.

- [ ] **Step 2: extend affix weights without breaking old fields**

Append named weight fields to `FGameXXKAffixTierWeights` and add:

```cpp
int32 GetWeight(EGameXXKAffixTier Tier) const;
void SetWeight(EGameXXKAffixTier Tier, int32 Weight);
```

Weights are:

```text
quality 1: tier 1 = 100
quality 2: tier 1 = 70, tier 2 = 30
quality r>=3: tier r-2 = 50, tier r-1 = 35, tier r = 15
```

- [ ] **Step 3: cap affix count at five and extend magnitude formula**

Replace numeric-quality-as-count with:

```cpp
return FMath::Min(QualityRank(Quality), 5);
```

Implement the confirmed basis/flat formulas from Task 1 with checked 64-bit intermediates.

- [ ] **Step 4: run GREEN and commit**

Run Equipment Catalog, Affix Catalog, Equipment Rules, Card Quality compatibility, Save Migration.

Commit:

```powershell
git commit -m "feat: extend equipment to ten qualities"
```

### Task 3: add gem catalog and sockets

Prerequisite: complete `docs/superpowers/plans/2026-08-24-gem-icon-progression.md` so all thirty declared texture paths exist before runtime item mapping is accepted.

**Files:**
- Create: `Source/GameXXK/Public/GameXXKGemRules.h`
- Create: `Source/GameXXK/Private/GameXXKGemRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKEquipmentTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKEquipmentRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKEquipmentEconomyRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Private/Tests/GameXXKGemRulesTest.cpp`
- Modify: Save migration/validation tests.

- [ ] **Step 1: Write and run socket/gem projection RED**

Assert socket capacities `{1,1,1,1,1,2,3,4,5,6}` and exact save round-trip for every equipment quality. Wish for Attack/Defense base `1 << (rank-1)` and Max Health `10 << (rank-1)`; equip gems in all valid sockets, build a loadout snapshot, and assert exact flat stat totals and battle projection. Invalid type/quality/over-capacity rejects.

- [ ] **Step 2: add gem types**

```cpp
UENUM(BlueprintType)
enum class EGameXXKGemType : uint8
{
	Invalid = 0 UMETA(Hidden), Attack = 1, Defense = 2, MaxHealth = 3
};

UENUM(BlueprintType)
enum class EGameXXKGemQuality : uint8
{
	Invalid = 0 UMETA(Hidden), Common = 1, Rare = 2, Epic = 3,
	Legendary = 4, Immortal = 5, Treasure = 6, Transcendent = 7,
	Celestial = 8, Ascendant = 9, Cosmic = 10
};

USTRUCT(BlueprintType)
struct FGameXXKSocketedGem
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKGemType Type = EGameXXKGemType::Invalid;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKGemQuality Quality = EGameXXKGemQuality::Invalid;
	bool IsEmpty() const { return Type == EGameXXKGemType::Invalid; }
};
```

- [ ] **Step 3: append sockets to equipment instance**

Append `TArray<FGameXXKSocketedGem> SocketedGems`. `GetSocketCapacity(Quality)` returns `1 + Max(0, rank-5)`. Normalize resizes only by appending empty entries; validation requires exact capacity.

- [ ] **Step 4: add stable gem item IDs**

`FGameXXKGemRules` maps `(Type,Quality)` to `Item.Gem.<Type>.<Quality>`, parses IDs, returns Chinese display, stat bonus, next quality, exact soft icon texture path, and validates every rank. Register all 30 IDs in `GetKnownItemIds`/`GetItemDef` as Material items. The icon path must be `/Game/GameXXK/UI/Items/Gems/T_Item_Gem_<Type>_<Quality>.T_Item_Gem_<Type>_<Quality>` and must come from the shared Gem Rules mapping rather than UI-side string construction. Automation loads every declared asset and rejects missing or cross-type mappings.

- [ ] **Step 5: project socket bonuses**

During `BuildLoadoutSnapshot`, sum gem flat stats with checked addition after enhanced equipment bases and before route modifiers. Include gem lines in equipment tooltip detail.

- [ ] **Step 6: migrate v24/v25 instances**

For equipment lacking socket data, create empty sockets from quality. Do not increment save version again. Add current-v25 absent-property fixture and exact round-trip.

- [ ] **Step 7: run GREEN and commit**

Run Gem Rules, Equipment Rules, Battle Integration, SaveGame.

Commit:

```powershell
git commit -m "feat: add quality-scaled equipment sockets and gems"
```

### Task 4: persist Tool progression and quality XP

**Files:**
- Create: `Source/GameXXK/Public/GameXXKEquipmentToolRules.h`
- Create: `Source/GameXXK/Private/GameXXKEquipmentToolRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKEquipmentToolRulesTest.cpp`
- Modify: Save migration/round-trip tests.

- [ ] **Step 1: Write and run Tool progression RED**

Assert rank multipliers through Cosmic, exact threshold boundaries, residual XP, cap, selected crafting-level clamping, confirmed overlapping item-level ranges, and save round-trip. Capture causal compile/assertion failure before production edits.

- [ ] **Step 2: add save state**

```cpp
USTRUCT(BlueprintType)
struct FGameXXKToolProgress
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 Level = 1;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 Experience = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 SelectedCraftingLevel = 1;
};
```

Append it to Runtime State. Migration defaults to `{1,0,1}`.

- [ ] **Step 3: implement exact tables**

```cpp
static int64 GetQualityExperienceMultiplier(int32 Rank); // checked power of 9
static int64 GetExperienceForNextLevel(int32 CurrentLevel); // CurrentLevel*100
static bool AddExperience(FGameXXKToolProgress& Progress, int64 BaseAward, int32 QualityRank);
static FInt32Interval GetCraftedItemLevelRange(int32 SelectedCraftingLevel);
```

Ranges are `[1,10]`, then `[(level-1)*10, level*10]`. AddExperience repeatedly advances while thresholds are met and caps at Level 10.

- [ ] **Step 4: complete quality award tests**

Assert rank multipliers through Cosmic, Dismantle sums, Combine `9*multiplier`, Enhance/Reforge/Socket single multiplier, exact threshold boundaries, residual XP, cap, selected-level clamping, and save round-trip.

- [ ] **Step 5: run GREEN and commit**

Commit:

```powershell
git commit -m "feat: persist ten-level tool progression"
```

### Task 5: RED/GREEN — atomic Dismantle and Combine

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKEquipmentToolRules.h`
- Modify: `Source/GameXXK/Private/GameXXKEquipmentToolRules.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentToolRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentFacadeTest.cpp`

- [ ] **Step 1: Write and run Dismantle/Combine RED**

Cover locked/stale inputs, exact 1–9 Dismantle reward/XP, Equipment 9-to-1 across every next-quality boundary, Cosmic cap, Gem 9-to-1, crafting-level endpoints, output placement, Auto Fill ordering/Include Warehouse, overflow, and byte-identical rollback. Capture failure before production edits.

- [ ] **Step 2: add typed input references**

```cpp
struct GAMEXXK_API FGameXXKToolInputRef
{
	EGameXXKDesktopItemContainer Container = EGameXXKDesktopItemContainer::Backpack;
	int32 SlotIndex = INDEX_NONE;
	FGameXXKDesktopInventoryEntryKey ExpectedEntry;
};
```

Every transaction re-resolves and compares all expected entries before mutation.

- [ ] **Step 3: make Dismantle include lock and XP**

Add a facade taking 1–9 input refs and explicit confirmation. Reject item inputs, equipped/stale/locked IDs, overflow, or route lock. Preview/commit uses one existing `DismantleBatch`, adds `10 gold + 1 stone + 1 sand` per instance, removes physical cells, adds summed quality XP, normalizes, validates, commits.

- [ ] **Step 4: implement equipment Combine**

Require nine equipment refs, same non-Cosmic quality, unlocked and unequipped. Seed output from collection seed, nine stable IDs, selected crafting level, and next instance ordinal. Pick one of six modern non-Starter sets, one of six slots, next quality, and an item level in the selected interval. Consume nine, create one, place it in the first freed Backpack cell else first freed Warehouse cell, add XP, validate, commit.

- [ ] **Step 5: implement gem Combine**

Require exactly nine units from one parsed gem stack type/quality. Because item stacks are whole-stack physical entries, the transaction decrements nine and keeps/rebuilds the stack cell; it creates one next-quality gem in the freed/first valid Backpack cell else Warehouse cell. Reject Cosmic, locked item ID, insufficient quantity, and capacity failure.

- [ ] **Step 6: add deterministic Auto Fill query**

```cpp
static bool BuildCombineAutoFill(
	const FGameXXKRuntimeState& State,
	EGameXXKToolCombineKind Kind,
	bool bIncludeWarehouse,
	const TOptional<FGameXXKToolCombineConstraint>& ExistingConstraint,
	TArray<FGameXXKToolInputRef>& OutInputs,
	FString* OutError = nullptr);
```

Use the confirmed eligibility and ordering. Query is pure and returns exactly nine or none.

- [ ] **Step 7: run GREEN and commit**

Commit:

```powershell
git commit -m "feat: implement locked-safe nine-to-one tool combine"
```

### Task 6: RED/GREEN — Enhance, Reforge preview, and Socket

**Files:**
- Modify: Tool Rules/Subsystem files.
- Modify: `Source/GameXXK/Public/GameXXKEquipmentTypes.h`
- Modify: Equipment Economy Rules.
- Modify: Tool/Equipment tests.

- [ ] **Step 1: Write and run Enhance/Reforge/Socket RED**

Cover insufficient stone/sand/gem, +10 cap, stale pending Reforge, accept/keep, invalid socket, replacement-full, manual lock permissions, route lock, XP once, and byte-identical failure.

- [ ] **Step 2: wrap Enhance with Tool XP**

Validate one input ref, permit lock, call `EnhanceInstance` on a candidate, add target-quality XP only after success, normalize/validate, commit.

- [ ] **Step 3: preserve explicit Reforge choice**

Begin creates the existing paid pending preview and awards quality XP in the same candidate. Append `bToolExperienceAwarded=true` to pending state so accept/keep cannot award again. Expose both resolve buttons; never auto-resolve from Workbench.

- [ ] **Step 4: implement Socket request**

```cpp
struct FGameXXKSocketGemRequest
{
	FGameXXKToolInputRef EquipmentInput;
	FGameXXKToolInputRef GemInput;
	int32 SocketIndex = INDEX_NONE;
};
```

Permit both locked equipment and a manually supplied locked gem, because Socket is an explicit manual action rather than auto-selection or Combine/Dismantle consumption. Validate capacity/type/quantity, decrement one gem, return an old gem to Backpack, write the new socket, add equipment-quality XP, normalize/validate, and commit. Full Backpack rejects replacement before decrement.

- [ ] **Step 5: complete rollback tests**

Cover insufficient stone/sand/gem, +10 cap, stale pending Reforge, accept/keep, invalid socket, replacement-full, lock rules, route lock, XP once, and byte-identical failure.

- [ ] **Step 6: run GREEN and commit**

Commit:

```powershell
git commit -m "feat: complete enhance reforge and socket tools"
```

### Task 7: build complete Tools UI

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Write and run Workbench Tools RED**

Add causal `OnClicked.Broadcast()` tests for all five tabs, Auto Fill Equipment/Item, Include Warehouse persistence, crafting-level boundaries, invalid drop retention, Reforge choices, every socket, result refresh, and failure notices.

- [ ] **Step 2: add UI state**

Persist only Tool Progress and Include Warehouse in runtime. Keep active mode, Combine kind, input reservations, selected socket, and preview widgets transient. Add Equipment/Item tabs, Auto Fill, Include Warehouse checkbox, crafting-level decrement/value/increment, output/cost/XP text, and disabled reason.

- [ ] **Step 3: enforce per-mode input shape**

Drop validation rules:

```text
Dismantle: equipment slots 1..9, unlocked
Combine Equipment: equipment slots exactly 9, same quality, unlocked
Combine Item: one gem stack with quantity >=9, unlocked
Enhance/Reforge: equipment slot 0 only
Socket: equipment slot 0, gem slot 1
```

Invalid drop keeps carry active and shows the reason.

- [ ] **Step 4: add explicit Reforge and socket controls**

When a preview exists, render old/new affix values and buttons `采用新词缀` / `保留原词缀`. Socket mode renders quality-derived socket buttons and current gem icons; confirm labels insert or replace.

- [ ] **Step 5: make every success visible**

After commit refresh Tool Level/XP, costs, materials, gold, input cells, equipment detail, character stats, and lock overlays. Return reservations only after the authoritative rule succeeds.

- [ ] **Step 6: run Workbench GREEN**

- [ ] **Step 7: commit**

```powershell
git commit -m "feat: connect every workbench tool mode"
```

### Task 8: full Tool work package verification

- [ ] **Step 1: cold build**

Save/stop/close UE safely, then cold UBT with no Hot Reload.

- [ ] **Step 2: focused suites**

Run:

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Equipment+GameXXK.MVP.UI.CharacterBackpackModel+GameXXK.DesktopTraining.Workbench" --automation-report EquipmentGemsToolsFinal --json
```

- [ ] **Step 3: independent reviews and real PIE**

Complete spec then quality review. In PIE, demonstrate one success and one failure for each Tool mode, lock exclusion, Equipment/Gem Auto Fill, Tool XP level-up, and per-character socket stat projection. Keep Training chest work pending.
