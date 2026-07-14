# Two-Tab Task Panel Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a high-resolution ink-and-parchment task panel to the town HUD, with clickable Main and Side tabs, task tracking, and no Daily or Encounter tab.

**Architecture:** `UGameXXKTaskPanelWidget` owns presentation, filtering, and buttons. `UGameXXKMVPRules` maps existing `QuestState` into a Main task view and persists one tracked task ID. The Side tab is clickable and deliberately renders an empty state until the project has genuine side content. `AGameXXKMVPPlayerController` owns the modal, while `UGameXXKTownOverlayWidget` supplies its entry button.

**Tech Stack:** Unreal Engine 5.8 C++, programmatic UMG, Tencent Docs source crops, built-in image generation for non-text asset upscaling, UE MCP Python import, UE Automation, UBT, PIE/MCP.

---

## File map

| File | Responsibility |
| --- | --- |
| `docs/ui/tasks/source_art/*.png` | 13 approved source atoms. |
| `Content/Python/gamexxk_import_task_ui_assets.py` | Imports all task-panel UI textures. |
| `Source/GameXXK/Public/GameXXKMVPRules.h` | Task types, tracked ID, pure rules API. |
| `Source/GameXXK/Private/GameXXKMVPRules.cpp` | Main task mapping and tracking mutation. |
| `Source/GameXXK/Public/UI/GameXXKTaskPanelWidget.h` | Modal panel public/test API. |
| `Source/GameXXK/Private/UI/GameXXKTaskPanelWidget.cpp` | Programmatic UMG layout and dynamic cards. |
| `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h` | Task panel ownership. |
| `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp` | Modal input and Escape/T handling. |
| `Source/GameXXK/Public/UI/GameXXKTownOverlayWidget.h` | Town task-entry declaration. |
| `Source/GameXXK/Private/UI/GameXXKTownOverlayWidget.cpp` | Town task-entry button binding. |
| `Source/GameXXK/Private/Tests/GameXXKTaskPanelTest.cpp` | Rules and widget regression tests. |
| `Content/Python/gamexxk_probe_real_play_flow.py` | Reports task panel state to PIE. |
| `scripts/gamexxk_real_play_flow_mcp.py` | Exercises task panel in the playable flow. |

## Task 1: Add a rules-level task view and tracked task ID

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKTaskPanelTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`

- [ ] **Step 1: Write the failing rule test.**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKTaskPanelRulesTest,
	"GameXXK.MVP.UI.TaskPanelRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskPanelRulesTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::Town;
	const TArray<FGameXXKTaskView> MainTasks = UGameXXKMVPRules::BuildTaskViews(State, EGameXXKTaskCategory::Main);
	TestEqual(TEXT("main tab has the real QingShan task"), MainTasks.Num(), 1);
	TestEqual(TEXT("task id is stable"), MainTasks[0].Id, UGameXXKMVPRules::TaskQingshanMain());
	TestFalse(TEXT("unaccepted task has no auto navigation"), MainTasks[0].bCanNavigate);
	TestEqual(TEXT("side tab never invents a task"), UGameXXKMVPRules::BuildTaskViews(State, EGameXXKTaskCategory::Side).Num(), 0);
	TestTrue(TEXT("tracking succeeds"), UGameXXKMVPRules::ToggleTrackedTask(State, UGameXXKMVPRules::TaskQingshanMain()));
	TestEqual(TEXT("tracking stores id"), State.TrackedTaskId, UGameXXKMVPRules::TaskQingshanMain());
	TestTrue(TEXT("same task toggles off"), UGameXXKMVPRules::ToggleTrackedTask(State, UGameXXKMVPRules::TaskQingshanMain()));
	TestTrue(TEXT("toggle clears id"), State.TrackedTaskId.IsNone());
	State.QuestState = EGameXXKQuestState::Accepted;
	const FGameXXKTaskView Accepted = UGameXXKMVPRules::BuildTaskViews(State, EGameXXKTaskCategory::Main)[0];
	TestTrue(TEXT("accepted task points to real town exit"), Accepted.bCanNavigate);
	TestEqual(TEXT("town exit target is stable"), Accepted.NavigationTarget, FName(TEXT("QingshanInn_TownExit")));
	return true;
}
```

- [ ] **Step 2: Run the test to prove the symbols do not exist.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.UI.TaskPanelRules;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: compile failure for `EGameXXKTaskCategory`, `FGameXXKTaskView`, `BuildTaskViews`, and `ToggleTrackedTask`.

- [ ] **Step 3: Add the data types and rule API.**

```cpp
UENUM(BlueprintType)
enum class EGameXXKTaskCategory : uint8 { Main, Side };

USTRUCT(BlueprintType)
struct FGameXXKTaskReward
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere) int32 Gold = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere) int32 Experience = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere) int32 Token = 0;
};

USTRUCT(BlueprintType)
struct FGameXXKTaskView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere) FName Id;
	UPROPERTY(BlueprintReadWrite, EditAnywhere) EGameXXKTaskCategory Category = EGameXXKTaskCategory::Main;
	UPROPERTY(BlueprintReadWrite, EditAnywhere) FText Title;
	UPROPERTY(BlueprintReadWrite, EditAnywhere) FText Description;
	UPROPERTY(BlueprintReadWrite, EditAnywhere) int32 ProgressCurrent = 0;
	UPROPERTY(BlueprintReadWrite, EditAnywhere) int32 ProgressTarget = 1;
	UPROPERTY(BlueprintReadWrite, EditAnywhere) FGameXXKTaskReward Reward;
	UPROPERTY(BlueprintReadWrite, EditAnywhere) bool bCanNavigate = false;
	UPROPERTY(BlueprintReadWrite, EditAnywhere) FName NavigationTarget;
};
```

Add `FName TrackedTaskId = NAME_None;` to `FGameXXKRuntimeState`, then declare:

```cpp
static FName TaskQingshanMain();
static TArray<FGameXXKTaskView> BuildTaskViews(const FGameXXKRuntimeState& State, EGameXXKTaskCategory Category);
static bool ToggleTrackedTask(UPARAM(ref) FGameXXKRuntimeState& State, FName TaskId);
```

- [ ] **Step 4: Map only the existing Main quest.**

```cpp
FName UGameXXKMVPRules::TaskQingshanMain() { return FName(TEXT("Task.QingshanMain")); }

TArray<FGameXXKTaskView> UGameXXKMVPRules::BuildTaskViews(const FGameXXKRuntimeState& State, EGameXXKTaskCategory Category)
{
	if (Category == EGameXXKTaskCategory::Side) return {};
	FGameXXKTaskView Task;
	Task.Id = TaskQingshanMain();
	Task.Title = NSLOCTEXT("GameXXKTask", "QingshanTitle", "初入江湖");
	Task.Reward = {500, 1200, 10};
	Task.ProgressTarget = 1;
	if (State.QuestState == EGameXXKQuestState::NotAccepted)
	{
		Task.Description = NSLOCTEXT("GameXXKTask", "BeforeAccept", "前往青山镇寻找引路人");
		Task.NavigationTarget = FName(TEXT("QingshanTown_QuestNpc"));
	}
	else if (State.QuestState == EGameXXKQuestState::Accepted)
	{
		Task.Description = NSLOCTEXT("GameXXKTask", "Accepted", "与引路人同行，前往北门出口");
		Task.ProgressCurrent = 1;
		Task.bCanNavigate = true;
		Task.NavigationTarget = FName(TEXT("QingshanInn_TownExit"));
	}
	else { Task.Description = NSLOCTEXT("GameXXKTask", "Complete", "黄山之行已经完成"); Task.ProgressCurrent = 1; }
	return {Task};
}

bool UGameXXKMVPRules::ToggleTrackedTask(FGameXXKRuntimeState& State, FName TaskId)
{
	if (TaskId != TaskQingshanMain()) return false;
	State.TrackedTaskId = State.TrackedTaskId == TaskId ? NAME_None : TaskId;
	return true;
}
```

`FGameXXKSaveState::RuntimeState` already stores the full runtime state, so do not add a redundant save field.

- [ ] **Step 5: Re-run the test and commit.**

Run the Step 2 command. Expected: `TaskPanelRules` passes. Commit only these files as `feat: add task panel state rules`.

## Task 2: Crop, restore, and import the 13 UI atoms

**Files:**
- Create: `docs/ui/tasks/source_art/task_panel_frame.png`
- Create: `docs/ui/tasks/source_art/task_panel_back_arrow.png`
- Create: `docs/ui/tasks/source_art/task_panel_title.png`
- Create: `docs/ui/tasks/source_art/task_tab_selected.png`
- Create: `docs/ui/tasks/source_art/task_tab_normal.png`
- Create: `docs/ui/tasks/source_art/task_tab_label_main.png`
- Create: `docs/ui/tasks/source_art/task_tab_label_side.png`
- Create: `docs/ui/tasks/source_art/task_entry_frame.png`
- Create: `docs/ui/tasks/source_art/task_action_track.png`
- Create: `docs/ui/tasks/source_art/task_action_go.png`
- Create: `docs/ui/tasks/source_art/reward_coin.png`
- Create: `docs/ui/tasks/source_art/reward_exp.png`
- Create: `docs/ui/tasks/source_art/reward_token.png`
- Create: `Content/Python/gamexxk_import_task_ui_assets.py`

- [ ] **Step 1: Crop against the Tencent Docs source, not the illustrative screenshot.**

Use `docs/reference-assets/2026-07-14-tencent-town-ui-source.png` as the master source. All expandable backgrounds retain 24 px of clean edge padding and contain no dynamic title, description, reward number, or progress value.

- [ ] **Step 2: Restore resolution non-destructively with built-in image generation.**

For frame/card/tab/button/reward crops use: `preserve the supplied ink-and-parchment UI atom exactly; isolate it; retain empty center and clean 9-slice edges; add no glyph, icon, border, watermark, or background.` Save only the approved high-resolution result to the exact filenames above.

For the fixed Chinese title and tab labels, reject any generated result that changes a glyph. Keep the sharpest untouched crop rather than accepting altered text. Never overwrite `docs/reference-assets/*`.

- [ ] **Step 3: Implement and run the import script.**

Use `Content/Python/gamexxk_import_inventory_ui_assets.py` as the implementation template. Set `SOURCE_DIR = PROJECT_ROOT / "docs" / "ui" / "tasks" / "source_art"`, `DESTINATION = "/Game/GameXXK/UI/Tasks/Textures"`, and import these names:

```python
IMPORTS = [
 ("task_panel_frame.png", "T_TaskPanelFrame"), ("task_panel_back_arrow.png", "T_TaskPanelBackArrow"),
 ("task_panel_title.png", "T_TaskPanelTitle"), ("task_tab_selected.png", "T_TaskTabSelected"),
 ("task_tab_normal.png", "T_TaskTabNormal"), ("task_tab_label_main.png", "T_TaskTabLabelMain"),
 ("task_tab_label_side.png", "T_TaskTabLabelSide"), ("task_entry_frame.png", "T_TaskEntryFrame"),
 ("task_action_track.png", "T_TaskActionTrack"), ("task_action_go.png", "T_TaskActionGo"),
 ("reward_coin.png", "T_RewardCoin"), ("reward_exp.png", "T_RewardExp"), ("reward_token.png", "T_RewardToken"),
]
```

The helper must apply `TMGS_NO_MIPMAPS`, `TC_EDITOR_ICON`, `srgb=True`, and `never_stream=True` to every `Texture2D`.

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -run=pythonscript -script='D:/UE5 demo/GameXXK/Content/Python/gamexxk_import_task_ui_assets.py' -Unattended -NoSplash -NoSound -NullRHI -log -stdout -FullStdOutLogOutput
```

Expected: JSON `"imported_count": 13`. Commit source images, script, and their UAssets as `feat: add task panel UI atoms`.

## Task 3: Build the data-driven two-tab modal

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKTaskPanelWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKTaskPanelWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTaskPanelTest.cpp`

- [ ] **Step 1: Add the failing widget assertions.**

```cpp
UGameXXKTaskPanelWidget* Panel = NewObject<UGameXXKTaskPanelWidget>();
Panel->SetMVPSubsystem(Subsystem);
Panel->TakeWidget();
TestTrue(TEXT("opens in main"), Panel->OpenTaskPanel());
TestEqual(TEXT("main category selected"), Panel->GetSelectedCategoryForTest(), EGameXXKTaskCategory::Main);
TestEqual(TEXT("main task is visible"), Panel->GetVisibleTaskCountForTest(), 1);
TestTrue(TEXT("frame resolves texture"), Panel->GetPanelFrameResourcePathForTest().Contains(TEXT("T_TaskPanelFrame")));
TestTrue(TEXT("side tab is clickable"), Panel->SelectCategoryForTest(EGameXXKTaskCategory::Side));
TestEqual(TEXT("side list is empty instead of fabricated"), Panel->GetVisibleTaskCountForTest(), 0);
TestTrue(TEXT("main tab restores"), Panel->SelectCategoryForTest(EGameXXKTaskCategory::Main));
TestTrue(TEXT("tracking changes runtime"), Panel->ToggleVisibleTaskTrackingForTest(0));
TestEqual(TEXT("runtime stores task id"), Subsystem->GetRuntimeState().TrackedTaskId, UGameXXKMVPRules::TaskQingshanMain());
```

- [ ] **Step 2: Compile the failing widget test using the Task 1 command.**

Expected: `UGameXXKTaskPanelWidget` and its API are unknown.

- [ ] **Step 3: Implement one focused widget with this public contract.**

```cpp
bool OpenTaskPanel();
bool CloseTaskPanel();
bool SelectCategoryForTest(EGameXXKTaskCategory Category);
EGameXXKTaskCategory GetSelectedCategoryForTest() const;
int32 GetVisibleTaskCountForTest() const;
bool ToggleVisibleTaskTrackingForTest(int32 VisibleIndex);
FString GetPanelFrameResourcePathForTest() const;
```

Build `TaskPanelRoot`, `TaskPanelBackdrop`, a 1120×690 `TaskPanelFrame`, `TaskPanelBackButton`, Main/Side tab buttons, and a `ScrollBox` of `TaskEntryFrame` cards. Use Box brushes for the frame/card/tabs/actions. Titles, descriptions, reward numbers, progress, action text, and `暂无支线委托` are `UTextBlock`s only.

- [ ] **Step 4: Wire state refresh and actions.**

`RefreshFromState()` assigns `VisibleTasks = UGameXXKMVPRules::BuildTaskViews(State, SelectedCategory)`. Side with zero tasks shows the empty text block. The task action calls `ToggleTrackedTask`; it never calls `AcceptQuest` or `OpenDungeonFromTownExit`. A usable `前往` state sets the same tracked task and closes the panel, leaving the player at their current world position.

- [ ] **Step 5: Run the focused test and commit.**

Expected: `TaskPanelRules` passes and tests the real Main task, empty Side state, texture path, and tracking. Commit as `feat: add two-tab task panel widget`.

## Task 4: Connect modal ownership and a town-HUD entry

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKTownOverlayWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKTownOverlayWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`

- [ ] **Step 1: Add failing controller assertions.**

```cpp
TestNotNull(TEXT("controller owns task panel"), PlayerController->GetTaskPanelWidgetForTest());
TestTrue(TEXT("town HUD has task button"), PlayerController->GetTownOverlayWidgetForTest()->HasTaskButtonForTest());
TestTrue(TEXT("controller opens task panel"), PlayerController->OpenTaskPanel());
TestTrue(TEXT("task panel locks town input"), PlayerController->IsTaskPanelModalInputLockedForTest());
TestTrue(TEXT("Esc closes task panel"), PlayerController->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Escape, IE_Pressed, 1.0f)));
TestFalse(TEXT("Esc restores town input"), PlayerController->IsTaskPanelModalInputLockedForTest());
```

- [ ] **Step 2: Run `GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets` and observe the missing API failure.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

- [ ] **Step 3: Add controller ownership.**

Add class/property/accessors analogous to `InventoryWindowWidget`: `EnsureTaskPanelWidget`, `OpenTaskPanel`, `CloseTaskPanel`, `GetTaskPanelWidgetForTest`, and `IsTaskPanelModalInputLockedForTest`. Add at Z-order 140, beneath the quest dialog (160). In `InputKey`, handle Esc in the order quest dialog → task panel → inventory → battle, and bind T to toggle the panel only in town. Opening sets `SetIgnoreMoveInput(true)`; closing sets it false; `ApplyPlayerFlowInputMode` focuses the open task panel.

- [ ] **Step 4: Add the HUD button without altering the command router.**

Create `TaskButton` beside the existing `背包` button, bind `HandleTaskClicked`, and use:

```cpp
void UGameXXKTownOverlayWidget::HandleTaskClicked()
{
	if (AGameXXKMVPPlayerController* Controller = ResolveMVPPlayerController())
	{
		Controller->OpenTaskPanel();
	}
}
```

Expose `HasTaskButtonForTest()` from the button pointer. Do not expose a HUD command that accepts the quest.

- [ ] **Step 5: Re-run both focused tests and commit.**

Expected: `PlayerControllerOwnsFlowWidgets` and `TaskPanelRules` pass. Commit as `feat: open task panel from town HUD`.

## Task 5: Extend PIE evidence and run the full flow

**Files:**
- Modify: `Content/Python/gamexxk_probe_real_play_flow.py`
- Modify: `scripts/gamexxk_real_play_flow_mcp.py`

- [ ] **Step 1: Add task panel state to the probe.**

Add `("task_panel", "get_task_panel_widget_for_test")` to the controller widget summary. Report panel open state, selected category, visible task count, and `TrackedTaskId`; add `--task-panel-command` supporting `OpenTaskPanel`, `SelectMain`, `SelectSide`, and `CloseTaskPanel` only.

- [ ] **Step 2: Update the stale harness camera range without touching the Blueprint.**

Keep the checks for `TopDownCamera`, `CameraBoom`, perspective, absolute rotation, and disabled collision. Replace the old 800/-60 threshold with `900.0 <= target_arm_length <= 1100.0` and `-40.0 <= pitch <= -20.0`, which accepts the user-tuned 1000/-30 camera but writes no asset.

- [ ] **Step 3: Insert real task-panel acceptance after town HUD validation.**

Open the panel through the controller probe, wait for it to report open, capture `Saved/Codex/real_flow_task_panel_main.png`, select Side and assert zero cards plus the empty-state widget, return to Main and assert one card, then close with Esc and assert movement input unlocks. Preserve the existing `F → quest dialog → Slate accept → follower/save` sequence after this segment.

- [ ] **Step 4: Run the full harness.**

```powershell
python scripts/gamexxk_real_play_flow_mcp.py --timeout 45
```

Expected: JSON `"ok": true` and the task-panel, quest-dialog, route-map, and battle screenshots under `Saved/Codex`.

- [ ] **Step 5: Inspect the task-panel screenshot, build, and commit.**

Run the three focused tests plus a disk build:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.UI.TaskPanelRules+GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets+GameXXK.MVP.UI.WidgetBasesDriveRules;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload -NoLiveCoding -NoHotReloadFromIDE
```

Save dirty UE packages through UE MCP before any restart. Stage only task-panel files; do not stage user-tuned map, character, camera, PaperZD, or occlusion assets. Commit probe/harness updates as `test: cover task panel in PIE`.
