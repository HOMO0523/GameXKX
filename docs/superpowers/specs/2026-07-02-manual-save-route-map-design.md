# Manual Save And Route Map Design

## Goal

GameXXK MVP should keep quest, follower, town, and route progress in memory while the player plays, but it should only write that progress to disk when the player clicks an explicit save command. After the route quest is accepted with `F`, entering the route should show a Slay the Spire-style node map instead of feeling like a hidden state change.

## Reference Pattern

Ocean keeps save writes behind explicit UI intent. Its pause menu lets the player pick a snapshot slot, then the save manager captures current runtime state. GameXXK should follow the same rule for this MVP: commands and interactions mutate runtime state; the save button is the only command that persists the current runtime state to the configured slot.

## Save Behavior

- `New Game` always creates a fresh runtime state and opens the world map. It must not read an existing save slot.
- `Continue` is a separate UI path. It opens a save-slot panel with five manual save slots.
- Clicking a populated continue slot loads exactly that saved runtime state. Clicking an empty continue slot fails/no-ops.
- Each slot has a delete affordance. Delete removes only that slot and does not mutate the current runtime state.
- Town, shop, battle, route, and `F` interactions do not autosave.
- Quest NPC `F` interaction accepts the route quest, joins the follower, and records the NPC location in runtime state only.
- Quest NPC follower movement updates the runtime NPC location only.
- `Save Game` is shown after play begins and writes the current full runtime state only when the player chooses a save slot.
- If the player accepts the quest but does not click `Save Game`, restarting should not restore that accepted quest.
- If the player accepts the quest, moves the follower, clicks `Save Game`, then restarts, the quest/follower/NPC location state should restore.
- Saved state includes current screen, current region/map id, route node index, dungeon-active flag, player HP/stats/equipment/inventory, quest/follower state, NPC location, and player location.

## Route Map Behavior

- `Enter Route Map` remains gated by the accepted route quest.
- After the accepted quest enters the route, the runtime screen becomes `DungeonMap`.
- On `DungeonMap`, the HUD/UMG adapter draws a vertical Slay the Spire-style route graph from generated runtime route data.
- The current MVP graph is deterministic for testing: 12 nodes, 15 directed edges, 6 layers, and room kinds matching 1Game's map vocabulary.
- Only reachable nodes are enabled and clickable. Future nodes remain visible but disabled until unlocked by completing an upstream node.
- Clicking an enabled visual node executes its concrete `RouteNode<ID>` command through the command router.
- Legacy commands (`SelectStart`, `SelectBattle`, `ResolveEventGold`, `ResolveCampHeal`, `SelectBoss`) remain available as compatibility shortcuts for the left command list and existing automation.
- The existing left command list remains as a fallback and automation surface.
- Manual saves include the route graph, visited nodes, reachable nodes, current node, and pending battle node so continuing a slot returns to the same route progression.

## Testing

- Add tests that prove route and shop commands do not save until `Save Game`.
- Add tests that prove `Save Game` persists the current route state.
- Add tests that prove `New Game` ignores saved slots while `Continue` restores only the manually saved state.
- Add tests that prove five save slots have stable names and delete removes the chosen slot.
- Add tests that prove `DungeonMap` exposes a generated route graph model with branching edges, enabled reachable nodes, disabled future nodes, and persisted route state.
- Update real-flow automation to click `Save Game` before expecting persisted quest/follower state.

## Non-Goals

- Do not add a new Unreal map asset for the route in this pass.
- Do not directly run the original 1Game map widget/controller Blueprint logic in this pass.
