# Quest NPC Save State Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Persist only the task NPC's follower state and town position for the playable MVP.

**Architecture:** Extend `FGameXXKRuntimeState` and `FGameXXKSaveState` with task NPC location fields. Let task NPC interaction/following record the position through `UGameXXKMVPSubsystem`, and let `AGameXXKMVPGameMode` apply restored state when it spawns the task NPC.

**Tech Stack:** Unreal Engine 5.8 C++, Automation Tests, UE MCP real-flow harness.

---

### Task 1: Save Data

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp`

- [ ] Write a failing save/load test for `bFollowerJoined`, `bHasQuestNpcLocation`, and `QuestNpcLocation`.
- [ ] Run `GameXXK.MVP.SaveGame.SlotRoundTrip` and confirm it fails on missing persisted task NPC fields.
- [ ] Add the runtime/save fields and copy them in `MakeSaveState` and `RestoreFromSaveState`.
- [ ] Re-run `GameXXK.MVP.SaveGame.SlotRoundTrip` and confirm it passes.

### Task 2: NPC Interaction And Movement Recording

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/Town/GameXXKTownNpcCharacter.cpp`
- Modify: `Source/GameXXK/Private/Town/GameXXKTownNpcActor.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTownShellTest.cpp`

- [ ] Write a failing test that pressing `F` on the task NPC records its location in runtime state and autosave.
- [ ] Add `RecordQuestNpcLocation` on `UGameXXKMVPSubsystem`.
- [ ] Record task NPC location after quest accept and while the task NPC follows.
- [ ] Re-run `GameXXK.MVP.Town.ShellInputInteractionFollower` and confirm it passes.

### Task 3: Restore Into Spawned Town NPC

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPGameMode.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPGameMode.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPPlayableShellTest.cpp`

- [ ] Write a failing test for applying saved task NPC location and follower target to a spawned task NPC.
- [ ] Add a GameMode helper used by `BeginPlay` after spawning the task NPC.
- [ ] Re-run `GameXXK.MVP.PlayableShell.GameModeDefaults`.

### Task 4: Real Flow Verification

**Files:**
- Modify: `Content/Python/gamexxk_probe_real_play_flow.py`
- Modify: `scripts/gamexxk_real_play_flow_mcp.py`

- [ ] Extend the real-flow probe to report task NPC save fields.
- [ ] Require the real-flow follower movement probe to see a saved task NPC location.
- [ ] Run `python scripts/gamexxk_real_play_flow_mcp.py --timeout 120`.
