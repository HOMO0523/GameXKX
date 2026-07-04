# 1Game Island Battle Map Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `/Game/GameXXK/Maps/L_Battle_1Game` run as an isolated original 1Game map island after the GameXXK route-map Battle node.

**Architecture:** Keep the GameXXK menu, town, save, quest, and `L_RouteMap` flow under GameXXK C++ control. Configure only `L_Battle_1Game` to use the original `/Game/1Game/Control/BP_Gamemode.BP_Gamemode_C`, which supplies the original 1Game player controller expected by `UI_地图选择`. Update validation and the real-flow harness to treat that original player controller as the accepted runtime state inside the island.

**Tech Stack:** UE 5.8, UE MCP project Python scripts, GameXXK C++/UMG runtime, Windows PIE screenshot harness.

---

### Task 1: Validation Contract

**Files:**
- Modify: `D:/UE5 demo/GameXXK/Content/Python/gamexxk_validate_1game_battle_ui_bridge.py`

- [ ] **Step 1: Write the failing validation expectation**

Replace copied-bridge constants with original 1Game island constants:

```python
EXPECTED_GAME_MODE_CLASS = "/Game/1Game/Control/BP_Gamemode.BP_Gamemode_C"
EXPECTED_PLAYER_CONTROLLER_CLASS = "/Game/1Game/Control/BP_PlayerController.BP_PlayerController_C"
```

Keep the same report shape, but rename keys from `expected_game_mode` and `expected_player_controller` to those constants and change the failure message to:

```python
raise RuntimeError("L_Battle_1Game is not configured as an isolated original 1Game island")
```

- [ ] **Step 2: Run validation before the asset fix**

Run:

```powershell
python -c "from scripts.ue_mcp_client import UnrealMCPClient; import json; c=UnrealMCPClient(timeout=60); c.require_connected(); print(json.dumps(c.run_project_python_file('Content/Python/gamexxk_validate_1game_battle_ui_bridge.py'), ensure_ascii=False))"
```

Expected: FAIL while `L_Battle_1Game` still points at the copied bridge or GameXXK flow GameMode.

### Task 2: Island Map Configuration

**Files:**
- Modify: `D:/UE5 demo/GameXXK/Content/Python/gamexxk_ensure_1game_battle_bridge_assets.py`

- [ ] **Step 1: Add an explicit island phase**

Add:

```python
ONEGAME_GAME_MODE_CLASS = "/Game/1Game/Control/BP_Gamemode.BP_Gamemode_C"
```

Add a new phase branch:

```python
elif phase == "configure-battle-island":
    _configure_map(report, BATTLE_MAP, _load_blueprint_class(ONEGAME_GAME_MODE))
```

This uses the existing `ONEGAME_GAME_MODE = "/Game/1Game/Control/BP_Gamemode"` asset path and keeps the copied bridge phases available for historical comparison.

- [ ] **Step 2: Run the island configuration through UE MCP**

Run:

```powershell
python -c "from scripts.ue_mcp_client import UnrealMCPClient; import json; c=UnrealMCPClient(timeout=120); c.require_connected(); print(json.dumps(c.run_project_python_file('Content/Python/gamexxk_ensure_1game_battle_bridge_assets.py', ['--phase', 'configure-battle-island']), ensure_ascii=False))"
```

Expected: `ok: true`, `maps[0].game_mode` is `/Game/1Game/Control/BP_Gamemode.BP_Gamemode_C`, and dirty packages are saved.

- [ ] **Step 3: Re-run validation**

Run the command from Task 1 Step 2.

Expected: PASS and report `game_mode` / `player_controller` match the original 1Game classes.

### Task 3: Real Flow Harness

**Files:**
- Modify: `D:/UE5 demo/GameXXK/scripts/gamexxk_real_play_flow_mcp.py`

- [ ] **Step 1: Update the battle player-controller token**

Change:

```python
ONEGAME_BATTLE_PC_TOKEN = "BP_1GameBattlePlayerController_C"
```

to:

```python
ONEGAME_BATTLE_PC_TOKEN = "BP_PlayerController_C"
```

Update related event/error text from "copied 1Game battle map/bridge" to "original 1Game island".

- [ ] **Step 2: Run Python syntax verification**

Run:

```powershell
python -m py_compile Content/Python/gamexxk_validate_1game_battle_ui_bridge.py Content/Python/gamexxk_ensure_1game_battle_bridge_assets.py scripts/gamexxk_real_play_flow_mcp.py
```

Expected: no output and exit code 0.

### Task 4: Runtime Acceptance

**Files:**
- Runtime evidence: `D:/UE5 demo/GameXXK/Saved/Codex/*.png`
- Runtime reports: `D:/UE5 demo/GameXXK/Saved/HarnessReports/*.json`

- [ ] **Step 1: Verify original 1Game nodes in isolated battle map**

Run a focused MCP/PIE screenshot probe:

```powershell
python -c "import sys, json, time; sys.path.insert(0, 'scripts'); from pathlib import Path; from ue_mcp_client import EDITOR_TOOLSET, UnrealMCPClient; from gamexxk_real_play_flow_mcp import SCENE_TOOLSET, PreviewInput; c=UnrealMCPClient(timeout=60); c.require_connected(); c.stop_pie() if c.is_in_pie() else None; c.call_tool('load_level', {'level_path':'/Game/GameXXK/Maps/L_Battle_1Game'}, toolset_name=SCENE_TOOLSET, timeout=60); c.call_tool('StartPIE', {'options': {'bSimulate': False, 'playMode': 'PlayMode_InEditorFloating', 'warmupSeconds': 1.5}}, toolset_name=EDITOR_TOOLSET, timeout=60); time.sleep(1); w=PreviewInput().find_preview_window(); data,size=PreviewInput().capture_window_png(w); p=Path('Saved/Codex/1game_island_after_config.png'); p.parent.mkdir(parents=True, exist_ok=True); p.write_bytes(data); print(json.dumps({'ok': True, 'screenshot': str(p), 'size': size}, ensure_ascii=False))"
```

Expected: screenshot shows generated 1Game nodes and connector lines.

- [ ] **Step 2: Run the full real play flow**

Run:

```powershell
python scripts/gamexxk_real_play_flow_mcp.py
```

Expected: `ok: true`; final map contains `L_Battle_1Game`; active player controller class name contains `BP_PlayerController_C`.

- [ ] **Step 3: Save and inspect Git state**

Run:

```powershell
python -c "from scripts.ue_mcp_client import UnrealMCPClient; import json; c=UnrealMCPClient(timeout=60); c.require_connected(); print(json.dumps(c.save_dirty_packages(), ensure_ascii=False))"
git status --short
```

Expected: no dirty packages after MCP save. Git status should only show intentional source/script/map asset changes for this task.
