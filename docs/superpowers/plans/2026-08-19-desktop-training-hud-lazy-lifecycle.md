# GameXXK Desktop Training HUD Lazy Lifecycle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `L_DesktopTrainingHUD` boot only the always-on training workbench, create every other player-flow surface only when explicitly requested, release collapsed backpack-side UI after a 3-second grace period while travel keeps running, and remeasure the same editor HUD-only memory profile.

**Architecture:** Add a map-derived player-flow boot profile to `AGameXXKMVPPlayerController` so the HUD map has a narrow startup and refresh path while all existing maps and no-world tests retain the full path. The workbench captures lightweight embedded-inventory state before collapse, removes the heavy widget tree immediately, defers a frame-safe GC request for exactly three seconds, and keeps its travel runtime plus current 1K atlas session alive. Memory evidence comes from a repeatable editor-only process sampler using the same map, resolution, and 20/50-second checkpoints as the recorded baseline.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, UE Automation Tests, project MCP/UBT scripts, PowerShell process sampling, Markdown production records.

---

## File map

- `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`: declares the boot profile, narrow HUD initializer, individual route/battle ensure functions, and focused automation seams.
- `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`: selects the startup profile from the current map, prevents refresh/Escape from escalating HUD-only startup into full creation, and keeps explicit page opens lazy.
- `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`: proves HUD-only boot creates only the workbench and proves the legacy full-flow fixture remains unchanged.
- `Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h`: defines the lightweight embedded-session snapshot contract.
- `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`: captures/restores tab, filter, sort, scroll, and uncommitted deck selection without retaining visual children.
- `Source/GameXXK/Private/Tests/GameXXKFinalInventoryWidgetTest.cpp`: proves snapshot round-trip behavior for the embedded backpack.
- `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`: owns the 3-second hibernation state, resource-generation token, saved session, and automation probes.
- `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`: schedules/cancels delayed release, requests one frame-safe GC, restores UI state, and leaves travel ticking.
- `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`: proves the 2.9/3.0-second boundary, warm reopen cancellation, state restoration, no duplicate release, and top-strip continuity.
- `scripts/measure_desktop_training_hud_memory.ps1`: launches only the HUD map in editor game mode, samples working/private bytes at 20 and 50 seconds, writes JSON evidence, and stops only the process it launched.
- `scripts/README.md`: documents the editor-only memory probe and its output.
- `docs/production/current-goal-acceptance.md`: records implementation/test/build evidence and before/after measurements without changing the default 3D entry.

### Task 1: Lock the HUD-only player-flow boot boundary

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`

- [ ] **Step 1: Add a failing HUD-only boot automation test**

Add `GameXXK.MVP.UI.DesktopTrainingLazyBoot` beside `PlayerControllerOwnsFlowWidgets`. The fixture explicitly selects the HUD profile, invokes the narrow ensure path, and asserts that only the workbench exists:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingLazyBootTest,
	"GameXXK.MVP.UI.DesktopTrainingLazyBoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingLazyBootTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>();
	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	Controller->SetMVPSubsystemForTest(Subsystem);
	Controller->SetDesktopTrainingBootProfileForTest(true);

	TestTrue(TEXT("HUD-only ensure creates the workbench"), Controller->EnsureDesktopTrainingWidgetsForTest());
	TestNotNull(TEXT("workbench is owned"), Controller->GetDesktopTrainingWorkbenchWidgetForTest());
	TestNull(TEXT("main menu stays lazy"), Controller->GetMainMenuWidgetForTest());
	TestNull(TEXT("world map stays lazy"), Controller->GetWorldMapWidgetForTest());
	TestNull(TEXT("town overlay stays lazy"), Controller->GetTownOverlayWidgetForTest());
	TestNull(TEXT("town HUD stays lazy"), Controller->GetTownHudWidgetForTest());
	TestNull(TEXT("route map stays lazy"), Controller->GetRouteMapWidgetForTest());
	TestNull(TEXT("battle board stays lazy"), Controller->GetBattleBoardWidgetForTest());
	TestNull(TEXT("legacy inventory stays lazy"), Controller->GetInventoryWindowWidgetForTest());
	TestNull(TEXT("shop stays lazy"), Controller->GetMetaShopWidgetForTest());
	TestNull(TEXT("roster stays lazy"), Controller->GetCompanionRosterWidgetForTest());
	TestNull(TEXT("quest dialog stays lazy"), Controller->GetQuestDialogWidgetForTest());
	TestNull(TEXT("task panel stays lazy"), Controller->GetTaskPanelWidgetForTest());
	TestNull(TEXT("encounter panel stays lazy"), Controller->GetRouteEncounterPanelWidgetForTest());
	TestNull(TEXT("merchant stays lazy"), Controller->GetRouteMerchantWidgetForTest());
	TestNull(TEXT("relic bar stays lazy"), Controller->GetRelicBarWidgetForTest());

	Controller->RefreshPlayerFlowWidgetsForTest();
	TestNull(TEXT("HUD refresh does not escalate to full creation"), Controller->GetMainMenuWidgetForTest());
	TestTrue(TEXT("explicit shop request succeeds"), Controller->OpenMetaShopWindow());
	TestNotNull(TEXT("explicit request creates only the shop"), Controller->GetMetaShopWidgetForTest());
	TestNull(TEXT("shop request does not create the route map"), Controller->GetRouteMapWidgetForTest());
	TestNull(TEXT("shop request does not create the old inventory"), Controller->GetInventoryWindowWidgetForTest());
	return true;
}
```

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```powershell
& scripts/run_mvp_test_suites.ps1 -Suites @('GameXXK.MVP.UI.DesktopTrainingLazyBoot') -TimeoutSeconds 240
```

Expected: compile or automation failure because `SetDesktopTrainingBootProfileForTest` and `EnsureDesktopTrainingWidgetsForTest` do not exist yet.

- [ ] **Step 3: Add the minimal boot policy and narrow ensure implementation**

Declare the private policy and focused helpers in the controller header:

```cpp
enum class EGameXXKPlayerFlowBootProfile : uint8
{
	FullPlayerFlow,
	DesktopTrainingOnly
};

EGameXXKPlayerFlowBootProfile ResolvePlayerFlowBootProfile() const;
bool EnsureDesktopTrainingWidgets();
UGameXXKOneGameRouteMapWidget* EnsureRouteMapWidget();
UGameXXKBattleBoardWidget* EnsureBattleBoardWidget();

#if WITH_DEV_AUTOMATION_TESTS
void SetDesktopTrainingBootProfileForTest(bool bEnabled);
bool EnsureDesktopTrainingWidgetsForTest();
#endif

TOptional<EGameXXKPlayerFlowBootProfile> OverrideBootProfileForTest;
```

Resolve the profile without changing no-world fixture semantics:

```cpp
EGameXXKPlayerFlowBootProfile AGameXXKMVPPlayerController::ResolvePlayerFlowBootProfile() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (OverrideBootProfileForTest.IsSet())
	{
		return OverrideBootProfileForTest.GetValue();
	}
#endif
	const UWorld* World = GetWorld();
	const FString PackageName = World && World->GetOutermost()
		? World->GetOutermost()->GetName()
		: FString();
	return GameXXKLevelFlow::IsDesktopTrainingHUDMapPackage(PackageName)
		? EGameXXKPlayerFlowBootProfile::DesktopTrainingOnly
		: EGameXXKPlayerFlowBootProfile::FullPlayerFlow;
}

bool AGameXXKMVPPlayerController::EnsureDesktopTrainingWidgets()
{
	bEnableDesktopTrainingWorkbench = true;
	EnsureDesktopTrainingWorkbenchWidget();
	return DesktopTrainingWorkbenchWidget != nullptr;
}
```

Change `BeginPlay` and `RefreshPlayerFlowWidgets` so `DesktopTrainingOnly` calls only the narrow ensure/refresh path. Change Escape handling to inspect already-created widgets in HUD-only mode instead of calling `EnsurePlayerFlowWidgets`. Extract the existing inline route-map and battle-board creation blocks into `EnsureRouteMapWidget` and `EnsureBattleBoardWidget`; the full coordinator still invokes every ensure unit and returns the same full-set predicate.

- [ ] **Step 4: Run the HUD-only and legacy full-flow tests and verify GREEN**

Run:

```powershell
& scripts/run_mvp_test_suites.ps1 -Suites @(
  'GameXXK.MVP.UI.DesktopTrainingLazyBoot',
  'GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets'
) -TimeoutSeconds 300
```

Expected: both tests pass; the first owns only the workbench and the second still owns the complete legacy set.

- [ ] **Step 5: Commit only the boot-boundary files**

```powershell
git add -- Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp
git commit -m "perf: lazy boot desktop training hud"
```

Before committing, verify `L_Main.umap` remains at SHA256 `EE6E8394E40298321F2A57CC030018BDD1109EED36248597A7D7F414E387E46B`.

### Task 2: Preserve the embedded backpack's lightweight session state

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKFinalInventoryWidgetTest.cpp`

- [ ] **Step 1: Add a failing snapshot round-trip test**

Create an embedded inventory, select the NPC owner, switch to Deck, edit but do not apply the three-card selection, select an inventory filter, enable sort, set a scroll offset, capture state, restore it into a fresh embedded widget, and assert all lightweight values match:

```cpp
const FGameXXKEmbeddedInventorySessionState Snapshot = Original->CaptureEmbeddedSessionState();
Restored->ConfigureDesktopTrainingCharacter(Snapshot.CharacterId);
Restored->RestoreEmbeddedSessionState(Snapshot);

TestEqual(TEXT("owner restored"), Restored->GetConfiguredCharacterIdForTest(), Snapshot.CharacterId);
TestEqual(TEXT("subpage restored"), Restored->GetActiveCharacterBackpackTabForTest(), Snapshot.ActiveCharacterTab);
TestEqual(TEXT("filter restored"), Restored->GetActiveInventoryFilterForTest(), Snapshot.ActiveInventoryFilter);
TestEqual(TEXT("sort restored"), Restored->IsBackpackSortedForTest(), Snapshot.bBackpackSorted);
TestEqual(TEXT("pending deck restored"), Restored->GetPendingHeroDeckIdsForTest(), Snapshot.PendingDeckIds);
TestTrue(TEXT("scroll offset restored"), FMath::IsNearlyEqual(Restored->GetBackpackScrollOffsetForTest(), Snapshot.BackpackScrollOffset));
```

- [ ] **Step 2: Run the focused inventory test and verify RED**

Run:

```powershell
& scripts/run_mvp_test_suites.ps1 -Suites @('GameXXK.MVP.UI.FinalInventory.EmbeddedSessionState') -TimeoutSeconds 240
```

Expected: compile or automation failure because the snapshot type and capture/restore APIs do not exist.

- [ ] **Step 3: Implement the snapshot as value-only data**

Add a non-UObject C++ struct that holds no widget or texture references:

```cpp
struct FGameXXKEmbeddedInventorySessionState
{
	FName CharacterId = NAME_None;
	EGameXXKInventoryFilter ActiveInventoryFilter = EGameXXKInventoryFilter::All;
	EGameXXKCharacterBackpackTab ActiveCharacterTab = EGameXXKCharacterBackpackTab::Equipment;
	bool bBackpackSorted = false;
	float BackpackScrollOffset = 0.0f;
	TArray<FName> PendingDeckIds;
};
```

Implement capture/restore with an explicit deferred-scroll member because Slate scroll geometry is not guaranteed during rebuild:

```cpp
FGameXXKEmbeddedInventorySessionState UGameXXKInventoryWindowWidget::CaptureEmbeddedSessionState() const
{
	FGameXXKEmbeddedInventorySessionState State;
	State.CharacterId = ConfiguredDesktopTrainingCharacterId;
	State.ActiveInventoryFilter = ActiveInventoryFilter;
	State.ActiveCharacterTab = ActiveCharacterTab;
	State.bBackpackSorted = bBackpackSorted;
	State.BackpackScrollOffset = BackpackScrollBox ? BackpackScrollBox->GetScrollOffset() : DeferredBackpackScrollOffset;
	State.PendingDeckIds = PendingHeroDeckIds;
	return State;
}

void UGameXXKInventoryWindowWidget::RestoreEmbeddedSessionState(const FGameXXKEmbeddedInventorySessionState& State)
{
	ConfiguredDesktopTrainingCharacterId = State.CharacterId;
	ActiveInventoryFilter = State.ActiveInventoryFilter;
	ActiveCharacterTab = State.ActiveCharacterTab;
	bBackpackSorted = State.bBackpackSorted;
	PendingHeroDeckIds = State.PendingDeckIds;
	DeferredBackpackScrollOffset = FMath::Max(0.0f, State.BackpackScrollOffset);
	RefreshWindow();
	if (BackpackScrollBox)
	{
		BackpackScrollBox->SetScrollOffset(DeferredBackpackScrollOffset);
	}
}
```

Expose only the capture/restore APIs plus `IsBackpackSortedForTest`, `SetBackpackScrollOffsetForTest`, and `GetBackpackScrollOffsetForTest`; do not retain the old widget in the snapshot.

- [ ] **Step 4: Run the snapshot test and the complete FinalInventory suite**

Run:

```powershell
& scripts/run_mvp_test_suites.ps1 -Suites @(
  'GameXXK.MVP.UI.FinalInventory.EmbeddedSessionState',
  'GameXXK.MVP.UI.FinalInventory'
) -TimeoutSeconds 300
```

Expected: the focused round-trip and all existing FinalInventory tests pass.

- [ ] **Step 5: Commit only the snapshot files**

```powershell
git add -- Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp Source/GameXXK/Private/Tests/GameXXKFinalInventoryWidgetTest.cpp
git commit -m "feat: preserve embedded inventory session"
```

### Task 3: Add the 3-second collapse hibernation state machine

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Add failing tests for the release deadline and warm reopen**

Add `GameXXK.DesktopTraining.Workbench.CollapsedResourceHibernate` with three fixtures:

```cpp
Workbench->OpenWorkbench();
Workbench->HandleActionClicked(60); // collapse
Workbench->TickForTest(2.9f);
TestTrue(TEXT("release is pending before deadline"), Workbench->IsCollapsedResourceUnloadPendingForTest());
TestFalse(TEXT("resources remain unreleased at 2.9 seconds"), Workbench->AreCollapsedResourcesReleasedForTest());
TestTrue(TEXT("top strip remains"), Workbench->HasTravelVisualStripForTest());
const int32 TickCountBefore = Workbench->GetTravelVisualNativeTickCountForTest();
Workbench->TickForTest(0.1f);
TestTrue(TEXT("resources release at 3.0 seconds"), Workbench->AreCollapsedResourcesReleasedForTest());
TestEqual(TEXT("one GC request"), Workbench->GetCollapsedGcRequestCountForTest(), 1);
TestTrue(TEXT("travel keeps ticking"), Workbench->GetTravelVisualNativeTickCountForTest() > TickCountBefore);

WarmReopen->HandleActionClicked(60);
WarmReopen->TickForTest(2.9f);
WarmReopen->HandleActionClicked(60);
TestFalse(TEXT("reopen cancels pending release"), WarmReopen->IsCollapsedResourceUnloadPendingForTest());
TestEqual(TEXT("warm reopen avoids GC"), WarmReopen->GetCollapsedGcRequestCountForTest(), 0);

ColdReopen->HandleActionClicked(60);
ColdReopen->TickForTest(3.0f);
ColdReopen->HandleActionClicked(60);
TestEqual(TEXT("cold reopen creates one embedded inventory"), ColdReopen->GetEmbeddedInventoryWidgetCountForTest(), 1);
TestEqual(TEXT("saved subpage restored"), ColdReopen->GetEmbeddedBackpackTabForTest(), EGameXXKCharacterBackpackTab::Deck);
```

Extend the existing item-carry test to keep proving collapse rolls back a carried item and all tool entries before the timer starts.

- [ ] **Step 2: Run the hibernation test and verify RED**

Run:

```powershell
& scripts/run_mvp_test_suites.ps1 -Suites @('GameXXK.DesktopTraining.Workbench.CollapsedResourceHibernate') -TimeoutSeconds 240
```

Expected: compile or automation failure because the hibernation probes and state do not exist.

- [ ] **Step 3: Implement scheduling, cancellation, and exactly-once release**

Add value state and no heavy UObject references to the workbench header:

```cpp
static constexpr float CollapsedResourceUnloadDelaySeconds = 3.0f;
float CollapsedResourceUnloadRemainingSeconds = 0.0f;
uint64 CollapsedResourceGeneration = 0;
int32 CollapsedGcRequestCount = 0;
bool bCollapsedResourceUnloadPending = false;
bool bCollapsedResourcesReleased = false;
bool bHasSavedEmbeddedInventorySession = false;
FGameXXKEmbeddedInventorySessionState SavedEmbeddedInventorySession;

void ScheduleCollapsedResourceUnload();
void CancelCollapsedResourceUnload();
void ReleaseCollapsedResources();
void RestoreExpandedSessionState();
```

At collapse, preserve the existing transaction rollback, capture the embedded session, rebuild the collapsed shell, then schedule release. At reopen, cancel before rebuilding and restore the session after `BuildBackpackPanel` creates the one embedded inventory:

```cpp
void UGameXXKDesktopTrainingWorkbenchWidget::ScheduleCollapsedResourceUnload()
{
	CollapsedResourceUnloadRemainingSeconds = CollapsedResourceUnloadDelaySeconds;
	bCollapsedResourceUnloadPending = true;
	bCollapsedResourcesReleased = false;
	++CollapsedResourceGeneration;
}

void UGameXXKDesktopTrainingWorkbenchWidget::CancelCollapsedResourceUnload()
{
	bCollapsedResourceUnloadPending = false;
	CollapsedResourceUnloadRemainingSeconds = 0.0f;
	++CollapsedResourceGeneration;
}

void UGameXXKDesktopTrainingWorkbenchWidget::ReleaseCollapsedResources()
{
	if (bBackpackExpanded || !bCollapsedResourceUnloadPending || bCollapsedResourcesReleased)
	{
		return;
	}
	bCollapsedResourceUnloadPending = false;
	bCollapsedResourcesReleased = true;
	++CollapsedGcRequestCount;
	if (UWorld* World = GetWorld())
	{
		World->ForceGarbageCollection(false);
	}
}
```

In `NativeTick`, decrement the timer only while the workbench is visible, collapsed, and not in challenge mode. Give reopen precedence by cancelling before any release check. Keep `UpdateTravelVisuals`, `EnsureTravelAtlasSession`, and the current travel atlas pins outside the hibernation release path. Do not call `ReleaseTravelAtlasSession` on collapse; it remains a `NativeDestruct` responsibility.

- [ ] **Step 4: Invalidate stale callbacks and clear non-travel visual references**

Use `CollapsedResourceGeneration` as the generation guard for any delayed work introduced by reopen/restore. The collapsed layout must null `EmbeddedInventoryWidget`, warehouse/tool/map/challenge/tooltip image pointers and clear `RootCanvas` children, while retaining these top-strip fields:

```cpp
RootScaleBox;
ReferenceCanvasBox;
RootCanvas;
TravelRuntime;
TravelAtlasCache;
TravelAtlasSessionToken;
TravelBackgroundImages;
TravelHeroImage;
TravelPartyImages;
TravelEnemyImages;
```

If `GetWorld()` is unavailable in automation, still flip the release state and increment the exactly-once counter; real PIE/editor uses the frame-end world GC request.

- [ ] **Step 5: Run the hibernation, layout, carry, and travel tests**

Run:

```powershell
& scripts/run_mvp_test_suites.ps1 -Suites @(
  'GameXXK.DesktopTraining.Workbench.CollapsedResourceHibernate',
  'GameXXK.DesktopTraining.Workbench.LayoutContract',
  'GameXXK.DesktopTraining.Workbench.ItemCarry',
  'GameXXK.Training'
) -TimeoutSeconds 360
```

Expected: all selected tests pass; no carried/tool item is lost, release occurs once at the deadline, and travel advances while collapsed.

- [ ] **Step 6: Commit only the hibernation files**

```powershell
git add -- Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp
git commit -m "perf: hibernate collapsed desktop ui"
```

### Task 4: Add a repeatable editor HUD-only memory sampler

**Files:**
- Create: `scripts/measure_desktop_training_hud_memory.ps1`
- Modify: `scripts/README.md`

- [ ] **Step 1: Add the read-only sampler**

The script resolves the project editor executable, starts one process with the exact HUD-only URL and 1672×941 resolution, samples at 20 and 50 seconds, and writes a timestamped `desktop-training-hud-memory-YYYYMMDD-HHMMSS.json` file under `Saved/HarnessReports`:

```powershell
param(
    [string]$EditorExe = $env:UE_EDITOR_EXE,
    [int[]]$SampleSeconds = @(20, 50)
)

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $projectRoot 'GameXXK.uproject'
$arguments = @(
    $projectFile,
    '/Game/GameXXK/Maps/L_DesktopTrainingHUD',
    '-game', '-windowed', '-ResX=1672', '-ResY=941',
    '-NoSplash', '-NoSound', '-Unattended', '-stdout'
)
$process = Start-Process -FilePath $EditorExe -ArgumentList $arguments -PassThru -WindowStyle Hidden
$startedAt = Get-Date
$samples = @()
try {
    foreach ($second in ($SampleSeconds | Sort-Object)) {
        $remaining = $second - [int]((Get-Date) - $startedAt).TotalSeconds
        if ($remaining -gt 0) { Start-Sleep -Seconds $remaining }
        $process.Refresh()
        if ($process.HasExited) { throw "UnrealEditor exited before ${second}s with code $($process.ExitCode)" }
        $samples += [ordered]@{
            seconds = $second
            working_set_mib = [math]::Round($process.WorkingSet64 / 1MB, 1)
            private_memory_mib = [math]::Round($process.PrivateMemorySize64 / 1MB, 1)
        }
    }
} finally {
    if (-not $process.HasExited) { Stop-Process -Id $process.Id }
}
```

Quote arguments using a `ProcessStartInfo.ArgumentList` implementation if the discovered editor path or project path contains spaces; never use a shell-built command string. Include process id, executable, map, resolution, git HEAD, start time, samples, and exit/cleanup status in JSON.

- [ ] **Step 2: Document the probe and validate PowerShell syntax**

Add to `scripts/README.md`:

```markdown
### Desktop Training HUD editor memory

`measure_desktop_training_hud_memory.ps1` launches only `L_DesktopTrainingHUD` in editor game mode at 1672×941, records 20/50-second working-set and private-memory samples, and stops only its own process. It does not cook or package the project.
```

Run:

```powershell
$errors = $null
[System.Management.Automation.Language.Parser]::ParseFile(
  (Resolve-Path 'scripts/measure_desktop_training_hud_memory.ps1'),
  [ref]$null,
  [ref]$errors
) | Out-Null
if ($errors.Count -gt 0) { $errors | Format-List; exit 1 }
```

Expected: exit code 0 and no parser errors.

- [ ] **Step 3: Commit only the sampler documentation**

```powershell
git add -- scripts/measure_desktop_training_hud_memory.ps1 scripts/README.md
git commit -m "test: add desktop hud memory probe"
```

### Task 5: Cold-build and regression verification

**Files:**
- Verify only; do not modify `Content/GameXXK/Maps/L_Main.umap`

- [ ] **Step 1: Save editor packages safely if UE is running**

Use `scripts/ue_mcp_client.py` to query editor state and save dirty packages through UE MCP. If MCP is unavailable and the editor may contain unsaved work, leave the editor open and stop for user direction instead of force-closing it.

- [ ] **Step 2: Run a cold C++ build without Live Coding or Hot Reload**

Run the project-supported command:

```powershell
$env:UE_SKIP_UBT_SDK_SETUP = '1'
& scripts/ue_tdd_pipeline.py --build-only --no-hot-reload
```

If the script's current CLI differs, inspect `scripts/ue_tdd_pipeline.py --help` and use its documented cold-build equivalent with `-NoHotReload -NoUBA -MaxParallelActions=2`.

Expected: UBT result `Succeeded`, exit code 0.

- [ ] **Step 3: Run the complete focused regression set**

Run:

```powershell
& scripts/run_mvp_test_suites.ps1 -Suites @(
  'GameXXK.MVP.UI.DesktopTrainingLazyBoot',
  'GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets',
  'GameXXK.DesktopTraining.Workbench',
  'GameXXK.Training',
  'GameXXK.MVP.UI.FinalInventory',
  'GameXXK.MVP.SaveGame'
) -TimeoutSeconds 600
```

Expected: zero failed tests and no automation errors.

- [ ] **Step 4: Run source contracts and verify the protected map hash**

Run:

```powershell
python -m pytest scripts/test_desktop_training_hud_migration.py scripts/test_run_mvp_test_suites.py -q
(Get-FileHash -Algorithm SHA256 'Content/GameXXK/Maps/L_Main.umap').Hash
```

Expected: pytest exit code 0; map hash remains `EE6E8394E40298321F2A57CC030018BDD1109EED36248597A7D7F414E387E46B`.

### Task 6: Remeasure memory and close the production record

**Files:**
- Modify: `docs/production/current-goal-acceptance.md`
- Evidence: `Saved/HarnessReports/desktop-training-hud-memory-*.json`

- [ ] **Step 1: Run the editor-only HUD memory probe**

Run:

```powershell
& scripts/measure_desktop_training_hud_memory.ps1
```

Expected: one JSON report with valid 20-second and 50-second samples; no packaging/cook process; the launched UnrealEditor process is stopped afterward.

- [ ] **Step 2: Inspect whether memory stabilizes instead of continuing the former climb**

Compare the report with the frozen baseline:

```text
Before @20s: Working 1060.5 MiB / Private 1248.8 MiB
Before @50s: Working 3447.3 MiB / Private 5008.8 MiB
```

Acceptance requires the report to show that the 50-second HUD-only process no longer creates the complete player-flow widget graph. If working/private memory still grows toward the old stable values, use UE object/reference evidence to find the remaining owner and return to Task 1 or Task 3; do not record the target as complete merely because panels are visually hidden.

- [ ] **Step 3: Update the rolling acceptance pointer with exact evidence**

Append one dated bullet that states the observed cold-UBT result, the exact passed/total automation count, the JSON report path, both measured working/private values, and the unchanged protected-map hash. Include the frozen baseline values `1060.5/1248.8 MiB @20s` and `3447.3/5008.8 MiB @50s` in the same bullet so the comparison cannot be selectively reported. Copy every result from the fresh command output; do not estimate or round beyond the sampler's one-decimal output.

- [ ] **Step 4: Run final repository checks**

Run:

```powershell
git diff --check
(Get-FileHash -Algorithm SHA256 'Content/GameXXK/Maps/L_Main.umap').Hash
git status --short
```

Expected: no whitespace errors; protected hash unchanged; status contains only intentional target files plus preserved pre-existing user changes.

- [ ] **Step 5: Commit only the production record**

```powershell
git add -- docs/production/current-goal-acceptance.md
git commit -m "docs: record desktop hud memory reduction"
```

Do not stage `L_Main.umap`, generated source art, unrelated probes, or any pre-existing user-tuned assets.
