# Asian Village Full Occlusion Material Coverage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Generate safe project-owned occlusion variants for every eligible StaticMesh material used by the Asian Village demo map and resolve them deterministically at runtime.

**Architecture:** An editor-only inventory scans the target map and writes an evidence manifest. Python and C++ share the same `MI_OC_<source-name>_<MD5-prefix>` naming rule, so runtime lookup uses the original full object path without a fragile hand-written table. The authoring pass preserves existing opaque, masked, and translucent behavior while combining the player circle with the appropriate opacity input.

**Tech Stack:** Unreal Engine 5.8 C++, Unreal Python through project UE MCP, MaterialEditingLibrary, JSON evidence, Python unittest, UE Automation Tests, UBT cold builds.

---

## File Structure

- Create `Content/Python/gamexxk_occlusion_material_naming.py`: pure deterministic naming and exclusion rules shared by inventory/authoring scripts.
- Create `Content/Python/gamexxk_inventory_asian_village_occlusion_materials.py`: read-only target-map scanner and manifest writer.
- Create `Content/Python/gamexxk_author_asian_village_occlusion_materials.py`: batch duplicate/reparent/patch/validate authoring pass.
- Create `Content/Python/gamexxk_validate_asian_village_occlusion_materials.py`: UE-side asset and blend-mode validator.
- Create `Config/GameXXK/Occlusion/AsianVillageMaterialInventory.json`: generated source inventory and evidence.
- Create `scripts/test_asian_village_occlusion_material_pipeline.py`: pure source/naming/exclusion contract tests.
- Modify `Source/GameXXK/Private/Town/GameXXKOcclusionMaterialMap.cpp`: deterministic full-path lookup and exclusions.
- Modify `Source/GameXXK/Private/Tests/GameXXKTownShellTest.cpp`: mapping, missing-asset, and pointer-identical restore regression coverage.
- Retire `Content/Python/gamexxk_author_occlusion_cutout_pilot.py` only after the full authoring pass validates; keep it until final migration verification.

### Task 1: Deterministic Naming and Exclusion Contract

**Files:**
- Create: `Content/Python/gamexxk_occlusion_material_naming.py`
- Create: `scripts/test_asian_village_occlusion_material_pipeline.py`

- [ ] **Step 1: Write the failing pure-Python tests**

```python
def test_cutout_name_is_stable_and_path_sensitive():
    a = cutout_asset_name("/Game/Asian_Village/A/MI_same.MI_same")
    b = cutout_asset_name("/Game/Asian_Village/B/MI_same.MI_same")
    assert a == cutout_asset_name("/Game/Asian_Village/A/MI_same.MI_same")
    assert a != b
    assert a.startswith("MI_OC_MI_same_")


def test_exclusions_protect_non_occluding_domains():
    assert is_excluded_source("/Game/Asian_Village/materials/water_materials/M_water.M_water")
    assert is_excluded_source("/Game/Asian_Village/materials/postprocess_materials/M_postprocess.M_postprocess")
    assert not is_excluded_source("/Game/Asian_Village/materials/building_materials/M_building_wood.M_building_wood")
```

- [ ] **Step 2: Run the tests and verify RED**

Run: `python -m unittest scripts.test_asian_village_occlusion_material_pipeline`

Expected: import failure for `gamexxk_occlusion_material_naming`.

- [ ] **Step 3: Implement the naming module**

```python
from hashlib import md5

DESTINATION = "/Game/GameXXK/Materials/Occlusion/AsianVillageFull"
EXCLUDED_SEGMENTS = (
    "/water_materials/",
    "/postprocess_materials/",
    "/sky_materials/",
)


def source_object_name(path: str) -> str:
    return path.rsplit(".", 1)[-1]


def cutout_asset_name(path: str) -> str:
    digest = md5(path.encode("utf-8")).hexdigest()[:8].upper()
    return f"MI_OC_{source_object_name(path)}_{digest}"


def cutout_object_path(path: str) -> str:
    name = cutout_asset_name(path)
    return f"{DESTINATION}/{name}.{name}"


def is_excluded_source(path: str) -> bool:
    lowered = path.lower()
    return not lowered.startswith("/game/asian_village/") or any(
        segment in lowered for segment in EXCLUDED_SEGMENTS
    )
```

- [ ] **Step 4: Run the tests and verify GREEN**

Run: `python -m unittest scripts.test_asian_village_occlusion_material_pipeline`

Expected: all tests pass.

- [ ] **Step 5: Commit**

```text
git add Content/Python/gamexxk_occlusion_material_naming.py scripts/test_asian_village_occlusion_material_pipeline.py
git commit -m "test: define full occlusion material naming contract"
```

### Task 2: Target-Map Material Inventory

**Files:**
- Create: `Content/Python/gamexxk_inventory_asian_village_occlusion_materials.py`
- Create: `Config/GameXXK/Occlusion/AsianVillageMaterialInventory.json`
- Modify: `scripts/test_asian_village_occlusion_material_pipeline.py`

- [ ] **Step 1: Add a failing source contract test**

Assert that the inventory script uses `StaticMeshComponent`, records `source_path`, `blend_mode`, `usage_count`, `excluded`, and writes only under `Config/GameXXK/Occlusion`.

- [ ] **Step 2: Run the test and verify RED**

Run: `python -m unittest scripts.test_asian_village_occlusion_material_pipeline`

Expected: missing inventory script.

- [ ] **Step 3: Implement the read-only inventory**

The script must load the target editor map, enumerate `unreal.StaticMeshComponent`, collect every non-null slot material by full object path, call `get_blend_mode()`, count usages, apply `is_excluded_source`, and write sorted JSON:

```json
{
  "map": "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo",
  "materials": [
    {
      "source_path": "/Game/Asian_Village/.../MI_example.MI_example",
      "target_path": "/Game/GameXXK/Materials/Occlusion/AsianVillageFull/MI_OC_MI_example_12345678.MI_OC_MI_example_12345678",
      "blend_mode": "BLEND_OPAQUE",
      "usage_count": 3,
      "excluded": false
    }
  ]
}
```

- [ ] **Step 4: Run through UE MCP**

Run `run_project_python_file('Content/Python/gamexxk_inventory_asian_village_occlusion_materials.py')` after stopping PIE.

Expected: nonzero material count, no duplicate target paths, excluded water/postprocess count reported separately.

- [ ] **Step 5: Run source tests and commit**

```text
python -m unittest scripts.test_asian_village_occlusion_material_pipeline
git add Content/Python/gamexxk_inventory_asian_village_occlusion_materials.py Config/GameXXK/Occlusion/AsianVillageMaterialInventory.json scripts/test_asian_village_occlusion_material_pipeline.py
git commit -m "feat: inventory Asian Village occlusion materials"
```

### Task 3: Blend-Safe Full Material Authoring

**Files:**
- Create: `Content/Python/gamexxk_author_asian_village_occlusion_materials.py`
- Modify: `scripts/test_asian_village_occlusion_material_pipeline.py`
- Generate: `Content/GameXXK/Materials/Occlusion/AsianVillageFull/*.uasset`

- [ ] **Step 1: Add failing source contract tests**

Tests must require three explicit branches for `BLEND_OPAQUE`, `BLEND_MASKED`, and translucent blend modes; use of `MP_OPACITY_MASK` for opaque/masked; use of `MP_OPACITY` for translucent; and calls to `force_masked_material_compilation`, `recompile_material`, and `save_asset`.

- [ ] **Step 2: Run the test and verify RED**

Run: `python -m unittest scripts.test_asian_village_occlusion_material_pipeline`

Expected: missing full authoring script.

- [ ] **Step 3: Implement family duplication**

For each non-excluded inventory source, recursively duplicate its parent chain into `AsianVillageFull`, reparent copied instances to copied parents, and use the deterministic name for every copied source object. Never write to `/Game/Asian_Village`.

- [ ] **Step 4: Combine the circle with existing opacity**

Build the same screen-space circle expression used by the pilot. Preserve an existing input:

```python
existing = unreal.MaterialEditingLibrary.get_material_property_input_node(material, property_kind)
if existing is None:
    connect_property(circle_keep, property_kind)
else:
    multiply = create(MaterialExpressionMultiply)
    connect(existing, multiply, "A")
    connect(circle_keep, multiply, "B")
    connect_property(multiply, property_kind)
```

Opaque roots switch to `BLEND_MASKED` and use `MP_OPACITY_MASK`. Existing Masked roots remain Masked and multiply their prior mask. Translucent roots preserve their original blend mode and multiply their prior `MP_OPACITY`.

- [ ] **Step 5: Run the authoring pass through UE MCP**

Expected report fields: `eligible_count`, `excluded_count`, `created_count`, `opaque_count`, `masked_count`, `translucent_count`, `failures`. Require `failures=[]` and `created_count == eligible_count plus copied ancestors not already present`.

- [ ] **Step 6: Run tests and commit**

```text
python -m unittest scripts.test_asian_village_occlusion_material_pipeline
git add Content/Python/gamexxk_author_asian_village_occlusion_materials.py Content/GameXXK/Materials/Occlusion/AsianVillageFull scripts/test_asian_village_occlusion_material_pipeline.py
git commit -m "feat: author full Asian Village occlusion materials"
```

### Task 4: Runtime Full-Path Resolution

**Files:**
- Modify: `Source/GameXXK/Private/Town/GameXXKOcclusionMaterialMap.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTownShellTest.cpp`

- [ ] **Step 1: Add failing C++ mapping tests**

Load two mapped materials from different categories, their deterministic targets, one excluded water material, and a transient unmapped material. Assert mapped sources resolve, excluded/unmapped sources return null, and restoration returns the exact original pointers.

- [ ] **Step 2: Cold-build and verify RED**

Save dirty packages through UE MCP, close the editor, then run:

```text
D:\UE_5.8\Engine\Build\BatchFiles\Build.bat GameXXKEditor Win64 Development "D:\UE5 demo\GameXXK\GameXXK.uproject" -WaitMutex -NoHotReload
```

Expected: compile succeeds but new automation assertions fail before resolver implementation.

- [ ] **Step 3: Implement C++ deterministic naming**

Use the original full object path, `FMD5::HashAnsiString`, uppercase first eight hex characters, and `OriginalMaterial->GetName()` to reproduce the Python target name. Reject non-Asian-Village and excluded path segments before `LoadObject<UMaterialInterface>`.

- [ ] **Step 4: Cold-build and run the focused automation test**

Run `GameXXK.MVP.Town.ShellInputInteractionFollower` in a responsive editor. Expected: PASS with mapped, excluded, missing, and pointer-identical restoration assertions.

- [ ] **Step 5: Commit**

```text
git add Source/GameXXK/Private/Town/GameXXKOcclusionMaterialMap.cpp Source/GameXXK/Private/Tests/GameXXKTownShellTest.cpp
git commit -m "feat: resolve occlusion materials by full source path"
```

### Task 5: UE Asset Validation and Pilot Retirement

**Files:**
- Create: `Content/Python/gamexxk_validate_asian_village_occlusion_materials.py`
- Modify: `scripts/test_asian_village_occlusion_material_pipeline.py`
- Delete only after successful validation: `Content/Python/gamexxk_author_occlusion_cutout_pilot.py`

- [ ] **Step 1: Add a failing validator contract test**

Require checks for every eligible manifest target, blend-mode rules, missing assets, duplicate targets, and writes under the project-owned destination only.

- [ ] **Step 2: Implement and run the UE validator**

Expected JSON: `eligible_count > 0`, `missing=[]`, `wrong_blend=[]`, `duplicate_targets=[]`, `foreign_writes=[]`.

- [ ] **Step 3: Verify no runtime dependency uses the pilot folder naming**

Run targeted `rg` across `Source`, `Content/Python`, `scripts`, and `Config`. Remove the pilot script only when no production command references it and the full validator passes.

- [ ] **Step 4: Run tests and commit**

```text
python -m unittest scripts.test_asian_village_occlusion_material_pipeline scripts.test_occlusion_cutout_pilot_assets
git add -A Content/Python scripts Content/GameXXK/Materials/Occlusion/AsianVillageFull
git commit -m "test: validate full occlusion material coverage"
```

### Task 6: PIE Visual and Performance Acceptance

**Files:**
- Create: `docs/production/evidence/asian-village-integration/occlusion-full-coverage/acceptance.json`
- Create screenshots under: `docs/production/evidence/asian-village-integration/occlusion-full-coverage/`

- [ ] **Step 1: Start PIE on the project-owned demo map**

Use UE MCP to open `L_Qingshan_AsianVillage_Demo`, start PIE, ensure Town runtime state, and verify the hero class is `BP_HeroCharacter_C`.

- [ ] **Step 2: Verify five occlusion categories**

Capture a screenshot and probe report for: north-gate columns/beams, multistory building at `(19693.19, 3065.41, 1612.67)`, thatched roof, tree canopy/trunk, and a multi-material stall/sign/fence cluster.

- [ ] **Step 3: Verify restoration and exclusions**

Move the hero to a clear location, confirm modified component count returns to zero after release duration, and verify water/sky/postprocess paths never appear as Cutout materials.

- [ ] **Step 4: Record performance**

Record editor FPS, frame time, simultaneously modified component count, and material switch count at the worst point. Acceptance requires no per-frame asset loading warnings, no missing-material warnings, and no persistent material replacement after PIE stops.

- [ ] **Step 5: Run final verification**

Run pure Python tests, UE validator, focused automation test, and one final UBT cold build. Read all outputs and require zero failures before reporting completion.

- [ ] **Step 6: Commit evidence**

```text
git add docs/production/evidence/asian-village-integration/occlusion-full-coverage
git commit -m "test: accept full Asian Village occlusion coverage"
```
