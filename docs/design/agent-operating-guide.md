# GameXXK Agent Operating Guide

## Read Order

1. `AGENTS.md`
2. Active `docs/production/*/01-semantics.md`
3. Matching `docs/production/*/03-plan.md`
4. Recent `docs/verification/*` only when the current task touches that area

## Current Flow To Preserve

- Main menu is the entry point on `L_Main`.
- New Game starts a fresh runtime state and opens the Qingshan town map.
- Town uses real character movement, HD2D sprites, NPC interaction, and a player-facing town overlay.
- Quest acceptance belongs to the NPC `F` interaction path.
- Route-map entry belongs to the in-world north gate `QingshanInn_TownExit`; after quest acceptance, walk to the gate approach area and press `F`.
- The route map and battle board are player-facing UI screens driven by the shared MVP command router.

## Verification Order

1. `git diff --check`
2. UBT build or `scripts/ue_tdd_pipeline.py`
3. Focused automation tests for UI/rules changes
4. `scripts/gamexxk_real_play_flow_mcp.py` for true PIE flow when UE MCP is available

## Safety Notes

- Treat existing `.uasset` changes as user/editor output unless you created them in the current task.
- Do not use UnrealBridge.
- Do not change camera or HD2D sprite tuning unless the user asks for visual adjustment.
