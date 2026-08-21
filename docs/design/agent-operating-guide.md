# GameXXK Agent Operating Guide

## Read Order

1. `AGENTS.md`
2. Active `docs/production/*/01-semantics.md`
3. Matching `docs/production/*/03-plan.md`
4. Recent `docs/verification/*` only when the current task touches that area

## Current Flow To Preserve

- The default editor/game entry is the pure-2D `/Game/GameXXK/Maps/L_DesktopTrainingHUD` surface.
- `Town` means the desktop挂机 workbench in the canonical product flow; routine PIE and visual verification stay on this map.
- An unlocked/replayable `挑战` starts the existing full-screen BattleBoard directly, without accepting or depending on the Qingshan quest and without changing maps.
- Leaving the training battle restores the same 2D workbench.
- `L_Main`, Qingshan town movement/NPC `F`/north-gate route flow and other 3D surfaces are preserved for explicit legacy or 3D-scoped checks only. Do not switch to them unless the user or current task asks for that scope.
- The BattleBoard remains the shared player-facing combat UI; do not recreate an embedded ChallengeViewport inside the workbench.

## Verification Order

1. `git diff --check`
2. UBT build or `scripts/ue_tdd_pipeline.py`
3. Focused automation tests for UI/rules changes
4. `scripts/gamexxk_real_play_flow_mcp.py` for true PIE flow when UE MCP is available

## Safety Notes

- Treat existing `.uasset` changes as user/editor output unless you created them in the current task.
- Do not use UnrealBridge.
- Do not change camera or HD2D sprite tuning unless the user asks for visual adjustment.
- For targeting-arrow presentation, keep the DPI-aware coordinate chain intact. The visible source tip `(1082,608)` in the `1254×1254` arrow texture—not the texture center—is the mouse hotspot and rotation pivot.
