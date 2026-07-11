# Qingshan PCG Dress B1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and verify an isolated, visually enriched Qingshan B1 town map with deterministic UE PCG populations, a broad stylized Landscape, real QuickRoad roads/intersections/road edges, Paper2D vegetation, reusable props, and four review cameras.

**Architecture:** B1 is an anchor-local JSON overlay on B0R. Host tools generate a deterministic 16-bit heightmap and simple six-building/prop proxy kit; a generic QuickRoad editor facade and the existing project PCG automation expose repeatable infrastructure and ISM/HISM spawning to UE Python. The B1 assembler duplicates B0R, cleans only a fixed cloned-proxy allowlist, generates PCG populations plus unique bridge/river/Paper2D/camera actors, and records a live scene manifest rather than trusting configuration hashes.

**Tech Stack:** Unreal Engine 5.8, QuickRoad, UE PCG, Paper2D, Substrate Toon BSDF, Blender 4.2.3, UE Python through project MCP, C++/UHT/UBT, Python `unittest`, Pillow, JSON/SHA-256 evidence.

---

## File Structure

- `Config/GameXXK/TownPCG/QingshanDressB1.json`: B1 overlay, coordinate contract, protected hashes, budgets, asset IDs, placements, and infrastructure parameters.
- `Content/Python/gamexxk_qingshan_dress_b1_config.py`: host-safe B0R merge and validation.
- `scripts/test_qingshan_dress_b1_config.py`: B1 contract tests.
- `scripts/build_qingshan_b1_heightmap.py`: deterministic 505 x 505 16-bit broad terrain and river trench generator.
- `scripts/test_qingshan_b1_heightmap.py`: heightmap structure and sampled-elevation tests.
- `scripts/blender/build_qingshan_b1_proxy_kit.py`: six building meshes plus reusable prop/card/mountain proxy meshes.
- `scripts/test_qingshan_b1_proxy_kit.py`: Blender source/manifest contract tests.
- `Content/ArtSource/Qingshan/B1/H_QS_B1_Terrain_505.png`: generated base heightmap.
- `Content/ArtSource/Qingshan/B1/H_QS_B1_Terrain_505.samples.json`: decoded terrain samples and PNG digest.
- `Content/ArtSource/Qingshan/B1/T_QS_B1_PlantProxy_4F.png`: four-frame approved-style plant strip.
- `Plugins/Quick_Road/Source/Quick_Road/Public/Quick_RoadAutomationLibrary.h`: generic QuickRoad callable facade.
- `Plugins/Quick_Road/Source/Quick_Road/Private/Quick_RoadAutomationLibrary.cpp`: landscape/road/reset/intersection/influence/edge/bake/audit implementation.
- `Plugins/Quick_Road/Source/Quick_Road/Quick_Road.Build.cs`: JSON dependency for safe structured results.
- `Source/GameXXKEditor/Private/GameXXKTownPCGAutomationLibrary.cpp`: add only the exact B1 graph root to the existing allowlist.
- `Source/GameXXKEditor/Public/GameXXKTownPCGAutomationLibrary.h`: add the explicit seed, road-edge exclusion, and material-override graph-authoring contract without changing existing call semantics.
- `scripts/test_quickroad_automation_facade.py`: facade and B1 PCG-root safety tests.
- `Content/Python/gamexxk_author_qingshan_dress_b1_assets.py`: FBX/texture import, Toon material instances, PaperSprites, and flipbook authoring.
- `scripts/test_qingshan_dress_b1_assets.py`: asset-authoring tests.
- `Content/Python/gamexxk_assemble_qingshan_dress_b1.py`: B1 map setup, PCG inputs/generation, unique actors, QuickRoad infrastructure, saving.
- `scripts/test_qingshan_dress_b1_scripts.py`: assembler/validator/acceptance source and behavior tests.
- `Content/Python/gamexxk_validate_qingshan_dress_b1.py`: read-only live validator and scene-manifest capture.
- `Content/Python/gamexxk_qingshan_dress_b1_acceptance.py`: two-run determinism, captures, stats, PIE, and regression coordinator.
- `scripts/run_qingshan_dress_b1.py`: host-side MCP state machine for bounded editor steps, asynchronous PCG polling, captures, PIE timing, and strict inner/outer success parsing.
- `docs/production/qingshan-pcg-dress-b1.md`: post-gate production record.
- `docs/production/evidence/qingshan-pcg-dress-b1/*`: live JSON and four camera PNGs.

### Task 1: Define the anchor-local B1 overlay and source-protection contract

**Files:**
- Create: `Config/GameXXK/TownPCG/QingshanDressB1.json`
- Create: `Content/Python/gamexxk_qingshan_dress_b1_config.py`
- Create: `scripts/test_qingshan_dress_b1_config.py`

- [ ] **Step 1: Write failing contract tests**

```python
from gamexxk_qingshan_dress_b1_config import contract_sha256, load_config

def test_coordinate_contract_is_anchor_local():
    config = load_config()
    assert config["coordinate_space"] == "anchor_local"
    assert config["coordinate_reference_actor"] == "QingshanInn_TownExit"
    assert config["landscape"]["center_local_cm"] == [-15500.0, 0.0, 0.0]

def test_protected_files_have_exact_raw_hashes():
    config = load_config()
    assert config["protected_files"] == {
        "Content/GameXXK/Maps/L_QingshanInn.umap": "a3639b38623d00e8ad3e5a610a3e1695a47b38c1d1e6fedb8115e1e9fdf5c8a8",
        "Content/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R.umap": "74292340df0cea97d99e905dd193a921038326bfec2f3ce034a5e9f70bd3f107",
        "Config/GameXXK/TownPCG/QingshanWhiteboxB0R.json": "3f231876d0083bffe28ee555b60af1a20b0966edbde9dc4bb6f2647e920eadb1",
    }

def test_building_and_style_counts_are_exact():
    config = load_config()
    assert len(config["building_plots"]) == 26
    assert config["base_building_count"] == 16
    assert config["additional_building_count"] == 10
    assert {p["archetype_id"] for p in config["building_plots"]} == {
        "gable_shop", "tall_house", "wide_house",
        "courtyard_wing", "bridge_house", "dock_shed",
    }
    assert {p["roof_palette"] for p in config["building_plots"]} == {
        "orange", "teal", "indigo", "ochre",
    }

def test_infrastructure_and_caps_are_conservative():
    config = load_config()
    assert config["landscape"]["resolution"] == [505, 505]
    assert config["landscape"]["edit_layer"] == "QR_B1_Roads"
    assert [n["width_cm"] for n in config["quickroad"]["networks"]] == [800, 450, 400]
    assert config["caps"] == {
        "buildings": 26, "props": 72, "vegetation": 100,
        "animated_vegetation": 30, "mountains": 24, "crossings": 2,
    }
    assert config["runtime_generation"] is False
    assert config["landscape"]["height_encoding"] == {
        "zero_raw": 32768, "units_per_cm_at_scale_z_100": 1.28,
    }
    assert config["quickroad"]["road_material"].endswith("MI_QS_B1_Road_Earth")
    assert {v["frame_variant"] for v in config["vegetation_records"] if not v["animated"]} == {0, 1, 2, 3}
    assert len(contract_sha256(config)) == 64
```

- [ ] **Step 2: Run and verify RED**

```powershell
python -m unittest scripts.test_qingshan_dress_b1_config -v
```

Expected: import/file-not-found failure.

- [ ] **Step 3: Create the exact overlay**

The JSON must contain:

```json
{
  "schema_version": 1,
  "revision_id": "B1",
  "seed": 20260711,
  "coordinate_space": "anchor_local",
  "coordinate_reference_actor": "QingshanInn_TownExit",
  "source_map": "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R",
  "dress_map": "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1",
  "asset_root": "/Game/GameXXK/Environment/TownPCG/B1",
  "runtime_generation": false,
  "expected_base_live_layout_sha256": "15d1b585739a2a180a6022974ec0c365144cbc29511c2426fc0058f1fd43dfbd",
  "landscape": {
    "label": "QS_B1_Landscape",
    "resolution": [505, 505],
    "quads_per_section": 63,
    "sections_per_component": 2,
    "component_count": [4, 4],
    "scale_cm": [100.0, 100.0, 100.0],
    "center_local_cm": [-15500.0, 0.0, 0.0],
    "edit_layer": "QR_B1_Roads",
    "heightmap_source": "Content/ArtSource/Qingshan/B1/H_QS_B1_Terrain_505.png"
  }
}
```

Add `height_encoding` metadata for the Unreal formula `raw = round(32768 + elevation_cm * 128 / scale_z_cm)`. Add the three network records with unique tags `QS_B1_Main`, `QS_B1_CoreNorth`, and `QS_B1_CoreSouth`; use the combined expression `QS_B1_Main|QS_B1_CoreNorth|QS_B1_CoreSouth`; set `road_material` to `/Game/GameXXK/Environment/TownPCG/B1/Materials/MI_QS_B1_Road_Earth`; use road mesh sample 300 cm, width subdivisions 3, curvature `8/2`, influence falloff 250 cm, blend 0.9, vertical offset -5 cm, smooth `4/0.6`, intersection `500/100/50/1.2`, and bake split 5000 cm. Add the exact protected-file hashes from Step 1, all 16 B0R plot assignments, and the ten additional plot records previously approved in the design: `BLD_S_11`, `BLD_S_12`, `BLD_M_06`, `BLD_S_13`, `BLD_S_14`, `BLD_S_15`, `BLD_M_07`, `BLD_S_16`, `BLD_S_17`, and `BLD_S_18`. Give every static vegetation record a `seed + stable_id` SHA-256-derived `frame_variant` 0-3 and collision false; record explicit collision policy for every prop category. Building-mounted sign/lantern/banner records require an `attachment_target_id`; only dock posts may opt into river overlap. Validate every population record against world/Landscape bounds, rotated building footprints, roads, river, and protected anchors so later PCG exclusion cannot reduce the required counts.

- [ ] **Step 4: Implement host-safe validation and merge**

The loader must import B0R `load_config`, copy all 16 accepted geometry records unchanged, add style assignments, append ten records, reject unknown IDs/archetypes/palettes, duplicate IDs, non-finite numbers, invalid caps, mismatched resolution, and any protected-file hash mismatch. Keep two hashes distinct:

```python
def contract_sha256(config):
    payload = {
        "seed": config["seed"],
        "coordinate_space": config["coordinate_space"],
        "landscape": config["landscape"],
        "quickroad": config["quickroad"],
        "building_plots": config["building_plots"],
        "prop_records": config["prop_records"],
        "vegetation_records": config["vegetation_records"],
        "mountain_records": config["mountain_records"],
        "cameras": config["cameras"],
        "caps": config["caps"],
    }
    encoded = json.dumps(payload, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()
```

`expected_base_live_layout_sha256` is retained for the UE validator only; the host loader must not pretend it can recompute the anchor-dependent live B0R hash.

- [ ] **Step 5: Run, verify, and commit**

```powershell
python -m unittest scripts.test_qingshan_dress_b1_config -v
git add -- Config/GameXXK/TownPCG/QingshanDressB1.json Content/Python/gamexxk_qingshan_dress_b1_config.py scripts/test_qingshan_dress_b1_config.py
git commit -m "feat: define qingshan B1 dress contract"
```

Expected: all Task 1 tests pass.

### Task 2: Generate the broad terrain, proxy mesh kit, and Paper2D plant source

**Files:**
- Create: `scripts/build_qingshan_b1_heightmap.py`
- Create: `scripts/test_qingshan_b1_heightmap.py`
- Create: `scripts/blender/build_qingshan_b1_proxy_kit.py`
- Create: `scripts/test_qingshan_b1_proxy_kit.py`
- Create: `Content/ArtSource/Qingshan/B1/H_QS_B1_Terrain_505.png`
- Create: `Content/ArtSource/Qingshan/B1/H_QS_B1_Terrain_505.samples.json`
- Create: `Content/ArtSource/Qingshan/B1/T_QS_B1_PlantProxy_4F.png`

- [ ] **Step 1: Write failing heightmap and proxy-kit tests**

```python
def test_heightmap_is_16bit_505_square_and_has_broad_relief():
    image = Image.open(HEIGHTMAP)
    assert image.size == (505, 505)
    assert image.mode in {"I;16", "I;16L", "I"}
    samples = sample_named_points(image)
    assert samples["core"] > samples["gate"] > samples["dock"] > samples["riverbed"]
    assert samples["core"] - samples["dock"] >= 320
    assert max_adjacent_delta(image) <= 900

def test_proxy_manifest_contains_real_category_meshes():
    manifest = build_manifest_dry_run()
    assert set(manifest["buildings"]) == {
        "gable_shop", "tall_house", "wide_house",
        "courtyard_wing", "bridge_house", "dock_shed",
    }
    assert set(manifest["props"]) == {
        "sign", "lantern", "banner", "fence", "crate", "stall",
        "well", "cart", "rock", "dock_post", "plant_card", "mountain",
    }
    for mesh in manifest["buildings"].values():
        assert mesh["material_slots"] == ["Wall", "Timber", "WindowPaper", "Roof"]
        assert mesh["window_paper_closed"] is True
        assert mesh["normalized_bounds_m"] == [1.0, 1.0, 1.0]
        assert mesh["pivot"] == "bottom_center"
        assert mesh["front_axis"] == "+Y"
```

- [ ] **Step 2: Run and verify RED**

```powershell
python -m unittest scripts.test_qingshan_b1_heightmap scripts.test_qingshan_b1_proxy_kit -v
```

Expected: missing modules/files.

- [ ] **Step 3: Implement deterministic heightmap generation**

Use B0R terrain-zone oriented rectangles as smooth low-frequency targets, blend with smoothstep weights, subtract a Gaussian river trench around the B0R river polyline, and add only two octaves of seeded noise with wavelength at least 6,000 cm and total amplitude at most 20 cm. Encode each sample with `raw = clamp(round(32768 + elevation_cm * 128 / scale_z_cm), 0, 65535)`, write a 16-bit PNG, and emit `H_QS_B1_Terrain_505.samples.json` containing local/pixel coordinates, raw and decoded-centimetre gate/core/south/dock-bank/riverbed values plus the PNG SHA-256. Choose the dock-bank sample at least 1,400 cm from the river centreline rather than sampling the trench. Tests must decode by `elevation_cm = (raw - 32768) * scale_z_cm / 128`, require at most one encoded-step sample error, and prove that the configured centre maps to Landscape actor origin `centerXY - ((resolution - 1) * scaleXY / 2)`. Building plot Z and later UE line traces use the same local coordinate conversion.

- [ ] **Step 4: Implement the Blender proxy kit**

Generate separate low-poly FBX files under `Saved/Automation/QingshanB1ProxyKit`. Reuse the Golden Inn builder's tapered-box, chunky-window, beam, small-bevel, transform-application, and finish-audit helpers or tested equivalents; do not reuse its high-density curved-roof function. Each building is one combined mesh normalized to 1 m x 1 m x 1 m with bottom-centre pivot and +Y frontage, broad 5-10% asymmetry, a 3-5-section overhanging roof shell, one chunky door, dark timber, and closed warm paper windows. The six archetypes must differ in story count, width, wing/awning, roof proportion, and silhouette. Generate the twelve named prop/card/mountain meshes from simple closed geometry with the exact slot table: sign `[Timber,Prop]`, lantern `[Timber,WindowPaper]`, banner `[Timber,Prop]`, fence `[Timber]`, crate `[Timber]`, stall `[Timber,Prop]`, well `[Prop,Timber]`, cart `[Timber]`, rock `[Prop]`, dock_post `[Timber]`, plant_card `[Foliage]`, mountain `[Mountain]`. Do not generate text or tiny roof tiles. Emit `proxy-kit-manifest.json` with current file hashes, stable geometry digests, normalized bounds, vertex/triangle counts, material slots, UV channels, pivot/front-axis, Blender version, and export settings. Cross-run determinism compares geometry digests, not FBX container bytes because FBX timestamps may vary.

- [ ] **Step 5: Generate and inspect the plant strip**

Use the `imagegen` skill with the approved Qingshan references. Generate the four-state source on a perfectly flat magenta chroma key, remove the key locally, then assemble a transparent 2048 x 512 strip from four equal 512 x 512 cells showing one chunky broad-leaf plant leaning left, neutral-left, neutral-right, and right. Per cell, preserve aspect ratio inside a 360 x 432 content box, align the root pivot to `(256,472)` within 4 px, require the crown centroid to move monotonically, largest alpha component at least 98%, transparent borders, alpha-area variation at most 15%, dark ink contour, muted teal/green fill, no text/ground line/disconnected small leaves. Inspect at original resolution and save the accepted strip at the specified source path.

- [ ] **Step 6: Run real generation, verify, and commit**

```powershell
python scripts/build_qingshan_b1_heightmap.py
python -m unittest scripts.test_qingshan_b1_heightmap scripts.test_qingshan_b1_proxy_kit -v
D:\Blender\blender.exe --background --factory-startup --python scripts/blender/build_qingshan_b1_proxy_kit.py -- --output Saved/Automation/QingshanB1ProxyKit
python -m unittest scripts.test_qingshan_b1_proxy_kit -v
git add -- scripts/build_qingshan_b1_heightmap.py scripts/test_qingshan_b1_heightmap.py scripts/blender/build_qingshan_b1_proxy_kit.py scripts/test_qingshan_b1_proxy_kit.py Content/ArtSource/Qingshan/B1/H_QS_B1_Terrain_505.png Content/ArtSource/Qingshan/B1/H_QS_B1_Terrain_505.samples.json Content/ArtSource/Qingshan/B1/T_QS_B1_PlantProxy_4F.png
git commit -m "feat: generate qingshan B1 proxy sources"
```

Expected: host tests pass, Blender exits 0, manifest lists 18 hashed FBX files, and no `.blend1`/temporary sidecars exist.

### Task 3: Expose repeatable QuickRoad infrastructure and the exact B1 PCG graph root

**Files:**
- Create: `Plugins/Quick_Road/Source/Quick_Road/Public/Quick_RoadAutomationLibrary.h`
- Create: `Plugins/Quick_Road/Source/Quick_Road/Private/Quick_RoadAutomationLibrary.cpp`
- Modify: `Plugins/Quick_Road/Source/Quick_Road/Quick_Road.Build.cs`
- Modify: `Source/GameXXKEditor/Public/GameXXKTownPCGAutomationLibrary.h`
- Modify: `Source/GameXXKEditor/Private/GameXXKTownPCGAutomationLibrary.cpp`
- Modify: `scripts/test_qingshan_whitebox_scripts.py`
- Create: `scripts/test_quickroad_automation_facade.py`

- [ ] **Step 1: Write failing source-contract tests**

Require exact API names, existing-generator calls, `LandscapeEditLayer.h`, `Json`, B1 graph-root acceptance, and rejection of sibling/broad roots. Require behavior guards for wrong map, locked layer, incompatible landscape, missing material, repeated reset, exactly three generated width groups, and two consecutive reset/generate/extract cycles yielding the same stable edge labels/count/digest. Require an explicit PCG base seed, edge-exclusion labels/digest, clearance, and material override contract in the public editor header.

```python
REQUIRED_API = {
    "ResetRoadInfrastructure", "EnsureLandscapeInfrastructure",
    "GenerateRoadNetwork", "RebuildRoadIntersections",
    "ClearAndApplyAllRoadInfluence", "ExtractRoadEdges",
    "BakeSingleRoadNetwork", "AuditInfrastructure",
}
```

- [ ] **Step 2: Run and verify RED**

```powershell
python -m unittest scripts.test_quickroad_automation_facade -v
```

Expected: facade missing and B1 graph root rejected.

- [ ] **Step 3: Declare the callable facade**

Use `UQuick_RoadAutomationLibrary : UBlueprintFunctionLibrary`. `EnsureLandscapeInfrastructure` must accept actor label, topology, anchor-transformed desired grid centre/scale, edit-layer name, material object path, and absolute heightmap file path. It computes the actor origin from the grid half-extent instead of assigning the desired centre directly. `GenerateRoadNetwork` accepts the B1 road material path and applies it to ribbon and intersection output. `RebuildRoadIntersections` must receive both intersection parameters and ribbon parameters (`sample distance`, `width subdivisions`, `curvature threshold`, `max curvature subdivisions`). `ClearAndApplyAllRoadInfluence` must accept the exact Landscape label and clear only the named edit layer height contribution before applying/smoothing once. `ExtractRoadEdges` returns stable labels/count/digest. `AuditInfrastructure` returns network count, source tag/width/material records, triangles, intersection patches, edge splines, edit-layer state, and a digest of the active edit-layer height buffer.

- [ ] **Step 4: Implement landscape creation and strict rollback**

Before calling `FQuick_RoadLandscapeCreator::TryCreateLandscape`, snapshot all `ALandscape*`. After success require exactly one new actor, otherwise destroy only the newly discovered actors and return failure. Compute `ActorOriginWorld = DesiredGridCenterWorld + RotationScaleTransform.TransformVector([-(ResolutionX-1)/2, -(ResolutionY-1)/2, 0])`, apply requested label/origin/rotation/scale/material, and call `PostEditChange`. On reuse, require exact topology and compare the reconstructed grid centre; correct or safely repair origin/rotation/scale/material. Include `LandscapeEditLayer.h`; enable layers, create/find the named layer, reject a locked layer, and set it active. Use UE JSON serializers and add `Json` to `Quick_Road.Build.cs`.

- [ ] **Step 5: Implement repeatable road operations**

`ResetRoadInfrastructure` operates only while the current map is under `/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1`. It removes only actors carrying `QingshanB1QuickRoadOwned` plus an allowlisted QuickRoad network/edge/intersection category tag. Generate each road tag with the requested width, ribbon settings, and earth-road material; tag networks as owned. Before and after edge extraction snapshot edge actors, assign the newly created set stable B1 labels and the ownership tag, and reject any unowned/duplicate result. Consolidate the three tags with the same ribbon settings. Before road influence:

```cpp
Landscape->ClearEditLayer(
    RoadLayer->GetGuid(),
    nullptr,
    ELandscapeToolTargetTypeFlags::Heightmap,
    true);
Landscape->SetEditingLayer(RoadLayer->GetGuid());
```

Select the exact Landscape, gather the consolidated network procedural meshes, apply once, smooth once, extract road edges, and return measured JSON. Bake must preserve and restore editor selection.

- [ ] **Step 6: Extend only the exact B1 PCG root**

Add `/Game/GameXXK/Environment/TownPCG/B1/` as a third exact managed graph root. Refactor the root safety tests so `VerticalSlice`, `B0R`, and `B1` are accepted while `/Game/GameXXK/Environment/TownPCG/`, sibling prefixes, source maps, and non-Dev maps remain rejected. Extend graph authoring with an explicit base seed, stable derived point seeds, road-edge actor labels plus geometry digest and minimum clearance, and material override paths; set and audit the attached `UPCGComponent` seed. Preserve the existing three-argument call path for current tests/callers.

- [ ] **Step 7: Verify source tests, cold compile, and commit**

```powershell
python -m unittest scripts.test_quickroad_automation_facade scripts.test_qingshan_whitebox_scripts -v
python scripts/ue_tdd_pipeline.py --pie-duration 1
git add -- Plugins/Quick_Road/Source/Quick_Road/Public/Quick_RoadAutomationLibrary.h Plugins/Quick_Road/Source/Quick_Road/Private/Quick_RoadAutomationLibrary.cpp Plugins/Quick_Road/Source/Quick_Road/Quick_Road.Build.cs Source/GameXXKEditor/Public/GameXXKTownPCGAutomationLibrary.h Source/GameXXKEditor/Private/GameXXKTownPCGAutomationLibrary.cpp scripts/test_qingshan_whitebox_scripts.py scripts/test_quickroad_automation_facade.py
git commit -m "feat: automate QuickRoad and B1 PCG roots"
```

Expected: host tests pass; MCP saves dirty packages before shutdown; UBT exits 0; fresh UE 5.8 editor launches and MCP reconnects.

### Task 4: Import the proxy kit and author Toon/Paper2D assets

**Files:**
- Create: `Content/Python/gamexxk_author_qingshan_dress_b1_assets.py`
- Create: `scripts/test_qingshan_dress_b1_assets.py`
- Create via UE: `/Game/GameXXK/Environment/TownPCG/B1/Materials/*`
- Create via UE: `/Game/GameXXK/Environment/TownPCG/B1/Meshes/*`
- Create via UE: `/Game/GameXXK/Environment/TownPCG/B1/Paper2D/*`

- [ ] **Step 1: Write failing asset-authoring tests**

Assert the script imports exactly 18 source-manifest FBXs, verifies imported bounds near 100 cm for every normalized source, creates one opaque Toon BSDF master plus one dedicated two-sided Masked foliage-card master, material instances for wall/timber/paper/four roofs/ground/road/water/four static foliage UV frames/animated foliage/mountain/props, derives 24 building mesh/material variants for every `(archetype, roof_palette)` pair, assigns `MI_QS_B1_Window_Paper` to every building `WindowPaper` slot, creates four 512 x 512 PaperSprites, and creates a five-FPS flipbook with frame order `[0, 1, 2, 3, 2, 1]`.

- [ ] **Step 2: Run and verify RED**

```powershell
python -m unittest scripts.test_qingshan_dress_b1_assets -v
```

Expected: script missing.

- [ ] **Step 3: Implement idempotent import and materials**

Import all FBXs with `combine_meshes=True`, no imported materials/textures, replacement enabled, and source manifest hash/geometry-digest checks. Require every normalized source bound to be approximately 100 cm and stop on scale drift. Create `M_QS_B1_Toon` with a `BaseColor` vector parameter plus metallic 0, specular 0.15, roughness 0.85 into `MaterialExpressionSubstrateToonBSDF`. Create `M_QS_B1_FoliageCard` as two-sided Masked, with the plant-strip texture feeding BaseColor and Alpha feeding Opacity Mask plus UV scale/offset parameters; derive four static-frame material instances. Create named instances including `MI_QS_B1_Ground`, `MI_QS_B1_Road_Earth`, `MI_QS_B1_Window_Paper`, and four roof palettes. Derive and save 24 building StaticMesh variants from the six source meshes, changing only the `Roof` slot; reject unknown/missing slots. Add simple collision only to solid gameplay-relevant buildings/props. Disable collision on signs, plant cards, lanterns, banners, and mountain backdrops; enable Nanite only when a recorded triangle threshold shows benefit, never merely by category.

- [ ] **Step 4: Implement Paper2D assets**

Import `T_QS_B1_PlantProxy_4F`, create four bottom-centre sprites from exact UV rectangles `(0,0)`, `(512,0)`, `(1024,0)`, `(1536,0)`, and build `FB_QS_B1_Plant_Sway` with the exact ping-pong sequence. Configure the texture for sprite alpha, nearest/trilinear choice recorded in evidence, and save all B1 assets.

- [ ] **Step 5: Run through MCP, verify, and commit**

```powershell
python -m unittest scripts.test_qingshan_dress_b1_assets -v
python scripts/ue_mcp_smoke.py
python -c "import json,sys; sys.path.insert(0,'scripts'); from ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(timeout=60); assert c.connect(); print(json.dumps(c.run_project_python_file('Content/Python/gamexxk_author_qingshan_dress_b1_assets.py'),ensure_ascii=False))"
git add -- Content/Python/gamexxk_author_qingshan_dress_b1_assets.py scripts/test_qingshan_dress_b1_assets.py Content/GameXXK/Environment/TownPCG/B1
git commit -m "feat: author qingshan B1 proxy assets"
```

Expected: script returns `success=true`, six building meshes and twelve category meshes load, four sprites/one flipbook exist, and all saved packages are under the B1 asset root.

### Task 5: Assemble B1 with real PCG and QuickRoad output

**Files:**
- Create: `Content/Python/gamexxk_assemble_qingshan_dress_b1.py`
- Create: `scripts/test_qingshan_dress_b1_scripts.py`
- Create via UE: `/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1`
- Create via UE: `/Game/GameXXK/Environment/TownPCG/B1/Graphs/*`

- [ ] **Step 1: Write failing safety/PCG/scene tests**

Require strict B1 current-map guards around every mutating phase, protected raw hashes before/after, first-run B0R clone cleanup allowlist, normal B1 managed cleanup only, six building PCG graph paths, repeated-prop PCG graphs, far-plant and mountain PCG graphs, 30 direct PaperFlipbook actors, bridge/river/cameras, three QuickRoad tags, real audit parsing, and save target B1 only.

- [ ] **Step 2: Run and verify RED**

```powershell
python -m unittest scripts.test_qingshan_dress_b1_scripts -v
```

Expected: assembler missing.

- [ ] **Step 3: Implement safe map lifecycle and coordinate conversion**

Support bounded phases `setup`, `infrastructure`, `finalize_begin`, and `finalize_poll`. Duplicate B0R only when B1 is absent, unload/reload only B1, then remove cloned B0R generated actors only if the current package is B1, the actor has `QingshanB0RManaged`, and its label is in `B0R_CLONE_CLEANUP_LABELS`. On rerun remove only `QingshanB1Managed`; QuickRoad-owned cleanup remains inside the guarded facade. Resolve exactly one `QingshanInn_TownExit` and use one function for every local-to-world conversion:

```python
def local_to_world(anchor_transform, local_xyz):
    return anchor_transform.transform_location(unreal.Vector(*map(float, local_xyz)))
```

Hash every protected file before and after each phase and fail on any mismatch.

- [ ] **Step 4: Build QuickRoad infrastructure and stable exclusion inputs**

Create the three tagged spline groups from anchor-local B0R records. Split main-road geometry at the bridge protected radius. Reset old owned QuickRoad networks/edges, ensure the 505 Landscape from the generated heightmap using the grid-centre-to-origin formula, generate widths 800/450/400 with the B1 earth-road material, rebuild intersections with full ribbon settings, clear/apply/smooth the named road edit layer once, and extract stable owned road edges. Audit must report all three source tag/width/material records, one consolidated network, nonzero triangles, enabled/unlocked named edit layer, nonzero height digest, stable road-edge labels/digest, and at least one road-edge spline.

- [ ] **Step 5: Generate real PCG populations from measured exclusions**

For every `(archetype_id, roof_palette)` building group, filter its point transforms against the live QuickRoad edge splines at the configured clearance, call the extended `CreateOrUpdateTownPCGGraph` with its derived mesh, material override, explicit base seed, consumed edge labels/digest, and filtered transforms, attach a B1 PCG volume, and call `GenerateTownPCG`. Repeat by shared mesh for the 72 prop records, four deterministic static-frame groups across the 70 far plant-card records, and 24 mountain records. All graph paths stay under `/Game/GameXXK/Environment/TownPCG/B1/Graphs/`. `finalize_begin` starts generation and returns immediately; repeated `finalize_poll` calls report `pending`, `complete`, or `failed` without blocking the editor thread, and record point count plus generated ISM/HISM instance count. Python must not spawn substitute StaticMeshActors for these categories.

- [ ] **Step 6: Create unique semantic actors**

Create the bridge assembly with deck/rails/posts/approach stones, the separate teal river ribbon and dock assembly, and the four cameras `CAM_QS_B1_GATE_ARRIVAL`, `CAM_QS_B1_TOWN_CORE`, `CAM_QS_B1_MAIN_BRIDGE`, `CAM_QS_B1_SOUTH_DOCK`. Spawn exactly 30 `PaperFlipbookActor` plants using `FB_QS_B1_Plant_Sway`, deterministic phase offsets, no collision, and finite cull distance. Snap PCG building Z and unique actors to the deterministic height function/landscape trace; require building-bottom error at most 25 cm.

- [ ] **Step 7: Finalize and save only B1**

Recheck protected hashes, managed label uniqueness, PCG completion/counts, bridge gap, and no dirty source/B0R package. Save the B1 map and B1 graph/assets only. Return `quickroad_status="verified_procmesh"` only from measured audit data.

- [ ] **Step 8: Run host tests and live assembly, then commit**

```powershell
python -m unittest scripts.test_qingshan_dress_b1_scripts -v
python scripts/ue_mcp_smoke.py
python scripts/run_qingshan_dress_b1.py --assemble-once
git add -- Content/Python/gamexxk_assemble_qingshan_dress_b1.py scripts/run_qingshan_dress_b1.py scripts/test_qingshan_dress_b1_scripts.py Content/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1.umap Content/GameXXK/Environment/TownPCG/B1/Graphs
git commit -m "feat: assemble enriched qingshan B1 scene"
```

Expected: all three phases return `success=true`; only B1 map/root packages are saved; real PCG and QuickRoad counts are nonzero.

### Task 6: Validate two-run determinism, performance, and four review cameras

**Files:**
- Create: `Content/Python/gamexxk_validate_qingshan_dress_b1.py`
- Create: `Content/Python/gamexxk_qingshan_dress_b1_acceptance.py`
- Modify: `scripts/test_qingshan_dress_b1_scripts.py`
- Create: `docs/production/evidence/qingshan-pcg-dress-b1/*`
- Modify: `scripts/run_qingshan_dress_b1.py`

- [ ] **Step 1: Write failing read-only/manifest/acceptance tests**

Require marker `GAMEXXK_QINGSHAN_B1_VALIDATION`, no save/delete calls in the validator, exact protected hashes, exact camera transforms/FOV, 26 PCG buildings, 6 archetypes, 4 live roof material palettes, 72 PCG props, 30 flipbook plus four nonempty static-frame groups totalling 70 PCG-card plants, 24 PCG mountains, category collision policy, bridge/river separation, three width/material records, stable road-edge labels/digest, every graph's consumed edge digest, Landscape resolution/layer/sample elevations/building ground error, explicit PCG/component seeds, and a live scene-manifest SHA-256. Require the host runner to reject outer MCP failure, missing marker JSON, inner `success=false`, timeout, and stale capture files with a nonzero exit code.

- [ ] **Step 2: Run and verify RED**

```powershell
python -m unittest scripts.test_qingshan_dress_b1_scripts -v
```

Expected: validator and acceptance scripts missing.

- [ ] **Step 3: Implement read-only live manifest validation**

Build a canonical manifest from live managed actor transforms, PCG graph paths/base and component seeds/point counts/generated component and instance counts/consumed road-edge labels and digest, actual mesh/material/collision paths, QuickRoad source width/material records/triangles/intersection patches/stable edge labels and digest, four camera transforms/FOV, bridge/river actors, Paper2D assets/phases/cull distances, Landscape topology/reconstructed grid centre/edit-layer state/height digest and decoded sample elevations, Actor/Component/Tick counts, and protected raw hashes. Serialize sorted compact JSON and hash it. Do not use the contract hash as the live digest.

- [ ] **Step 4: Implement two-run acceptance and performance evidence**

The host runner runs setup/infrastructure/finalize_begin plus bounded finalize_poll calls twice, then invokes the read-only validator and requires identical live scene-manifest hashes. It requests one named CameraActor capture per MCP call at 1920 x 1080, waits for fresh files, starts PIE, samples frame/stat data through separate calls while the editor ticks for five seconds, then stops PIE. Record editor memory, road triangles, PCG instances, Actor/Component/Tick counts, and frame-time/stat evidence only under the B1 evidence directory. Every step parses a unique marker JSON and requires both outer MCP success and inner `success=true`; otherwise the host process exits nonzero.

- [ ] **Step 5: Run full pre-gate verification**

```powershell
python -m unittest scripts.test_qingshan_dress_b1_config scripts.test_qingshan_b1_heightmap scripts.test_qingshan_b1_proxy_kit scripts.test_quickroad_automation_facade scripts.test_qingshan_dress_b1_assets scripts.test_qingshan_dress_b1_scripts -v
python scripts/run_qingshan_dress_b1.py --acceptance
python scripts/harness_state_validator.py
python scripts/gamexxk_real_play_flow_mcp.py
git status --short
```

Expected: all host tests and live acceptance pass, two manifest hashes match, four captures exist, protected hashes match, harness state is valid, and the existing MVP real-play flow still succeeds.

- [ ] **Step 6: Commit evidence and stop at the user visual gate**

```powershell
git add -- Content/Python/gamexxk_validate_qingshan_dress_b1.py Content/Python/gamexxk_qingshan_dress_b1_acceptance.py scripts/run_qingshan_dress_b1.py scripts/test_qingshan_dress_b1_scripts.py docs/production/evidence/qingshan-pcg-dress-b1
git commit -m "test: verify qingshan B1 review gate"
```

Present all four captures and the measured concerns to the user. Do not run Task 7 until the user accepts this gate.

### Task 7: After visual approval, bake QuickRoad and record production status

**Files:**
- Modify: `Content/Python/gamexxk_assemble_qingshan_dress_b1.py`
- Modify: `Content/Python/gamexxk_validate_qingshan_dress_b1.py`
- Modify: `scripts/test_qingshan_dress_b1_scripts.py`
- Create: `docs/production/qingshan-pcg-dress-b1.md`
- Create via UE: `/Game/GameXXK/Environment/TownPCG/B1/QuickRoadBake/*`
- Modify via UE: `/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1`

- [ ] **Step 1: Write failing bake persistence/collision tests**

Require explicit `bake` phase, one consolidated source network, baked blueprint package save, chunk package saves, B1 map save after source deletion/spawn, at least one simple collision on each chunk, and validator status `verified_baked` with source-network-to-chunk evidence.

- [ ] **Step 2: Run and verify RED**

```powershell
python -m unittest scripts.test_qingshan_dress_b1_scripts -v
```

Expected: no bake phase/persistence checks.

- [ ] **Step 3: Implement bake and explicit persistence**

Call `BakeSingleRoadNetwork` with `/Game/GameXXK/Environment/TownPCG/B1/QuickRoadBake`, split 5000 cm, delete source true. Load every created StaticMesh, add simple box/convex collision when missing, disable Nanite below 50,000 triangles, enable it only at or above that threshold, preserve one fallback LOD on each non-Nanite chunk, and record the decision per chunk. Save every mesh and the baked blueprint package, then save the current B1 map. Audit must prove the procedural source network is absent, the baked blueprint is placed, every chunk exists and has simple collision, and the intersection patch was retained.

- [ ] **Step 4: Run final verification and production record**

```powershell
python -m unittest scripts.test_qingshan_dress_b1_config scripts.test_qingshan_b1_heightmap scripts.test_qingshan_b1_proxy_kit scripts.test_quickroad_automation_facade scripts.test_qingshan_dress_b1_assets scripts.test_qingshan_dress_b1_scripts -v
python -c "import json,sys; sys.path.insert(0,'scripts'); from ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(timeout=60); assert c.connect(); print(json.dumps(c.run_project_python_file('Content/Python/gamexxk_assemble_qingshan_dress_b1.py',['--phase','bake']),ensure_ascii=False))"
python -c "import json,sys; sys.path.insert(0,'scripts'); from ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(timeout=60); assert c.connect(); print(json.dumps(c.run_project_python_file('Content/Python/gamexxk_validate_qingshan_dress_b1.py'),ensure_ascii=False))"
python scripts/harness_state_validator.py
python scripts/gamexxk_real_play_flow_mcp.py
git status --short
```

Record exact map/asset paths, counts, live manifest hash, protected hashes, Landscape samples/layer digest, PCG graph/output counts, QuickRoad source widths/triangles/edges/bake chunks, Paper2D counts/cull distances, performance evidence, captures, and deferred formal-asset replacements in `docs/production/qingshan-pcg-dress-b1.md`.

- [ ] **Step 5: Commit only B1 bake and production files**

```powershell
git add -- Content/Python/gamexxk_assemble_qingshan_dress_b1.py Content/Python/gamexxk_validate_qingshan_dress_b1.py scripts/test_qingshan_dress_b1_scripts.py Content/GameXXK/Environment/TownPCG/B1/QuickRoadBake Content/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1.umap docs/production/qingshan-pcg-dress-b1.md
git commit -m "feat: bake qingshan B1 roads"
```

## Plan Self-Review

- Real PCG: buildings, repeated props, far plant cards, and mountains are generated by UE PCG graphs/components; Python only authors deterministic inputs and unique semantic actors.
- Real QuickRoad: the facade generates three width groups, consolidates intersections, clears/applies one edit layer, extracts road edges, and audits measured plugin output.
- Terrain: a deterministic 16-bit broad heightmap carries B0R terraces, south/dock descent, river trench, and low-frequency relief before road influence.
- Determinism: every infrastructure run clears the named road edit layer; the two-run gate hashes live PCG, QuickRoad, Landscape, cameras, direct actors, and source hashes.
- Safety: each mutating phase proves B1 current map; source/B0R raw hashes detect clean-but-saved mutations; deletion is tag/root/label constrained.
- Visual direction: six actual proxy meshes, four roof palettes, warm paper windows, separate props, real Paper2D animation, river/bridge separation, and mountain ring are represented.
- Performance: PCG uses instanced static meshes, only 30 Paper2D plants animate, QuickRoad remains editor-time, and measured Actor/Component/Tick/frame/memory evidence is required.
- Workflow: root `main` is used because project rules forbid worktrees; C++ uses cold UBT; the process stops at the requested four-camera user gate before baking.
