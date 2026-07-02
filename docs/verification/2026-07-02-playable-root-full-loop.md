# 2026-07-02 Playable Root Full Loop Verification

## Scope

This slice expands `UGameXXKPlayableRootWidget` from the first menu/world-map entry into a real UMG command surface for the full MVP loop:

- Start/Continue
- world map region buttons and locked Tanjiang gating
- Qingshan town quest acceptance
- merchant buy/sell supply loop
- dungeon node commands
- battle failure back to town
- retry through event, camp, and Boss
- Tanjiang unlock
- autosave restore from the playable root slot

The slice also introduces `GameXXKMVPCommandRouter` so the Canvas HUD and the UMG playable root share one command list, status text, command execution path, and autosave behavior.

## TDD Evidence

Red run:

```powershell
python scripts\gamexxk_mvp_playtest.py --build-timeout 900 --test-timeout 300 --report Saved\HarnessReports\playable-root-full-loop-red-mvp-playtest.json
```

Observed result: build exit `0`, automation failed. The new `GameXXK.MVP.PIE.PlayableRootCommandsDriveFullLoop` test could not complete the root widget route because the UMG root did not expose/execute the town, dungeon, battle, Boss, Tanjiang, and autosave commands yet.

Green run:

```powershell
python scripts\gamexxk_mvp_playtest.py --build-timeout 900 --test-timeout 300 --report Saved\HarnessReports\playable-root-full-loop-green-mvp-playtest.json
```

Observed result: build exit `0`, automation exit `0`; the new `GameXXK.MVP.PIE.PlayableRootCommandsDriveFullLoop` test completed with `Success`.

Post-refactor run:

```powershell
python scripts\gamexxk_mvp_playtest.py --build-timeout 900 --test-timeout 300 --report Saved\HarnessReports\playable-root-full-loop-post-refactor-mvp-playtest.json
```

Observed result: build exit `0`, automation exit `0`; 8 `GameXXK.MVP` tests completed successfully after the root widget moved to dynamic command buttons and the HUD/root command logic was centralized.

Reviewer follow-up red run:

```powershell
python scripts\gamexxk_mvp_playtest.py --build-timeout 900 --test-timeout 300 --report Saved\HarnessReports\playable-root-full-loop-root-restore-mvp-playtest.json
```

Observed result: build exit `0`, automation failed. The first follow-up assertion over-specified restore behavior by expecting `Start/Continue` to reopen the previously selected Tanjiang town screen. Existing MVP save semantics persist progress, quest, unlocks, XP, level, and gold, then restore to the world map.

Reviewer follow-up green run:

```powershell
python scripts\gamexxk_mvp_playtest.py --build-timeout 900 --test-timeout 300 --report Saved\HarnessReports\playable-root-full-loop-root-restore-green-mvp-playtest.json
```

Observed result: build exit `0`, automation exit `0`. The reloaded `UGameXXKPlayableRootWidget` now drives save restore through its own `StartGame` command, verifies the restored world-map state keeps the completed quest and Tanjiang unlock, then clicks the restored Tanjiang command back into town.

Code-review follow-up red run:

```powershell
$env:PYTHONIOENCODING='utf-8'; python scripts\gamexxk_mvp_playtest.py --build-timeout 900 --test-timeout 300 --report Saved\HarnessReports\playable-root-hud-integration-red2-mvp-playtest.json
```

Observed result: build exit `0`, automation failed in `GameXXK.MVP.PIE.PlayableRootHUDIntegration`. The HUD-created UMG root did not inherit the HUD's test/save slot override, and commands executed through the HUD path did not refresh the root's dynamic button state.

Code-review follow-up green run:

```powershell
$env:PYTHONIOENCODING='utf-8'; python scripts\gamexxk_mvp_playtest.py --build-timeout 900 --test-timeout 300 --report Saved\HarnessReports\playable-root-hud-integration-green-mvp-playtest.json
```

Observed result: build exit `0`, automation exit `0`. `PlayableRootHUDIntegration` now initializes the real programmatic UMG button tree, clicks the root Start/Continue button through a HUD-owned slot, verifies restored Tanjiang unlock, executes a HUD command, and then clicks the refreshed root button into Qingshan town.

Autosave-failure follow-up red run:

```powershell
$env:PYTHONIOENCODING='utf-8'; python scripts\gamexxk_mvp_playtest.py --build-timeout 900 --test-timeout 300 --report Saved\HarnessReports\playable-root-hud-autosave-failure-red2-mvp-playtest.json
```

Observed result: build exit `0`, automation failed in `GameXXK.MVP.PIE.PlayableRootHUDIntegration`. A deliberately overlong save slot made the subsystem command mutate state while autosave returned `false`; the HUD-owned root still displayed its stale pre-command button state.

Autosave-failure follow-up green run:

```powershell
$env:PYTHONIOENCODING='utf-8'; python scripts\gamexxk_mvp_playtest.py --build-timeout 900 --test-timeout 300 --report Saved\HarnessReports\playable-root-hud-autosave-failure-green-mvp-playtest.json
```

Observed result: build exit `0`, automation exit `0`. HUD command dispatch now refreshes the root after command execution even when the combined command/autosave result is `false`, so the dynamic root button state matches the already-mutated subsystem state.

Final verification batch:

```powershell
$env:PYTHONIOENCODING='utf-8'; python scripts\gamexxk_mvp_playtest.py --build-timeout 900 --test-timeout 300 --report Saved\HarnessReports\playable-root-full-loop-final-mvp-playtest.json
python scripts\ue_tdd_pipeline.py --pie-duration 1 --log-lines 200
python scripts\ue_mcp_smoke.py --port 18765 --tool-timeout 30
python scripts\gamexxk_character_visual_check.py --port 18765 --report Saved\HarnessReports\playable-root-full-loop-character-visual-check.json
python scripts\gamexxk_paperzd_character_check.py --port 18765 --report Saved\HarnessReports\playable-root-full-loop-paperzd-character-check.json
python scripts\gamexxk_content_assembly_check.py --port 18765 --report Saved\HarnessReports\playable-root-full-loop-content-assembly-check.json
python scripts\gamexxk_town_scene_check.py --port 18765 --report Saved\HarnessReports\playable-root-full-loop-town-scene-check.json
git diff --check
python -m py_compile scripts\gamexxk_mvp_playtest.py scripts\ue_tdd_pipeline.py scripts\ue_mcp_smoke.py
```

Observed result: all commands exited `0`. The final playtest report includes successful `PlayableRootCommandsDriveFullLoop` and `PlayableRootHUDIntegration` automation tests; UE MCP smoke, character visual content, PaperZD setup, content assembly, and Qingshan town scene validators all reported `"ok": true`.

## Current Boundaries

The root widget is now a real UMG command surface for the MVP loop, but visual presentation remains intentionally functional. Later UI polish can replace the programmatic vertical command list with designed Widget Blueprints while preserving the shared command router.
