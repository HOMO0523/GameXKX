# Battle Target Arrow Alignment Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore the accepted card-targeting arrow/ink alignment by removing the direction-dependent arrow-head translation while preserving cursor tracking and stage-space target resolution.

**Architecture:** Extract the arrow brush top-left calculation into one deterministic class seam that is called by `NativePaint` and directly covered by a focused automation test. First preserve the current offset in that seam to prove the test fails, then reduce the implementation to center-on-end placement, remove temporary cursor diagnostics, and verify the result through cold UBT, Unreal automation, the deterministic art check, and real PIE screenshots.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate painting, Unreal Automation Tests, project UE MCP scripts, UBT, PowerShell.

---

## File map

- Modify `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`: expose one deterministic, stateless arrow-placement test seam alongside the existing battle targeting seams.
- Modify `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`: make `NativePaint` call the tested placement calculation, remove the direction-based offset, remove temporary Win32/paint diagnostics, and preserve stage-space hover resolution.
- Modify `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp`: add a focused automation test covering horizontal, vertical, and diagonal targeting directions.
- Read/verify `scripts/gamexxk_battle_target_art_check.py`: confirm the existing arrow/dab assets are intact; do not modify them.
- Run `scripts/gamexxk_real_play_flow_mcp.py`: produce real-PIE targeting screenshots and verify pointer/preview behavior; do not modify the harness.
- Preserve screenshots under `Saved/Codex` and the JSON report under `Saved/HarnessReports`; these are verification evidence, not source files to commit.

### Task 1: Add a focused regression seam and prove the current offset fails

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h:463-470`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp:2771-2784`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp:183-190`

- [ ] **Step 1: Preserve the accepted visual baseline and inspect only the relevant dirty paths**

Run:

```powershell
$accepted = 'Saved\Codex\target_outcome_Single.png'
$acceptedBackup = 'Saved\Codex\arrow_alignment_accepted_baseline.png'
if (-not (Test-Path -LiteralPath $accepted)) { throw "Missing accepted baseline: $accepted" }
Copy-Item -LiteralPath $accepted -Destination $acceptedBackup -Force
git status --short -- 'Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h' 'Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp' 'Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp'
git diff -- 'Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp'
```

Expected: the backup exists; the current C++ diff contains the temporary Win32/cursor diagnostics, `[ArrowPaint]` logging, `ArrowHeadTipOffsetRatio`, and the independent `TryResolveCardTargetUnitAtStagePosition` correction. Do not revert the stage-space correction.

- [ ] **Step 2: Declare the production-connected placement seam**

Add beside the existing targeting test seams in `GameXXKBattleBoardWidget.h`:

```cpp
	/** Returns the unrotated arrow brush top-left used by NativePaint. */
	static FVector2D ResolveTargetingArrowHeadTopLeftForTest(
		FVector2D TargetingStart,
		FVector2D TargetingEnd,
		FVector2D ArrowSize);
```

- [ ] **Step 3: Extract the current faulty placement without changing its behavior**

Add this definition near the existing targeting `ForTest` methods in `GameXXKBattleBoardWidget.cpp`:

```cpp
FVector2D UGameXXKBattleBoardWidget::ResolveTargetingArrowHeadTopLeftForTest(
	const FVector2D TargetingStart,
	const FVector2D TargetingEnd,
	const FVector2D ArrowSize)
{
	const FVector2D Delta = TargetingEnd - TargetingStart;
	const float Distance = Delta.Size();
	if (Distance <= KINDA_SMALL_NUMBER)
	{
		return TargetingEnd - ArrowSize * 0.5f;
	}

	static constexpr float ArrowHeadTipOffsetRatio = (1082.0f - 627.0f) / 1254.0f;
	return TargetingEnd
		- (Delta / Distance) * (ArrowSize.X * ArrowHeadTipOffsetRatio)
		- ArrowSize * 0.5f;
}
```

Replace only the arrow-position calculation in `NativePaint`; retain the existing rotation calculation:

```cpp
	const FVector2D ArrowSize(74.0f, 56.0f);
	const FVector2D ArrowPosition = ResolveTargetingArrowHeadTopLeftForTest(Start, End, ArrowSize);
	FSlateBrush ArrowBrush = BuildTextureBrush(TargetingArrowHeadTexture.Get(), ArrowSize, FLinearColor(1.0f, 1.0f, 1.0f, 0.96f));
```

This is a behavior-preserving extraction: the visual must still be wrong at this checkpoint so the new test can demonstrate the regression.

- [ ] **Step 4: Write the failing direction-invariance automation test**

Insert before `FGameXXKBattleBoardWidgetTest` in `GameXXKBattleBoardWidgetTest.cpp`:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleTargetArrowAlignmentTest,
	"GameXXK.MVP.Battle.TargetArrowAlignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleTargetArrowAlignmentTest::RunTest(const FString& Parameters)
{
	const FVector2D End(640.0f, 360.0f);
	const FVector2D ArrowSize(74.0f, 56.0f);
	const FVector2D ExpectedTopLeft(603.0f, 332.0f);

	const auto VerifyDirection = [this, End, ArrowSize, ExpectedTopLeft](
		const TCHAR* Label,
		const FVector2D Start)
	{
		const FVector2D TopLeft = UGameXXKBattleBoardWidget::ResolveTargetingArrowHeadTopLeftForTest(
			Start,
			End,
			ArrowSize);
		TestTrue(
			FString::Printf(TEXT("%s targeting keeps the brush top-left centered on End"), Label),
			TopLeft.Equals(ExpectedTopLeft, 0.01f));
		TestTrue(
			FString::Printf(TEXT("%s targeting keeps the brush center equal to End"), Label),
			(TopLeft + ArrowSize * 0.5f).Equals(End, 0.01f));
	};

	VerifyDirection(TEXT("horizontal"), FVector2D(320.0f, 360.0f));
	VerifyDirection(TEXT("vertical"), FVector2D(640.0f, 40.0f));
	VerifyDirection(TEXT("diagonal"), FVector2D(320.0f, 120.0f));
	return true;
}
```

- [ ] **Step 5: Save UE packages, stop PIE, close the project editor safely, and cold-build the RED checkpoint**

Run from the repository root:

```powershell
python -c "from scripts.ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(timeout=120.0); assert c.connect(); c.stop_pie(); assert c.wait_for_pie_state(False); r=c.save_dirty_packages(); print(r); assert r.get('save_result') and not r.get('dirty_after')"
python -c "from scripts.ue_tdd_pipeline import kill_editor; assert kill_editor()"
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload
```

Expected: MCP reports `save_result=True` with an empty `dirty_after`; the project editor closes; UBT exits `0` and reports a successful `GameXXKEditor` build. Do not use Live Coding or Hot Reload.

- [ ] **Step 6: Run the focused automation and confirm it fails for the intended reason**

Run:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.Battle.TargetArrowAlignment;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: `GameXXK.MVP.Battle.TargetArrowAlignment` is discovered but fails the horizontal, vertical, and diagonal center/top-left assertions. A compile error, missing-test message, crash, or unrelated failure does not count as the required RED result.

### Task 2: Restore center-on-end placement, clean diagnostics, and make the suite green

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp:1-10, 2307-2379, 2713-2741, 2771-2784, 3058-3088, 4633-4670`
- Test: `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp`

- [ ] **Step 1: Replace the faulty helper with the minimal approved calculation**

Replace the complete helper body with:

```cpp
FVector2D UGameXXKBattleBoardWidget::ResolveTargetingArrowHeadTopLeftForTest(
	const FVector2D TargetingStart,
	const FVector2D TargetingEnd,
	const FVector2D ArrowSize)
{
	static_cast<void>(TargetingStart);
	return TargetingEnd - ArrowSize * 0.5f;
}
```

`NativePaint` must continue calling this helper, and `Direction` must continue to control only `MakeRotatedBox` rotation and the ink curve normal.

- [ ] **Step 2: Remove investigation-only diagnostics**

Delete the Windows platform include block:

```cpp
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif
```

Delete the recurring block that logs `[ArrowPaint]`. Restore `GetBattleBoardDebugStateForTest` to its compact, platform-independent form:

```cpp
FString UGameXXKBattleBoardWidget::GetBattleBoardDebugStateForTest() const
{
	const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* const State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const FGameXXKPendingCardChoice* PendingChoice = State
		&& State->Screen == EGameXXKScreen::Battle
		&& State->CardRun.bHasActiveCardBattle
		? &State->CardRun.ActiveBattle.Deck.PendingChoice
		: nullptr;
	return FString::Printf(
		TEXT("token=%llu visuals=%d proxies=%d queue=%d commit=%d deferred=%d pending=%d mode=%d targeting=%d screen=%d choice=%d choiceCandidates=%d atlasPins=%d alive=%d"),
		ActiveBattleVisualSessionToken,
		UnitVisuals.Num(),
		UnitTargetProxies.Num(),
		BattlePresentationQueue.Num(),
		bPlayedCardCommitActive ? 1 : 0,
		static_cast<int32>(DeferredBattlePresentationContinuation),
		IsBattlePresentationPending() ? 1 : 0,
		static_cast<int32>(InteractionMode),
		IsCardTargetingActive() ? 1 : 0,
		State ? static_cast<int32>(State->Screen) : -1,
		PendingChoice ? static_cast<int32>(PendingChoice->Kind) : -1,
		PendingChoice ? PendingChoice->Candidates.Num() : -1,
		PinnedUnitAtlasPaths.Num(),
		GAliveBattleBoardInstances);
}
```

- [ ] **Step 3: Confirm the independent stage-space correction remains intact**

The relevant `UpdateTargetingPointer` branch must remain:

```cpp
			if (TryResolveCardTargetUnitAtStagePosition(ScreenPosition, ResolvedUnitId)
				&& LegalCardTargetUnitIds.Contains(ResolvedUnitId))
```

Run:

```powershell
rg -n 'ArrowHeadTipOffsetRatio|\[ArrowPaint\]|GetCursorPos|Windows\.h' 'Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp'
rg -n 'TryResolveCardTargetUnitAtStagePosition\(ScreenPosition' 'Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp'
git diff --check -- 'Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h' 'Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp' 'Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp'
```

Expected: the first search has no results, the second finds exactly the stage-space target resolution call, and `git diff --check` is clean.

- [ ] **Step 4: Cold-build and rerun the focused automation**

Run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.Battle.TargetArrowAlignment;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected: UBT exits `0`; the focused test reports `Success` with all six direction-invariance assertions passing.

- [ ] **Step 5: Run the adjacent board test and deterministic targeting-art check**

Run:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.Battle.TargetArrowAlignment+GameXXK.MVP.Battle.BoardWidget;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
python scripts/gamexxk_battle_target_art_check.py
```

Expected: both automation tests report `Success`; the art check reports the approved arrow source plus all 12 ink-dab pieces valid. No texture or `.uasset` changes are required.

- [ ] **Step 6: Review and commit only the approved source/test scope**

Run:

```powershell
git diff -- 'Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h' 'Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp' 'Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp'
git add -- 'Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h' 'Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp' 'Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp'
git diff --cached --check
git diff --cached --stat
git commit -m 'fix: restore battle target arrow alignment'
```

Expected staged content: the tested placement seam, its automation test, center-on-end production use, cleanup of temporary diagnostics, and preservation of stage-space hover resolution. Do not stage unrelated dirty or untracked files.

### Task 3: Verify the repaired composition in a fresh real PIE session

**Files:**
- Verify: `Saved/Codex/arrow_alignment_accepted_baseline.png`
- Create verification evidence: `Saved/Codex/arrow_alignment_repaired_single.png`
- Create verification report: `Saved/HarnessReports/battle-target-arrow-alignment-final-20260815.json`
- Preserve: `Saved/Codex/target_outcome_Single.png` as the original accepted capture after comparison

- [ ] **Step 1: Run the project no-Live-Coding pipeline and start a fresh MCP editor**

Run:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 1 --log-lines 120
```

Expected: dirty packages are saved through MCP before any existing editor closes, cold UBT succeeds or reports up-to-date, a fresh UE 5.8 editor starts, PIE starts and stops cleanly, and the pipeline returns success.

- [ ] **Step 2: Run the real target-outcome flow across multiple targeting directions**

Run:

```powershell
python scripts/gamexxk_real_play_flow_mcp.py --target-outcome-preview --report 'Saved/HarnessReports/battle-target-arrow-alignment-final-20260815.json'
```

Expected: top-level `ok`, `target_outcome_preview_verdict.ok`, and `cleanup.ok` are all `true`; `Outcome.Single`, `Outcome.Healing`, and the other scenarios each record `ok=true` and a screenshot. The harness restores its save-game backup and clears fixtures.

- [ ] **Step 3: Preserve the repaired capture and restore the canonical accepted screenshot**

Run:

```powershell
$accepted = 'Saved\Codex\target_outcome_Single.png'
$acceptedBackup = 'Saved\Codex\arrow_alignment_accepted_baseline.png'
$repaired = 'Saved\Codex\arrow_alignment_repaired_single.png'
if (-not (Test-Path -LiteralPath $acceptedBackup)) { throw "Missing accepted backup: $acceptedBackup" }
if (-not (Test-Path -LiteralPath $accepted)) { throw "Missing repaired harness capture: $accepted" }
Copy-Item -LiteralPath $accepted -Destination $repaired -Force
Copy-Item -LiteralPath $acceptedBackup -Destination $accepted -Force
```

Expected: both accepted and repaired images exist under distinct names, while `target_outcome_Single.png` again contains the original accepted baseline.

- [ ] **Step 4: Perform the visual acceptance comparison**

Open these files with the local image viewer:

```text
Saved/Codex/arrow_alignment_accepted_baseline.png
Saved/Codex/arrow_alignment_repaired_single.png
Saved/Codex/target_outcome_Healing.png
```

Accept only when:

- the repaired orange arrow occupies the same endpoint relationship to the ink dabs as the accepted baseline;
- there is no extra down-right gap in `Outcome.Single`;
- changing toward the allied target in `Outcome.Healing` rotates the head without reintroducing a direction-dependent translation;
- the ink curve, dab order, tooltip placement, and cursor-follow behavior remain otherwise unchanged.

If any criterion fails, do not alter the textures. Return to Task 1 with the observed coordinates and add a failing geometry assertion before changing production code again.

- [ ] **Step 5: Record final repository and evidence state**

Run:

```powershell
git status --short --branch
git log -3 --oneline
Get-Item -LiteralPath 'Saved\Codex\arrow_alignment_accepted_baseline.png','Saved\Codex\arrow_alignment_repaired_single.png','Saved\HarnessReports\battle-target-arrow-alignment-final-20260815.json' | Select-Object FullName,Length,LastWriteTime
```

Expected: the implementation commit is present; no unrelated files were staged or committed; all three evidence files exist and are non-empty. Report any pre-existing dirty paths separately rather than claiming a clean worktree.
