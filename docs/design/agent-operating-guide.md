# GameXXK Agent Operating Guide

## Read by Task

- Start with applicable AGENTS.md constraints.
- For gameplay semantics or goal completion, consult docs/production/current-goal-acceptance.md and the active unit semantics.
- Read the matching plan when executing that plan; recent verification evidence is useful only for the affected area.
- Small text or localized edits do not require reading all production documents.

## Current Flow To Preserve

- The default editor/game entry is the pure-2D `/Game/GameXXK/Maps/L_DesktopTrainingHUD` surface.
- `Town` means the desktop挂机 workbench in the canonical product flow; routine PIE and visual verification stay on this map.
- An unlocked/replayable `挑战` starts the existing full-screen BattleBoard directly, without accepting or depending on the Qingshan quest and without changing maps.
- Leaving the training battle restores the same 2D workbench.
- `L_Main`, Qingshan town movement/NPC `F`/north-gate route flow and other 3D surfaces are preserved for explicit legacy or 3D-scoped checks only. Do not switch to them unless the user or current task asks for that scope.
- The BattleBoard remains the shared player-facing combat UI; do not recreate an embedded ChallengeViewport inside the workbench.

## Verification by Change

- Text/documentation: inspect the scoped diff and run git diff --check on changed files.
- C++: run UBT or scripts/ue_tdd_pipeline.py after the required MCP save and editor shutdown procedure.
- Runtime UI/rules: run focused automation or a reproduction of the changed behavior. Use scripts/gamexxk_real_play_flow_mcp.py when real PIE flow is affected and UE MCP is available.
- Pure art: deterministic asset checks and visual review; no automatic TDD or C++ build.
- Reuse valid evidence for unchanged relevant code and environment. Broaden or repeat checks only for changes, failures, or unresolved concerns.

## Safety Notes

- Treat existing `.uasset` changes as user/editor output unless you created them in the current task.
- Do not use UnrealBridge.
- Do not change camera or HD2D sprite tuning unless the user asks for visual adjustment.
- For targeting-arrow presentation, keep the DPI-aware coordinate chain intact. The visible source tip `(1082,608)` in the `1254×1254` arrow texture—not the texture center—is the mouse hotspot and rotation pivot.
