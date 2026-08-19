# GameXXK Desktop Training Four-Profile Performance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a repeatable same-machine performance matrix for the empty HUD shell, active Travel workbench, ChallengeViewport battle, and rollback 3D Qingshan town before any default-entry decision.

**Architecture:** Extend the existing PowerShell sampler to launch one isolated Unreal process per profile in a fixed order and aggregate all samples into one schema-v2 JSON report. Add a narrow `travel` performance boot action beside the existing `empty` and `challenge` actions; each HUD profile uses `L_DesktopTrainingHUD`, while `town3d` directly loads the current accepted Qingshan map. CPU is computed from process-time deltas, and GPU Engine/Dedicated/Shared values come from the stable Windows GPU performance CIM classes when available.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, UE Automation, PowerShell 7, Windows CIM performance counters, JSON evidence, cold UBT.

---

## File map

- `scripts/measure_desktop_training_hud_memory.ps1`: owns the four profile definitions, isolated launch loop, process/CPU/GPU sampling, cleanup, and schema-v2 aggregate report.
- `scripts/test_measure_desktop_training_hud_memory.py`: locks profile names, maps, arguments, schema, and the no-cook/no-shell-concatenation boundary.
- `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`: exposes the existing performance-profile application through a focused automation seam.
- `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`: applies `travel` and `challenge` startup states only on the explicit performance command line.
- `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`: proves `travel` starts the cleared 1-1 TravelRunner without creating legacy full-flow widgets.
- `scripts/README.md`: documents single-profile and full-matrix commands plus report fields.
- `docs/production/current-goal-acceptance.md`: records exact matrix values and the resulting entry decision.

### Task 1: Freeze the four-profile sampler contract

**Files:**
- Modify: `scripts/test_measure_desktop_training_hud_memory.py`
- Modify: `scripts/measure_desktop_training_hud_memory.ps1`

- [x] **Step 1: Write the failing describe-only matrix test**

Replace the old single-map assertion with a request for all profiles:

```python
def test_describe_only_reports_the_four_profile_matrix(self) -> None:
    completed = subprocess.run(
        [
            "pwsh.exe", "-NoProfile", "-ExecutionPolicy", "Bypass",
            "-File", str(SAMPLER), "-DescribeOnly", "-Profile", "all",
        ],
        cwd=PROJECT_ROOT,
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8-sig",
    )
    contract = json.loads(completed.stdout)
    self.assertEqual(contract["schema_version"], 2)
    self.assertEqual(contract["resolution"], {"width": 1672, "height": 941})
    self.assertEqual(contract["sample_seconds"], [20, 50])
    profiles = {profile["name"]: profile for profile in contract["profiles"]}
    self.assertEqual(list(profiles), ["empty", "travel", "challenge", "town3d"])
    self.assertEqual(profiles["empty"]["map"], "/Game/GameXXK/Maps/L_DesktopTrainingHUD")
    self.assertIn("-GameXXKPerfProfile=empty", profiles["empty"]["arguments"])
    self.assertIn("-GameXXKPerfProfile=travel", profiles["travel"]["arguments"])
    self.assertIn("-GameXXKPerfProfile=challenge", profiles["challenge"]["arguments"])
    self.assertEqual(
        profiles["town3d"]["map"],
        "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo",
    )
    self.assertFalse(any("GameXXKPerfProfile" in arg for arg in profiles["town3d"]["arguments"]))
```

Extend the source contract:

```python
def test_sampler_records_cpu_and_gpu_process_metrics(self) -> None:
    text = self.sampler_text()
    self.assertIn("TotalProcessorTime", text)
    self.assertIn("Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine", text)
    self.assertIn("Win32_PerfFormattedData_GPUPerformanceCounters_GPUProcessMemory", text)
    for field in ("cpu_percent", "gpu_engine_percent", "gpu_dedicated_mib", "gpu_shared_mib"):
        self.assertIn(field, text)
```

- [x] **Step 2: Run the sampler test and verify RED**

Run:

```powershell
python scripts/test_measure_desktop_training_hud_memory.py
```

Expected: FAIL because `-Profile`, schema version 2, profile definitions, and GPU/CPU fields do not exist.

- [x] **Step 3: Implement profile definitions and schema-v2 describe output**

Add the parameter and fixed profile definitions:

```powershell
[ValidateSet('empty', 'travel', 'challenge', 'town3d', 'all')]
[string]$Profile = 'all'

$hudMap = '/Game/GameXXK/Maps/L_DesktopTrainingHUD'
$townMap = '/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo'
$profileDefinitions = @(
    [ordered]@{ name = 'empty'; map = $hudMap; extra_arguments = @('-GameXXKPerfProfile=empty') },
    [ordered]@{ name = 'travel'; map = $hudMap; extra_arguments = @('-GameXXKPerfProfile=travel') },
    [ordered]@{ name = 'challenge'; map = $hudMap; extra_arguments = @('-GameXXKPerfProfile=challenge') },
    [ordered]@{ name = 'town3d'; map = $townMap; extra_arguments = @() }
)
$selectedProfiles = if ($Profile -eq 'all') {
    @($profileDefinitions)
} else {
    @($profileDefinitions | Where-Object { $_.name -eq $Profile })
}
```

Build each profile's `arguments` from the same common list and return this describe contract:

```powershell
$contract = [ordered]@{
    schema_version = 2
    editor_exe = $EditorExe
    project_file = $projectFile
    resolution = [ordered]@{ width = 1672; height = 941 }
    sample_seconds = @($resolvedSampleSeconds)
    report_directory = $reportDirectory
    profiles = @($selectedProfiles | ForEach-Object {
        [ordered]@{
            name = $_.name
            map = $_.map
            arguments = @($projectFile, $_.map, '-game', '-windowed', '-ResX=1672', '-ResY=941', '-NoSplash') + @($_.extra_arguments)
        }
    })
}
```

- [x] **Step 4: Run describe-only GREEN verification**

Run:

```powershell
python scripts/test_measure_desktop_training_hud_memory.py
pwsh -NoProfile -File scripts/measure_desktop_training_hud_memory.ps1 -DescribeOnly -Profile all
```

Expected: Python tests pass; JSON lists exactly `empty`, `travel`, `challenge`, `town3d` in that order.

### Task 2: Add deterministic Travel performance boot

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`

- [x] **Step 1: Write the failing Travel profile automation test**

Add `GameXXK.MVP.UI.DesktopTrainingTravelPerfProfile`:

```cpp
UGameInstance* TestGameInstance = NewObject<UGameInstance>();
UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
Controller->SetMVPSubsystemForTest(Subsystem);
Controller->SetDesktopTrainingBootProfileForTest(true);
TestTrue(TEXT("travel profile fixture starts in town"), Subsystem->StartGame());
TestTrue(TEXT("travel profile creates only the workbench"), Controller->EnsureDesktopTrainingWidgetsForTest());
TestTrue(TEXT("travel profile applies"), Controller->ApplyDesktopTrainingPerfProfileForTest(TEXT("travel")));
TestEqual(
    TEXT("travel runner enters walking"),
    Subsystem->GetTrainingTravelRuntimeCopy().Phase,
    EGameXXKTrainingTravelPhase::Walking);
TestEqual(
    TEXT("travel profile selects cleared 1-1"),
    Subsystem->GetTrainingTravelRuntimeCopy().StageId,
    FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1));
TestNull(TEXT("travel profile keeps legacy route map lazy"), Controller->GetRouteMapWidgetForTest());
TestNull(TEXT("travel profile keeps legacy battle board lazy"), Controller->GetBattleBoardWidgetForTest());
```

- [x] **Step 2: Run focused Automation and verify RED**

Run:

```powershell
& scripts/run_mvp_test_suites.ps1 -Suites @('GameXXK.MVP.UI.DesktopTrainingTravelPerfProfile') -TimeoutSeconds 240
```

Expected: compile failure because `ApplyDesktopTrainingPerfProfileForTest` does not exist.

- [x] **Step 3: Implement one profile application function**

Declare:

```cpp
bool ApplyDesktopTrainingPerfProfile(const FString& Profile);
#if WITH_DEV_AUTOMATION_TESTS
bool ApplyDesktopTrainingPerfProfileForTest(const FString& Profile);
#endif
```

Implement and call it from `BeginPlay` after `OpenDesktopTrainingWorkbench()`:

```cpp
bool AGameXXKMVPPlayerController::ApplyDesktopTrainingPerfProfile(const FString& Profile)
{
    if (!DesktopTrainingWorkbenchWidget || Profile.IsEmpty() || Profile == TEXT("empty"))
    {
        return Profile.IsEmpty() || Profile == TEXT("empty");
    }
    DesktopTrainingWorkbenchWidget->OpenBackpack();
    if (Profile == TEXT("travel"))
    {
        DesktopTrainingWorkbenchWidget->SelectStageForTest(FName(TEXT("Training.Normal.1-1")));
        return DesktopTrainingWorkbenchWidget->ClickTravelForTest();
    }
    if (Profile == TEXT("challenge"))
    {
        DesktopTrainingWorkbenchWidget->SelectStageForTest(FName(TEXT("Training.Normal.1-2")));
        return DesktopTrainingWorkbenchWidget->ClickChallengeForTest();
    }
    return false;
}
```

The automation seam returns `ApplyDesktopTrainingPerfProfile(Profile.ToLower())`. `empty` remains handled before workbench creation and must not allocate the workbench.

- [x] **Step 4: Run focused and legacy GREEN tests**

Run:

```powershell
& scripts/run_mvp_test_suites.ps1 -Suites @(
  'GameXXK.MVP.UI.DesktopTrainingTravelPerfProfile',
  'GameXXK.MVP.UI.DesktopTrainingLazyBoot',
  'GameXXK.DesktopTraining.Workbench',
  'GameXXK.Training'
) -TimeoutSeconds 480
```

Expected: zero failed tests and no Automation errors.

### Task 3: Implement isolated process and CPU/GPU sampling

**Files:**
- Modify: `scripts/measure_desktop_training_hud_memory.ps1`
- Modify: `scripts/test_measure_desktop_training_hud_memory.py`

- [x] **Step 1: Add the failing schema source assertions**

Require the aggregate report to contain profile status, cleanup, samples, and errors:

```python
def test_schema_v2_keeps_each_profile_isolated(self) -> None:
    text = self.sampler_text()
    self.assertIn("profile_results", text)
    self.assertIn("closed-launched-process", text)
    self.assertIn("killed-launched-process-tree", text)
    self.assertIn("profile_name", text)
    self.assertIn("gpu_metric_error", text)
```

- [x] **Step 2: Run the test and verify RED**

Run `python scripts/test_measure_desktop_training_hud_memory.py`.

Expected: FAIL because schema-v2 execution fields are absent.

- [x] **Step 3: Add metric helpers and one-process-per-profile loop**

Use stable CIM classes and return zero plus a recorded error only when unavailable:

```powershell
function Get-GpuProcessMetrics([int]$ProcessId) {
    $prefix = "pid_${ProcessId}_"
    try {
        $engine = @(Get-CimInstance Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine |
            Where-Object { $_.Name.StartsWith($prefix) -and $_.Name -match 'engtype_(3D|Compute)' })
        $memory = @(Get-CimInstance Win32_PerfFormattedData_GPUPerformanceCounters_GPUProcessMemory |
            Where-Object { $_.Name.StartsWith($prefix) })
        return [ordered]@{
            gpu_engine_percent = [math]::Round((($engine | Measure-Object UtilizationPercentage -Sum).Sum), 2)
            gpu_dedicated_mib = [math]::Round((($memory | Measure-Object DedicatedUsage -Sum).Sum) / 1MB, 1)
            gpu_shared_mib = [math]::Round((($memory | Measure-Object SharedUsage -Sum).Sum) / 1MB, 1)
            gpu_metric_error = $null
        }
    } catch {
        return [ordered]@{ gpu_engine_percent = 0.0; gpu_dedicated_mib = 0.0; gpu_shared_mib = 0.0; gpu_metric_error = $_.Exception.Message }
    }
}
```

For CPU, retain the previous sample's `TotalProcessorTime` and elapsed time:

```powershell
$cpuDeltaSeconds = ($process.TotalProcessorTime - $previousCpuTime).TotalSeconds
$wallDeltaSeconds = [math]::Max(0.001, $stopwatch.Elapsed.TotalSeconds - $previousElapsedSeconds)
$cpuPercent = [math]::Round(100.0 * $cpuDeltaSeconds / ($wallDeltaSeconds * [Environment]::ProcessorCount), 2)
```

Each profile result contains `profile_name`, `map`, `arguments`, `process_id`, `status`, `cleanup`, `samples`, and `error`. Finish and close one process before starting the next. If one profile fails, record it and continue the remaining profiles; exit nonzero after writing the aggregate report.

- [x] **Step 4: Run PowerShell parser and Python GREEN tests**

Run:

```powershell
$parseErrors = $null
[System.Management.Automation.Language.Parser]::ParseFile(
  (Resolve-Path 'scripts/measure_desktop_training_hud_memory.ps1'),
  [ref]$null,
  [ref]$parseErrors
) | Out-Null
if ($parseErrors.Count -gt 0) { $parseErrors | Format-List; exit 1 }
python scripts/test_measure_desktop_training_hud_memory.py
```

Expected: parser exit 0 and all sampler tests pass.

### Task 4: Run the matrix, record the decision, and checkpoint

**Files:**
- Modify: `scripts/README.md`
- Modify: `docs/production/current-goal-acceptance.md`
- Modify: `docs/production/2026-08-19-goal-progress-evidence.md`
- Evidence: `Saved/HarnessReports/desktop-training-performance-matrix-*.json`

- [x] **Step 1: Cold-build and run focused Automation**

Run the standard no-Hot-Reload UBT command, followed by:

```powershell
python scripts/ai_production_loop.py --run-automation --automation-tests GameXXK.MVP.UI.DesktopTrainingTravelPerfProfile --automation-report DesktopTrainingTravelPerfProfile-20260819 --json
python scripts/ai_production_loop.py --run-script-tests --script-tests all --json
```

Expected: UBT `Result: Succeeded`; focused Automation and headless/all exit 0.

- [x] **Step 2: Run the four-profile matrix** *(report `desktop-training-performance-matrix-20260819-184206.json`; challenge profile is a pre-correction baseline because the user rejected the embedded ChallengeViewport)*

Run:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/measure_desktop_training_hud_memory.ps1 -Profile all
```

Expected: one schema-v2 report with four completed profile results and 20/50-second CPU/GPU/Working/Private samples. Every launched process has `closed-launched-process` or `killed-launched-process-tree`, and no UnrealEditor remains.

- [ ] **Step 3: Apply the decision rule**

Compute the 50-second deltas relative to `town3d` and record them without inventing a target:

```text
travel_delta = travel_50s - town3d_50s
challenge_delta = challenge_50s - town3d_50s
empty_delta = empty_50s - town3d_50s
```

If `travel` has lower Working Set, Private Memory, GPU Dedicated, and CPU than `town3d`, record the performance direction as PASS while retaining exact values. If any key metric is worse or any profile fails, keep the entry gate closed and open a root-cause optimization task using object/asset evidence; do not tune the default entry.

- [ ] **Step 4: Update README and production evidence**

Document `-Profile all` and each single profile. Update the rolling pointer with the report path, exact four-profile values, deltas, cleanup status, protected hashes, and the entry decision.

- [ ] **Step 5: Verify and commit only target files**

Run:

```powershell
git diff --check
python scripts/harness_state_validator.py --json
(Get-FileHash -Algorithm SHA256 'Content/GameXXK/Maps/L_Main.umap').Hash
git status --short
```

Stage only the sampler, its tests, controller/test files, README, this plan, and production records. Commit with:

```powershell
git commit -m "perf: measure desktop training four-profile matrix"
```

Do not stage `L_Main.umap`, `scripts/test_battle_camera_framing.py`, `SourceAssets/`, `SourceArt/` review trees, root `Private/Public`, or historical probes.
