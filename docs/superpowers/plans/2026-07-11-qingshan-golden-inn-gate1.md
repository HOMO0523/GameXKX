# Qingshan Golden Inn Gate 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build, render, and automatically validate the first Blender-authored golden building `BLD_QS_M_A_INN` so the user can perform Gate 1 visual approval once.

**Architecture:** The deterministic environment catalog remains the source of production constraints. A pure-Python module converts the inn's `asset.json` golden contract into a validated component plan; a Blender-only script consumes that plan and writes the `.blend`, three fixed renders, a 128 px silhouette, and a geometry report. A host runner discovers Blender 4.2+, executes it headlessly, verifies every output and hash, then writes a Gate 1 evidence record without touching the protected UE map or user-tuned assets.

**Tech Stack:** Python 3 `unittest`, Blender 4.2.3 LTS Python API (`bpy`, `bmesh`, `mathutils`), deterministic JSON, Eevee/Freestyle preview rendering, Git on project `main`.

---

## Scope and file map

This plan implements Gate 1 only. It does not create the remaining four building families, import to UE, edit PCG, author the UE BSDF material, or approve the golden asset.

**Create:**

- `scripts/qingshan_golden_inn.py` — Blender-independent contract loader, component-plan generator, output-path policy, and report verifier.
- `scripts/test_qingshan_golden_inn.py` — host-side contract and verification tests.
- `scripts/blender/build_qingshan_golden_inn.py` — Blender scene construction, materials, cameras, rendering, audit, and save.
- `scripts/run_qingshan_golden_inn.py` — Blender discovery, headless execution, and host verification entry point.
- `SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_M_A_INN/source/blender/` — generated `.blend`, fixed renders, silhouette, and report.
- `docs/production/qingshan-golden-inn-gate1.md` — Gate 1 evidence and review state.

**Modify:**

- `scripts/build_qingshan_environment_catalog.py` — declare Blender as the building pipeline and emit the inn golden contract.
- `scripts/test_qingshan_environment_assets.py` — replace obsolete Tripo assertions and lock the golden contract.
- `SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_M_A_INN/asset.json` — regenerated deterministically by the catalog builder.
- `SourceAssets/TownPCG/QingshanEnvironment/style/QS_InkToon_v1.json` — regenerated model-pipeline summary from the catalog builder.
- `SourceAssets/TownPCG/QingshanEnvironment/manifest.json` and other catalog outputs only if the deterministic builder reports required drift.

**Must not modify:**

- `Config/DefaultEditor.ini`.
- `docs/superpowers/specs/2026-07-11-qingshan-pcg-whitebox-b0r-design.md`.
- `/Game/GameXXK/Maps/L_QingshanInn`, character/PaperZD assets, placed cameras, or any manually tuned HD2D planes.

### Task 1: Lock the Blender golden-building catalog contract

**Files:**

- Modify: `scripts/test_qingshan_environment_assets.py`
- Modify: `scripts/build_qingshan_environment_catalog.py`
- Regenerate: `SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_M_A_INN/asset.json`
- Regenerate: `SourceAssets/TownPCG/QingshanEnvironment/style/QS_InkToon_v1.json`

- [ ] **Step 1: Replace the obsolete Tripo assertion with a failing golden-contract test**

In `test_buildings_use_measured_largest_anchor_and_distinct_staggered_families`, replace the source assertion with:

```python
expected_source = (
    "Blender_procedural_golden"
    if asset_id == "BLD_QS_M_A_INN"
    else "Blender_modular_derivative"
)
self.assertEqual(asset["building"]["model_pipeline"]["source"], expected_source)
```

Add this focused test to `DeterministicCatalogBuilderTests`:

```python
def test_golden_inn_declares_buildable_visual_contract(self):
    with tempfile.TemporaryDirectory() as directory:
        _root, _builder, _summary, assets = self._build(directory)
        inn = assets["BLD_QS_M_A_INN"]
        golden = inn["building"]["golden_contract"]

        self.assertEqual(golden["workflow"], "golden_sample_gate1")
        self.assertEqual(golden["blender_min_version"], [4, 2, 0])
        self.assertEqual(golden["deformation_percent_range"], [5, 10])
        self.assertEqual(golden["column_lean_degrees_range"], [4, 7])
        self.assertEqual(golden["window_frame_ratio_range"], [0.08, 0.12])
        self.assertEqual(golden["max_window_divisions"], 3)
        self.assertEqual(golden["max_capital_layers"], 2)
        self.assertEqual(golden["ground_contact_plane_z_m"], 0.0)
        self.assertEqual(golden["required_render_views"], ["hero_3q", "front", "player_camera"])
        self.assertEqual(
            golden["output_root"],
            "assets/BLD_QS_M_A_INN/source/blender",
        )
```

- [ ] **Step 2: Run the focused test and verify it fails**

Run:

```powershell
python -m unittest scripts.test_qingshan_environment_assets.DeterministicCatalogBuilderTests.test_golden_inn_declares_buildable_visual_contract -v
```

Expected: `ERROR` or `FAIL` because `golden_contract` does not exist and the source is still `Tripo_high_precision`.

- [ ] **Step 3: Emit the building source and golden contract from the deterministic builder**

In the building branch of `_category_fields`, replace the current `model_pipeline` source/material fields with:

```python
model_source = (
    "Blender_procedural_golden"
    if asset_id == "BLD_QS_M_A_INN"
    else "Blender_modular_derivative"
)
building_fields = {
    "model_pipeline": {
        "approved_quad_face_range": approved_range,
        "material_stage": "ue_after_gate1_geometry_approval",
        "source": model_source,
        "target_quad_faces": target_quad_faces,
        "topology": "quad_dominant",
    },
    "scale_anchor": "BLD_QS_L_A_MARKET_SHOP",
}
if asset_id == "BLD_QS_M_A_INN":
    building_fields["golden_contract"] = {
        "blender_min_version": [4, 2, 0],
        "column_lean_degrees_range": [4, 7],
        "deformation_percent_range": [5, 10],
        "ground_contact_plane_z_m": 0.0,
        "max_capital_layers": 2,
        "max_window_divisions": 3,
        "output_root": "assets/BLD_QS_M_A_INN/source/blender",
        "required_render_views": ["hero_3q", "front", "player_camera"],
        "window_frame_ratio_range": [0.08, 0.12],
        "workflow": "golden_sample_gate1",
    }
```

Use `"building": building_fields` in the returned fields. Update the existing building assertions to require `topology == "quad_dominant"` and `material_stage == "ue_after_gate1_geometry_approval"`. In the style-profile builder change:

```python
"building_source": "Blender_procedural_modular",
"material_stage": "ue_after_geometry_gate_approval",
```

Do not alter landmark, near-mountain, or rock-kit source rules in this task.

- [ ] **Step 4: Run focused and catalog tests**

Run:

```powershell
python -m unittest scripts.test_qingshan_environment_assets.DeterministicCatalogBuilderTests.test_golden_inn_declares_buildable_visual_contract scripts.test_qingshan_environment_assets.DeterministicCatalogBuilderTests.test_buildings_use_measured_largest_anchor_and_distinct_staggered_families -v
```

Expected: both tests pass.

- [ ] **Step 5: Regenerate and validate the deterministic catalog**

Run:

```powershell
python scripts/build_qingshan_environment_catalog.py
python scripts/build_qingshan_environment_catalog.py --check-no-rewrite
python scripts/qingshan_environment_assets.py --root SourceAssets/TownPCG/QingshanEnvironment --check
```

Expected: builder write succeeds, check reports no drift, and catalog validation succeeds.

- [ ] **Step 6: Commit the contract change**

```powershell
git add scripts/build_qingshan_environment_catalog.py scripts/test_qingshan_environment_assets.py SourceAssets/TownPCG/QingshanEnvironment
git commit -m "feat: define qingshan Blender golden building contract"
```

### Task 2: Add the host-side component-plan contract

**Files:**

- Create: `scripts/qingshan_golden_inn.py`
- Create: `scripts/test_qingshan_golden_inn.py`

- [ ] **Step 1: Write failing tests for contract loading and the large-shape plan**

Create `scripts/test_qingshan_golden_inn.py` with:

```python
from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from qingshan_golden_inn import (  # noqa: E402
    GoldenInnError,
    build_component_plan,
    load_golden_contract,
    output_paths,
    verify_gate1_report,
)


class GoldenInnContractTests(unittest.TestCase):
    def setUp(self):
        self.asset_path = (
            PROJECT_ROOT
            / "SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_M_A_INN/asset.json"
        )

    def test_component_plan_uses_chunky_asymmetric_major_forms(self):
        contract = load_golden_contract(self.asset_path)
        plan = build_component_plan(contract)

        self.assertEqual(plan["asset_id"], "BLD_QS_M_A_INN")
        self.assertEqual(plan["ground_z_m"], 0.0)
        self.assertGreaterEqual(plan["asymmetry"], 0.05)
        self.assertLessEqual(plan["asymmetry"], 0.10)
        self.assertEqual(plan["window_divisions"], 2)
        self.assertGreaterEqual(plan["window_frame_ratio"], 0.08)
        self.assertLessEqual(plan["window_frame_ratio"], 0.12)
        self.assertEqual(plan["capital_layers"], 1)
        self.assertEqual(plan["modules"], ["BODY", "ROOF_A", "DOOR_A", "WINDOW_A", "EAVE_A"])
        self.assertEqual(len(plan["columns"]), 4)
        self.assertNotEqual(plan["columns"][0]["lean_deg"], plan["columns"][1]["lean_deg"])

    def test_output_paths_are_versioned_and_inside_asset_source(self):
        paths = output_paths(PROJECT_ROOT, "v001")
        root = PROJECT_ROOT / "SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_M_A_INN/source/blender"
        self.assertEqual(paths["blend"], root / "BLD_QS_M_A_INN__golden__v001.blend")
        self.assertEqual(paths["hero_3q"], root / "BLD_QS_M_A_INN__hero_3q__v001.png")
        self.assertEqual(paths["front"], root / "BLD_QS_M_A_INN__front__v001.png")
        self.assertEqual(paths["player_camera"], root / "BLD_QS_M_A_INN__player_camera__v001.png")
        self.assertEqual(paths["silhouette_128"], root / "BLD_QS_M_A_INN__silhouette_128__v001.png")
        self.assertEqual(paths["report"], root / "BLD_QS_M_A_INN__gate1_report__v001.json")

    def test_invalid_contract_is_rejected(self):
        data = json.loads(self.asset_path.read_text(encoding="utf-8"))
        data["building"]["golden_contract"]["max_window_divisions"] = 8
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "asset.json"
            path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(GoldenInnError, "max_window_divisions"):
                load_golden_contract(path)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the tests and verify import failure**

Run:

```powershell
python -m unittest scripts.test_qingshan_golden_inn -v
```

Expected: `ModuleNotFoundError: No module named 'qingshan_golden_inn'`.

- [ ] **Step 3: Implement deterministic contract loading and component-plan generation**

Create `scripts/qingshan_golden_inn.py`. It must:

```python
ASSET_ID = "BLD_QS_M_A_INN"
VERSIONS = ("v001", "v002", "v003")
REQUIRED_VIEWS = ("hero_3q", "front", "player_camera")

class GoldenInnError(ValueError):
    pass

def load_golden_contract(path: Path) -> dict:
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("asset_id") != ASSET_ID:
        raise GoldenInnError("asset_id must be BLD_QS_M_A_INN")
    golden = data.get("building", {}).get("golden_contract")
    if not isinstance(golden, dict):
        raise GoldenInnError("building.golden_contract is required")
    if golden.get("max_window_divisions") not in (2, 3):
        raise GoldenInnError("max_window_divisions must be 2 or 3")
    if golden.get("max_capital_layers") not in (1, 2):
        raise GoldenInnError("max_capital_layers must be 1 or 2")
    deformation = golden.get("deformation_percent_range")
    if deformation != [5, 10]:
        raise GoldenInnError("deformation_percent_range must be [5, 10]")
    dimensions = data.get("target_dimensions_m")
    if dimensions != [4.8, 5.6, 5.2]:
        raise GoldenInnError("target_dimensions_m drifted")
    return {"asset": data, "golden": golden}

def build_component_plan(contract: dict) -> dict:
    golden = contract["golden"]
    return {
        "asset_id": ASSET_ID,
        "dimensions_m": contract["asset"]["target_dimensions_m"],
        "ground_z_m": golden["ground_contact_plane_z_m"],
        "asymmetry": 0.075,
        "facade_offset_m": 0.16,
        "roof_ridge_offset_m": -0.22,
        "window_frame_ratio": 0.10,
        "window_divisions": 2,
        "capital_layers": 1,
        "modules": ["BODY", "ROOF_A", "DOOR_A", "WINDOW_A", "EAVE_A"],
        "columns": [
            {"name": "Column_FL", "x_m": -2.0, "y_m": 2.45, "lean_deg": -5.5},
            {"name": "Column_FR", "x_m": 1.86, "y_m": 2.45, "lean_deg": 4.5},
            {"name": "Column_BL", "x_m": -2.0, "y_m": -2.4, "lean_deg": -4.0},
            {"name": "Column_BR", "x_m": 1.86, "y_m": -2.4, "lean_deg": 6.0},
        ],
    }
```

Also implement `output_paths(project_root, version)` with strict version validation and `verify_gate1_report(report_path, paths)` that rejects missing outputs, a nonzero ground minimum, more than two material slots per module, more than two capital layers, or a report whose render views differ from `REQUIRED_VIEWS`.

- [ ] **Step 4: Run the host-side tests**

Run:

```powershell
python -m unittest scripts.test_qingshan_golden_inn -v
```

Expected: all tests pass.

- [ ] **Step 5: Commit the host contract**

```powershell
git add scripts/qingshan_golden_inn.py scripts/test_qingshan_golden_inn.py
git commit -m "feat: add qingshan golden inn component contract"
```

### Task 3: Build the deterministic Blender scene

**Files:**

- Create: `scripts/blender/build_qingshan_golden_inn.py`
- Modify: `scripts/test_qingshan_golden_inn.py`

- [ ] **Step 1: Add a failing Blender-script structure test**

Append:

```python
def test_blender_builder_has_required_entry_points(self):
    script = PROJECT_ROOT / "scripts/blender/build_qingshan_golden_inn.py"
    text = script.read_text(encoding="utf-8")
    for function_name in (
        "create_tapered_box",
        "create_beam_between",
        "create_curved_roof",
        "create_chunky_window",
        "create_single_capital",
        "create_fixed_cameras",
        "audit_scene",
        "main",
    ):
        self.assertIn(f"def {function_name}(", text)
```

- [ ] **Step 2: Run the test and verify missing file failure**

Run:

```powershell
python -m unittest scripts.test_qingshan_golden_inn.GoldenInnContractTests.test_blender_builder_has_required_entry_points -v
```

Expected: `FileNotFoundError`.

- [ ] **Step 3: Implement the Blender builder with large, closed components**

Create `scripts/blender/build_qingshan_golden_inn.py` with these required behaviors:

```python
def create_tapered_box(name, bottom_xy, top_xy, z0, z1, material, collection):
    """Create one closed eight-corner wall or base mass with flat z0 contact."""

def create_beam_between(name, start, end, width, depth, material, collection):
    """Create a chunky rectangular beam aligned between two points."""

def create_curved_roof(name, width, depth, eave_z, ridge_z, ridge_offset, material, collection):
    """Create two broad quad-grid roof sheets with unequal upturned eaves and no tile geometry."""

def create_chunky_window(name, center, width, height, frame_ratio, skew, material, collection):
    """Create one mildly trapezoidal opening frame with exactly two broad divisions."""

def create_single_capital(name, center, width, depth, height, material, collection):
    """Create one oversized bracket block; do not stack repeating dougong cubes."""
```

The `main()` scene must create named collections `BODY`, `ROOF_A`, `DOOR_A`, `WINDOW_A`, and `EAVE_A`; four differently leaning main columns; one wide offset double door; three chunky windows; one broad asymmetric roof; uneven large stone-base blocks; one-layer capitals; and no sign, lantern, text, tile, plant, prop, or scenery object.

Use meter units, apply transforms before audit, add small bevel modifiers, and keep every object's origin and module name deterministic. Create four flat preview materials: warm plaster, dark timber, terracotta roof, and gray stone. Each object receives one material, keeping every module at no more than two unique materials.

Create cameras with fixed names and transforms:

```python
CAMERAS = {
    "hero_3q": ((8.4, 9.6, 7.1), (0.0, 0.0, 2.5), 52),
    "front": ((0.0, 11.8, 3.2), (0.0, 0.0, 2.4), 58),
    "player_camera": ((7.2, 10.5, 8.8), (0.0, 0.0, 2.1), 48),
}
```

Render with Eevee at `1536x1024`, ambient occlusion/contact shadows, neutral warm background, no floor geometry, and Freestyle dark outlines. Render the three views, then render `silhouette_128` at `128x128` with a black material override and white background. Save the `.blend` only after all images and the report are written.

`audit_scene()` must record JSON fields:

```python
{
    "asset_id": "BLD_QS_M_A_INN",
    "version": "v001",
    "blender_version": list(bpy.app.version),
    "bounds_m": {"min": [x, y, z], "max": [x, y, z], "size": [x, y, z]},
    "ground_min_z_m": z,
    "object_count": count,
    "mesh_object_count": count,
    "faces": {"triangles": n, "quads": n, "ngons": n},
    "non_manifold_edges": n,
    "negative_scale_objects": [],
    "unapplied_transform_objects": [],
    "module_material_counts": {"BODY": n, "ROOF_A": n, "DOOR_A": n, "WINDOW_A": n, "EAVE_A": n},
    "capital_layers": 1,
    "window_divisions": 2,
    "render_views": ["hero_3q", "front", "player_camera"],
}
```

- [ ] **Step 4: Run the script structure test**

Run:

```powershell
python -m unittest scripts.test_qingshan_golden_inn.GoldenInnContractTests.test_blender_builder_has_required_entry_points -v
```

Expected: pass.

- [ ] **Step 5: Commit the Blender builder**

```powershell
git add scripts/blender/build_qingshan_golden_inn.py scripts/test_qingshan_golden_inn.py
git commit -m "feat: build qingshan golden inn in Blender"
```

### Task 4: Add the headless runner and output verification

**Files:**

- Create: `scripts/run_qingshan_golden_inn.py`
- Modify: `scripts/test_qingshan_golden_inn.py`

- [ ] **Step 1: Add failing runner tests**

Append tests that patch `subprocess.run` and assert the exact command:

```python
[
    str(blender_path),
    "--background",
    "--factory-startup",
    "--python",
    str(builder_script),
    "--",
    "--project-root",
    str(PROJECT_ROOT),
    "--version",
    "v001",
]
```

Also assert discovery prefers environment variable `BLENDER_EXE`, then `D:/Blender/blender.exe`, and rejects Blender versions below 4.2.0.

- [ ] **Step 2: Run the runner tests and verify import failure**

Run:

```powershell
python -m unittest scripts.test_qingshan_golden_inn -v
```

Expected: failure because `run_qingshan_golden_inn.py` is missing.

- [ ] **Step 3: Implement the runner**

Create `scripts/run_qingshan_golden_inn.py` with:

```python
def discover_blender(explicit: Path | None = None) -> Path:
    candidates = [
        explicit,
        Path(os.environ["BLENDER_EXE"]) if os.environ.get("BLENDER_EXE") else None,
        Path("D:/Blender/blender.exe"),
        Path("C:/Program Files/Blender Foundation/Blender 4.2/blender.exe"),
    ]
    for candidate in candidates:
        if candidate is not None and candidate.is_file():
            return candidate.resolve()
    raise GoldenInnError("Blender 4.2+ executable not found")
```

Parse `--blender`, `--version`, and `--check-only`. Normal mode runs Blender with `check=True`, captures the transcript to `Saved/HarnessReports/qingshan-golden-inn-v001-blender.log`, then calls `verify_gate1_report`. `--check-only` performs no generation and verifies the existing report and hashes.

- [ ] **Step 4: Run all host tests**

Run:

```powershell
python -m unittest scripts.test_qingshan_golden_inn -v
```

Expected: all tests pass.

- [ ] **Step 5: Commit the runner**

```powershell
git add scripts/run_qingshan_golden_inn.py scripts/test_qingshan_golden_inn.py
git commit -m "feat: run and verify qingshan golden inn build"
```

### Task 5: Generate and validate the Gate 1 artifact pack

**Files:**

- Create: `SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_M_A_INN/source/blender/BLD_QS_M_A_INN__golden__v001.blend`
- Create: `SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_M_A_INN/source/blender/BLD_QS_M_A_INN__hero_3q__v001.png`
- Create: `SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_M_A_INN/source/blender/BLD_QS_M_A_INN__front__v001.png`
- Create: `SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_M_A_INN/source/blender/BLD_QS_M_A_INN__player_camera__v001.png`
- Create: `SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_M_A_INN/source/blender/BLD_QS_M_A_INN__silhouette_128__v001.png`
- Create: `SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_M_A_INN/source/blender/BLD_QS_M_A_INN__gate1_report__v001.json`

- [ ] **Step 1: Run the real Blender build**

Run:

```powershell
python scripts/run_qingshan_golden_inn.py --blender D:/Blender/blender.exe --version v001
```

Expected: Blender exits `0`; six Gate 1 files exist; host verification reports success.

- [ ] **Step 2: Run check-only verification independently**

Run:

```powershell
python scripts/run_qingshan_golden_inn.py --version v001 --check-only
```

Expected: `success=true`, all files and hashes verified without launching Blender.

- [ ] **Step 3: Inspect the three renders and silhouette**

Use image inspection on all four PNGs. Reject internally and create `v002` only if any of these are true:

- the large silhouette still appears ruler-straight or mirror-symmetric;
- windows have more than three divisions or frames read thin at 128 px;
- capitals visually stack into more than two layers;
- the ground contact is not flat;
- the roof contains modeled individual tiles;
- any sign, lantern, label, text, plant, or scenery leaked into the module.

Do not ask the user to review an internally rejected version.

- [ ] **Step 4: Run the full relevant regression suite**

Run:

```powershell
python -m unittest scripts.test_qingshan_golden_inn scripts.test_qingshan_environment_assets -v
python scripts/build_qingshan_environment_catalog.py --check-no-rewrite
python scripts/harness_state_validator.py
```

Expected: all unit tests pass, catalog has no drift, harness validator exits `0` with `OK`.

- [ ] **Step 5: Commit the verified source artifact pack**

```powershell
git add SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_M_A_INN/source/blender
git commit -m "art: generate qingshan golden inn Gate 1 pack"
```

### Task 6: Record Gate 1 evidence without granting approval

**Files:**

- Create: `docs/production/qingshan-golden-inn-gate1.md`

- [ ] **Step 1: Write the Gate 1 evidence record**

Record:

```markdown
# Qingshan Golden Inn Gate 1 Review

Status: **PENDING_USER_REVIEW**

- Blender: `4.2.3 LTS`
- Asset: `BLD_QS_M_A_INN`
- Version: `v001` or the first internally passing version
- Source `.blend`: linked project-relative path and SHA-256
- Fixed renders: hero 3/4, front, player camera, each linked with SHA-256
- Silhouette: linked 128 px PNG and SHA-256
- Geometry report: linked JSON and SHA-256

## Automated checks

List bounds, ground minimum, object/mesh counts, triangle/quad/ngon totals, non-manifold count, negative/unapplied transform lists, per-module material counts, capital layers, and window divisions.

## Visual self-review

Record pass/fail for controlled large-form asymmetry, chunky doors/windows, single-layer capitals, broad tile-free roof, clean module isolation, and flat ground/module interfaces.

## Gate decision

Do not write `APPROVED` until the user explicitly approves the rendered golden building.
```

- [ ] **Step 2: Verify the evidence links and status**

Run:

```powershell
rg -n "PENDING_USER_REVIEW|BLD_QS_M_A_INN|hero_3q|player_camera|silhouette|SHA-256" docs/production/qingshan-golden-inn-gate1.md
git diff --check
```

Expected: all required fields found; no whitespace errors.

- [ ] **Step 3: Commit the evidence record**

```powershell
git add docs/production/qingshan-golden-inn-gate1.md
git commit -m "docs: record qingshan golden inn Gate 1 review"
```

- [ ] **Step 4: Present the single Gate 1 review package**

Show the hero 3/4, front, and player-camera renders together with only: result, files, major visual rules achieved, any failed self-check, and the next action. Ask the user for one decision: approve the golden building or provide a compact parameter adjustment such as `歪+1`, `窗+1`, `碎-1`, or `夸张+1`.

## Plan self-review result

- Spec coverage: Gate 1 contract, Blender source, controlled asymmetry, chunky openings, simplified capitals, modular separation, three fixed renders, silhouette, geometry audit, communication gate, performance budget source, and protected-file constraints are all mapped to tasks.
- Placeholder scan: no unfinished markers or unspecified test step remains.
- Type consistency: `v001`, output keys, required view names, asset ID, module names, and golden-contract field names are identical across catalog, host module, Blender builder, runner, tests, and evidence record.
