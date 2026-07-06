---
unit_id: 2026-07-03-playable-entry-flow
status: verified
owner: codex
updated_at: 2026-07-04T04:07:00+08:00
source_commit: working-tree
depends_on: []
parallel_lock: GameXXK.PlayableEntryFlow
---

# Test Results

- `Build.bat GameXXKEditor Win64 Development -Project="D:\UE5 demo\GameXXK\GameXXK.uproject" -WaitMutex -NoHotReload` passed.
- `GameXXK.MVP.SaveGame` passed: 1/1.
- `GameXXK.MVP.PlayableShell.GameModeDefaults` passed: 1/1.
- `GameXXK.MVP.UI` passed: 4/4.
- `python -m py_compile scripts/harness_state_validator.py scripts/ai_production_loop.py scripts/gamexxk_real_play_flow_mcp.py Content/Python/gamexxk_probe_real_play_flow.py` passed.
- `python scripts/ue_mcp_smoke.py --port 18765 --tool-timeout 10 --report Saved\HarnessReports\ue-mcp-smoke-player-save-route-fix.json` passed.
- `python scripts/gamexxk_real_play_flow_mcp.py --timeout 180 --report Saved\HarnessReports\real_flow_after_player_save_route_fix.json` passed with `ok: true`.

Real-flow evidence: main menu visible, Start opens `L_QingshanInn`, town overlay visible, top-down camera present, WASD moves 445.56 cm, East and NorthEast walk/idle flipbooks switch correctly, F accepts quest, task NPC follows 95.44 cm with `FB_Npc_Walk_East`, manual save records player location with distance `0.0`, route map widget becomes `VISIBLE`, route node 1 opens battle and battle board is `VISIBLE`.

## 2026-07-04 North-Gate Route Entry

- `Build.bat GameXXKEditor Win64 Development -Project="D:\UE5 demo\GameXXK\GameXXK.uproject"` passed after closing the editor through UE MCP; direct build while Live Coding was active was rejected by UBT as expected and was not used as verification.
- MCP automation passed 5/5: `GameXXK.MVP.PlayableShell.GameModeDefaults`, `GameXXK.MVP.SaveGame.SlotRoundTrip`, `GameXXK.MVP.Town.ShellInputInteractionFollower`, `GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets`, `GameXXK.MVP.UI.TownOverlayCommands`.
- `python scripts/gamexxk_real_play_flow_mcp.py` passed with `ok: true`; report: `Saved\HarnessReports\gamexxk-real-play-flow-20260704-012705.json`.

Real-flow evidence: `Start` opens `L_QingshanInn`; WASD/diagonal walk-idle checks pass; `F` accepts the quest; manual save records player and quest NPC locations with distance `0.0`; harness walks to `QingshanInn_TownExit` at `(0, 1380, 120)`, approaches `(0, 1060, 120)`, presses `F`, enters `DUNGEON_MAP`, route-map widget is `VISIBLE`, town overlay is `COLLAPSED`, route start node advances, battle node opens `BATTLE`, and battle board is `VISIBLE`.

## 2026-07-04 Independent Route/Battle Map Load

- RED verified: adding `GameXXK.MVP.LevelFlow` first failed compilation because `MVP/GameXXKLevelFlow.h` did not exist.
- RED verified: adding `GameXXK.MVP.FlowMap.GameModeDefaults` first failed compilation because `MVP/GameXXKFlowMapGameMode.h` did not exist.
- `python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 80` passed after implementing `GameXXKLevelFlow`, `GameXXKFlowMapGameMode`, command-router travel, and town-exit travel.
- UE MCP asset copy/configuration passed in phases:
  - `Content/Python/gamexxk_ensure_1game_battle_bridge_assets.py --phase copy`
  - `--phase configure-route`
  - `--phase configure-battle`
- `python scripts\gamexxk_real_play_flow_mcp.py` passed with `ok: true`; report: `Saved\HarnessReports\gamexxk-real-play-flow-20260704-023304.json`.
- `git diff --check` passed with line-ending warnings only.
- `python -m py_compile scripts\gamexxk_real_play_flow_mcp.py Content\Python\gamexxk_ensure_1game_battle_bridge_assets.py` passed.
- `python scripts\harness_state_validator.py --require-units --json` passed.
- UE MCP `save_dirty_packages` returned `dirty_before=[]` and `dirty_after=[]`.

Real-flow evidence: north-gate `F` loads `L_RouteMap` with runtime screen `DUNGEON_MAP`, route-map widget `VISIBLE`, and town overlay `COLLAPSED`; clicking route node 0 advances the route; clicking route node 1 loads `L_Battle_1Game` with runtime screen `BATTLE` and battle board `VISIBLE`.

## 2026-07-04 PIE Map-Match Hardening

- RED verified: `GameXXK.MVP.LevelFlow` first failed compilation with unresolved `GameXXKLevelFlow::MapPackageMatches`.
- `python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 60` passed after stripping `UEDPIE_N_` prefixes from current-map comparisons.
- `python scripts\gamexxk_real_play_flow_mcp.py` passed with `ok: true`; report: `Saved\HarnessReports\gamexxk-real-play-flow-20260704-024414.json`.
- `python -m py_compile scripts\gamexxk_real_play_flow_mcp.py Content\Python\gamexxk_ensure_1game_battle_bridge_assets.py` passed.
- `git diff --check` passed with line-ending warnings only.
- `python scripts\harness_state_validator.py --require-units --json` passed.
- Current `Saved\Logs\GameXXK.log` search found no new `Fatal error` or `World Memory Leaks`.
- UE MCP `save_dirty_packages` returned `dirty_before=[]` and `dirty_after=[]`.

Real-flow evidence: Start opens `L_QingshanInn`; `F` accepts quest; manual save records player and quest NPC locations with distance `0.0`; north-gate `F` loads `L_RouteMap`; route node 0 advances in-place without reopening the route map; route node 1 loads `L_Battle_1Game` with runtime screen `BATTLE` and battle board `VISIBLE`.

## 2026-07-04 Direct 1Game UI Bridge

- RED verified: `Content/Python/gamexxk_validate_testmap_compat.py` failed because `/Script/TestMap.MyBlueprintFunctionLibrary` was missing.
- `python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 80` passed after adding the `TestMap` compatibility module.
- `Content/Python/gamexxk_validate_testmap_compat.py` passed: `/Script/TestMap.MyBlueprintFunctionLibrary` is loadable and the all-zero weighted index call returns `0`.
- `Content/Python/gamexxk_validate_1game_ui_blueprint_compile.py` passed: `UI_地图选择`, node, line, and boss widgets compile to `BS_UP_TO_DATE`.
- `Content/Python/gamexxk_validate_1game_battle_ui_bridge.py` passed: `L_Battle_1Game` uses `BP_1GameBattleGameMode_C`, and that copied GameMode uses `BP_1GameBattlePlayerController_C`.
- `python -m py_compile scripts\gamexxk_real_play_flow_mcp.py Content\Python\gamexxk_ensure_1game_battle_bridge_assets.py Content\Python\gamexxk_validate_testmap_compat.py Content\Python\gamexxk_validate_1game_ui_blueprint_compile.py Content\Python\gamexxk_validate_1game_battle_ui_bridge.py Content\Python\gamexxk_probe_active_widgets.py` passed.
- `python scripts\gamexxk_real_play_flow_mcp.py` passed with `ok: true`; report: `Saved\HarnessReports\gamexxk-real-play-flow-20260704-032047.json`.

Visual evidence: `Saved\Codex\real_flow_after_battle.png` shows the copied 1Game scroll-map UI after the battle route node loads `L_Battle_1Game`; the active PIE player controller is `BP_1GameBattlePlayerController_C`.

## 2026-07-04 Route Node Click Adapter

- RED verified: `python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 40` failed compilation after adding the sparse route-node-id test because `UGameXXKOneGameRouteMapWidget` did not yet expose `GetRouteNodeVisualStatesForTest` or `ExecuteRouteNodeById`.
- `python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 60` passed after adding real-node-id execution and route-node visual-state diagnostics.
- MCP automation passed `GameXXK.MVP.RouteMap.OneGameAdapter`: 1 passed / 0 failed.
- `python -m py_compile Content\Python\gamexxk_probe_real_play_flow.py scripts\gamexxk_real_play_flow_mcp.py` passed.
- `python scripts\gamexxk_real_play_flow_mcp.py` passed with `ok: true`; report: `Saved\HarnessReports\gamexxk-real-play-flow-20260704-040650.json`.

Real-flow evidence: route-map probe exposes `route_node_visual_states`; visible Start node has `node_id=0`, `command_name=RouteNode0`, and a Slate hit center. The harness clicked Start at absolute screen `(925, 470)`, advancing `dungeon_node_index` to `1`; it then clicked Battle node `1` at `(887, 432)`, opening `L_Battle_1Game` with runtime screen `BATTLE`.

## 2026-07-06 GameXXK-Owned Route Map

- RED verified: `python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 40` failed after adding `GameXXK.MVP.RouteMap.SeedRules` because `UGameXXKMVPRules::GenerateRouteMapForSeed` did not exist.
- RED verified: route widget visual tests failed compilation before `HasRouteBackgroundVisualForTest`, `GetRouteBackgroundTexturePathForTest`, `GetRouteContentSizeForTest`, and `GetLastAppliedScrollOffsetForTest` existed.
- `python scripts\ue_tdd_pipeline.py --pie-duration 0.1 --log-lines 80` passed: UBT up to date, editor launched, MCP became ready, PIE started and stopped.
- `python scripts\gamexxk_validate_owned_route_map_mcp.py` passed with `ok: true`; `L_RouteMap` GameMode is `/Script/GameXXK.GameXXKFlowMapGameMode`.
- `python scripts\harness_state_validator.py` passed with `OK`.
- `python scripts\gamexxk_mvp_playtest.py --test-timeout 600` succeeded at build after closing the editor through UE MCP (`save_dirty_packages` returned `dirty_before=[]`, `dirty_after=[]`). Report: `Saved\HarnessReports\gamexxk-mvp-playtest-20260706-180305.json`.
- Target automation passed: `GameXXK.MVP.RouteMap.SeedRules`, `GameXXK.MVP.RouteMap.OneGameAdapter`, `GameXXK.MVP.OneGameIslandRouteMapBridge`, `GameXXK.MVP.Battle.BoardWidget`, and `GameXXK.MVP.SaveGame.SlotRoundTrip`.
- Broad `GameXXK.MVP` still exits nonzero because old all-flow tests assume a fixed linear route sequence and legacy save expectations: failing tests are `GameXXK.MVP.FullFlow`, `GameXXK.MVP.PIE.PlayableRootCommandsDriveFullLoop`, and `GameXXK.MVP.PlayableShell.HUDCommandsDriveFullLoop`.

Acceptance evidence: GameXXK now owns route seed/state/clicks; the route widget renders 1Game-inspired background/icons/lines through GameXXK UMG; battle-map entry can use a live GameXXK `Battle` runtime state; and `L_RouteMap` is verified by UE MCP as GameXXK GameMode-owned.
