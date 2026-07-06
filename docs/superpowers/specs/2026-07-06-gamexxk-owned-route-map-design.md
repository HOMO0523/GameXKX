# GameXXK-Owned Route Map Design

## Purpose

Replace the current fragile route-map handoff with a GameXXK-owned route map. The route map should use 1Game visual assets for the background, room icons, and connector styling, but all gameplay ownership must stay in GameXXK C++ and UMG.

This is a stop-loss design. It intentionally avoids more work on the original 1Game PlayerController and original 1Game route-map runtime.

## Current Problem

GameXXK currently has two route-map paths that can be confused:

- `L_RouteMap` is intended to be the GameXXK-controlled route map.
- The copied 1Game route UI and bridge code can still depend on original 1Game runtime state such as its PlayerController `current_level`.

The current GameXXK route widget only creates nodes, lines, and transparent hit boxes. It references 1Game node and line assets, but it does not instantiate a proper background layer. Its current node canvas also compresses the route into a small area, which makes nodes overlap and lines feel crowded.

The fix is not to keep patching the original 1Game PlayerController. The fix is to make `L_RouteMap` a full GameXXK-owned screen.

## Scope

In scope:

- `L_RouteMap` displays a GameXXK-owned route-map UMG widget.
- The widget uses 1Game-style visual assets for background, node icons, disabled icons, and connector lines.
- Route-map state is generated, selected, saved, restored, and advanced through `UGameXXKMVPSubsystem` and `UGameXXKMVPRules`.
- Clicking a battle, elite, or boss node sets the GameXXK pending route node and opens the existing `L_Battle_1Game` battle layout.
- Winning the battle resolves the pending node and returns to `L_RouteMap`.
- The route map opens scrolled to the first reachable node.

Out of scope for this pass:

- Reworking the battle screen itself.
- Reworking town movement, NPC following, or main-menu save-slot layout.
- Adding a full 1Game-equivalent procedural route generator with many floors.
- Continuing to patch original 1Game PlayerController behavior.

## Architecture

`GameXXKLevelFlow` remains the map resolver:

- `Town` -> `/Game/GameXXK/Maps/L_QingshanInn`
- `DungeonMap` -> `/Game/GameXXK/Maps/L_RouteMap`
- `Battle` -> `/Game/GameXXK/Maps/L_Battle_1Game`

`UGameXXKMVPSubsystem` remains the runtime owner. UI widgets never update route progress directly. They call subsystem commands:

- `OpenDungeonFromTownExit()`
- `SelectRouteNodeById(NodeId)`
- `ResolveBattleVictory(bBossBattle)`

`UGameXXKMVPRules` remains the deterministic rules layer. It owns:

- route-map generation
- reachable and visited node sets
- pending battle node semantics
- battle victory completion
- save/restore compatibility through `FGameXXKRuntimeState`

`UGameXXKOneGameRouteMapWidget` becomes the player-facing route-map view. It should be a GameXXK widget that borrows 1Game assets, not an embedded 1Game route-map runtime.

## Route State

The route state is stored in `FGameXXKRuntimeState`:

- `bHasGeneratedRouteMap`
- `RouteSeed`
- `CurrentRouteNodeId`
- `PendingRouteNodeId`
- `RouteMapNodes`
- `RouteMapEdges`
- `VisitedRouteNodeIds`
- `ReachableRouteNodeIds`

The first implementation should make `RouteSeed` real but conservative:

- New dungeon entry assigns a nonzero route seed if the route map is not already active.
- The same seed produces the same route-map nodes and edges.
- Different seeds can change node kinds and branch shape within a small, testable structure.
- Continue/load restores the saved generated nodes and does not regenerate over them.

For MVP stability, generated maps can remain small: start, two or three mid layers, a late layer, and boss. The key requirement is that seed ownership and persistence are correct.

## Route UI Layout

The route widget should use this structure:

- root canvas or overlay
- background image layer
- scroll box viewport
- route content canvas with dynamic height
- connector line layer
- node visual layer
- transparent clickable hit-box layer

The route content canvas should not use the current compressed `360x320` coordinate space. It should calculate content size from route layers:

- fixed route width based on the viewport, with safe side padding
- layer gap large enough for 96px room icons and labels
- content height based on the highest layer index
- node positions derived from `LayerIndex`, `ColumnIndex`, and normalized lane position

The first visible scroll offset should focus the earliest reachable node. If no reachable node exists, it should focus the last visited node or the start node.

## Visual Assets

Use the existing 1Game soft references already collected in `UGameXXKOneGameRouteMapWidget` for room icons and disabled icons.

Add a route background asset reference to the GameXXK route widget. If the exact 1Game parent widget background is hard to extract safely, use a direct texture or material asset rather than instantiating the original 1Game parent widget.

Do not use the original 1Game route-map PlayerController or its `current_level` progression as a source of truth.

## Click Flow

When the player clicks a reachable non-combat node:

1. `UGameXXKOneGameRouteMapWidget` calls `SelectRouteNodeById(NodeId)`.
2. `UGameXXKMVPRules` applies the reward or rest effect.
3. The node is added to `VisitedRouteNodeIds`.
4. Outgoing nodes become reachable.
5. The route widget refreshes in place and remains on `L_RouteMap`.

When the player clicks a reachable combat node:

1. `UGameXXKOneGameRouteMapWidget` calls `SelectRouteNodeById(NodeId)`.
2. `UGameXXKMVPRules` sets `PendingRouteNodeId = NodeId`.
3. Runtime screen becomes `Battle`.
4. `GameXXKLevelFlow` opens `L_Battle_1Game`.
5. Battle victory calls `ResolveBattleVictory`.
6. The pending node is completed.
7. Runtime screen becomes `DungeonMap`.
8. `GameXXKLevelFlow` opens `L_RouteMap`.

The route node is not marked visited until the battle is won.

## Save And Continue

Manual save should persist the route map because `MakeSaveState` stores `RuntimeState`.

Continue should restore:

- current screen
- current map target
- route seed
- route nodes and edges
- visited nodes
- reachable nodes
- pending battle node, if the save happened during a battle

If a save is loaded while `Screen == Battle`, the player should open into `L_Battle_1Game`. If a save is loaded while `Screen == DungeonMap`, the player should open into `L_RouteMap`.

## Tests

Add tests before implementation.

Rules tests:

- entering dungeon assigns a nonzero route seed
- same seed produces the same route nodes and edges
- two different seeds can produce a visible difference
- selecting a non-combat reachable node completes it in-place
- selecting combat sets `PendingRouteNodeId` and changes screen to `Battle`
- battle victory completes the pending node and returns to `DungeonMap`

Widget tests:

- route widget creates a background visual
- route widget creates one node visual per generated route node
- route widget creates one line visual per route edge
- route widget exposes hit boxes only for reachable nodes
- route widget computes a larger dynamic content height instead of the old compressed layout
- route widget records or exposes the initial scroll target near the first reachable node

Flow tests:

- town exit opens `L_RouteMap`
- clicking route start node stays on `L_RouteMap`
- clicking battle node opens `L_Battle_1Game`
- battle victory returns to `L_RouteMap`
- save/load restores route-map state without regenerating a different route

Automation:

- Keep using `scripts/ue_tdd_pipeline.py` and focused Automation tests.
- Use UE MCP scripts only for editor/package validation where needed.
- Do not use UnrealBridge.

## Acceptance

This pass is done when:

- `L_RouteMap` displays a visible 1Game-style background behind GameXXK-owned nodes.
- Nodes are spaced across a scrollable map rather than crowded into the old small canvas.
- Only reachable nodes are clickable.
- Combat clicks open `L_Battle_1Game`.
- Battle victory returns to the same GameXXK route state on `L_RouteMap`.
- Route seed and route progress survive manual save and Continue.
- No acceptance path depends on original 1Game PlayerController route-map logic.

## Risks

The main risk is asset fragility. Loading direct 1Game textures is safer than embedding the original 1Game parent widget. If a desired background only exists inside a Widget Blueprint, extract or reference the underlying image/texture through a small UE MCP asset query instead of depending on that widget's runtime graph.

Another risk is overbuilding the route generator. The first implementation should keep the generator small and deterministic. The important milestone is ownership and persistence, not variety.
