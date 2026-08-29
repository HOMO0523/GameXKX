# Desktop 2D Narrative and Story Task Drawer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the 3D-town-dependent story entry and legacy `I/Q/C` windows with a Workbench task drawer and a screen-level 2D Narrative Layer that can always pause safely, replay from the authored segment entry, and claim material rewards later.

**Architecture:** The existing Desktop Overlay remains the only desktop native window. Workbench, StoryTaskDrawer, and NarrativeLayer become sibling presentation layers; pure StoryTask rules build actionable/claimable views and atomically separate story completion from material reward claiming. PlayerController owns semantic input routing and the fail-safe abort path, while Dialogue/Narrative coordinators remain map- and widget-geometry-independent.

**Tech Stack:** UE 5.8 C++, UMG/Slate `SWindow`, project DirectComposition overlay, existing Dialogue/Narrative cores, JSON authoring/importers, UE Automation, cold UBT, project UE MCP and Win32/luna acceptance.

---

## File map

### New runtime files

- `Source/GameXXK/Public/Narrative/GameXXKStoryTaskDrawerRules.h` — pure task filtering, sorting, selection and red-dot rules.
- `Source/GameXXK/Private/Narrative/GameXXKStoryTaskDrawerRules.cpp` — rule implementation.
- `Source/GameXXK/Public/UI/GameXXKStoryTaskDrawerWidget.h` — warehouse-sized task drawer.
- `Source/GameXXK/Private/UI/GameXXKStoryTaskDrawerWidget.cpp` — two tabs, compact rows, fixed detail/action area.
- `Source/GameXXK/Public/UI/GameXXKDesktopNarrativeLayerWidget.h` — full-work-area Narrative host API.
- `Source/GameXXK/Private/UI/GameXXKDesktopNarrativeLayerWidget.cpp` — 2D stage, formal panel, history, pause and safe-area layout.
- `Source/GameXXK/Public/Narrative/GameXXKDesktopNarrativeExecutor.h` — typed replayable 2D stage command executor.
- `Source/GameXXK/Private/Narrative/GameXXKDesktopNarrativeExecutor.cpp` — semantic slot, role, prop and VFX commands.

### New tests

- `Source/GameXXK/Private/Tests/GameXXKStoryTaskClaimFlowTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKStoryTaskDrawerRulesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKStoryTaskDrawerWidgetTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKDesktopNarrativeLayerTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKNarrativeAbortRecoveryTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKDesktopNarrativeInputTest.cpp`

### Existing files modified

- `Source/GameXXK/Public/Narrative/GameXXKNarrativeTypes.h`
- `Source/GameXXK/Private/Narrative/GameXXKStoryCatalog.cpp`
- `Source/GameXXK/Private/Narrative/GameXXKStoryRules.cpp`
- `Source/GameXXK/Public/Narrative/GameXXKStoryRules.h`
- `Source/GameXXK/Public/Narrative/GameXXKNarrativeCoordinator.h`
- `Source/GameXXK/Private/Narrative/GameXXKNarrativeCoordinator.cpp`
- `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- `Source/GameXXK/Public/UI/GameXXKDesktopWorkbenchSessionState.h`
- `Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp`
- `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKInteractionRouterTest.cpp`
- `SourceAssets/Narrative/runtime-catalog.json`
- `SourceAssets/Narrative/Dialogues/Dialogue.Tutorial.001.dialogue.json`
- `SourceAssets/Narrative/Sequences/Sequence.Main.XuXiake.CarriageArrival.sequence.json`
- `Content/Python/gamexxk_import_dialogue_json.py`
- `Content/Python/gamexxk_import_narrative_sequence_json.py`

## Working-tree safety

The root `main` worktree is intentionally dirty and the controller/workbench/test files contain user-owned changes. Do not create a worktree, reset, checkout, restore, stage, or commit those overlapping files. Use `apply_patch`, keep edits scoped, run `git diff --check -- <exact files>`, and commit only newly created isolated files or documentation when a path-limited commit cannot capture user changes.

Before every cold build: if UE is running, save dirty packages through `scripts/ue_mcp_client.py`, stop PIE, save once more, and close normally. Never use Live Coding or Hot Reload.

---

### Task 1: Close the current dual-window baseline before adding Narrative ownership

**Files:**
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Verify the interrupted regression is present**

The real lifecycle test must create a retained-but-disabled Workbench and assert Battle cannot be overwritten by fallback:

```cpp
Controller->SetMVPSubsystemForTest(Subsystem);
Controller->SetDesktopTrainingWorkbenchEnabledForTest(true);
Controller->SetDesktopTrainingWorkbenchEnabledForTest(false);
Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Battle;
Controller->RefreshPlayerFlowWidgetsForTest();
TestEqual(TEXT("disabled retained workbench preserves Battle fullscreen"),
    Controller->GetLastRequestedPrimaryWindowPresentationForTest(),
    EGameXXKWindowPresentation::FullscreenGameplay);
```

- [ ] **Step 2: Run the four focused window tests**

Run Automation filters:

```text
GameXXK.DesktopTraining.Workbench.WindowPresentationResolver
GameXXK.DesktopTraining.Workbench.WindowPresentationActions
GameXXK.DesktopTraining.Workbench.WindowPresentationLifecycle
GameXXK.DesktopTraining.Workbench.WindowPresentationDisabledWorkbenchRoute
```

Expected: 4 discovered, 4 succeeded, zero warning/error. If the fourth test has never been observed RED, temporarily restore the unconditional fallback line, run it once to capture actual `ViewportFallback`, then reapply the production-used `ResolveWindowPresentation(..., false, false)` fix.

- [ ] **Step 3: Cold-build GREEN**

Run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development `
  '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex `
  -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2
```

Expected: `Result: Succeeded`.

- [ ] **Step 4: Record but do not hide unrelated Workbench failures**

Run full `GameXXK.DesktopTraining.Workbench`. Record the existing `TransparentDesktopPlacement` paper-art failure and missing 1K atlas warnings separately; do not modify protected art to make this task green.

---

### Task 2: Add authored task presentation, story flags and separated material rewards

**Files:**
- Modify: `Source/GameXXK/Public/Narrative/GameXXKNarrativeTypes.h`
- Modify: `Source/GameXXK/Private/Narrative/GameXXKStoryCatalog.cpp`
- Modify: `Source/GameXXK/Public/Narrative/GameXXKStoryRules.h`
- Modify: `Source/GameXXK/Private/Narrative/GameXXKStoryRules.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKStoryTaskClaimFlowTest.cpp`

- [ ] **Step 1: Write failing domain tests**

Add tests that require:

```cpp
const auto ExportRoster = [](const FGameXXKCompanionRosterState& Roster)
{
    FString Text;
    FGameXXKCompanionRosterState::StaticStruct()->ExportText(
        Text, &Roster, &Roster, nullptr, PPF_None, nullptr);
    return Text;
};
const FString BeforeRosterText = ExportRoster(State.CardRun.CompanionRoster);

TestTrue(TEXT("available task can be accepted"),
    Subsystem->AcceptNarrativeTask(TEXT("Task.Main.XuXiake.Prologue"), &Error));
TestEqual(TEXT("accepted task is active"), Task.State, EGameXXKTaskState::Active);

TestTrue(TEXT("normal story completion commits plot results atomically"),
    Subsystem->CompleteNarrativeTaskStory(TaskId, TEXT("Step.Main.XuXiake.RiverScroll"), &Error));
TestTrue(TEXT("YueBai meeting is a story flag"), Story.Flags.Contains(TEXT("StoryFlag.Met.YueBai")));
TestEqual(TEXT("story completion waits for reward claim"), Task.State, EGameXXKTaskState::Completed);
TestFalse(TEXT("story completion does not commit material reward"), Task.bRewardCommitted);
TestEqual(TEXT("NPC configuration is unchanged"),
    ExportRoster(State.CardRun.CompanionRoster),
    BeforeRosterText);

TestTrue(TEXT("claim commits the complete reward bundle"),
    Subsystem->ClaimNarrativeTaskReward(TaskId, &Error));
TestEqual(TEXT("claimed task is rewarded"), Task.State, EGameXXKTaskState::Rewarded);
TestTrue(TEXT("reward receipt is permanent"), Task.bRewardCommitted);
TestFalse(TEXT("reward cannot be claimed twice"),
    Subsystem->ClaimNarrativeTaskReward(TaskId, &Error));
```

Create a failure fixture whose item destination rejects the bundle and assert gold, XP, items, state and red-dot eligibility are byte-for-byte unchanged.

- [ ] **Step 2: Run RED**

Expected: compile failure for missing presentation/reward fields and subsystem APIs.

- [ ] **Step 3: Add exact domain types**

Extend definitions, not save-state identity, with:

```cpp
USTRUCT()
struct FGameXXKNarrativeTaskRewardDefinition
{
    GENERATED_BODY()
    int32 Gold = 0;
    int32 Experience = 0;
    TMap<FName, int32> Items;
};

// FGameXXKTaskDefinition
FText Title;
FText Summary;
FText Description;
int32 AuthoredOrder = 0;
FGameXXKNarrativeTaskRewardDefinition MaterialReward;
TSet<FName> CompletionStoryFlags;

// FGameXXKStoryProgress
TSet<FName> Flags;

// FGameXXKTaskProgress
int64 CompletedAtUtcTicks = 0;
```

Do not add NPC unlock/ownership fields.

- [ ] **Step 4: Implement atomic state transitions**

Implement candidates and validate before assignment:

```cpp
bool AcceptNarrativeTask(FName TaskId, FString* OutError);
bool CompleteNarrativeTaskStory(FName TaskId, FName CompletedStepId, FString* OutError);
bool ClaimNarrativeTaskReward(FName TaskId, FString* OutError);
```

`CompleteNarrativeTaskStory` writes story flags, advances the authored graph, sets `CompletedAtUtcTicks`, then makes the task Completed without material mutation. `ClaimNarrativeTaskReward` preflights the entire bundle, applies it to one candidate, marks Rewarded and `bRewardCommitted=true`, validates, assigns and immediately saves. Roll back if save fails.

- [ ] **Step 5: Run GREEN**

Run `GameXXK.Narrative.StoryTask` and the new `GameXXK.Narrative.StoryTask.ClaimFlow`. Expected: zero failed/error.

- [ ] **Step 6: Commit isolated new test only if safe**

Commit the new test file only; leave overlapping domain files unstaged if they contain user changes.

---

### Task 3: Build pure task-drawer views and selection rules

**Files:**
- Create: `Source/GameXXK/Public/Narrative/GameXXKStoryTaskDrawerRules.h`
- Create: `Source/GameXXK/Private/Narrative/GameXXKStoryTaskDrawerRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKStoryTaskDrawerRulesTest.cpp`

- [ ] **Step 1: Write failing view-model tests**

Require exact views:

```cpp
const FGameXXKStoryTaskDrawerSnapshot Snapshot =
    FGameXXKStoryTaskDrawerRules::BuildSnapshot(Catalog, Progress, UiState);
TestEqual(TEXT("active precedes available"), Snapshot.Actionable[0].State, EGameXXKTaskState::Active);
TestEqual(TEXT("actionable active label"), Snapshot.Actionable[0].ActionLabel.ToString(), TEXT("继续剧情"));
TestEqual(TEXT("actionable available label"), Snapshot.Actionable[1].ActionLabel.ToString(), TEXT("接取任务"));
TestEqual(TEXT("newest completion first"), Snapshot.Claimable[0].TaskId, NewestCompletedId);
TestEqual(TEXT("claim label"), Snapshot.Claimable[0].ActionLabel.ToString(), TEXT("领取奖励"));
TestTrue(TEXT("claimable tab has red dot"), Snapshot.bHasClaimableRedDot);
TestFalse(TEXT("locked task hidden"), Snapshot.Contains(LockedTaskId));
TestFalse(TEXT("rewarded task hidden"), Snapshot.Contains(RewardedTaskId));
```

Also test independent selected TaskId/scroll state per tab and invalid saved selection fallback.

- [ ] **Step 2: Run RED**

Expected: missing rules types.

- [ ] **Step 3: Implement the pure rules**

Define:

```cpp
enum class EGameXXKStoryTaskDrawerTab : uint8 { Actionable, Claimable };
struct FGameXXKStoryTaskDrawerEntryView { FName TaskId; FText Title; FText Summary; FText Description; FText ActionLabel; ... };
struct FGameXXKStoryTaskDrawerUiState { EGameXXKStoryTaskDrawerTab ActiveTab; FName SelectedActionableTaskId; FName SelectedClaimableTaskId; float ActionableScrollOffset; float ClaimableScrollOffset; };
struct FGameXXKStoryTaskDrawerSnapshot { TArray<...> Actionable; TArray<...> Claimable; bool bHasClaimableRedDot; ... };
```

Sorting must exactly follow the spec and never mutate RuntimeState.

- [ ] **Step 4: Run GREEN and commit the isolated rules/test files**

Expected: `GameXXK.Narrative.StoryTask.DrawerRules` all green.

---

### Task 4: Build the warehouse-sized StoryTaskDrawer widget

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKStoryTaskDrawerWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKStoryTaskDrawerWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKStoryTaskDrawerWidgetTest.cpp`

- [ ] **Step 1: Write failing widget contracts**

Construct the widget with a transient subsystem and assert named controls:

```cpp
TestNotNull(TEXT("task drawer uses unified close position"), Find(TEXT("StoryTaskDrawerClose")));
TestNotNull(TEXT("actionable tab exists"), Find(TEXT("StoryTaskDrawerActionableTab")));
TestNotNull(TEXT("claimable tab exists"), Find(TEXT("StoryTaskDrawerClaimableTab")));
TestNotNull(TEXT("claim red dot exists"), Find(TEXT("StoryTaskDrawerClaimableRedDot")));
TestNotNull(TEXT("scrolling task list exists"), Find(TEXT("StoryTaskDrawerList")));
TestNotNull(TEXT("fixed action button exists"), Find(TEXT("StoryTaskDrawerPrimaryAction")));
TestEqual(TEXT("no row owns an action button"), Drawer->CountRowActionButtonsForTest(), 0);
```

Assert the close button rect equals the current warehouse close rect, red-dot visibility tracks the snapshot, and selecting a row changes only the fixed action button.

- [ ] **Step 2: Run RED**

Expected: missing widget class.

- [ ] **Step 3: Implement the drawer**

Use the approved tall panel texture and the same local panel footprint as Warehouse. The widget receives snapshot data and delegates:

```cpp
DECLARE_DELEGATE(FGameXXKStoryTaskDrawerClosed);
DECLARE_DELEGATE_TwoParams(FGameXXKStoryTaskDrawerAction, FName, EGameXXKTaskState);
```

Rows are selection-only. `Esc`, the close button and external task-button toggle all call the same close delegate. Preserve the two scroll offsets and selected IDs in `FGameXXKStoryTaskDrawerUiState`.

- [ ] **Step 4: Run GREEN and commit isolated widget/test files**

Expected: widget test zero warning/error.

---

### Task 5: Replace the warehouse bool with a left-panel mode and embed StoryTasks

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopWorkbenchSessionState.h`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Write failing Workbench integration tests**

Require:

```cpp
Widget->OpenBackpack();
Widget->TriggerStoryTaskButtonForTest();
TestEqual(TEXT("story button opens StoryTasks"), Widget->GetLeftPanelForTest(), EGameXXKDesktopTrainingLeftPanel::StoryTasks);
TestTrue(TEXT("task and town rail uses open-panel placement"), Widget->UsesOpenLeftPanelButtonPlacementForTest());
Widget->TriggerStoryTaskButtonForTest();
TestEqual(TEXT("shifted story button toggles closed"), Widget->GetLeftPanelForTest(), EGameXXKDesktopTrainingLeftPanel::None);
```

Also assert close button and Esc use the same close path, Tab remains expanded, Warehouse page/sort/filter state survives, and opening StoryTasks aborts carry/tool transactions.

- [ ] **Step 2: Run RED**

Expected: missing left-panel enum/APIs.

- [ ] **Step 3: Implement the left-panel state**

Replace decisions based on `bWarehousePanelOpen` with:

```cpp
enum class EGameXXKDesktopTrainingLeftPanel : uint8 { None, Warehouse, StoryTasks };
EGameXXKDesktopTrainingLeftPanel LeftPanel = EGameXXKDesktopTrainingLeftPanel::None;
```

Keep compatibility accessors for existing tests/session snapshots while migrating internal layout, native hit regions, button placement and close actions to `LeftPanel != None`. Build exactly one of Warehouse or StoryTaskDrawer.

The task action no longer calls `RequestDesktopTutorialQuestFromWorkbench`; it toggles StoryTasks. `RequestDesktopTutorialQuestFromWorkbench` remains only as an explicit Legacy API until Task 10 removes its default routing.

- [ ] **Step 4: Run GREEN**

Run focused StoryTaskDrawer integration plus complete Workbench. Separate unrelated art failures as in Task 1.

---

### Task 6: Redirect I/Q/C and TownHud buttons into Workbench semantics

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKDesktopNarrativeInputTest.cpp`

- [ ] **Step 1: Replace old expected behavior with failing redirect tests**

Delete the assertion that `I` opens FreeInventory and require:

```cpp
Controller->InputKey(Simulate(EKeys::I));
TestTrue(TEXT("I expands Workbench"), Workbench->IsBackpackExpandedForTest());
TestEqual(TEXT("I selects embedded backpack"),
    Workbench->GetActiveCenterPageForTest(),
    EGameXXKDesktopTrainingCenterPage::Backpack);
TestEqual(TEXT("I never opens independent inventory"), InventoryWindow->GetWindowModeForTest(), EGameXXKInventoryWindowMode::None);

Controller->InputKey(Simulate(EKeys::Q));
TestEqual(TEXT("Q opens StoryTasks"), Workbench->GetLeftPanelForTest(), EGameXXKDesktopTrainingLeftPanel::StoryTasks);

Controller->InputKey(Simulate(EKeys::C));
TestEqual(TEXT("C selects formation"), Workbench->GetActiveCenterPageForTest(), EGameXXKDesktopTrainingCenterPage::Formation);
```

Open a blocking Narrative session and assert `Tab/I/Q/C/F` all return handled without changing Workbench state.

- [ ] **Step 2: Run RED**

Expected: old `I/Q/C` windows still open or new page APIs missing.

- [ ] **Step 3: Implement one semantic router**

Add:

```cpp
enum class EGameXXKWorkbenchShortcut : uint8 { ToggleTab, Backpack, StoryTasks, Formation };
bool RouteWorkbenchShortcut(EGameXXKWorkbenchShortcut Shortcut);
```

`InputKey` and TownHud buttons both call it. It validates Town, opens Workbench, expands when needed, closes mutually exclusive old modals, and selects the target. Narrative input consumes C in addition to existing keys.

- [ ] **Step 4: Run GREEN**

Run the new input test, `GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets`, Interaction Router and TownHud tests.

---

### Task 7: Add the screen-level Desktop Narrative Layer and full-work-area host mode

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKDesktopNarrativeLayerWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKDesktopNarrativeLayerWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKDesktopNarrativeLayerTest.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`

- [ ] **Step 1: Write failing layer/geometry tests**

Require:

```cpp
Workbench->EnterNarrativePresentationForTest();
TestTrue(TEXT("Narrative layer active"), Workbench->IsNarrativeLayerActiveForTest());
TestFalse(TEXT("Workbench layer hidden"), Workbench->IsWorkbenchLayerVisibleForTest());
TestTrue(TEXT("host uses work area"), Workbench->GetDesktopWindowSizeForHost().Equals(WorkAreaSize));
TestTrue(TEXT("dialogue paper is bottom safe-area anchored"), Layer->IsDialoguePaperBottomAnchoredForTest());
TestTrue(TEXT("pause is top-right safe-area anchored"), Layer->IsPauseTopRightAnchoredForTest());
TestTrue(TEXT("center stage exists"), Layer->HasSemanticStageForTest());
```

After exit, require Workbench visible, ordinary folded strip, Tab unlocked and original desktop window bounds restored.

- [ ] **Step 2: Run RED**

Expected: missing Narrative Layer/host mode.

- [ ] **Step 3: Build the layer**

Create the layer as a fullscreen sibling under `DesktopOverlayRootCanvas`. It owns existing-style DialoguePanel/History children and a new stage canvas. Define semantic slots with normalized Safe Area coordinates; do not use desktop HUD anchor values.

Add `EGameXXKDesktopOverlaySurface { Workbench, NarrativeFullscreen }`. In NarrativeFullscreen, `GetDesktopWindowSizeForHost` and native layout use monitor work area; the workbench root is Collapsed and Narrative Layer Visible. Update native hit testing so pause, dialogue, choices and history are client input while transparent areas remain pass-through where safe.

- [ ] **Step 4: Run GREEN**

Run layer tests and existing native-layout/mouse-passthrough tests.

---

### Task 8: Bind Dialogue/Sequence to the 2D host and implement replayable stage commands

**Files:**
- Create: `Source/GameXXK/Public/Narrative/GameXXKDesktopNarrativeExecutor.h`
- Create: `Source/GameXXK/Private/Narrative/GameXXKDesktopNarrativeExecutor.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Public/Narrative/GameXXKNarrativeCoordinator.h`
- Modify: `Source/GameXXK/Private/Narrative/GameXXKNarrativeCoordinator.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKNarrativeCoordinatorTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKInteractionRouterTest.cpp`

- [ ] **Step 1: Write failing desktop-host command tests**

Test commands:

```text
stageShowRole, stageHideRole, stageMoveRole, stageSetFacing,
stageShowProp, stageHideProp, stagePlayAction, stagePlayVfx,
stageFlash, showToast, dialogue
```

Assert every command references a declared semantic slot/role, presentation commands are replayable, asynchronous action completion is Pending and cancellable, and unknown required resources trigger the fail-safe abort delegate.

- [ ] **Step 2: Run RED**

Expected: missing executor and host binding.

- [ ] **Step 3: Implement the typed executor**

The executor receives only `UGameXXKDesktopNarrativeLayerWidget` plus compiled command data. It never accesses world actors, map paths or numeric world transforms. `CancelPending` cancels timers/delegates and restores stage idle without committing gameplay state.

When a desktop story starts, PlayerController binds DialogueCoordinator to the Narrative Layer presenters instead of creating viewport-only Dialogue widgets. Legacy NPC interaction may keep the old viewport host only on explicit 3D maps.

- [ ] **Step 4: Run GREEN**

Run NarrativeCoordinator, Dialogue, Interaction Router and new executor tests.

---

### Task 9: Implement fail-safe pause, segment replay and no-auto-resume loading

**Files:**
- Modify: `Source/GameXXK/Public/Narrative/GameXXKNarrativeCoordinator.h`
- Modify: `Source/GameXXK/Private/Narrative/GameXXKNarrativeCoordinator.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKNarrativeAbortRecoveryTest.cpp`

- [ ] **Step 1: Write the full RED recovery matrix**

Fixtures must pause during Dialogue, choice, wait, move/action Pending, missing asset, null DialogueCoordinator, null NarrativeCoordinator and repeated pause. Every fixture asserts:

```cpp
TestFalse(TEXT("Narrative layer closes"), Workbench->IsNarrativeLayerActiveForTest());
TestTrue(TEXT("folded Workbench returns"), Workbench->IsWorkbenchVisibleForTest() && !Workbench->IsBackpackExpandedForTest());
TestFalse(TEXT("Tab unlocked"), Workbench->IsNarrativeTabLockedForTest());
TestFalse(TEXT("move lock released"), Controller->IsMoveInputIgnored());
TestFalse(TEXT("look lock released"), Controller->IsLookInputIgnored());
TestEqual(TEXT("task remains active"), Task.State, EGameXXKTaskState::Active);
TestEqual(TEXT("sequence reset to segment entry"), Session.CurrentSequenceStepId, Asset.EntryStepId);
```

Migrate/load an active Dialogue/Narrative save and require an inactive presenter, folded desktop, Active task at segment entry, and no automatic Start/Resume call. Migrate a Completed-unclaimed task and require only claim red dot.

- [ ] **Step 2: Run RED**

Expected: current PauseAndExit/Release do not reset segment/UI/save or survive null dependencies.

- [ ] **Step 3: Implement `AbortNarrativeToDesktop`**

Use an idempotent re-entry guard and cleanup without early returns:

```cpp
bool AGameXXKMVPPlayerController::AbortNarrativeToDesktop(EGameXXKNarrativeAbortReason Reason)
{
    const TGuardValue<bool> Guard(bNarrativeAbortInProgress, true);
    if (NarrativeCoordinator) NarrativeCoordinator->ResetActiveSegmentToEntry();
    if (DialogueCoordinator) DialogueCoordinator->PauseAndExit();
    SetNarrativeInputLocked(false);
    if (Workbench) Workbench->ExitNarrativePresentationToFoldedDesktop();
    return Subsystem && Subsystem->SaveCurrentGame();
}
```

The real implementation must preserve the original failure reason even if saving fails, always restore UI/input first, and never mutate NPC/party state.

Add a bounded timeout per required asynchronous command; timeout invokes the same abort path. Pause button and Esc call the same function. Normal completion and abort share an atomic completion guard.

- [ ] **Step 4: Normalize load/shutdown**

Increment the actual next save version once (do not collide with an already-landed migration). Normalize active Dialogue/Narrative sessions to the authored segment entry while keeping Task Active and executed gameplay idempotency keys. Do not open Narrative Layer from BeginPlay. Normal game close calls abort-before-save; crash recovery is handled by migration/load normalization.

- [ ] **Step 5: Run GREEN**

Run abort recovery, Dialogue, Narrative, save migration and Workbench tests. Expected zero failure/error in the new scope.

---

### Task 10: Route the real story button, extend window ownership and verify the complete player flow

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`
- Modify: `SourceAssets/Narrative/Dialogues/Dialogue.Tutorial.001.dialogue.json`
- Modify: `SourceAssets/Narrative/Sequences/Sequence.Main.XuXiake.CarriageArrival.sequence.json`
- Modify: `SourceAssets/Narrative/runtime-catalog.json`
- Create: `Content/Python/gamexxk_probe_desktop_story_flow.py`
- Create: `scripts/run_desktop_story_flow_mcp.py`

- [ ] **Step 1: Write failing end-to-end Automation contracts**

Require real button routing:

```text
expand Tab → click story button → StoryTasks open in warehouse region
→ accept task → Workbench layer hidden → Narrative layer active
→ Space/choices complete story → plot flags/next step committed
→ folded Workbench returns → claim red dot visible
→ reopen StoryTasks/Claimable → claim reward once
```

Assert no map travel, no 3D SceneProfile requirement, no NPC unlock mutation, and no viewport Dialogue presenter.

- [ ] **Step 2: Extend the window resolver RED/GREEN**

Change the production-used resolver signature to include `bNarrativeLayerActive` and test:

```cpp
Desktop map + Workbench hidden + Narrative active + Overlay attached
    => DesktopIdleOverlay;
Desktop map + Workbench hidden + Narrative inactive
    => ViewportFallback;
```

PlayerTick reassert conditions use Overlay ownership (`Workbench visible || Narrative active`), not only Workbench visibility.

- [ ] **Step 3: Author/import the first desktop story**

Encode the approved prologue text in `Dialogue.Tutorial.001`. Convert its Sequence from world/SceneProfile commands to typed desktop stage slots. Completion writes `StoryFlag.Met.YueBai`, advances the authored main step and marks the task Completed; it never unlocks an NPC or auto-grants material reward.

Run validators before import. Pure art/flipbook work does not use TDD; verify dimensions, frame counts, alpha edges, hashes and contact sheets deterministically.

- [ ] **Step 4: Implement real MCP flow probe**

The probe reports:

```text
map, screen, overlay HWND mode, primary HWND state,
left panel, task tab, selected task, red dot,
narrative active, stage roles/slots, dialogue node,
Workbench visibility, Tab lock, input locks,
story flags, task state, reward receipt
```

Actions are bounded: expand, open tasks, select, primary action, advance, option, pause, reopen, claim.

- [ ] **Step 5: Verify pause/restart and shutdown**

Pause at three boundaries, verify folded desktop, then click Continue and require replay from segment entry. Close/restart during active narrative and require no auto-entry. Complete without claim, restart and require only the claim red dot.

- [ ] **Step 6: Verify window and visual presentation**

In `UnrealEditor -game`:

- Desktop task drawer: primary minimized, Overlay HUD-sized.
- Narrative: primary minimized, Overlay full-work-area, no Workbench/hang strip visible.
- Route/Battle: primary restored WindowedFullscreen, Overlay hidden.
- Pause/complete: Overlay returns to HUD-sized folded desktop.

Capture task drawer, Narrative dialogue, pause recovery and claimable red-dot screenshots. Use `codex_vision.ps1 -Effort max` and record only observed facts.

- [ ] **Step 7: Final gates**

Run cold `GameXXKEditor` and `GameXXK` targets, Dialogue/Narrative/SaveMigration/Workbench/Input/LevelFlow suites, JSON validators, harness state validator and real flow. Record unrelated pre-existing art blockers separately. Leave `L_DesktopTrainingHUD` in folded idle state; do not leave an active narrative or 3D map.

---

## Execution notes

- Tasks 2–6 produce a usable task drawer and retired old inputs before the full Narrative stage lands.
- Tasks 7–9 make the 2D presentation and pause path safe before routing real story content.
- Task 10 is the only point where the default `剧情任务` player entry stops map travel and the old 3D-dependent prologue plan is superseded.
- Do not delete Legacy 3D assets/maps in this plan. Remove them only under a later user-approved cleanup after the 2D flow is accepted.
