---
unit_id: 2026-07-03-playable-entry-flow
status: verified
owner: codex
updated_at: 2026-07-04T03:22:00+08:00
source_commit: working-tree
depends_on: []
parallel_lock: GameXXK.PlayableEntryFlow
---

# Review

Reviewed after focused C++ automation and UE MCP real-flow verification.

- The original player-location save bug is fixed at the save boundary, not hidden in UI code: the live pawn position is captured immediately before writing the save slot.
- Continue/town restore has a dedicated GameMode path and is covered by automation.
- The route map visibility regression exposed by real PIE was fixed by propagating state changes through the owning player controller; this also covers route and battle widget transitions.
- Route-map entry is now a real town interaction, not a visible town-overlay shortcut: the north gate `QingshanInn_TownExit` is guaranteed by GameMode, uses `F`, and is validated by the MCP real-flow harness.
- The player-facing TownOverlay no longer exposes `Enter Route Map`; low-level command-router tests remain available for deterministic rule coverage.
- Route and battle are now real UE map loads in the validated player path: `QingshanInn_TownExit` opens `L_RouteMap`, and the Battle route node opens the copied 1Game map `L_Battle_1Game`.
- The original `/Game/1Game` assets remain reference inputs. Runtime authority stays in GameXXK through `UGameXXKMVPSubsystem`, `GameXXKLevelFlow`, and `GameXXKFlowMapGameMode`.
- The copied 1Game battle bridge is now active for `L_Battle_1Game`. The previous Blueprint compile error was caused by a missing migrated TestMap function library; the new `TestMap` compatibility module restores only that required function.
- `L_Battle_1Game` now intentionally executes the copied 1Game GameMode/PlayerController path so the original scroll-map UI appears after selecting the battle route node. The copied player controller remains reparented to `AGameXXKMVPPlayerController`.
- The UE 5.8 `World Memory Leaks` crash is treated as an editor automation hazard from duplicating/loading map worlds in one MCP Python call. The asset setup is split by phase, and current-map checks now understand PIE `UEDPIE_N_` names.
- Residual risk: the project worktree still contains many existing UE asset changes from the broader MVP session. They were not reverted or split in this pass.

## 2026-07-06 GameXXK-Owned Route Map Review

- Route-map gameplay ownership is now on the GameXXK side: route seed, reachable/visited/pending node state, node clicks, battle handoff, and battle return are driven through `UGameXXKMVPRules`, `UGameXXKMVPSubsystem`, `GameXXKMVPCommandRouter`, and `UGameXXKOneGameRouteMapWidget`.
- 1Game is now a visual/source-layout input for the route-map slice, not the route state owner. The owned route widget uses 1Game background/icon/line assets while avoiding original 1Game PlayerController logic.
- `L_RouteMap` ownership is covered by UE MCP validation and currently resolves to `/Script/GameXXK.GameXXKFlowMapGameMode`.
- `L_Battle_1Game` direct entry is protected for both paths: live GameXXK `Battle` state opens the battle board directly, while non-Battle live subsystems no longer block the older isolated 1Game fallback.
- The old full-flow automation is now the main residual risk. It still encodes a fixed linear route sequence (`Start -> Battle -> Event -> Camp -> Boss`) and legacy save assumptions, so it should be rewritten around concrete `RouteNode<ID>` commands and the current manual-save contract before being used as a release gate.
