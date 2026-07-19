# Town MCP Key-State Input Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the existing real-PIE harness hold town movement through the hero's persistent key-state path, then verify the full menu-to-battle flow.

**Architecture:** `AGameXXKHeroCharacter` exposes a narrow reflected key-state dispatcher that reuses its existing D/W/A/S pressed/released functions. The project Python probe serializes that dispatcher for MCP, while the existing flow harness owns duration, release ordering, and report events. No game input binding or HUD behavior changes.

**Tech Stack:** UE 5.8 C++, Unreal Automation Framework, project Python through GameXXK MCP, Python `unittest`, UBT cold build.

---

### Task 1: Hero key-state automation seam

**Files:**
- Modify: `Source/GameXXK/Public/Town/GameXXKHeroCharacter.h`
- Modify: `Source/GameXXK/Private/Town/GameXXKHeroCharacter.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKTownShellTest.cpp`

- [ ] **Step 1: Write the failing automation test**

Add assertions after the existing hero movement checks:

```cpp
TestNotNull(TEXT("automation key seam is reflected"),
    HeroCharacter->FindFunctionByName(TEXT("SetTownAutomationKeyState")));
TestTrue(TEXT("automation D press is accepted"),
    HeroCharacter->SetTownAutomationKeyState(EKeys::D.GetFName(), true));
HeroCharacter->MoveHorizontal(0.0f);
TestTrue(TEXT("zero axis callback does not clear held automation D"), HeroCharacter->IsTownMoving());
TestEqual(TEXT("held automation D keeps right movement intent"),
    HeroCharacter->GetTownMovementIntentVector(), FVector::RightVector);
TestTrue(TEXT("automation D release is accepted"),
    HeroCharacter->SetTownAutomationKeyState(EKeys::D.GetFName(), false));
TestFalse(TEXT("automation D release returns hero to idle"), HeroCharacter->IsTownMoving());
TestFalse(TEXT("unsupported automation key is rejected"),
    HeroCharacter->SetTownAutomationKeyState(TEXT("F"), true));
```

- [ ] **Step 2: Run the target Automation test and verify RED**

Run after the next cold build attempt. Expected initial failure is missing `SetTownAutomationKeyState`.

- [ ] **Step 3: Implement the minimal reflected dispatcher**

Declare and define:

```cpp
UFUNCTION(BlueprintCallable, Category = "GameXXK|Town|Automation")
bool SetTownAutomationKeyState(FName KeyName, bool bPressed);

bool AGameXXKHeroCharacter::SetTownAutomationKeyState(FName KeyName, bool bPressed)
{
    if (KeyName == EKeys::D.GetFName()) { bPressed ? MoveRightPressed() : MoveRightReleased(); return true; }
    if (KeyName == EKeys::A.GetFName()) { bPressed ? MoveLeftPressed() : MoveLeftReleased(); return true; }
    if (KeyName == EKeys::W.GetFName()) { bPressed ? MoveForwardPressed() : MoveForwardReleased(); return true; }
    if (KeyName == EKeys::S.GetFName()) { bPressed ? MoveBackwardPressed() : MoveBackwardReleased(); return true; }
    return false;
}
```

- [ ] **Step 4: Cold-build and run the target Automation test**

Run UBT with `-NoHotReload`, then `GameXXK.MVP.Town.ShellInputInteractionFollower`. Expected: build exit 0 and test succeeds.

### Task 2: Rewire the existing MCP flow script

**Files:**
- Modify: `Content/Python/gamexxk_probe_real_play_flow.py`
- Modify: `scripts/gamexxk_real_play_flow_mcp.py`
- Modify: `scripts/test_gamexxk_real_play_flow_probe.py`
- Modify: `scripts/test_gamexxk_real_play_flow_mcp.py`

- [ ] **Step 1: Write failing Python contracts**

Require `--town-key KEY STATE`, `pawn.set_town_automation_key_state(unreal.Name(key), pressed)`, and harness events with `backend="mcp_project_python"`. Update fake clients to return a `town_key` response. Require the walking source to use a key-hold context manager, never `town_axis` or `PreviewInput` key injection.

- [ ] **Step 2: Run both Python test scripts and verify RED**

```powershell
& 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts/test_gamexxk_real_play_flow_probe.py
& 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts/test_gamexxk_real_play_flow_mcp.py
```

Expected: missing town-key handler/helper assertions.

- [ ] **Step 3: Implement key press/release transport**

The probe validates `down`/`up`, sends `unreal.Name(key)` to the hero dispatcher, and returns a `town_key` payload. The harness adds `town_key()` and `hold_town_keys()`; it sends key-down once, releases successfully pressed keys in reverse order in `finally`, records release errors, preserves a primary error, and raises if release alone fails. Map world-X movement to W/S and world-Y movement to D/A. Use nested holds for W+D delayed release.

- [ ] **Step 4: Run both Python contracts and syntax/diff checks**

Expected: both scripts pass, AST parse succeeds, and `git diff --check` reports no whitespace errors.

### Task 3: Full real-PIE verification

**Files:**
- Output: `Saved/HarnessReports/battle-actor-resource-hud-mcp-key.json`
- Output: `Saved/Codex/real_flow_after_battle.png`

- [ ] **Step 1: Save, stop PIE, and cold build through the project lifecycle**

Use MCP `save_dirty_packages`, stop PIE, then run the project UBT build with `-NoHotReload`. Do not use Live Coding or Hot Reload.

- [ ] **Step 2: Start the editor using the project lifecycle**

Use `scripts/ue_tdd_pipeline.py --no-build` so the editor receives `-DDC-ForceMemoryCache`; smoke-test MCP before running PIE.

- [ ] **Step 3: Run the full harness and inspect evidence**

```powershell
& 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts/gamexxk_real_play_flow_mcp.py --keep-pie --timeout 180 --report 'Saved\HarnessReports\battle-actor-resource-hud-mcp-key.json'
```

Expected: menu, world map, town, visible walk/idle states, quest/NPC follower, route map, battle map, actor-owned resource/status HUD evidence, and a battle screenshot.

- [ ] **Step 4: Save packages and hand off verified results**

Save through MCP, stop PIE if still active, report exact test/build/harness outcomes and screenshot paths.
