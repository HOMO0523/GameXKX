# 1Game Battle Bridge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Route-map Battle nodes load a GameXXK-owned 1Game battle-map copy instead of only switching the in-memory battle screen.

**Architecture:** GameXXK remains the runtime authority through `UGameXXKMVPSubsystem`; 1Game assets are copied under `/Game/GameXXK` and used as presentation/reference. Route and battle levels are independent UE maps, with GameXXK GameModes/PlayerControllers deciding state transitions.

**Tech Stack:** UE 5.8, C++ module `GameXXK`, UE MCP project Python scripts, UBT automation tests.

---

### Task 1: Level Flow Contract

**Files:**
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/MVP/GameXXKLevelFlow.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/MVP/GameXXKLevelFlow.cpp`
- Test: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKLevelFlowTest.cpp`

- [ ] **Step 1: Write failing tests**

Assert these exact map names:

```cpp
TestEqual(TEXT("town map"), GameXXKLevelFlow::MapForScreen(EGameXXKScreen::Town), FName(TEXT("/Game/GameXXK/Maps/L_QingshanInn")));
TestEqual(TEXT("route map"), GameXXKLevelFlow::MapForScreen(EGameXXKScreen::DungeonMap), FName(TEXT("/Game/GameXXK/Maps/L_RouteMap")));
TestEqual(TEXT("1Game battle copy map"), GameXXKLevelFlow::MapForScreen(EGameXXKScreen::Battle), FName(TEXT("/Game/GameXXK/Maps/L_Battle_1Game")));
```

- [ ] **Step 2: Run targeted automation**

Run:

```powershell
python scripts/ue_tdd_pipeline.py --skip-ubt --automation "GameXXK.MVP.LevelFlow"
```

Expected: FAIL because `GameXXKLevelFlow` does not exist.

- [ ] **Step 3: Implement minimal map resolver**

Add a namespace with `MapForScreen`, `MapForRuntimeState`, and `OpenMapForRuntimeState`. `OpenMapForRuntimeState` must no-op outside game worlds so editor-only tests stay stable.

- [ ] **Step 4: Re-run targeted automation**

Expected: PASS for `GameXXK.MVP.LevelFlow`.

### Task 2: Route Node Opens Runtime Maps

**Files:**
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/UI/GameXXKMVPCommandRouter.cpp`
- Test: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKOneGameRouteMapAdapterTest.cpp`

- [ ] **Step 1: Write failing assertion**

After executing the Battle route node, assert the runtime map resolver points at `/Game/GameXXK/Maps/L_Battle_1Game`.

- [ ] **Step 2: Run targeted automation**

Run:

```powershell
python scripts/ue_tdd_pipeline.py --skip-ubt --automation "GameXXK.MVP.RouteMap.OneGameAdapter"
```

Expected: FAIL until the route command calls the map-flow helper.

- [ ] **Step 3: Implement command-router map handoff**

After `SelectQingshan`, `EnterDungeon`, generated `RouteNode<ID>`, legacy `SelectBattle`, `ResolveBattleVictory`, and `FailBattle`, call `GameXXKLevelFlow::OpenMapForRuntimeState(Subsystem)`.

- [ ] **Step 4: Re-run targeted automation**

Expected: PASS for `GameXXK.MVP.RouteMap.OneGameAdapter`.

### Task 3: 1Game Asset Copies

**Files:**
- Create: `D:/UE5 demo/GameXXK/Content/Python/gamexxk_ensure_1game_battle_bridge_assets.py`
- Create asset: `/Game/GameXXK/Battle/BP_1GameBattlePlayerController`
- Create asset: `/Game/GameXXK/Maps/L_RouteMap`
- Create asset: `/Game/GameXXK/Maps/L_Battle_1Game`

- [ ] **Step 1: Write MCP asset script**

Use `EditorAssetLibrary.duplicate_asset` to copy:

```text
/Game/1Game/Control/BP_PlayerController -> /Game/GameXXK/Battle/BP_1GameBattlePlayerController
/Game/GameXXK/Maps/L_Main -> /Game/GameXXK/Maps/L_RouteMap
/Game/1Game/Map/NewWorld -> /Game/GameXXK/Maps/L_Battle_1Game
```

Compile/save the duplicated controller. Save dirty packages through MCP.

- [ ] **Step 2: Run script through UE MCP**

Run:

```powershell
python -c "from scripts.ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(); assert c.connect(); print(c.run_project_python_file('Content/Python/gamexxk_ensure_1game_battle_bridge_assets.py'))"
```

Expected: report shows all three destination assets exist and original `/Game/1Game/*` assets were not modified.

### Task 4: Real Flow Verification

**Files:**
- Modify: `D:/UE5 demo/GameXXK/scripts/gamexxk_real_play_flow_mcp.py`

- [ ] **Step 1: Update harness expectations**

After town gate `F`, expect `L_RouteMap`; after Battle node, expect `L_Battle_1Game`.

- [ ] **Step 2: Run full verification**

Run:

```powershell
python scripts/ue_tdd_pipeline.py --automation "GameXXK.MVP.LevelFlow;GameXXK.MVP.RouteMap.OneGameAdapter;GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets"
python scripts/gamexxk_real_play_flow_mcp.py
```

Expected: tests pass and the real flow reaches the copied 1Game battle map.
