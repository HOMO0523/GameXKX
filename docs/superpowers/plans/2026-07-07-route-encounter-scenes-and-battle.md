# Route Encounter Scenes And Battle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make route-map nodes enter independent GameXXK scenes, resolve their own gameplay, return to the route map, and support a first playable battle loop with monster configuration and HP damage.

**Architecture:** `UGameXXKMVPSubsystem` and `UGameXXKMVPRules` remain the only gameplay state owners. `L_RouteMap` only selects reachable route nodes; clicked nodes become pending encounters and travel to a node-specific map. Encounter maps resolve pending nodes and return to `L_RouteMap`, while battle uses a minimal data-driven encounter state with up to three enemies on the left and up to three party units on the right.

**Tech Stack:** UE 5.8 C++, Automation tests, GameXXK MVP subsystem/rules, GameXXK UMG widgets, UE MCP/UBT verification. No UnrealBridge and no git worktrees.

---

## Scope

This plan builds the first vertical slice, not the final RPG battle system.

In scope:
- Route node clicks close/hide the map by changing screen and opening a dedicated map for Battle, Boss, Camp, Event, Chest, and Merchant.
- Start remains an in-map route-map node because it is the route entry point, not an encounter room.
- Pending node state is used for all room types, not just combat.
- Non-combat rooms complete their pending route node only after the room action is confirmed.
- Battle initializes a party snapshot and enemy units from route node kind and seed.
- Right-click/basic attack damages enemies; killing all enemies completes the pending route node and returns to the route map.
- Save/continue keeps route progress through existing `RuntimeState` persistence, but in-room manual save is disabled or omitted for this slice.

Out of scope for this first slice:
- Final visual polish for battle sprites.
- Full BP/weakness/break implementation.
- Full shop inventory UI inside the route merchant scene.
- Final event narrative UI.

## File Structure

- Modify `D:/UE5 demo/GameXXK/Source/GameXXK/Public/GameXXKMVPRules.h`
  - Add route encounter screen values.
  - Add battle skill/monster/party runtime structs.
  - Add rule functions for pending room completion and basic battle attack.
- Modify `D:/UE5 demo/GameXXK/Source/GameXXK/Private/GameXXKMVPRules.cpp`
  - Change route node selection semantics.
  - Generate basic encounter state.
  - Resolve pending non-combat rooms.
  - Resolve battle victory from combat state.
- Modify `D:/UE5 demo/GameXXK/Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
  - Expose wrappers for pending room completion and basic attack.
- Modify `D:/UE5 demo/GameXXK/Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
  - Implement wrappers.
- Modify `D:/UE5 demo/GameXXK/Source/GameXXK/Private/MVP/GameXXKLevelFlow.cpp`
  - Map route encounter screens to independent levels.
- Modify `D:/UE5 demo/GameXXK/Source/GameXXK/Private/UI/GameXXKMVPCommandRouter.cpp`
  - Build commands for route encounter screens.
  - Travel after completing pending rooms.
- Modify `D:/UE5 demo/GameXXK/Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
  - Replace immediate victory on right-click with basic attack.
  - Keep existing debug victory command as a fallback command, not the primary board action.
- Create `D:/UE5 demo/GameXXK/Source/GameXXK/Public/MVP/GameXXKRouteEncounterSceneActor.h`
  - Own the in-world `F` interaction path for Event/Camp/Merchant maps.
- Create `D:/UE5 demo/GameXXK/Source/GameXXK/Public/MVP/GameXXKBattleScenePresenter.h`
  - Own the GameXXK battle scene layout and left-enemy/right-party placement.
- Create `D:/UE5 demo/GameXXK/Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h`
  - Own right-click/basic-attack interaction for in-world battle units.
- Modify tests:
  - `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKLevelFlowTest.cpp`
  - `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKRouteMapSeedRulesTest.cpp`
  - Create `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKBattleEncounterRulesTest.cpp`
  - Create `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKRouteEncounterSceneActorTest.cpp`
  - Create `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`
- Create UE maps through MCP/editor Python after rules compile:
  - `/Game/GameXXK/Maps/L_RouteEvent`
  - `/Game/GameXXK/Maps/L_RouteCamp`
  - `/Game/GameXXK/Maps/L_RouteMerchant`
  - `/Game/GameXXK/Maps/L_BattleScene`

## Task 1: Route Encounter Screens

**Files:**
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/MVP/GameXXKLevelFlow.cpp`
- Test: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKLevelFlowTest.cpp`
- Test: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKRouteMapSeedRulesTest.cpp`

- [x] **Step 1: Write failing route screen tests**

Add assertions that `RouteEvent`, `RouteCamp`, and `RouteMerchant` map to independent levels, and that selecting Event/Camp/Merchant/Chest route nodes sets `PendingRouteNodeId` without visiting the node immediately.

- [x] **Step 2: Run test and verify RED**

Run:

```powershell
python scripts\gamexxk_mvp_playtest.py
```

Expected: compile or automation failure because the new screens and pending non-combat semantics do not exist.

- [x] **Step 3: Implement route encounter screens**

Add new `EGameXXKScreen` values:

```cpp
RouteEvent,
RouteCamp,
RouteMerchant,
Battle
```

Update `SelectRouteNodeById`:
- Start completes immediately on `DungeonMap`.
- Battle/Elite/Boss set pending and `Screen = Battle`.
- Event/Chest set pending and `Screen = RouteEvent`.
- Camp sets pending and `Screen = RouteCamp`.
- Merchant sets pending and `Screen = RouteMerchant`.

- [x] **Step 4: Run tests and verify GREEN**

Run:

```powershell
python scripts\gamexxk_mvp_playtest.py
```

Expected: route and level-flow tests pass or only later-task battle assertions fail.

## Task 2: Pending Room Completion

**Files:**
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/UI/GameXXKMVPCommandRouter.cpp`
- Test: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKRouteMapSeedRulesTest.cpp`

- [x] **Step 1: Write failing completion tests**

Test:
- Resolving a pending event rewards gold or item, clears pending, marks node visited, returns to `DungeonMap`.
- Resolving a pending camp heals or grants item, clears pending, marks node visited, returns to `DungeonMap`.
- Resolving a pending merchant completes the node and returns to `DungeonMap`.

- [x] **Step 2: Verify RED**

Run focused automation through the MVP playtest suite.

- [x] **Step 3: Implement completion wrappers**

Add:

```cpp
static bool ResolvePendingRouteEvent(FGameXXKRuntimeState& State, bool bTakeGold);
static bool ResolvePendingRouteCamp(FGameXXKRuntimeState& State, bool bHealNow);
static bool ResolvePendingRouteMerchant(FGameXXKRuntimeState& State);
```

Expose equivalent subsystem methods and commands:
- `ResolveEventGold`
- `ResolveCampHeal`
- `CompleteMerchantNode`

- [x] **Step 4: Verify GREEN**

Run:

```powershell
python scripts\gamexxk_mvp_playtest.py
```

## Task 3: Minimal Battle Encounter State

**Files:**
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Test: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKBattleEncounterRulesTest.cpp`

- [x] **Step 1: Write failing battle initialization tests**

Test:
- Selecting a Battle node creates active battle state.
- Normal battle creates 1-3 enemies.
- Elite creates stronger enemies than Battle.
- Boss creates one Boss enemy.
- Party has Hero and Follower when follower joined.

- [x] **Step 2: Verify RED**

Run MVP playtest. Expected failure: active battle state fields and initialization do not exist.

- [x] **Step 3: Implement minimal battle runtime structs**

Add:

```cpp
USTRUCT(BlueprintType)
struct FGameXXKBattleRuntimeUnit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName Id;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText DisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 HP = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MaxHP = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Attack = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Defense = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Speed = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Shield = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bEnemy = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bDefeated = false;
};
```

Store `bHasActiveBattle`, `ActiveBattleNodeId`, `ActiveBattleEnemies`, and `ActiveBattleParty` on `FGameXXKRuntimeState`.

- [x] **Step 4: Verify GREEN**

Run MVP playtest.

## Task 4: Basic Attack And Victory Return

**Files:**
- Modify: `D:/UE5 demo/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Test: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKBattleEncounterRulesTest.cpp`

- [x] **Step 1: Write failing attack tests**

Test:
- Hero basic attack reduces enemy HP by `max(1, Attack - Defense)`.
- Defeated enemy is marked defeated.
- Defeating all enemies completes pending route node and returns to `DungeonMap`.

- [x] **Step 2: Verify RED**

Run MVP playtest.

- [x] **Step 3: Implement attack**

Add:

```cpp
static bool ExecuteBattleBasicAttack(FGameXXKRuntimeState& State, int32 PartyIndex, int32 EnemyIndex);
```

The function should:
- Require `State.Screen == Battle`.
- Require active battle state.
- Ignore defeated enemies.
- Apply deterministic damage.
- Resolve victory when all enemies are defeated.

- [x] **Step 4: Wire battle board right-click**

Change `UGameXXKBattleBoardWidget::ExecutePrimaryEnemyAction()` to call the subsystem basic attack against the first undefeated enemy.

- [x] **Step 5: Verify GREEN**

Run MVP playtest and then real flow if maps are present.

## Task 5: Encounter Maps

**Files:**
- Create: `/Game/GameXXK/Maps/L_RouteEvent`
- Create: `/Game/GameXXK/Maps/L_RouteCamp`
- Create: `/Game/GameXXK/Maps/L_RouteMerchant`
- Modify or create: `D:/UE5 demo/GameXXK/Content/Python/gamexxk_ensure_route_encounter_maps.py`
- Test/Validate: `D:/UE5 demo/GameXXK/Content/Python/gamexxk_validate_route_encounter_maps.py`

- [x] **Step 1: Write validation script**

Validate that all three map packages exist and use `GameXXKFlowMapGameMode`.

- [x] **Step 2: Verify RED**

Run the validation script through UE MCP and confirm missing maps fail validation.

- [x] **Step 3: Create minimal maps**

Use UE MCP/editor Python to create blank 45-degree-friendly maps with the same flow GameMode. Do not touch user-tuned town or character assets.

- [x] **Step 4: Verify GREEN**

Run validation and route click flow.

## Task 6: GameXXK-Owned Encounter And Battle Scene Actors

**Files:**
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/MVP/GameXXKRouteEncounterSceneActor.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/MVP/GameXXKRouteEncounterSceneActor.cpp`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/MVP/GameXXKBattleScenePresenter.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/MVP/GameXXKBattleScenePresenter.cpp`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp`
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `D:/UE5 demo/GameXXK/Content/Python/gamexxk_ensure_route_encounter_maps.py`
- Modify: `D:/UE5 demo/GameXXK/Content/Python/gamexxk_validate_route_encounter_maps.py`
- Test: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKRouteEncounterSceneActorTest.cpp`
- Test: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`

- [x] **Step 1: Hide old player-facing encounter overlay**

`RouteEvent`, `RouteCamp`, and `RouteMerchant` are now scene maps. The old `MainMenuWidget` encounter buttons remain test/debug-callable but are not visible or focused in the player path.

- [x] **Step 2: Add route encounter scene actor**

`AGameXXKRouteEncounterSceneActor` resolves pending Event/Camp/Merchant nodes from `F` interaction and returns to `L_RouteMap` through `GameXXKLevelFlow`.

- [x] **Step 3: Add battle scene actor slice**

`AGameXXKBattleScenePresenter` converts active battle state into left enemy and right party placements. `AGameXXKBattleSceneUnitActor` applies the existing GameXXK basic attack rule on right click and returns to the route map after victory.

- [x] **Step 4: Compile, run map ensure, and validate**

Completed with UE MCP enabled. The route encounter maps and the owned battle scene map are created/validated through project Python, and the real flow reaches `L_BattleScene`.

```powershell
python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 30
python -c "import sys; sys.path.insert(0, 'scripts'); from ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(timeout=180); c.require_connected(); print(c.run_project_python_file('Content/Python/gamexxk_ensure_route_encounter_maps.py'))"
python -c "import sys; sys.path.insert(0, 'scripts'); from ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(timeout=180); c.require_connected(); print(c.run_project_python_file('Content/Python/gamexxk_validate_route_encounter_maps.py'))"
python scripts\gamexxk_mvp_playtest.py
python scripts\gamexxk_real_play_flow_mcp.py --timeout 180
```

## Verification

Required commands:

```powershell
python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 30
python scripts\gamexxk_mvp_playtest.py
python scripts\gamexxk_real_play_flow_mcp.py --timeout 180
```

Expected final acceptance:
- `L_RouteMap` remains route-only.
- Battle/Boss/Camp/Event/Chest/Merchant nodes open separate maps.
- Completing the room returns to `L_RouteMap`.
- The route map remembers visited/current/reachable state.
- Battle scene has left enemy slots and right party slots.
- Basic attack reduces enemy HP.
- Defeating all enemies completes the pending node.
