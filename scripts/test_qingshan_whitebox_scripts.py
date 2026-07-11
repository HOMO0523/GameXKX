import re
import ast
import json
import math
import unittest
import weakref
from pathlib import Path
from types import SimpleNamespace

from scripts.qingshan_whitebox_acceptance import canonical_layout_hash


ROOT = Path(__file__).resolve().parents[1]
IMPLEMENTATION = ROOT / "Source/GameXXKEditor/Private/GameXXKTownPCGAutomationLibrary.cpp"
ASSEMBLER = ROOT / "Content/Python/gamexxk_assemble_qingshan_whitebox_b0r.py"
VALIDATOR = ROOT / "Content/Python/gamexxk_validate_qingshan_whitebox_b0r.py"
ACCEPTANCE = ROOT / "Content/Python/gamexxk_qingshan_whitebox_acceptance.py"
EXPECTED_ROOTS = {
    "VerticalSliceGraphRoot": "/Game/GameXXK/Environment/TownPCG/VerticalSlice/",
    "B0RGraphRoot": "/Game/GameXXK/Environment/TownPCG/B0R/",
    "B1GraphRoot": "/Game/GameXXK/Environment/TownPCG/B1/",
    "PrototypeMapRoot": "/Game/GameXXK/Maps/Prototype/",
    "B0RMapRoot": "/Game/GameXXK/Maps/Dev/",
}
EXPECTED_HELPERS = {
    "IsManagedGraphPath": ("VerticalSliceGraphRoot", "B0RGraphRoot", "B1GraphRoot"),
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
    clauses = [
        rf"Path\.StartsWith\({re.escape(root)}, ESearchCase::CaseSensitive\)"
        for root in root_names
    ]
    helper_pattern = re.compile(
        rf"bool {re.escape(helper_name)}\(const FString& Path\)\s*"
        rf"\{{\s*return {'\\s*\\|\\| '.join(clauses)};\s*\}}"
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
    return any(
        constants.get(root_name) is not None and path.startswith(constants[root_name])
        for root_name in EXPECTED_HELPERS[helper_name]
    )


class QingshanWhiteboxRootSafetyTests(unittest.TestCase):
    def test_editor_automation_has_the_exact_approved_root_contract(self):
        source = IMPLEMENTATION.read_text(encoding="utf-8")
        self.assertTrue(_has_safe_root_contract(source))

    def test_root_contract_rejects_broad_constants_used_by_helpers(self):
        source = IMPLEMENTATION.read_text(encoding="utf-8")
        broad_root_mutations = (
            ("VerticalSliceGraphRoot", "/Game/"),
            ("B0RGraphRoot", "/Game/GameXXK/"),
            ("B1GraphRoot", "/Game/GameXXK/Environment/"),
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
            ("IsManagedGraphPath", "/Game/GameXXK/Environment/TownPCG/B1ogue/Graph"),
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

    def _function(self, name):
        return next(
            node for node in self.tree.body
            if isinstance(node, ast.FunctionDef) and node.name == name
        )

    def _host_finalize_validator(self):
        names = {"SOURCE_MAP", "WHITEBOX_MAP", "B0R_CONTENT_ROOT"}
        constants = [
            node for node in self.tree.body
            if isinstance(node, ast.Assign)
            and len(node.targets) == 1
            and isinstance(node.targets[0], ast.Name)
            and node.targets[0].id in names
        ]
        module = ast.Module(
            body=[*constants, self._function("_validate_finalize_dirty_state")],
            type_ignores=[],
        )
        namespace = {}
        exec(compile(ast.fix_missing_locations(module), str(ASSEMBLER), "exec"), namespace)
        return namespace["_validate_finalize_dirty_state"]

    def _host_function(self, name, *, dependencies=(), namespace=None):
        module = ast.Module(
            body=[*(self._function(item) for item in dependencies), self._function(name)],
            type_ignores=[],
        )
        globals_dict = dict(namespace or {})
        exec(compile(ast.fix_missing_locations(module), str(ASSEMBLER), "exec"), globals_dict)
        return globals_dict[name]

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
        self.assertIn('"allow_nan": False', self.source)

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

    def test_ue58_camera_component_property_fallback_is_used_everywhere(self):
        for path in (ASSEMBLER, VALIDATOR, ACCEPTANCE):
            with self.subTest(path=path.name):
                source = path.read_text(encoding="utf-8")
                tree = ast.parse(source)
                functions = {
                    node.name for node in tree.body if isinstance(node, ast.FunctionDef)
                }
                self.assertIn("_camera_component", functions)
                self.assertIn('get_editor_property("camera_component")', source)

        self.assertIn("component = _camera_component(actor)", self.source)
        validator_source = VALIDATOR.read_text(encoding="utf-8")
        acceptance_source = ACCEPTANCE.read_text(encoding="utf-8")
        self.assertIn("fov = float(_camera_component(actor).get_editor_property", validator_source)
        self.assertGreaterEqual(
            acceptance_source.count("_camera_component(_unique_actor(camera_id))"), 1
        )
        self.assertIn("fov = float(_camera_component(actor).get_editor_property", acceptance_source)

    def test_each_spline_call_supplies_cpp_ownership_and_semantic_tags(self):
        constants = {
            node.targets[0].id: ast.literal_eval(node.value)
            for node in self.tree.body
            if isinstance(node, ast.Assign)
            and len(node.targets) == 1
            and isinstance(node.targets[0], ast.Name)
            and node.targets[0].id == "ROAD_SEMANTIC_TAGS"
        }
        self.assertEqual(constants.get("ROAD_SEMANTIC_TAGS"), {
            "Road_Main": "Quick_Road_MainRoad",
            "Road_Core_North": "Quick_Road_MainRoad",
            "Road_Core_South": "Quick_Road_MainRoad",
        })
        setup = ast.unparse(self._function("setup_whitebox"))
        self.assertIn("('PrototypeOnly', 'Quick_Road_CityScope')", setup)
        self.assertIn("semantic_tag = ROAD_SEMANTIC_TAGS[spline['id']]", setup)
        self.assertIn("'PrototypeOnly', semantic_tag, 'Quick_Road_LayoutInput'", setup)
        self.assertIn("('PrototypeOnly', 'TownPCG_River'", setup)
        helper = ast.unparse(self._function("_create_or_update_managed_spline_actor"))
        self.assertIn("{MANAGED_TAG, *extra_tags}", helper)

    def test_finalize_refuses_dirty_source_map_and_content_before_loading_map(self):
        finalize = self._function("finalize_whitebox")
        calls = [
            node for node in ast.walk(finalize)
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
        ]
        preflight = next(node for node in calls if node.func.id == "_require_finalize_clean")
        load = next(node for node in calls if node.func.id == "_load_whitebox_only")
        self.assertLess(preflight.lineno, load.lineno)
        preflight_source = ast.unparse(self._function("_require_finalize_clean"))
        validator_source = ast.unparse(self._function("_validate_finalize_dirty_state"))
        self.assertIn("SOURCE_MAP in dirty_maps", validator_source)
        self.assertIn("_dirty_content_package_names()", preflight_source)

    def test_finalize_allows_dirty_current_whitebox_and_managed_b0r_content(self):
        validate = self._host_finalize_validator()
        validate(
            "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R",
            ["/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R"],
            [
                "/Game/GameXXK/Environment/TownPCG/B0R/PCG_QS_B0R_Buildings",
                "/Game/GameXXK/Environment/TownPCG/B0R/PCG_QS_B0R_Foliage",
                "/Game/GameXXK/Environment/TownPCG/B0R/PCG_QS_B0R_Mountains",
            ],
        )

    def test_finalize_blocks_dirty_source_before_load(self):
        validate = self._host_finalize_validator()
        with self.assertRaisesRegex(RuntimeError, "source map is dirty before finalize map load"):
            validate(
                "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R",
                ["/Game/GameXXK/Maps/L_QingshanInn"],
                [],
            )

    def test_finalize_blocks_unrelated_dirtiness_and_any_dirty_switch(self):
        validate = self._host_finalize_validator()
        cases = (
            (
                "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R",
                ["/Game/GameXXK/Maps/L_Unrelated"], [],
            ),
            (
                "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R",
                [], ["/Game/GameXXK/UI/WBP_Unrelated"],
            ),
            (
                "/Game/GameXXK/Maps/L_Main",
                [], ["/Game/GameXXK/Environment/TownPCG/B0R/PCG_QS_B0R_Buildings"],
            ),
            (
                "/Game/GameXXK/Maps/L_Main",
                ["/Game/GameXXK/Maps/L_Main"], [],
            ),
        )
        for current_map, dirty_maps, dirty_content in cases:
            with self.subTest(current_map=current_map, content=dirty_content):
                with self.assertRaisesRegex(RuntimeError, "refuses"):
                    validate(current_map, dirty_maps, dirty_content)

    def test_mountain_cubes_are_raised_by_scaled_half_height(self):
        author = ast.unparse(self._function("_author_graphs"))
        self.assertIn(
            "z_offset=abs(float(record['scale'][2])) * 50.0",
            author,
        )

    def test_all_rotators_use_named_pitch_yaw_roll_arguments(self):
        rotators = [
            node for node in ast.walk(self.tree)
            if isinstance(node, ast.Call)
            and isinstance(node.func, ast.Attribute)
            and node.func.attr == "Rotator"
        ]
        self.assertTrue(rotators)
        for call in rotators:
            with self.subTest(line=call.lineno):
                self.assertEqual(call.args, [])
                self.assertEqual({keyword.arg for keyword in call.keywords}, {
                    "pitch", "yaw", "roll",
                })


class QingshanWhiteboxTask6SourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = ASSEMBLER.read_text(encoding="utf-8")
        cls.tree = ast.parse(cls.source)
        cls.validator_source = VALIDATOR.read_text(encoding="utf-8")
        cls.acceptance_source = ACCEPTANCE.read_text(encoding="utf-8")
        cls.validator_tree = ast.parse(cls.validator_source)
        cls.acceptance_tree = ast.parse(cls.acceptance_source)

    def _function(self, tree, name=None):
        if name is None:
            name, tree = tree, self.tree
        return next(node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name == name)

    def _host_function(self, tree, filename=None, name=None, dependencies=(), namespace=None):
        if isinstance(tree, str):
            name, tree, filename = tree, self.tree, ASSEMBLER
        module = ast.Module(
            body=[*(self._function(tree, item) for item in dependencies), self._function(tree, name)],
            type_ignores=[],
        )
        scope = dict(namespace or {})
        exec(compile(ast.fix_missing_locations(module), str(filename), "exec"), scope)
        return scope[name]

    def test_validator_and_probe_have_stable_entrypoints_and_camera_ids(self):
        self.assertIn("def validate_whitebox", self.validator_source)
        self.assertIn("GAMEXXK_QINGSHAN_B0R_VALIDATION", self.validator_source)
        for action in ("snapshot", "view", "topdown"):
            self.assertIn(f'"{action}"', self.acceptance_source)
        camera_ids = {
            "CAM_QS_GATE_ARRIVAL", "CAM_QS_TOWN_CORE",
            "CAM_QS_MAIN_BRIDGE", "CAM_QS_SOUTH_DOCK",
        }
        self.assertEqual(
            set(ast.literal_eval(next(
                node.value for node in self.acceptance_tree.body
                if isinstance(node, ast.Assign)
                and any(isinstance(target, ast.Name) and target.id == "CAMERA_IDS" for target in node.targets)
            ))),
            camera_ids,
        )

    def test_scripts_are_read_only_except_viewport_camera(self):
        combined = self.validator_source + self.acceptance_source
        forbidden = (
            "save_current_level", "save_asset", "save_loaded_asset", "delete_asset",
            "destroy_actor", "spawn_actor", "set_actor_", "set_editor_property",
            "add_instance", "remove_instance", "clear_instances", "generate_town_pcg",
            "clear_town_pcg", "create_or_update", "attach_town_pcg_graph",
        )
        for token in forbidden:
            with self.subTest(token=token):
                self.assertNotIn(token, combined)
        self.assertIn("set_level_viewport_camera_info", self.acceptance_source)

    def test_validator_result_contract_and_deterministic_sources_are_explicit(self):
        for key in (
            "current_map", "source_map_clean", "protected_actor_snapshot",
            "managed_label_uniqueness", "road_spline_count", "river_spline_count",
            "bridge_road_horizontal_distance_cm", "bridge_river_horizontal_distance_cm",
            "building_instance_count", "foliage_instance_count", "mountain_instance_count",
            "crossing_count", "runtime_generation_disabled", "camera_count", "camera_ids",
            "terrain_zone_count", "player_gate_facing_angle_degrees", "layout_sha256",
            "errors", "success",
        ):
            self.assertIn(f'"{key}"', self.validator_source)
        self.assertIn("load_config", self.validator_source)
        self.assertIn("generate_seeded_proxy_transforms", self.validator_source)
        self.assertIn("canonical_layout_hash", self.validator_source)
        self.assertIn("get_town_pcg_status", self.validator_source)
        self.assertIn("get_instance_transform", self.validator_source)

    def test_spline_transform_ownership_and_geometry_checks_are_present(self):
        required = (
            "get_number_of_spline_points", "get_location_at_spline_point", "is_closed_loop",
            "building_footprint_overlap", "protected_anchor_overlap", "road_overlap",
            "river_overlap", "mountain_playable_bounds", "QingshanB0RManaged",
            "get_level", "get_graph", "PCG Generated Component",
        )
        for token in required:
            self.assertIn(token, self.validator_source)
        self.assertIn("anchor_transform", self.validator_source)
        self.assertIn("MainBridgeAnchor", self.validator_source)
        self.assertIn("SouthDockAnchor", self.validator_source)

    def test_all_task6_rotators_use_named_arguments(self):
        for tree in (self.validator_tree, self.acceptance_tree):
            for call in ast.walk(tree):
                if isinstance(call, ast.Call) and isinstance(call.func, ast.Attribute) and call.func.attr == "Rotator":
                    self.assertEqual(call.args, [], f"positional Rotator at line {call.lineno}")
                    self.assertEqual({item.arg for item in call.keywords}, {"pitch", "yaw", "roll"})

    def test_pure_math_helpers_handle_wrap_vertical_and_polyline_projection(self):
        angle = self._host_function(self.validator_tree, VALIDATOR, "_angle_delta_degrees", namespace={"math": math})
        distance = self._host_function(self.validator_tree, VALIDATOR, "_horizontal_distance_to_polyline", namespace={"math": math})
        look_at = self._host_function(self.acceptance_tree, ACCEPTANCE, "_look_at_degrees", namespace={"math": math})
        self.assertAlmostEqual(angle(179.0, -179.0), 2.0)
        self.assertAlmostEqual(distance((5, 3, 99), [(0, 0, 0), (10, 0, 0)]), 3.0)
        self.assertEqual(look_at((0, 0, 0), (0, 0, 10)), (90.0, 0.0, 0.0))
        with self.assertRaises(ValueError):
            look_at((0, 0, 0), (float("nan"), 0, 0))
        with self.assertRaises(ValueError):
            distance((0, 0, 0), [(0, 0, 0)])

    def test_transform_tolerance_rejects_nonfinite_and_uses_wrapped_angles(self):
        matches = self._host_function(
            self.validator_tree, VALIDATOR, "_transform_matches",
            dependencies=("_angle_delta_degrees",), namespace={"math": math},
        )
        actual = {"location_cm": [1, 2, 3], "rotation_degrees": [0, 179.9, 0], "scale": [1, 1, 1]}
        expected = {"location_cm": [1.05, 2, 3], "rotation_degrees": [0, -179.9, 0], "scale": [1, 1, 1]}
        self.assertTrue(matches(actual, expected, 0.1, 0.3, 0.01))
        actual["location_cm"][0] = float("inf")
        self.assertFalse(matches(actual, expected, 0.1, 0.3, 0.01))

    def test_acceptance_dirty_refusal_happens_before_map_load(self):
        events = []
        fake_unreal = SimpleNamespace(
            EditorLevelLibrary=SimpleNamespace(get_editor_world=lambda: SimpleNamespace(get_outermost=lambda: SimpleNamespace(get_path_name=lambda: "/Game/Else"))),
            EditorLoadingAndSavingUtils=SimpleNamespace(
                get_dirty_map_packages=lambda: [SimpleNamespace(get_path_name=lambda: "/Game/Dirty")],
                get_dirty_content_packages=lambda: [],
                load_map=lambda path: events.append(path),
            ),
        )
        ensure = self._host_function(
            self.acceptance_tree, ACCEPTANCE, "_ensure_whitebox",
            namespace={
                "unreal": fake_unreal,
                "WHITEBOX_MAP": "/Game/Whitebox",
                "_current_map": self._host_function(self.acceptance_tree, ACCEPTANCE, "_current_map", dependencies=("_package_path",), namespace={"unreal": fake_unreal}),
                "_dirty_maps": lambda: ["/Game/Dirty"], "_dirty_content": lambda: [],
            },
        )
        with self.assertRaisesRegex(RuntimeError, "dirty"):
            ensure()
        self.assertEqual(events, [])

    def test_view_verifies_configured_world_transform_before_moving_viewport(self):
        view_source = ast.unparse(self._function(self.acceptance_tree, "_camera_view"))
        self.assertIn("_north_local_location", view_source)
        self.assertIn("_transform_matches", view_source)
        self.assertIn("camera transform differs from config", view_source)
        verify = view_source.index("_transform_matches")
        move = view_source.index("set_level_viewport_camera_info")
        self.assertLess(verify, move)

    def test_runtime_enabled_flag_is_a_validation_error(self):
        validate_runtime = self._host_function(
            self.validator_tree, VALIDATOR, "_validate_runtime_generation",
            dependencies=("_strict_property", "_runtime_generation_evidence"),
            namespace={"_error": lambda result, check, message, **details: result["errors"].append({"check": check, **details})},
        )
        result = {"errors": []}
        fields = {
            "generation_trigger": "GenerateOnDemand",
            "generate_on_drop_when_trigger_on_demand": False,
            "regenerate_in_editor": False,
        }
        component = SimpleNamespace(get_editor_property=lambda field: fields[field])
        self.assertTrue(validate_runtime(result, component, "PCG"))
        self.assertEqual(result["errors"], [])
        fields["generation_trigger"] = "<PCGComponentGenerationTrigger.GENERATE_ON_DEMAND: 1>"
        self.assertTrue(validate_runtime(result, component, "PCGEnum"))
        self.assertEqual(result["errors"], [])
        fields["generate_on_drop_when_trigger_on_demand"] = True
        self.assertFalse(validate_runtime(result, component, "PCG"))
        self.assertEqual(result["errors"][0]["check"], "runtime_generation")
        fields.update(generation_trigger="GenerateOnLoad", generate_on_drop_when_trigger_on_demand=False)
        self.assertFalse(validate_runtime(result, component, "WrongTrigger"))
        self.assertEqual(result["errors"][-1]["check"], "runtime_generation")
        missing = SimpleNamespace(get_editor_property=lambda field: (_ for _ in ()).throw(AttributeError(field)))
        self.assertFalse(validate_runtime(result, missing, "Missing"))
        self.assertEqual(result["errors"][-1]["check"], "runtime_generation_fields")

    def test_player_gate_facing_uses_horizontal_direction_not_pitch_or_gate_rotation(self):
        angle = self._host_function(
            self.validator_tree, VALIDATOR, "_player_gate_facing_angle",
            namespace={"math": math},
        )
        self.assertAlmostEqual(angle((0, 0, 0), 0, (0, 10, 999)), 90.0)
        self.assertAlmostEqual(angle((0, 0, 0), 179, (-10, -0.2, -999)), 2.1457628, places=5)
        self.assertAlmostEqual(angle((0, 0, 0), 0, (10, 0, -999)), 0.0)

    def test_preserved_snapshot_is_loaded_and_full_transforms_are_compared(self):
        self.assertIn("QingshanTownPreservedActors.json", self.validator_source)
        validate_source = ast.unparse(self._function(self.validator_tree, "_validate_preserved_actors"))
        self.assertIn("_validate_preserved_transform", validate_source)
        self.assertIn("preserved_baseline", validate_source)
        compare = self._host_function(
            self.validator_tree, VALIDATOR, "_baseline_transform",
            namespace={"math": math},
        )
        record = {
            "location_cm": {"x": 1, "y": 2, "z": 3},
            "rotation_degrees": {"roll": 4, "pitch": 5, "yaw": 6},
            "scale": {"x": 1, "y": 1, "z": 1},
        }
        self.assertEqual(compare(record)["rotation_degrees"], [4.0, 5.0, 6.0])
        with self.assertRaises(ValueError):
            compare({})
        validate = self._host_function(
            self.validator_tree, VALIDATOR, "_validate_preserved_transform",
            namespace={
                "_baseline_transform": compare,
                "_transform_matches": lambda actual, expected, *args: actual == expected,
                "_error": lambda result, check, message, **details: result["errors"].append(check),
            },
        )
        result = {"errors": []}
        validate(result, "Actor", {"wrong": True}, record)
        self.assertEqual(result["errors"], ["protected_actor_transform"])
        header = self._host_function(
            self.validator_tree, VALIDATOR, "_validate_baseline_header",
            namespace={"SOURCE_MAP": "/Game/Source"},
        )
        header({"schema_version": 1, "source_map": "/Game/Source", "captured_read_only": True, "actors": {}})
        with self.assertRaises(ValueError):
            header({"schema_version": 2, "source_map": "/Game/Source", "captured_read_only": True, "actors": {}})
        actor_class = self._host_function(
            self.validator_tree, VALIDATOR, "_validate_preserved_actor_class",
            namespace={
                "_object_path": lambda value: value,
                "_error": lambda result, check, message, **details: result["errors"].append(check),
            },
        )
        result = {"errors": []}
        actor_class(result, "Actor", SimpleNamespace(get_class=lambda: "/Actual"), {"class_path": "/Expected"})
        self.assertEqual(result["errors"], ["protected_actor_class"])

    def test_observed_transforms_require_unique_id_matching(self):
        matcher = self._host_function(
            self.validator_tree, VALIDATOR, "_match_observed_transforms",
            namespace={
                "_transform_matches": lambda actual, expected, *args: actual["value"] == expected["value"],
                "ID_ASSOCIATION_TOLERANCE": 0.00049,
            },
        )
        expected = [{"id": "A", "value": 1}, {"id": "B", "value": 2}]
        matched = matcher([{"value": 2}, {"value": 1}], expected, "buildings")
        self.assertEqual([item["id"] for item in matched], ["A", "B"])
        with self.assertRaisesRegex(RuntimeError, "missing"):
            matcher([{"value": 1}], expected, "buildings")
        with self.assertRaisesRegex(RuntimeError, "ambiguous"):
            matcher([{"value": 1}, {"value": 1}], [{"id": "A", "value": 1}, {"id": "B", "value": 1}], "buildings")

    def test_whole_layout_hash_is_strict_after_tolerant_id_association(self):
        transform_matches = self._host_function(
            self.validator_tree, VALIDATOR, "_transform_matches",
            dependencies=("_angle_delta_degrees",), namespace={"math": math},
        )
        matcher = self._host_function(
            self.validator_tree, VALIDATOR, "_match_observed_transforms",
            namespace={"_transform_matches": transform_matches, "ID_ASSOCIATION_TOLERANCE": 0.00049},
        )
        deviations = self._host_function(
            self.validator_tree, VALIDATOR, "_layout_max_deviations",
            dependencies=("_angle_delta_degrees",), namespace={"math": math},
        )
        validate_hash = self._host_function(
            self.validator_tree, VALIDATOR, "_validate_observed_layout",
            namespace={
                "_match_observed_transforms": matcher,
                "canonical_layout_hash": canonical_layout_hash,
                "_error": lambda result, check, message, **details: result["errors"].append(check),
                "_layout_max_deviations": deviations,
                "HASH_DECIMALS": 3,
            },
        )
        expected = {"buildings": [{"id": "A", "location_cm": [1.00049, 0, 0], "rotation_degrees": [0, 0, 0], "scale": [1, 1, 1]}]}
        exact = {"buildings": [{"location_cm": [1.00049, 0, 0], "rotation_degrees": [0, 0, 0], "scale": [1, 1, 1]}]}
        result = {"errors": []}
        validate_hash(result, exact, expected)
        self.assertEqual(result["errors"], [])
        self.assertEqual(result["observed_layout_sha256"], result["expected_layout_sha256"])
        noisy = {"buildings": [{"location_cm": [1.00051, 0, 0], "rotation_degrees": [0, 0, 0], "scale": [1, 1, 1]}]}
        result = {"errors": []}
        validate_hash(result, noisy, expected)
        self.assertEqual(result["errors"], ["layout_sha256"])

    def test_map_load_truthiness_requires_actual_whitebox_postcondition(self):
        for tree, filename, function_name in (
            (self.validator_tree, VALIDATOR, "_ensure_whitebox_read_only"),
            (self.acceptance_tree, ACCEPTANCE, "_ensure_whitebox"),
        ):
            state = {"current": "/Game/GameXXK/Maps/L_Main"}
            ensure = self._host_function(
                tree, filename, function_name,
                namespace={
                    "WHITEBOX_MAP": "/Game/Whitebox",
                    "_current_map": lambda: state["current"],
                    "_dirty_maps": lambda: [], "_dirty_content": lambda: [],
                    "unreal": SimpleNamespace(EditorLoadingAndSavingUtils=SimpleNamespace(load_map=lambda path: True)),
                },
            )
            with self.subTest(script=filename.name):
                with self.assertRaisesRegex(RuntimeError, "current map"):
                    ensure()

    def test_validator_reports_expected_and_observed_layout_hashes(self):
        self.assertIn('"expected_layout_sha256"', self.validator_source)
        self.assertIn('"observed_layout_sha256"', self.validator_source)
        self.assertIn("_match_observed_transforms", self.acceptance_source)
        self.assertNotIn('f"{key}_{index:03d}"', self.acceptance_source)

    def test_bridge_alignment_uses_actual_actor_and_closest_spline_locations(self):
        validate_source = ast.unparse(self._function(self.validator_tree, "_validate_actual_bridge_alignment"))
        self.assertIn("find_location_closest_to_world_location", validate_source)
        self.assertIn("get_actor_location", validate_source)
        self.assertIn("bridge_road_horizontal_distance_cm", validate_source)
        class Spline:
            def __init__(self, location): self.location = location
            def find_location_closest_to_world_location(self, query, coordinate_space): return self.location
        class Actor:
            def __init__(self, location=None, component=None): self.location, self.component = location, component
            def get_actor_location(self): return self.location
            def get_components_by_class(self, component_class): return [self.component] if self.component else []
        vector = lambda x, y, z=0: SimpleNamespace(x=x, y=y, z=z)
        actors = {
            "QS_B0R_Bridge_Main": Actor(location=vector(0, 0)),
            "QS_B0R_Road_Main": Actor(component=Spline(vector(50, 0))),
            "QS_B0R_River_Main": Actor(component=Spline(vector(0, 80))),
        }
        validate = self._host_function(
            self.validator_tree, VALIDATOR, "_validate_actual_bridge_alignment",
            namespace={
                "math": math,
                "ROAD_LABELS": {"Road_Main": "QS_B0R_Road_Main"},
                "RIVER_LABELS": {"River_Main": "QS_B0R_River_Main"},
                "unreal": SimpleNamespace(SplineComponent=object, SplineCoordinateSpace=SimpleNamespace(WORLD="world")),
                "_matches": lambda values, label: [values[label]] if label in values else [],
                "_vector": lambda value: [value.x, value.y, value.z],
                "_error": lambda result, check, message, **details: result["errors"].append(check),
            },
        )
        result = {"errors": []}
        validate(result, actors)
        self.assertEqual(result["bridge_road_horizontal_distance_cm"], 50.0)
        self.assertEqual(result["bridge_river_horizontal_distance_cm"], 80.0)
        self.assertEqual(result["errors"], [])

    def test_topdown_rotates_local_bounds_center_through_nonidentity_anchor(self):
        location = self._host_function(
            self.acceptance_tree, ACCEPTANCE, "_topdown_location",
            namespace={"math": math, "_vector": lambda value: [value.x, value.y, value.z]},
        )
        anchor = SimpleNamespace(
            get_actor_location=lambda: SimpleNamespace(x=100.0, y=200.0, z=300.0),
            get_actor_rotation=lambda: SimpleNamespace(pitch=0.0, yaw=90.0, roll=0.0),
            get_actor_scale3d=lambda: SimpleNamespace(x=1.0, y=1.0, z=1.0),
        )
        actual = location(anchor, [-10.0, 30.0, -20.0, 20.0])
        self.assertAlmostEqual(actual[0], 100.0)
        self.assertAlmostEqual(actual[1], 210.0)
        self.assertAlmostEqual(actual[2], 42300.0)

    def test_duplicate_reference_is_released_and_ue58_unload_outputs_are_checked(self):
        class Duplicate:
            pass

        class FakeAssets:
            duplicate_ref = None

            @staticmethod
            def does_asset_exist(path):
                return path.endswith("L_QingshanInn")

            @classmethod
            def duplicate_asset(cls, source, destination):
                duplicate = Duplicate()
                cls.duplicate_ref = weakref.ref(duplicate)
                return duplicate

            @staticmethod
            def save_loaded_asset(asset, only_if_dirty):
                return True

            @staticmethod
            def get_package_for_object(asset):
                return "whitebox-package"

        class FakeSaving:
            result = (True, "")

            @classmethod
            def unload_packages(cls, packages):
                if FakeAssets.duplicate_ref() is not None:
                    raise AssertionError("duplicate asset reference was retained during unload")
                return cls.result

        fake_unreal = SimpleNamespace(
            EditorAssetLibrary=FakeAssets,
            EditorLoadingAndSavingUtils=FakeSaving,
            SystemLibrary=SimpleNamespace(collect_garbage=lambda: None),
        )
        constants = {
            "SOURCE_MAP": "/Game/GameXXK/Maps/L_QingshanInn",
            "WHITEBOX_MAP": "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R",
            "unreal": fake_unreal,
            "_require_editor_clean": lambda context: ([], []),
        }
        ensure = self._host_function("_ensure_whitebox_map", namespace=constants)
        self.assertTrue(ensure())
        FakeSaving.result = (False, "still referenced")
        with self.assertRaisesRegex(RuntimeError, "still referenced"):
            ensure()

    def test_curve_plate_specs_follow_each_segment_without_terminal_overshoot(self):
        build = self._host_function("_build_curve_plate_specs", namespace={"math": math})
        specs = build([(0.0, 0.0, 10.0), (3.0, 4.0, 20.0), (3.0, 8.0, 30.0)], 200.0, 10.0)
        self.assertEqual(len(specs), 2)
        self.assertAlmostEqual(specs[0]["length_cm"], 5.0)
        self.assertAlmostEqual(specs[0]["yaw_degrees"], math.degrees(math.atan2(4.0, 3.0)))
        self.assertEqual(specs[0]["location_cm"], [1.5, 2.0, 10.0])
        self.assertAlmostEqual(specs[1]["length_cm"], 4.0)
        self.assertAlmostEqual(specs[1]["yaw_degrees"], 90.0)
        last = specs[-1]
        endpoint_x = last["location_cm"][0] + math.cos(math.radians(last["yaw_degrees"])) * last["length_cm"] / 2
        endpoint_y = last["location_cm"][1] + math.sin(math.radians(last["yaw_degrees"])) * last["length_cm"] / 2
        self.assertAlmostEqual(endpoint_x, 3.0)
        self.assertAlmostEqual(endpoint_y, 8.0)
        self.assertEqual(last["width_cm"], 200.0)
        self.assertEqual(last["thickness_cm"], 10.0)

    def test_north_anchor_requires_finite_upright_unit_transform(self):
        validate = self._host_function("_validate_north_anchor", namespace={"math": math})

        def actor(location=(1.0, 2.0, 3.0), rotation=(0.0, 45.0, 0.0), scale=(1.0, 1.0, 1.0)):
            transform = SimpleNamespace(
                translation=SimpleNamespace(x=location[0], y=location[1], z=location[2]),
                scale3d=SimpleNamespace(x=scale[0], y=scale[1], z=scale[2]),
            )
            rotator = SimpleNamespace(pitch=rotation[0], yaw=rotation[1], roll=rotation[2])
            return SimpleNamespace(
                get_actor_transform=lambda: transform,
                get_actor_rotation=lambda: rotator,
            )

        self.assertIsNotNone(validate(actor()))
        invalid = (
            actor(rotation=(0.01, 45.0, 0.0)),
            actor(rotation=(0.0, float("nan"), 0.0)),
            actor(scale=(1.0, 1.01, 1.0)),
            actor(location=(float("inf"), 2.0, 3.0)),
        )
        for item in invalid:
            with self.subTest(actor=item):
                with self.assertRaises(RuntimeError):
                    validate(item)

    def test_transform_payload_rejects_nonfinite_location_rotation_and_scale(self):
        payload = self._host_function(
            "_transform_payload",
            dependencies=("_vector_payload",),
            namespace={
                "math": math,
                "Any": object,
                "_actor_class_path": lambda actor: "FakeActor",
            },
        )

        def actor(location=(1.0, 2.0, 3.0), rotation=(0.0, 45.0, 0.0), scale=(1.0, 1.0, 1.0)):
            transform = SimpleNamespace(
                translation=SimpleNamespace(x=location[0], y=location[1], z=location[2]),
                scale3d=SimpleNamespace(x=scale[0], y=scale[1], z=scale[2]),
            )
            rotator = SimpleNamespace(roll=rotation[0], pitch=rotation[1], yaw=rotation[2])
            return SimpleNamespace(
                get_actor_transform=lambda: transform,
                get_actor_rotation=lambda: rotator,
            )

        self.assertEqual(payload(actor())["scale"], [1.0, 1.0, 1.0])
        for item in (
            actor(location=(float("nan"), 2.0, 3.0)),
            actor(rotation=(0.0, float("inf"), 0.0)),
            actor(scale=(1.0, 1.0, float("nan"))),
        ):
            with self.assertRaises(RuntimeError):
                payload(item)

    def test_compact_json_serialization_has_guaranteed_failure_payload(self):
        serialize = self._host_function("_compact_json_result", namespace={"json": json})
        valid = serialize({"success": True, "value": 3})
        self.assertEqual(valid, '{"success":true,"value":3}')
        for bad in ({"bad": object()}, {"bad": float("nan")}):
            result = serialize(bad)
            self.assertNotIn('": ', result)
            self.assertNotIn(', "', result)
            decoded = json.loads(result)
            self.assertFalse(decoded["success"])
            self.assertFalse(decoded["complete"])
            self.assertIn("serialization", decoded["error"].lower())


if __name__ == "__main__":
    unittest.main()
