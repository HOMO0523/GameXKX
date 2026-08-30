# Desktop HUD Drag Stability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the desktop idle strip track the pressed pointer one-for-one during a drag without oscillation, while preserving its existing controls, boundary clamp, and saved position.

**Architecture:** Keep hit testing and native-window placement unchanged, but replace the moving-window-relative drag calculation with an immutable drag-start snapshot. A pure layout helper converts a physical cursor delta into a normalized anchor; the workbench samples `GetCursorPos` only during its locally captured drag events and never combines current Slate Geometry with `GetWindowRect`.

**Tech Stack:** Unreal Engine 5.8 C++, Slate/UMG input events, Win32 cursor sampling, Unreal Automation Tests, Python source-policy regression test, cold UBT.

---

## File map and ownership constraints

- Modify `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`: add the behavior-first automation regression for pure drag math.
- Modify `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h`: declare the pure drag resolver.
- Modify `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp`: implement the pure drag resolver without platform or widget state.
- Modify `scripts/test_desktop_mouse_hook_policy.py`: add a source-policy regression that rejects moving-window-relative coordinates in the drag updater.
- Modify `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`: replace the pointer-offset state with immutable drag-start state and declare cursor sampling.
- Modify `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`: snapshot the drag start and resolve each move from global physical cursor delta.

The layout header, layout implementation, and workbench implementation already contain unrelated unstaged changes. Preserve them byte-for-byte outside the planned hunks. Do not stage or commit the runtime/test files automatically; leave the final focused diff for the user because a normal file-level commit would also capture pre-existing work. The three user-staged scrollbar deletions must remain staged throughout.

### Task 1: Establish the safe baseline and add the failing behavior test

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp:1497`
- Read only: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h`
- Read only: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp`

- [ ] **Step 1: Confirm no Unreal process is holding the editor modules**

Run:

```powershell
Get-CimInstance Win32_Process |
    Where-Object { $_.Name -like 'UnrealEditor*' } |
    Select-Object ProcessId, Name, CommandLine
```

Expected: no rows. If an editor is present, stop here; save dirty packages through the project UE MCP workflow before closing it. Never force-close an editor that may hold unsaved packages.

- [ ] **Step 2: Record the targeted dirty baseline**

Run:

```powershell
git status --short -- `
    Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp `
    Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h `
    Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp `
    Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h `
    Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp `
    scripts/test_desktop_mouse_hook_policy.py

git diff --cached --name-status -- `
    Content/GameXXK/UI/MasterV2/Approved/T_MasterV2_BackpackScrollbarRight.uasset `
    SourceArt/UI/PSD/gamexxk-v4/ui-master/Assets/LayoutAssets/03_主角背包_inventory_scrollbar_right.png `
    SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeApproved/T_MasterV2_BackpackScrollbarRight.png
```

Expected: the layout `.h/.cpp` and workbench `.cpp` are already modified, the Python policy test is untracked, and all three scrollbar paths remain staged as `D`.

- [ ] **Step 3: Verify the existing no-global-hook policy is green**

Run:

```powershell
python scripts/test_desktop_mouse_hook_policy.py
```

Expected: `Ran 2 tests` and `OK`.

- [ ] **Step 4: Add the failing pure drag-math automation test**

Insert this test immediately before `FGameXXKDesktopTrainingStablePresentationScaleTest`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingStableDragAnchorTest,
	"GameXXK.DesktopTraining.Workbench.StableDragAnchor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingStableDragAnchorTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopTrainingLayout;
	const FVector2D PhysicalWorkArea(1920.0f, 1020.0f);
	const FVector2D CollapsedStripSize(1038.0f, 202.0f);
	const FVector2D DragStartAnchor(0.4f, 0.6f);
	const FVector2D DragStartPointer(600.0f, 700.0f);

	TestEqual(
		TEXT("a stationary pointer preserves the exact drag-start anchor"),
		ResolveDesktopHudDragAnchor(
			DragStartAnchor,
			DragStartPointer,
			DragStartPointer,
			PhysicalWorkArea,
			CollapsedStripSize),
		DragStartAnchor);

	const FVector2D MovedAnchor = ResolveDesktopHudDragAnchor(
		DragStartAnchor,
		DragStartPointer,
		DragStartPointer + FVector2D(88.2f, -81.8f),
		PhysicalWorkArea,
		CollapsedStripSize);
	TestTrue(TEXT("horizontal physical delta maps one-for-one into anchor travel"),
		FMath::IsNearlyEqual(MovedAnchor.X, 0.5f, 0.001f));
	TestTrue(TEXT("vertical physical delta maps one-for-one into anchor travel"),
		FMath::IsNearlyEqual(MovedAnchor.Y, 0.5f, 0.001f));
	TestEqual(
		TEXT("repeating the same pointer sample is deterministic"),
		ResolveDesktopHudDragAnchor(
			DragStartAnchor,
			DragStartPointer,
			DragStartPointer + FVector2D(88.2f, -81.8f),
			PhysicalWorkArea,
			CollapsedStripSize),
		MovedAnchor);

	TestEqual(
		TEXT("large negative movement clamps to the work-area origin"),
		ResolveDesktopHudDragAnchor(
			DragStartAnchor,
			DragStartPointer,
			DragStartPointer - FVector2D(10000.0f, 10000.0f),
			PhysicalWorkArea,
			CollapsedStripSize),
		FVector2D::ZeroVector);
	TestEqual(
		TEXT("large positive movement clamps to the far work-area edge"),
		ResolveDesktopHudDragAnchor(
			DragStartAnchor,
			DragStartPointer,
			DragStartPointer + FVector2D(10000.0f, 10000.0f),
			PhysicalWorkArea,
			CollapsedStripSize),
		FVector2D(1.0f, 1.0f));

	const FVector2D DegenerateAxisAnchor = ResolveDesktopHudDragAnchor(
		DragStartAnchor,
		DragStartPointer,
		DragStartPointer + FVector2D(100.0f, 81.8f),
		FVector2D(800.0f, 1020.0f),
		CollapsedStripSize);
	TestTrue(TEXT("an axis with no available travel resolves to its canonical origin"),
		FMath::IsNearlyZero(DegenerateAxisAnchor.X));
	TestTrue(TEXT("a valid axis remains responsive beside a degenerate axis"),
		FMath::IsNearlyEqual(DegenerateAxisAnchor.Y, 0.7f, 0.001f));
	return true;
}
```

- [ ] **Step 5: Run cold UBT and observe the expected RED failure**

Run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' `
    GameXXKEditor Win64 Development `
    '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' `
    -WaitMutex -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2
```

Expected: compilation fails because `ResolveDesktopHudDragAnchor` is not declared. A failure caused by any other symbol, stale editor process, or unrelated file is not the intended RED and must be resolved before continuing.

### Task 2: Implement and verify the pure drag resolver

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h:128`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp:249`
- Test: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Declare the resolver next to the other coordinate helpers**

Add after `PhysicalPixelsToSlateHost` in `GameXXKDesktopTrainingLayout.h`:

```cpp
	GAMEXXK_API FVector2D ResolveDesktopHudDragAnchor(
		const FVector2D& DragStartNormalizedAnchor,
		const FVector2D& DragStartPointerScreen,
		const FVector2D& CurrentPointerScreen,
		const FVector2D& PhysicalWorkAreaSize,
		const FVector2D& CollapsedStripSize);
```

- [ ] **Step 2: Implement only the deterministic axis calculation**

Add after `PhysicalPixelsToSlateHost` in `GameXXKDesktopTrainingLayout.cpp`:

```cpp
	FVector2D ResolveDesktopHudDragAnchor(
		const FVector2D& DragStartNormalizedAnchor,
		const FVector2D& DragStartPointerScreen,
		const FVector2D& CurrentPointerScreen,
		const FVector2D& PhysicalWorkAreaSize,
		const FVector2D& CollapsedStripSize)
	{
		const FVector2D SafeStartAnchor(
			FMath::Clamp(DragStartNormalizedAnchor.X, 0.0f, 1.0f),
			FMath::Clamp(DragStartNormalizedAnchor.Y, 0.0f, 1.0f));
		const FVector2D PointerDelta = CurrentPointerScreen - DragStartPointerScreen;
		const auto ResolveAxis = [](const float StartAnchor,
			const float Delta,
			const float HostExtent,
			const float StripExtent)
		{
			const float AvailableTravel = HostExtent - StripExtent;
			if (AvailableTravel <= KINDA_SMALL_NUMBER)
			{
				return 0.0f;
			}
			return FMath::Clamp(StartAnchor + Delta / AvailableTravel, 0.0f, 1.0f);
		};
		return FVector2D(
			ResolveAxis(
				SafeStartAnchor.X,
				PointerDelta.X,
				PhysicalWorkAreaSize.X,
				CollapsedStripSize.X),
			ResolveAxis(
				SafeStartAnchor.Y,
				PointerDelta.Y,
				PhysicalWorkAreaSize.Y,
				CollapsedStripSize.Y));
	}
```

- [ ] **Step 3: Run cold UBT and verify GREEN compilation**

Run the same cold UBT command from Task 1, Step 5.

Expected: `Result: Succeeded` with no Live Coding or Hot Reload verification.

- [ ] **Step 4: Run only the new headless behavior test**

Run:

```powershell
& scripts/run_mvp_test_suites.ps1 `
    -Suites @('GameXXK.DesktopTraining.Workbench.StableDragAnchor') `
    -TimeoutSeconds 300
```

Expected: `StableDragAnchor: 1 passed, 0 failed` and `Suites passed: 1 / 1`. This command uses `-unattended -nullrhi`; it does not open an interactive window, move the player's mouse, or synthesize clicks.

### Task 3: Add the failing coordinate-source policy test

**Files:**
- Modify: `scripts/test_desktop_mouse_hook_policy.py`
- Read only: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp:8076-8118`

- [ ] **Step 1: Add a source-policy test for the drag update body**

Add this method to `DesktopMouseHookPolicyTest`:

```python
    def test_drag_update_uses_immutable_screen_delta(self) -> None:
        source = WORKBENCH_CPP.read_text(encoding="utf-8")
        drag_body = source.split(
            "void UGameXXKDesktopTrainingWorkbenchWidget::"
            "UpdateDesktopOverlayAnchorFromPointer",
            1,
        )[1].split(
            "void UGameXXKDesktopTrainingWorkbenchWidget::"
            "UpdateExpansionDirectionFromNativeWindow",
            1,
        )[0]

        for forbidden in (
            "HostGeometry.AbsoluteToLocal",
            "GetWindowRect",
            "DesktopHudDragPointerOffset",
        ):
            self.assertNotIn(forbidden, drag_body)

        self.assertIn("ResolveDesktopHudDragAnchor(", drag_body)
        self.assertIn("DesktopHudDragStartPointerScreen", drag_body)
        self.assertIn("DesktopHudDragStartNormalizedAnchor", drag_body)
```

- [ ] **Step 2: Run the policy test and observe the expected RED failure**

Run:

```powershell
python scripts/test_desktop_mouse_hook_policy.py
```

Expected: the two existing tests pass and the new test fails because the current updater still contains `HostGeometry.AbsoluteToLocal`, `GetWindowRect`, and `DesktopHudDragPointerOffset` and does not call `ResolveDesktopHudDragAnchor`.

### Task 4: Integrate immutable drag-start state into the workbench

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h:547-549,973`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp:1518-1611,8076-8118`
- Test: `scripts/test_desktop_mouse_hook_policy.py`
- Test: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Replace the drag updater declaration and pointer-offset field**

Replace the current updater declaration with:

```cpp
	bool TryGetDesktopHudPointerScreenPosition(FVector2D& OutPointerScreen) const;
	void UpdateDesktopOverlayAnchorFromPointer();
```

Replace `DesktopHudDragPointerOffset` with:

```cpp
	FVector2D DesktopHudDragStartPointerScreen = FVector2D::ZeroVector;
	FVector2D DesktopHudDragStartNormalizedAnchor = FVector2D::ZeroVector;
```

- [ ] **Step 2: Snapshot the physical cursor and normalized anchor on mouse-down**

Replace the existing `if (!bIdleControl)` body with:

```cpp
			if (!bIdleControl)
			{
				FVector2D PointerScreen;
				if (TryGetDesktopHudPointerScreenPosition(PointerScreen))
				{
					bDesktopHudDragging = true;
					DesktopHudDragStartPointerScreen = PointerScreen;
					DesktopHudDragStartNormalizedAnchor = DesktopWindowPositionNormalized;
					return FReply::Handled().CaptureMouse(TakeWidget());
				}
			}
```

The existing `InGeometry.AbsoluteToLocal(...)` remains for stationary hit testing only; do not remove or rewrite the control hit regions.

- [ ] **Step 3: Remove moving Geometry arguments from captured move and release**

In `NativeOnMouseButtonUp`, replace the updater call with:

```cpp
		UpdateDesktopOverlayAnchorFromPointer();
```

In `NativeOnMouseMove`, replace the updater call with the same zero-argument call. Keep capture release, `bDesktopHudDragging`, and `SaveDesktopNativeWindowPosition()` behavior unchanged.

- [ ] **Step 4: Add local Win32 cursor sampling**

Insert immediately before the updater implementation:

```cpp
bool UGameXXKDesktopTrainingWorkbenchWidget::TryGetDesktopHudPointerScreenPosition(
	FVector2D& OutPointerScreen) const
{
#if PLATFORM_WINDOWS
	POINT CursorPoint = {};
	if (::GetCursorPos(&CursorPoint))
	{
		OutPointerScreen = FVector2D(
			static_cast<float>(CursorPoint.x),
			static_cast<float>(CursorPoint.y));
		return true;
	}
#endif
	OutPointerScreen = FVector2D::ZeroVector;
	return false;
}
```

This call is permitted only from the widget's local mouse-down/move/up flow. Do not add a timer, raw-input listener, automation driver, `WH_MOUSE_LL`, or any other global hook.

- [ ] **Step 5: Replace the updater with the pure resolver**

Replace the complete existing `UpdateDesktopOverlayAnchorFromPointer` implementation with:

```cpp
void UGameXXKDesktopTrainingWorkbenchWidget::UpdateDesktopOverlayAnchorFromPointer()
{
	if (PresentationMode != EGameXXKDesktopHudPresentationMode::DesktopWindow)
	{
		return;
	}
	const FVector2D HostSize = DesktopOverlayHostSize;
	if (HostSize.X <= 1.0f || HostSize.Y <= 1.0f)
	{
		return;
	}
	FVector2D CurrentPointerScreen;
	if (!TryGetDesktopHudPointerScreenPosition(CurrentPointerScreen))
	{
		return;
	}
	const float Scale = bDesktopResolvedMetricsValid
		? DesktopResolvedMetrics.Scale
		: GameXXKDesktopTrainingLayout::ComputeEffectiveHudScale(
			HostSize,
			HudScalePercent);
	const FVector2D CollapsedStripSize =
		GameXXKDesktopTrainingLayout::GetCollapsedHudLogicalSize() * Scale;
	DesktopWindowPositionNormalized =
		GameXXKDesktopTrainingLayout::ResolveDesktopHudDragAnchor(
			DesktopHudDragStartNormalizedAnchor,
			DesktopHudDragStartPointerScreen,
			CurrentPointerScreen,
			HostSize,
			CollapsedStripSize);
	UpdateDesktopOverlayPlacement(HostSize);
	bDesktopNativeLayoutDirty = true;
}
```

Do not change `ApplyDesktopNativeWindowLayout`, `SetWindowPos` flags, passthrough polling, visibility, folding, story, task, town, or battle code in this task.

- [ ] **Step 6: Run both source-policy and behavior tests**

Run:

```powershell
python scripts/test_desktop_mouse_hook_policy.py
```

Expected: `Ran 3 tests` and `OK`.

Then run the cold UBT command from Task 1, Step 5. Expected: `Result: Succeeded`.

Then run:

```powershell
& scripts/run_mvp_test_suites.ps1 `
    -Suites @(
        'GameXXK.DesktopTraining.Workbench.StableDragAnchor',
        'GameXXK.DesktopTraining.Workbench'
    ) `
    -TimeoutSeconds 1500
```

Expected: both suites report `PASS`, zero failed tests, and `Suites passed: 2 / 2`. These are headless commandlet tests and do not operate the user's pointer.

### Task 5: Verify scope, preserve staged state, and hand off a clean manual test

**Files:**
- Verify only: all six modified implementation/test paths
- Preserve: the three user-staged scrollbar deletions

- [ ] **Step 1: Check formatting and forbidden residue**

Run:

```powershell
git diff --check -- `
    Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp `
    Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h `
    Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp `
    Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h `
    Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp `
    scripts/test_desktop_mouse_hook_policy.py

rg -n "WH_MOUSE_LL|SetWindowsHookExW|DesktopHudDragPointerOffset" `
    Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp `
    Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h
```

Expected: `git diff --check` prints nothing and `rg` finds nothing.

- [ ] **Step 2: Confirm the focused runtime diff and staged deletions**

Run:

```powershell
git diff -- `
    Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp `
    Source/GameXXK/Public/UI/GameXXKDesktopTrainingLayout.h `
    Source/GameXXK/Private/UI/GameXXKDesktopTrainingLayout.cpp `
    Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h `
    Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp `
    scripts/test_desktop_mouse_hook_policy.py

git diff --cached --name-status -- `
    Content/GameXXK/UI/MasterV2/Approved/T_MasterV2_BackpackScrollbarRight.uasset `
    SourceArt/UI/PSD/gamexxk-v4/ui-master/Assets/LayoutAssets/03_主角背包_inventory_scrollbar_right.png `
    SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeApproved/T_MasterV2_BackpackScrollbarRight.png
```

Expected: only the planned drag hunks are new relative to the recorded dirty baseline, and the three scrollbar deletions still show `D`. Do not run `git add` or commit the dirty shared runtime files.

- [ ] **Step 3: Launch one ordinary visible manual-test window**

Run:

```powershell
$GameXXKManualProcess = Start-Process `
    -FilePath 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
    -ArgumentList @(
        '"D:\UE5 demo\GameXXK\GameXXK.uproject"',
        '/Game/GameXXK/Maps/L_DesktopTrainingHUD',
        '-game',
        '-windowed',
        '-ResX=1280',
        '-ResY=720',
        '-NoSplash'
    ) `
    -PassThru

Get-CimInstance Win32_Process -Filter "ProcessId=$($GameXXKManualProcess.Id)" |
    Select-Object ProcessId, CommandLine
```

Expected: the command line contains the canonical 2D map and `-game`, and contains no `Automation`, runner script, synthesized click, or real-flow harness argument. Leave this window open for the user; do not click, drag, minimize, or close it automatically.

- [ ] **Step 4: Ask the user to perform the final interaction acceptance**

Ask the user to verify:

1. Pressing a blank strip area does not snap the strip.
2. Slow, fast, horizontal, vertical, and diagonal drags keep the pressed point under the pointer.
3. Holding the pointer still does not oscillate.
4. Folded and expanded states both drag smoothly.
5. Work-area edges clamp without rebound.
6. Buttons remain clickable and the global mouse remains responsive.
7. Close/relaunch restores the released position.

Do not claim the interaction bug fixed until this manual acceptance passes.
