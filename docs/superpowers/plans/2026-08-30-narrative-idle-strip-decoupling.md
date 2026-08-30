# Narrative / Idle Strip Decoupling Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore the idle strip as an independently owned interaction surface so Narrative presentation, abort, completion, replay recovery, and tutorial settlement never change its fold or notice state.

**Architecture:** `OverlaySurface` and `bNarrativeTabLocked` remain Narrative-owned. Workbench Tab and page modals may close during the handoff, but `bIdleStripFolded`, notice presentation, travel runtime, HUD anchor, and native idle hit regions remain idle-strip-owned. Narrative hides the existing `RootScaleBox` through `ApplyOverlaySurfaceVisibility()` instead of simulating a strip-fold action.

**Tech Stack:** UE 5.8 C++, UMG/Slate, Unreal Automation Tests, project cold-UBT/MCP pipeline. Work directly on root `main` as required by `AGENTS.md`; do not create a worktree and do not use Live Coding or Hot Reload.

---

### Task 1: Add a failing idle-strip ownership contract

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopNarrativeLayerTest.cpp`

- [ ] **Step 1: Add the focused ownership test**

Add a new Automation test beside `WorkbenchTransition` using the existing `MakeWorkbench` fixture:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopNarrativeIdleStripOwnershipTest,
	"GameXXK.DesktopNarrative.Layer.IdleStripOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopNarrativeIdleStripOwnershipTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* UnfoldedSubsystem = nullptr;
	UGameXXKDesktopTrainingWorkbenchWidget* const Unfolded =
		MakeWorkbench(UnfoldedSubsystem);
	TestFalse(TEXT("fixture starts with the ordinary unfolded idle strip"),
		Unfolded->IsIdleStripFoldedForTest());
	Unfolded->HandleActionClicked(683);
	Unfolded->TickForTest(0.0f);
	TestNotNull(TEXT("fixture opens the idle-owned notice settings"),
		Unfolded->WidgetTree
			? Unfolded->WidgetTree->FindWidget(TEXT("DesktopNoticeSettingsPanel"))
			: nullptr);
	TestTrue(TEXT("unfolded fixture enters Narrative"),
		Unfolded->EnterNarrativePresentationForTest());
	TestTrue(TEXT("unfolded fixture exits Narrative"),
		Unfolded->ExitNarrativePresentationToFoldedDesktopForTest());
	TestFalse(TEXT("Narrative never folds an unfolded idle strip"),
		Unfolded->IsIdleStripFoldedForTest());
	TestNotNull(TEXT("Narrative preserves the idle-owned notice settings"),
		Unfolded->WidgetTree
			? Unfolded->WidgetTree->FindWidget(TEXT("DesktopNoticeSettingsPanel"))
			: nullptr);

	UGameXXKMVPSubsystem* FoldedSubsystem = nullptr;
	UGameXXKDesktopTrainingWorkbenchWidget* const Folded =
		MakeWorkbench(FoldedSubsystem);
	Folded->HandleActionClicked(653);
	Folded->TickForTest(0.0f);
	TestTrue(TEXT("fixture explicitly folds through the idle-owned action"),
		Folded->IsIdleStripFoldedForTest());
	TestTrue(TEXT("folded fixture enters Narrative"),
		Folded->EnterNarrativePresentationForTest());
	TestTrue(TEXT("folded fixture exits Narrative"),
		Folded->ExitNarrativePresentationToFoldedDesktopForTest());
	TestTrue(TEXT("Narrative never unfolds a folded idle strip"),
		Folded->IsIdleStripFoldedForTest());
	return true;
}
```

- [ ] **Step 2: Compile and run RED**

Run:

```powershell
py -3 scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 120
& .\scripts\run_mvp_test_suites.ps1 -Suites @("GameXXK.DesktopNarrative.Layer.IdleStripOwnership") -TimeoutSeconds 900
```

Expected: compilation succeeds, then the test fails because current Narrative exit changes an unfolded strip to folded and clears notice settings. If the test passes immediately, inspect the fixture because it is not reproducing the current defect.

---

### Task 2: Remove Narrative ownership of idle-strip state

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopNarrativeInputTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopNarrativeLayerTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKNarrativeAbortRecoveryTest.cpp`

- [ ] **Step 1: Rename the misleading exit API**

Replace `ExitNarrativePresentationToFoldedDesktop` and its test wrapper with:

```cpp
bool ExitNarrativePresentationToDesktop();
bool ExitNarrativePresentationToDesktopForTest()
{
	return ExitNarrativePresentationToDesktop();
}
```

Update every production and test call site. There must be no remaining `ExitNarrativePresentationToFoldedDesktop` identifier.
Update the new ownership test from Task 1 to call `ExitNarrativePresentationToDesktopForTest()` as part of this rename.

- [ ] **Step 2: Make Narrative transitions leave idle-strip state untouched**

In `EnterNarrativePresentation`, keep closing Workbench pages and transient inventory actions, but remove the Narrative-owned write to `bNoticeSettingsOpen`.

In `ExitNarrativePresentationToDesktop`, keep hiding Narrative, restoring the Workbench surface, closing the Workbench Tab/pages, unlocking Tab, and recalculating the native window, but remove both writes below:

```cpp
bIdleStripFolded = true;
bNoticeSettingsOpen = false;
```

Do not replace them with saved Narrative copies. Same-session preservation follows automatically when Narrative does not write the values.

- [ ] **Step 3: Remove the tutorial-settlement fold side effect**

In `ActionCombatTutorialSettlementConfirm`, retain:

```cpp
bCombatBasicsTutorialRouteSurfaceOpen = false;
bBackpackExpanded = false;
UnregisterDesktopStoryGuideTargets();
```

Delete only `bIdleStripFolded = true;` and update the comment so it describes semantic-target cleanup rather than a folded desktop.

- [ ] **Step 4: Run the focused test GREEN**

Run the exact `IdleStripOwnership` suite again. Expected: one test passes, zero fails.

---

### Task 3: Correct legacy tests that encoded the rejected coupling

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopNarrativeLayerTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKNarrativeAbortRecoveryTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopStoryFlowTest.cpp`

- [ ] **Step 1: Replace forced-fold assertions with ownership assertions**

For fixtures whose idle strip starts unfolded, require `IsIdleStripFoldedForTest()` to remain false after completion, abort, failure recovery, and interrupted-load normalization. Replace the old reduced-height assertion with equality to the ordinary idle HUD size. Keep `IsBackpackExpandedForTest() == false`, because closing the Workbench Tab remains intentional.

- [ ] **Step 2: Preserve the explicitly folded case**

Retain the new test's folded fixture so both values are covered. Do not globally invert every fold assertion: only assertions caused by Narrative or tutorial-story transitions change.

- [ ] **Step 3: Run the affected suites**

Run:

```powershell
& .\scripts\run_mvp_test_suites.ps1 -Suites @("GameXXK.DesktopNarrative.Layer", "GameXXK.DesktopNarrative.InputRouting", "GameXXK.Narrative.AbortRecovery", "GameXXK.DesktopStory") -TimeoutSeconds 900
```

Expected: every suite reports `PASS`, at least one test executed per suite, and zero `Result={Fail}` records.

---

### Task 4: Verify the runtime boundary and report the pre-story revisions

**Files:**
- Verify only: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Verify only: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`

- [ ] **Step 1: Static ownership audit**

Run:

```powershell
rg -n "ExitNarrativePresentationToFoldedDesktop|bIdleStripFolded\s*=\s*true|bNoticeSettingsOpen\s*=\s*false" Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp
```

Expected: no old API name; no Narrative/desktop-story call path writes the idle fold flag. Remaining notice writes must belong to explicit Workbench/notice lifecycle paths, not Narrative enter/exit.

- [ ] **Step 2: Cold verification**

Ensure no Unreal Editor is running with dirty packages, then run:

```powershell
py -3 scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 160
```

Expected: UBT reports success without Live Coding or Hot Reload and the short MCP/editor smoke exits cleanly.

- [ ] **Step 3: Record Git ancestry**

Report these distinct baselines:

```text
207f12b  last commit before the current Desktop 2D story/task design documents
16bd8bb  parent of ec59d95, therefore the last commit before current story drawer/runtime implementation
a32d519  parent of 39e0972, therefore the last commit before the broader reusable Dialogue runtime implementation
```

Do not reset or checkout any baseline unless the user explicitly requests it.

- [ ] **Step 4: Preserve unrelated staged and dirty work**

Confirm the three user-staged scrollbar deletions remain staged and no unrelated file is staged, reverted, or committed. Because the runtime files already contain overlapping in-progress changes, do not create a broad runtime commit; hand off the exact focused diff and verification evidence.
