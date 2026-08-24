# Live 2D Workbench Numeric Refresh Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep every visible numeric value on the 2D idle workbench current without rebuilding or reopening Backpack.

**Architecture:** `UGameXXKDesktopTrainingWorkbenchWidget` owns a one-second, visibility-gated presentation refresh and retains pointers to the outer numeric widgets. `UGameXXKInventoryWindowWidget` exposes one non-rebuilding refresh entry point for its currently visible character, inventory, equipment, and detail controls. Existing combat health-bar animation remains independent.

**Tech Stack:** Unreal Engine 5.8, C++ UMG, UE Automation Tests, project UBT/automation scripts.

---

### Task 1: Lock the live-refresh contract with failing automation tests

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKFinalInventoryWidgetTest.cpp`

- [ ] **Step 1: Add a workbench test for idle-strip, Backpack, and tool metrics**

Add `GameXXK.DesktopTraining.Workbench.LiveNumericRefreshWithoutRebuild`. Build a new-game subsystem and workbench, expand Backpack, select Attributes, and open Tools. Capture the workbench build count and embedded inventory pointer. Mutate the existing fixture through `GetMutableRuntimeState()`:

```cpp
FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
State.PlayerGold = 4321;
State.PlayerLevel = 2;
State.PlayerXP = 50;
State.PlayerHP = FMath::Max(1, State.PlayerMaxHP - 7);
State.Training.PendingTravelGold = 111;
State.Training.PendingTravelExperience = 222;
State.Training.PendingTravelNormalChestCount = 3;
State.Training.PendingTravelAdvancedChestCount = 4;
State.Training.TravelNormalChestCooldownRemainingSeconds = 119;
State.Training.TravelAdvancedChestCooldownRemainingSeconds = 179;
State.ToolProgress.Level = 2;
State.ToolProgress.Experience = 9;

FGameXXKTrainingChestToken NormalToken;
NormalToken.Tier = EGameXXKTrainingRewardTier::NormalChest;
NormalToken.AcquisitionOrdinal = ++State.Training.NextChestAcquisitionOrdinal;
State.Training.OwnedChestTokens.Add(NormalToken);
```

Call `Widget->TickForTest(1.0f)` while Travel is inactive, then assert:

```cpp
TestEqual(TEXT("live refresh never rebuilds the workbench"),
    Widget->GetProgrammaticLayoutBuildCountForTest(), BuildCountBeforeRefresh);
TestTrue(TEXT("live refresh preserves the embedded inventory instance"),
    FindEmbeddedInventory(Widget) == EmbeddedBeforeRefresh);
TestEqual(TEXT("Backpack gold updates in place"),
    TextOf(Widget, TEXT("BackpackGoldText")), FString(TEXT("4321")));
TestEqual(TEXT("normal chest count updates in place"),
    TextOf(Widget, TEXT("TrainingNormalChestCountText")), FString(TEXT("×1")));
TestTrue(TEXT("normal chest button becomes enabled"),
    CastChecked<UButton>(Widget->WidgetTree->FindWidget(TEXT("TrainingNormalChestButton")))->GetIsEnabled());
TestTrue(TEXT("hover text updates pending rewards and cooldown"),
    Widget->WidgetTree->FindWidget(TEXT("TrainingTravelStrip"))->GetToolTipText().ToString().Contains(TEXT("01:59")));
TestTrue(TEXT("visible Attributes page updates XP"),
    TextOf(EmbeddedBeforeRefresh, TEXT("InventoryCharacterExperienceText")).Contains(TEXT("50 / 200")));
TestTrue(TEXT("visible tool progress updates in place"),
    TextOf(Widget, TEXT("ToolProgressText")).Contains(TEXT("9/")));
```

Use a small local `TextOf` helper that finds a named `UTextBlock` and returns its text, so the assertions exercise the real UMG controls.

- [ ] **Step 2: Add a test proving an active tooltip continues to change**

After the first refresh, decrement both cooldown fields, call `TickForTest(1.0f)`, and assert the same `TrainingTravelStrip` instance now exposes `01:58`/`02:58`. Also assert that the layout build count is still unchanged.

- [ ] **Step 3: Add a task-NPC experience ownership assertion**

Select a known quest NPC, open Attributes, mutate the hero XP, refresh in place, and assert `InventoryCharacterExperienceText` and `InventoryCharacterExperienceBar` are collapsed. This prevents the current fallback from presenting hero experience as NPC experience.

- [ ] **Step 4: Add a right-click unequip presentation regression**

Extend `GameXXK.MVP.UI.FinalInventory.SixSlotsAndRightClickActions` in `Source/GameXXK/Private/Tests/GameXXKFinalInventoryWidgetTest.cpp`. Immediately after `QuickUnequipSlotForTest`, assert the returned instance is present in the desktop Backpack slot map and the already-open inventory exposes both a real slot and a non-empty icon path:

```cpp
const int32 ReturnedSlot = Inventory->FindBackpackEquipmentInstanceSlotForTest(ReplacementWeapon);
TestTrue(TEXT("right-click unequip normalizes the desktop Backpack slot"), ReturnedSlot >= 0);
TestFalse(TEXT("returned equipment icon is visible immediately"),
    Inventory->GetBackpackSlotIconResourcePathForTest(ReturnedSlot).IsEmpty());
TestTrue(TEXT("desktop Backpack owns the returned instance"),
    FGameXXKDesktopInventoryRules::FindEntrySlot(
        State,
        EGameXXKDesktopItemContainer::Backpack,
        FGameXXKDesktopInventoryRules::MakeEquipmentEntry(ReplacementWeapon)) >= 0);
```

- [ ] **Step 5: Run the focused tests and record the red result**

Save and stop PIE through UE MCP, close the editor, then run:

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.DesktopTraining.Workbench.LiveNumericRefreshWithoutRebuild,GameXXK.MVP.UI.FinalInventory.SixSlotsAndRightClickActions"
```

Expected: compile succeeds; the automation test fails because named live widgets and in-place refresh do not yet exist.

### Task 2: Add the non-rebuilding refresh path

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`

- [ ] **Step 1: Add the embedded inventory refresh entry point**

Declare a public method:

```cpp
/** Refreshes existing visible controls from runtime state without rebuilding WidgetTree. */
void RefreshVisibleRuntimeValues();
```

Implement it by returning early for a closed/collapsed window, then updating only the active page:

```cpp
void UGameXXKInventoryWindowWidget::RefreshVisibleRuntimeValues()
{
    if (WindowMode == EGameXXKInventoryWindowMode::None
        || GetVisibility() == ESlateVisibility::Collapsed)
    {
        return;
    }

    RefreshCharacterTabs();
    if (ActiveCharacterTab == EGameXXKCharacterBackpackTab::Equipment)
    {
        RefreshBackpackSlots();
        RefreshEquipmentSlots();
        RefreshDetailPanel();
        UpdateBackpackScrollbarThumb();
    }
    else if (ActiveCharacterTab == EGameXXKCharacterBackpackTab::Deck)
    {
        RefreshHeroDeckCards();
    }
}
```

In `RefreshCharacterTabs`, treat a quest NPC as a non-progressing display owner: keep its projected attributes, hide the XP text/bar, and never initialize its level/experience from `State.PlayerLevel`/`State.PlayerXP`.

In `QuickEquipBackpackInstanceForTest` and `QuickUnequipSlotForTest`, call `NormalizeDesktopInventoryState()` after a successful equipment transaction and before `RefreshProgrammaticLayout()`. This makes the direct embedded right-click path match the existing workbench wrapper, which already normalizes before rebuilding, and ensures the physical Backpack slot map owns the returned instance before the existing icon refresh reads it.

- [ ] **Step 2: Retain pointers to outer live controls**

Add transient members for `BackpackGoldText`, `TrainingTravelStrip`, the two chest buttons/count labels, `ToolProgressText`, warehouse summary/footer controls, and a `float LivePresentationAccumulator`. Reset the widget pointers inside `BuildProgrammaticLayout` before rebuilding children.

Name the existing local labels when constructing them:

```cpp
BackpackGoldText = WidgetTree->ConstructWidget<UTextBlock>(
    UTextBlock::StaticClass(), TEXT("BackpackGoldText"));
ToolProgressText = WidgetTree->ConstructWidget<UTextBlock>(
    UTextBlock::StaticClass(), TEXT("ToolProgressText"));
```

Extend `MakeIconLabelContent` with an optional output label pointer and name so the two chest labels become `TrainingNormalChestCountText` and `TrainingAdvancedChestCountText` without changing other callers.

- [ ] **Step 3: Implement one centralized outer refresh**

Add `FLivePresentationSnapshot`, `CaptureLivePresentationSnapshot()`, and `RefreshLivePresentation(bool bForce)`. The snapshot is a plain private C++ struct with equality over these exact fields:

```cpp
struct FLivePresentationSnapshot
{
    int32 PlayerGold = 0;
    int32 PlayerLevel = 0;
    int32 PlayerExperience = 0;
    int32 PlayerHealth = 0;
    int32 PlayerMana = 0;
    int32 PendingGold = 0;
    int32 PendingExperience = 0;
    int32 PendingNormalChests = 0;
    int32 PendingAdvancedChests = 0;
    int32 HeldNormalChests = 0;
    int32 HeldAdvancedChests = 0;
    int32 NormalChestCooldown = 0;
    int32 AdvancedChestCooldown = 0;
    int32 WarehouseOccupancy = 0;
    int32 WarehousePageCount = 1;
    int32 ToolLevel = 1;
    int64 ToolExperience = 0;
    int32 ToolCraftingLevel = 1;
    int32 OccupiedToolSlots = 0;

    bool operator==(const FLivePresentationSnapshot& Other) const = default;
};
```

Read one consistent subsystem view, compare it to `LastLivePresentationSnapshot`, and update existing controls only:

```cpp
void UGameXXKDesktopTrainingWorkbenchWidget::RefreshLivePresentation(const bool bForce)
{
    const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
    if (!Subsystem || GetVisibility() == ESlateVisibility::Collapsed)
    {
        return;
    }

    const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
    const FGameXXKTrainingProgress Progress = Subsystem->GetTrainingProgressCopy();
    const FGameXXKTrainingOfflineReward Pending = Subsystem->GetPendingTrainingTravelRewardCopy();
    const FLivePresentationSnapshot Current = CaptureLivePresentationSnapshot(
        *Subsystem, State, Progress, Pending);
    if (!bForce && LastLivePresentationSnapshot.IsSet()
        && LastLivePresentationSnapshot.GetValue() == Current)
    {
        return;
    }

    if (BackpackGoldText)
    {
        BackpackGoldText->SetText(FText::AsNumber(Current.PlayerGold));
    }
    UpdateTravelRewardTooltip(Current);
    UpdateTrainingChestPresentation(
        EGameXXKTrainingRewardTier::NormalChest,
        Current.HeldNormalChests);
    UpdateTrainingChestPresentation(
        EGameXXKTrainingRewardTier::AdvancedChest,
        Current.HeldAdvancedChests);
    UpdateWarehouseNumericPresentation(Current);
    UpdateToolNumericPresentation(Current);
    if (EmbeddedInventoryWidget)
    {
        EmbeddedInventoryWidget->RefreshVisibleRuntimeValues();
    }
    LastLivePresentationSnapshot = Current;
}
```

`UpdateTravelRewardTooltip` uses the existing Chinese wording and `MM:SS` formatter. `UpdateTrainingChestPresentation` sets the retained count label, button tooltip, and enabled state. `UpdateWarehouseNumericPresentation` updates the retained occupancy/page/footer controls. `UpdateToolNumericPresentation` updates tool level/XP/next-level text, selected crafting level, and confirm enablement. Existing inventory/tool actions continue to refresh their changed icons immediately; the snapshot path handles numeric changes originating from idle progression or external state mutation.

- [ ] **Step 4: Drive time and action refreshes**

In `NativeTick`, accumulate non-negative delta time and call `RefreshLivePresentation(false)` once per second. Call it again immediately after a successful `AdvanceTravelForTest` step so encounter rewards display in the same logical second. After successful button mutations that do not already rebuild a panel, call `RefreshLivePresentation(true)`.

Never call `RefreshLayout` from the live path. Leave `UpdateTravelVisuals` as the sole health-bar animation path.

- [ ] **Step 5: Run the focused test until green**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.DesktopTraining.Workbench.LiveNumericRefreshWithoutRebuild,GameXXK.MVP.UI.FinalInventory.SixSlotsAndRightClickActions"
```

Expected: PASS; workbench build count and embedded inventory identity remain unchanged while all named values change.

- [ ] **Step 6: Commit only the task hunks**

Stage the new test and the live-refresh hunks only, preserving unrelated dirty edits:

```powershell
git add -p -- Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKFinalInventoryWidgetTest.cpp Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp
git commit -m "fix: refresh visible workbench values live"
```

### Task 3: Regression and real PIE verification

**Files:**
- Verify only; do not modify user-tuned assets.

- [ ] **Step 1: Run the related automation group**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.DesktopTraining.Workbench,GameXXK.InventoryWindow.CharacterTabs,GameXXK.Training"
```

Expected: the new live-refresh test and related Backpack/Training tests pass. Record any pre-existing unrelated failures separately rather than altering their files.

- [ ] **Step 2: Launch the canonical 2D map and verify interaction state**

Launch the editor on `/Game/GameXXK/Maps/L_DesktopTrainingHUD`, start PIE, expand Backpack, open Attributes, and keep the workbench visible. Confirm the selected role, page, scroll position, and panel layout remain unchanged over several refreshes.

- [ ] **Step 3: Verify live reward and cooldown presentation**

Keep the pointer over the idle strip and observe at least two cooldown decrements. Complete one encounter and verify gold/XP/chest values and button enablement update without pressing Tab, closing Backpack, or changing page.

- [ ] **Step 4: Leave PIE ready for user acceptance**

Keep PIE running on `L_DesktopTrainingHUD` with Backpack expanded and Attributes visible so the user can directly inspect the live experience bar, chest counts, and hover countdown.

### Task 4: Remove the Blade-Finish automatic-battle deadlock

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Verify: `Source/GameXXK/Private/Tests/GameXXKBladePartnerCoreRuntimeTest.cpp`
- Verify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`

- [ ] **Step 1: Reproduce the exact failure with existing tests**

Run the already-authored Lie Feng round-boundary contract and the generic automatic end-turn contract:

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.Data.PartnerCards.BladeRuntime.LieFengFinishReturnsNextRoundFirstActive,GameXXK.Integration.CardBattle.BoardAutoPlayEndsTurn"
```

Expected red result before the fix: Lie Feng cannot end player phase and reports `trigger_round=2` while `phase=Player` and `round=1`. The live PIE debug state must show the same error, zero shared energy, no pending choice, and no presentation queue.

- [ ] **Step 2: Defer future-round state installation until the phase boundary is valid**

In `EndPlayerCardPhase`, retain the effective last-Blade definition and source snapshot locally, but do not call `ArmImplementedPartnerBladeFinish` or `ResolvePoJunAfterSuccessfulBladeFinish` before player-side DoT resolution. After player-side DoT/healer resolution and `NewRuntime.Phase = Enemy`, install the deferred Finish/PoJun state, then reset current-round PoJun progress and run final runtime validation.

Terminal Victory/Defeat skips deferred installation and keeps the existing terminal cleanup. Supplemental Finish damage remains before player-side DoT, so only the future-state write timing changes.

- [ ] **Step 3: Run the focused Blade and automatic-board tests**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests "GameXXK.Data.PartnerCards.BladeRuntime.LieFengFinishReturnsNextRoundFirstActive,GameXXK.Integration.CardBattle.BoardAutoPlayEndsTurn,GameXXK.Integration.CardBattle.RouteAutoBattleStall"
```

Expected: all pass; the Blade Finish is valid in Enemy phase with trigger round `current + 1`, and automatic battle advances rather than repeating a failed end-turn request.

- [ ] **Step 4: Verify the production encounter in PIE**

Restart the canonical 2D PIE, enable automatic battle, and observe a round where a Blade card is last and shared energy reaches zero. Confirm the board enters Enemy and then the next Player round without manual input or a persistent `lastError`.

### Task 5: Align Backpack title and deck-card portraits with battle

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKFinalInventoryWidgetTest.cpp`

- [ ] **Step 1: Add the six-profession red test**

Open each permanent companion's Deck page and assert `InventoryHeroDeckPortrait_00` uses the matching `T_CardPortrait_Role_*` resource rather than either hero portrait. Assert the named Backpack title text is exactly `背包`.

- [ ] **Step 2: Use battle-equivalent identity fields and resources**

Resolve profession card art from `FGameXXKCardDefinition::Role` (the same field used by BattleBoard), keep the existing NPC-id mapping, and use `/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Hero` for hero cards. Name the title control `InventoryWindowTitleText`, render `背包` for free inventory, and retain `商铺` for merchant mode.

- [ ] **Step 3: Verify the full FinalInventory group**

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests "GameXXK.MVP.UI.FinalInventory"
```

Expected: every FinalInventory test passes, including all six profession portraits, title, and embedded unequip presentation.
