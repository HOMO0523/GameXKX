# Idle Strip Nonblocking Guide Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every visible idle-strip and Workbench control usable regardless of story task, tutorial-route, or Guide state, while retaining nonblocking story observation and Narrative-only fullscreen presentation.

**Architecture:** The idle strip executes its own actions without consulting Guide authority. Tutorial route actions keep their dedicated validation, but unrelated ActionIds fall through to the normal Workbench handler. Workbench Guides are Soft observers that receive post-success events; only `NarrativeFullscreen` may resize the native overlay or consume the hidden Workbench shortcuts.

**Tech Stack:** UE 5.8 C++, UMG/Slate, JSON-authored Guide assets, Unreal Automation Tests, cold UBT and project MCP/import scripts. Work on root `main`; no worktree, UnrealBridge, Live Coding, or Hot Reload.

---

### Task 1: Prove tutorial and Guide states currently block the idle strip

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopStoryFlowTest.cpp`

- [ ] **Step 1: Add the route-surface regression assertion**

In `FGameXXKDesktopStoryCombatRouteProductionSurfaceTest`, immediately after `OpenCombatBasicsTutorialRouteContinuation()` succeeds, execute the real fold ActionId and require the idle state to change:

```cpp
TestFalse(TEXT("route fixture begins with unfolded idle strip"),
	Workbench->IsIdleStripFoldedForTest());
Workbench->HandleActionClicked(653);
Workbench->TickForTest(0.0f);
TestTrue(TEXT("tutorial route never blocks the idle fold control"),
	Workbench->IsIdleStripFoldedForTest());
Workbench->HandleActionClicked(653);
Workbench->TickForTest(0.0f);
```

- [ ] **Step 2: Replace the FirstJourney blocking expectations**

In `FGameXXKDesktopStoryFirstJourneyGuideProfilesTest`, capture the ordinary native HUD size before presenting the Guide, then require:

```cpp
const FVector2D HudSizeBeforeGuide = NewWorkbench->GetDesktopWindowSizeForHost();
TestTrue(TEXT("new-player imported guide starts"),
	NewWorkbench->PresentFirstJourneyGuideFromAuthority());
TestEqual(TEXT("Workbench guide keeps the ordinary native host"),
	NewWorkbench->GetDesktopWindowSizeForHost(), HudSizeBeforeGuide);
TestFalse(TEXT("new-player Workbench guide owns no input token"),
	NewWorkbench->IsGuideInputTokenHeldForTest());
TestTrue(TEXT("new-player Workbench guide allows unrelated actions"),
	FGameXXKGuideTargetRegistry::Get().IsActionAllowed(
		TEXT("Action.Desktop.Unrelated")));
NewWorkbench->HandleActionClicked(653);
NewWorkbench->TickForTest(0.0f);
TestTrue(TEXT("active Workbench guide never blocks idle folding"),
	NewWorkbench->IsIdleStripFoldedForTest());
NewWorkbench->HandleActionClicked(653);
NewWorkbench->TickForTest(0.0f);
```

Keep the semantic Tab → Training → Normal → Stage 1-1 → Travel progression assertions.

- [ ] **Step 3: Run RED**

Run a cold compile, close the MCP editor after saving, then run:

```powershell
& .\scripts\run_mvp_test_suites.ps1 -Suites @(
	"GameXXK.DesktopStory.Route.ProductionSurfaceAllActivities",
	"GameXXK.DesktopStory.Guide.FirstJourneyProfiles") -TimeoutSeconds 900
```

Expected: the route test fails because the non-tutorial ActionId returns at the route-surface guard; the Guide test fails because NewPlayer still owns a Forced input token, denies unrelated actions, and expands the native host.

---

### Task 2: Remove story and Guide vetoes from normal Workbench actions

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `SourceAssets/Narrative/Guides/Guide.Desktop.FirstJourney.guide.json`

- [ ] **Step 1: Let unrelated actions fall through the tutorial route surface**

Delete:

```cpp
if (bCombatBasicsTutorialRouteSurfaceOpen && !bCombatTutorialAction)
{
	return;
}
```

Keep the `if (bCombatTutorialAction)` branch and its route-specific Guide checks.

- [ ] **Step 2: Remove the general Workbench Guide veto**

Delete the `GuideAction` mapping and `IsActionAllowed` early return that currently precede Town, Story, fold, notice, scale, inventory, navigation, chest, and travel actions. Post-success `QueueDesktopStoryGuideEvent` calls remain.

- [ ] **Step 3: Remove duplicate per-control vetoes**

From `ToggleExpandedTab`, remove the `Action.Desktop.Tab` `IsActionAllowed` check.

From stage selection, remove the `IsActionAllowed(GuideAction)` condition but retain stage validation and the Normal 1-1 success event.

From the difficulty dropdown, remove `bForcedNormalConfirmation`; the real dropdown always opens normally, and selecting the Normal option emits the completion event.

- [ ] **Step 4: Make FirstJourney authoring nonblocking**

In every `Guide.Desktop.FirstJourney` step, set:

```json
"inputPolicy": "soft"
```

Keep targets, text, completion events, and next-step graph unchanged.

- [ ] **Step 5: Compile and run GREEN for the two focused tests**

Expected: both suites pass with zero failures while the semantic Guide sequence still advances.

---

### Task 3: Restrict fullscreen ownership and fold state to their real owners

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopNarrativeLayerTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKNarrativeAbortRecoveryTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopStoryFlowTest.cpp`

- [ ] **Step 1: Keep the native host HUD-sized for Guides**

Change `ShouldUseFullDesktopOverlayHost()` to:

```cpp
return OverlaySurface == EGameXXKDesktopOverlaySurface::NarrativeFullscreen;
```

- [ ] **Step 2: Remove non-user fold writes**

Retain the already-tested Narrative exit behavior that leaves `bIdleStripFolded` and notice settings untouched. Delete `bIdleStripFolded = false` from `RestoreSessionStateAfterMapTravel`; ordinary Workbench open/close and the explicit fold Action remain the idle-strip lifecycle owners.

- [ ] **Step 3: Stop pending story state from normalizing an already visible Workbench**

Remove calls to `ExitNarrativePresentationToDesktop()` from `OpenDesktopTrainingWorkbench()` and the post-map-travel `HasInterruptedNarrativeReplayPending()` branch. Widget construction and `RestoreSessionStateAfterMapTravel()` already select the Workbench surface and hide Narrative without changing normal bar navigation.

- [ ] **Step 4: Correct old forced-fold/full-host test contracts**

For fixtures beginning with an unfolded idle strip, require it to remain unfolded after completion, abort, failure, and interrupted-load recovery. Keep `bBackpackExpanded == false` where the requirement is specifically “Workbench Tab closes.” Replace folded-size comparisons with equality to the ordinary idle HUD size.

- [ ] **Step 5: Run the affected suites**

Run:

```powershell
& .\scripts\run_mvp_test_suites.ps1 -Suites @(
	"GameXXK.DesktopNarrative.Layer",
	"GameXXK.DesktopNarrative.InputRouting",
	"GameXXK.Narrative.AbortRecovery",
	"GameXXK.DesktopStory") -TimeoutSeconds 900
```

Expected: all suites execute and pass; no Workbench Guide assertion expects a full-work-area host or Forced token.

---

### Task 4: Validate/import the Guide and run final gates

**Files:**
- Modify generated asset through project importer: `Content/GameXXK/Narrative/Guides/DA_Guide_Desktop_FirstJourney.uasset`
- Verify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`

- [ ] **Step 1: Validate JSON and import through UE MCP**

Run:

```powershell
py -3 scripts/validate_guide_json.py SourceAssets/Narrative/Guides/Guide.Desktop.FirstJourney.guide.json
py -3 scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 120
py -3 -c "from scripts.ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(timeout=120); assert c.connect(); print(c.execute_console_command('py \"D:/UE5 demo/GameXXK/Content/Python/gamexxk_import_guide_json.py\" \"D:/UE5 demo/GameXXK/SourceAssets/Narrative/Guides/Guide.Desktop.FirstJourney.guide.json\"')); print(c.save_dirty_packages())"
```

Expected: validation reports no errors; the importer reports `DA_Guide_Desktop_FirstJourney`; the MCP save result is true with `dirty_after=[]`. Do not touch character, PaperZD, map, camera, or HD2D assets.

- [ ] **Step 2: Run a static ownership audit**

Run:

```powershell
rg -n "bCombatBasicsTutorialRouteSurfaceOpen && !bCombatTutorialAction|Action.Desktop.Unrelated|ShouldUseFullDesktopOverlayHost|bIdleStripFolded\s*=" Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp
```

Expected: no route-wide early return; no general Workbench unrelated-action veto; full-host logic is Narrative-only; fold assignments belong only to explicit idle-strip lifecycle/action paths.

- [ ] **Step 3: Run cold UBT/MCP smoke**

Run:

```powershell
py -3 scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 160
```

Expected: `Result: Succeeded`, MCP becomes ready, PIE starts/stops, and no Live Coding or Hot Reload is used.

- [ ] **Step 4: Preserve unrelated work**

Confirm the three user-staged scrollbar deletions remain staged. Do not create a broad runtime commit because the central runtime files contain overlapping uncommitted work; report the exact focused diff and all test counts instead.
