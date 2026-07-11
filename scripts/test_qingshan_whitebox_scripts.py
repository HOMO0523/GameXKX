import re
import ast
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
IMPLEMENTATION = ROOT / "Source/GameXXKEditor/Private/GameXXKTownPCGAutomationLibrary.cpp"
ASSEMBLER = ROOT / "Content/Python/gamexxk_assemble_qingshan_whitebox_b0r.py"
EXPECTED_ROOTS = {
    "VerticalSliceGraphRoot": "/Game/GameXXK/Environment/TownPCG/VerticalSlice/",
    "B0RGraphRoot": "/Game/GameXXK/Environment/TownPCG/B0R/",
    "PrototypeMapRoot": "/Game/GameXXK/Maps/Prototype/",
    "B0RMapRoot": "/Game/GameXXK/Maps/Dev/",
}
EXPECTED_HELPERS = {
    "IsManagedGraphPath": ("VerticalSliceGraphRoot", "B0RGraphRoot"),
    "IsManagedPrototypeMapPath": ("PrototypeMapRoot", "B0RMapRoot"),
}


def _parse_root_constants(source):
    return dict(
        re.findall(
            r'constexpr TCHAR (\w+)\[\] = TEXT\("([^"]+)"\);',
            source,
        )
    )


def _helper_has_exact_logic(source, helper_name, root_names):
    first_root, second_root = map(re.escape, root_names)
    helper_pattern = re.compile(
        rf"bool {re.escape(helper_name)}\(const FString& Path\)\s*"
        rf"\{{\s*return Path\.StartsWith\({first_root}, ESearchCase::CaseSensitive\)\s*"
        rf"\|\| Path\.StartsWith\({second_root}, ESearchCase::CaseSensitive\);\s*\}}"
    )
    return helper_pattern.search(source) is not None


def _has_safe_root_contract(source):
    constants = _parse_root_constants(source)
    if {name: constants.get(name) for name in EXPECTED_ROOTS} != EXPECTED_ROOTS:
        return False
    return all(
        _helper_has_exact_logic(source, helper_name, root_names)
        for helper_name, root_names in EXPECTED_HELPERS.items()
    )


def _managed_path_is_accepted(source, helper_name, path):
    constants = _parse_root_constants(source)
    return any(path.startswith(constants[root_name]) for root_name in EXPECTED_HELPERS[helper_name])


class QingshanWhiteboxRootSafetyTests(unittest.TestCase):
    def test_editor_automation_has_the_exact_approved_root_contract(self):
        source = IMPLEMENTATION.read_text(encoding="utf-8")
        self.assertTrue(_has_safe_root_contract(source))

    def test_root_contract_rejects_broad_constants_used_by_helpers(self):
        source = IMPLEMENTATION.read_text(encoding="utf-8")
        broad_root_mutations = (
            ("VerticalSliceGraphRoot", "/Game/"),
            ("B0RGraphRoot", "/Game/GameXXK/"),
            ("PrototypeMapRoot", "/Game/GameXXK/Maps/"),
            ("VerticalSliceGraphRoot", "/Game/GameXXK/Environment/TownPCG/"),
        )
        for constant_name, broad_root in broad_root_mutations:
            with self.subTest(constant=constant_name, broad_root=broad_root):
                mutated = source.replace(
                    f'{constant_name}[] = TEXT("{EXPECTED_ROOTS[constant_name]}")',
                    f'{constant_name}[] = TEXT("{broad_root}")',
                )
                self.assertNotEqual(mutated, source)
                self.assertFalse(_has_safe_root_contract(mutated))

    def test_root_contract_rejects_sibling_prefix_acceptance(self):
        source = IMPLEMENTATION.read_text(encoding="utf-8")
        sibling_paths = (
            ("IsManagedGraphPath", "/Game/GameXXK/Environment/TownPCG/VerticalSliceSibling/Graph"),
            ("IsManagedGraphPath", "/Game/GameXXK/Environment/TownPCG/B0Rogue/Graph"),
            ("IsManagedPrototypeMapPath", "/Game/GameXXK/Maps/PrototypeSibling/Map"),
            ("IsManagedPrototypeMapPath", "/Game/GameXXK/Maps/Developer/Map"),
        )
        for helper_name, sibling_path in sibling_paths:
            with self.subTest(helper=helper_name, path=sibling_path):
                self.assertFalse(_managed_path_is_accepted(source, helper_name, sibling_path))

        for constant_name, approved_root in EXPECTED_ROOTS.items():
            with self.subTest(constant=constant_name):
                mutated = source.replace(
                    f'{constant_name}[] = TEXT("{approved_root}")',
                    f'{constant_name}[] = TEXT("{approved_root.rstrip("/")}")',
                )
                self.assertNotEqual(mutated, source)
                self.assertFalse(_has_safe_root_contract(mutated))

    def test_root_rejection_diagnostics_name_both_managed_scopes(self):
        source = IMPLEMENTATION.read_text(encoding="utf-8")
        self.assertIn("managed town PCG graph roots", source)
        self.assertIn("managed prototype/Dev maps", source)
        self.assertIn("managed prototype/Dev map level", source)
        self.assertNotIn("managed town PCG VerticalSlice root", source)
        self.assertNotIn("restricted to prototype maps", source)
        self.assertNotIn("not a prototype map level", source)


class QingshanWhiteboxAssemblerSourceTests(unittest.TestCase):
    def setUp(self):
        self.source = ASSEMBLER.read_text(encoding="utf-8")
        self.tree = ast.parse(self.source)

    def test_stable_map_phase_and_ownership_contract(self):
        required = {
            "SOURCE_MAP": "/Game/GameXXK/Maps/L_QingshanInn",
            "WHITEBOX_MAP": "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R",
            "MANAGED_TAG": "QingshanB0RManaged",
            "PHASE_SETUP": "setup",
            "PHASE_FINALIZE": "finalize",
        }
        constants = {}
        for node in self.tree.body:
            if isinstance(node, ast.Assign) and len(node.targets) == 1:
                target = node.targets[0]
                if isinstance(target, ast.Name) and isinstance(node.value, ast.Constant):
                    constants[target.id] = node.value.value
        self.assertEqual({name: constants.get(name) for name in required}, required)

    def test_imports_validated_config_and_deterministic_layout_helpers(self):
        self.assertIn("from gamexxk_qingshan_whitebox_config import load_config", self.source)
        self.assertIn("from qingshan_whitebox_acceptance import (", self.source)
        self.assertIn("generate_seeded_proxy_transforms", self.source)
        self.assertIn("canonical_layout_hash", self.source)

    def test_required_labels_and_proxy_assets_are_literal_and_stable(self):
        labels = (
            "QS_B0R_Road_Main", "QS_B0R_River_Main", "QS_B0R_Bridge_Main",
            "QS_B0R_Dock_South", "QS_B0R_Terrain_Base",
            "QS_B0R_Terrain_CorePlateau", "QS_B0R_Terrain_SouthSlope",
            "QS_B0R_Terrain_DockLowland", "QS_B0R_PCG_Buildings",
            "QS_B0R_PCG_Foliage", "QS_B0R_PCG_Mountains",
            "CAM_QS_GATE_ARRIVAL", "CAM_QS_TOWN_CORE", "CAM_QS_MAIN_BRIDGE",
            "CAM_QS_SOUTH_DOCK", "/Engine/BasicShapes/Cube",
            "/Engine/BasicShapes/Cone",
        )
        for value in labels:
            with self.subTest(value=value):
                self.assertIn(value, self.source)

    def test_source_map_is_never_a_save_target(self):
        forbidden = re.compile(r"save_(?:loaded_asset|asset)\s*\(\s*SOURCE_MAP")
        self.assertIsNone(forbidden.search(self.source))
        self.assertNotIn("save_map(SOURCE_MAP", self.source)
        self.assertIn("save_current_level()", self.source)
        self.assertIn("_current_map_package() != WHITEBOX_MAP", self.source)

    def test_no_broad_actor_delete_or_untagged_mutation_architecture(self):
        self.assertNotIn("destroy_actor", self.source)
        self.assertNotIn("delete_asset", self.source)
        self.assertNotIn("for actor in _all_actors():\n        actor.", self.source)
        self.assertIn("_require_managed_actor", self.source)
        self.assertIn("MANAGED_TAG", self.source)

    def test_idempotent_helpers_and_safety_gates_exist(self):
        functions = {
            node.name for node in self.tree.body if isinstance(node, ast.FunctionDef)
        }
        required = {
            "_ensure_whitebox_map", "_dirty_map_package_names",
            "_dirty_content_package_names", "_find_unique_actor_by_label",
            "_snapshot_preserved_actors", "_north_local_to_world",
            "_parse_operation_payload", "_create_or_update_managed_mesh_actor",
            "_create_or_update_managed_spline_actor", "_create_or_update_camera",
            "setup_whitebox", "finalize_whitebox", "assemble_whitebox", "main",
        }
        self.assertFalse(required - functions)
        self.assertIn("duplicate_asset(SOURCE_MAP, WHITEBOX_MAP)", self.source)
        self.assertIn("refuses map/content dirtiness", self.source)
        self.assertIn("preserved gameplay actors changed", self.source)

    def test_graph_counts_finalize_state_and_quickroad_truthfulness_are_explicit(self):
        self.assertIn(
            'EXPECTED_COUNTS = {"buildings": 16, "foliage": 100, "mountains": 24}',
            self.source,
        )
        self.assertIn('"quickroad_status": "proxy_spline"', self.source)
        self.assertIn('"generated"', self.source)
        self.assertIn('"generating"', self.source)
        self.assertIn('"pending": True', self.source)
        self.assertIn('allow_nan=False', self.source)

    def test_graph_paths_are_explicit_stable_literals(self):
        for suffix in ("Buildings", "Foliage", "Mountains"):
            self.assertIn(
                f"/Game/GameXXK/Environment/TownPCG/B0R/PCG_QS_B0R_{suffix}",
                self.source,
            )

    def test_local_rotations_compose_the_north_anchor_yaw(self):
        self.assertIn("def _north_local_yaw", self.source)
        self.assertIn("_north_local_yaw(north_transform, float(rotation[2]))", self.source)

    def test_exact_camera_and_terrain_category_counts_are_verified(self):
        self.assertIn("_managed_category_count", self.source)
        self.assertIn('"QingshanB0RCamera"', self.source)
        self.assertIn('"QingshanB0RTerrainProxy"', self.source)
        self.assertIn("!= 4", self.source)


if __name__ == "__main__":
    unittest.main()
