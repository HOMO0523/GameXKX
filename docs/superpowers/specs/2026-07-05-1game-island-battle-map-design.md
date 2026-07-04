# 1Game Island Battle Map Design

## Context

The first `L_Battle_1Game` bridge duplicated 1Game assets into `/Game/GameXXK` and reparented the copied player controller to `GameXXKMVPPlayerController`. That made the map load from the GameXXK flow, but it broke the original 1Game route UI contract.

The original `/Game/1Game/UI/UI_地图选择` widget is not a static map. Its designer tree only contains the parchment, title, scroll box, and legend. The route nodes and lines are created at runtime from the widget EventGraph. The widget casts the active player controller to `/Game/1Game/Control/BP_PlayerController_C` and then calls that controller's route events.

The copied GameXXK bridge controller is not a child of the original 1Game controller, so the original widget can display its static shell while its controller-dependent route path fails.

## Evidence

UE MCP probes on 2026-07-04 showed:

- `/Game/1Game/Map/NewWorld` with the original 1Game GameMode generates route nodes and connector lines in PIE.
- `/Game/GameXXK/Maps/L_Battle_1Game` opened temporarily with `?game=/Game/1Game/Control/BP_Gamemode.BP_Gamemode_C` also generates route nodes and connector lines.
- Clicking a visible generated node in that temporary session triggers the original 1Game transition path.
- `UI_地图选择` has a 16-widget static tree and a 432-node EventGraph; the route visuals are runtime-generated, not editor-placed.

## Decision

Use `L_Battle_1Game` as an isolated 1Game island.

The GameXXK flow owns the menu, town, quest, save slots, and the GameXXK route-map entry level. Once a GameXXK route node chooses Battle, the flow opens `/Game/GameXXK/Maps/L_Battle_1Game`. That level uses the original 1Game GameMode/PlayerController pair so the original 1Game UI can generate nodes and run its own click path without casting through GameXXK's C++ player controller.

This keeps GameXXK and 1Game responsibilities separate:

- GameXXK owns progression up to entering the 1Game island.
- 1Game owns its internal map UI, node generation, node click animation, and prototype room transition while inside the island.
- A follow-up bridge can add explicit entry and exit contracts between the island and `UGameXXKMVPSubsystem`, but that should be done at the boundary, not by reparenting original 1Game controller logic.

## Current Scope

For this MVP pass:

- Configure `L_Battle_1Game` to use `/Game/1Game/Control/BP_Gamemode.BP_Gamemode_C`.
- Keep `/Game/GameXXK/Maps/L_RouteMap` as the GameXXK-controlled route map that is reached from town.
- Keep the GameXXK route-node click that opens `L_Battle_1Game`.
- Update validation to expect the original 1Game player controller while in `L_Battle_1Game`.
- Verify that `L_Battle_1Game` shows generated 1Game nodes in PIE and that clicking a generated node triggers the original 1Game transition.

## Non-Goals

- Do not reparent the original 1Game controller.
- Do not modify the original 1Game UI EventGraph in this pass.
- Do not persist 1Game internal room state into GameXXK save slots yet.
- Do not replace the GameXXK-owned `L_RouteMap` adapter in this pass.

## Acceptance

- `L_Battle_1Game` world settings use the original 1Game GameMode.
- Opening `L_Battle_1Game` in PIE shows route nodes and connector lines, not only the parchment shell.
- A visible 1Game route node can be clicked and the original UI transition path runs.
- The main GameXXK flow still reaches `L_Battle_1Game` from `L_Main -> L_QingshanInn -> L_RouteMap -> Battle`.
- UE MCP save-dirty check reports no unsaved packages after automation.
