# Desktop Training Wave Health Reset Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

Status: completed and verified on 2026-08-22. The checkboxes below preserve the original RED/GREEN execution recipe; final evidence is recorded in the implementation handoff.

**Goal:** Make every pure-2D Travel encounter spawn after five Walking seconds with both party and enemy formations at full health, while keeping enemy bars bound to authoritative per-slot snapshots across repeated waves.

**Architecture:** Keep the existing `Walking -> Combat` runner and presentation queue. Put the five-second duration and full-health materialization in `FGameXXKTrainingRules`, then remove the active enemy bar's dependency on the flattened compatibility mirror in `FGameXXKTrainingTravelVisualRuntime`. The Workbench renders those fractions without changing layout/assets and owns the presentation-only boundary that prevents queued death animation time from consuming the visible Walking interval.

**Acceptance-driven deviation:** Whole-stage PIE verification found that enemy `SProgressBar` fills could remain gray after looping even while authoritative health, visual fractions, and UMG Percent were all `1.0`. The implemented Workbench therefore replaces only the enemy bars with code-native `UBorder` track/fill siblings whose left-anchored `RenderScale.X` is the fraction. This additional change is covered by bar-type, opacity, volatility, respawn, and full-loop screenshot tests.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, UE Automation Tests, project UE MCP scripts, UBT cold build.

---

## File map

- Modify `Source/GameXXK/Public/GameXXKTrainingRules.h`: named five-second Travel spawn-delay constant.
- Modify `Source/GameXXK/Private/GameXXKTrainingRules.cpp`: full-health party materialization and use of the delay constant.
- Modify `Source/GameXXK/Private/Tests/GameXXKTrainingRulesTest.cpp`: runner timing, full-health, and repeated-wave regression coverage.
- Modify `Source/GameXXK/Private/UI/GameXXKTrainingTravelVisualRuntime.cpp`: authoritative per-slot enemy health lookup.
- Modify `Source/GameXXK/Private/Tests/GameXXKTrainingTravelVisualRuntimeTest.cpp`: stale compatibility-mirror regression.
- Modify `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`: five-second hidden-enemy Walking boundary and full-bar spawn contract.
- Modify `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`: retain the current layout/assets, gate the visible Walking boundary, and use deterministic enemy track/fill rendering.

### Task 1: Five-second spawn delay and full-health encounter materialization

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKTrainingRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKTrainingRules.h`
- Modify: `Source/GameXXK/Private/GameXXKTrainingRules.cpp`

- [ ] **Step 1: Write the runner RED test**

Add a focused automation test near `FGameXXKTrainingTravelRunnerLoopTest`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingTravelEncounterSpawnResetTest,
	"GameXXK.Training.TravelEncounterSpawnReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelEncounterSpawnResetTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingProgress Progress;
	FGameXXKTrainingRules::InitializeNewGame(Progress);
	const TArray<FGameXXKTrainingTravelPartyUnitRuntime> DamagedParty = {
		FGameXXKTrainingTravelPartyUnitRuntime(TEXT("Hero"), 40, 100, 100000),
		FGameXXKTrainingTravelPartyUnitRuntime(TEXT("Companion.Blade.Test"), 30, 90, 100000),
		FGameXXKTrainingTravelPartyUnitRuntime(TEXT("Npc.TusiChief"), 20, 80, 100000)};
	FGameXXKTrainingTravelRuntime Runner;
	TestTrue(TEXT("damaged party fixture initializes"),
		FGameXXKTrainingRules::InitializeTravelRunner(Progress, Runner, DamagedParty));
	TestEqual(TEXT("spawn delay is five logical seconds"), Runner.WalkStepsRequired, 5);
	for (const FGameXXKTrainingTravelPartyUnitRuntime& Unit : Runner.PartyUnits)
	{
		TestEqual(TEXT("encounter materialization fully heals every party unit"), Unit.HP, Unit.MaxHP);
	}

	bool bEncounterCompleted = false;
	bool bStageCompleted = false;
	bool bDefeated = false;
	FGameXXKTrainingReward Reward;
	for (int32 Second = 1; Second < 5; ++Second)
	{
		TestTrue(TEXT("walking second advances"), FGameXXKTrainingRules::AdvanceTravelRunner(
			Progress, Runner, bEncounterCompleted, bStageCompleted, bDefeated, Reward));
		TestEqual(TEXT("first four seconds remain Walking"), Runner.Phase, EGameXXKTrainingTravelPhase::Walking);
	}
	TestTrue(TEXT("fifth walking second advances"), FGameXXKTrainingRules::AdvanceTravelRunner(
		Progress, Runner, bEncounterCompleted, bStageCompleted, bDefeated, Reward));
	TestEqual(TEXT("fifth second enters Combat"), Runner.Phase, EGameXXKTrainingTravelPhase::Combat);

	for (FGameXXKTrainingTravelPartyUnitRuntime& Unit : Runner.PartyUnits)
	{
		Unit.HP = 1;
	}
	for (int32 Guard = 0; Guard < 8 && !bEncounterCompleted; ++Guard)
	{
		TestTrue(TEXT("high-attack party settles the current encounter"), FGameXXKTrainingRules::AdvanceTravelRunner(
			Progress, Runner, bEncounterCompleted, bStageCompleted, bDefeated, Reward));
	}
	TestTrue(TEXT("the current encounter settles within the bounded guard"), bEncounterCompleted);
	for (const FGameXXKTrainingTravelPartyUnitRuntime& Unit : Runner.PartyUnits)
	{
		TestEqual(TEXT("next encounter fully heals every party unit"), Unit.HP, Unit.MaxHP);
	}
	for (const FGameXXKTrainingTravelEnemyRuntime& Enemy : Runner.Enemies)
	{
		TestEqual(TEXT("next encounter materializes every enemy at full health"), Enemy.HP, Enemy.MaxHP);
	}
	return true;
}
```

In this RED step, update old two-step/three-step timing fixtures with literal five-second bounds so the test target still compiles before the production constant exists. In the Workbench combat presentation test, replace the two initial `TickForTest(1.0f)` calls with four ticks that assert `Walking` and collapsed enemy images/bars, followed by the fifth tick that asserts `EncounterIdle` and full visible bars. After Step 3 adds `TravelEncounterSpawnDelaySeconds`, replace those new timing literals with the named constant as a no-behavior-change cleanup.

- [ ] **Step 2: Run RED and confirm the intended failures**

With the editor saved and closed, run:

```powershell
D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe "D:\UE5 demo\GameXXK\GameXXK.uproject" -Unattended -NoSound -NullRHI -NoSplash -NoPause -ReportOutputPath="D:\UE5 demo\GameXXK\Saved\Automation\WaveHealthResetRed" -ExecCmds="Automation RunTests GameXXK.Training.TravelEncounterSpawnReset; Automation RunTests GameXXK.DesktopTraining.Workbench.TravelCombatPresentation; Quit"
```

Expected: `TravelEncounterSpawnReset` fails because `WalkStepsRequired` is `2` and damaged party HP is preserved; the Workbench test fails because combat begins before second five.

- [ ] **Step 3: Implement the minimal rule change**

In `GameXXKTrainingRules.h`, add the named constant beside the existing Travel constants:

```cpp
static constexpr int32 TravelEncounterSpawnDelaySeconds = 5;
```

In the party normalization loop in `InitializeTravelRunner`, replace preservation of source HP with full-health materialization:

```cpp
Unit.MaxHP = FMath::Max(1, Unit.MaxHP);
Unit.HP = Unit.MaxHP;
Unit.Attack = FMath::Max(1, Unit.Attack);
```

Replace the literal delay assignment:

```cpp
OutRuntime.WalkStepsRequired = TravelEncounterSpawnDelaySeconds;
```

Keep the existing party identity/stat array and deterministic cursor carry-forward at encounter settlement; only HP is reset by the initializer.

- [ ] **Step 4: Run GREEN for Training and Workbench timing**

Run the same two-test command with `ReportOutputPath=...\WaveHealthResetGreen`, then parse its `index.json` with:

```powershell
python scripts/parse_automation_index.py Saved/Automation/WaveHealthResetGreen/index.json
```

Expected: both focused tests pass with zero errors.

- [ ] **Step 5: Preserve unrelated working-tree changes**

Run `git diff --check` and inspect only the four Task 1 files. Do not stage or commit pre-existing route-map, protected-asset, diagnostic-accessor, or UI-tuning changes. If Task 1 hunks can be isolated safely, commit only those hunks as `fix: reset travel encounter health and spawn delay`; otherwise leave implementation uncommitted and report the overlap.

### Task 2: Bind enemy bars to authoritative formation snapshots

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKTrainingTravelVisualRuntimeTest.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKTrainingTravelVisualRuntime.cpp`

- [ ] **Step 1: Write the stale-mirror RED test**

Add:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingTravelVisualRuntimeAuthoritativeEnemyHealthTest,
	"GameXXK.DesktopTraining.TravelVisualRuntime.AuthoritativeEnemyHealth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelVisualRuntimeAuthoritativeEnemyHealthTest::RunTest(const FString& Parameters)
{
	FGameXXKTrainingTravelRuntime Snapshot = MakeTravelRuntime(
		EGameXXKTrainingTravelPhase::Combat,
		TEXT("Enemy.Ch1.Rooster"),
		90,
		100);
	Snapshot.Enemies[0].HP = 25;
	Snapshot.Enemies[0].MaxHP = 100;
	Snapshot.EnemyHP = 90;       // Deliberately stale compatibility mirror.
	Snapshot.EnemyMaxHP = 100;

	FGameXXKTrainingTravelVisualRuntime Visual;
	Visual.Synchronize(Snapshot);
	TestTrue(TEXT("active enemy bar reads the authoritative slot array"),
		FMath::IsNearlyEqual(Visual.GetEnemyHealthFractionForSlot(0), 0.25f));
	TestTrue(TEXT("compatibility getter delegates to the authoritative active slot"),
		FMath::IsNearlyEqual(Visual.GetEnemyHealthFraction(), 0.25f));
	return true;
}
```

- [ ] **Step 2: Run RED**

Run:

```powershell
D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe "D:\UE5 demo\GameXXK\GameXXK.uproject" -Unattended -NoSound -NullRHI -NoSplash -NoPause -ReportOutputPath="D:\UE5 demo\GameXXK\Saved\Automation\AuthoritativeEnemyHealthRed" -ExecCmds="Automation RunTests GameXXK.DesktopTraining.TravelVisualRuntime.AuthoritativeEnemyHealth; Quit"
```

Expected: FAIL because the active getter returns the stale mirror fraction `0.9`.

- [ ] **Step 3: Implement authoritative lookup**

In `GetEnemyHealthFraction`, resolve `GetPresentedEnemySlotIndex()` and use `LatestRuntime.Enemies[slot]` whenever that slot exists. During an active event, resolve before/after HP and MaxHP from `ActiveCombatEvent.EnemiesBefore/EnemiesAfter[slot]`; keep scalar fields only as a fallback for legacy one-enemy fixtures whose arrays are empty.

Use this shape:

```cpp
const auto Fraction = [](const int32 HP, const int32 MaxHP)
{
	return MaxHP > 0
		? FMath::Clamp(static_cast<float>(HP) / static_cast<float>(MaxHP), 0.0f, 1.0f)
		: 0.0f;
};
```

Do not change target identity, animation durations, slot visibility, or event ordering.

- [ ] **Step 4: Run GREEN and related visual-runtime tests**

Run:

```powershell
D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe "D:\UE5 demo\GameXXK\GameXXK.uproject" -Unattended -NoSound -NullRHI -NoSplash -NoPause -ReportOutputPath="D:\UE5 demo\GameXXK\Saved\Automation\AuthoritativeEnemyHealthGreen" -ExecCmds="Automation RunTests GameXXK.DesktopTraining.TravelVisualRuntime; Quit"
python scripts/parse_automation_index.py Saved/Automation/AuthoritativeEnemyHealthGreen/index.json
```

Expected: the new test and all existing formation/death-boundary tests pass.

- [ ] **Step 5: Check the focused diff**

Run `git diff --check -- Source/GameXXK/Private/UI/GameXXKTrainingTravelVisualRuntime.cpp Source/GameXXK/Private/Tests/GameXXKTrainingTravelVisualRuntimeTest.cpp`. Commit only these isolated hunks as `fix: use authoritative travel enemy health` if doing so cannot include prior user work.

### Task 3: Cold build and real pure-2D acceptance

**Files:**
- Verify: all Task 1 and Task 2 files
- Evidence: `Saved/Automation/*`, `Saved/HarnessReports/*`, `Saved/Codex/*`

- [ ] **Step 1: Save and close the editor through UE MCP**

Use `scripts/ue_mcp_client.py` APIs to call `save_dirty_packages`, verify PIE is stopped, and issue `QUIT_EDITOR`. Do not force-close the editor.

- [ ] **Step 2: Run cold UBT**

```powershell
D:\UE_5.8\Engine\Build\BatchFiles\Build.bat GameXXKEditor Win64 Development -Project="D:\UE5 demo\GameXXK\GameXXK.uproject" -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2
```

Expected: `Result: Succeeded`.

- [ ] **Step 3: Run focused automation suites**

Run `GameXXK.Training`, `GameXXK.DesktopTraining.TravelVisualRuntime`, and `GameXXK.DesktopTraining.Workbench` into a fresh report directory, then parse `index.json`. Expected: zero failed and zero errors.

- [ ] **Step 4: Run multi-wave PIE sampling on the canonical map**

Launch the exact project editor and run `scripts/run_training_visual_pie_probe.py` on `/Game/GameXXK/Maps/L_DesktopTrainingHUD`. Sample at 0.1 seconds across several complete waves. At every `Walking -> EncounterIdle` boundary assert:

```text
Walking duration: >= 5.0 logical seconds
Walking enemy visibility: false
Spawn party HP: every HP == MaxHP
Spawn enemy HP: every HP == MaxHP
UMG bar percent: matches visual fraction
Programmatic layout build count: stable during automatic travel
```

- [ ] **Step 5: Capture and visually review the boundary**

Capture one Walking frame near second four and one first-combat frame after second five, then review them with a suitable method. Acceptance: no enemy or enemy bars during Walking; the next formation appears with all enemy and party bars full; existing positions, sizes, facing, ground anchors, and assets are unchanged.

- [ ] **Step 6: Final verification record**

Run `git diff --check`, list exactly which tracked files changed, and report cold-build, automation, PIE data, and visual-evidence paths. Do not claim unrelated dirty files or protected assets were verified or modified.
