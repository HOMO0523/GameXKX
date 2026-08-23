# Inventory, Character View, Carry, and Locks Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make shared Backpack/Warehouse cells, thirteen per-character loadouts, center art, two-click placement/swapping, equipment replacement, and persistent Alt-locks behave as one atomic system.

**Architecture:** Extend the existing save-authoritative physical slot model and equipment economy facades instead of storing ownership in widgets. The Workbench keeps only a non-committing carry preview; every final move/swap/equip goes through candidate-copy rules and one subsystem commit. The embedded inventory binds all presentation to the selected owner ID.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, SaveGame USTRUCTs, UE Automation Tests, UE MCP PIE.

---

## Source specification

`docs/superpowers/specs/2026-08-24-shared-inventory-equipment-tools-chests-design.md`

### Task 1: RED — reproduce persistent lock and migration failures

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopInventoryRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp`

- [ ] **Step 1: Add persistent lock RED tests**

Create one equipment instance and one item stack, then wish for:

```cpp
TestTrue(TEXT("equipment locks by stable instance"),
	FGameXXKDesktopInventoryRules::SetEntryLocked(
		State, FGameXXKDesktopInventoryRules::MakeEquipmentEntry(InstanceId), true, &Error));
TestTrue(TEXT("item locks cover the whole stack"),
	FGameXXKDesktopInventoryRules::SetEntryLocked(
		State, FGameXXKDesktopInventoryRules::MakeItemEntry(ItemId), true, &Error));
```

Assert locks survive Backpack/Warehouse moves and save round-trip, stale IDs normalize away, valid IDs remain, and Include Warehouse defaults true.

- [ ] **Step 2: Add v24→v25 RED**

Load a version-24 fixture and assert target version 25, empty lock sets, Include Warehouse true, and byte-identical existing cells/loadouts.

- [ ] **Step 3: Run RED**

Run:

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.Data.DesktopInventory+GameXXK.MVP.SaveGame" --automation-report InventoryLocksRed --json
```

Expected: compile or assertions fail only for missing lock fields/rules and version-25 migration.

### Task 2: append persistent locks and the v25 migration boundary

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Modify: `Source/GameXXK/Private/GameXXKDesktopInventoryRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKDesktopInventoryRules.h`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopInventoryRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp`

- [ ] **Step 1: Add lock state**

Append to `FGameXXKDesktopInventoryState`:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
TSet<FName> LockedEquipmentInstanceIds;

UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
TSet<FName> LockedItemIds;

UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
bool bToolAutoFillIncludesWarehouse = true;
```

- [ ] **Step 2: Add lock rules**

Declare and implement:

```cpp
static bool IsEntryLocked(
	const FGameXXKRuntimeState& State,
	const FGameXXKDesktopInventoryEntryKey& Entry);
static bool SetEntryLocked(
	FGameXXKRuntimeState& InOutState,
	const FGameXXKDesktopInventoryEntryKey& Entry,
	bool bLocked,
	FString* OutError = nullptr);
```

Validate locked equipment exists exactly once in `EquipmentCollection`; validate locked item has a positive Backpack or Warehouse stack. Normalize removes only stale lock IDs and never unlocks a valid moved entry.

- [ ] **Step 3: Claim save version 25**

Append in `GameXXKSaveMigration.h`:

```cpp
static constexpr int32 EquipmentToolsAndChestWalletIntroducedSaveVersion = 25;
static constexpr int32 CurrentSaveVersion = 25;
```

For source versions below 25, initialize empty lock sets and `bToolAutoFillIncludesWarehouse=true`. Preserve every existing cell/loadout. Add round-trip tests and update all exact current-version assertions. Document that the later Ordered Formation redesign must use 26.

- [ ] **Step 4: Run GREEN and commit**

Run `GameXXK.Data.DesktopInventory+GameXXK.MVP.SaveGame`. Expected: all pass.

Commit:

```powershell
git commit -m "feat: persist inventory entry locks"
```

### Task 3: implement authoritative move/swap and equipment-cell exchange

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKDesktopInventoryRules.h`
- Modify: `Source/GameXXK/Private/GameXXKDesktopInventoryRules.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopInventoryRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentFacadeTest.cpp`

- [ ] **Step 1: Write and run move/swap/equipment RED**

Add same-container/cross-container occupied swaps, whole item stacks, equipment partition mirrors, stale rollback, Hero/Guard/NPC slot isolation, exact displaced-source return, wrong-slot, full, and route-lock cases. Run the focused tests and confirm causal failure.

- [ ] **Step 2: Add typed request**

```cpp
struct GAMEXXK_API FGameXXKDesktopInventoryMoveRequest
{
	EGameXXKDesktopItemContainer FromContainer = EGameXXKDesktopItemContainer::Backpack;
	int32 FromSlotIndex = INDEX_NONE;
	EGameXXKDesktopItemContainer ToContainer = EGameXXKDesktopItemContainer::Backpack;
	int32 ToSlotIndex = INDEX_NONE;
	bool bAllowSwap = true;
};
```

- [ ] **Step 3: Replace move-only logic with candidate swap**

`MoveOrSwap` normalizes a candidate, validates both indices, applies container partition changes for both entries, swaps physical keys, synchronizes material mirrors, validates, then commits. Same source/destination succeeds without mutation. It never merges or splits stacks.

- [ ] **Step 4: Add equipment-from-cell facade**

```cpp
bool EquipEquipmentFromDesktopCell(
	FName CharacterId,
	EGameXXKEquipmentSlot Slot,
	EGameXXKDesktopItemContainer SourceContainer,
	int32 SourceSlotIndex,
	FGameXXKEquipmentTransactionResult& OutResult);
```

On a candidate, validate source equipment and slot; remove its physical source projection; call the existing equipment economy `Equip`; project any displaced instance back into the exact source container/cell; normalize/validate; commit once. If there is no displaced instance, the source cell becomes empty.

- [ ] **Step 5: Run GREEN and commit**

Run DesktopInventory, Equipment Economy, Facade, and CharacterBackpackModel suites.

Commit:

```powershell
git commit -m "feat: atomically swap storage and equipment cells"
```

### Task 4: route every embedded target through the Workbench carry state

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKFinalInventoryWidgetTest.cpp`

- [ ] **Step 1: Write and run UI carry RED**

Assert the visible carried image is hit-test-invisible, embedded equipment delegates placement, Backpack/Warehouse/Tool occupied targets swap, Alt-lock takes priority, and failure keeps the carried entry. Run Parent/Carry/FinalInventory filters and capture failure.

- [ ] **Step 2: make the carried image non-blocking**

After construction:

```cpp
CarriedItemImage->SetVisibility(ESlateVisibility::HitTestInvisible);
CarriedItemImage->SetIsEnabled(false);
```

The image stays last in paint order but never owns mouse input.

- [ ] **Step 3: add explicit host destinations**

Expose:

```cpp
bool HasDesktopCarriedEntry() const;
bool HandleDesktopEquipmentSlotLeftClicked(EGameXXKEquipmentSlot Slot);
bool HandleDesktopSlotAltClicked(
	EGameXXKDesktopItemContainer Container,
	int32 SlotIndex);
```

In `HandleConfiguredSlotClicked`, embedded Backpack clicks continue to delegate; embedded Equipment clicks delegate whenever the host carries an entry or Alt is held. Without a carry, equipment click keeps normal select/detail behavior.

- [ ] **Step 4: support occupied destination swaps**

Backpack/Warehouse callbacks call `MoveOrSwap`. Tool callbacks swap the two reservation structs. Equipment callbacks call `EquipEquipmentFromDesktopCell`. Only successful rules clear carry state; failures retain the carried payload and show the returned reason.

- [ ] **Step 5: wire Alt-lock and overlays**

Use `FSlateApplication::Get().GetModifierKeys().IsAltDown()` only to choose the action; tests call an explicit seam. Add `T_MasterV2_CardLockedIcon` overlays to Backpack, Warehouse, Equipment, and Tool cells, with `HitTestInvisible` visibility.

- [ ] **Step 6: run GREEN and commit**

Run Workbench ItemCarry, ParentCloseStack, EmbeddedDeferredRefresh, and FinalInventory suites.

Commit:

```powershell
git commit -m "fix: complete two-click inventory placement"
```

### Task 5: bind center art and equipment presentation to the viewed owner

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKFinalInventoryWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Write and run owner-presentation RED**

For all thirteen owner IDs, assert configured owner, distinct six-slot snapshot, changing central resource/UV, no non-Hero legacy fallback, shared Backpack identity, and unchanged Formation/Travel. Run the CharacterRoster/FinalInventory filters and capture failure.

- [ ] **Step 2: add presentation resolver**

Implement:

```cpp
FGameXXKBattleAnimationClipDescriptor ResolveCentralCharacterIdleClip() const;
void RefreshCentralCharacterPresentation();
```

Hero uses `T_MasterV2_HeroFullBody`. Non-Hero resolves the owner's 2K Idle descriptor via `FGameXXKBattleAnimationPresentation`, loads the atlas, applies frame-zero UV, and bottom-centers the brush. Invalid/missing art clears the resource and opacity.

- [ ] **Step 3: remove cross-owner legacy fallback**

In `RefreshEquipmentSlots`, evaluate legacy top-level item mirrors only when `ResolveInventoryCharacterId()==HeroCharacterId()`. Companion/NPC empty slots use their empty labels and no icon.

- [ ] **Step 4: strengthen owner tests**

For 13 owner IDs assert:

```cpp
TestEqual(TEXT("embedded owner follows selection"),
	Embedded->GetConfiguredCharacterIdForTest(), OwnerId);
TestEqual(TEXT("snapshot belongs to the same owner"),
	Snapshot.CharacterId, OwnerId);
```

Assert center resource changes across Hero/Guard/YueBai and returns, equipment instance IDs never cross owners, shared Backpack slot keys stay identical, and Formation/Travel state is byte-identical.

- [ ] **Step 5: run GREEN and commit**

Run CharacterBackpackModel, CharacterBackpackTabs, FinalInventoryWidget, PartnerBackpackWidget, and Workbench CharacterRoster suites.

Commit:

```powershell
git commit -m "fix: bind backpack presentation to each character owner"
```

### Task 6: full inventory work package verification

- [ ] **Step 1: save and cold-build**

If PIE is running, save dirty packages through UE MCP, stop PIE, close the editor gracefully, then run cold `GameXXKEditor Win64 Development -NoHotReload -NoHotReloadFromIDE`.

- [ ] **Step 2: focused automation**

Run:

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.Data.DesktopInventory+GameXXK.MVP.UI.CharacterBackpackModel+GameXXK.MVP.Inventory+GameXXK.DesktopTraining.Workbench" --automation-report InventoryCharacterCarryFinal --json
```

- [ ] **Step 3: independent spec and quality reviews**

Review only this plan's commits. Fix every Critical/Important issue and re-review. Close Minor false-positive test gaps before handoff.

- [ ] **Step 4: real PIE checkpoint**

On `L_DesktopTrainingHUD`, visibly switch Hero, Guard, and YueBai; move/swap Backpack/Warehouse cells; replace one equipped item; lock/unlock one item; verify Formation remains unchanged. Keep later work packages pending rather than claiming the whole feature complete.
