# NPC Character Spawn Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make quest, merchant, and follower NPCs real hero-style Character/Pawn blueprint actors instead of `GameXXKTownNpcActor` shells with visual child characters.

**Architecture:** Add `AGameXXKTownNpcCharacter` as an NPC-specific subclass of `AGameXXKHeroCharacter` that implements `IGameXXKInteractable` and owns role/follow/interaction behavior directly. Reparent `BP_NpcCharacter` and `BP_MerchantCharacter` to that class, and make `AGameXXKMVPGameMode` spawn those blueprint classes directly. Keep `AGameXXKTownNpcActor` available for old tests/assets, but remove it from the MVP runtime spawn path.

**Tech Stack:** Unreal Engine C++, Paper2D flipbooks, UE Python asset assembly, UE MCP runner, UBT automation tests.

---

### Task 1: Lock The Spawn Contract

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPPlayableShellTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTownShellTest.cpp`

- [x] **Step 1: Write the failing tests**

Add assertions that `AGameXXKMVPGameMode` exposes quest/merchant NPC classes as `AGameXXKHeroCharacter` children and not `AGameXXKTownNpcActor` children. Add a town-shell behavior test that spawns `AGameXXKTownNpcCharacter`, verifies `CanOfferQuest`, `CanTrade`, `ActivateFollower`, `DismissFollower`, and input-follow movement.

- [x] **Step 2: Run test to verify it fails**

Run:

```powershell
& 'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NullRHI -ExecCmds='Automation RunTests GameXXK.MVP.PlayableShell.GameModeDefaults;Automation RunTests GameXXK.MVP.Town.ShellInputInteractionFollower;Quit' -TestExit='Automation Test Queue Empty'
```

Expected: FAIL because `AGameXXKTownNpcCharacter` and direct NPC blueprint class accessors do not exist yet.

### Task 2: Add NPC Character Runtime Class

**Files:**
- Create: `Source/GameXXK/Public/Town/GameXXKTownNpcCharacter.h`
- Create: `Source/GameXXK/Private/Town/GameXXKTownNpcCharacter.cpp`
- Modify: `Source/GameXXK/Private/Interaction/GameXXKInteractionComponent.cpp`

- [x] **Step 1: Implement minimal class**

Create `AGameXXKTownNpcCharacter : public AGameXXKHeroCharacter, public IGameXXKInteractable`. Copy the existing role, follow, prompt, and default interaction behavior from `AGameXXKTownNpcActor`. In the constructor, disable auto possess, keep ticking enabled, and keep the inherited hero visual/camera structure intact.

- [x] **Step 2: Route interaction**

Allow `UGameXXKInteractionComponent` to call `Interact_Implementation` directly on `AGameXXKTownNpcCharacter`, matching the existing exact-class routing for `AGameXXKTownNpcActor`.

### Task 3: Spawn NPC Character Blueprints Directly

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPGameMode.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPGameMode.cpp`
- Modify: `Content/Python/gamexxk_assemble_npc_character_visuals.py`

- [x] **Step 1: Update GameMode**

Replace visual-class properties with NPC-character-class properties. Load `/Game/GameXXK/Characters/Follower/BP_NpcCharacter.BP_NpcCharacter_C` and `/Game/GameXXK/Characters/Merchant/BP_MerchantCharacter.BP_MerchantCharacter_C` as `TSubclassOf<AGameXXKTownNpcCharacter>`, then spawn those classes directly with roles.

- [x] **Step 2: Update UE Python assembly**

Ensure `BP_NpcCharacter` and `BP_MerchantCharacter` are reparented to `/Script/GameXXK.GameXXKTownNpcCharacter` while preserving their Paper2D idle/walk flipbook defaults and disabled auto possess settings.

### Task 4: Update Runtime Probes

**Files:**
- Modify: `Content/Python/gamexxk_probe_real_play_flow.py`
- Modify: `scripts/gamexxk_real_play_flow_mcp.py`
- Modify: `Content/Python/gamexxk_validate_npc_character_visuals.py`

- [x] **Step 1: Probe direct NPC Character actors**

Report NPCs whose class is `BP_NpcCharacter_C`, `BP_MerchantCharacter_C`, or `GameXXKTownNpcCharacter`; include role, flipbook, collision, possession, and location from the actor itself.

- [x] **Step 2: Validate real flow**

Require quest/merchant/follower NPC actor classes to match the expected blueprint classes directly. Remove the old requirement for `visual_character` child actors.

### Task 5: Verify Through UE

**Files:**
- Modify: `docs/verification/2026-07-02-npc-character-visuals.md`

- [x] **Step 1: Build**

Run UBT through `scripts/ue_tdd_pipeline.py` and require success.

- [x] **Step 2: Assemble assets through UE MCP**

Run `Content/Python/gamexxk_assemble_npc_character_visuals.py` through UE MCP and require both NPC blueprints to report `parent_class` as `GameXXKTownNpcCharacter`.

- [x] **Step 3: Run automation and real play flow**

Run `GameXXK.MVP.PlayableShell.GameModeDefaults`, `GameXXK.MVP.Town.ShellInputInteractionFollower`, `scripts/gamexxk_real_play_flow_mcp.py --timeout 90`, and `scripts/gamexxk_npc_character_visual_check.py`. Record the generated report path and direct NPC class evidence in the verification doc.
