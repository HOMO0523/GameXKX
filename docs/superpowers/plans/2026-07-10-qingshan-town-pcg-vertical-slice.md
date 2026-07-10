# Qingshan Town PCG Vertical Slice Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Install and validate QuickRoad 5.8, then build an isolated Qingshan town prototype map where a real UE PCG component places the existing toon shop along the approved north-gate-to-bridge layout without modifying `L_QingshanInn`.

**Architecture:** Vendor QuickRoad as a project plugin and verify its demo assets before touching prototype content. Keep layout and performance values in a versioned JSON contract; use a small editor-only C++ automation library to author a deterministic `Create Points → Static Mesh Spawner` PCG graph, and an idempotent UE Python script to duplicate the town map, create fixed layout anchors, attach the graph, and generate instances. This is a vertical slice: QuickRoad-compatible road and edge tags are present, while the first PCG graph uses twelve explicit roadside lot points; replacing that source with sampled QuickRoad RoadEdge data is a later plan after the plugin workflow is visually proven.

**Tech Stack:** Unreal Engine 5.8, QuickRoad 1.0 project plugin, UE PCG, GameXXKEditor C++, Unreal Python, UE 5.8 MCP (`scripts/ue_mcp_client.py`), Python `unittest`, UBT without Live Coding.

---

## Scope and file map

This plan creates or changes only the following units:

- `Plugins/Quick_Road/**`: unmodified vendored third-party plugin.
- `GameXXK.uproject`: enables QuickRoad, PCG, and ProceduralMeshComponent.
- `Config/GameXXK/TownPCG/QingshanTownVerticalSlice.json`: single source of truth for prototype layout and conservative performance limits.
- `Content/Python/gamexxk_qingshan_town_pcg_config.py`: host-testable JSON validation.
- `Source/GameXXKEditor/Public/GameXXKTownPCGAutomationLibrary.h`: Python-callable editor automation contract.
- `Source/GameXXKEditor/Private/GameXXKTownPCGAutomationLibrary.cpp`: deterministic PCG graph authoring and attachment.
- `Source/GameXXKEditor/GameXXKEditor.Build.cs`: PCG editor dependencies.
- `Content/Python/gamexxk_assemble_qingshan_town_pcg_vertical_slice.py`: idempotent prototype map assembly.
- `Content/Python/gamexxk_validate_qingshan_town_pcg_vertical_slice.py`: UE-side structural and performance-budget validation.
- `scripts/test_quick_road_install.py`: plugin installation contract test.
- `scripts/test_qingshan_town_pcg_config.py`: layout/configuration unit tests.
- `scripts/test_qingshan_town_pcg_scripts.py`: Python script contract tests.
- UE assets created by automation under `/Game/GameXXK/Environment/TownPCG/VerticalSlice` and `/Game/GameXXK/Maps/Prototype/L_QingshanTown_PCG_Prototype`.

The existing `/Game/GameXXK/Maps/L_QingshanInn` and all placed gameplay actors are read-only inputs.

### Task 1: Vendor and compile QuickRoad 5.8

**Files:**
- Create: `scripts/test_quick_road_install.py`
- Create: `Plugins/Quick_Road/**`
- Modify: `GameXXK.uproject`

- [ ] **Step 1: Write the failing plugin contract test**

Create `scripts/test_quick_road_install.py` with this complete test:

```python
from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
UPROJECT = ROOT / "GameXXK.uproject"
PLUGIN = ROOT / "Plugins" / "Quick_Road" / "Quick_Road.uplugin"


class QuickRoadInstallTests(unittest.TestCase):
    def test_quick_road_and_dependencies_are_enabled(self):
        self.assertTrue(PLUGIN.is_file())
        descriptor = json.loads(PLUGIN.read_text(encoding="utf-8-sig"))
        self.assertEqual(descriptor["FriendlyName"], "Quick Road")
        self.assertEqual(descriptor["EngineVersion"], "5.8.0")
        self.assertTrue(descriptor["CanContainContent"])

        project = json.loads(UPROJECT.read_text(encoding="utf-8-sig"))
        enabled = {entry["Name"]: entry.get("Enabled", False) for entry in project["Plugins"]}
        for name in ("Quick_Road", "PCG", "ProceduralMeshComponent"):
            self.assertTrue(enabled.get(name), f"{name} must be enabled")


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the test and verify the pre-install failure**

Run:

```powershell
python -m unittest scripts.test_quick_road_install -v
```

Expected: FAIL because `Plugins/Quick_Road/Quick_Road.uplugin` does not exist.

- [ ] **Step 3: Save the running editor before plugin installation**

Use `UnrealMCPClient` on port `18765`; stop PIE only if active, call `save_dirty_packages()`, and require `dirty_after` to be empty. Do not close the editor if MCP cannot save dirty packages.

- [ ] **Step 4: Copy the plugin without changing vendor files**

Verify that the resolved destination is exactly `D:\UE5 demo\GameXXK\Plugins\Quick_Road`, then copy from:

```text
C:\Users\shxuw\Downloads\260703_虚幻插件QuickRoad_V1(带演示关卡版)\Quick_Road_5.8\Quick_Road_5.8
```

Expected: the destination contains `Quick_Road.uplugin`, `Source`, `Content`, and `Resources`; no pre-existing project plugin directory is overwritten.

- [ ] **Step 5: Enable the three plugin entries in the project descriptor**

Add these entries to the existing `Plugins` array without removing or reordering unrelated entries:

```json
{
  "Name": "Quick_Road",
  "Enabled": true,
  "TargetAllowList": ["Editor"]
},
{
  "Name": "PCG",
  "Enabled": true
},
{
  "Name": "ProceduralMeshComponent",
  "Enabled": true
}
```

- [ ] **Step 6: Run the plugin contract test**

Run:

```powershell
python -m unittest scripts.test_quick_road_install -v
```

Expected: 1 test passes.

- [ ] **Step 7: Close the saved editor and perform a clean UBT verification**

Run the project compile through `scripts/ue_tdd_pipeline.py` or directly through UE 5.8 UBT after the saved editor exits. Live Coding and Hot Reload are forbidden.

Expected: `GameXXKEditor Win64 Development` builds successfully with the QuickRoad module.

- [ ] **Step 8: Restart UE and verify the plugin content through MCP**

Run `scripts/ue_mcp_smoke.py`, then query for these assets:

```text
/Quick_Road/L_QuickRoad
/Quick_Road/PCG_Graph/PCG_CityRoads_Main
/Quick_Road/PCG_Graph/NewPCGGraph
```

Expected: MCP reconnects and all three assets load.

- [ ] **Step 9: Commit the plugin installation**

```powershell
git add -- GameXXK.uproject Plugins/Quick_Road scripts/test_quick_road_install.py
git commit -m "build: install QuickRoad 5.8 plugin"
```

### Task 2: Define the deterministic layout and performance contract

**Files:**
- Create: `Config/GameXXK/TownPCG/QingshanTownVerticalSlice.json`
- Create: `Content/Python/gamexxk_qingshan_town_pcg_config.py`
- Create: `scripts/test_qingshan_town_pcg_config.py`

- [ ] **Step 1: Write the failing configuration test**

The test must load the module without Unreal and assert:

```python
self.assertEqual(config["schema_version"], 1)
self.assertEqual(config["seed"], 20260710)
self.assertEqual(config["source_map"], "/Game/GameXXK/Maps/L_QingshanInn")
self.assertEqual(config["prototype_map"], "/Game/GameXXK/Maps/Prototype/L_QingshanTown_PCG_Prototype")
self.assertEqual(config["north_gate_label"], "QingshanInn_TownExit")
self.assertEqual(config["building_limit"], 12)
self.assertLessEqual(config["building_limit"], config["building_hard_cap"])
self.assertEqual(config["building_hard_cap"], 16)
self.assertEqual(len(config["building_points"]), 12)
self.assertEqual(config["building_mesh"], "/Game/GameXXK/Environment/TownPCG/Prototype/QingshanShopA/SM_Qingshan_Shop_A_HQ_Retop50K")
```

Also assert that every point contains `location_cm`, `yaw_degrees`, and `side`; left-side points face east and right-side points face west.

- [ ] **Step 2: Run the test and verify it fails**

Run:

```powershell
python -m unittest scripts.test_qingshan_town_pcg_config -v
```

Expected: FAIL because the loader and JSON do not exist.

- [ ] **Step 3: Create the JSON contract**

Use these fixed prototype values:

```json
{
  "schema_version": 1,
  "seed": 20260710,
  "source_map": "/Game/GameXXK/Maps/L_QingshanInn",
  "prototype_map": "/Game/GameXXK/Maps/Prototype/L_QingshanTown_PCG_Prototype",
  "asset_root": "/Game/GameXXK/Environment/TownPCG/VerticalSlice",
  "graph_path": "/Game/GameXXK/Environment/TownPCG/VerticalSlice/PCG_QingshanTown_VerticalSlice",
  "north_gate_label": "QingshanInn_TownExit",
  "building_mesh": "/Game/GameXXK/Environment/TownPCG/Prototype/QingshanShopA/SM_Qingshan_Shop_A_HQ_Retop50K",
  "building_limit": 12,
  "building_hard_cap": 16,
  "main_road_width_cm": 800,
  "river_width_cm": 2000,
  "bridge_width_cm": 900,
  "road_setback_cm": 250,
  "tree_instance_limit": 48,
  "prop_instance_limit": 24,
  "runtime_generation": false,
  "building_points": [
    {"location_cm": [-900, -1600, 0], "yaw_degrees": 0, "side": "left"},
    {"location_cm": [-900, -3200, 0], "yaw_degrees": 0, "side": "left"},
    {"location_cm": [-900, -4800, 0], "yaw_degrees": 0, "side": "left"},
    {"location_cm": [-900, -6400, 0], "yaw_degrees": 0, "side": "left"},
    {"location_cm": [-900, -8000, 0], "yaw_degrees": 0, "side": "left"},
    {"location_cm": [-900, -9600, 0], "yaw_degrees": 0, "side": "left"},
    {"location_cm": [900, -1600, 0], "yaw_degrees": 180, "side": "right"},
    {"location_cm": [900, -3200, 0], "yaw_degrees": 180, "side": "right"},
    {"location_cm": [900, -4800, 0], "yaw_degrees": 180, "side": "right"},
    {"location_cm": [900, -6400, 0], "yaw_degrees": 180, "side": "right"},
    {"location_cm": [900, -8000, 0], "yaw_degrees": 180, "side": "right"},
    {"location_cm": [900, -9600, 0], "yaw_degrees": 180, "side": "right"}
  ]
}
```

All locations are offsets from the fixed north-gate anchor after the prototype copy is created. The river and bridge are placed south of the final pair of building points.

- [ ] **Step 4: Implement the host-testable loader**

`gamexxk_qingshan_town_pcg_config.py` must expose `CONFIG_PATH`, `load_config(path=CONFIG_PATH)`, and `validate_config(data)`. Validation must reject schema versions other than `1`, duplicate building points, a `building_limit` outside `1..building_hard_cap`, more listed points than the hard cap, missing mesh/map paths, or `runtime_generation=true`.

- [ ] **Step 5: Run the unit test**

Run:

```powershell
python -m unittest scripts.test_qingshan_town_pcg_config -v
```

Expected: all configuration tests pass.

- [ ] **Step 6: Commit the configuration contract**

```powershell
git add -- Config/GameXXK/TownPCG/QingshanTownVerticalSlice.json Content/Python/gamexxk_qingshan_town_pcg_config.py scripts/test_qingshan_town_pcg_config.py
git commit -m "test: define qingshan town pcg contract"
```

### Task 3: Add the editor-only PCG graph authoring library

**Files:**
- Create: `Source/GameXXKEditor/Public/GameXXKTownPCGAutomationLibrary.h`
- Create: `Source/GameXXKEditor/Private/GameXXKTownPCGAutomationLibrary.cpp`
- Create: `scripts/test_qingshan_town_pcg_scripts.py`
- Modify: `Source/GameXXKEditor/GameXXKEditor.Build.cs`

- [ ] **Step 1: Add a failing source contract test**

Extend `scripts/test_qingshan_town_pcg_scripts.py` to require the header and implementation and assert the source contains these public operations:

```text
CreateOrUpdateTownPCGGraph
AttachTownPCGGraph
GenerateTownPCG
ClearTownPCG
```

Run the test and expect failure because the files do not exist.

- [ ] **Step 2: Declare the Python-callable API**

The header must define `UGameXXKTownPCGAutomationLibrary : UBlueprintFunctionLibrary` with four `BlueprintCallable` editor functions:

```cpp
UFUNCTION(BlueprintCallable, Category="GameXXK|Town PCG")
static FString CreateOrUpdateTownPCGGraph(
    const FString& GraphAssetPath,
    const FString& StaticMeshPath,
    const TArray<FTransform>& BuildingTransforms);

UFUNCTION(BlueprintCallable, Category="GameXXK|Town PCG")
static FString AttachTownPCGGraph(
    const FString& ActorLabel,
    const FString& GraphAssetPath,
    const FVector& Center,
    const FVector& Extent);

UFUNCTION(BlueprintCallable, Category="GameXXK|Town PCG")
static FString GenerateTownPCG(const FString& ActorLabel);

UFUNCTION(BlueprintCallable, Category="GameXXK|Town PCG")
static FString ClearTownPCG(const FString& ActorLabel);
```

Each function returns compact JSON with `success`, `operation`, `actor_label`, `graph`, `point_count`, `generated_component_count`, and `error` fields as applicable.

- [ ] **Step 3: Add PCG module dependencies**

Add `"PCG"` to `PublicDependencyModuleNames` and `"PCGEditor"` to `PrivateDependencyModuleNames`; preserve all existing dependencies.

- [ ] **Step 4: Implement deterministic graph authoring**

`CreateOrUpdateTownPCGGraph` must:

1. Load the static mesh and reject a missing or non-static-mesh asset.
2. Create or load a `UPCGGraph` package at the requested asset path.
3. Remove previously authored nodes except the graph input/output nodes.
4. Add `UPCGCreatePointsSettings` and fill `PointsToCreate` with the supplied transforms, density `1.0`, and stable per-point seeds derived from the point index.
5. Add `UPCGStaticMeshSpawnerSettings`, set selector type to `UPCGMeshSelectorWeighted`, and add one `FPCGMeshSelectorWeightedEntry(mesh, 1)`.
6. Connect the Create Points default output to the spawner default input, then connect the spawner output to the graph output.
7. Save the package and return the authored point count.

`AttachTownPCGGraph` must find or spawn one `APCGVolume`, set its label, center and box extent, assign the graph through its `UPCGComponent`, set editor generation, and keep runtime generation disabled.

`GenerateTownPCG` and `ClearTownPCG` must operate only on the uniquely labelled PCG actor and return an error if zero or multiple actors match.

- [ ] **Step 5: Run the source contract test**

Run:

```powershell
python -m unittest scripts.test_qingshan_town_pcg_scripts -v
```

Expected: the source API contract passes.

- [ ] **Step 6: Save packages, close the editor, and compile through UBT**

Use MCP to save dirty packages before closing. Compile `GameXXKEditor Win64 Development` without Live Coding.

Expected: zero compiler or linker errors from PCG symbols.

- [ ] **Step 7: Restart UE and probe the automation library**

Through MCP, verify `unreal.GameXXKTownPCGAutomationLibrary` exists and all four methods are callable.

- [ ] **Step 8: Commit the editor automation library**

```powershell
git add -- Source/GameXXKEditor Content/Python/gamexxk_qingshan_town_pcg_config.py scripts/test_qingshan_town_pcg_scripts.py
git commit -m "feat: add town pcg graph automation"
```

### Task 4: Assemble the isolated water-axis prototype map

**Files:**
- Create: `Content/Python/gamexxk_assemble_qingshan_town_pcg_vertical_slice.py`
- Modify: `scripts/test_qingshan_town_pcg_scripts.py`
- Create via UE: `/Game/GameXXK/Maps/Prototype/L_QingshanTown_PCG_Prototype`
- Create via UE: `/Game/GameXXK/Environment/TownPCG/VerticalSlice/PCG_QingshanTown_VerticalSlice`

- [ ] **Step 1: Write the failing assembly-script contract test**

Require these stable constants and labels:

```python
self.assertEqual(module.SOURCE_MAP, "/Game/GameXXK/Maps/L_QingshanInn")
self.assertEqual(module.PROTOTYPE_MAP, "/Game/GameXXK/Maps/Prototype/L_QingshanTown_PCG_Prototype")
self.assertEqual(module.PCG_ACTOR_LABEL, "QingshanTown_PCG_Buildings")
self.assertIn("QingshanInn_TownExit", module.PRESERVED_LABELS)
self.assertIn("PlayerStart_QingshanInn", module.PRESERVED_LABELS)
```

Also require `Quick_Road_CityScope`, `Quick_Road_MainRoad`, and `Quick_Road_RoadEdge` in the script's tag contract.

- [ ] **Step 2: Run the contract test and verify failure**

Run:

```powershell
python -m unittest scripts.test_qingshan_town_pcg_scripts -v
```

Expected: FAIL because the assembly script does not exist.

- [ ] **Step 3: Implement idempotent map duplication**

The script must duplicate `L_QingshanInn` only when the prototype map does not yet exist, load only the prototype, and verify the source map package remains clean. It must preserve `QingshanInn_TownExit`, the quest NPC path, and `PlayerStart_QingshanInn` in the duplicate.

- [ ] **Step 4: Create fixed layout actors relative to the north gate**

Create or update uniquely labelled actors for:

```text
QingshanTown_CityScope          tag Quick_Road_CityScope
QingshanTown_MainRoad          tag Quick_Road_MainRoad
QingshanTown_RoadEdge_Left     tag Quick_Road_RoadEdge
QingshanTown_RoadEdge_Right    tag Quick_Road_RoadEdge
QingshanTown_River             tag TownPCG_River
QingshanTown_Bridge            tag TownPCG_FixedAnchor
QingshanTown_Market            tag TownPCG_FixedAnchor
QingshanTown_SouthWharf        tag TownPCG_FixedAnchor
```

Use spline actors for scope, road, road edges, and river. Use temporary static-mesh actors for bridge, market, wharf, and tree-debug markers. Temporary meshes must use `/Engine/BasicShapes` or `/Quick_Road/Mesh` assets, include `PrototypeOnly` tags, and never be copied into a production asset set.

- [ ] **Step 5: Author and attach the PCG graph**

Convert the twelve JSON point offsets into world transforms relative to `QingshanInn_TownExit`, call `CreateOrUpdateTownPCGGraph`, attach the graph to `QingshanTown_PCG_Buildings`, then call `GenerateTownPCG`.

Expected: the JSON results report 12 source points and one unique PCG actor.

- [ ] **Step 6: Save only prototype packages**

Save the graph and prototype map, then call `save_dirty_packages()` through MCP. Require `/Game/GameXXK/Maps/L_QingshanInn` not to appear in the saved or dirty package list.

- [ ] **Step 7: Run the assembly script twice**

Execute with `run_project_python_file` twice.

Expected: both runs succeed, the second creates no duplicate labelled actors, and the PCG graph still contains exactly 12 source points.

- [ ] **Step 8: Commit the assembly automation**

```powershell
git add -- Content/Python/gamexxk_assemble_qingshan_town_pcg_vertical_slice.py scripts/test_qingshan_town_pcg_scripts.py Content/GameXXK/Maps/Prototype Content/GameXXK/Environment/TownPCG/VerticalSlice
git commit -m "feat: assemble qingshan pcg vertical slice"
```

### Task 5: Add UE-side structural validation

**Files:**
- Create: `Content/Python/gamexxk_validate_qingshan_town_pcg_vertical_slice.py`
- Modify: `scripts/test_qingshan_town_pcg_scripts.py`

- [ ] **Step 1: Write the failing validator contract test**

Require the validator to expose:

```text
PROTOTYPE_MAP
REQUIRED_FIXED_LABELS
REQUIRED_QUICK_ROAD_TAGS
PCG_ACTOR_LABEL
BUILDING_HARD_CAP
```

Expected: FAIL before the validator exists.

- [ ] **Step 2: Implement the UE validator**

The validator must load only the prototype map and print one JSON object containing:

- `success` and `errors`.
- Required fixed labels and missing labels.
- QuickRoad tag counts for CityScope, MainRoad, and RoadEdge.
- PCG graph path and source-point count.
- Generated PCG actor count and instanced-component count.
- Static mesh identity for generated building instances.
- Building instance count and hard-cap check.
- Bridge-to-river and road-to-bridge centerline tolerances.
- Collision mode for the building mesh.
- Confirmation that runtime graph generation is disabled.
- Confirmation that `L_QingshanInn` is not dirty.

The validator passes only when there is one PCG actor, 12 generated building instances, all required anchors/tags exist, the hard cap is respected, and the original map remains clean.

- [ ] **Step 3: Run host tests**

Run:

```powershell
python -m unittest scripts.test_quick_road_install scripts.test_qingshan_town_pcg_config scripts.test_qingshan_town_pcg_scripts -v
```

Expected: all tests pass.

- [ ] **Step 4: Run the validator through UE MCP**

Execute `gamexxk_validate_qingshan_town_pcg_vertical_slice.py` through `run_project_python_file`.

Expected: `success=true`, `building_instance_count=12`, `building_hard_cap=16`, and no errors.

- [ ] **Step 5: Commit validation**

```powershell
git add -- Content/Python/gamexxk_validate_qingshan_town_pcg_vertical_slice.py scripts/test_qingshan_town_pcg_scripts.py
git commit -m "test: validate qingshan pcg vertical slice"
```

### Task 6: Visual, gameplay, and performance acceptance

**Files:**
- Verify: `/Game/GameXXK/Maps/Prototype/L_QingshanTown_PCG_Prototype`
- Verify: `/Game/GameXXK/Maps/L_QingshanInn`
- Record: `docs/production/qingshan-town-pcg-vertical-slice.md`

- [ ] **Step 1: Inspect the prototype from the approved player route**

Open the prototype map and capture editor/PIE views from north gate, market, bridge, and south wharf.

Expected: the route reads clearly as north gate → main road → market → bridge → wharf; buildings face the road; no building intersects the river, bridge, road, or another building.

- [ ] **Step 2: Confirm PCG ownership and deterministic regeneration**

Record the twelve building transforms, call `ClearTownPCG`, regenerate, and compare transforms.

Expected: transforms match exactly for seed `20260710`, and generated buildings are owned by PCG instanced components rather than twelve independent actors.

- [ ] **Step 3: Measure the conservative performance profile**

In PIE at 1080p, record `Stat Unit`, `Stat GPU`, `Stat RHI`, instance counts, draw calls, and approximate texture memory while walking the complete route.

Expected: no sustained frame time above 16.67ms on the agreed RTX 2060 / RX 6600 class target; building count remains 12; tree markers remain at or below 48; prop markers remain at or below 24. If the available machine differs from the target, record measurements without claiming target-hardware acceptance.

- [ ] **Step 4: Run the existing north-gate gameplay regression on the production map**

Run `scripts/gamexxk_real_play_flow_mcp.py` against the existing flow.

Expected: Start/New Game opens `L_QingshanInn`, town UI is visible, F accepts the quest, save persistence is intact, town enters route map, and route node enters battle. Do not redirect the production flow to the prototype map in this task.

- [ ] **Step 5: Write the production record**

Record plugin version, graph/map paths, seed, source point count, generated instance count, mesh path, material slots, texture resolution, Nanite state, collision mode, measured performance, screenshots, validation commands, and any target-hardware qualification in `docs/production/qingshan-town-pcg-vertical-slice.md`.

- [ ] **Step 6: Validate production-state files**

Run:

```powershell
python scripts/harness_state_validator.py
```

Expected: production-unit files validate successfully.

- [ ] **Step 7: Commit the verified vertical slice record**

```powershell
git add -- docs/production/qingshan-town-pcg-vertical-slice.md
git commit -m "docs: record qingshan pcg vertical slice"
```

## Completion gate

The vertical slice is complete only when all host tests pass, UBT succeeds without Live Coding, UE MCP validation returns success, the prototype visually follows the approved A layout, the original `L_QingshanInn` remains unchanged, and the production gameplay regression still passes. Formal bridge, river, tree, and five additional building assets are explicitly outside this plan; their JSON → reference image → Tripo high precision → retopology → texture → Toon BSDF production is the next standalone plan after this slice is accepted.
