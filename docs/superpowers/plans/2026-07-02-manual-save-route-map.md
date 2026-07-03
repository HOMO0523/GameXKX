# Manual Save Route Map Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make GameXXK persist only from an explicit `Save Game` command, split `New Game` from `Continue`, expose five manual save slots with delete, and show a clickable Slay the Spire-style route graph after the route quest enters `DungeonMap`.

**Architecture:** Keep `UGameXXKMVPSubsystem` as the state owner and `GameXXKMVPCommandRouter` as the command gate. Add a small route-map view model for HUD drawing and tests, while reusing existing dungeon commands for actual progression.

**Tech Stack:** Unreal Engine C++, Automation Tests, existing GameXXK MVP subsystem/rules/HUD/UMG shell.

---

### Task 1: Manual Save Command

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKMVPCommandRouter.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKMVPCommandRouter.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKPlayableRootWidget.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKMVPHUD.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKMVPPlayableShellTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKPlayableRootWidgetTest.cpp`

- [ ] **Step 1: Write failing tests**

Add assertions that `BuyHealingPowder`, `EnterDungeon`, and route completion do not create or update the configured save slot until `SaveGame` is executed. Add assertions that `SaveGame` is visible after `StartGame` and persists the current state.

- [ ] **Step 2: Run tests and confirm RED**

Run: `python scripts\gamexxk_mvp_playtest.py --skip-build --test-timeout 420 --report Saved\HarnessReports\gamexxk-mvp-playtest-manual-save-red.json`

Expected: failures mention missing `SaveGame` command or old autosave expectations.

- [x] **Step 3: Implement minimal command routing**

Add `SaveGame` to non-main-menu command lists. Change normal command execution to return command success without saving. Make only `SaveGame` call `SaveCurrentGame`. Split main-menu `New Game` and `Continue` command routing.

- [x] **Step 4: Run tests and confirm GREEN for command behavior**

Run: `python scripts\gamexxk_mvp_playtest.py --skip-build --test-timeout 420 --report Saved\HarnessReports\gamexxk-mvp-playtest-manual-save-green.json`

Expected: manual-save tests pass and old autosave tests are updated.

Verified on 2026-07-02 with targeted automation:
- `GameXXK.MVP.SaveGame.SlotRoundTrip`
- `GameXXK.MVP.UI.WidgetBasesDriveRules`
- `GameXXK.MVP.PIE.MainMenuContinueWorldMap`
- `GameXXK.MVP.PIE.PlayableRootCommandsDriveFullLoop`

### Task 1B: Five Save Slots And Full-State Restore

**Files:**
- Modified: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modified: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modified: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modified: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modified: `Source/GameXXK/Public/UI/GameXXKMainMenuWidget.h`
- Modified: `Source/GameXXK/Private/UI/GameXXKMainMenuWidget.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKPlayableRootWidgetTest.cpp`

- [x] **Step 1: Write failing tests**

Tests require `New Game` to ignore existing saves, `Continue` to load only populated slots, five stable manual slot names, delete support, and full runtime state persistence.

- [x] **Step 2: Run tests and confirm RED**

UBT failed on missing `CurrentMapId`, player location fields, manual slot APIs, continue API, and delete/existence APIs.

- [x] **Step 3: Implement minimal slot/full-state support**

`FGameXXKSaveState` now stores full `FGameXXKRuntimeState` for version 2 saves while retaining legacy fields for older save fallback. `UGameXXKMVPSubsystem` exposes five manual slot names, `ContinueGameFromSlot`, `DoesSaveGameExist`, `DeleteSaveGame`, and player-location recording. `GameXXKMVPCommandRouter` / `UGameXXKPlayableRootWidget` expose `ContinueSlot1..5`, `DeleteSlot1..5`, and `SaveSlot1..5`.

- [x] **Step 4: Run tests and confirm GREEN**

UBT succeeded. Targeted automation listed above succeeded.

### Task 1C: 1Game Route UI Read-In

**Files:**
- Added: `Source/GameXXKEditor/Public/GameXXKBlueprintInspectorLibrary.h`
- Added: `Source/GameXXKEditor/Private/GameXXKBlueprintInspectorLibrary.cpp`
- Modified: `Source/GameXXKEditor/GameXXKEditor.Build.cs`
- Added: `Content/Python/gamexxk_check_1game_blueprint_inspector.py`
- Modified: `scripts/gamexxk_analyze_1game_ui.py`
- Added: `docs/superpowers/specs/2026-07-02-1game-ui-framework-analysis.md`

- [x] **Step 1: Add editor-side Blueprint inspector**

Expose graph/node/pin, widget-tree, struct-field, and enum reading to UE Python/MCP.

- [x] **Step 2: Validate inspector through UE MCP**

`gamexxk_check_1game_blueprint_inspector.py` passed and wrote `Saved/AssetAnalysis/1game_blueprint_inspector_check.json`.

- [x] **Step 3: Analyze 1Game UI framework**

`scripts/gamexxk_analyze_1game_ui.py` wrote `Saved/AssetAnalysis/1game_ui_framework.json`; analysis summarized in the new spec doc.

- [x] **Step 4: Implement GameXXK route-map adapter**

Keep `UGameXXKMVPSubsystem` as source of truth, port 1Game route data shape into a GameXXK route adapter view model, then attach a UMG adapter that forwards node clicks into GameXXK command routing.

Implemented on 2026-07-03:
- Added `UGameXXKOneGameRouteMapWidget`.
- The adapter keeps a soft class reference to `/Game/1Game/UI/UI_地图选择.UI_地图选择_C`.
- `BuildAdapterNodes` maps GameXXK route nodes to 1Game-like room types.
- `ExecuteRouteNode` only executes enabled/current nodes through `GameXXKMVPCommandRouter`.
- `AGameXXKMVPHUD` now creates and refreshes the route adapter widget.
- Verified with `GameXXK.MVP.RouteMap.OneGameAdapter` and `GameXXK.MVP.PIE.PlayableRootCommandsDriveFullLoop`.

Visual safety pass on 2026-07-03:
- The adapter now also keeps soft references to `/Game/1Game/UI/UI_地图选择-关卡.UI_地图选择-关卡_C`, `/Game/1Game/UI/UI_地图选择-关卡-线.UI_地图选择-关卡-线_C`, and `/Game/1Game/UI/UI_地图选择-Boss.UI_地图选择-Boss_C`.
- Directly instantiating the 1Game node widget currently pulls in `UI_地图选择`, which fails Blueprint compile because `FunCL_GetRandomArrayToIndex` is missing from a dependency.
- To keep MVP flow stable, `bUseOneGameBlueprintVisualWidgets` defaults to false. The adapter creates safe GameXXK-owned fallback node/line visuals while preserving 1Game asset paths for the later visual swap.
- Verified with UBT and `GameXXK.MVP.RouteMap.OneGameAdapter`.

Fallback visual pass on 2026-07-03:
- The safe visual layer now uses `UBorder` route nodes with bound labels and `UBorder` connector lines instead of text-only placeholder widgets.
- Node colors distinguish visited, currently enabled, future, and boss nodes while the transparent button overlay continues to own clicks.
- Verified with UBT and `GameXXK.MVP.RouteMap.OneGameAdapter`.

Safe 1Game texture pass on 2026-07-03:
- The safe fallback nodes now embed `UImage` icons and select 1Game `Texture2D` assets by room type/state.
- Current/available battle nodes use `/Game/1Game/Texture/小怪.小怪`; unavailable battle nodes use `/Game/1Game/Texture/小怪灰色.小怪灰色`; start uses `/Game/1Game/Texture/脚印.脚印`.
- The same pattern is configured for elite/boss, camp, chest, merchant, and random event textures.
- The adapter still does not load 1Game Widget Blueprints by default, so the missing `FunCL_GetRandomArrayToIndex` dependency stays out of the playable flow.
- C++ compilation reached link on 2026-07-03, but final UBT link/automation were blocked by a running UnrealEditor process holding `UnrealEditor-GameXXK*.dll`.

Dynamic route graph pass on 2026-07-03:
- `FGameXXKRuntimeState` now owns generated route-map state: nodes, edges, current/pending node ids, visited node ids, reachable node ids, and route seed.
- `EnterDungeon` generates a deterministic 12-node, 6-layer route graph with branching paths and 1Game-style room kinds: start, battle, elite, event, camp, chest, merchant, and boss.
- `GameXXKMVPCommandRouter` exposes concrete `RouteNode<ID>` commands for enabled route nodes while retaining the old `SelectStart`, `SelectBattle`, `ResolveEventGold`, `ResolveCampHeal`, and `SelectBoss` commands for automation compatibility.
- `UGameXXKOneGameRouteMapWidget` now creates one node visual per generated node and one connector line per generated edge, using safe 1Game `Texture2D` icons instead of loading fragile 1Game Widget Blueprints.
- Manual saves persist the generated route graph and active route progression.
- Verified with UBT and targeted Automation: `GameXXK.MVP.RouteMap.OneGameAdapter`, `GameXXK.MVP.SaveGame.SlotRoundTrip`, `GameXXK.MVP.Battle.BoardWidget`, and `GameXXK.MVP.PIE.PlayableRootCommandsDriveFullLoop`.

PIE visual pass on 2026-07-03:
- `AGameXXKMVPHUD` now suppresses the old Canvas route-map fallback once the UMG route adapter exists, so the route screen no longer draws duplicate gray node boxes over the 1Game-style adapter.
- `UGameXXKOneGameRouteMapWidget::RefreshFromState` rebuilds its programmatic node/line tree when a widget was first created on `MainMenu` and later refreshed into `DungeonMap`.
- The route adapter viewport was nudged clear of the left command panel, and visual node click hit boxes were verified in PIE: Slot 5 continue -> Start node -> Battle node.
- Verified with UBT, `GameXXK.MVP.RouteMap.OneGameAdapter`, `GameXXK.MVP.Battle.BoardWidget`, and `GameXXK.MVP.PIE.PlayableRootCommandsDriveFullLoop`.

Next step: replace placeholder combat-board slots with real HD2D party/enemy visuals, then split primary enemy action into explicit skill/attack commands instead of immediate victory.

### Task 3B: Battle Board Adapter

**Files:**
- Added: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Added: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modified: `Source/GameXXK/Public/UI/GameXXKMVPHUD.h`
- Modified: `Source/GameXXK/Private/UI/GameXXKMVPHUD.cpp`
- Added: `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp`

- [x] **Step 1: Write failing test**

`GameXXK.MVP.Battle.BoardWidget` requires Battle screen to create a horizontal combat board: enemy slot on the left, hero/follower slots on the right, and a primary enemy action that resolves the existing battle-victory command.

- [x] **Step 2: Run tests and confirm RED**

UBT failed on missing `UI/GameXXKBattleBoardWidget.h`, confirming the test was covering new behavior.

- [x] **Step 3: Implement minimal battle board adapter**

Added `UGameXXKBattleBoardWidget`. It builds a safe UMG canvas with a left enemy slot and right party slots, shows only on `Screen == Battle`, and maps right-click/primary enemy action to `ResolveBattleVictory` through `GameXXKMVPCommandRouter`. `AGameXXKMVPHUD` now creates and refreshes it alongside the playable root and route map widgets.

- [x] **Step 4: Run tests and confirm GREEN**

Verified on 2026-07-03:
- UBT `GameXXKEditor Win64 Development`
- `GameXXK.MVP.Battle.BoardWidget`
- `GameXXK.MVP.RouteMap.OneGameAdapter`
- `GameXXK.MVP.PIE.PlayableRootCommandsDriveFullLoop`

PIE layout pass on 2026-07-03:
- BattleBoard slot positions now leave the left command panel clear.
- Monster stays in the left combat lane; Hero/Follower stay in a right party lane that does not cover the scene hero sprite.
- Right-click on the battle board was verified in PIE to execute the primary enemy action and return to the route map.

Next step: replace the placeholder battle slots with real HD2D character/enemy visuals, then split primary enemy action into explicit skill/attack commands instead of immediate victory.

### Task 2: Remove NPC Interaction Autosaves

**Files:**
- Modify: `Source/GameXXK/Private/Town/GameXXKTownNpcActor.cpp`
- Modify: `Source/GameXXK/Public/Town/GameXXKTownNpcActor.h`
- Modify: `Source/GameXXK/Private/Town/GameXXKTownNpcCharacter.cpp`
- Modify: `Source/GameXXK/Public/Town/GameXXKTownNpcCharacter.h`
- Test: `Source/GameXXK/Private/Tests/GameXXKTownShellTest.cpp`

- [ ] **Step 1: Write failing tests**

Change quest and merchant interaction assertions so `F` changes runtime state but loading the default slot fails before `SaveGame`. Add one positive save assertion by calling `SaveCurrentGame` after the interaction.

- [ ] **Step 2: Run tests and confirm RED**

Run: `python scripts\gamexxk_mvp_playtest.py --skip-build --test-timeout 420 --report Saved\HarnessReports\gamexxk-mvp-playtest-npc-save-red.json`

Expected: failures point at interaction autosave.

- [ ] **Step 3: Implement minimal NPC change**

Remove `SaveInteractionProgress` calls from quest, merchant, and follower movement logic. Keep `RecordQuestNpcLocation` runtime updates.

- [ ] **Step 4: Run tests and confirm GREEN**

Run: `python scripts\gamexxk_mvp_playtest.py --skip-build --test-timeout 420 --report Saved\HarnessReports\gamexxk-mvp-playtest-npc-save-green.json`

Expected: NPC save behavior matches explicit save only.

### Task 3: Slay The Spire-Style Route Graph

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKMVPHUD.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKMVPHUD.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKMVPCommandRouter.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKMVPCommandRouter.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKMVPPlayableShellTest.cpp`

- [x] **Step 1: Write failing tests**

Add a test that enters `DungeonMap`, asks the router/HUD for route map nodes, and expects five nodes: Start enabled, Battle/Event/Camp/Boss visible but disabled at node index 0.

- [x] **Step 2: Run tests and confirm RED**

Run: `python scripts\gamexxk_mvp_playtest.py --skip-build --test-timeout 420 --report Saved\HarnessReports\gamexxk-mvp-playtest-route-map-red.json`

Expected: failures mention missing route map node API.

- [x] **Step 3: Implement route node descriptors and HUD drawing**

Add a small route node descriptor struct. Build nodes from fixed dungeon nodes and current `DungeonNodeIndex`. Draw connected circles in `DrawHUD` when `Screen == DungeonMap`; enabled nodes add hit boxes using the same command names.

- [x] **Step 4: Run tests and confirm GREEN**

Run: `python scripts\gamexxk_mvp_playtest.py --skip-build --test-timeout 420 --report Saved\HarnessReports\gamexxk-mvp-playtest-route-map-green.json`

Expected: route graph tests pass.

Implemented outcome:
- The original five-node graph became a generated route model rather than only a HUD drawing helper.
- The current graph has 12 nodes and 15 directed edges across 6 layers.
- The route widget and command router consume the same generated node descriptors, so visible map clicks and command-list clicks share one route-selection path.
- Legacy commands stay available only as compatibility shortcuts for existing automation and the left-side MVP command list.

### Task 4: Real Flow Verification

**Files:**
- Modify: `Content/Python/gamexxk_probe_real_play_flow.py`
- Modify: `scripts/gamexxk_real_play_flow_mcp.py`

- [ ] **Step 1: Update automation expectations**

Make the probe click `Save Game` before checking persisted quest/follower/NPC state. Keep F as the only quest acceptance input.

- [ ] **Step 2: Build and run verification**

Run UBT, the MVP automation harness, and the real-flow MCP script:

```powershell
python scripts\gamexxk_mvp_playtest.py --skip-build --test-timeout 420 --report Saved\HarnessReports\gamexxk-mvp-playtest-manual-save-route-map.json
python scripts\gamexxk_real_play_flow_mcp.py --timeout 120 --report Saved\HarnessReports\gamexxk-real-play-flow-manual-save-route-map.json
```

Expected: all selected tests pass, real flow confirms Start, Qingshan, F quest acceptance, manual save, and DungeonMap route graph.
