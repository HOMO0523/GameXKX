---
unit_id: 2026-07-03-playable-entry-flow
status: active
owner: codex
updated_at: 2026-07-03T00:00:00+08:00
source_commit: working-tree
depends_on: []
parallel_lock: GameXXK.PlayableEntryFlow
---

# Plan

1. Add a formal Town Overlay widget for the player-facing town screen.
2. Move runtime player-flow widget creation to `AGameXXKMVPPlayerController`.
3. Keep HUD as debug/compatibility support, not the normal UI owner.
4. Wire town overlay buttons through `GameXXKMVPCommandRouter`.
5. Update the MCP real-flow harness to verify Start opens Qingshan directly and UI is visible from PlayerController-owned widgets.
6. Run build/tests and record results in `06-test-results.md`.
