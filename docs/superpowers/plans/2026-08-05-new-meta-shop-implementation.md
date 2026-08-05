# GameXXK New Meta Shop Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the player-facing legacy town merchant with a deterministic seven-product meta shop that sells six equipment-set packs and one companion pack through the approved UI V2 presentation.

**Architecture:** Add a focused `FGameXXKMetaShopRules` transaction layer and saved meta-shop state, expose it through `UGameXXKMVPSubsystem`, and render it in a dedicated `UGameXXKMetaShopWidget`. The town merchant and controller route only to the new widget; legacy item buy/sell APIs remain serialization-compatible but are unreachable from player UI. Route merchant rules and UI remain isolated and unchanged.

**Tech Stack:** Unreal Engine 5.8 C++, UMG, SaveGame migration, UE Automation Tests, UE MCP, project Python asset-import scripts, approved 512×512 UI V2 PNG assets.

---

## Execution order override: Master UI first

Per the user's 2026-08-05 direction, complete Task 0 and obtain a reviewable Master UI result before starting Task 1. Task 0 is pure visual/master-art work: do not use TDD; validate only after generation with manifest, dimensions, alpha/source checks, hashes, and visual review. Tasks 1–10 remain the later code/integration phase and keep their normal TDD requirements.

## Task 0: Iterate the Master UI before code integration

**Files:**

- Modify: `scripts/gamexxk_ui_master_pages.py`
- Modify: `scripts/build_gamexxk_ui_master.py`
- Modify: `SourceArt/UI/PSD/gamexxk-v4/ui-master/ui-master-spec.json`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Previews/00_公共组件.png`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Previews/03_主角背包.png`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Previews/07_商店交易.png`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/GameXXK_UI_Master_ContactSheet.png`
- Generate: `SourceArt/UI/PSD/gamexxk-v4/ui-master/master-manifest.json`
- Modify: `docs/production/2026-08-04-gamexxk-ui-calibration-v2-progress.md`

- [x] **Step 1: Route the three priority pages to approved V2 sources**

Use `calibration-v2/Generated/hero_backpack_large_panel_clean.png`, the 28 approved controls, the final Hero Idle, the 45 approved equipment/item icons, and editable text. Do not read rejected procedural V1 art, the rejected seven-item sheet, Legacy compatibility sheet, or belt drafts.

- [x] **Step 2: Replace page 07 with the new seven-product meta shop**

Render six equipment-pack cards in set order using the six approved weapon icons, plus one companion-pack card using `Content/nav_companion.png`. Show fixed prices 100/500, permanent gold, a selected product detail area, `普通 70% / 稀有 25% / 珍稀 5%`, confirmation affordance, and no medicine shelf, selling control, or legacy three-slot equipment.

On page `03_主角背包`, replace the 16-slot viewport with 20 visible slots in a 4×5 grid, show total capacity 200, and place the vertical scrollbar on the right side of the grid.

- [x] **Step 3: Rebuild canonical Master outputs**

Run: `python scripts/build_gamexxk_ui_master.py`

Expected: `ok: true`, 18 pages at 1920×1080, priority pages marked `v2_master`, and the contact sheet regenerated.

- [x] **Step 4: Run post-generation checks**

Run `python -m py_compile scripts/gamexxk_ui_master_pages.py scripts/build_gamexxk_ui_master.py`, validate every manifest path, verify the three priority previews are 1920×1080, verify page 07 contains seven product image layers and no legacy product names, and run `git diff --check`.

- [x] **Step 5: Visually review at original resolution**

Inspect the three priority pages and contact sheet for paper/ink consistency, readable hierarchy, icon fill, no crop seams, no stretched controls, and exact 6+1 product presentation. Record hashes and review status in the production note.

- [x] **Step 6: Commit the Master UI milestone**

Stage only the Master builder/page changes, canonical manifests/previews/contact sheet, and production record. Keep UE import and C++ untouched until the visual milestone is accepted.

```powershell
git commit -m "feat: iterate Master UI for new meta shop"
```

## File map

**New rule and data units**

- Create `Source/GameXXK/Public/GameXXKMetaShopTypes.h`: product IDs, product view/result payloads, errors, and saved meta-shop state.
- Create `Source/GameXXK/Public/GameXXKMetaShopRules.h`: immutable catalog, preview, deterministic seed, and purchase API.
- Create `Source/GameXXK/Private/GameXXKMetaShopRules.cpp`: atomic equipment/companion pack transactions.
- Create `Source/GameXXK/Private/Tests/GameXXKMetaShopRulesTest.cpp`: catalog, probability boundaries, atomicity, and deterministic results.

**Existing runtime and migration**

- Modify `Source/GameXXK/Public/GameXXKMVPRules.h`: embed `FGameXXKMetaShopState` in `FGameXXKRuntimeState`.
- Modify `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`: bump save version from 10 to 11 and name the meta-shop schema gate.
- Modify `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`: initialize and validate meta-shop state for old/current saves.
- Modify `Source/GameXXK/Private/GameXXKMVPRules.cpp`: initialize new-game meta-shop seed and preserve it in save projections.
- Modify `Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp`: v10→v11 migration, round-trip, and corruption rejection.

**Facade and UI**

- Modify `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h` and `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`: product views, preview, purchase, and last result.
- Create `Source/GameXXK/Public/UI/GameXXKMetaShopWidget.h` and `Source/GameXXK/Private/UI/GameXXKMetaShopWidget.cpp`: dedicated full-screen seven-card store.
- Create `Source/GameXXK/Private/Tests/GameXXKMetaShopFacadeTest.cpp` and `Source/GameXXK/Private/Tests/GameXXKMetaShopWidgetTest.cpp`.

**Assets and player flow**

- Create `Content/Python/gamexxk_import_meta_shop_ui_v2.py`: import six approved weapon icons and the approved companion navigation icon.
- Create `scripts/validate_meta_shop_ui_v2.py` and `scripts/test_validate_meta_shop_ui_v2.py`: source/target manifest validation.
- Modify `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h` and `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`: own and open the new widget; reject legacy merchant opening.
- Modify `Source/GameXXK/Private/Town/GameXXKTownNpcActor.cpp` and `Source/GameXXK/Private/Town/GameXXKTownNpcCharacter.cpp`: merchant `F` calls `OpenMetaShopWindow`.
- Modify legacy merchant assertions in `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`, `GameXXKMVPUIWidgetTest.cpp`, `GameXXKMVPFlowTest.cpp`, `GameXXKMVPPlayableShellTest.cpp`, and `GameXXKPlayableRootWidgetTest.cpp`.
- Modify `Content/Python/gamexxk_probe_inventory_window.py` or create `Content/Python/gamexxk_probe_meta_shop_window.py` when the old probe cannot express the new widget contract.
- Modify `docs/production/2026-07-03-playable-entry-flow/05-implementation-log.md` and `06-test-results.md` with final evidence.

## Task 1: Freeze meta-shop types and seven-product catalog

**Files:**

- Create: `Source/GameXXK/Public/GameXXKMetaShopTypes.h`
- Create: `Source/GameXXK/Public/GameXXKMetaShopRules.h`
- Create: `Source/GameXXK/Private/GameXXKMetaShopRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKMetaShopRulesTest.cpp`

- [x] **Step 1: Add a failing catalog automation test**

Create a test named `GameXXK.MetaShop.Catalog` that asserts seven products, stable order, six equipment packs priced at 100, one companion pack priced at 500, unique IDs, and the exact set mapping:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameXXKMetaShopCatalogTest,
    "GameXXK.MetaShop.Catalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMetaShopCatalogTest::RunTest(const FString& Parameters)
{
    const TArray<FGameXXKMetaShopProductDefinition>& Products = FGameXXKMetaShopRules::GetProducts();
    TestEqual(TEXT("seven fixed products"), Products.Num(), 7);
    TestEqual(TEXT("PoJun first"), Products[0].EquipmentSet, EGameXXKEquipmentSet::PoJun);
    TestEqual(TEXT("ShanHe sixth"), Products[5].EquipmentSet, EGameXXKEquipmentSet::ShanHe);
    TestEqual(TEXT("equipment price"), Products[0].Price, 100);
    TestEqual(TEXT("companion price"), Products[6].Price, 500);
    TestEqual(TEXT("companion last"), Products[6].Kind, EGameXXKMetaShopProductKind::CompanionPack);
    return true;
}
```

- [x] **Step 2: Run a cold compile and verify the red failure**

Run: `python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 120`

Expected: compile fails because `GameXXKMetaShopRules.h` and its catalog types do not exist.

- [x] **Step 3: Define the complete public types**

Define stable enums and save payloads in `GameXXKMetaShopTypes.h`:

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
    CompanionPack
};

UENUM(BlueprintType)
enum class EGameXXKMetaShopProductKind : uint8
{
    EquipmentPack = 0,
    CompanionPack
};

UENUM(BlueprintType)
enum class EGameXXKMetaShopError : uint8
{
    None = 0,
    InvalidProduct,
    NotInTown,
    InsufficientGold,
    WarehouseFull,
    PendingCompanionExists,
    PurchaseOrdinalExhausted,
    EquipmentCreationFailed,
    CompanionCreationFailed,
    InvalidRuntimeState
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKMetaShopState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
    int32 Seed = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
    int32 NextPurchaseOrdinal = 0;
};
```

Also define `FGameXXKMetaShopProductDefinition`, `FGameXXKMetaShopPurchasePreview`, and `FGameXXKMetaShopPurchaseResult` with Blueprint-readable product ID, kind, equipment set, price, availability, error/message, gold delta, generated equipment ID, and companion result.

- [x] **Step 4: Implement the immutable catalog**

Expose this focused interface in `GameXXKMetaShopRules.h`:

```cpp
class GAMEXXK_API FGameXXKMetaShopRules final
{
public:
    static constexpr int32 EquipmentPackPrice = 100;
    static constexpr int32 CompanionPackPrice = 500;

    static const TArray<FGameXXKMetaShopProductDefinition>& GetProducts();
    static const FGameXXKMetaShopProductDefinition* FindProduct(EGameXXKMetaShopProductId ProductId);
    static bool ValidateState(const FGameXXKRuntimeState& State, FString* OutError = nullptr);
    static int32 DeriveSeed(const FGameXXKRuntimeState& State);
    static FText GetErrorMessage(EGameXXKMetaShopError Error);
};
```

Build products in the exact approved order and use icon soft paths under `/Game/GameXXK/UI/MetaShop/V2/`.

- [x] **Step 5: Run catalog automation and verify green**

Run the cold pipeline, then run:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests GameXXK.MetaShop.Catalog;Quit' '-TestExit=Automation Test Queue Empty' -log
```

Expected: UBT succeeds and `GameXXK.MetaShop.Catalog` reports Success.

- [x] **Step 6: Commit the catalog slice**

```powershell
git add -- Source/GameXXK/Public/GameXXKMetaShopTypes.h Source/GameXXK/Public/GameXXKMetaShopRules.h Source/GameXXK/Private/GameXXKMetaShopRules.cpp Source/GameXXK/Private/Tests/GameXXKMetaShopRulesTest.cpp
git commit -m "feat: define meta shop catalog"
```

## Task 2: Persist and migrate meta-shop state as save version 11

**Files:**

- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h:370-500`
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h:20-31`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp:700-900`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp:1600-1670,3130-3200`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp`

- [x] **Step 1: Add failing v10→v11 migration tests**

Add `GameXXK.MetaShop.SaveMigration` assertions that a valid v10 save receives a positive deterministic seed and ordinal zero, current v11 round-trips byte-stably, negative ordinals are rejected, and future versions remain rejected:

```cpp
FGameXXKSaveState VersionTen = UGameXXKMVPRules::MakeSaveState(UGameXXKMVPRules::CreateNewGame());
VersionTen.SaveVersion = 10;
VersionTen.RuntimeState.MetaShop = FGameXXKMetaShopState();

FGameXXKSaveState Migrated;
FGameXXKSaveMigrationReport Report;
TestTrue(TEXT("v10 migrates"), FGameXXKSaveMigration::MigrateToCurrent(VersionTen, Migrated, Report));
TestEqual(TEXT("target v11"), Migrated.SaveVersion, 11);
TestTrue(TEXT("seed initialized"), Migrated.RuntimeState.MetaShop.Seed > 0);
TestEqual(TEXT("ordinal starts at zero"), Migrated.RuntimeState.MetaShop.NextPurchaseOrdinal, 0);
```

- [x] **Step 2: Run the target compile and verify red**

Run: `python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 120`

Expected: compile fails because `FGameXXKRuntimeState::MetaShop` and save version 11 do not exist.

- [x] **Step 3: Embed the state and bump the schema**

Add to `FGameXXKRuntimeState`:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
FGameXXKMetaShopState MetaShop;
```

In `GameXXKSaveMigration.h` set:

```cpp
static constexpr int32 MetaShopIntroducedSaveVersion = 11;
static constexpr int32 CurrentSaveVersion = 11;
```

- [x] **Step 4: Implement deterministic initialization and validation**

Use one shared derivation function rather than wall-clock randomness:

```cpp
int32 FGameXXKMetaShopRules::DeriveSeed(const FGameXXKRuntimeState& State)
{
    const uint32 Mixed = HashCombine(
        GetTypeHash(State.EquipmentCollection.CollectionSeed),
        GetTypeHash(State.CardRun.CompanionRoster.RecruitSequenceSeed));
    return FMath::Max(1, static_cast<int32>(Mixed & 0x7fffffffU));
}
```

For saves below version 11, set `Seed = DeriveSeed(Candidate.RuntimeState)` and `NextPurchaseOrdinal = 0`. Current-version validation requires `Seed > 0` and `0 <= NextPurchaseOrdinal < MAX_int32`. New-game creation initializes the same fields after equipment and companion seeds exist.

- [x] **Step 5: Run migration and equipment regression automation**

Run cold compile, then:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests GameXXK.MetaShop.SaveMigration;Automation RunTests GameXXK.Equipment.SaveMigration;Quit' '-TestExit=Automation Test Queue Empty' -log
```

Expected: both prefixes report zero failures.

- [x] **Step 6: Commit the save slice**

```powershell
git add -- Source/GameXXK/Public/GameXXKMVPRules.h Source/GameXXK/Public/MVP/GameXXKSaveMigration.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp Source/GameXXK/Private/GameXXKMVPRules.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp
git commit -m "feat: persist meta shop sequence"
```

## Task 3: Implement deterministic equipment-pack transactions

**Files:**

- Modify: `Source/GameXXK/Public/GameXXKMetaShopRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMetaShopRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKMetaShopRulesTest.cpp`

- [ ] **Step 1: Add failing equipment purchase tests**

Add `GameXXK.MetaShop.EquipmentPurchase` covering all six products, forced product set, slot range, player level clamp, exact 100-gold debit, warehouse insertion, ordinal advance, deterministic replay, 70/25/5 threshold mapping, insufficient gold, warehouse full, ordinal exhaustion, and corrupt-state byte equality after failure.

Use a public testable roll helper so exact boundaries do not require sampling:

```cpp
TestEqual(TEXT("70 is common"), FGameXXKMetaShopRules::QualityFromRoll(70), EGameXXKEquipmentQuality::Common);
TestEqual(TEXT("71 is rare"), FGameXXKMetaShopRules::QualityFromRoll(71), EGameXXKEquipmentQuality::Rare);
TestEqual(TEXT("95 is rare"), FGameXXKMetaShopRules::QualityFromRoll(95), EGameXXKEquipmentQuality::Rare);
TestEqual(TEXT("96 is epic"), FGameXXKMetaShopRules::QualityFromRoll(96), EGameXXKEquipmentQuality::Epic);
```

- [ ] **Step 2: Run the test and verify red**

Run the cold pipeline.

Expected: compile/link fails because `PreviewPurchase`, `Purchase`, and `QualityFromRoll` are missing.

- [ ] **Step 3: Add the purchase interface**

Add:

```cpp
static EGameXXKEquipmentQuality QualityFromRoll(int32 RollOneToHundred);
static bool PreviewPurchase(
    const FGameXXKRuntimeState& State,
    EGameXXKMetaShopProductId ProductId,
    FGameXXKMetaShopPurchasePreview& OutPreview);
static bool Purchase(
    FGameXXKRuntimeState& InOutState,
    EGameXXKMetaShopProductId ProductId,
    FGameXXKMetaShopPurchaseResult& OutResult);
```

- [ ] **Step 4: Implement equipment purchase on a state copy**

Mix `MetaShop.Seed`, `NextPurchaseOrdinal`, and product ID into a local `FRandomStream`. Draw slot `0..5`, draw quality `1..100`, and create:

```cpp
FGameXXKEquipmentCreateRequest Request;
Request.Set = Product->EquipmentSet;
Request.Quality = QualityFromRoll(QualityRoll);
Request.ItemLevel = FMath::Clamp(Candidate.PlayerLevel, 1, FGameXXKEquipmentRules::MaxItemLevel);
Request.bForceSlot = true;
Request.ForcedSlot = static_cast<EGameXXKEquipmentSlot>(SlotIndex + 1);
```

Perform `CreateRolledInstance`, subtract exactly 100 gold, increment the ordinal only after all checks, synchronize runtime mirrors, validate the complete candidate, then move the candidate into `InOutState`. On failure reset `OutResult` to a typed failure and leave input bytes unchanged.

- [ ] **Step 5: Run equipment purchase automation**

Run cold compile and `Automation RunTests GameXXK.MetaShop.EquipmentPurchase`.

Expected: all assertions pass; `GameXXK.Equipment` regressions remain green.

- [ ] **Step 6: Commit equipment-pack transactions**

```powershell
git add -- Source/GameXXK/Public/GameXXKMetaShopRules.h Source/GameXXK/Private/GameXXKMetaShopRules.cpp Source/GameXXK/Private/Tests/GameXXKMetaShopRulesTest.cpp
git commit -m "feat: purchase deterministic equipment packs"
```

## Task 4: Implement companion-pack transactions

**Files:**

- Modify: `Source/GameXXK/Private/GameXXKMetaShopRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKMetaShopRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCompanionRecruitmentFlowTest.cpp`

- [ ] **Step 1: Add failing companion purchase tests**

Add `GameXXK.MetaShop.CompanionPurchase` for: 500-gold debit, deterministic explicit recruit order, direct recruit below capacity, persisted candidate at capacity 12, rejection while a candidate exists, cancel without refund, same pre-purchase state producing the same candidate, and atomic failure on invalid roster.

```cpp
FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
State.Screen = EGameXXKScreen::Town;
State.PlayerGold = 500;
const int32 GoldBefore = State.PlayerGold;
FGameXXKMetaShopPurchaseResult Result;
TestTrue(TEXT("companion pack succeeds"), FGameXXKMetaShopRules::Purchase(State, EGameXXKMetaShopProductId::CompanionPack, Result));
TestEqual(TEXT("spends 500"), State.PlayerGold, GoldBefore - 500);
TestEqual(TEXT("ordinal advances"), State.MetaShop.NextPurchaseOrdinal, 1);
```

- [ ] **Step 2: Run and verify red behavior**

Run cold compile and the new test prefix.

Expected: equipment products pass while companion product reports an unimplemented/typed failure.

- [ ] **Step 3: Implement companion purchase through existing roster rules**

On a full runtime-state copy, derive a saved order seed from the meta-shop seed and ordinal, then call:

```cpp
FGameXXKCompanionRecruitOrder Order;
if (!FGameXXKCompanionRules::CreateRecruitOrder(Candidate.CardRun.CompanionRoster, OrderSeed, Order, &Error)
    || !FGameXXKCompanionRules::ResolvePendingRecruitOrder(Candidate.CardRun.CompanionRoster, OutResult.CompanionResult, &Error))
{
    return Fail(EGameXXKMetaShopError::CompanionCreationFailed, Error, OutResult);
}
```

Reject before ordering if `PendingRecruitment.bHasPendingRecruitment` or `PendingRecruitOrder.bHasPendingOrder` is already true. Debit 500 and advance meta-shop ordinal in the same candidate transaction. Do not alter `DiscardPendingRecruitment`; its existing behavior already advances the recruitment sequence and does not touch gold.

- [ ] **Step 4: Run companion and replacement regressions**

Run `GameXXK.MetaShop.CompanionPurchase`, `GameXXK.Companion.Recruitment`, and `GameXXK.Equipment.CompanionReplacement` after a cold build.

Expected: direct, duplicate/protection, full-roster pending, replacement, and discard paths pass.

- [ ] **Step 5: Commit companion-pack transactions**

```powershell
git add -- Source/GameXXK/Private/GameXXKMetaShopRules.cpp Source/GameXXK/Private/Tests/GameXXKMetaShopRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKCompanionRecruitmentFlowTest.cpp
git commit -m "feat: purchase deterministic companion packs"
```

## Task 5: Expose the new subsystem facade

**Files:**

- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h:70-120`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp:700-920`
- Create: `Source/GameXXK/Private/Tests/GameXXKMetaShopFacadeTest.cpp`

- [ ] **Step 1: Add failing facade tests**

Create `GameXXK.MetaShop.Facade` to assert the subsystem returns seven views, previews without mutation, purchases only in town, commits successful results, and rejects route/battle screens without changing state.

- [ ] **Step 2: Run the compile and verify missing facade symbols**

Run the cold pipeline.

Expected: link failure for the new facade signatures.

- [ ] **Step 3: Add Blueprint-callable facade methods**

Add exact methods:

```cpp
UFUNCTION(BlueprintPure, Category = "GameXXK|MetaShop")
TArray<FGameXXKMetaShopProductDefinition> GetMetaShopProducts() const;

UFUNCTION(BlueprintPure, Category = "GameXXK|MetaShop")
bool PreviewMetaShopPurchase(EGameXXKMetaShopProductId ProductId, FGameXXKMetaShopPurchasePreview& OutPreview) const;

UFUNCTION(BlueprintCallable, Category = "GameXXK|MetaShop")
bool PurchaseMetaShopProduct(EGameXXKMetaShopProductId ProductId, FGameXXKMetaShopPurchaseResult& OutResult);
```

`PurchaseMetaShopProduct` first requires `RuntimeState.Screen == EGameXXKScreen::Town`, then delegates to the pure rule. It must not call legacy `BuyItem` or `SellItem`.

- [ ] **Step 4: Run facade and rule automation**

Run `GameXXK.MetaShop` after cold compile.

Expected: catalog, save, equipment, companion, and facade tests all pass.

- [ ] **Step 5: Commit the facade**

```powershell
git add -- Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp Source/GameXXK/Private/Tests/GameXXKMetaShopFacadeTest.cpp
git commit -m "feat: expose meta shop subsystem facade"
```

## Task 6: Import and validate the approved UI V2 product icons

**Files:**

- Create: `Content/Python/gamexxk_import_meta_shop_ui_v2.py`
- Create: `scripts/validate_meta_shop_ui_v2.py`
- Create: `scripts/test_validate_meta_shop_ui_v2.py`
- Read: `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/content-manifest.json`

- [ ] **Step 1: Add failing manifest validation tests**

The Python test must require these seven source names and UE targets:

```python
EXPECTED = {
    "pojun_weapon": "/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_PoJunPack",
    "xuanjia_weapon": "/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_XuanJiaPack",
    "qingnang_weapon": "/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_QingNangPack",
    "zhuifeng_weapon": "/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_ZhuiFengPack",
    "shigu_weapon": "/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_ShiGuPack",
    "shanhe_weapon": "/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_ShanHePack",
    "nav_companion": "/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_CompanionPack",
}
```

Assert each source exists, is 512×512 where applicable, has a non-empty alpha channel, and matches the approved content manifest. The companion navigation icon may retain its existing 256×256 size.

- [ ] **Step 2: Run the Python test and verify red**

Run: `python -m unittest scripts.test_validate_meta_shop_ui_v2 -v`

Expected: failure because the validator/import manifest does not exist.

- [ ] **Step 3: Implement the focused validator and UE importer**

The importer reads only the seven approved files, imports/reuses Texture2D assets under `/Game/GameXXK/UI/MetaShop/V2`, sets UI texture group, disables mipmaps, uses transparent compression settings, and saves only touched packages through the editor Python API. It must not scan or import the rejected seven-item sheet, Legacy compatibility sheet, or belt drafts.

- [ ] **Step 4: Run source validation and editor import**

Run:

```powershell
python -m unittest scripts.test_validate_meta_shop_ui_v2 -v
python scripts/validate_meta_shop_ui_v2.py
python -c "import json,sys; sys.path.insert(0,'scripts'); from ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(timeout=180); c.require_connected(); print(json.dumps(c.run_project_python_file('Content/Python/gamexxk_import_meta_shop_ui_v2.py'), ensure_ascii=False)); print(json.dumps(c.save_dirty_packages(), ensure_ascii=False))"
```

Expected: seven sources validate, seven Texture2D assets exist, and UE MCP reports no remaining dirty packages after save.

- [ ] **Step 5: Commit importer and runtime assets**

Stage only the two scripts, importer, and seven `/Game/GameXXK/UI/MetaShop/V2` assets. Do not stage unrelated untracked source art.

```powershell
git commit -m "feat: import meta shop UI V2 icons"
```

## Task 7: Build the dedicated meta-shop widget

**Files:**

- Create: `Source/GameXXK/Public/UI/GameXXKMetaShopWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKMetaShopWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKMetaShopWidgetTest.cpp`

- [ ] **Step 1: Add failing widget contract tests**

Create `GameXXK.MetaShop.Widget` and require: full-screen paper frame, seven product cards, gold text, product detail, confirm/cancel dialog, disabled reason, purchase result panel, six correct weapon textures, companion texture, and no widget named `MerchantStockGrid` or `SellButton`.

Expose narrow automation seams:

```cpp
bool OpenMetaShopForTest();
bool SelectProductForTest(EGameXXKMetaShopProductId ProductId);
bool RequestPurchaseForTest();
bool ConfirmPurchaseForTest();
bool CancelPurchaseForTest();
int32 GetProductCardCountForTest() const;
FText GetDisabledReasonForTest() const;
FGameXXKMetaShopPurchaseResult GetLastPurchaseResultForTest() const;
```

- [ ] **Step 2: Run cold compile and verify red**

Expected: widget class and test seams are missing.

- [ ] **Step 3: Implement the programmatic UMG tree**

Follow `UGameXXKInventoryWindowWidget` ownership conventions but keep the class focused. Build:

- one dimmed full-screen backdrop;
- one approved paper panel;
- top gold label and close button;
- `UUniformGridPanel` with seven product buttons;
- detail column with price, probability/rules text, and purchase button;
- confirmation overlay;
- success/result overlay.

Every refresh calls `GetMetaShopProducts` and `PreviewMetaShopPurchase`. Confirm calls `PurchaseMetaShopProduct`; a full-roster companion result invokes a delegate that the controller binds to `OpenCompanionRoster`.

- [ ] **Step 4: Bind exact UI behavior**

Use 4 cards on row one and 3 on row two. Equipment descriptions list six possible slots and `普通 70% / 稀有 25% / 珍稀 5%`. Companion description states the 12-person capacity and non-refundable cancellation. On success, equipment result resolves `GetEquipmentTooltipSnapshot`; companion result displays the returned candidate/outcome.

- [ ] **Step 5: Run widget and facade tests**

Run `GameXXK.MetaShop.Widget` and `GameXXK.MetaShop.Facade` after cold compile.

Expected: all UI contract assertions pass without loading legacy merchant stock.

- [ ] **Step 6: Commit the widget**

```powershell
git add -- Source/GameXXK/Public/UI/GameXXKMetaShopWidget.h Source/GameXXK/Private/UI/GameXXKMetaShopWidget.cpp Source/GameXXK/Private/Tests/GameXXKMetaShopWidgetTest.cpp
git commit -m "feat: add UI V2 meta shop window"
```

## Task 8: Route the town merchant exclusively to the new shop

**Files:**

- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h:1-340`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp:1-1700`
- Modify: `Source/GameXXK/Private/Town/GameXXKTownNpcActor.cpp:250-290`
- Modify: `Source/GameXXK/Private/Town/GameXXKTownNpcCharacter.cpp:245-285`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPFlowTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPPlayableShellTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayableRootWidgetTest.cpp`

- [ ] **Step 1: Change player-flow tests to the new contract before implementation**

Require controller ownership of `UGameXXKMetaShopWidget`, merchant interaction opening it, repeated interaction closing it, modal movement lock, town exit closing it, and this hard legacy assertion:

```cpp
TestFalse(TEXT("legacy merchant window is retired"), PlayerController->OpenMerchantTradeWindow());
TestFalse(TEXT("legacy inventory trade mode remains hidden"),
    PlayerController->GetInventoryWindowWidgetForTest()->GetWindowModeForTest() == EGameXXKInventoryWindowMode::MerchantTrade);
```

Remove old assertions that buy/sell healing powder through the merchant. Retain free inventory, equipment, consumable, and route merchant tests.

- [ ] **Step 2: Run tests and verify the old flow fails**

Run cold compile and the updated `GameXXK.MVP.UI`/player-flow tests.

Expected: failures show the merchant still opens `MerchantTrade` and no meta-shop widget is owned.

- [ ] **Step 3: Add controller ownership and opening methods**

Add:

```cpp
UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
bool OpenMetaShopWindow();

UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow", meta = (DeprecatedFunction, DeprecationMessage = "Use OpenMetaShopWindow."))
bool OpenMerchantTradeWindow();
```

`OpenMerchantTradeWindow` returns `false` without creating or showing legacy trade mode. `OpenMetaShopWindow` ensures the widget, sets the subsystem, closes conflicting modal windows, toggles visibility, and applies the same modal input lock contract used by inventory/companion windows.

- [ ] **Step 4: Switch both merchant actor implementations**

Replace only the merchant branch call:

```cpp
bLastInteractionSuccessful = PlayerController->OpenMetaShopWindow();
```

Do not change NPC transforms, sprite assets, collision, proximity range, quest paths, or follower behavior.

- [ ] **Step 5: Remove legacy commands from visible player routing**

Stop exposing `BuyHealingPowder`, sell, and `MerchantTrade` commands in town command construction. Keep `UGameXXKMVPRules::BuyItem/SellItem` and `FGameXXKEquipmentEconomyRules::PurchaseLegacyEquipmentForCompatibility` compiled solely for migration/compatibility tests.

- [ ] **Step 6: Run focused and MVP regressions**

Run cold pipeline, then `GameXXK.MetaShop`, `GameXXK.MVP.UI`, `GameXXK.MVP.PlayerFlow`, `GameXXK.MVP.FullFlow`, and `GameXXK.Route.Merchant` prefixes.

Expected: merchant `F` opens only the new shop; old trade mode cannot open; route merchant remains green.

- [ ] **Step 7: Commit player-flow replacement**

Stage only the controller, two NPC source files, command routing files actually changed, and corresponding tests.

```powershell
git commit -m "feat: replace legacy town merchant flow"
```

## Task 9: Add a real PIE/MCP meta-shop acceptance probe

**Files:**

- Create: `Content/Python/gamexxk_probe_meta_shop_window.py`
- Modify: `scripts/gamexxk_real_play_flow_mcp.py`
- Modify: `docs/production/2026-07-03-playable-entry-flow/05-implementation-log.md`
- Modify: `docs/production/2026-07-03-playable-entry-flow/06-test-results.md`

- [ ] **Step 1: Add a failing acceptance expectation**

Extend the real-flow report schema with `meta_shop` evidence requiring:

```json
{
  "merchant_interaction_opened_meta_shop": true,
  "product_count": 7,
  "legacy_trade_visible": false,
  "equipment_purchase_gold_delta": -100,
  "equipment_instance_delta": 1,
  "old_buy_item_command_visible": false
}
```

- [ ] **Step 2: Run the probe and verify red**

After saving dirty packages through UE MCP, run the existing real-flow harness with the new expectation.

Expected: report fails because the meta-shop probe is not yet wired.

- [ ] **Step 3: Implement focused editor introspection**

The probe must locate the local player controller, invoke the same merchant interaction path used by `F`, inspect the live `UGameXXKMetaShopWidget`, buy one affordable equipment pack through selection/request/confirm, and report before/after gold and warehouse counts. It must never mutate map or NPC assets.

- [ ] **Step 4: Run cold final verification**

Use the canonical sequence:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 160
python scripts/gamexxk_mvp_playtest.py --skip-build --test-timeout 600 --report Saved/HarnessReports/new-meta-shop-mvp.json
python scripts/gamexxk_real_play_flow_mcp.py --timeout 240 --report Saved/HarnessReports/new-meta-shop-real-flow.json
python scripts/harness_state_validator.py --require-units --json
git diff --check
```

Also run the `GameXXK.MetaShop` and `GameXXK.Route.Merchant` automation prefixes. Expected: cold UBT succeeds, all target tests pass, real flow reports `ok: true`, legacy trade stays hidden, and UE MCP `save_dirty_packages` ends with `dirty_after=[]`.

- [ ] **Step 5: Visually inspect the live 1920×1080 shop**

Capture a PIE screenshot and verify: seven product cards, correct weapon/companion icons, readable price and probability copy, no stretched paper/slots, clear disabled reason, purchase confirmation, result panel, and no old medicine shelf/sell button. This visual check is post-generation and does not use TDD.

- [ ] **Step 6: Record evidence and commit**

Append exact commands, report paths, test counts, screenshot path, and UE dirty-package result to the two production records. Stage only the new probe, harness change, records, and accepted screenshot if production records conventionally track it.

```powershell
git commit -m "test: verify new meta shop playable flow"
```

## Task 10: Final requirements audit

**Files:**

- Read: `docs/superpowers/specs/2026-08-05-new-meta-shop-design.md`
- Read: `docs/superpowers/plans/2026-08-05-new-meta-shop-implementation.md`
- Read: `docs/production/2026-07-03-playable-entry-flow/06-test-results.md`

- [ ] **Step 1: Check each completion condition against fresh evidence**

Confirm all six statements with file/test/report references:

1. Merchant `F` opens only `UGameXXKMetaShopWidget`.
2. The widget exposes exactly six equipment packs and one companion pack.
3. Equipment transactions obey price, set, level, slot, quality, warehouse, determinism, and atomicity rules.
4. Companion transactions obey price, uniqueness/protection, full-roster pending, non-refundable discard, determinism, and atomicity rules.
5. Legacy town buy/sell is unreachable while save compatibility remains valid.
6. Route merchant behavior and tests are unchanged.

- [ ] **Step 2: Run one final clean status check**

Run:

```powershell
git status --short
git log -10 --oneline
```

Distinguish pre-existing user files from this plan's files. Do not delete, stage, revert, or overwrite unrelated content.

- [ ] **Step 3: Hand off the verified result**

Report the meta-shop commit sequence, validation counts, real-flow report, live screenshot, and any explicit remaining UI polish items. Do not claim completion if cold build, target automation, route-merchant regression, or live merchant interaction evidence is missing.
