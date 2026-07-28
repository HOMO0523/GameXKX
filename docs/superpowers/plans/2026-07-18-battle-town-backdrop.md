# Battle Town Backdrop Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the battle setting a render-only copy of the live Qingshan town, retaining only terrain, vegetation, and cliffs/mountains while retaining the current battle presentation and flow.

**Architecture:** The live town source `/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo` is never edited. A normal interactive UE Editor session duplicates it to a new isolated battle map, removes everything except Landscape, foliage components using approved tree/plant/cliff meshes, and static-mesh tree/plant/cliff actors, then adds a minimal battle-only camera, presenter, PlayerStart, and light. The runtime Battle route moves to that verified map; the user-tuned legacy `/Game/GameXXK/Maps/L_BattleScene` remains untouched.

**Tech Stack:** UE 5.8 Editor Python through project MCP, Unreal map/asset APIs, Python `unittest`, existing UBT cold-build pipeline, PIE screenshot verification.

---

### Task 1: Lock the source, target, and retention contract

**Files:**
- Create: `scripts/test_battle_town_backdrop_pipeline.py`
- Create: `Content/Python/gamexxk_build_battle_town_backdrop.py`

- [ ] **Step 1: Write the failing contract test**

```python
def test_backdrop_uses_live_town_as_read_only_source(self):
    self.assertEqual(module.SOURCE_MAP, "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo")
    self.assertEqual(module.TARGET_BATTLE_MAP, "/Game/GameXXK/Maps/L_BattleTown")
    self.assertNotEqual(module.SOURCE_MAP, module.TARGET_BATTLE_MAP)
    self.assertEqual(module.KEEP_MESH_PREFIXES, (
        "/Game/Asian_Village/meshes/trees/",
        "/Game/Asian_Village/meshes/plants/",
        "/Game/Asian_Village/meshes/cliff/",
    ))
```

- [ ] **Step 2: Run the test and verify it fails because the module does not exist**

Run: `python -m unittest scripts.test_battle_town_backdrop_pipeline`

Expected: import failure for `gamexxk_build_battle_town_backdrop`.

- [ ] **Step 3: Implement the minimal module constants and read-only preflight helpers**

```python
SOURCE_MAP = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"
TARGET_BATTLE_MAP = "/Game/GameXXK/Maps/L_BattleTown"
KEEP_MESH_PREFIXES = (
    "/Game/Asian_Village/meshes/trees/",
    "/Game/Asian_Village/meshes/plants/",
    "/Game/Asian_Village/meshes/cliff/",
)
```

Preflight must reject PIE, dirty packages, a missing source map, an existing target map, and a source map outside `/Game/GameXXK/Maps/Prototype/`.

- [ ] **Step 4: Re-run the unit test**

Run: `python -m unittest scripts.test_battle_town_backdrop_pipeline`

Expected: PASS.

### Task 2: Duplicate and clean the isolated backdrop map

**Files:**
- Modify: `Content/Python/gamexxk_build_battle_town_backdrop.py`
- Modify: `scripts/test_battle_town_backdrop_pipeline.py`

- [ ] **Step 1: Add failing tests for the deletion policy**

```python
def test_mesh_policy_keeps_only_town_vegetation_and_cliffs(self):
    self.assertTrue(module.should_keep_mesh_path("/Game/Asian_Village/meshes/plants/SM_grass_01"))
    self.assertTrue(module.should_keep_mesh_path("/Game/Asian_Village/meshes/trees/SM_tree_01"))
    self.assertTrue(module.should_keep_mesh_path("/Game/Asian_Village/meshes/cliff/SM_cliff_01"))
    self.assertFalse(module.should_keep_mesh_path("/Game/Asian_Village/meshes/building/SM_house_01"))
    self.assertFalse(module.should_keep_mesh_path("/Game/Asian_Village/meshes/props/SM_cart_01"))

def test_actor_policy_only_keeps_landscape_or_approved_meshes(self):
    self.assertTrue(module.should_keep_actor_class("Landscape"))
    self.assertTrue(module.should_keep_actor_class("LandscapeStreamingProxy"))
    self.assertFalse(module.should_keep_actor_class("PlayerStart"))
    self.assertFalse(module.should_keep_actor_class("CameraActor"))
```

- [ ] **Step 2: Run the tests and verify the missing-policy failure**

Run: `python -m unittest scripts.test_battle_town_backdrop_pipeline`

Expected: FAIL because policy helpers do not yet exist.

- [ ] **Step 3: Implement a delete-only duplicate pass**

Use `EditorAssetLibrary.duplicate_asset(SOURCE_MAP, TARGET_BATTLE_MAP)` in the running full editor only. Load only the duplicate, enumerate its actors, keep Landscape/LandscapeStreamingProxy, and prune `InstancedFoliageActor` components whose `static_mesh` is not under `KEEP_MESH_PREFIXES`. Keep `StaticMeshActor` only when its mesh is in the same prefixes; delete all other map actors. Save after bounded batches (20–50 actor changes), then write a JSON manifest containing source/target package paths, source SHA-256 before/after, retained actor counts, removed actor counts, retained foliage component paths, and forbidden mesh prefixes found (must be empty).

- [ ] **Step 4: Re-run the policy tests**

Run: `python -m unittest scripts.test_battle_town_backdrop_pipeline`

Expected: PASS.

### Task 3: Add battle-only scaffolding and route to the verified new map

**Files:**
- Modify: `Content/Python/gamexxk_build_battle_town_backdrop.py`
- Modify: `scripts/test_battle_town_backdrop_pipeline.py`

- [ ] **Step 1: Add a failing test for battle-only scaffolding**

```python
def test_battle_scaffolding_has_no_floor_and_routes_to_the_new_map(self):
    self.assertEqual(module.TARGET_BATTLE_MAP, "/Game/GameXXK/Maps/L_BattleTown")
    self.assertEqual(module.PRESENTER_LABEL, "GameXXK_BattleScene_Presenter")
    self.assertEqual(module.CAMERA_LABEL, "GameXXK_BattleScene_Camera")
    self.assertEqual(module.PLAYER_START_LABEL, "GameXXK_Encounter_PlayerStart")
    self.assertNotIn("GameXXK_Encounter_Floor", module.scaffold_plan()["actors"])
```

- [ ] **Step 2: Run the test and verify it fails for the missing integration contract**

Run: `python -m unittest scripts.test_battle_town_backdrop_pipeline`

Expected: FAIL on missing constants/scaffolding function.

- [ ] **Step 3: Implement battle-only scaffolding without a floor**

On `TARGET_BATTLE_MAP`, set `GameXXKFlowMapGameMode`, spawn exactly one `GameXXK_BattleScene_Presenter`, `GameXXK_BattleScene_Camera`, `GameXXK_Encounter_PlayerStart`, and `GameXXK_Encounter_Light`, using the existing battle camera coordinates and 63° FOV. Do not create `GameXXK_Encounter_Floor`, and do not call `gamexxk_ensure_route_encounter_maps.py`. Update `GameXXKLevelFlow.cpp`, its C++ tests, the real-flow harness, and the route-map validator so Battle opens `TARGET_BATTLE_MAP` only after the map has passed verification.

- [ ] **Step 4: Re-run the integration unit test**

Run: `python -m unittest scripts.test_battle_town_backdrop_pipeline`

Expected: PASS.

### Task 4: Execute and verify in the interactive editor

**Files:**
- Modify: `Content/Python/gamexxk_build_battle_town_backdrop.py`
- Create: `Saved/HarnessReports/battle-town-backdrop-manifest.json` (generated)

- [ ] **Step 1: Preflight with no mutations**

Run through UE MCP: `Content/Python/gamexxk_build_battle_town_backdrop.py --preflight`.

Expected: PIE stopped, no dirty packages, source and legacy battle-map hashes recorded, target absent, source file hash readable.

- [ ] **Step 2: Run duplicate-and-clean only**

Run through UE MCP: `Content/Python/gamexxk_build_battle_town_backdrop.py --build-backdrop`.

Expected: duplicate map exists, source hash unchanged, manifest has Landscape/vegetation/cliff retains, and no building/props/water/sky mesh paths retained.

- [ ] **Step 3: Re-open the duplicate and verify manifest after reload**

Run through UE MCP: `Content/Python/gamexxk_build_battle_town_backdrop.py --verify-backdrop`.

Expected: target map is clean after reload; no town NPC, PlayerStart, CameraActor, building, prop, water, road, or gameplay actors remain.

- [ ] **Step 4: Add battle scaffolding and route to the verified map**

Run through UE MCP: `Content/Python/gamexxk_build_battle_town_backdrop.py --add-battle-scaffolding`.

Expected: exactly one battle presenter, camera, PlayerStart, and light exist, no `GameXXK_Encounter_Floor` exists, and the legacy `L_BattleScene` package hash is unchanged.

- [ ] **Step 5: Verify a real PIE battle screenshot and cold compile**

Run: `scripts/ue_tdd_pipeline.py --pie-duration 1 --log-lines 180`, then existing real-play flow to a battle screenshot.

Expected: the battle uses real Qingshan terrain, grass, trees and cliffs; buildings/NPCs/interactables are absent; cards, intentions, units, camera and battle input still load.

### Self-review

- [ ] The plan protects the live town source and the existing battle map before any mutation.
- [ ] The keep list exactly covers terrain, vegetation, and mountains/cliffs; all other town content is delete-only.
- [ ] The verified town-derived battle map is the runtime entrypoint and the legacy battle level remains unchanged.
- [ ] The final check includes both a cold compile and a live PIE scene, not only static tests.
