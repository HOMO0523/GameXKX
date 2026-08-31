# Workbench Parent and Close Stack Implementation Plan

> **Control-placement override:** The expanded Backpack Tab no longer renders
> CloseInk. Use `2026-08-23-life-saving-charm-and-workbench-polish.md` for the
> selected/normal Tab skins and the separate Backpack-paper `X`.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep the backpack as a stable Workbench parent, stop occupied-slot callbacks from blanking it, and implement the approved local/global close semantics.

**Architecture:** Retain the current programmatic Workbench but make rebuild requests input-safe. Local panel closes mutate only their owning region; the Backpack `X` and `Tab` use one global-reset helper and reopen to a clean Backpack rather than restoring the old child combination.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, UE Automation Tests, UE MCP, cold UBT.

---

## File map

- Modify `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`: close helpers, test seams, and action constants if exposed.
- Modify `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`: guarded embedded callbacks, deferred rebuild flush, local close buttons, global reset.
- Modify `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`: occupied-slot, close-stack, and clean-reopen automation.
- Read-only verify `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`: embedded slot forwarding remains the source of real callbacks.

### Task 1: RED — occupied embedded slot must defer the parent rebuild

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Add the failing real-callback automation test**

Add beside `GameXXK.DesktopTraining.Workbench.ItemCarryStateMachine`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingEmbeddedBackpackDeferredRefreshTest,
	"GameXXK.DesktopTraining.Workbench.EmbeddedBackpackDeferredRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingEmbeddedBackpackDeferredRefreshTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestTrue(TEXT("fixture starts"), Subsystem && Subsystem->StartGame());
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	Widget->ConstructForTest();
	TestTrue(TEXT("backpack opens"), Widget->OpenBackpack());

	UGameXXKInventoryWindowWidget* Embedded = Widget->WidgetTree
		? Cast<UGameXXKInventoryWindowWidget>(Widget->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")))
		: nullptr;
	if (!TestNotNull(TEXT("embedded backpack exists"), Embedded))
	{
		return false;
	}
	const int32 SlotIndex = Widget->FindFirstBackpackEquipmentSlotForTest();
	if (!TestTrue(TEXT("occupied slot exists"), SlotIndex != INDEX_NONE))
	{
		return false;
	}
	const int32 BuildCountBefore = Widget->GetProgrammaticLayoutBuildCountForTest();
	Embedded->HandleConfiguredSlotClicked(EGameXXKInventorySlotSource::PlayerBackpack, SlotIndex, NAME_None);

	TestTrue(TEXT("occupied click keeps parent expanded"), Widget->IsBackpackExpandedForTest());
	TestTrue(TEXT("occupied click enters carry state"), Widget->IsCarryingItemForTest());
	TestEqual(TEXT("callback performs no synchronous parent rebuild"),
		Widget->GetProgrammaticLayoutBuildCountForTest(), BuildCountBefore);
	TestTrue(TEXT("callback leaves one pending refresh"), Widget->HasPendingLayoutRefreshForTest());

	Widget->TickForTest(0.0f);
	TestEqual(TEXT("next tick performs exactly one rebuild"),
		Widget->GetProgrammaticLayoutBuildCountForTest(), BuildCountBefore + 1);
	TestTrue(TEXT("rebuilt backpack remains expanded"), Widget->IsBackpackExpandedForTest());
	TestNotNull(TEXT("rebuilt embedded backpack remains present"),
		Widget->WidgetTree ? Widget->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")) : nullptr);
	return true;
}
```

- [ ] **Step 2: Cold-build the RED test**

Run:

```powershell
D:\UE_5.8\Engine\Build\BatchFiles\Build.bat GameXXKEditor Win64 Development -Project="D:\UE5 demo\GameXXK\GameXXK.uproject" -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2
```

Expected: build succeeds.

- [ ] **Step 3: Run RED and record the intended failure**

```powershell
D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe "D:\UE5 demo\GameXXK\GameXXK.uproject" -Unattended -NoSound -NullRHI -NoSplash -NoPause -ReportOutputPath="D:\UE5 demo\GameXXK\Saved\Automation\WorkbenchParentRed" -ExecCmds="Automation RunTests GameXXK.DesktopTraining.Workbench.EmbeddedBackpackDeferredRefresh; Quit"
python scripts/parse_automation_index.py --index Saved/Automation/WorkbenchParentRed/index.json --json
```

Expected: FAIL because the occupied slot synchronously increases the build count and leaves no pending refresh.

### Task 2: GREEN — guard embedded callbacks and flush without a World timer

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`

- [ ] **Step 1: Guard both embedded entry points**

Replace the two forwarding methods with:

```cpp
bool UGameXXKDesktopTrainingWorkbenchWidget::HandleDesktopBackpackSlotLeftClicked(const int32 SlotIndex)
{
	FScopedActionCallbackGuard CallbackGuard(bInActionCallback);
	return CarriedEntry.IsValid()
		? DropCarriedOnDesktopSlot(EGameXXKDesktopItemContainer::Backpack, SlotIndex)
		: PickUpDesktopEntry(EGameXXKDesktopItemContainer::Backpack, SlotIndex);
}

bool UGameXXKDesktopTrainingWorkbenchWidget::HandleDesktopBackpackSlotRightClicked(const int32 SlotIndex)
{
	FScopedActionCallbackGuard CallbackGuard(bInActionCallback);
	if (CarriedEntry.IsValid())
	{
		const bool bCancelled = CancelCarriedItem();
		if (bCancelled)
		{
			RefreshLayout();
		}
		return bCancelled;
	}
	return RouteBackpackRightClick(SlotIndex);
}
```

- [ ] **Step 2: Make guarded headless refresh remain deferred**

In `RefreshLayout`, remove the no-World synchronous fallback:

```cpp
		else
		{
			bLayoutRebuildScheduled = false;
			// Headless automation has no World timer. Keep the request pending;
			// NativeTick flushes it after the originating callback returns.
		}
		return;
```

At the start of `NativeTick`, before constructing `NativeTickGuard`, add:

```cpp
	if (bLayoutRefreshPending && !bLayoutRebuildScheduled && !bInActionCallback)
	{
		RebuildLayoutNow();
	}
```

- [ ] **Step 3: Run GREEN**

Rebuild cold, then run the focused test into `Saved/Automation/WorkbenchParentGreen`.

Expected: 1/1 passed, zero errors.

- [ ] **Step 4: Commit the callback fix only**

```powershell
git add -- Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp
git commit -m "fix: defer embedded backpack parent refresh"
```

Do not stage unrelated hunks if these files already contain user work; leave the implementation uncommitted and record the overlap instead.

### Task 3: RED — local close relationships and clean global reopen

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Add the close-stack test**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchCloseStackTest,
	"GameXXK.DesktopTraining.Workbench.ParentCloseStack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchCloseStackTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestTrue(TEXT("fixture starts"), Subsystem && Subsystem->StartGame());
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("backpack opens"), Widget->OpenBackpack());

	Widget->HandleActionClicked(0); // Warehouse.
	Widget->HandleActionClicked(3); // Tools.
	Widget->HandleActionClicked(2); // Talents in the center.
	TestTrue(TEXT("warehouse is open"), Widget->IsWarehousePanelOpenForTest());
	TestTrue(TEXT("tools are open"), Widget->IsToolsPanelActiveForTest());
	TestEqual(TEXT("talents own the center"), Widget->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Talents);

	Widget->HandleActionClicked(63); // Central close.
	TestEqual(TEXT("central close returns to backpack"), Widget->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Backpack);
	TestTrue(TEXT("central close preserves warehouse"), Widget->IsWarehousePanelOpenForTest());
	TestTrue(TEXT("central close preserves tools"), Widget->IsToolsPanelActiveForTest());

	Widget->HandleActionClicked(62); // Warehouse close.
	TestFalse(TEXT("warehouse close affects only warehouse"), Widget->IsWarehousePanelOpenForTest());
	TestTrue(TEXT("warehouse close preserves tools"), Widget->IsToolsPanelActiveForTest());

	Widget->HandleActionClicked(64); // Right-panel close.
	TestFalse(TEXT("right close closes tools"), Widget->IsRightPanelOpenForTest());

	Widget->HandleActionClicked(0);
	Widget->HandleActionClicked(4); // Training right panel.
	Widget->HandleActionClicked(1); // Formation center.
	Widget->HandleActionClicked(60); // Global Backpack/Tab close.
	TestFalse(TEXT("global close collapses backpack"), Widget->IsBackpackExpandedForTest());
	TestFalse(TEXT("global close closes warehouse"), Widget->IsWarehousePanelOpenForTest());
	TestFalse(TEXT("global close closes right rail"), Widget->IsRightPanelOpenForTest());

	TestTrue(TEXT("Tab reopens"), Widget->OpenBackpack());
	TestEqual(TEXT("reopen starts on clean backpack"), Widget->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Backpack);
	TestFalse(TEXT("reopen does not restore warehouse"), Widget->IsWarehousePanelOpenForTest());
	TestFalse(TEXT("reopen does not restore right rail"), Widget->IsRightPanelOpenForTest());
	return true;
}
```

- [ ] **Step 2: Run RED**

Expected: FAIL because action IDs 62/63/64 do not close the required regions and action 60 currently captures/restores the expanded session.

### Task 4: GREEN — local close buttons and global reset helper

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`

- [ ] **Step 1: Add scoped action constants and helpers**

In the `.cpp` anonymous namespace:

```cpp
	constexpr int32 ActionCloseWarehouse = 62;
	constexpr int32 ActionCloseCentralPage = 63;
	constexpr int32 ActionCloseRightPanel = 64;
```

In the class private section:

```cpp
	void CloseWarehousePanelToParent();
	void CloseCentralPageToBackpack();
	void CloseRightPanelToParent();
	void ResetWorkbenchChildrenForGlobalClose();
	void BuildPanelCloseButton(FName WidgetName, int32 ActionId, FVector2D Position);
```

- [ ] **Step 2: Implement state mutations**

```cpp
void UGameXXKDesktopTrainingWorkbenchWidget::CloseWarehousePanelToParent()
{
	CancelCarryForStructuralChange();
	bWarehousePanelOpen = false;
	if (ActiveNav == EGameXXKDesktopTrainingNav::Warehouse)
	{
		ActiveNav = EGameXXKDesktopTrainingNav::None;
	}
	RefreshLayout();
}

void UGameXXKDesktopTrainingWorkbenchWidget::CloseCentralPageToBackpack()
{
	CancelCarryForStructuralChange();
	ActiveCenterPage = EGameXXKDesktopTrainingCenterPage::Backpack;
	if (ActiveNav == EGameXXKDesktopTrainingNav::Formation || ActiveNav == EGameXXKDesktopTrainingNav::Talents)
	{
		ActiveNav = EGameXXKDesktopTrainingNav::None;
	}
	RefreshLayout();
}

void UGameXXKDesktopTrainingWorkbenchWidget::CloseRightPanelToParent()
{
	CancelCarryForStructuralChange();
	ReturnAllToolEntries();
	RightPanel = EGameXXKDesktopTrainingRightPanel::None;
	if (ActiveNav == EGameXXKDesktopTrainingNav::Tools || ActiveNav == EGameXXKDesktopTrainingNav::Training)
	{
		ActiveNav = EGameXXKDesktopTrainingNav::None;
	}
	RefreshLayout();
}

void UGameXXKDesktopTrainingWorkbenchWidget::ResetWorkbenchChildrenForGlobalClose()
{
	CancelCarryForStructuralChange();
	ReturnAllToolEntries();
	bWarehousePanelOpen = false;
	ActiveCenterPage = EGameXXKDesktopTrainingCenterPage::Backpack;
	RightPanel = EGameXXKDesktopTrainingRightPanel::None;
	ActiveNav = EGameXXKDesktopTrainingNav::None;
	bHasCollapsedWorkbenchSession = false;
	bHasSavedEmbeddedInventorySession = false;
}
```

- [ ] **Step 3: Build one approved close button per open region**

```cpp
void UGameXXKDesktopTrainingWorkbenchWidget::BuildPanelCloseButton(
	const FName WidgetName,
	const int32 ActionId,
	const FVector2D Position)
{
	UGameXXKDesktopTrainingActionButton* Button = WidgetTree->ConstructWidget<UGameXXKDesktopTrainingActionButton>(
		UGameXXKDesktopTrainingActionButton::StaticClass(), WidgetName);
	Button->Configure(this, ActionId);
	Button->SetStyle(MakeImageButtonStyle(CloseInkTexturePath, FVector2D(44.0f, 44.0f)));
	Button->SetBackgroundColor(FLinearColor::White);
	AddCanvas(RootCanvas, Button, Position, FVector2D(44.0f, 44.0f));
	ActionButtons.Add(Button);
}
```

Define `CloseInkTexturePath` as the approved `T_MasterV2_CloseInk` resource already used by inventory/roster widgets. Call the helper from:

```cpp
BuildWarehousePanel();  // top-right of left rail, action 62
BuildFormationPanel();  // top-right of central content, action 63
BuildTalentsPanel();    // top-right of central content, action 63
BuildToolsPanel();      // top-right of right rail, action 64
BuildTrainingMapPanel();// top-right of right rail, action 64
```

Use panel-local reference-canvas positions derived from the existing left/content/right rectangles; do not change their sizes.

- [ ] **Step 4: Route action IDs and replace action 60 restoration**

Add before the main action switch:

```cpp
	if (ActionId == ActionCloseWarehouse) { CloseWarehousePanelToParent(); return; }
	if (ActionId == ActionCloseCentralPage) { CloseCentralPageToBackpack(); return; }
	if (ActionId == ActionCloseRightPanel) { CloseRightPanelToParent(); return; }
```

Replace the expanded branch of action 60 with:

```cpp
		if (bBackpackExpanded)
		{
			ResetWorkbenchChildrenForGlobalClose();
			bBackpackExpanded = false;
			bExitConfirmationOpen = false;
			RefreshLayout();
		}
```

`OpenBackpack` no longer restores `SavedEmbeddedInventorySession` after a global close. Existing non-global persistence boundaries remain safe.

In `BuildBackpackTabToggle`, keep the collapsed down-arrow opener, but when `bBackpackExpanded` is true render the approved CloseInk glyph with no arrow text:

```cpp
	Toggle->SetStyle(bBackpackExpanded
		? MakeImageButtonStyle(CloseInkTexturePath, FVector2D(68.0f, 44.0f))
		: MakeTextureButtonStyle(CharacterTabNormalTexturePath, FVector2D(68.0f, 44.0f), FMargin(0.08f)));
	Toggle->SetContent(bBackpackExpanded
		? nullptr
		: MakeButtonText(WidgetTree, FText::FromString(TEXT("▼")), 24, Ink));
```

The expanded parent `X` and keyboard Tab both dispatch action 60 and therefore execute the same global reset.

- [ ] **Step 5: Run GREEN**

Run `GameXXK.DesktopTraining.Workbench.ParentCloseStack` and `EmbeddedBackpackDeferredRefresh`.

Expected: 2/2 passed, zero errors.

### Task 5: Regression and real PIE acceptance

**Files:**
- Verify only.

- [ ] **Step 1: Run the complete Workbench suite**

```powershell
D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe "D:\UE5 demo\GameXXK\GameXXK.uproject" -Unattended -NoSound -NullRHI -NoSplash -NoPause -ReportOutputPath="D:\UE5 demo\GameXXK\Saved\Automation\WorkbenchParentFinal" -ExecCmds="Automation RunTests GameXXK.DesktopTraining.Workbench; Quit"
python scripts/parse_automation_index.py --index Saved/Automation/WorkbenchParentFinal/index.json --json
```

Expected: all discovered Workbench tests succeed, zero errors.

- [ ] **Step 2: Cold UBT**

Run the project cold-build command from Task 1. Expected: `Result: Succeeded`.

- [ ] **Step 3: Real pure-2D clicks**

On `L_DesktopTrainingHUD`:

1. Open Backpack.
2. Click an occupied item; confirm the embedded paper remains visible.
3. Right-click an equipment item; confirm quick-equip and visible Backpack.
4. Open Warehouse + Talents + Tools; close each locally and verify unaffected regions.
5. Reopen all three; press Tab; verify only the idle strip remains.
6. Press Tab; verify clean Backpack with no restored child panels.

- [ ] **Step 4: Review the captured visual evidence**

Capture occupied-click, three-open-panels, local-close, global-collapse, and clean-reopen screenshots. Review them with a suitable method. Acceptance: approved close glyphs are aligned in panel-local coordinates, no panel blanks, and unrelated geometry is unchanged.

- [ ] **Step 5: Commit Unit A**

Stage only scoped source/test files and commit:

```powershell
git commit -m "fix: stabilize workbench parent close stack"
```
