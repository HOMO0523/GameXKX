---
unit_id: 2026-07-03-playable-entry-flow
status: verified
owner: codex
updated_at: 2026-07-08T10:57:58+08:00
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

## 2026-07-06 GameXXK-Owned Route Map

- Stopped treating the original 1Game route PlayerController as gameplay authority for this slice. `UGameXXKMVPRules` now generates deterministic, seed-backed route graphs owned by GameXXK runtime state.
- Added `GameXXK.MVP.RouteMap.SeedRules` to cover deterministic seed signatures, nonzero stored seeds, branching, reachable/visited/pending node state, and combat victory resolution.
- Hardened seed normalization for `0`, ordinary negative seeds, and `MIN_int32`.
- Reworked `UGameXXKOneGameRouteMapWidget` into a GameXXK-owned route map that uses 1Game visual assets for the background, icons, and lines while keeping node state and clicks in `GameXXKMVPCommandRouter`.
- Added scrollable route content sizing, initial scroll-to-reachable-node behavior, background texture diagnostics, and rendered-node hit-box diagnostics.
- Stabilized route widget refresh so repeated state updates reuse existing route children unless rendered node count, rendered line count, or visual mode changes.
- Updated the 1Game battle bridge so `L_Battle_1Game` can open the battle board directly from a live GameXXK `Battle` runtime state, while preserving the old isolated/manual 1Game fallback when the live subsystem is not in Battle.
- Added UE MCP ownership validation for `L_RouteMap`: `scripts/gamexxk_validate_owned_route_map_mcp.py` runs `Content/Python/gamexxk_validate_owned_route_map.py` and verifies the map GameMode is GameXXK-owned.

## 2026-07-08 Battle Targeting And Town Inventory

- Added the first battle command-menu anchoring pass; it was later corrected to use selected party-unit screen position rather than cursor/click position.
- Added PPT-backed item definitions for `金疮药`, `灵芝散`, `清心茶`, `鹤羽香囊`, `青锋短剑`, `竹编轻甲`, `鹤纹护符`, and `墨砚坠饰`.
- Changed equipment application to use explicit weapon/armor/accessory slots plus stat recalculation from level base stats, preventing repeated equip calls from stacking bonuses.
- Added arbitrary consumable use, MP restore, accessory equipment, and failure-return full HP/MP restore in `UGameXXKMVPRules`.
- Changed merchant `F` interaction to open the town shop panel instead of auto-buying a healing item; explicit buy/sell remains routed through the shop/trade APIs.
- Added Town Overlay panel states for backpack, character, shop, and close. The backpack panel exposes 20 slots and uses the imported ink backpack slot texture.
- Generated water-ink inventory/equipment source art and imported nine Texture2D assets under `/Game/GameXXK/UI/Inventory/Textures`.
- Updated `scripts/gamexxk_mvp_playtest.py` so MCP port-binding log noise from a concurrently running editor no longer causes a false automation failure when all automation tests pass.

## 2026-07-08 Battle Command Anchor And Unified Inventory UX

- Corrected the previous battle command-menu anchor approach: party commands now anchor from the selected party unit screen position, not the mouse/click point. Real right-click flow projects the party actor PaperFlipbook bounds before opening the menu, so the command menu stays beside the selected hero/follower instead of drifting into the enemy area.
- Reduced the battle command menu width from `360` to `260` and kept the vertical menu centered on the selected party unit. Targeting arrows now start from the selected unit point rather than the right-click coordinate.
- Added a second command-anchor guard for embedded PIE: if the projected selected-party position still lands on the enemy/left side of the battle canvas, the battle board corrects the command source back into the right-side party lane before placing the menu and targeting arrow.
- Fixed battle targeting pointer coordinates for embedded PIE/editor windows: targeting now converts Slate absolute mouse coordinates into BattleBoard-local coordinates before painting the ink-dab Bezier arrow, so the arrow end tracks the visible cursor instead of drawing along the top of the viewport.
- Tuned the battle command menu default anchor offset to `(-500, 0)` from the selected party unit position, leaving the existing viewport clamp and right-side fallback in place for narrow layouts.
- Added `I` key handling in `AGameXXKMVPPlayerController`: town `I` toggles the unified inventory panel open/closed through the same runtime `TownPanelMode` state used by buttons.
- Rebuilt `UGameXXKTownOverlayWidget` panel structure around a shared inventory backplate, fixed 72x72 backpack slot wrappers, runtime item labels, and explicit weapon/armor/accessory equipment slots.
- Changed town shop mode to display merchant stock beside the same shared player backpack grid instead of presenting shop as a separate text-only inventory surface.
- Disabled unity build for the `GameXXK` module because existing anonymous-namespace helper names collide when multiple `.cpp` files are folded into `Module.GameXXK.cpp`; non-unity builds keep each translation unit isolated and restore cold UBT verification stability.
