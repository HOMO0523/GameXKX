# Qingshan PCG Whitebox B0R Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and verify an isolated UE 5.8 B0R whitebox map with the north gate on the map's right side, a curved entry road, asymmetric building clusters, a diagonal river, a connected main bridge, a lower south dock, an irregular mountain ring, and four reproducible review cameras.

**Architecture:** Add a new immutable JSON layout contract and host-safe validator, then call the existing editor-only PCG automation through a narrowly extended B0R allowlist. An idempotent UE Python assembler duplicates the protected town map into `/Game/GameXXK/Maps/Dev`, creates spline/fixed-anchor proxies, and drives separate PCG graphs for scaled building, foliage, and mountain instances. A UE validator and host coordinator prove deterministic regeneration, protected-map isolation, route connectivity, budgets, and camera transforms before any stylized image generation.

**Tech Stack:** Unreal Engine 5.8, UE PCG, QuickRoad 5.8 capability probe, GameXXKEditor C++, Unreal Python, UE 5.8 MCP via `scripts/ue_mcp_client.py`, Python `unittest`, UBT without Live Coding or Hot Reload.

---

## Scope and file map

This plan covers only whitebox production and evidence. Stylizing the four camera captures is a later plan after user approval.

- Create `Config/GameXXK/TownPCG/QingshanWhiteboxB0R.json`: canonical B0R bounds, stable IDs, splines, anchors, plots, instance budgets, and camera definitions.
- Create `Content/Python/gamexxk_qingshan_whitebox_config.py`: host-safe load/validation API.
- Create `scripts/test_qingshan_whitebox_config.py`: contract and failure-case tests.
- Modify `Source/GameXXKEditor/Private/GameXXKTownPCGAutomationLibrary.cpp`: admit only the explicit B0R graph/map roots in addition to the existing vertical-slice roots.
- Modify `scripts/test_qingshan_town_pcg_scripts.py`: regression tests for the existing root restrictions.
- Create `scripts/test_qingshan_whitebox_scripts.py`: source contracts for the B0R assembler, validator, and acceptance probe.
- Create `Content/Python/gamexxk_assemble_qingshan_whitebox_b0r.py`: idempotent Dev-map assembly and PCG setup/finalize phases.
- Create `scripts/run_qingshan_whitebox_b0r.py`: bounded MCP coordinator.
- Create `scripts/test_run_qingshan_whitebox_b0r.py`: coordinator success/error/timeout tests.
- Create `Content/Python/gamexxk_validate_qingshan_whitebox_b0r.py`: read-only UE structural validator.
- Create `Content/Python/gamexxk_qingshan_whitebox_acceptance.py`: snapshot and viewport-camera actions.
- Create `scripts/qingshan_whitebox_acceptance.py`: canonical evidence hashing and comparison.
- Create `scripts/test_qingshan_whitebox_acceptance.py`: deterministic evidence tests.
- Create via UE `/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R`.
- Create via UE `/Game/GameXXK/Environment/TownPCG/B0R/PCG_QS_B0R_{Buildings,Foliage,Mountains}`.
- Create `docs/production/qingshan-pcg-whitebox-b0r.md` and evidence under `docs/production/evidence/qingshan-pcg-whitebox-b0r/`.

Read-only inputs:

- `/Game/GameXXK/Maps/L_QingshanInn`
- `Config/DefaultEditor.ini`
- Existing Batch 0 reference images and JSON
- Existing player, PaperZD, camera, north-gate F, quest NPC, and HD2D assets

### Task 1: Define the B0R layout contract

**Files:**
- Create: `scripts/test_qingshan_whitebox_config.py`
- Create: `Content/Python/gamexxk_qingshan_whitebox_config.py`
- Create: `Config/GameXXK/TownPCG/QingshanWhiteboxB0R.json`

- [ ] **Step 1: Write the failing canonical-contract test**

Create `scripts/test_qingshan_whitebox_config.py` with imports following `scripts/test_qingshan_town_pcg_config.py`, then assert these exact invariants:

```python
self.assertEqual(data["schema_version"], 1)
self.assertEqual(data["revision_id"], "B0R")
self.assertEqual(data["seed"], 20260711)
self.assertEqual(data["source_map"], "/Game/GameXXK/Maps/L_QingshanInn")
self.assertEqual(data["whitebox_map"], "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R")
self.assertEqual(data["asset_root"], "/Game/GameXXK/Environment/TownPCG/B0R")
self.assertEqual(data["coordinate_reference"]["anchor_id"], "NorthGateFAnchor")
self.assertEqual(data["coordinate_reference"]["actor_label"], "QingshanInn_TownExit")
self.assertEqual(data["playable_bounds_cm"], [-30000, 0, -12000, 12000])
self.assertEqual(data["world_bounds_cm"], [-38000, 7000, -19000, 19000])
self.assertEqual(len(data["building_plots"]), 16)
self.assertEqual(sum(p["size_class"] == "large" for p in data["building_plots"]), 1)
self.assertEqual(sum(p["size_class"] == "medium" for p in data["building_plots"]), 5)
self.assertEqual(sum(p["size_class"] == "small" for p in data["building_plots"]), 10)
self.assertEqual(set(data["cameras"]), {
    "CAM_QS_GATE_ARRIVAL", "CAM_QS_TOWN_CORE",
    "CAM_QS_MAIN_BRIDGE", "CAM_QS_SOUTH_DOCK",
})
self.assertFalse(data["runtime_generation"])
```

Add rejection tests for duplicate stable IDs; non-finite vectors; missing north-gate anchor; a building count outside `13..19`; a camera FOV outside `35..65`; missing `location_cm`/`target_cm`; `runtime_generation=true`; a road, river, plot, or camera outside world bounds; and a bridge center whose horizontal distance is farther than `100cm` from both main-road and river control points.

- [ ] **Step 2: Run the test and verify RED**

Run:

```powershell
python -m unittest scripts.test_qingshan_whitebox_config -v
```

Expected: FAIL because the loader and B0R JSON do not exist.

- [ ] **Step 3: Implement the host-safe loader and validator**

`gamexxk_qingshan_whitebox_config.py` must export exactly:

```python
__all__ = ("CONFIG_PATH", "load_config", "validate_config")
CONFIG_PATH = PROJECT_ROOT / "Config" / "GameXXK" / "TownPCG" / "QingshanWhiteboxB0R.json"
```

Implement `_require_id`, `_require_vector`, `_require_bounds`, `_require_package_path`, `_require_integer`, `validate_config`, `load_config`, and `_reject_json_numeric_constant`. `_require_package_path` accepts trimmed long package names beginning `/Game/` with no whitespace or object suffix; vector validation rejects booleans, NaN, and infinity; `load_config` uses `json.load(..., parse_constant=_reject_json_numeric_constant)`. Do not import `unreal`.

- [ ] **Step 4: Create the canonical B0R JSON**

Use north-gate-local UE centimeters. The exact structure and primary control data are:

```json
{
  "schema_version": 1,
  "revision_id": "B0R",
  "seed": 20260711,
  "source_map": "/Game/GameXXK/Maps/L_QingshanInn",
  "whitebox_map": "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R",
  "asset_root": "/Game/GameXXK/Environment/TownPCG/B0R",
  "coordinate_reference": {"anchor_id": "NorthGateFAnchor", "actor_label": "QingshanInn_TownExit"},
  "playable_bounds_cm": [-30000, 0, -12000, 12000],
  "world_bounds_cm": [-38000, 7000, -19000, 19000],
  "runtime_generation": false,
  "spawn_caps": {"buildings": 19, "foliage": 100, "mountains": 30, "crossings": 2},
  "fixed_anchors": [
    {"id": "NorthGateFAnchor", "location_cm": [0, 0, 120], "yaw_degrees": 0, "protected_radius_cm": 800},
    {"id": "TownCoreAnchor", "location_cm": [-12000, -2200, 300], "yaw_degrees": 25, "protected_radius_cm": 1200},
    {"id": "MainBridgeAnchor", "location_cm": [-15500, -6500, 100], "yaw_degrees": -35, "protected_radius_cm": 1200},
    {"id": "SouthDockAnchor", "location_cm": [-21500, -10000, -200], "yaw_degrees": -20, "protected_radius_cm": 1000}
  ],
  "road_splines": [
    {"id": "Road_Main", "width_cm": 800, "points_cm": [[0,0,120],[-3500,-500,140],[-7500,-2500,180],[-11500,-3500,250],[-15500,-6500,100],[-20500,-9000,-100]]},
    {"id": "Road_Core_North", "width_cm": 450, "points_cm": [[-7500,-2500,180],[-9000,1200,250],[-13000,2600,400]]},
    {"id": "Road_Core_South", "width_cm": 400, "points_cm": [[-11500,-3500,250],[-14500,-1500,300],[-18500,-3000,250]]}
  ],
  "river_splines": [
    {"id": "River_Main", "width_cm": 1800, "points_cm": [[-26000,-12000,-250],[-21000,-9000,-200],[-15500,-6500,-150],[-9000,-8000,-100],[-3500,-11000,-100]]}
  ],
  "terrain_zones": [
    {"id": "Terrain_Base", "center_cm": [-15000,0,-500], "size_cm": [30000,24000,400], "yaw_degrees": 0},
    {"id": "Terrain_CorePlateau", "center_cm": [-12000,-1800,-150], "size_cm": [12500,8500,500], "yaw_degrees": 10},
    {"id": "Terrain_SouthSlope", "center_cm": [-17800,-6800,-350], "size_cm": [10500,6500,350], "yaw_degrees": -25},
    {"id": "Terrain_DockLowland", "center_cm": [-21800,-9800,-550], "size_cm": [8500,4800,250], "yaw_degrees": -20}
  ],
  "proxy_generation": {
    "foliage": {"count": 100, "scale_range": [2.5,6.5], "exclusion_margin_cm": 250},
    "mountains": {"count": 24, "scale_xy_range": [25,65], "scale_z_range": [35,95], "perimeter_band_cm": [1500,7000]}
  },
  "cameras": {
    "CAM_QS_GATE_ARRIVAL": {"location_cm": [-2500,2500,3200], "target_cm": [-6500,-1200,300], "fov_degrees": 50},
    "CAM_QS_TOWN_CORE": {"location_cm": [-5000,3500,4500], "target_cm": [-12500,-3000,300], "fov_degrees": 50},
    "CAM_QS_MAIN_BRIDGE": {"location_cm": [-9000,-1000,4000], "target_cm": [-15500,-6500,0], "fov_degrees": 48},
    "CAM_QS_SOUTH_DOCK": {"location_cm": [-15000,-4000,4200], "target_cm": [-21500,-9500,-200], "fov_degrees": 50}
  }
}
```

Add exactly 16 `building_plots` with IDs `BLD_L_01`, `BLD_M_01..05`, and `BLD_S_01..10`. Each record contains `location_cm`, `yaw_degrees`, `size_cm`, `entrance_axis`, and `cluster_id`. Use these exact centers and yaw values:

```python
(
    ("BLD_L_01", "large",  [-12000,-1800,300],  25),
    ("BLD_M_01", "medium", [ -8200,-4700,220], -15),
    ("BLD_M_02", "medium", [-10000,  800,380],  18),
    ("BLD_M_03", "medium", [-14800,-1900,360],  42),
    ("BLD_M_04", "medium", [-17400,-10800, 20], -28),
    ("BLD_M_05", "medium", [-19200,-4800,180],  12),
    ("BLD_S_01", "small",  [ -5200,-2500,180], -22),
    ("BLD_S_02", "small",  [ -6500, 1700,300],  31),
    ("BLD_S_03", "small",  [ -7200,-6200, 80],  11),
    ("BLD_S_04", "small",  [-12200, 3300,460], -35),
    ("BLD_S_05", "small",  [-14700,-3500,180],  55),
    ("BLD_S_06", "small",  [-16200,  900,420], -12),
    ("BLD_S_07", "small",  [-18500,-1500,300],  27),
    ("BLD_S_08", "small",  [-21400,-5200,-20], -46),
    ("BLD_S_09", "small",  [-24800,-7600,-80],  16),
    ("BLD_S_10", "small",  [-23800,-5800,100],  39),
)
```

Assign sizes in ID order: `BLD_L_01=[1800,1400,1200]`; medium sizes are `[1200,900,800]`, `[1100,850,750]`, `[1400,1000,900]`, `[1000,800,700]`, `[1300,900,850]`; small sizes are `[800,600,500]`, `[700,550,450]`, `[900,700,600]`, `[750,600,500]`, `[850,650,550]`, `[650,550,450]`, `[900,650,550]`, `[750,550,500]`, `[850,700,550]`, `[700,600,480]`. Set `entrance_axis` to `+Y` and use yaw for world-facing direction. Assign cluster IDs `approach` to `S01,S02`; `core` to `L01,M01,M02,S03,S04,S05`; `bridge` to `M03,S06,S07`; and `south` to `M04,M05,S08,S09,S10`. Define foliage exclusion zones as the `NorthGateFAnchor`, `MainBridgeAnchor`, and `SouthDockAnchor` circles, road corridors at half-width plus `250cm`, the river corridor at half-width plus `400cm`, and every building footprint plus `200cm`. `TownCoreAnchor` identifies the intended large-building group and is therefore not a building exclusion zone. The deterministic generator expands `proxy_generation` into exactly 100 foliage transforms and 24 mountain transforms; these generated transforms are evidence, not additional authored config.

- [ ] **Step 5: Run GREEN tests and commit**

Run:

```powershell
python -m unittest scripts.test_qingshan_whitebox_config -v
```

Expected: all B0R config tests pass.

Commit:

```powershell
git add -- Config/GameXXK/TownPCG/QingshanWhiteboxB0R.json Content/Python/gamexxk_qingshan_whitebox_config.py scripts/test_qingshan_whitebox_config.py
git commit -m "test: define qingshan B0R whitebox contract"
```

### Task 2: Extend editor automation root safety for B0R

**Files:**
- Modify: `Source/GameXXKEditor/Private/GameXXKTownPCGAutomationLibrary.cpp`
- Modify: `scripts/test_qingshan_town_pcg_scripts.py`
- Create: `scripts/test_qingshan_whitebox_scripts.py`

- [ ] **Step 1: Write failing allowlist source tests**

Require the C++ implementation to contain both old and new roots:

```python
self.assertIn('/Game/GameXXK/Environment/TownPCG/VerticalSlice/', source)
self.assertIn('/Game/GameXXK/Environment/TownPCG/B0R/', source)
self.assertIn('/Game/GameXXK/Maps/Prototype/', source)
self.assertIn('/Game/GameXXK/Maps/Dev/', source)
self.assertIn('IsManagedGraphPath', source)
self.assertIn('IsManagedPrototypeMapPath', source)
```

Also assert no root is widened to `/Game/`, `/Game/GameXXK/`, or `/Game/GameXXK/Maps/` as an unrestricted prefix.

- [ ] **Step 2: Run the tests and verify RED**

Run:

```powershell
python -m unittest scripts.test_qingshan_town_pcg_scripts scripts.test_qingshan_whitebox_scripts -v
```

Expected: FAIL because the B0R roots and helpers are absent.

- [ ] **Step 3: Replace single-prefix checks with narrow helpers**

Add the following constants and helpers inside `GameXXKTownPCGAutomation`:

```cpp
constexpr TCHAR VerticalSliceGraphRoot[] = TEXT("/Game/GameXXK/Environment/TownPCG/VerticalSlice/");
constexpr TCHAR B0RGraphRoot[] = TEXT("/Game/GameXXK/Environment/TownPCG/B0R/");
constexpr TCHAR PrototypeMapRoot[] = TEXT("/Game/GameXXK/Maps/Prototype/");
constexpr TCHAR B0RMapRoot[] = TEXT("/Game/GameXXK/Maps/Dev/");

bool IsManagedGraphPath(const FString& Path)
{
    return Path.StartsWith(VerticalSliceGraphRoot, ESearchCase::CaseSensitive)
        || Path.StartsWith(B0RGraphRoot, ESearchCase::CaseSensitive);
}

bool IsManagedPrototypeMapPath(const FString& Path)
{
    return Path.StartsWith(PrototypeMapRoot, ESearchCase::CaseSensitive)
        || Path.StartsWith(B0RMapRoot, ESearchCase::CaseSensitive);
}
```

Use these helpers wherever the existing library validates graph or prototype map paths. Keep all transaction, uniqueness, read-only-package, finite-transform, and error-JSON checks unchanged.

- [ ] **Step 4: Run source tests GREEN**

Run:

```powershell
python -m unittest scripts.test_qingshan_town_pcg_scripts scripts.test_qingshan_whitebox_scripts -v
```

Expected: all source-contract tests pass.

- [ ] **Step 5: Save the editor and compile without Live Coding**

If UE is running, use `scripts/ue_mcp_client.py` to stop PIE when active and save dirty packages. If MCP cannot save dirty packages, do not close UE. Once clean, close the editor and run:

```powershell
python scripts/ue_tdd_pipeline.py --project "D:\UE5 demo\GameXXK\GameXXK.uproject" --target GameXXKEditor --platform Win64 --configuration Development
```

Expected: UBT exits `0`; no Live Coding or Hot Reload is used.

- [ ] **Step 6: Restart UE, run MCP smoke, and commit**

Run:

```powershell
python scripts/ue_mcp_smoke.py
```

Expected: `ok=true`, including `gamexxk_mcp_tdd_toolset.GameXXKTDDToolset`.

Commit only the root-safety change and its tests:

```powershell
git add -- Source/GameXXKEditor/Private/GameXXKTownPCGAutomationLibrary.cpp scripts/test_qingshan_town_pcg_scripts.py scripts/test_qingshan_whitebox_scripts.py
git commit -m "feat: allow isolated B0R town pcg assets"
```

### Task 3: Implement deterministic layout expansion

**Files:**
- Create: `scripts/qingshan_whitebox_acceptance.py`
- Create: `scripts/test_qingshan_whitebox_acceptance.py`

- [ ] **Step 1: Write failing deterministic-layout tests**

Require public helpers:

```python
from scripts.qingshan_whitebox_acceptance import (
    canonical_layout_hash,
    generate_seeded_proxy_transforms,
)
```

Test that seed `20260711` produces exactly 100 foliage transforms and 24 mountain transforms; reversed input order does not change `canonical_layout_hash`; all foliage avoids roads, river, building footprints, and fixed-anchor radii; all mountains lie outside playable bounds but inside world bounds; and two independent calls return byte-identical canonical JSON.

- [ ] **Step 2: Run RED**

```powershell
python -m unittest scripts.test_qingshan_whitebox_acceptance -v
```

Expected: FAIL because the module does not exist.

- [ ] **Step 3: Implement the pure deterministic helpers**

Use `random.Random(seed)` only; never use global `random`. Implement:

```python
def generate_seeded_proxy_transforms(config: Mapping[str, Any]) -> dict[str, list[dict[str, Any]]]: ...
def canonical_layout_hash(payload: Mapping[str, Any], decimals: int = 3) -> dict[str, Any]: ...
```

Foliage sampling is rejection-based within playable bounds with a maximum of `20_000` attempts. Mountains sample a four-sided perimeter band outside playable bounds. Each transform contains stable `id`, `location_cm`, `rotation_degrees`, and `scale`. If a budget cannot be filled inside the attempt cap, raise `RuntimeError` with the requested and accepted counts.

- [ ] **Step 4: Run GREEN and commit**

```powershell
python -m unittest scripts.test_qingshan_whitebox_acceptance -v
git add -- scripts/qingshan_whitebox_acceptance.py scripts/test_qingshan_whitebox_acceptance.py
git commit -m "test: define deterministic B0R proxy layout"
```

Expected: all deterministic-layout tests pass before commit.

### Task 4: Build the idempotent UE whitebox assembler

**Files:**
- Create: `Content/Python/gamexxk_assemble_qingshan_whitebox_b0r.py`
- Modify: `scripts/test_qingshan_whitebox_scripts.py`
- Create via UE: `/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R`
- Create via UE: `/Game/GameXXK/Environment/TownPCG/B0R/PCG_QS_B0R_Buildings`
- Create via UE: `/Game/GameXXK/Environment/TownPCG/B0R/PCG_QS_B0R_Foliage`
- Create via UE: `/Game/GameXXK/Environment/TownPCG/B0R/PCG_QS_B0R_Mountains`

- [ ] **Step 1: Write failing assembly source contracts**

Require these constants and phases:

```python
SOURCE_MAP = "/Game/GameXXK/Maps/L_QingshanInn"
WHITEBOX_MAP = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R"
MANAGED_TAG = "QingshanB0RManaged"
PHASE_SETUP = "setup"
PHASE_FINALIZE = "finalize"
```

Require labels `QS_B0R_Road_Main`, `QS_B0R_River_Main`, `QS_B0R_Bridge_Main`, `QS_B0R_Dock_South`, `QS_B0R_Terrain_Base`, `QS_B0R_Terrain_CorePlateau`, `QS_B0R_Terrain_SouthSlope`, `QS_B0R_Terrain_DockLowland`, `QS_B0R_PCG_Buildings`, `QS_B0R_PCG_Foliage`, and `QS_B0R_PCG_Mountains`. Require all four `CAM_QS_*` IDs. Assert the source never calls `save_asset(SOURCE_MAP)`, never deletes actors without `QingshanB0RManaged`, and imports the B0R config loader.

- [ ] **Step 2: Run RED**

```powershell
python -m unittest scripts.test_qingshan_whitebox_scripts -v
```

Expected: FAIL because the assembler does not exist.

- [ ] **Step 3: Implement protected map duplication and snapshots**

Implement `_dirty_map_package_names`, `_ensure_whitebox_map`, `_snapshot_preserved_actors`, `_north_local_to_world`, and `_parse_operation_payload`. `_ensure_whitebox_map` duplicates `SOURCE_MAP` only when `WHITEBOX_MAP` is absent, refuses the operation when any map/content package is dirty, then loads only `WHITEBOX_MAP`. Before and after assembly, snapshot `QingshanInn_TownExit`, `PlayerStart_QingshanInn`, and all labels containing `quest`, `npc`, `follower`, or `interaction`. Abort if the source map is dirty or any preserved transform changes.

- [ ] **Step 4: Create splines and visible fixed proxies**

Create one closed world-bounds spline, the three road splines, and the river spline from JSON. Tag road splines with `Quick_Road_LayoutInput`, but set config evidence to one of `quickroad_verified` or `proxy_spline`; do not claim QuickRoad verification based on tags alone. Create low-cost visible cube proxies for bridge and dock, plus sampled flat road/river plates so the curves are visible in screenshots. Create the four terrain-zone cubes from JSON with their top faces at the configured elevations, yielding a base, raised core plateau, descending south slope, and lower dock area. Every created actor must have `QingshanB0RManaged` and a stable label.

- [ ] **Step 5: Author three PCG graphs**

Use `/Engine/BasicShapes/Cube` for building and mountain proxies and `/Engine/BasicShapes/Cone` for foliage proxies. Convert size centimeters to cube scale by dividing by `100`; offset cube Z by half its height. Call `create_or_update_town_pcg_graph` separately for the 16 buildings, 100 foliage transforms, and 24 mountain transforms. Attach each graph to one uniquely labelled PCG volume, clear, save the whitebox map, then schedule generation.

- [ ] **Step 6: Create four saved CameraActors**

For every config camera, convert north-gate-local location and target to world coordinates, compute a look-at `unreal.Rotator`, spawn or update one `unreal.CameraActor` with the camera ID as its actor label, set `field_of_view` to the configured value, and add `QingshanB0RManaged`. Store the exact world location, rotation, target, and FOV in setup JSON.

- [ ] **Step 7: Implement finalize and idempotency checks**

`finalize` must wait until all three PCG actors report `generated=true`, require instance counts `16`, `100`, and `24`, require exactly four saved camera actors and four terrain-zone actors, require no duplicate managed labels, compare preserved snapshots, save only the whitebox map, and return compact JSON containing `success`, `complete`, `pending`, graph paths, counts, camera transforms, dirty maps, and the canonical layout SHA-256.

- [ ] **Step 8: Run source tests GREEN and commit scripts**

```powershell
python -m unittest scripts.test_qingshan_whitebox_config scripts.test_qingshan_whitebox_acceptance scripts.test_qingshan_whitebox_scripts -v
git add -- Content/Python/gamexxk_assemble_qingshan_whitebox_b0r.py scripts/test_qingshan_whitebox_scripts.py
git commit -m "feat: assemble qingshan B0R whitebox"
```

### Task 5: Add the bounded host coordinator

**Files:**
- Create: `scripts/run_qingshan_whitebox_b0r.py`
- Create: `scripts/test_run_qingshan_whitebox_b0r.py`

- [ ] **Step 1: Write failing coordinator tests**

Define `FakeClient.run_project_python_file()` to record phase arguments and return JSON stdout, and `FakeClock` with controlled `monotonic()`/`sleep()`. Cover setup failure, pending-to-complete success, terminal error, deadline timeout, invalid JSON stdout, and rejection of non-positive timeout/poll values. Require the coordinator to call only:

```python
ASSEMBLY_SCRIPT = "Content/Python/gamexxk_assemble_qingshan_whitebox_b0r.py"
("--phase", "setup")
("--phase", "finalize")
```

- [ ] **Step 2: Run RED**

```powershell
python -m unittest scripts.test_run_qingshan_whitebox_b0r -v
```

Expected: FAIL because the coordinator does not exist.

- [ ] **Step 3: Implement the coordinator**

Implement `_decode_phase_response`, `_call_phase`, `run_whitebox_b0r`, `_parse_args`, and `main`. The public coordinator signature is:

```python
def run_whitebox_b0r(
    client,
    *,
    timeout_seconds: float = 60.0,
    poll_interval: float = 0.75,
    monotonic=time.monotonic,
    sleep=time.sleep,
) -> dict[str, Any]: ...
```

`main()` connects `UnrealMCPClient`, prints one JSON result, and returns `0` only when all three PCG generations complete.

- [ ] **Step 4: Run GREEN and commit**

```powershell
python -m unittest scripts.test_run_qingshan_whitebox_b0r -v
git add -- scripts/run_qingshan_whitebox_b0r.py scripts/test_run_qingshan_whitebox_b0r.py
git commit -m "feat: coordinate B0R whitebox generation"
```

### Task 6: Add UE structural and camera validation

**Files:**
- Create: `Content/Python/gamexxk_validate_qingshan_whitebox_b0r.py`
- Create: `Content/Python/gamexxk_qingshan_whitebox_acceptance.py`
- Modify: `scripts/test_qingshan_whitebox_scripts.py`

- [ ] **Step 1: Write failing validator/probe source tests**

Require the validator to expose `validate_whitebox()` and emit marker `GAMEXXK_QINGSHAN_B0R_VALIDATION`. Require the acceptance probe actions `snapshot`, `view`, and `topdown`, and require `set_level_viewport_camera_info`. Assert the four camera IDs exactly match the config.

- [ ] **Step 2: Run RED**

```powershell
python -m unittest scripts.test_qingshan_whitebox_scripts -v
```

Expected: FAIL before the validator and probe exist.

- [ ] **Step 3: Implement the read-only UE validator**

Load only `WHITEBOX_MAP` after refusing to switch when any package is dirty. Return one JSON object with these required checks:

```text
current_map
source_map_clean
protected_actor_snapshot
managed_label_uniqueness
road_spline_count = 3
river_spline_count = 1
bridge_distance_to_road_cm <= 100
bridge_distance_to_river_cm <= 100
building_instances = 16 and <= 19
foliage_instances = 100 and <= 100
mountain_instances = 24 and <= 30
crossings <= 2
runtime_generation_disabled = true
camera_count = 4
camera_ids
terrain_zone_count = 4
player_gate_facing_angle_degrees >= 35
layout_sha256
errors
success
```

Also verify building footprints do not overlap one another, protected anchors, road corridors, or the river corridor; every mountain instance is outside playable bounds; every saved CameraActor matches its configured world transform/FOV within tolerance; and the duplicate-map PlayerStart forward yaw differs from the direction to `NorthGateFAnchor` by at least `35°`.

- [ ] **Step 4: Implement acceptance actions**

`snapshot` returns all managed transforms and canonical counts. `view --camera-id ID` finds the saved CameraActor, verifies its transform and FOV against config, calls `set_level_viewport_camera_info` with that actor's location/rotation, and returns location, target, rotation, and FOV. `topdown` uses the center of `world_bounds_cm`, height `42000cm`, and pitch `-90` to expose the complete layout.

- [ ] **Step 5: Run host tests GREEN and commit**

```powershell
python -m unittest scripts.test_qingshan_whitebox_config scripts.test_qingshan_whitebox_acceptance scripts.test_qingshan_whitebox_scripts scripts.test_run_qingshan_whitebox_b0r -v
git add -- Content/Python/gamexxk_validate_qingshan_whitebox_b0r.py Content/Python/gamexxk_qingshan_whitebox_acceptance.py scripts/test_qingshan_whitebox_scripts.py
git commit -m "test: validate qingshan B0R whitebox"
```

### Task 7: Execute the UE whitebox pipeline and prove determinism

**Files:**
- Create via UE: `Content/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R.umap`
- Create via UE: `Content/GameXXK/Environment/TownPCG/B0R/*.uasset`
- Record: `docs/production/evidence/qingshan-pcg-whitebox-b0r/layout-run-a.json`
- Record: `docs/production/evidence/qingshan-pcg-whitebox-b0r/layout-run-b.json`
- Record: `docs/production/evidence/qingshan-pcg-whitebox-b0r/validation.json`

- [ ] **Step 1: Confirm UE MCP and QuickRoad status**

Run:

```powershell
python scripts/ue_mcp_smoke.py
python -m unittest scripts.test_quick_road_install -v
```

Expected: MCP smoke reports `ok=true`; QuickRoad installation test passes. Through a read-only UE Python probe, record whether an actual QuickRoad class/asset is used. If only tagged splines are available, record `quickroad_status=proxy_spline` and continue without claiming plugin integration.

- [ ] **Step 2: Generate the whitebox**

Run:

```powershell
python scripts/run_qingshan_whitebox_b0r.py --timeout-seconds 90
```

Expected: `success=true`, terminal state `complete`, and counts `16/100/24`.

- [ ] **Step 3: Run the UE validator**

Use `UnrealMCPClient.run_project_python_file` for:

```text
Content/Python/gamexxk_validate_qingshan_whitebox_b0r.py
```

Expected: `success=true`, no errors, source map clean, and four camera IDs present. Save stdout JSON as `validation.json` without hand-editing.

- [ ] **Step 4: Prove deterministic regeneration**

Run the acceptance `snapshot` action and save canonical evidence to `layout-run-a.json`. Clear and rerun the complete whitebox coordinator, then save `layout-run-b.json`. Compare counts, sorted transforms, and SHA-256.

Expected: both SHA-256 values are identical and all instance counts match exactly.

- [ ] **Step 5: Save only B0R assets and commit UE artifacts**

Use MCP `save_dirty_packages()` and require no source-map package in `saved` or `dirty_after`. Stage only the Dev map, B0R PCG assets, and three evidence JSON files; verify `Config/DefaultEditor.ini` is unstaged.

```powershell
git commit -m "feat: build qingshan B0R whitebox map"
```

### Task 8: Capture four review views and record whitebox acceptance

**Files:**
- Create: `docs/production/evidence/qingshan-pcg-whitebox-b0r/topdown.png`
- Create: `docs/production/evidence/qingshan-pcg-whitebox-b0r/gate-arrival.png`
- Create: `docs/production/evidence/qingshan-pcg-whitebox-b0r/town-core.png`
- Create: `docs/production/evidence/qingshan-pcg-whitebox-b0r/main-bridge.png`
- Create: `docs/production/evidence/qingshan-pcg-whitebox-b0r/south-dock.png`
- Create: `docs/production/qingshan-pcg-whitebox-b0r.md`

- [ ] **Step 1: Position and capture the top-down debug view**

Run the UE acceptance `topdown` action, then capture only the visible UE viewport. Verify the PNG signature and a 16:9-or-wider review resolution of at least `1280×720`.

- [ ] **Step 2: Capture each fixed camera independently**

For each camera ID, call the `view` action, capture the UE viewport, and save to the exact filenames above. Do not build a four-panel collage and do not use image generation in this task.

- [ ] **Step 3: Perform the visual whitebox checklist**

Record pass/fail for:

```text
North gate reads on the map's right side.
Initial player-facing route does not point straight at the gate.
Main road bends left into town.
Largest building is dominant but off-center.
Small buildings vary in size, height, setback, and yaw.
River reads diagonally with at least two bends.
Main bridge visibly connects both road and river banks.
South dock is lower than the town core.
Mountain ring encloses the map without blocking the gate opening.
Every view has readable foreground, midground, and background.
```

Any failure returns to Task 1 coordinates or Task 4 assembly; do not repair composition with stylized generation.

- [ ] **Step 4: Run protected gameplay regression**

Run `scripts/gamexxk_real_play_flow_mcp.py` against the production flow without redirecting it to B0R.

Expected: main menu → `L_QingshanInn`; town UI; quest NPC `F`; manual save persistence; north-gate `F`; route map; battle screen all pass.

- [ ] **Step 5: Run the final host suite and production validator**

```powershell
python -m unittest scripts.test_qingshan_whitebox_config scripts.test_qingshan_whitebox_acceptance scripts.test_qingshan_whitebox_scripts scripts.test_run_qingshan_whitebox_b0r -v
python scripts/harness_state_validator.py
```

Expected: all listed unit tests pass and production-unit validation exits `0`.

- [ ] **Step 6: Write and commit the production record**

Record config/map/graph paths, seed, QuickRoad status, instance counts, canonical SHA-256, protected actor snapshots, exact camera transforms, image hashes, validation commands, visual checklist, known whitebox limitations, and the statement that no stylized keyframe has been generated.

```powershell
git add -- docs/production/qingshan-pcg-whitebox-b0r.md docs/production/evidence/qingshan-pcg-whitebox-b0r
git commit -m "docs: record qingshan B0R whitebox acceptance"
```

## Completion gate

The whitebox is complete only when:

- UBT succeeds after the narrow C++ allowlist change, without Live Coding or Hot Reload.
- UE MCP setup/finalize completes with building, foliage, and mountain counts `16/100/24`.
- Two complete regenerations have identical canonical SHA-256 values.
- The UE validator returns `success=true` with no source-map dirtiness.
- The four fixed camera captures and top-down debug capture pass the visual checklist.
- The existing production gameplay regression still passes.
- `Config/DefaultEditor.ini`, `L_QingshanInn`, and user-tuned assets remain unstaged and unchanged by B0R automation.

Only after the user approves these five whitebox images should a separate implementation plan generate four independent atmosphere keyframes using the UE captures, Style v003, and the all-category medium-exaggeration rules.
