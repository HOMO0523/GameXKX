# Meta Shop and Quality Economy Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore the permanent shop as the second backpack toolbar action, retain six 10000-gold set packs, replace the obsolete companion pack with a 5000-gold gem pack, and apply the approved chest and probabilistic nine-to-one quality rules.

**Architecture:** Existing MetaShop remains the single town/permanent shop and keeps its seven-card layout. Product generation, chest quality and combine quality use integer-weight deterministic rules and candidate RuntimeState transactions. Serialized companion product IDs remain reserved for compatibility but can no longer be listed or purchased.

**Tech Stack:** UE 5.8 C++, GameXXK MetaShop/Equipment/Gem/Training/Tool rules, UMG, save migration v31, Automation Tests, deterministic asset generation/import.

---

## File map

- Modify `Source/GameXXK/Public/GameXXKMetaShopTypes.h` — add GemPack without reordering retired CompanionPack.
- Modify `Source/GameXXK/Public/GameXXKMetaShopRules.h` and private `.cpp` — prices, seven products, gem transaction.
- Modify `Source/GameXXK/Public/UI/GameXXKMetaShopWidget.h` and private `.cpp` — remove companion branches and show gem results.
- Modify `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h` and private `.cpp` — six-button toolbar geometry.
- Modify `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h` and private `.cpp` — shop action after pin.
- Modify `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h` and private `.cpp` — open/close shared MetaShop from desktop and NarrativeSequence command.
- Modify `Source/GameXXK/Public/GameXXKMVPRules.h` — `NextCombineOrdinal`.
- Modify `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h` and private `.cpp` — v31 shop/combine migration and obsolete order refund.
- Modify `Source/GameXXK/Public/GameXXKTrainingChestRules.h` and private `.cpp` — exact PPM quality tables.
- Modify `Source/GameXXK/Public/GameXXKEquipmentToolRules.h` and private `.cpp` — deterministic probabilistic combine quality.
- Create `SourceArt/UI/MetaShop/T_MetaShop_GemPack.png` — approved gem-pack source art.
- Create `Content/GameXXK/UI/MetaShop/V2/T_MetaShop_GemPack.uasset` — imported UI texture.
- Modify existing MetaShop, Workbench, Chest, Tool and Save Migration tests.

### Task 1: Replace the companion product with GemPack

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKMetaShopTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKMetaShopRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMetaShopRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKMetaShopRulesTest.cpp`

- [ ] **Step 1: Write failing seven-product contract tests**

Expect exactly six set packs followed by GemPack, prices and retired CompanionPack rejection:

```cpp
const TArray<FGameXXKMetaShopProductDefinition>& Products = FGameXXKMetaShopRules::GetProducts();
TestEqual(TEXT("seven products"), Products.Num(), 7);
TestEqual(TEXT("gem is seventh"), Products.Last().ProductId, EGameXXKMetaShopProductId::GemPack);
for (int32 Index = 0; Index < 6; ++Index)
    TestEqual(TEXT("set pack costs 10000"), Products[Index].Price, 10000);
TestEqual(TEXT("gem pack costs 5000"), Products.Last().Price, 5000);
TestNull(TEXT("companion pack retired"), FGameXXKMetaShopRules::FindProduct(EGameXXKMetaShopProductId::CompanionPack));
```

- [ ] **Step 2: Run RED**

Run `GameXXK.MetaShop`; expect old 100/500 product contract failures.

- [ ] **Step 3: Change types without serialized reordering**

Keep `CompanionPack` at its old numeric value and append:

```cpp
UENUM(BlueprintType)
enum class EGameXXKMetaShopProductId : uint8
{
    Invalid = 0,
    PoJunPack,
    XuanJiaPack,
    QingNangPack,
    ZhuiFengPack,
    ShiGuPack,
    ShanHePack,
    CompanionPack UMETA(Hidden),
    GemPack
};

enum class EGameXXKMetaShopProductKind : uint8
{
    EquipmentPack = 0,
    CompanionPack UMETA(Hidden),
    GemPack
};
```

Add `EquipmentPackPrice = 10000`, `GemPackPrice = 5000`. `BuildProducts` returns six set packs plus GemPack. `FindProduct(CompanionPack)` returns null because it is absent from the catalog.

- [ ] **Step 4: Run GREEN and commit**

```powershell
git add -- Source/GameXXK/Public/GameXXKMetaShopTypes.h Source/GameXXK/Public/GameXXKMetaShopRules.h Source/GameXXK/Private/GameXXKMetaShopRules.cpp Source/GameXXK/Private/Tests/GameXXKMetaShopRulesTest.cpp
git commit -m "feat: replace companion pack with gem pack"
```

### Task 2: Implement atomic GemPack purchase

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKMetaShopTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKMetaShopRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKMetaShopRulesTest.cpp`

- [ ] **Step 1: Write failing deterministic gem tests**

For fixed seeds/ordinals, verify attack/defense/health types appear uniformly over a large deterministic sample and quality counts approach 70/25/5. Verify the result exposes `GeneratedItemId`/quantity and 5000 gold delta. Test full backpack with a new stack, existing-stack success, overflow and invalid state without mutation.

- [ ] **Step 2: Add result fields and transaction**

```cpp
UPROPERTY(BlueprintReadOnly) FName GeneratedItemId = NAME_None;
UPROPERTY(BlueprintReadOnly) int32 GeneratedItemQuantity = 0;
```

Append `BackpackFull`, `GemCreationFailed` and `QuantityOverflow` to `EGameXXKMetaShopError` without reordering existing values, and provide explicit localized messages.

For GemPack, derive the stream from MetaShop seed, purchase ordinal and product ID; roll type uniformly 1–3 and quality with `<=70 Common`, `<=95 Rare`, else Epic. Add one `FGameXXKGemRules::MakeItemId` to shared backpack, normalize physical inventory, then deduct gold and increment purchase ordinal only after validation.

- [ ] **Step 3: Run GREEN and commit**

```powershell
git add -- Source/GameXXK/Public/GameXXKMetaShopTypes.h Source/GameXXK/Private/GameXXKMetaShopRules.cpp Source/GameXXK/Private/Tests/GameXXKMetaShopRulesTest.cpp
git commit -m "feat: purchase deterministic gem packs"
```

### Task 3: Update shop art, UI and backpack toolbar entry

**Files:**
- Create: `SourceArt/UI/MetaShop/T_MetaShop_GemPack.png`
- Create: `Content/GameXXK/UI/MetaShop/V2/T_MetaShop_GemPack.uasset`
- Modify: `Source/GameXXK/Public/UI/GameXXKMetaShopWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKMetaShopWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: MetaShop and Workbench Widget tests.

- [ ] **Step 1: Write failing UI/layout tests**

Expect top toolbar labels/order `置顶, 店, 声, 信, 设置, 退`, no overlap at 100%/50% HUD scale, GemPack as seventh product, no companion portrait/result/replacement delegate, and gem result icon/name.

- [ ] **Step 2: Produce and approve GemPack art**

Generate one 512×512 transparent-background simplified ink-cartoon pouch containing three distinct attack/defense/health gem silhouettes. Match existing equipment-pack density and palette; do not use SVG or text. Show the image to the user before import. Verify dimensions, RGBA mode and zero outer-edge alpha.

- [ ] **Step 3: Import art and update UI**

Import as `/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_GemPack`. Remove all companion result/replacement branches. Gem result uses `FGameXXKGemRules::GetIconTexturePathForItemId` and displays exact type/quality.

Add `ActionOpenMetaShop` immediately after the pin button. Both toolbar and NarrativeSequence command `openShop` call the same PlayerController `OpenMetaShop`; close restores the previous workbench/narrative modal owner.

- [ ] **Step 4: Run visual/unit tests and commit**

```powershell
git add -- SourceArt/UI/MetaShop/T_MetaShop_GemPack.png Content/GameXXK/UI/MetaShop/V2/T_MetaShop_GemPack.uasset Source/GameXXK/Public/UI/GameXXKMetaShopWidget.h Source/GameXXK/Private/UI/GameXXKMetaShopWidget.cpp Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Private/Tests/GameXXKMetaShopWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp
git commit -m "feat: restore permanent shop toolbar entry"
```

### Task 4: Migrate obsolete recruitment state and add combine ordinal

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Modify: save migration tests.

- [ ] **Step 1: Write failing v30→v31 tests**

Expect `NextCombineOrdinal = 0`. Preserve owned companions. If a paid unresolved companion recruitment/order exists, clear both pending structures and refund exactly 500 gold once; already resolved/no-pending saves receive no refund. Re-running migration must not refund again.

- [ ] **Step 2: Implement migration**

Add `MetaShopGemAndCombineProbabilityIntroducedSaveVersion = 31`, advance CurrentSaveVersion, and add:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
int32 NextCombineOrdinal = 0;
```

to `FGameXXKToolProgress`. Validate nonnegative/non-`MAX_int32` ordinal. Migration clears obsolete pending recruitment data after the one-time refund.

- [ ] **Step 3: Run GREEN and commit**

```powershell
git add -- Source/GameXXK/Public/GameXXKMVPRules.h Source/GameXXK/Public/MVP/GameXXKSaveMigration.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp Source/GameXXK/Private/Tests/GameXXKCompanionBirthPoolMigrationTest.cpp
git commit -m "feat: migrate retired partner shop state"
```

### Task 5: Apply exact normal/advanced chest quality tables

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKTrainingChestRules.h`
- Modify: `Source/GameXXK/Private/GameXXKTrainingChestRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTrainingChestRulesTest.cpp`

- [ ] **Step 1: Write failing PPM boundary tests**

Expose pure `QualityRankFromRoll(Tier, RollZeroTo999999)`. Test every interval boundary and exact total:

```cpp
// Normal PPM: Common 679778, Rare 250000, Epic 50000,
// Legendary 20000, Immortal 200, Treasure 20, Transcendent 2.
// Advanced PPM: Common 469112, Rare 250000, Epic 200000,
// Legendary 80000, Immortal 800, Treasure 80, Transcendent 8.
```

Assert no chest returns Celestial, Ascendant or Cosmic.

- [ ] **Step 2: Implement integer tables**

Use cumulative integer PPM comparisons only; no float probability. Return rank 1–7, then convert with `FGameXXKEquipmentQualityRules::EquipmentQualityFromRank` or `FGameXXKGemRules::QualityFromRank`. Roll quality once per equipment/gem chest outcome and reuse it for the generated item. Materials keep their existing quantities and do not consume a quality roll.

- [ ] **Step 3: Run GREEN and commit**

```powershell
git add -- Source/GameXXK/Public/GameXXKTrainingChestRules.h Source/GameXXK/Private/GameXXKTrainingChestRules.cpp Source/GameXXK/Private/Tests/GameXXKTrainingChestRulesTest.cpp
git commit -m "feat: apply approved chest quality odds"
```

### Task 6: Apply probabilistic nine-to-one rules to equipment and gems

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKEquipmentToolRules.h`
- Modify: `Source/GameXXK/Private/GameXXKEquipmentToolRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentToolRulesTest.cpp`

- [ ] **Step 1: Write failing pure quality-roll tests**

Expose `ResolveCombineOutputRank(InputRank, RollZeroTo999)`. Test all boundaries:

```text
1→2 1000/1000
2→3 1000/1000
3→4 1000/1000
4: 499→5, 499→6, 2→7
5: 499→6, 499→7, 2→8
6: 499→7, 499→8, 2→9
7: 659→7, 339→8, 2→9
8: 659→8, 339→9, 2→10
9: 750→9, 250→10
10: invalid recipe
```

- [ ] **Step 2: Add deterministic transaction seed**

Derive roll from collection seed, `NextCombineOrdinal`, combine kind, input quality and sorted input IDs/item ID. Increment ordinal only on successful atomic commit. Equipment and gem combines use the same resolved rank and still consume nine inputs when rank stays unchanged.

- [ ] **Step 3: Preserve output/container behavior**

Equipment continues to roll set, slot and tool-level item range and outputs to Backpack first, Warehouse second. Gem keeps its input type and outputs to an existing stack, Backpack first, Warehouse second. Cosmic inputs are rejected before mutation.

- [ ] **Step 4: Run GREEN and commit**

```powershell
git add -- Source/GameXXK/Public/GameXXKEquipmentToolRules.h Source/GameXXK/Private/GameXXKEquipmentToolRules.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentToolRulesTest.cpp
git commit -m "feat: add probabilistic high-tier combining"
```

### Task 7: Final economy regression and PIE acceptance

**Files:**
- Modify: MetaShop/Workbench/Chest/Tool tests only if evidence reveals a real contract gap.
- Create: `Content/Python/gamexxk_probe_meta_shop_quality_flow.py`.
- Create: `scripts/run_meta_shop_quality_acceptance.py`.

- [ ] **Step 1: Run deterministic statistical audits**

Run at least one million pure rolls per chest table and one million per probabilistic combine row. Expected counts equal exact integer bucket sizes because the test iterates the complete roll domain, not stochastic tolerances.

- [ ] **Step 2: Run real PIE purchase flow**

From `L_DesktopTrainingHUD`: open Backpack, verify six toolbar buttons and Shop second; buy every set pack and GemPack with fixed funds; inspect generated set/type/quality, realtime gold, capacity rejection and no companion UI.

- [ ] **Step 3: Run regression/build gates**

Run `GameXXK.MetaShop`, `GameXXK.Gems`, `GameXXK.Equipment.Tools`, `GameXXK.Training.Chests`, Workbench, SaveGame, Editor Target and Game Target cold builds. Expected 0 failures/errors.

- [ ] **Step 4: Leave correct PIE and commit harness**

```powershell
git add -- Content/Python/gamexxk_probe_meta_shop_quality_flow.py scripts/run_meta_shop_quality_acceptance.py
git commit -m "test: verify permanent shop quality economy"
```
