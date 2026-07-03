---
unit_id: 2026-07-03-playable-entry-flow
status: active
owner: codex
updated_at: 2026-07-03T00:00:00+08:00
source_commit: working-tree
depends_on: []
parallel_lock: GameXXK.PlayableEntryFlow
---

# Infra Audit

- UE MCP project tooling already exists: `Content/Python/gamexxk_mcp_tdd_toolset.py`.
- No-LiveCoding pipeline already exists: `scripts/ue_tdd_pipeline.py`.
- Real PIE harness already exists: `scripts/gamexxk_real_play_flow_mcp.py`.
- Gap: player-facing widgets were HUD-owned/test-owned, not reliably PlayerController-owned in real PIE.
- Gap: real flow harness still expected old Start -> WorldMap -> Qingshan behavior.
