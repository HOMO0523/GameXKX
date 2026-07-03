---
unit_id: 2026-07-03-playable-entry-flow
status: verified
owner: codex
updated_at: 2026-07-04T04:07:00+08:00
source_commit: working-tree
depends_on: []
parallel_lock: GameXXK.PlayableEntryFlow
---

# Implementation Log

- Created GameXXK production-unit files and root operating guide.
- Runtime/UI code changes are tracked in the working tree.
- Fixed real manual save capture: `UGameXXKMVPSubsystem::SaveCurrentGame` now records the live player pawn location before writing the slot, and `FGameXXKSaveState` mirrors player location for probes/slot inspection.
- Restored saved town player position in `AGameXXKMVPGameMode::BeginPlay` through `ApplyPlayerRuntimeState`.
- Added player-flow widget refresh propagation so TownOverlay, RouteMap, and BattleBoard state-changing commands refresh sibling widgets without manual test-only refresh calls.
- Extended the UE MCP real-flow probe/harness to report and assert saved player location distance.
- Made `QingshanInn_TownExit` the player-facing route-map entry: `AGameXXKMVPGameMode` now guarantees one town exit at `(0, 1380, 120)` and reuses any map-placed exit instead of duplicating it.
- Moved the town exit interaction box to the player-facing side of the north gate so `F` can trigger before the gate mesh.
- Hid the TownOverlay `Enter Route Map` player button while keeping the lower-level command router path for focused automation and rule regression.
- Updated `scripts/gamexxk_real_play_flow_mcp.py` to walk to `QingshanInn_TownExit` and press `F` instead of calling the town `EnterDungeon` command.
- Fixed the town-exit interaction refresh path so successful `F` entry updates the owning player controller widgets and reveals the route map immediately.

## 2026-07-04 Independent Route/Battle Maps

- Added `GameXXKLevelFlow` as the single map resolver for runtime screens: `MainMenu/WorldMap -> L_Main`, `Town -> L_QingshanInn`, `DungeonMap -> L_RouteMap`, and `Battle -> L_Battle_1Game`.
- Added `GameXXKFlowMapGameMode`, a flow-only GameMode for non-town maps that uses `AGameXXKMVPPlayerController`, `AGameXXKMVPHUD`, and `ASpectatorPawn` so route/battle maps do not spawn town NPCs or town exits.
- Updated generated route-node command execution, Continue slot loading, battle victory/failure, and the north-gate town-exit interaction to call `GameXXKLevelFlow::OpenMapForRuntimeState` after state transitions.
- Copied `/Game/1Game/Control/BP_PlayerController` to `/Game/GameXXK/Battle/BP_1GameBattlePlayerController` and reparented the copy to `AGameXXKMVPPlayerController`; the original 1Game controller was left untouched.
- Copied `/Game/GameXXK/Maps/L_Main` to `/Game/GameXXK/Maps/L_RouteMap` and `/Game/1Game/Map/NewWorld` to `/Game/GameXXK/Maps/L_Battle_1Game`, then set both copied maps to `GameXXKFlowMapGameMode`.
- Split the UE MCP asset setup into copy/configure phases after UE 5.8 reported a fatal World leak when duplicating and loading multiple map worlds in one Python call.
- Hardened `GameXXKLevelFlow` map matching for PIE package names such as `/Game/GameXXK/Maps/UEDPIE_0_L_RouteMap`, so route-node commands no longer re-open the already loaded route map.
- Hardened `gamexxk_ensure_1game_battle_bridge_assets.py` to skip `load_map` when the requested editor map is already current.
- Added a small `TestMap` compatibility runtime module that restores `/Script/TestMap.MyBlueprintFunctionLibrary::FunCL_GetRandomArrayToIndex`, matching the UE 5.4 reference project dependency required by `/Game/1Game/UI/UI_地图选择`.
- Re-enabled the copied 1Game battle UI bridge after the missing dependency was restored: `/Game/GameXXK/Battle/BP_1GameBattleGameMode` now points to `/Game/GameXXK/Battle/BP_1GameBattlePlayerController`, and `L_Battle_1Game` uses that copied GameMode.
- Kept a `configure-battle-flow` asset-script phase as a rollback path to restore `L_Battle_1Game` to `GameXXKFlowMapGameMode` if the direct 1Game bridge needs to be disabled again.

## 2026-07-04 Route Node Click Adapter

- Adapted `UGameXXKOneGameRouteMapWidget` so route-node clicks execute by real GameXXK node id through `ExecuteRouteNodeById`, while the old `ExecuteRouteNode(index)` API remains as an index-based compatibility wrapper.
- Added `FGameXXKOneGameRouteNodeVisualState` diagnostics for route node id, concrete `RouteNode<ID>` command, room type, enabled/visited state, icon path, and hit-box geometry.
- Changed route-node button callbacks to use stored node ids instead of assuming node ids and array indices always match.
- Updated `Content/Python/gamexxk_probe_real_play_flow.py` to expose route node visual states to MCP probes.
- Updated `scripts/gamexxk_real_play_flow_mcp.py` so the acceptance harness now clicks the actual visible Start/Battle route-node buttons using Slate absolute hit geometry, instead of calling `execute_route_node` directly.
