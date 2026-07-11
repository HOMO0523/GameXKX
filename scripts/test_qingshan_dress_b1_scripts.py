from __future__ import annotations

import ast
import contextlib
import importlib.util
import io
import json
import math
from pathlib import Path
import subprocess
import sys
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
ASSEMBLER_PATH = ROOT / "Content/Python/gamexxk_assemble_qingshan_dress_b1.py"
RUNNER_PATH = ROOT / "scripts/run_qingshan_dress_b1.py"
ASSEMBLY_SCRIPT = "Content/Python/gamexxk_assemble_qingshan_dress_b1.py"
ASSEMBLY_MARKER = "GAMEXXK_QINGSHAN_B1_ASSEMBLY"
PHASES = ("setup", "infrastructure", "finalize_begin", "finalize_poll")


def _function_body(source: str, name: str) -> str:
    marker = f"def {name}("
    if marker not in source:
        return ""
    tree = ast.parse(source)
    node = next(
        item for item in ast.walk(tree)
        if isinstance(item, (ast.FunctionDef, ast.AsyncFunctionDef)) and item.name == name
    )
    lines = source.splitlines()
    return "\n".join(lines[node.lineno - 1:node.end_lineno])


def _load_module(path: Path, name: str):
    if not path.is_file():
        raise AssertionError(f"required module is missing: {path}")
    content_python = str(ROOT / "Content/Python")
    if content_python not in sys.path:
        sys.path.insert(0, content_python)
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise AssertionError(f"could not load module spec: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class B1AssemblerSourceContractTests(unittest.TestCase):
    def setUp(self):
        self.source = ASSEMBLER_PATH.read_text(encoding="utf-8") if ASSEMBLER_PATH.exists() else ""

    def test_script_exists_and_parses(self):
        self.assertTrue(ASSEMBLER_PATH.is_file(), "B1 assembler is missing")
        ast.parse(self.source)

    def test_exact_bounded_phase_and_marker_contract(self):
        for name, value in (
            ("PHASE_SETUP", "setup"),
            ("PHASE_INFRASTRUCTURE", "infrastructure"),
            ("PHASE_FINALIZE_BEGIN", "finalize_begin"),
            ("PHASE_FINALIZE_POLL", "finalize_poll"),
        ):
            self.assertIn(f'{name} = "{value}"', self.source)
        self.assertIn(f'ASSEMBLY_MARKER = "{ASSEMBLY_MARKER}"', self.source)
        dispatcher = _function_body(self.source, "assemble_b1")
        for phase in PHASES:
            self.assertIn(phase, dispatcher)
        self.assertIn("_phase_from_argv", self.source)
        self.assertIn("unsupported phase", self.source)

    def test_every_mutating_phase_has_exact_b1_current_map_and_hash_guards(self):
        for function_name in (
            "setup_b1", "build_infrastructure", "begin_finalize", "poll_finalize",
        ):
            with self.subTest(function=function_name):
                body = _function_body(self.source, function_name)
                self.assertIn("_require_b1_current_map", body)
                self.assertIn("_snapshot_protected_actors", body)
                self.assertIn("_assert_protected_actors", body)
        guarded = _function_body(self.source, "_run_guarded_phase")
        self.assertIn("validate_protected_file_hashes", guarded)
        self.assertIn("before_hashes", guarded)
        self.assertIn("after_hashes", guarded)
        self.assertIn("before_hashes != after_hashes", guarded)
        self.assertIn("except Exception", guarded)
        self.assertGreaterEqual(guarded.count("validate_protected_file_hashes"), 2)
        map_guard = _function_body(self.source, "_require_b1_current_map")
        self.assertIn("_current_level_package", map_guard)
        self.assertIn("world/current level", map_guard)
        self.assertIn("EXPECTED_PROTECTED_FILE_COUNT = 3", self.source)
        self.assertIn('B1_MAP = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1"', self.source)
        self.assertIn('B0R_MAP = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Whitebox_B0R"', self.source)

    def test_every_mutating_phase_preflights_dirty_scope_before_first_write(self):
        first_write_tokens = {
            "setup_b1": "_ensure_b1_map",
            "build_infrastructure": "reset_road_infrastructure",
            "begin_finalize": "_author_pcg_groups",
            "poll_finalize": "_apply_generated_component_policy",
        }
        for function_name, first_write in first_write_tokens.items():
            with self.subTest(function=function_name):
                body = _function_body(self.source, function_name)
                preflight_offset = body.find("_validate_dirty_scope()")
                write_offset = body.find(first_write)
                self.assertGreaterEqual(preflight_offset, 0)
                self.assertGreater(write_offset, preflight_offset)

    def test_map_clone_and_cleanup_are_first_run_only_exact_and_owned(self):
        ensure = _function_body(self.source, "_ensure_b1_map")
        self.assertIn("does_asset_exist(B1_MAP)", ensure)
        self.assertIn("duplicate_asset(B0R_MAP, B1_MAP)", ensure)
        self.assertIn("save_loaded_asset", ensure)
        cleanup = _function_body(self.source, "_cleanup_created_b0r_clone")
        self.assertIn("created", cleanup)
        self.assertIn("QingshanB0RManaged", cleanup)
        self.assertIn("B0R_CLONE_CLEANUP_LABELS", cleanup)
        self.assertIn("unexpected cloned B0R managed actor", cleanup)
        setup = _function_body(self.source, "setup_b1")
        self.assertIn("_cleanup_created_b0r_clone(created", setup)
        rerun = _function_body(self.source, "_cleanup_b1_rerun_actors")
        self.assertIn("QingshanB1Managed", rerun)
        self.assertNotIn("QingshanB0RManaged", rerun)
        self.assertNotIn("delete_directory", self.source)
        self.assertNotIn("delete_asset(B0R_MAP", self.source)

    def test_one_local_to_world_path_is_used_for_all_anchor_local_records(self):
        body = _function_body(self.source, "local_to_world")
        self.assertIn("anchor_transform.transform_location", body)
        self.assertIn("unreal.Vector(*map(float, local_xyz))", body)
        self.assertEqual(self.source.count("anchor_transform.transform_location("), 1)
        for token in (
            "building_plots", "prop_records", "vegetation_records",
            "mountain_records", "cameras", "center_local_cm", "points_cm",
        ):
            self.assertIn(token, self.source)

    def test_protected_gate_player_and_npc_actors_are_never_cleanup_targets(self):
        snapshot = _function_body(self.source, "_snapshot_protected_actors")
        self.assertIn("QingshanInn_TownExit", snapshot)
        self.assertIn("unreal.PlayerStart", snapshot)
        for token in ("quest", "npc", "follower", "interaction"):
            self.assertIn(token, snapshot.lower())
        self.assertIn("_validate_north_gate_anchor", self.source)
        cleanup = _function_body(self.source, "_cleanup_b1_rerun_actors")
        self.assertIn("protected_labels", cleanup)
        self.assertIn("refusing to delete protected actor", cleanup)

    def test_bridge_gap_and_three_real_quickroad_width_groups_are_required(self):
        infrastructure = _function_body(self.source, "build_infrastructure")
        for token in (
            "reset_road_infrastructure",
            "ensure_landscape_infrastructure",
            "generate_road_network",
            "rebuild_road_intersections",
            "clear_and_apply_all_road_influence",
            "extract_road_edges",
            "audit_infrastructure",
            "QS_B1_Main", "QS_B1_CoreNorth", "QS_B1_CoreSouth",
            "800", "450", "400",
        ):
            self.assertIn(token, infrastructure)
        self.assertIn("_split_polyline_outside_circle", infrastructure)
        self.assertIn("MainBridgeAnchor", infrastructure)
        self.assertIn("protected_radius_cm", infrastructure)
        self.assertNotIn("BakeSingleRoadNetwork", infrastructure)

    def test_managed_spline_helper_always_supplies_prototype_ownership_tag(self):
        helper = _function_body(self.source, "_create_or_update_b1_spline")
        self.assertIn('"PrototypeOnly"', helper)
        self.assertIn("B1_MANAGED_TAG", helper)

    def test_quickroad_audit_is_measured_strict_and_multi_boundary(self):
        body = _function_body(self.source, "_validate_quickroad_audit")
        for token in (
            '"network_count"', '"owned_network_count"', '"unowned_network_count"',
            '"road_triangle_count"', '"intersection_patch_count"',
            '"source_width_group_count"', '"source_records"',
            '"edit_layer_active"', '"edit_layer_visible"', '"edit_layer_locked"',
            '"non_neutral_sample_count"', '"edit_layer_delta_sha256"',
            '"edge_actor_labels"', '"edge_spline_count"', '"edge_geometry_sha256"',
        ):
            self.assertIn(token, body)
        self.assertIn("edge_spline_count < 2", body)
        self.assertIn("verified_procmesh", body)
        self.assertIn("source_records", body)

    def test_real_configuration_builds_exactly_34_advanced_pcg_groups(self):
        module = _load_module(ASSEMBLER_PATH, "_gamexxk_b1_assembler_group_test")
        config_module = _load_module(
            ROOT / "Content/Python/gamexxk_qingshan_dress_b1_config.py",
            "_gamexxk_b1_config_group_test",
        )
        groups = module.build_pcg_group_specs(config_module.load_config())
        by_category = {}
        for group in groups:
            by_category[group["category"]] = by_category.get(group["category"], 0) + 1
        self.assertEqual(by_category, {
            "building": 19, "prop": 10, "static_plant": 4, "mountain": 1,
        })
        self.assertEqual(len(groups), 34)
        self.assertEqual(sum(len(group["records"]) for group in groups), 192)
        self.assertEqual(len({group["graph_path"] for group in groups}), 34)
        self.assertTrue(all(
            group["graph_path"].startswith(
                "/Game/GameXXK/Environment/TownPCG/B1/Graphs/"
            ) for group in groups
        ))

    def test_pcg_authoring_consumes_live_edges_seeds_materials_and_never_substitutes_actors(self):
        author = _function_body(self.source, "_author_pcg_groups")
        for token in (
            "create_or_update_town_pcg_graph_advanced",
            "attach_town_pcg_graph_advanced",
            "clear_town_pcg",
            "consumed_edge_labels", "consumed_edge_digest",
            "minimum_road_clearance_cm", "material_override_paths",
            "base_seed", "component_seed", "_filter_transforms_against_live_edges",
        ):
            self.assertIn(token, author)
        self.assertNotIn("spawn_actor_from_class(unreal.StaticMeshActor", author)
        self.assertIn("rejected_stable_ids", author)
        self.assertIn("EXPECTED_PCG_GROUP_COUNTS", self.source)
        self.assertIn('"building": 19', self.source)
        self.assertIn('"prop": 10', self.source)
        self.assertIn('"static_plant": 4', self.source)
        self.assertIn('"mountain": 1', self.source)

    def test_bridge_gap_validation_measures_generated_edge_output_not_only_inputs(self):
        body = _function_body(self.source, "_validate_live_bridge_gap")
        for token in (
            "edge_actor_labels",
            "edge_spline_count",
            "_road_edge_spline_components",
            "generated_edge_closest_distances_cm",
        ):
            self.assertIn(token, body)

    def test_finalize_begin_and_poll_are_bounded_and_nonblocking(self):
        begin = _function_body(self.source, "begin_finalize")
        poll = _function_body(self.source, "poll_finalize")
        self.assertIn("generate_town_pcg", begin)
        self.assertNotIn("while ", begin)
        self.assertNotIn("sleep(", begin)
        self.assertIn('"state": "pending"', begin)
        self.assertIn("get_town_pcg_status", poll)
        self.assertNotIn("while ", poll)
        self.assertNotIn("sleep(", poll)
        for state in ("pending", "complete", "failed"):
            self.assertIn(f'"state": "{state}"', poll)
        self.assertIn("_generated_instance_count", poll)

    def test_semantic_bridge_river_dock_flipbooks_and_four_cameras_are_explicit(self):
        for label in (
            "CAM_QS_B1_GATE_ARRIVAL", "CAM_QS_B1_TOWN_CORE",
            "CAM_QS_B1_MAIN_BRIDGE", "CAM_QS_B1_SOUTH_DOCK",
        ):
            self.assertIn(label, self.source)
        for token in (
            "QingshanB1BridgeDeck", "QingshanB1BridgeRail",
            "QingshanB1BridgePost", "QingshanB1BridgeApproachStone",
            "QingshanB1River", "QingshanB1Dock",
            "unreal.PaperFlipbookActor", "FB_QS_B1_Plant_Sway",
            "EXPECTED_ANIMATED_PLANT_COUNT = 30",
        ):
            self.assertIn(token, self.source)
        flipbooks = _function_body(self.source, "_create_animated_plants")
        self.assertIn("set_playback_position_in_frames", flipbooks)
        self.assertIn("start_frame", flipbooks)

    def test_live_collision_cull_ground_snap_and_building_bottom_contracts_exist(self):
        for token in (
            "unreal.CollisionEnabled.NO_COLLISION",
            "unreal.CollisionEnabled.QUERY_AND_PHYSICS",
            "set_cull_distances",
            "set_cull_distance",
            "line_trace_single",
            "MAX_BUILDING_BOTTOM_ERROR_CM = 25.0",
            "building_bottom_error_cm",
        ):
            self.assertIn(token, self.source)
        policy = _function_body(self.source, "_apply_generated_component_policy")
        self.assertIn("collision_enabled", policy)
        self.assertIn("cull_end_cm", policy)
        self.assertIn("actors_to_ignore=[actor]", policy)
        primitive_policy = _function_body(self.source, "_set_primitive_policy")
        self.assertIn("component.set_cull_distance", primitive_policy)
        flipbooks = _function_body(self.source, "_create_animated_plants")
        self.assertIn("component.set_cull_distance", flipbooks)
        self.assertNotIn(
            'set_editor_property("cached_max_draw_distance"', self.source
        )

    def test_instance_transform_python_return_is_direct_and_none_is_rejected(self):
        module = _load_module(
            ASSEMBLER_PATH, "_gamexxk_b1_assembler_instance_transform_test"
        )
        sentinel = object()
        self.assertIs(module._require_instance_transform(sentinel, "Volume", 3), sentinel)
        with self.assertRaisesRegex(RuntimeError, "Volume.*3"):
            module._require_instance_transform(None, "Volume", 3)

    def test_generated_component_static_mesh_uses_ue58_editor_property(self):
        module = _load_module(
            ASSEMBLER_PATH, "_gamexxk_b1_assembler_static_mesh_property_test"
        )
        component = mock.Mock()
        mesh = object()
        component.get_editor_property.return_value = mesh
        self.assertIs(
            module._require_component_static_mesh(component, "Volume"), mesh
        )
        component.get_editor_property.assert_called_once_with("static_mesh")
        component.get_editor_property.return_value = None
        with self.assertRaisesRegex(RuntimeError, "Volume.*StaticMesh"):
            module._require_component_static_mesh(component, "Volume")

    def test_line_trace_python_return_is_direct_and_none_is_rejected(self):
        module = _load_module(
            ASSEMBLER_PATH, "_gamexxk_b1_assembler_line_trace_return_test"
        )
        sentinel = object()
        location = mock.Mock(x=10.0, y=20.0, z=30.0)
        self.assertIs(module._require_trace_hit_result(sentinel, location), sentinel)
        with self.assertRaisesRegex(RuntimeError, r"world point.*10\.0.*20\.0.*30\.0"):
            module._require_trace_hit_result(None, location)

        impact_point = mock.Mock(x=1.0, y=2.0, z=3.0)
        hit_result = mock.Mock()
        hit_result.to_dict.return_value = {"impact_point": impact_point}
        self.assertIs(module._impact_point_from_hit_result(hit_result), impact_point)
        hit_result.to_dict.return_value = {}
        with self.assertRaisesRegex(RuntimeError, "impact_point"):
            module._impact_point_from_hit_result(hit_result)

    def test_save_scope_is_only_b1_map_and_b1_assets(self):
        save = _function_body(self.source, "_save_b1_only")
        self.assertIn("_validate_dirty_scope", save)
        self.assertIn("save_current_level", save)
        self.assertIn("save_asset", save)
        self.assertIn("B1_ASSET_ROOT", save)
        self.assertIn("B1_MAP", save)
        self.assertNotIn("save_directory", self.source)
        self.assertNotIn("save_asset(B0R_MAP", self.source)
        self.assertNotIn("save_asset(SOURCE_MAP", self.source)

    def test_pure_bridge_gap_clipping_produces_two_outside_segments(self):
        module = _load_module(ASSEMBLER_PATH, "_gamexxk_b1_assembler_gap_test")
        points = [(-10.0, 0.0, 0.0), (0.0, 0.0, 0.0), (10.0, 0.0, 0.0)]
        result = module._split_polyline_outside_circle(points, (0.0, 0.0, 0.0), 2.0)
        self.assertEqual(len(result), 2)
        self.assertTrue(all(len(segment) >= 2 for segment in result))
        for segment in result:
            for point in segment:
                self.assertGreaterEqual(math.hypot(point[0], point[1]), 2.0 - 1.0e-6)


def _complete_payload(phase: str) -> dict:
    return {
        "success": True, "phase": phase, "state": "complete",
        "complete": True, "pending": False, "error": "",
    }


def _pending_payload(phase: str) -> dict:
    return {
        "success": True, "phase": phase, "state": "pending",
        "complete": False, "pending": True, "error": "",
    }


class FakeClock:
    def __init__(self):
        self.now = 0.0
        self.sleeps = []

    def monotonic(self):
        return self.now

    def sleep(self, seconds):
        self.sleeps.append(seconds)
        self.now += seconds


class FakeClient:
    def __init__(self, payloads, outer_success=True):
        self.payloads = list(payloads)
        self.outer_success = outer_success
        self.calls = []

    def call_tool(self, tool_name, arguments, toolset_name=None, timeout=None):
        argv = json.loads(arguments["argv_json"])
        phase = argv[1]
        self.calls.append((phase, timeout, tool_name, arguments, toolset_name))
        payload = self.payloads.pop(0)
        if isinstance(payload, BaseException):
            raise payload
        if isinstance(payload, str):
            stdout = payload
        else:
            stdout = f"log noise\n{ASSEMBLY_MARKER} {json.dumps(payload, separators=(',', ':'))}\n"
        outer = {
            "success": self.outer_success,
            "relative_path": ASSEMBLY_SCRIPT,
            "argv": argv,
            "run_as_main": True,
            "stdout": stdout,
        }
        return json.dumps(outer, separators=(",", ":"))


class B1RunnerTests(unittest.TestCase):
    def _runner(self):
        return _load_module(RUNNER_PATH, "_gamexxk_b1_runner_test")

    def test_runner_import_is_host_safe(self):
        self.assertTrue(RUNNER_PATH.is_file(), "B1 runner is missing")
        code = (
            "import sys; import scripts.run_qingshan_dress_b1; "
            "assert 'scripts.ue_mcp_client' not in sys.modules; "
            "assert 'ue_mcp_client' not in sys.modules; "
            "assert 'unreal' not in sys.modules"
        )
        completed = subprocess.run(
            [sys.executable, "-c", code], cwd=ROOT,
            capture_output=True, text=True, check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_happy_path_calls_four_phases_and_polls_to_complete(self):
        runner = self._runner()
        final = _complete_payload("finalize_poll")
        final.update(graph_count=34, actual_counts={
            "buildings": 26, "props": 72, "static_plants": 70,
            "mountains": 24, "animated_plants": 30,
        })
        client = FakeClient([
            _complete_payload("setup"),
            _complete_payload("infrastructure"),
            _pending_payload("finalize_begin"),
            _pending_payload("finalize_poll"),
            final,
        ])
        clock = FakeClock()
        result = runner.run_dress_b1(
            client, timeout_seconds=5.0, poll_interval=0.5,
            monotonic=clock.monotonic, sleep=clock.sleep,
        )
        self.assertTrue(result["success"])
        self.assertEqual(result["terminal_state"], "complete")
        self.assertEqual(result["poll_attempts"], 2)
        self.assertEqual([call[0] for call in client.calls], [
            "setup", "infrastructure", "finalize_begin",
            "finalize_poll", "finalize_poll",
        ])
        self.assertEqual(clock.sleeps, [0.5])

    def test_outer_mcp_failure_is_rejected_even_with_successful_inner_marker(self):
        runner = self._runner()
        client = FakeClient([_complete_payload("setup")], outer_success=False)
        result = runner.run_dress_b1(client, timeout_seconds=2.0)
        self.assertFalse(result["success"])
        self.assertEqual(result["terminal_state"], "setup_error")
        self.assertIn("outer", result["error"].lower())

    def test_missing_duplicate_invalid_and_nonfinite_markers_are_rejected(self):
        runner = self._runner()
        valid = json.dumps(_complete_payload("setup"), separators=(",", ":"))
        cases = (
            "plain log without marker",
            f"{ASSEMBLY_MARKER} {valid}\n{ASSEMBLY_MARKER} {valid}",
            f"{ASSEMBLY_MARKER} not-json",
            f'{ASSEMBLY_MARKER} {{"success":true,"phase":"setup","state":"complete",'
            '"complete":true,"pending":false,"value":NaN}',
        )
        for stdout in cases:
            with self.subTest(stdout=stdout):
                client = FakeClient([stdout])
                result = runner.run_dress_b1(client, timeout_seconds=2.0)
                self.assertFalse(result["success"])
                self.assertEqual(result["terminal_state"], "setup_error")

    def test_inner_failure_stops_at_each_nonpoll_phase(self):
        runner = self._runner()
        for failing_phase, prefix in zip(PHASES[:3], range(3)):
            payloads = [_complete_payload(phase) for phase in PHASES[:prefix]]
            payloads.append({
                "success": False, "phase": failing_phase, "state": "failed",
                "complete": False, "pending": False, "error": "inner refused",
            })
            with self.subTest(phase=failing_phase):
                client = FakeClient(payloads)
                result = runner.run_dress_b1(client, timeout_seconds=2.0)
                self.assertFalse(result["success"])
                self.assertIn("inner refused", result["error"])
                self.assertEqual(len(client.calls), prefix + 1)

    def test_phase_mismatch_and_inconsistent_booleans_are_rejected(self):
        runner = self._runner()
        wrong_phase = _complete_payload("infrastructure")
        inconsistent = _complete_payload("setup")
        inconsistent["pending"] = True
        for payload in (wrong_phase, inconsistent):
            with self.subTest(payload=payload):
                result = runner.run_dress_b1(
                    FakeClient([payload]), timeout_seconds=2.0,
                )
                self.assertFalse(result["success"])
                self.assertEqual(result["terminal_state"], "setup_error")

    def test_timeout_clips_sleep_and_never_accepts_late_pending(self):
        runner = self._runner()
        client = FakeClient([
            _complete_payload("setup"), _complete_payload("infrastructure"),
            _pending_payload("finalize_begin"),
            _pending_payload("finalize_poll"), _pending_payload("finalize_poll"),
        ])
        clock = FakeClock()
        result = runner.run_dress_b1(
            client, timeout_seconds=0.75, poll_interval=0.5,
            monotonic=clock.monotonic, sleep=clock.sleep,
        )
        self.assertFalse(result["success"])
        self.assertEqual(result["terminal_state"], "timeout")
        self.assertEqual(clock.sleeps, [0.5, 0.25])

    def test_main_returns_nonzero_for_connection_or_inner_failure(self):
        runner = self._runner()

        class Client:
            endpoint = "fake"

            def connect(self):
                return False

        output = io.StringIO()
        with mock.patch.object(runner, "_load_client_class", return_value=Client):
            with contextlib.redirect_stdout(output):
                code = runner.main(["--assemble-once", "--timeout-seconds", "2"])
        self.assertNotEqual(code, 0)
        payload = json.loads(output.getvalue())
        self.assertFalse(payload["success"])


if __name__ == "__main__":
    unittest.main()
