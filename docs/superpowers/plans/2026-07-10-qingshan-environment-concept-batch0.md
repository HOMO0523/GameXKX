# Qingshan Environment Concept Batch 0 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and validate the complete 35-JSON Qingshan environment concept catalog, preserve the approved style references, and generate the four Batch 0 style-lock boards for user review.

**Architecture:** Keep all project-bound concept sources under `SourceAssets/TownPCG/QingshanEnvironment`, with a host-safe Python validator as the contract boundary. Generate the 35 concrete asset JSON files from one deterministic catalog builder, register imagegen outputs through a hash-recording CLI, and use UE 5.8 MCP only for a read-only measurement of the existing L-A shop. Batch 0 outputs remain unapproved and all Tripo, Sprite, and UE-import gates remain false.

**Tech Stack:** Python 3 standard library, `unittest`, JSON Schema Draft 2020-12 as a documented schema, PowerShell binary copy/hash verification, UE 5.8 MCP via `scripts/ue_mcp_client.py`, built-in `image_gen` with local style references, Git on `main` without worktrees.

---

## Scope boundary

This plan implements only the catalog foundation and Batch 0. Batch 1, Batch 2, Batch 3, Tripo, Paper2D cutting, Unreal imports, Landscape edits, and PCG changes require later plans after the Batch 0 user approval gate.

Do not modify or stage `Config/DefaultEditor.ini`. Do not save any Unreal map. Do not use UnrealBridge, Live Coding, or Hot Reload.

## File map

- `scripts/qingshan_environment_assets.py`: validate catalog, register generated outputs, and write the Batch 0 review record.
- `scripts/build_qingshan_environment_catalog.py`: deterministic source definitions for all 35 JSON records.
- `scripts/qingshan_environment_source_probe.py`: host coordinator for the read-only UE MCP source-mesh probe.
- `scripts/test_qingshan_environment_assets.py`: host behavioral tests for schema, batches, budgets, plants, registration, and report writing.
- `scripts/test_qingshan_environment_source_probe.py`: host/source-contract tests for the UE probe.
- `Content/Python/gamexxk_probe_qingshan_environment_sources.py`: read-only L-A bounds/material-slot probe executed inside UE.
- `SourceAssets/TownPCG/QingshanEnvironment/schema/qingshan_environment_asset.schema.json`: documented JSON contract.
- `SourceAssets/TownPCG/QingshanEnvironment/manifest.json`: exact 35-ID catalog and batch budgets.
- `SourceAssets/TownPCG/QingshanEnvironment/source_metrics.json`: measured L-A source facts.
- `SourceAssets/TownPCG/QingshanEnvironment/style/QS_InkToon_v1.json`: shared art-style rules.
- `SourceAssets/TownPCG/QingshanEnvironment/style/references/*`: stable copies of seven user-provided references.
- `SourceAssets/TownPCG/QingshanEnvironment/style/references/provenance.json`: source path, role, byte count, and SHA-256 for each reference.
- `SourceAssets/TownPCG/QingshanEnvironment/style/boards/*.png`: four generated Batch 0 boards.
- `SourceAssets/TownPCG/QingshanEnvironment/assets/<ASSET_ID>/asset.json`: 35 concrete JSON records.
- `docs/production/qingshan-environment-concept-batch0.md`: generated evidence and pending-review record.

### Task 1: Add the host catalog contract and validator

**Files:**
- Create: `scripts/test_qingshan_environment_assets.py`
- Create: `scripts/qingshan_environment_assets.py`
- Create: `SourceAssets/TownPCG/QingshanEnvironment/schema/qingshan_environment_asset.schema.json`

- [ ] **Step 1: Write failing validator tests**

Create tests that import the future module, construct one concrete valid building fixture, and verify these failures independently:

```python
from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from qingshan_environment_assets import CatalogError, validate_asset, validate_catalog


def valid_building() -> dict:
    return {
        "schema_version": 1,
        "asset_id": "BLD_QS_M_A_INN",
        "display_name": "青山镇中型客栈 A",
        "category": "building",
        "batch": "B1",
        "priority": "P0",
        "status": "concept_planned",
        "style_profile": "QS_InkToon_v1",
        "intended_zone": ["town_interior"],
        "gameplay_role": "street_node",
        "target_dimensions_m": [4.8, 5.6, 5.2],
        "pivot": "bottom_center",
        "forward_axis": "+Y",
        "palette": ["warm_off_white", "warm_red_brown", "dark_teal_ink"],
        "silhouette_keywords": ["two_storey", "asymmetric_xieshan_roof"],
        "material_language": ["ink_toon", "warm_plaster", "dark_timber"],
        "negative_prompt": ["photorealistic", "modern_signage", "mirror_symmetry"],
        "dependencies": ["REF_QS_ENV_STYLE_LOCK", "REF_QS_SCALE_LINEUP"],
        "source_provenance": {"source_kind": "project_concept"},
        "reference_images": [
            {"kind": "hero_3q", "approval_state": "planned", "file_stub": "BLD_QS_M_A_INN__hero_3q"},
            {"kind": "structure_sheet", "approval_state": "planned", "file_stub": "BLD_QS_M_A_INN__structure_sheet"},
        ],
        "generation": {
            "use_case": "stylized-concept",
            "asset_type": "game_environment_asset_reference",
            "required_view_kinds": ["hero_3q", "structure_sheet"],
            "max_versions_per_view": 3,
            "max_generation_calls": 6,
            "generation_calls_used": 0,
            "blocked_after_revisions": 2,
        },
        "unreal": {"asset_class": "StaticMesh", "nanite": True, "material_slots_max": 2},
        "pcg": {"allowed_zones": ["town_interior"], "excluded_zones": ["road", "river", "interaction"]},
        "workflow_gates": {
            "style_locked": False,
            "reference_approved": False,
            "concept_generation_allowed": False,
            "batch_unlocked": False,
            "previous_batch_approved": False,
            "batch_approval_id": None,
            "model_or_sprite_production_allowed": False,
            "tripo_allowed": False,
            "ue_import_allowed": False,
        },
        "acceptance": {"silhouette_unique": False, "player_camera_readability": False},
    }


class AssetValidationTests(unittest.TestCase):
    def test_valid_building_passes(self):
        validate_asset(valid_building())

    def test_empty_value_is_rejected(self):
        data = valid_building()
        data["palette"] = []
        with self.assertRaisesRegex(CatalogError, "empty"):
            validate_asset(data)

    def test_required_views_are_exact(self):
        data = valid_building()
        data["generation"]["required_view_kinds"].append("random_extra")
        with self.assertRaisesRegex(CatalogError, "required_view_kinds"):
            validate_asset(data)

    def test_v004_budget_is_impossible(self):
        data = valid_building()
        data["generation"]["max_versions_per_view"] = 4
        with self.assertRaisesRegex(CatalogError, "max_versions_per_view"):
            validate_asset(data)
```

- [ ] **Step 2: Run the focused test and confirm red state**

Run:

```powershell
python -m unittest scripts.test_qingshan_environment_assets -v
```

Expected: import failure for `qingshan_environment_assets`.

- [ ] **Step 3: Implement the minimal validator**

Implement these exact public interfaces:

```python
class CatalogError(ValueError):
    pass


EXPECTED_VIEWS = {
    "reference": ("board",),
    "registry": ("existing_asset_registry",),
    "building": ("hero_3q", "structure_sheet"),
    "landmark": ("route_3q", "structure_connection_sheet"),
    "near_mountain": ("module_lineup", "assembly_player_scale"),
    "far_mountain": ("silhouette_layers", "player_camera_composite"),
    "rock_kit": ("variant_lineup", "silhouette_scale"),
    "paper2d_plant": ("neutral_variants", "silhouette_scale_cluster", "wind_poses"),
    "surface": ("seamless_transition_sheet", "player_camera_material_sheet"),
    "linear_kit": ("variant_lineup", "usage_scale_sheet"),
    "prop_kit": ("variant_lineup", "usage_scale_sheet"),
}

BATCH_COUNTS = {"REGISTRY": 1, "B0": 4, "B1": 12, "B2": 13, "B3": 5}
BATCH_CALL_CAPS = {"REGISTRY": 0, "B0": 12, "B1": 48, "B2": 52, "B3": 20}


def validate_asset(data: dict) -> None:
    required = (
        "schema_version", "asset_id", "display_name", "category", "batch", "priority",
        "status", "style_profile", "intended_zone", "gameplay_role", "target_dimensions_m",
        "pivot", "forward_axis", "palette", "silhouette_keywords", "material_language",
        "negative_prompt", "dependencies", "source_provenance", "reference_images",
        "generation", "unreal", "pcg", "workflow_gates", "acceptance",
    )
    missing = [key for key in required if key not in data]
    if missing:
        raise CatalogError(f"missing fields: {missing}")
    for key, value in data.items():
        if value == "" or value == [] or value == {}:
            raise CatalogError(f"empty field: {key}")
    expected = EXPECTED_VIEWS[data["category"]]
    actual = tuple(data["generation"]["required_view_kinds"])
    if actual != expected:
        raise CatalogError(f"required_view_kinds must be {expected}, got {actual}")
    if data["generation"]["max_versions_per_view"] != 3:
        raise CatalogError("max_versions_per_view must be 3")
    if data["generation"]["generation_calls_used"] > data["generation"]["max_generation_calls"]:
        raise CatalogError("generation call budget exceeded")
    gates = data["workflow_gates"]
    if gates["tripo_allowed"] or gates["model_or_sprite_production_allowed"] or gates["ue_import_allowed"]:
        raise CatalogError("production gates must remain false in Batch 0")


def validate_catalog(root: Path) -> dict:
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    ids = manifest["asset_ids"]
    if len(ids) != 35 or len(set(ids)) != 35:
        raise CatalogError("catalog must contain 35 unique asset ids")
    assets = []
    for asset_id in ids:
        path = root / "assets" / asset_id / "asset.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        validate_asset(data)
        assets.append(data)
    counts = {batch: sum(item["batch"] == batch for item in assets) for batch in BATCH_COUNTS}
    if counts != BATCH_COUNTS:
        raise CatalogError(f"batch counts mismatch: {counts}")
    plant_cells = sum(item.get("paper2d", {}).get("atlas_cell_count", 0) for item in assets)
    if plant_cells != 40:
        raise CatalogError(f"Paper2D plant atlas must total 40 cells, got {plant_cells}")
    return {"ok": True, "asset_count": len(assets), "batch_counts": counts, "plant_cells": plant_cells}
```

Add this CLI shape; the called functions perform validation before writing:

```python
def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    actions = parser.add_mutually_exclusive_group(required=True)
    actions.add_argument("--check", action="store_true")
    actions.add_argument("--register-output", nargs=4, metavar=("ASSET_ID", "VIEW_KIND", "VERSION", "FILE"))
    actions.add_argument("--write-batch-report", nargs=2, metavar=("BATCH", "OUTPUT"))
    args = parser.parse_args()
    if args.check:
        print(json.dumps(validate_catalog(args.root), ensure_ascii=False, sort_keys=True))
        return 0
    if args.register_output:
        asset_id, view_kind, version, file_name = args.register_output
        print(json.dumps(register_output(args.root, asset_id, view_kind, version, Path(file_name)), ensure_ascii=False, sort_keys=True))
        return 0
    batch, output = args.write_batch_report
    write_batch_report(args.root, batch, Path(output))
    return 0
```

`register_output` must compute SHA-256, accept only `v001|v002|v003`, increment `generation_calls_used`, and update only the matching `reference_images` entry. `write_batch_report("B0")` must derive the Markdown from JSON rather than accept manually supplied hashes.

- [ ] **Step 4: Add the JSON Schema document**

Use Draft 2020-12, require the same core fields, set `additionalProperties: false` at the top level, constrain `asset_id` to `^(REF|BLD|LMK|ENV|P2D|SRF|KIT|PROP)_QS_[A-Z0-9_]+$`, constrain `batch` to `REGISTRY|B0|B1|B2|B3`, and constrain `generation.max_versions_per_view` with `const: 3`. Define nested `generation` and `workflow_gates` objects with their required keys and `additionalProperties: false`. The Python validator remains authoritative because no new `jsonschema` dependency is introduced.

- [ ] **Step 5: Run tests and commit**

Run:

```powershell
python -m unittest scripts.test_qingshan_environment_assets -v
python scripts/qingshan_environment_assets.py --help
```

Expected: all tests pass; CLI help lists the four modes.

Commit:

```powershell
git add -- scripts/qingshan_environment_assets.py scripts/test_qingshan_environment_assets.py SourceAssets/TownPCG/QingshanEnvironment/schema/qingshan_environment_asset.schema.json
git commit -m "feat: add qingshan concept catalog contract"
```

### Task 2: Preserve the seven style references with provenance

**Files:**
- Modify: `scripts/test_qingshan_environment_assets.py`
- Create: `SourceAssets/TownPCG/QingshanEnvironment/style/references/style_env_day.jpeg`
- Create: `SourceAssets/TownPCG/QingshanEnvironment/style/references/style_env_night.jpeg`
- Create: `SourceAssets/TownPCG/QingshanEnvironment/style/references/style_character_scale.jpeg`
- Create: `SourceAssets/TownPCG/QingshanEnvironment/style/references/style_nature_group.jpeg`
- Create: `SourceAssets/TownPCG/QingshanEnvironment/style/references/style_creature_warm.jpeg`
- Create: `SourceAssets/TownPCG/QingshanEnvironment/style/references/style_creature_mass.jpeg`
- Create: `SourceAssets/TownPCG/QingshanEnvironment/style/references/layout_dense_foliage.jpg`
- Create: `SourceAssets/TownPCG/QingshanEnvironment/style/references/provenance.json`

- [ ] **Step 1: Add a failing provenance test**

Assert all seven copied files exist, their byte counts and SHA-256 values match the table below, and only `layout_dense_foliage.jpg` has role `layout_density_only`.

| Destination | Bytes | SHA-256 | Role |
|---|---:|---|---|
| `style_env_day.jpeg` | 160447 | `bdc4c4e76938dfc416b37ad5d32c710d56604cf344a1346189b152b245bb15bf` | `environment_style` |
| `style_env_night.jpeg` | 164803 | `9947131a990cfcae6def4f82d26bd392b9900b042e40cd0f9b7b5cae05bde57b` | `environment_atmosphere` |
| `style_character_scale.jpeg` | 24557 | `ed509274c438f2d7a81f5ec7cb35e9186dba730f33299f158efdf0b40f5c537d` | `character_scale` |
| `style_nature_group.jpeg` | 48689 | `c6aa7189710f32c59c4842529a9ff47601698e2666cf5a0683dd1b74a6878b3b` | `shape_language` |
| `style_creature_warm.jpeg` | 73115 | `c371fc141a614afc54789c0cb65c1603106ed02c9ff374d5a0c18de3b51ef0d9` | `warm_palette` |
| `style_creature_mass.jpeg` | 40962 | `0b38e1901489220b3d82bb6aff983ec2fbb49bac8a29ca7c0db050ce16df6a7d` | `mass_and_outline` |
| `layout_dense_foliage.jpg` | 229157 | `e29852562f1002ac6f8436cd260bf7fc1f8e3d64f663f9004279a75c8d73ec93` | `layout_density_only` |

Use this exact source mapping:

| Destination | Source |
|---|---|
| `style_env_day.jpeg` | `<TENCENT_ORIGINALS>/74a507717280d3a9e8f075e8a3d8201b.jpeg` |
| `style_env_night.jpeg` | `<TENCENT_ORIGINALS>/6cb6304fa60ec4084affc48be722555e.jpeg` |
| `style_character_scale.jpeg` | `<TENCENT_ORIGINALS>/b3e9c4c5f18ab38abfa6f49336b623c0.jpeg` |
| `style_nature_group.jpeg` | `<TENCENT_ORIGINALS>/b470ad572a2d2eb3959754c13d6cf0f1.jpeg` |
| `style_creature_warm.jpeg` | `<TENCENT_ORIGINALS>/d848c66bef90df2589b2b81c7575cd22.jpeg` |
| `style_creature_mass.jpeg` | `<TENCENT_ORIGINALS>/55bc66504b926d1917317b93c0a42f6b.jpeg` |
| `layout_dense_foliage.jpg` | `<WECHAT_TEMP>/ed2a4aca8dd0aac3628ef65c3b405348.jpg` |

- [ ] **Step 2: Confirm the test fails because stable copies are absent**

Run:

```powershell
python -m unittest scripts.test_qingshan_environment_assets.AssetValidationTests.test_reference_provenance -v
```

Expected: FAIL with a missing destination file.

- [ ] **Step 3: Copy the binary references without altering them**

Resolve the seven logical source locators outside tracked files, then use `Copy-Item -LiteralPath` for the resolved local paths. Do not resize, recompress, color-correct, or rename the source files. Write `provenance.json` with destination name, logical source locator, role, byte count, and the exact SHA-256 above; do not persist local user paths or account identifiers.

- [ ] **Step 4: Re-run the focused test and commit**

Run:

```powershell
python -m unittest scripts.test_qingshan_environment_assets.AssetValidationTests.test_reference_provenance -v
```

Expected: PASS, seven files verified byte-for-byte.

Commit only the reference copies, provenance, and test:

```powershell
git add -- SourceAssets/TownPCG/QingshanEnvironment/style/references scripts/test_qingshan_environment_assets.py
git commit -m "assets: preserve qingshan style references"
```

### Task 3: Measure the existing L-A source through read-only UE MCP

**Files:**
- Create: `Content/Python/gamexxk_probe_qingshan_environment_sources.py`
- Create: `scripts/qingshan_environment_source_probe.py`
- Create: `scripts/test_qingshan_environment_source_probe.py`
- Create: `SourceAssets/TownPCG/QingshanEnvironment/source_metrics.json`

- [ ] **Step 1: Write source-contract and parser tests**

The tests must verify that the UE script contains the exact shop path, calls `get_bounds`, prints one JSON object, never calls a save API, and that the host parser rejects non-positive bounds.

```python
SHOP = "/Game/GameXXK/Environment/TownPCG/Prototype/QingshanShopA/SM_Qingshan_Shop_A_HQ_Retop50K"

def test_probe_is_read_only(self):
    source = UE_PROBE.read_text(encoding="utf-8")
    self.assertIn(SHOP, source)
    self.assertIn("get_bounds", source)
    self.assertNotIn("save_", source.lower())
    self.assertNotIn("set_editor_property", source)

def test_parse_rejects_zero_bounds(self):
    with self.assertRaisesRegex(ValueError, "positive"):
        parse_probe({"ok": True, "bounds_size_m": [0, 1, 1]})
```

- [ ] **Step 2: Run the tests and confirm red state**

Run:

```powershell
python -m unittest scripts.test_qingshan_environment_source_probe -v
```

Expected: missing probe/coordinator imports or files.

- [ ] **Step 3: Implement the UE probe**

Use this read-only structure:

```python
from __future__ import annotations

import json
import unreal

SHOP = "/Game/GameXXK/Environment/TownPCG/Prototype/QingshanShopA/SM_Qingshan_Shop_A_HQ_Retop50K"


def main() -> dict:
    mesh = unreal.load_asset(SHOP)
    if mesh is None:
        raise RuntimeError(f"missing mesh: {SHOP}")
    bounds = mesh.get_bounds()
    extent = bounds.box_extent
    size_m = [float(extent.x) * 0.02, float(extent.y) * 0.02, float(extent.z) * 0.02]
    return {
        "ok": all(value > 0 for value in size_m),
        "asset_path": SHOP,
        "bounds_size_m": size_m,
        "material_slot_count": len(mesh.get_editor_property("static_materials")),
    }


if __name__ == "__main__":
    print(json.dumps(main(), ensure_ascii=False, sort_keys=True))
```

The host coordinator must call `UnrealMCPClient.run_project_python_file`, parse captured stdout with `gamexxk_content_assembly_check.parse_stdout_json`, validate positive finite bounds, and write the returned object plus `captured_at`, editor endpoint, and probe path to `source_metrics.json`.

- [ ] **Step 4: Run host tests**

Run:

```powershell
python -m unittest scripts.test_qingshan_environment_source_probe -v
```

Expected: all tests pass.

- [ ] **Step 5: Execute the read-only MCP probe**

Run:

```powershell
python scripts/qingshan_environment_source_probe.py
```

Expected: `ok=true`, three positive finite `bounds_size_m` values, material slot count 1, and no dirty-package save. If MCP is unavailable, stop; do not restart or close the editor and do not invent dimensions.

- [ ] **Step 6: Commit the probe and measured result**

```powershell
git add -- Content/Python/gamexxk_probe_qingshan_environment_sources.py scripts/qingshan_environment_source_probe.py scripts/test_qingshan_environment_source_probe.py SourceAssets/TownPCG/QingshanEnvironment/source_metrics.json
git commit -m "feat: record qingshan environment source metrics"
```

### Task 4: Generate the deterministic 35-JSON catalog

**Files:**
- Create: `scripts/build_qingshan_environment_catalog.py`
- Modify: `scripts/test_qingshan_environment_assets.py`
- Create: `SourceAssets/TownPCG/QingshanEnvironment/manifest.json`
- Create: `SourceAssets/TownPCG/QingshanEnvironment/style/QS_InkToon_v1.json`
- Create: `SourceAssets/TownPCG/QingshanEnvironment/assets/*/asset.json`

- [ ] **Step 1: Add failing full-catalog tests**

Add tests for exact counts `REGISTRY=1, B0=4, B1=12, B2=13, B3=5`, 35 unique IDs, batch caps `0/12/48/52/20`, all production gates false, `REGISTRY` calls 0, exact category views, and plant cells 40.

- [ ] **Step 2: Confirm red state**

Run:

```powershell
python -m unittest scripts.test_qingshan_environment_assets -v
```

Expected: full-catalog tests fail because manifest/assets are absent.

- [ ] **Step 3: Implement the builder with the complete ID table**

The builder must contain this exact ordered catalog:

```python
CATALOG = {
    "REGISTRY": ["BLD_QS_L_A_MARKET_SHOP"],
    "B0": [
        "REF_QS_ENV_STYLE_LOCK", "REF_QS_SCALE_LINEUP",
        "REF_QS_CAMERA_ROUTE", "REF_QS_SURFACE_PALETTE",
    ],
    "B1": [
        "BLD_QS_M_A_INN", "BLD_QS_S_A_HOUSE", "LMK_QS_GATE_NORTH",
        "LMK_QS_BRIDGE_MAIN", "LMK_QS_DOCK_SOUTH", "ENV_QS_MTN_NEAR_A_RIDGE",
        "P2D_QS_MTN_FAR_A_MIST", "ENV_QS_ROCK_KIT_A_FIELD",
        "P2D_QS_TREE_PINE_A", "P2D_QS_SHRUB_A",
        "SRF_QS_GROUND_EARTH_A", "SRF_QS_WATER_RIVER_A",
    ],
    "B2": [
        "BLD_QS_M_B_STREET_SHOP", "BLD_QS_S_B_WORKSHOP", "BLD_QS_S_C_RIVER_HUT",
        "ENV_QS_MTN_NEAR_B_CLIFF", "P2D_QS_MTN_FAR_B_SILHOUETTE",
        "ENV_QS_ROCK_KIT_B_RIVERBANK", "P2D_QS_TREE_BROADLEAF_A",
        "P2D_QS_TREE_BAMBOO_A", "P2D_QS_GRASS_TUFT_A", "P2D_QS_FLOWER_WILD_A",
        "SRF_QS_GROUND_GRASS_A", "SRF_QS_ROAD_STONE_A", "SRF_QS_RIVERBANK_BLEND_A",
    ],
    "B3": [
        "KIT_QS_FENCE_WOOD_A", "KIT_QS_PATH_STEPPING_STONE_A",
        "PROP_QS_MARKET_KIT_A", "PROP_QS_DOCK_KIT_A", "PROP_QS_STREET_KIT_A",
    ],
}
```

Use this concrete detail table. `source_metrics.bounds_size_m` means read the measured numeric array before writing the registry JSON; it is not emitted as a string.

| Asset ID | Category | target_dimensions_m | Zone / role | Primary request |
|---|---|---|---|---|
| `BLD_QS_L_A_MARKET_SHOP` | registry | source metrics | market / landmark shop | Register the existing approved largest shop without new generation |
| `REF_QS_ENV_STYLE_LOCK` | reference | `[160,100,50]` | global / style board | Qingshan ink-cartoon environment style board |
| `REF_QS_SCALE_LINEUP` | reference | `[60,10,20]` | global / scale board | Character, buildings, vegetation, bridge and mountain scale lineup |
| `REF_QS_CAMERA_ROUTE` | reference | `[160,100,40]` | global / route board | Four fixed-camera views from gate to market, bridge and dock |
| `REF_QS_SURFACE_PALETTE` | reference | `[8,8,1]` | global / surface board | Earth, grass, road, bank, water and rock palette relationship |
| `BLD_QS_M_A_INN` | building | `[4.8,5.6,5.2]` | town interior / street node | Two-storey inn with warm red-brown asymmetric xieshan roof |
| `BLD_QS_S_A_HOUSE` | building | `[3.6,4.2,3.8]` | courtyard / small house | Compact warm-white house with indigo double-slope roof |
| `LMK_QS_GATE_NORTH` | landmark | `[24,6,12]` | north gate / fixed anchor | Asymmetric Chinese town gate preserving an 18–24m valley opening |
| `LMK_QS_BRIDGE_MAIN` | landmark | `[9,24,5]` | bridge / traversal | Nine-metre-wide timber-and-stone main bridge with clear deck |
| `LMK_QS_DOCK_SOUTH` | landmark | `[30,18,2]` | south bank / traversal | Modular dock platform, posts, steps and railing without boats |
| `ENV_QS_MTN_NEAR_A_RIDGE` | near_mountain | `[60,24,18]` | perimeter / solid ridge | Long low near-mountain ridge with two joinable variants |
| `P2D_QS_MTN_FAR_A_MIST` | far_mountain | `[120,0.1,45]` | far backdrop / mist layer | Pale layered mist-mountain band for the first depth layer |
| `ENV_QS_ROCK_KIT_A_FIELD` | rock_kit | `[2.5,2,1.8]` | perimeter/yard / rock kit | S/M/L dry field-rock trio with distinct silhouettes |
| `P2D_QS_TREE_PINE_A` | paper2d_plant | `[4,0.05,7]` | perimeter / tree | Three pine silhouettes; only A receives five wind poses |
| `P2D_QS_SHRUB_A` | paper2d_plant | `[2,0.05,1.5]` | yard/road edge / shrub | Three shrub silhouettes; only A receives five wind poses |
| `SRF_QS_GROUND_EARTH_A` | surface | `[4,4,0.02]` | town interior / ground | Seamless compact earth with ink-toon transition edges |
| `SRF_QS_WATER_RIVER_A` | surface | `[8,8,0.02]` | river / water | Blue-green toon river with flow, shallow/deep bands and foam |
| `BLD_QS_M_B_STREET_SHOP` | building | `[4.2,5,4.6]` | town interior / street shop | One-and-a-half-storey shop with ink-green deep eaves |
| `BLD_QS_S_B_WORKSHOP` | building | `[4,4.8,3.6]` | side lane / workshop | Ochre hard-gable workshop with one attached side shed |
| `BLD_QS_S_C_RIVER_HUT` | building | `[3.8,4.5,3.5]` | riverbank / cottage | Teal low-eave riverside cottage on a compact footprint |
| `ENV_QS_MTN_NEAR_B_CLIFF` | near_mountain | `[40,20,22]` | perimeter / cliff turn | Joinable near-cliff corner pair with broken ink silhouettes |
| `P2D_QS_MTN_FAR_B_SILHOUETTE` | far_mountain | `[120,0.1,60]` | far backdrop / silhouette | Deeper blue-gray far-peak silhouette layer |
| `ENV_QS_ROCK_KIT_B_RIVERBANK` | rock_kit | `[2.8,2.2,1.5]` | riverbank / wet rock kit | S/M/L damp river rocks with moss and flattened bases |
| `P2D_QS_TREE_BROADLEAF_A` | paper2d_plant | `[5,0.05,6]` | yard/perimeter / tree | Three broadleaf silhouettes; only A receives five wind poses |
| `P2D_QS_TREE_BAMBOO_A` | paper2d_plant | `[4,0.05,6]` | perimeter/yard / bamboo | Three bamboo-clump silhouettes; only A receives five wind poses |
| `P2D_QS_GRASS_TUFT_A` | paper2d_plant | `[0.8,0.05,0.8]` | ground / grass | Three low grass tufts; only A receives four wind poses |
| `P2D_QS_FLOWER_WILD_A` | paper2d_plant | `[0.7,0.05,0.9]` | yard/riverbank / flower | Three restrained wildflower clumps; only A receives four wind poses |
| `SRF_QS_GROUND_GRASS_A` | surface | `[4,4,0.02]` | perimeter / ground | Sparse low-saturation grass ground, not individual blades |
| `SRF_QS_ROAD_STONE_A` | surface | `[4,4,0.02]` | road / surface | Worn stone-and-earth road compatible with QuickRoad geometry |
| `SRF_QS_RIVERBANK_BLEND_A` | surface | `[4,4,0.02]` | riverbank / transition | Wet mud, gravel and moss transition family |
| `KIT_QS_FENCE_WOOD_A` | linear_kit | `[3,0.25,1.5]` | yard / fence | Straight, corner and gate wooden-fence trio |
| `KIT_QS_PATH_STEPPING_STONE_A` | linear_kit | `[3,1,0.2]` | yard/path / stepping stones | Straight, curved and scattered stepping-stone trio |
| `PROP_QS_MARKET_KIT_A` | prop_kit | `[3,2,2.8]` | market / props | Stall frame, sign frame and basket trio |
| `PROP_QS_DOCK_KIT_A` | prop_kit | `[2.5,2,1.8]` | dock / props | Mooring post, rope coil and crate trio |
| `PROP_QS_STREET_KIT_A` | prop_kit | `[2,1,3]` | street / props | Lantern frame, bench and road-sign frame trio |

Apply these nonempty shared defaults:

```python
COMMON_NEGATIVE = [
    "photorealistic", "modern object", "readable text", "logo", "watermark",
    "neon green", "mirror symmetry", "cropped subject", "PBR plastic shine",
]
COMMON_MATERIAL = ["QS_InkToon_v1", "two_to_three_value_bands", "dry_brush_edges"]
COMMON_PALETTE = ["warm_off_white", "dark_teal_ink", "jade_green", "blue_gray", "warm_ochre"]
```

Category builders must supply the exact `required_view_kinds` and budgets from Task 1, variant caps from the approved design, meaningful nonempty zones and dependencies, and false production gates. For the six plant families write exactly three variants `A/B/C`, `animated_variants=["A"]`, and atlas cells `7,7,7,7,6,6` in the table order, totaling 40. The builder must refuse missing source metrics/provenance, sort JSON keys, write UTF-8 with two-space indentation, and produce no empty list/object/string.

The builder CLI must support a normal write mode with no arguments and `--check-no-rewrite`. Check mode renders every expected JSON in memory, compares it byte-for-byte with the committed files, prints `{"ok": true, "unchanged": 37}` for manifest + style profile + 35 assets, and exits nonzero on any drift. Add a unit test that changes one generated field in a temporary catalog and verifies check mode reports drift without rewriting the file.

- [ ] **Step 4: Write the complete Batch 0 prompts into the four REF JSONs**

Each REF has `required_view_kinds=["board"]`, `max_generation_calls=3`, `batch_unlocked=true`, `concept_generation_allowed=true`, all downstream production gates false, and one of the exact prompts in Tasks 5–8.

- [ ] **Step 5: Build, validate, and commit**

Run:

```powershell
python scripts/build_qingshan_environment_catalog.py
python scripts/qingshan_environment_assets.py --root SourceAssets/TownPCG/QingshanEnvironment --check
python -m unittest scripts.test_qingshan_environment_assets scripts.test_qingshan_environment_source_probe -v
```

Expected JSON summary:

```json
{"ok": true, "asset_count": 35, "batch_counts": {"REGISTRY": 1, "B0": 4, "B1": 12, "B2": 13, "B3": 5}, "plant_cells": 40}
```

Commit:

```powershell
git add -- scripts/build_qingshan_environment_catalog.py scripts/test_qingshan_environment_assets.py SourceAssets/TownPCG/QingshanEnvironment/manifest.json SourceAssets/TownPCG/QingshanEnvironment/style/QS_InkToon_v1.json SourceAssets/TownPCG/QingshanEnvironment/assets
git commit -m "assets: add qingshan environment concept catalog"
```

### Task 5: Generate `REF_QS_ENV_STYLE_LOCK`

**Files:**
- Create: `SourceAssets/TownPCG/QingshanEnvironment/style/boards/REF_QS_ENV_STYLE_LOCK__board__v001.png`
- Modify: `SourceAssets/TownPCG/QingshanEnvironment/assets/REF_QS_ENV_STYLE_LOCK/asset.json`

- [ ] **Step 1: Generate one board with built-in imagegen**

Use all references except `layout_dense_foliage.jpg`; label them as style, palette, mass, and scale references. Use this prompt verbatim from the JSON:

```text
Use case: stylized-concept
Asset type: game environment style-lock board
Primary request: define the approved visual language for Qingshan town, a Chinese ink-cartoon town enclosed by layered mountains
Input images: environment day and night images are atmosphere and brushwork references; character and creature images are proportion, outline, shape-language, and palette references; do not copy their subjects
Scene/backdrop: warm rice-paper board with a compact town vignette, small material swatches, roof-color families, foliage shapes, mountain layers, river and stone-road samples; no written labels
Subject: warm off-white walls, dark timber, roofs in warm red-brown, ink green, indigo, ochre and teal; jade and blue-green foliage; blue-gray mist mountains; dark teal variable ink outlines
Style/medium: hand-drawn Chinese ink cartoon concept art, dry-brush broken edges, simplified chunky silhouettes, two to three value bands, restrained watercolor wash
Composition/framing: wide coherent style board, central environment vignette with orderly visual samples around it, generous margins
Lighting/mood: calm bright morning with a small cool mist sample, friendly adventure tone
Constraints: no text, no symbols, no logos, no watermark, no photorealism, no PBR shine, no neon green, no modern objects, no mirror symmetry
```

- [ ] **Step 2: Save and register the project-bound output**

Read the absolute file path returned by the built-in tool, copy that exact file to the destination above, and run:

```powershell
python scripts/qingshan_environment_assets.py --root SourceAssets/TownPCG/QingshanEnvironment --register-output REF_QS_ENV_STYLE_LOCK board v001 SourceAssets/TownPCG/QingshanEnvironment/style/boards/REF_QS_ENV_STYLE_LOCK__board__v001.png
python scripts/qingshan_environment_assets.py --root SourceAssets/TownPCG/QingshanEnvironment --check
```

For a targeted revision, the same registration command must also record the successful
generation trace. Repeat `--generation-input-path` once for every image directly supplied
to imagegen, repeat `--generation-reference-lineage` for the complete stable-reference
lineage, and pass the full base prompt plus revision directive with `--generation-prompt`.
All three trace options are supplied together. Planned dependencies are never recorded as
direct inputs unless they were actually passed to the successful generation call. Every
successful Batch 0 board registration uses these trace options, including first versions.
The current built-in imagegen runtime accepts at most five direct tool inputs; the lineage
is not limited to five and currently covers six stable references. A six-path request that
the runtime rejects before producing an image is not recorded as a successful generation.

Expected: calls used 1/3, `approval_state=generated_pending_review`, stored SHA-256, all production gates false.

- [ ] **Step 3: Commit**

```powershell
git add -- SourceAssets/TownPCG/QingshanEnvironment/style/boards/REF_QS_ENV_STYLE_LOCK__board__v001.png SourceAssets/TownPCG/QingshanEnvironment/assets/REF_QS_ENV_STYLE_LOCK/asset.json
git commit -m "assets: generate qingshan environment style board"
```

#### Final v003 style-board decision

User review found v002 still too fragmented for later single-object production. The final
allowed style-board revision is therefore v003; it must use the reusable `detail_budget`
contract, emphasize larger masses and more rice-paper space, and becomes the generated
style-lock input for Tasks 6–8 and every non-B0 concept. v001 and v002 remain preserved as
revision evidence. v003 exhausts this view's 3/3 budget, and v004 remains forbidden.

The v003 direct tool inputs are exactly v002 plus the character-scale, nature-group,
warm-creature and mass-creature references. Its complete lineage contains the six stable
style references plus v001 and v002. The actual trace stores the original Task 5 prompt
followed by the approved final simplification directive.

### Task 6: Generate `REF_QS_SCALE_LINEUP`

**Files:**
- Create: `SourceAssets/TownPCG/QingshanEnvironment/style/boards/REF_QS_SCALE_LINEUP__board__v001.png`
- Modify: `SourceAssets/TownPCG/QingshanEnvironment/assets/REF_QS_SCALE_LINEUP/asset.json`

- [ ] **Step 1: Generate the scale board**

Use `style_env_day.jpeg`, `style_character_scale.jpeg`, `style_nature_group.jpeg`, and the approved-looking v001 style board as references. Prompt:

```text
Use case: stylized-concept
Asset type: game environment scale-lineup board
Primary request: show Qingshan town gameplay assets at one consistent scale for a fixed high three-quarter camera
Input images: project environment and character images define ink-cartoon style and hero scale; the Qingshan style-lock board defines palette and materials
Scene/backdrop: plain warm rice-paper background with a single horizontal ground baseline, no full scene
Subject: one small project hero, L/M/S Chinese town-building silhouettes, north gate, nine-meter bridge section, dock section, pine, broadleaf tree, bamboo, shrub and near mountain-foot module, all clearly proportional
Style/medium: the exact QS_InkToon_v1 hand-drawn ink-cartoon language
Composition/framing: wide lineup, every subject fully visible and separated, high three-quarter asset view, no cropping
Lighting/mood: neutral soft daylight for shape comparison
Constraints: no labels, no numbers, no text, no watermark, no duplicated subjects, no perspective distortion, no modern objects
Detail budget: Every object must read at 128px; use at most 3 primary masses and 5 secondary internal accent strokes or stroke groups; use 2 main value bands plus 1 shadow accent; forbid individual_roof_tiles, repeated_window_lattices, leaf_by_leaf_foliage, stone_or_pebble_tessellation, tiny_prop_piles; every board sample follows its matching building, bridge, plant, mountain, rock, or surface rule.
```

- [ ] **Step 2: Copy, register, validate, and commit**

Read the absolute file path returned by imagegen, copy that file to `SourceAssets/TownPCG/QingshanEnvironment/style/boards/REF_QS_SCALE_LINEUP__board__v001.png`, then run:

```powershell
python scripts/qingshan_environment_assets.py --root SourceAssets/TownPCG/QingshanEnvironment --register-output REF_QS_SCALE_LINEUP board v001 SourceAssets/TownPCG/QingshanEnvironment/style/boards/REF_QS_SCALE_LINEUP__board__v001.png
python scripts/qingshan_environment_assets.py --root SourceAssets/TownPCG/QingshanEnvironment --check
git add -- SourceAssets/TownPCG/QingshanEnvironment/style/boards/REF_QS_SCALE_LINEUP__board__v001.png SourceAssets/TownPCG/QingshanEnvironment/assets/REF_QS_SCALE_LINEUP/asset.json
git commit -m "assets: generate qingshan environment scale board"
```

Expected: calls used 1/3 and `approval_state=generated_pending_review`.


### Task 7: Generate `REF_QS_CAMERA_ROUTE`

**Files:**
- Create: `SourceAssets/TownPCG/QingshanEnvironment/style/boards/REF_QS_CAMERA_ROUTE__board__v001.png`
- Modify: `SourceAssets/TownPCG/QingshanEnvironment/assets/REF_QS_CAMERA_ROUTE/asset.json`

- [ ] **Step 1: Generate the four-route-view board**

Use `style_env_day.jpeg`, `style_env_night.jpeg`, `layout_dense_foliage.jpg` as layout-density-only, `style_character_scale.jpeg`, and the generated style board. Prompt:

```text
Use case: stylized-concept
Asset type: four-panel game environment camera-route concept board
Primary request: show the Qingshan town player route in four consistent fixed-camera views: north gate arrival, market and largest shop, main bridge, south dock
Input images: project ink illustrations define style; the foliage screenshot defines only dense overlapping canopy rhythm, never its rendering style; the style-lock board defines final palette and materials
Scene/backdrop: one coherent mountain-enclosed Chinese town across four equal visual panels without borders or text
Subject: north gate remains on screen right; a readable horizontal gentle S-shaped road runs right to left through an asymmetric market, offset bridge, river and left-side dock; one large shop, smaller staggered houses, clustered foliage, near solid mountain feet and pale Paper2D-like far mountain silhouettes
Style/medium: QS_InkToon_v1 hand-drawn Chinese ink cartoon environment concept
Composition/framing: fixed high three-quarter camera matching the game; four route moments share scale, geography and asset identity; player path remains clear
Lighting/mood: fresh morning, calm mist, readable gameplay contrast
Constraints: no text, no UI, no labels, no watermark, no straight building rows, no mirror symmetry, no fluorescent foliage, no mountain wall blocking the lower center, no modern objects
Detail budget: Every object must read at 128px; use at most 3 primary masses and 5 secondary internal accent strokes or stroke groups; use 2 main value bands plus 1 shadow accent; forbid individual_roof_tiles, repeated_window_lattices, leaf_by_leaf_foliage, stone_or_pebble_tessellation, tiny_prop_piles; every board sample follows its matching building, bridge, plant, mountain, rock, or surface rule.
```

- [ ] **Step 2: Copy, register, validate, and commit**

Read the absolute file path returned by imagegen, copy that file to `SourceAssets/TownPCG/QingshanEnvironment/style/boards/REF_QS_CAMERA_ROUTE__board__v001.png`, then run:

```powershell
python scripts/qingshan_environment_assets.py --root SourceAssets/TownPCG/QingshanEnvironment --register-output REF_QS_CAMERA_ROUTE board v001 SourceAssets/TownPCG/QingshanEnvironment/style/boards/REF_QS_CAMERA_ROUTE__board__v001.png
python scripts/qingshan_environment_assets.py --root SourceAssets/TownPCG/QingshanEnvironment --check
git add -- SourceAssets/TownPCG/QingshanEnvironment/style/boards/REF_QS_CAMERA_ROUTE__board__v001.png SourceAssets/TownPCG/QingshanEnvironment/assets/REF_QS_CAMERA_ROUTE/asset.json
git commit -m "assets: generate qingshan camera route board"
```

Expected: calls used 1/3 and `approval_state=generated_pending_review`.


### Task 8: Generate `REF_QS_SURFACE_PALETTE`

**Files:**
- Create: `SourceAssets/TownPCG/QingshanEnvironment/style/boards/REF_QS_SURFACE_PALETTE__board__v001.png`
- Modify: `SourceAssets/TownPCG/QingshanEnvironment/assets/REF_QS_SURFACE_PALETTE/asset.json`

- [ ] **Step 1: Generate the surface relationship board**

Use the two environment references, the warm palette reference, and the generated style board. Prompt:

```text
Use case: stylized-concept
Asset type: game environment surface-palette board
Primary request: define the coordinated ground and water surfaces for Qingshan town
Input images: project environment references define ink wash and atmosphere; warm creature reference defines restrained ochre accents; the style-lock board defines final palette
Scene/backdrop: warm rice-paper presentation board, no written labels
Subject: adjacent samples of compact earth, sparse grass ground, worn stone-and-dirt road, wet muddy riverbank, blue-green shallow river water, mossy field rock and dry mountain rock; include two clean transition strips and one small player-camera terrain vignette
Style/medium: QS_InkToon_v1 hand-painted environment material concept, large readable shapes, dry-brush edges, two to three tonal bands
Composition/framing: wide organized material board with separated but visually connected samples, no perspective-heavy hero object
Lighting/mood: neutral overcast-soft light for color comparison
Constraints: no text, no labels, no watermark, no photoreal PBR spheres, no shiny plastic, no neon colors, no obvious repeating photo texture
Detail budget: Every object must read at 128px; use at most 3 primary masses and 5 secondary internal accent strokes or stroke groups; use 2 main value bands plus 1 shadow accent; forbid individual_roof_tiles, repeated_window_lattices, leaf_by_leaf_foliage, stone_or_pebble_tessellation, tiny_prop_piles; surface uses broad flat swatches without cobble tessellation, pebble noise, or other micro-texture.
```

- [ ] **Step 2: Copy, register, validate, and commit**

Read the absolute file path returned by imagegen, copy that file to `SourceAssets/TownPCG/QingshanEnvironment/style/boards/REF_QS_SURFACE_PALETTE__board__v001.png`, then run:

```powershell
python scripts/qingshan_environment_assets.py --root SourceAssets/TownPCG/QingshanEnvironment --register-output REF_QS_SURFACE_PALETTE board v001 SourceAssets/TownPCG/QingshanEnvironment/style/boards/REF_QS_SURFACE_PALETTE__board__v001.png
python scripts/qingshan_environment_assets.py --root SourceAssets/TownPCG/QingshanEnvironment --check
git add -- SourceAssets/TownPCG/QingshanEnvironment/style/boards/REF_QS_SURFACE_PALETTE__board__v001.png SourceAssets/TownPCG/QingshanEnvironment/assets/REF_QS_SURFACE_PALETTE/asset.json
git commit -m "assets: generate qingshan surface palette board"
```

Expected: calls used 1/3 and `approval_state=generated_pending_review`.


### Task 9: Produce the Batch 0 review record and final verification

**Files:**
- Create: `docs/production/qingshan-environment-concept-batch0.md`
- Modify: `SourceAssets/TownPCG/QingshanEnvironment/manifest.json`

- [ ] **Step 1: Add report-writing tests**

The test must register four temporary image files, call `write_batch_report`, and assert the Markdown includes all four asset IDs, exact paths, SHA-256 values, `generated_pending_review`, `calls used 1/3`, and explicit false gates for Tripo/Sprite/UE.

- [ ] **Step 2: Run the focused test red then green**

Before implementing report writing, run the test and confirm it fails. Implement `write_batch_report` in `scripts/qingshan_environment_assets.py`, then rerun:

```powershell
python -m unittest scripts.test_qingshan_environment_assets -v
```

Expected: all tests pass.

- [ ] **Step 3: Generate the real report from registered JSON**

Run:

```powershell
python scripts/qingshan_environment_assets.py --root SourceAssets/TownPCG/QingshanEnvironment --write-batch-report B0 docs/production/qingshan-environment-concept-batch0.md
```

Expected: four rows, each generated and pending user review; B1 remains locked.

- [ ] **Step 4: Run complete fresh verification**

Run:

```powershell
python scripts/build_qingshan_environment_catalog.py --check-no-rewrite
python scripts/qingshan_environment_assets.py --root SourceAssets/TownPCG/QingshanEnvironment --check
python -m unittest scripts.test_qingshan_environment_assets scripts.test_qingshan_environment_source_probe -v
git diff --check
git status --short
```

Expected:

- 35 unique JSON records.
- Batch counts `1/4/12/13/5`.
- Plant cell total 40.
- Four Batch 0 image files with valid nonzero SHA-256 and one call used each.
- B0 status `generated_pending_review`; B1/B2/B3 locked.
- All Tripo, model/Sprite production, and UE-import gates false.
- Only the pre-existing user-owned `Config/DefaultEditor.ini` may remain modified outside this task.

- [ ] **Step 5: Commit the review record**

```powershell
git add -- scripts/qingshan_environment_assets.py scripts/test_qingshan_environment_assets.py docs/production/qingshan-environment-concept-batch0.md SourceAssets/TownPCG/QingshanEnvironment/manifest.json
git commit -m "docs: record qingshan concept batch zero"
```

- [ ] **Step 6: Present all four boards for user approval**

Show the four project-local images and ask one decision: approve Batch 0 as a set or name the single board that needs a targeted revision. Do not unlock Batch 1 automatically.
