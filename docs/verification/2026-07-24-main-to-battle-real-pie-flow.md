# Main menu to battle — real PIE verification

Date: 2026-07-24

## Scope

This record verifies the existing player-facing entry flow only. It is not evidence that the broader route, shop, equipment, art, or three-chapter acceptance is complete.

## Command

```powershell
python scripts\gamexxk_real_play_flow_mcp.py --timeout 90 --report D:\GameXXKBuildTemp\real-play-flow-route-audit-final-geometry.json
```

## Result

The command exited with code `0` and its report has `ok: true`.

Observed in a real PIE session:

1. Main-menu start button opened the world map.
2. The Qingshan town marker opened the town level and visible town overlay.
3. Character movement, four directional/diagonal movement state transitions, quest interaction, manual-save path, and north-gate route entry completed.
4. A route start node and then an enabled battle node were clicked through visible Slate buttons.
5. The battle scene contained three enemies and three party units.
6. The battle HUD verifier passed using the fixed slot contract. UE 5.8's Python binding cannot expose Slate geometry methods, so it used the separately verified fixed canvas-slot/resource/status evidence rather than pretending pixel rectangles were available.

## Evidence

- Machine-readable report: `D:\GameXXKBuildTemp\real-play-flow-route-audit-final-geometry.json`
- Battle screenshot: `Saved\Codex\real_flow_after_battle.png`
- Harness regression tests: `python -m unittest scripts.test_gamexxk_real_play_flow_mcp -q` — 48 tests passed.

## Next acceptance work

Exercise real event, chest/relic, and merchant interactions from reachable route nodes, then verify their return-to-route transitions and persisted rewards. Run cold compilation only after C++ changes.
