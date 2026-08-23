# Training Chest Wallet, Loot, and Strip Icons Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace inventory chest stacks with deterministic source-level chest tokens, open one/all transactions, random equipment/item loot, and two vertical Q-version chest buttons beside the always-on Training strip.

**Architecture:** Training rewards append save-authoritative chest tokens. A dedicated pure Chest Rules unit owns deterministic loot selection and atomic Backpack capacity checks; the Workbench only renders counts and dispatches left/right actions. Existing online/offline reward authorities supply stage and player level when creating tokens.

**Tech Stack:** Unreal Engine 5.8 C++, Training SaveGame state, equipment/gem rules, deterministic random streams, UMG/Slate, UE MCP texture import, UE Automation Tests.

---

## Source specification

`docs/superpowers/specs/2026-08-24-shared-inventory-equipment-tools-chests-design.md`

Prerequisites: both prior 2026-08-24 inventory and tools plans are complete.

### Task 1: finalize and import the two chest icons

**Files:**
- Input: `SourceArt/UI/Items/Chests/final/T_Item_TrainingNormalChest_v3_candidate.png`
- Input: `SourceArt/UI/Items/Chests/final/T_Item_TrainingAdvancedChest_v3_candidate.png`
- Create: `SourceArt/UI/Items/Chests/final/T_Item_TrainingNormalChest_v3.png`
- Create: `SourceArt/UI/Items/Chests/final/T_Item_TrainingAdvancedChest_v3.png`
- Import: `Content/GameXXK/UI/Items/T_Item_TrainingNormalChest.uasset`
- Import: `Content/GameXXK/UI/Items/T_Item_TrainingAdvancedChest.uasset`

- [ ] **Step 1: background-extract with the image generation skill**

Use each v3 candidate as the edit target. Prompt: change only the baked checkerboard into real transparent alpha; preserve the Q-version equipment-icon chest pixels, outline, colors, scale, and composition. Do not use Python for image editing.

- [ ] **Step 2: verify and normalize art deterministically**

Require real alpha pixels and no checkerboard-colored opaque corners. Center each approved cutout on a 512×512 transparent canvas at equal occupied-height scale using the project's non-generative asset processing path. Record width, height, RGBA format, transparent-pixel count, and SHA256. If background extraction is still RGB-only, stop and report the art blocker rather than importing fake transparency.

- [ ] **Step 3: import through UE MCP**

Import to the exact object paths above. Match existing equipment icon settings: UI texture group, no mips, sRGB, clamp, non-streaming, alpha preserved. Save packages and verify paths/settings twice.

- [ ] **Step 4: commit art**

```powershell
git commit -m "art: add normal and advanced training chest icons"
```

### Task 2: RED — chest tokens and migration

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKTrainingRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp`

- [ ] **Step 1: add token/migration RED**

Wish for:

```cpp
FGameXXKTrainingChestToken Token;
Token.Tier = EGameXXKTrainingRewardTier::NormalChest;
Token.SourceStageId = TEXT("Training.Normal.1-1");
Token.SourceItemLevel = 17;
Token.AcquisitionOrdinal = 1;
```

Assert stable sort/order, derived counts, level clamp, unique positive ordinals, and v24 conversion of `Item.TrainingNormalChest`/`AdvancedChest` quantities into tokens while removing inventory entries and physical cells.

- [ ] **Step 2: run RED**

Expected: missing token state, validation, and migration behavior.

### Task 3: add chest wallet state and v25 migration

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKTrainingRules.h`
- Modify: `Source/GameXXK/Private/GameXXKTrainingRules.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.h`
- Modify: Training/Save tests.

- [ ] **Step 1: append token state**

```cpp
USTRUCT(BlueprintType)
struct FGameXXKTrainingChestToken
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKTrainingRewardTier Tier = EGameXXKTrainingRewardTier::None;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName SourceStageId = NAME_None;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 SourceItemLevel = 1;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 AcquisitionOrdinal = 0;
};
```

Append to Training Progress:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
TArray<FGameXXKTrainingChestToken> OwnedChestTokens;
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
int32 NextChestAcquisitionOrdinal = 0;
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
int32 NextChestOpenOrdinal = 0;
```

- [ ] **Step 2: validate and derive counts**

Add pure helpers to append one token, count by tier, and validate strict acquisition order. Item level clamps 1–100; only Normal/Advanced tiers are valid.

- [ ] **Step 3: extend version-25 migration**

The inventory/tools plan already raised current version to 25. Extend that migration and add an idempotent current-v25 reconciliation: whenever legacy chest stack counts still exist, append Normal then Advanced tokens with saved Current Travel stage and clamped Player Level, remove both item maps/locks/physical entries, and normalize. A current v25 state with neither tokens nor legacy chest items receives empty defaults; a v25 state already converted remains byte-identical.

- [ ] **Step 4: run GREEN and commit**

Run Training migration and SaveGame round-trip.

Commit:

```powershell
git commit -m "feat: persist source-level training chest tokens"
```

### Task 4: implement deterministic atomic Chest Rules

**Files:**
- Create: `Source/GameXXK/Public/GameXXKTrainingChestRules.h`
- Create: `Source/GameXXK/Private/GameXXKTrainingChestRules.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTrainingChestRulesTest.cpp`

- [ ] **Step 1: Write and run loot/open RED**

For controlled seeds, cover both 50/50 branches, all six equipment slots/modern sets, all five item outcomes per tier, Normal/Common and Advanced/Rare quality, material quantities 1/3, and source item level. Assert one open, tier-specific open-all, token order, stack reuse, equipment physical placement, full Backpack stop-before-consume, stale/overflow rollback, and reload preserving the blocked next outcome.

- [ ] **Step 2: add result types**

```cpp
UENUM(BlueprintType)
enum class EGameXXKTrainingChestOpenError : uint8
{
	None, NoChest, BackpackFull, InvalidToken, LootInvalid, Overflow
};

USTRUCT(BlueprintType)
struct FGameXXKTrainingChestOpenResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bSucceeded = false;
	UPROPERTY(BlueprintReadOnly) EGameXXKTrainingChestOpenError Error = EGameXXKTrainingChestOpenError::None;
	UPROPERTY(BlueprintReadOnly) int32 OpenedCount = 0;
	UPROPERTY(BlueprintReadOnly) TArray<FName> EquipmentInstanceIds;
	UPROPERTY(BlueprintReadOnly) TMap<FName,int32> ItemDeltas;
	UPROPERTY(BlueprintReadOnly) FText Message;
};
```

- [ ] **Step 3: add pure roll**

Seed one `FRandomStream` from Training reward seed, token acquisition ordinal, next open ordinal, tier, and stage ID. First roll is equipment/item at equal 50%. Equipment selects one of six modern non-Starter sets and one of six slots, uses token level, and fixed tier quality. Item branch selects one of five equal outcomes.

- [ ] **Step 4: implement one-token candidate commit**

On a copied runtime, resolve the oldest requested-tier token and predetermined loot. Equipment creation must have one free Backpack cell after normalize; it remains an unequipped collection instance and is not marked Warehouse-partitioned. Item output uses `AddItem` and an existing or free physical item cell. Only after normalize/validate succeeds remove the token and increment open ordinal.

- [ ] **Step 5: implement bounded open-all**

Capture the starting count as the hard loop bound. Repeatedly call the internal candidate step on one outer candidate; stop on first `BackpackFull` without consuming that token. Any structural error rolls back the entire public call; ordinary capacity stop commits already opened tokens and reports the retained count.

- [ ] **Step 6: add subsystem facades**

```cpp
UFUNCTION(BlueprintCallable, Category="GameXXK|Training")
bool OpenOneTrainingChest(EGameXXKTrainingRewardTier Tier, FGameXXKTrainingChestOpenResult& OutResult);
UFUNCTION(BlueprintCallable, Category="GameXXK|Training")
bool OpenAllTrainingChests(EGameXXKTrainingRewardTier Tier, FGameXXKTrainingChestOpenResult& OutResult);
```

- [ ] **Step 7: run GREEN and commit**

Run Chest Rules, DesktopInventory, Equipment Rules, Gem Rules, SaveGame.

Commit:

```powershell
git commit -m "feat: open training chests into backpack loot"
```

### Task 5: route online/offline chest rewards into tokens

**Files:**
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/GameXXKTrainingRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTrainingRulesTest.cpp`
- Modify: offline/Workbench reward tests.

- [ ] **Step 1: Write and run reward-bridge RED**

Assert online Challenge/Travel and offline collection append exact tier/source-stage/source-level tokens, never visible item stacks; collection remains atomic with gold/experience and legacy IDs stay hidden.

- [ ] **Step 2: replace `AddItem(chest)` bridge**

When an online Challenge/Travel reward rolls a chest, append a token using the active stage ID and `Clamp(PlayerLevel,1,100)`. Do not add legacy chest item IDs.

- [ ] **Step 3: replace offline count collection**

At offline collection, validate counts, append exactly that many tier tokens using saved Current Travel stage and collection-time clamped Player Level, then clear pending counts with gold/experience in the existing atomic collection candidate.

- [ ] **Step 4: retain compatibility IDs only for migration**

Legacy chest `GetItemDef` entries remain hidden migration sentinels and are excluded from known visible inventory IDs, shop pools, lock targets, and item icon routing.

- [ ] **Step 5: run GREEN and commit**

Commit:

```powershell
git commit -m "fix: route training chest rewards into chest wallet"
```

### Task 6: build vertical strip chest buttons

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`
- Modify: `Content/Python/gamexxk_probe_training_visual_mvp.py`
- Modify: relevant script predicate tests.

- [ ] **Step 1: Write and run strip-button RED**

Assert two textures/counts, attached rail geometry, collapsed/expanded persistence, left-one, real right-all tier isolation, zero-count disable, capacity notice, and owner/session preservation.

- [ ] **Step 2: add two action IDs**

Reserve stable IDs outside existing ranges, for example Normal `600`, Advanced `601`. Left `ApplyAction` calls Open One. `HandleActionRightClicked` maps the same IDs to Open All and consumes right click without firing left click.

- [ ] **Step 3: build icons beside the strip**

Reserve the rightmost 68 reference pixels of `GetIdleStripRect()` as a chest rail and reduce the clipped Travel lane's usable width by the same amount. Create two `UGameXXKDesktopTrainingActionButton`s in one vertical column inside that attached rail, clear of characters, health bars, top toolbar, and every expanded panel. Use the imported normal/advanced textures, a lower-right count badge, and tooltips:

```text
普通历练宝箱 ×N\n左键开启1个；右键开启全部
高级历练宝箱 ×N\n左键开启1个；右键开启全部
```

Buttons remain visible in collapsed and expanded Workbench states. Disable a zero-count button.

- [ ] **Step 4: surface exact results**

After open, show `OpenedCount`, equipment names, item totals, or capacity-stop reason in the Workbench notice. Refresh Backpack slots only after commit; do not reset current owner or page.

- [ ] **Step 5: complete causal UI tests**

Broadcast left click and invoke real right mouse on each button. Assert tier isolation, one/all count changes, disabled zero state, no legacy chest inventory entries, capacity stop, geometry non-overlap, texture paths, count labels, collapsed persistence, and current owner/session preservation.

- [ ] **Step 6: update live probe**

Expose owned Normal/Advanced counts, icon paths, and last open result in `gamexxk_probe_training_visual_mvp.py`. Predicate tests fail closed on missing fields.

- [ ] **Step 7: run GREEN and commit**

Commit:

```powershell
git commit -m "feat: open training chests from the idle strip"
```

### Task 7: full chest and complete-feature handoff

- [ ] **Step 1: cold UBT and script checks**

Run `git diff --check`, Python acceptance/predicate tests, cold UHT/UBT, no Hot Reload.

- [ ] **Step 2: focused automation**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Training+GameXXK.Data.DesktopInventory+GameXXK.Equipment+GameXXK.DesktopTraining.Workbench+GameXXK.MVP.SaveGame" --automation-report InventoryToolsChestsFinal --json
```

- [ ] **Step 3: independent reviews**

Run spec, quality, then cross-work-package integration reviews. Fix/re-review every issue.

- [ ] **Step 4: direct visual and gameplay PIE**

Without Luna, verify both Q-version chest icons/counts, one/all opening, full-cap stop, Hero/Guard/YueBai Backpack separation, left-click swaps, lock overlays, every Tool mode, ten qualities, Tool XP/crafting level, multi-socket replacement, and chest loot. Leave `/Game/GameXXK/Maps/L_DesktopTrainingHUD` PIE running for the user.
