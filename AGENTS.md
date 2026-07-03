# GameXXK — AI Agent Constraints

## Hard Constraints

### Canonical Workflow
- Work in the root project on `main`; do not create or use git worktrees for this project unless the user explicitly reverses this rule.
- Keep repository scans targeted. Prefer `rg`/specific file reads over whole-project enumeration because UE assets make the working tree large.
- Do not use UnrealBridge for this project. Use UE 5.8 MCP, UBT, command-line scripts, Visual Studio tooling, or focused editor Python through MCP.
- Do not revert or overwrite user-tuned assets, especially character sprites, PaperZD assets, placed levels, camera transforms, and manually adjusted HD2D plane values.

### UE MCP Automation
- Project UE automation should use `scripts/ue_mcp_client.py`, `scripts/ue_mcp_smoke.py`, `scripts/ue_tdd_pipeline.py`, and project scripts under `Content/Python`.
- If the editor is running, save dirty packages through UE MCP before closing or restarting it.
- If UE MCP is unavailable and the editor may have unsaved changes, do not force-close the editor unless the user explicitly accepts the risk.

### Compile Rule
- Do not use Live Coding or Hot Reload as verification.
- For C++ verification, save through MCP, close/restart the editor when needed, then run UBT or `scripts/ue_tdd_pipeline.py`.
- `--check-only` can inspect logs but is not a compile verification.

### Current MVP Acceptance
- `L_Main` PIE shows a player-facing main menu.
- `Start/New Game` is clickable and opens `/Game/GameXXK/Maps/L_QingshanInn`.
- Town UI is visible after the level load.
- `F` accepts the quest through the quest NPC interaction path.
- Quest NPC state persists through manual save: quest accepted/follower active and NPC location.
- Town button enters the Slay-the-Spire-style route map.
- Route-map button/node enters the battle screen.

## Navigation

| Resource | Purpose |
|---|---|
| `docs/design/agent-operating-guide.md` | Low-context handoff checklist |
| `docs/production/*` | Production-unit state files and acceptance records |
| `scripts/harness_state_validator.py` | Validate production-unit files |
| `scripts/ai_production_loop.py` | Run lightweight project-management and optional gameplay verification loop |
| `scripts/gamexxk_real_play_flow_mcp.py` | Real PIE/MCP playable-flow harness |
