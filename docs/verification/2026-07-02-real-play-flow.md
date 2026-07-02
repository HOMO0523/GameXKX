# 2026-07-02 Real Play Flow Verification

This slice moves MVP acceptance away from the C++/HUD command shell and into the real player path:

- Load the root project and `L_Main`.
- Start PIE through UE MCP.
- Click the rendered `Start / Continue` button in the Preview window.
- Click the rendered `Qingshan Town` button.
- Verify PIE travels to `/Game/GameXXK/Maps/L_QingshanInn`.
- Verify the possessed pawn is editable `/Game/GameXXK/Characters/Hero/BP_HeroCharacter.BP_HeroCharacter_C`.
- Press `D` and verify the hero moves and switches to the East flipbook.
- Walk to the Quest NPC with real WASD key input.
- Press `F` and verify the Quest NPC interaction succeeds and activates the follower.

## Permanent Harness

- UE-side state probe: `Content/Python/gamexxk_probe_real_play_flow.py`
- External UE MCP driver: `scripts/gamexxk_real_play_flow_mcp.py`

The harness does not summon a temporary hero and does not call `ExecuteVisibleCommand` directly. It uses UE MCP for PIE/window screenshots/state probes and command-line Win32 input for real clicks and keys against the Preview window.

## RED

Command:

```powershell
python scripts\gamexxk_real_play_flow_mcp.py --timeout 30
```

Observed result before the fix:

- Start clicked successfully and opened the World Map.
- Qingshan click did not enter a playable town map; PIE remained in `L_Main`.
- After adding real map travel, the harness reached `L_QingshanInn` but `D` changed facing only; hero location stayed unchanged.

## GREEN

Fixes:

- `GameXXKMVPCommandRouter` now opens `L_QingshanInn` after a successful Qingshan world-map selection in a real game world.
- `BP_HeroCharacter`'s native parent now applies top-down XY movement from the current WASD/arrow intent each tick, while preserving 8-direction flipbook switching.

Observed result after the fix:

- Harness result: `"ok": true`
- Real `D` movement distance: about `195 cm`
- Quest NPC distance before F: about `48 cm`
- Quest NPC result: `was_last_interaction_successful=true`, `is_follower_active=true`, `follow_target=BP_HeroCharacter_C_0`

Latest report:

`Saved/HarnessReports/gamexxk-real-play-flow-20260702-145714.json`

## Remaining Boundary

The shell UI is still functional MVP UI, not final presentation. The important acceptance boundary is now explicit: PlayableShell tests are supporting coverage only; the real playable proof is the UE MCP flow that clicks through the Preview window and verifies the live PIE world.
