# 2026-07-02 Playable Root Widget Verification

## Scope

This slice adds a real UMG root widget for the early playable flow:

- `UGameXXKPlayableRootWidget` exposes Start/Continue and world map region buttons.
- `AGameXXKMVPHUD` creates the widget during `BeginPlay` and adds it to the viewport.
- The widget drives the MVP subsystem directly for Start/Continue, Qingshan entry, and locked Tanjiang gating.

## TDD Evidence

Red run:

```powershell
python scripts\gamexxk_mvp_playtest.py --build-timeout 900 --test-timeout 300 --report Saved\HarnessReports\playable-root-widget-red-mvp-playtest.json
```

Observed failure: build failed because `UI/GameXXKPlayableRootWidget.h` did not exist yet.

Green run:

```powershell
python scripts\gamexxk_mvp_playtest.py --build-timeout 900 --test-timeout 300 --report Saved\HarnessReports\playable-root-widget-green-mvp-playtest.json
```

Observed result: build exit `0`, automation exit `0`, and `GameXXK.MVP.PIE.MainMenuContinueWorldMap` completed with `Success`.

Review fix red run:

```powershell
python scripts\gamexxk_mvp_playtest.py --build-timeout 900 --test-timeout 300 --report Saved\HarnessReports\playable-root-widget-slot-red-mvp-playtest.json
```

Observed failure: build failed because `UGameXXKPlayableRootWidget::SetStartGameSlotForTest` did not exist yet. This protected the playable root automation from loading a real default autosave slot.

Review fix green run:

```powershell
python scripts\gamexxk_mvp_playtest.py --build-timeout 900 --test-timeout 300 --report Saved\HarnessReports\playable-root-widget-slot-green-mvp-playtest.json
```

Observed result: build exit `0`, automation exit `0`, and 7 `GameXXK.MVP` tests completed successfully after the playable root test switched to an isolated slot and `FGameXXKMVPCommandDescriptor` moved to a neutral UI header.

## Fresh Verification

```powershell
python -m py_compile scripts\gamexxk_mvp_playtest.py scripts\ue_tdd_pipeline.py scripts\ue_mcp_smoke.py
```

Result: exit `0`.

```powershell
git diff --check
```

Result: exit `0`; only CRLF normalization warnings were printed for modified text files.

```powershell
python scripts\gamexxk_mvp_playtest.py --build-timeout 900 --test-timeout 300 --report Saved\HarnessReports\playable-root-widget-final-mvp-playtest.json
```

Result: `ok: true`; build exit `0`; automation exit `0`; 7 `GameXXK.MVP` tests were found and completed successfully, including `GameXXK.MVP.PIE.MainMenuContinueWorldMap`.

```powershell
python scripts\ue_tdd_pipeline.py --pie-duration 1 --log-lines 200
```

Result: exit `0`; editor launched, UE MCP became ready, PIE started, PIE world time reached `2.92s`, and PIE stopped. The report contained one `[TDD]` marker and no failed marker lines.

```powershell
python scripts\ue_mcp_smoke.py --port 18765 --tool-timeout 30
```

Result: `ok: true`; required toolsets were present, including `gamexxk_mcp_tdd_toolset.GameXXKTDDToolset`.

```powershell
python scripts\gamexxk_character_visual_check.py --port 18765 --report Saved\HarnessReports\playable-root-widget-character-visual-check.json
```

Result: `ok: true`; validated the 8-direction expanded hero walk sheet, 48 sprites, 8 flipbooks, and editable `BP_HeroCharacter` visual configuration.

```powershell
python scripts\gamexxk_paperzd_character_check.py --port 18765 --report Saved\HarnessReports\playable-root-widget-paperzd-character-check.json
```

Result: `ok: true`; validated the PaperZD animation source, anim blueprint, directional walk sequence, and all 8 Paper2D flipbook references.

```powershell
python scripts\gamexxk_content_assembly_check.py --port 18765 --report Saved\HarnessReports\playable-root-widget-content-assembly-check.json
```

Result: `ok: true`; no missing directories, missing assets, invalid assets, or config errors.

```powershell
python scripts\gamexxk_town_scene_check.py --port 18765 --report Saved\HarnessReports\playable-root-widget-town-scene-check.json
```

Result: `ok: true`; `/Game/GameXXK/Maps/L_QingshanInn` loaded, 15 required labels were present, and no invalid actors or config errors were reported.

## Final Pre-Commit Verification

After review fixes, the following final checks were run:

```powershell
python -m py_compile scripts\gamexxk_mvp_playtest.py scripts\ue_tdd_pipeline.py scripts\ue_mcp_smoke.py
git diff --check
python scripts\gamexxk_mvp_playtest.py --build-timeout 900 --test-timeout 300 --report Saved\HarnessReports\playable-root-widget-final2-mvp-playtest.json
python scripts\ue_tdd_pipeline.py --pie-duration 1 --log-lines 200
python scripts\ue_mcp_smoke.py --port 18765 --tool-timeout 30
python scripts\gamexxk_character_visual_check.py --port 18765 --report Saved\HarnessReports\playable-root-widget-final2-character-visual-check.json
python scripts\gamexxk_paperzd_character_check.py --port 18765 --report Saved\HarnessReports\playable-root-widget-final2-paperzd-character-check.json
python scripts\gamexxk_content_assembly_check.py --port 18765 --report Saved\HarnessReports\playable-root-widget-final2-content-assembly-check.json
python scripts\gamexxk_town_scene_check.py --port 18765 --report Saved\HarnessReports\playable-root-widget-final2-town-scene-check.json
```

Results:

- Python compile: exit `0`.
- Diff whitespace check: exit `0`, with CRLF normalization warnings only.
- MVP playtest: `ok: true`; build exit `0`; automation exit `0`; 7 `GameXXK.MVP` tests completed successfully.
- UE TDD pipeline: exit `0`; editor launched, UE MCP became ready, PIE started, PIE world time reached `2.61s`, and PIE stopped.
- UE MCP smoke: `ok: true`; required MCP toolsets were present.
- Character visual, PaperZD, content assembly, and town scene checks: each returned `ok: true`.

## Remaining MVP Boundaries

This slice verifies the first real UMG entry surface. Dialog/trade/battle UI polish and visual click-through coverage remain broader MVP work, tracked by the active MVP goal.
