# Warehouse Page Batch Transfer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace warehouse previous/next/sort controls with six page tabs and two deterministic current-page batch-transfer buttons that preserve locks and partially succeed when capacity is exhausted.

**Architecture:** Add one transactional batch-transfer API to `FGameXXKDesktopInventoryRules`; the workbench supplies transient exclusions for carried/tool-reserved entries and never edits runtime arrays directly. The warehouse UI keeps six page tabs, calls the rules API through the authoritative runtime state, and refreshes the current page in place.

**Tech Stack:** Unreal Engine 5.8 C++, UMG, existing desktop inventory slot model, UE Automation Tests.

**Project constraint:** Work on root `main`; do not create a worktree and do not use UnrealBridge.

---

## File map

- `Source/GameXXK/Public/GameXXKDesktopInventoryRules.h`: request/result types and public batch API.
- `Source/GameXXK/Private/GameXXKDesktopInventoryRules.cpp`: candidate collection, whole-stack merge, exact-page placement, partial completion, validation.
- `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`: batch button pointers and test facade.
- `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`: page tabs, two buttons, transient exclusions, notices, refresh.
- `Source/GameXXK/Private/Tests/GameXXKDesktopInventoryBatchTransferTest.cpp`: pure rule coverage.
- `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`: UI resource/behavior and live refresh coverage.

### Task 1: Define batch-transfer behavior with failing rule tests

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKDesktopInventoryBatchTransferTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKDesktopInventoryRules.h`

- [ ] **Step 1: Declare exact request and result types**

Add before `FGameXXKDesktopInventoryRules`:

```cpp
struct GAMEXXK_API FGameXXKDesktopInventoryBatchTransferRequest
{
	EGameXXKDesktopItemContainer FromContainer = EGameXXKDesktopItemContainer::Backpack;
	EGameXXKDesktopItemContainer ToContainer = EGameXXKDesktopItemContainer::Warehouse;
	int32 WarehousePageIndex = 0;
	int32 WarehousePageSize = 36;
	TSet<FGameXXKDesktopInventoryEntryKey> ExcludedEntries;
};

struct GAMEXXK_API FGameXXKDesktopInventoryBatchTransferResult
{
	bool bSucceeded = false;
	bool bDestinationFull = false;
	int32 MovedEntryCount = 0;
	FString Error;
};
```

`MovedEntryCount` counts source entries: one equipment instance or one whole stack equals one moved entry, regardless of stack quantity.

Declare:

```cpp
static bool CanBatchTransferCurrentWarehousePage(
	const FGameXXKRuntimeState& State,
	const FGameXXKDesktopInventoryBatchTransferRequest& Request);
static bool BatchTransferCurrentWarehousePage(
	FGameXXKRuntimeState& InOutState,
	const FGameXXKDesktopInventoryBatchTransferRequest& Request,
	FGameXXKDesktopInventoryBatchTransferResult& OutResult);
```

- [ ] **Step 2: Create a fixture with page-two capacity**

The test fixture must start a new game, set `Talent.Root=1` to unlock warehouse page two, and normalize. Add equipment and item stacks through existing creation helpers, move selected entries to exact warehouse page cells, and preserve a locked equipment/item ID.

- [ ] **Step 3: Add complete rule cases**

Implement separate automation sections that assert:

```cpp
TestTrue(TEXT("backpack to current warehouse page succeeds"),
	FGameXXKDesktopInventoryRules::BatchTransferCurrentWarehousePage(State, Request, Result));
TestEqual(TEXT("whole stacks count as one moved entry"), Result.MovedEntryCount, ExpectedSourceEntries);
TestTrue(TEXT("locked equipment remains locked after transfer"),
	FGameXXKDesktopInventoryRules::IsEntryLocked(State, LockedEquipment));
TestTrue(TEXT("locked item remains locked after transfer"),
	FGameXXKDesktopInventoryRules::IsEntryLocked(State, LockedItem));
TestTrue(TEXT("excluded tool reservation stays in source"),
	FGameXXKDesktopInventoryRules::FindEntrySlot(State, Request.FromContainer, ExcludedEntry) != INDEX_NONE);
TestTrue(TEXT("only the selected warehouse page supplies warehouse-to-backpack candidates"),
	FGameXXKDesktopInventoryRules::FindEntrySlot(State, EGameXXKDesktopItemContainer::Warehouse, OtherPageEntry) != INDEX_NONE);
TestTrue(TEXT("full destination reports partial success"), Result.bDestinationFull && Result.MovedEntryCount > 0);
```

Also assert: source order is preserved at newly allocated target slots; an existing destination stack receives the full source quantity; no current-page change is stored in runtime state; the original state remains byte-identical when the request itself is invalid.

- [ ] **Step 4: Compile and run RED**

Run the cold pipeline and `Automation RunTests GameXXK.DesktopInventory.BatchTransfer`.

Expected: the new test fails to link or reports missing batch-transfer implementation.

- [ ] **Step 5: Commit the failing rule test and API**

```powershell
git add Source/GameXXK/Public/GameXXKDesktopInventoryRules.h Source/GameXXK/Private/Tests/GameXXKDesktopInventoryBatchTransferTest.cpp
git commit -m "test: define warehouse page batch transfer"
```

### Task 2: Implement deterministic transactional batch transfer

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKDesktopInventoryRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKDesktopInventoryRules.h`

- [ ] **Step 1: Add page-range and candidate helpers**

Use these exact bounds:

```cpp
bool ResolveWarehousePageRange(
	const FGameXXKRuntimeState& State,
	const int32 PageIndex,
	const int32 PageSize,
	int32& OutFirst,
	int32& OutLastExclusive)
{
	if (PageIndex < 0 || PageSize <= 0) return false;
	const int32 Capacity = LogicalCapacityFor(State, EGameXXKDesktopItemContainer::Warehouse);
	OutFirst = PageIndex * PageSize;
	OutLastExclusive = FMath::Min(OutFirst + PageSize, Capacity);
	return OutFirst < OutLastExclusive;
}
```

For warehouse source or destination, iterate only `[First, LastExclusive)`. For backpack source/destination, iterate `[0, LogicalCapacityFor(...))`.

- [ ] **Step 2: Add whole-stack merge without allocating another slot**

Implement:

```cpp
bool MergeItemStackAcrossContainers(
	FGameXXKRuntimeState& State,
	const FGameXXKDesktopInventoryEntryKey& Entry,
	const EGameXXKDesktopItemContainer From,
	const EGameXXKDesktopItemContainer To,
	FString* OutError)
{
	if (!Entry.IsValid() || Entry.bEquipmentInstance) return false;
	auto ItemStacksFor = [&State](const EGameXXKDesktopItemContainer Container) -> TMap<FName, int32>&
	{
		return Container == EGameXXKDesktopItemContainer::Warehouse
			? State.DesktopInventory.WarehouseItems
			: State.Inventory;
	};
	TMap<FName, int32>& Source = ItemStacksFor(From);
	TMap<FName, int32>& Destination = ItemStacksFor(To);
	const int32 SourceQuantity = Source.FindRef(Entry.EntryId);
	const int32 DestinationQuantity = Destination.FindRef(Entry.EntryId);
	if (SourceQuantity <= 0 || DestinationQuantity > MAX_int32 - SourceQuantity)
	{
		SetError(OutError, TEXT("Desktop inventory stack merge is invalid or overflows."));
		return false;
	}
	Source.Remove(Entry.EntryId);
	Destination.Add(Entry.EntryId, DestinationQuantity + SourceQuantity);
	return true;
}
```

An existing target-container stack may reside outside the current warehouse page; merging into it is allowed because the authoritative model stores one stack per item ID. The current-page restriction controls warehouse source candidates and allocation of new warehouse slots.

- [ ] **Step 3: Implement candidate-by-candidate transfer on one candidate state**

Algorithm:

1. Normalize a copy of `InOutState`.
2. Validate containers are opposite and page range is valid.
3. Snapshot source entries in physical source-slot order.
4. Skip invalid or excluded entries.
5. For an item whose target-container stack exists, merge it and clear the exact source slot.
6. Otherwise find the first empty target slot in the allowed target range and call `MoveEntry` with swapping disabled.
7. If no target slot exists, set `bDestinationFull=true` and stop.
8. Normalize and validate once more; commit the candidate only if final validation succeeds.
9. Return `bSucceeded=true` even when zero entries moved due solely to no candidates; UI uses `CanBatchTransferCurrentWarehousePage` to disable that case.

The implementation must never test `IsEntryLocked`; locks transfer with the ID sets unchanged.

- [ ] **Step 4: Implement the pure enablement query**

`CanBatchTransferCurrentWarehousePage` returns true only when at least one non-excluded source entry can either merge into an existing target stack or occupy an allowed empty target slot.

- [ ] **Step 5: Run rule and existing inventory tests**

Run:

```text
GameXXK.DesktopInventory.BatchTransfer
GameXXK.Talents.Capacity
GameXXK.DesktopTraining.Workbench.ItemCarryStateMachine
GameXXK.MVP.UI.FinalInventory
```

Expected: all succeed and locks remain preserved.

- [ ] **Step 6: Commit batch rules**

```powershell
git add Source/GameXXK/Public/GameXXKDesktopInventoryRules.h Source/GameXXK/Private/GameXXKDesktopInventoryRules.cpp Source/GameXXK/Private/Tests/GameXXKDesktopInventoryBatchTransferTest.cpp
git commit -m "feat: add current-page warehouse batch transfer"
```

### Task 3: Replace warehouse controls with tabs and two batch buttons

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Add failing warehouse control assertions**

Open Backpack and Warehouse in a workbench fixture, then assert:

```cpp
TestNull(TEXT("warehouse has no previous button"), Widget->WidgetTree->FindWidget(TEXT("WarehousePreviousButton")));
TestNull(TEXT("warehouse has no next button"), Widget->WidgetTree->FindWidget(TEXT("WarehouseNextButton")));
TestNull(TEXT("warehouse has no sort button"), Widget->WidgetTree->FindWidget(TEXT("WarehouseSortButton")));
TestNotNull(TEXT("warehouse-to-backpack batch button exists"),
	Widget->WidgetTree->FindWidget(TEXT("WarehouseBatchToBackpackButton")));
TestNotNull(TEXT("backpack-to-warehouse batch button exists"),
	Widget->WidgetTree->FindWidget(TEXT("BackpackBatchToWarehouseButton")));
for (int32 Page = 0; Page < 6; ++Page)
{
	TestNotNull(TEXT("all six page tabs exist"),
		Widget->WidgetTree->FindWidget(*FString::Printf(TEXT("WarehousePageTab_%d"), Page)));
}
```

Run the individual test and expect failure against current previous/next/sort controls.

- [ ] **Step 2: Replace pointer fields and action IDs**

Remove `WarehousePreviousButton` and `WarehouseNextButton`. Add:

```cpp
UPROPERTY(Transient)
TObjectPtr<UGameXXKDesktopTrainingActionButton> WarehouseBatchToBackpackButton;
UPROPERTY(Transient)
TObjectPtr<UGameXXKDesktopTrainingActionButton> BackpackBatchToWarehouseButton;
```

Declare `TSet<FGameXXKDesktopInventoryEntryKey> BuildBatchTransferExclusions() const;` with the private workbench helpers.

Reuse action IDs 40 and 41 for the two batch directions. Keep 70–75 for page tabs. Keep `PreviousWarehousePageForTest` and `NextWarehousePageForTest` only as non-visual test helpers if existing tests still call them.

- [ ] **Step 3: Build exactly six page tabs and exactly two footer buttons**

Use current tab textures. At the footer use `CharacterTabNormalTexturePath` with two `145×42` buttons:

```cpp
WarehouseBatchToBackpackButton->Configure(this, 40);
WarehouseBatchToBackpackButton->SetContent(
	MakeButtonText(WidgetTree, FText::FromString(TEXT("仓库 → 背包")), 14, Ink));
AddCanvas(RootCanvas, WarehouseBatchToBackpackButton.Get(), FVector2D(30.0f, 842.0f), FVector2D(145.0f, 42.0f));

BackpackBatchToWarehouseButton->Configure(this, 41);
BackpackBatchToWarehouseButton->SetContent(
	MakeButtonText(WidgetTree, FText::FromString(TEXT("背包 → 仓库")), 14, Ink));
AddCanvas(RootCanvas, BackpackBatchToWarehouseButton.Get(), FVector2D(195.0f, 842.0f), FVector2D(145.0f, 42.0f));
```

Do not create a warehouse sort button.

- [ ] **Step 4: Build transient exclusions and call the authoritative rule**

Add a helper:

```cpp
TSet<FGameXXKDesktopInventoryEntryKey> UGameXXKDesktopTrainingWorkbenchWidget::BuildBatchTransferExclusions() const
{
	TSet<FGameXXKDesktopInventoryEntryKey> Result;
	if (CarriedEntry.IsValid()) Result.Add(CarriedEntry.Payload.Entry);
	for (const FDesktopToolEntry& ToolEntry : ToolSlots)
	{
		if (ToolEntry.IsValid()) Result.Add(ToolEntry.Entry);
	}
	return Result;
}
```

Action 40 uses Warehouse→Backpack; action 41 uses Backpack→Warehouse. Both requests pass `WarehousePageIndex`, `WarehousePageSize`, and the exclusion set.

After success, call `RefreshLayout()` and set exactly one notice:

```cpp
const FString Message = Result.bDestinationFull
	? FString::Printf(TEXT("已移动 %d 件，目标空间不足"), Result.MovedEntryCount)
	: FString::Printf(TEXT("已移动 %d 件"), Result.MovedEntryCount);
SetNotice(FText::FromString(Message));
```

- [ ] **Step 5: Drive enablement from `CanBatchTransferCurrentWarehousePage`**

Build both direction requests with the current exclusions and set each button enabled from the pure query. Re-evaluate after every inventory/talent/tool reservation refresh.

- [ ] **Step 6: Run the warehouse UI test and commit**

```powershell
git add Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp
git commit -m "feat: replace warehouse paging controls with batch transfer"
```

### Task 4: Verify partial success, locks, reservations, and live refresh through UI

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Add a two-direction integration test**

Create `GameXXK.DesktopTraining.Workbench.WarehouseBatchTransfer` that:

1. Unlocks root/page two.
2. Places locked and unlocked entries in Backpack and current warehouse page.
3. Reserves one entry in a Tool slot and begins carrying another.
4. Clicks Backpack→Warehouse.
5. Verifies locked entries moved and stayed locked; carried/reserved entries did not move.
6. Clicks Warehouse→Backpack.
7. Verifies only current-page entries moved and current page index stayed unchanged.

- [ ] **Step 2: Add a capacity-full UI case**

Fill all but one target slot, provide three candidates, click the direction button, and assert the notice contains both `已移动 1 件` and `目标空间不足`.

- [ ] **Step 3: Add live button enablement assertions**

After transfer, assert the exhausted direction button becomes disabled without closing Warehouse, while the reverse direction becomes enabled.

- [ ] **Step 4: Run focused workbench tests**

Run:

```text
GameXXK.DesktopTraining.Workbench.WarehouseBatchTransfer
GameXXK.DesktopTraining.Workbench.ItemCarryToolReservationAuthority
GameXXK.DesktopTraining.Workbench.LiveNumericRefreshWithoutRebuild
GameXXK.MVP.UI.FinalInventory.PersistentLockOverlays
```

Expected: all succeed.

- [ ] **Step 5: Commit integration coverage**

```powershell
git add Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp
git commit -m "test: cover warehouse batch transfer interactions"
```

### Task 5: Full regression and canonical PIE acceptance

**Files:**
- Modify only when a regression is proven: scoped files from Tasks 1–4
- Save evidence under: `Saved/HarnessReports/WarehouseBatchTransfer-*`

- [ ] **Step 1: Run a cold compile**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --filter "[TDD]" --log-lines 200
```

Expected: `Result: Succeeded` without Live Coding.

- [ ] **Step 2: Run final suites**

Require success for:

```text
GameXXK.DesktopInventory.BatchTransfer
GameXXK.DesktopTraining.Workbench
GameXXK.MVP.UI.FinalInventory
GameXXK.Equipment.Tools
GameXXK.Talents.Capacity
```

- [ ] **Step 3: Perform PIE acceptance on the canonical map**

In `/Game/GameXXK/Maps/L_DesktopTrainingHUD`:

1. Open Backpack and Warehouse.
2. Confirm only page tabs 1–6 and two batch buttons appear below Warehouse.
3. Switch page and run Backpack→Warehouse; confirm new slots use the current page.
4. Run Warehouse→Backpack; confirm entries on other pages stay untouched.
5. Lock one item and repeat; confirm its lock overlay follows it.
6. Fill the destination and confirm partial-success notice.
7. Reserve one Tool entry and carry another; confirm both remain excluded.

- [ ] **Step 4: Save and commit final corrections**

Use UE MCP `save_dirty_packages`, require `dirty_after=[]`, then:

```powershell
git add Source/GameXXK/Public/GameXXKDesktopInventoryRules.h Source/GameXXK/Private/GameXXKDesktopInventoryRules.cpp Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp Source/GameXXK/Private/Tests/GameXXKDesktopInventoryBatchTransferTest.cpp Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp
git commit -m "feat: finish warehouse page batch transfer"
```
