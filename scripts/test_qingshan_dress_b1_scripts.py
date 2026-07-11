from __future__ import annotations

import ast
import contextlib
import importlib.util
import io
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zlib


ROOT = Path(__file__).resolve().parents[1]
ASSEMBLER_PATH = ROOT / "Content/Python/gamexxk_assemble_qingshan_dress_b1.py"
RUNNER_PATH = ROOT / "scripts/run_qingshan_dress_b1.py"
VALIDATOR_PATH = ROOT / "Content/Python/gamexxk_validate_qingshan_dress_b1.py"
ACCEPTANCE_PATH = ROOT / "Content/Python/gamexxk_qingshan_dress_b1_acceptance.py"
QUICKROAD_AUTOMATION_SOURCE = (
    ROOT / "Plugins/Quick_Road/Source/Quick_Road/Private/Quick_RoadAutomationLibrary.cpp"
)
EDITOR_CAPTURE_HEADER = (
    ROOT / "Source/GameXXKEditor/Public/GameXXKEditorCaptureAutomationLibrary.h"
)
EDITOR_CAPTURE_SOURCE = (
    ROOT / "Source/GameXXKEditor/Private/GameXXKEditorCaptureAutomationLibrary.cpp"
)
EDITOR_BUILD = ROOT / "Source/GameXXKEditor/GameXXKEditor.Build.cs"
MCP_TDD_TOOLSET_PATH = ROOT / "Content/Python/gamexxk_mcp_tdd_toolset.py"
ASSEMBLY_SCRIPT = "Content/Python/gamexxk_assemble_qingshan_dress_b1.py"
ASSEMBLY_MARKER = "GAMEXXK_QINGSHAN_B1_ASSEMBLY"
VALIDATOR_SCRIPT = "Content/Python/gamexxk_validate_qingshan_dress_b1.py"
VALIDATION_MARKER = "GAMEXXK_QINGSHAN_B1_VALIDATION"
ACCEPTANCE_SCRIPT = "Content/Python/gamexxk_qingshan_dress_b1_acceptance.py"
ACCEPTANCE_MARKER = "GAMEXXK_QINGSHAN_B1_ACCEPTANCE"
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

    def test_quickroad_active_layer_is_required_for_assembly_but_optional_for_reload_audit(self):
        body = _function_body(self.source, "_validate_quickroad_audit")
        self.assertIn("require_active: bool = True", body)
        self.assertIn("require_active and", body)
        self.assertIn('audit.get("edit_layer_visible") is not True', body)
        validator_source = VALIDATOR_PATH.read_text(encoding="utf-8")
        validator_quickroad = _function_body(validator_source, "_quickroad_manifest")
        self.assertIn("require_active=False", validator_quickroad)

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

    def test_animated_plants_use_stop_position_play_three_stage_sync(self):
        flipbooks = _function_body(self.source, "_create_animated_plants")
        for token in (
            "pending_playback", "component.stop()",
            "component.set_play_rate(1.0)",
            "for component, start_frame in pending_playback:",
            "component.set_playback_position_in_frames(start_frame, False)",
            "for component, _start_frame in pending_playback:",
            "component.play()",
        ):
            self.assertIn(token, flipbooks)
        self.assertLess(
            flipbooks.index("component.stop()"),
            flipbooks.index("component.set_playback_position_in_frames"),
        )
        self.assertLess(
            flipbooks.index("component.set_playback_position_in_frames"),
            flipbooks.rindex("component.play()"),
        )

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

    def test_semantic_collision_policy_persists_via_explicit_profile_and_readback(self):
        body = _function_body(self.source, "_set_primitive_policy")
        self.assertIn("component.modify()", body)
        self.assertIn("set_collision_profile_name", body)
        self.assertIn('"NoCollision"', body)
        self.assertIn('"BlockAll"', body)
        self.assertIn("get_collision_enabled", body)

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

    def test_bridge_deck_height_includes_live_river_surface_clearance(self):
        module = _load_module(
            ASSEMBLER_PATH, "_gamexxk_b1_assembler_bridge_height_test"
        )
        self.assertEqual(module.BRIDGE_WATER_CLEARANCE_CM, 120.0)
        center_z = module._compute_bridge_deck_center_z(
            approach_ground_z_values=[-340.0, -365.0],
            crossing_water_top_z=5.0,
            deck_half_extent_z=60.0,
        )
        self.assertAlmostEqual(center_z, 185.0)
        self.assertGreaterEqual(
            center_z - 60.0 - 5.0,
            module.BRIDGE_WATER_CLEARANCE_CM,
        )
        create = _function_body(self.source, "_create_bridge_assembly")
        self.assertIn("_river_surface_top_world_z_at_local_point", create)
        self.assertIn("_compute_bridge_deck_center_z", create)
        bridge_river = _function_body(
            (VALIDATOR_PATH.read_text(encoding="utf-8") if VALIDATOR_PATH.is_file() else ""),
            "_bridge_river_manifest",
        )
        self.assertIn("crossing_candidates", bridge_river)
        self.assertIn("max(", bridge_river)


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


def _png_chunk(chunk_type: bytes, payload: bytes) -> bytes:
    checksum = zlib.crc32(chunk_type + payload) & 0xFFFFFFFF
    return (
        len(payload).to_bytes(4, "big") + chunk_type + payload
        + checksum.to_bytes(4, "big")
    )


def _valid_rgba_png(width: int, height: int) -> bytes:
    ihdr = (
        int(width).to_bytes(4, "big") + int(height).to_bytes(4, "big")
        + b"\x08\x06\x00\x00\x00"
    )
    scanline = b"\x00" + b"\x00" * (int(width) * 4)
    pixels = scanline * int(height)
    return (
        b"\x89PNG\r\n\x1a\n"
        + _png_chunk(b"IHDR", ihdr)
        + _png_chunk(b"IDAT", zlib.compress(pixels, 9))
        + _png_chunk(b"IEND", b"")
    )


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


class B1ValidatorSourceContractTests(unittest.TestCase):
    def setUp(self):
        self.source = (
            VALIDATOR_PATH.read_text(encoding="utf-8")
            if VALIDATOR_PATH.is_file() else ""
        )

    def test_validator_exists_parses_and_is_host_importable(self):
        self.assertTrue(VALIDATOR_PATH.is_file(), "B1 live validator is missing")
        ast.parse(self.source)
        code = (
            "import sys; import gamexxk_validate_qingshan_dress_b1; "
            "assert 'unreal' not in sys.modules"
        )
        completed = subprocess.run(
            [sys.executable, "-c", code], cwd=ROOT / "Content/Python",
            capture_output=True, text=True, check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_validator_is_read_only_and_emits_unique_validation_marker(self):
        self.assertIn(f'VALIDATION_MARKER = "{VALIDATION_MARKER}"', self.source)
        tree = ast.parse(self.source)
        forbidden = {
            "save_asset", "save_current_level", "save_dirty_packages",
            "delete_asset", "delete_directory", "destroy_actor", "load_map",
            "spawn_actor_from_class", "generate_town_pcg", "clear_town_pcg",
            "set_actor_location", "set_actor_rotation", "set_actor_scale3d",
            "set_editor_property", "set_collision_enabled", "set_cull_distances",
        }
        observed = set()
        for call in (node for node in ast.walk(tree) if isinstance(node, ast.Call)):
            if isinstance(call.func, ast.Attribute):
                observed.add(call.func.attr)
            elif isinstance(call.func, ast.Name):
                observed.add(call.func.id)
        self.assertFalse(observed & forbidden, observed & forbidden)
        self.assertIn("def main()", self.source)
        self.assertIn("VALIDATION_MARKER", _function_body(self.source, "main"))

    def test_validator_manifest_contract_covers_every_live_gate(self):
        for token in (
            'EXPECTED_COUNTS = {', '"buildings": 26', '"props": 72',
            '"static_plants": 70', '"mountains": 24',
            '"animated_plants": 30', 'EXPECTED_ARCHETYPE_COUNT = 6',
            'EXPECTED_ROOF_PALETTE_COUNT = 4', 'EXPECTED_LANDSCAPE_RESOLUTION = 505',
            'EXPECTED_LANDSCAPE_COMPONENT_COUNT = 16', 'EXPECTED_CAMERA_COUNT = 4',
            '"graph_path"', '"base_seed"', '"component_seed"',
            '"point_seed_sha256"', '"generated_component_count"',
            '"instance_count"', '"consumed_edge_labels"',
            '"consumed_edge_digest"', '"mesh_path"', '"material_paths"',
            '"collision_enabled"', '"cull_distances_cm"',
            '"source_records"', '"road_triangle_count"',
            '"intersection_patch_count"', '"edge_actor_labels"',
            '"edge_geometry_sha256"', '"edit_layer_delta_sha256"',
            '"decoded_sample_elevations_cm"', '"base_height_u16_sha256"',
            '"building_bottom_error_cm"', '"protected_file_hashes"',
            '"actor_count"', '"component_count"', '"tick_enabled_count"',
            '"live_scene_manifest_sha256"', '"bridge_river"',
        ):
            with self.subTest(token=token):
                self.assertIn(token, self.source)
        for digest in (
            "a3639b38623d00e8ad3e5a610a3e1695a47b38c1d1e6fedb8115e1e9fdf5c8a8",
            "74292340df0cea97d99e905dd193a921038326bfec2f3ce034a5e9f70bd3f107",
            "3f231876d0083bffe28ee555b60af1a20b0966edbde9dc4bb6f2647e920eadb1",
        ):
            self.assertIn(digest, self.source)

    def test_canonical_live_hash_is_sorted_compact_and_rejects_nonfinite(self):
        module = _load_module(VALIDATOR_PATH, "_gamexxk_b1_validator_hash_test")
        first = {"z": [2, 1], "a": {"y": 2, "x": 1}}
        second = {"a": {"x": 1, "y": 2}, "z": [2, 1]}
        self.assertEqual(
            module.canonical_manifest_sha256(first),
            module.canonical_manifest_sha256(second),
        )
        encoded = module.canonical_manifest_bytes(first)
        self.assertNotIn(b" ", encoded)
        self.assertNotIn(b"\n", encoded)
        with self.assertRaises((ValueError, TypeError)):
            module.canonical_manifest_sha256({"bad": math.nan})

    def test_approved_505_heightmap_is_decoded_without_external_packages(self):
        module = _load_module(VALIDATOR_PATH, "_gamexxk_b1_validator_height_test")
        path = ROOT / "Content/ArtSource/Qingshan/B1/H_QS_B1_Terrain_505.png"
        decoded = module.decode_png16_grayscale(path)
        self.assertEqual((decoded["width"], decoded["height"]), (505, 505))
        self.assertEqual(len(decoded["values"]), 505 * 505)
        self.assertEqual(len(decoded["u16_sha256"]), 64)
        self.assertTrue(all(0 <= value <= 65535 for value in decoded["values"]))

    def test_validator_uses_ue58_direct_trace_and_static_mesh_property_contracts(self):
        trace = _function_body(self.source, "_trace_ground")
        self.assertIn("result = unreal.SystemLibrary.line_trace_single", trace)
        self.assertIn("result is None", trace)
        self.assertIn("result.to_dict()", trace)
        self.assertIn('"impact_point"', trace)
        self.assertNotIn("hit, result", trace)
        components = _function_body(self.source, "_pcg_group_components")
        self.assertIn('_read_property(component, "static_mesh")', components)
        self.assertNotIn("component.get_static_mesh()", components)
        self.assertIn("transform is None", components)
        animated = _function_body(self.source, "_animated_plant_manifest")
        phase_snapshot = _function_body(
            self.source, "_animated_plant_phase_snapshot")
        self.assertIn("get_playback_position()", phase_snapshot)
        self.assertIn("component.get_flipbook_framerate()", animated)
        self.assertNotIn("flipbook.get_frames_per_second()", animated)
        self.assertIn("frames_per_second - 5.0", animated)
        self.assertIn("frame_count != 6", animated)
        self.assertIn("live_relative_phase_delta", animated)
        self.assertIn("live Paper2D relative phase mismatch", animated)

    def test_animated_plant_phase_is_snapshotted_in_one_tight_batch(self):
        animated = _function_body(self.source, "_animated_plant_manifest")
        snapshot = _function_body(self.source, "_animated_plant_phase_snapshot")
        self.assertIn("get_playback_position()", snapshot)
        self.assertIn("actor_components", snapshot)
        self.assertNotIn("get_playback_position_in_frames", self.source)
        snapshot_index = animated.index("_animated_plant_phase_snapshot")
        slow_loop_index = animated.index("for record in records:")
        self.assertLess(snapshot_index, slow_loop_index)
        self.assertIn('observed_seconds = float(phase_snapshot[label])', animated)
        self.assertIn("_circular_phase_error_frames", animated)
        self.assertIn("phase_error_frames > 0.25", animated)
        self.assertIn("component.get_play_rate()", animated)
        self.assertIn("component.is_reversing()", animated)

    def test_continuous_phase_math_survives_float32_truncation_boundary(self):
        module = _load_module(VALIDATOR_PATH, "_gamexxk_b1_phase_math_test")
        reference_seconds = 0.20000002
        observed_seconds = 0.59999996
        truncated_delta = (
            int(observed_seconds * 5.0) - int(reference_seconds * 5.0)
        ) % 6
        self.assertEqual(truncated_delta, 1)
        error = module._circular_phase_error_frames(
            reference_seconds=reference_seconds,
            observed_seconds=observed_seconds,
            configured_delta_frames=2,
            frame_count=6,
            frames_per_second=5.0,
        )
        self.assertLess(error, 0.001)
        self.assertGreater(module._circular_phase_error_frames(
            reference_seconds=reference_seconds,
            observed_seconds=0.54,
            configured_delta_frames=2,
            frame_count=6,
            frames_per_second=5.0,
        ), 0.25)

    def test_validator_requires_full_live_landscape_base_and_merged_height_digests(self):
        quickroad_source = QUICKROAD_AUTOMATION_SOURCE.read_text(encoding="utf-8")
        for token in (
            'TEXT("base_height_u16_sha256")',
            'TEXT("merged_height_u16_sha256")',
            "TArray<uint16> MergedPacked",
            "Landscape, FGuid(), MergedMinX",
            "LandscapeDataAccess::GetLocalHeight(MergedPacked[Index])",
            "LandscapeDataAccess::GetLocalHeight(RawDelta[Index])",
        ):
            self.assertIn(token, quickroad_source)
        landscape = _function_body(self.source, "_landscape_manifest")
        self.assertIn("base_height_u16_sha256", landscape)
        self.assertIn("merged_height_u16_sha256", landscape)
        self.assertIn("source_base_height_u16_sha256", landscape)
        self.assertIn("live Landscape base height digest mismatch", landscape)

    def test_landscape_topology_comes_from_live_ue58_quickroad_audit(self):
        quickroad_source = QUICKROAD_AUTOMATION_SOURCE.read_text(encoding="utf-8")
        for token in (
            'TEXT("landscape_component_count")',
            'TEXT("component_size_quads")',
            'TEXT("num_subsections")',
            'TEXT("subsection_size_quads")',
            'TEXT("height_bounds")',
            "Landscape->ComponentSizeQuads",
            "Landscape->NumSubsections",
            "Landscape->SubsectionSizeQuads",
        ):
            self.assertIn(token, quickroad_source)
        quickroad = _function_body(self.source, "_quickroad_manifest")
        for field in (
            "landscape_component_count", "component_size_quads",
            "num_subsections", "subsection_size_quads", "height_bounds",
        ):
            self.assertIn(field, quickroad)
        topology = _function_body(self.source, "_landscape_topology")
        self.assertIn('quickroad["component_size_quads"]', topology)
        self.assertIn('quickroad["num_subsections"]', topology)
        self.assertIn('quickroad["subsection_size_quads"]', topology)
        self.assertIn('quickroad["height_bounds"]', topology)
        self.assertNotIn('_read_property(landscape, "component_size_quads")', topology)

    def test_landscape_heightmap_source_comes_from_live_ue58_quickroad_audit(self):
        quickroad_source = QUICKROAD_AUTOMATION_SOURCE.read_text(encoding="utf-8")
        self.assertIn('TEXT("heightmap_source_path")', quickroad_source)
        self.assertIn("Landscape->ReimportHeightmapFilePath", quickroad_source)
        quickroad = _function_body(self.source, "_quickroad_manifest")
        self.assertIn('raw["heightmap_source_path"]', quickroad)
        self.assertIn("_project_relative_manifest_path", quickroad)
        landscape = _function_body(self.source, "_landscape_manifest")
        self.assertIn('quickroad["heightmap_source_path"]', landscape)
        self.assertIn("PROJECT_ROOT", landscape)
        self.assertIn("_project_relative_manifest_path", landscape)
        self.assertNotIn(
            '_read_property(landscape, "reimport_heightmap_file_path")', landscape)

    def test_canonical_heightmap_source_is_project_relative_and_cannot_escape(self):
        module = _load_module(VALIDATOR_PATH, "_gamexxk_b1_validator_source_path_test")
        relative = module._project_relative_manifest_path(
            "Content/ArtSource/Qingshan/B1/H_QS_B1_Terrain_505.png")
        self.assertEqual(
            relative, "Content/ArtSource/Qingshan/B1/H_QS_B1_Terrain_505.png")
        encoded = module.canonical_manifest_bytes({
            "quickroad": {"heightmap_source_path": relative},
        })
        self.assertNotIn(str(ROOT).encode("utf-8"), encoded)
        self.assertIn(relative.encode("utf-8"), encoded)
        with self.assertRaises(ValueError):
            module._project_relative_manifest_path(str(ROOT / "absolute.png"))
        with self.assertRaises(ValueError):
            module._project_relative_manifest_path("../outside.png")


class B1AcceptanceSourceContractTests(unittest.TestCase):
    def setUp(self):
        self.source = (
            ACCEPTANCE_PATH.read_text(encoding="utf-8")
            if ACCEPTANCE_PATH.is_file() else ""
        )

    def test_acceptance_script_exists_parses_and_has_two_bounded_actions(self):
        self.assertTrue(ACCEPTANCE_PATH.is_file(), "B1 UE acceptance script is missing")
        ast.parse(self.source)
        self.assertIn(f'ACCEPTANCE_MARKER = "{ACCEPTANCE_MARKER}"', self.source)
        for token in (
            'ACTION_CAPTURE = "capture"',
            'ACTION_PERFORMANCE_SAMPLE = "performance_sample"',
            '"--camera-label"', '"--output-path"',
            'take_high_res_screenshot', '1920', '1080',
            'unreal.CameraActor', 'get_world_delta_seconds', 'get_time_seconds',
            '"actor_count"', '"component_count"', '"tick_enabled_count"',
        ):
            self.assertIn(token, self.source)

    def test_performance_sample_uses_ue58_supported_timing_apis(self):
        body = _function_body(self.source, "performance_sample")
        self.assertIn("GameplayStatics.get_world_delta_seconds(world)", body)
        self.assertIn("SystemLibrary.get_platform_time_seconds()", body)
        self.assertNotIn("unreal.App", body)

    def test_capture_begin_prepares_and_audits_internal_level_viewport(self):
        self.assertTrue(EDITOR_CAPTURE_HEADER.is_file())
        self.assertTrue(EDITOR_CAPTURE_SOURCE.is_file())
        header = EDITOR_CAPTURE_HEADER.read_text(encoding="utf-8")
        source = EDITOR_CAPTURE_SOURCE.read_text(encoding="utf-8")
        build = EDITOR_BUILD.read_text(encoding="utf-8")
        self.assertIn("UGameXXKEditorCaptureAutomationLibrary", header)
        self.assertIn("PrepareLevelViewportForCapture", header)
        for token in (
            "GetLevelEditorTab", "ActivateInParent", "FocusViewport",
            "SetKeyboardFocusToThisViewport", "IsForeground",
            "GetActiveTopLevelWindow", "GetSizeXY", "InvalidateDisplay",
            'TEXT("viewport_width")', 'TEXT("viewport_height")',
            'TEXT("success")', 'TEXT("error")',
        ):
            self.assertIn(token, source)
        for dependency in ('"LevelEditor"', '"Slate"', '"SlateCore"'):
            self.assertIn(dependency, build)
        capture = _function_body(self.source, "capture_camera")
        prepare_index = capture.index("_prepare_level_viewport_for_capture()")
        screenshot_index = capture.index("take_high_res_screenshot")
        self.assertLess(prepare_index, screenshot_index)

    def test_each_capture_action_accepts_exactly_one_named_camera(self):
        body = _function_body(self.source, "capture_camera")
        self.assertIn("camera_label", body)
        self.assertIn("_find_unique_camera", body)
        self.assertIn("take_high_res_screenshot", body)
        self.assertNotIn("for camera", body)
        self.assertNotIn("save_", self.source)
        self.assertNotIn("delete_", self.source)

    def test_capture_task_store_survives_fresh_script_globals(self):
        first_module = _load_module(ACCEPTANCE_PATH, "_gamexxk_b1_acceptance_state_test_a")
        first = first_module._capture_tasks()
        first["sentinel"] = {"task": object()}
        try:
            second_module = _load_module(ACCEPTANCE_PATH, "_gamexxk_b1_acceptance_state_test_b")
            self.assertIs(second_module._capture_tasks(), first)
            self.assertIn("sentinel", second_module._capture_tasks())
            self.assertIn('CAPTURE_ABORT = "abort"', self.source)
            self.assertIn("tasks.pop(camera_label, None)", self.source)
            first["expired"] = {"requested_wall_ns": 1}
            self.assertTrue(second_module._release_expired_capture(
                "expired", second_module.CAPTURE_TASK_EXPIRY_NS + 1,
            ))
            self.assertNotIn("expired", first)
        finally:
            first.clear()

    def test_abort_keeps_uncancellable_pending_task_until_done_or_invalid(self):
        module = _load_module(
            ACCEPTANCE_PATH, "_gamexxk_b1_acceptance_uncancellable_task_test")

        class Task:
            def __init__(self):
                self.valid = True
                self.done = False

            def is_valid_task(self):
                return self.valid

            def is_task_done(self):
                return self.done

        label = "CAM_QS_B1_GATE_ARRIVAL"
        task = Task()
        tasks = module._capture_tasks()
        tasks[label] = {"task": task, "requested_wall_ns": 1, "path": "probe.png"}
        try:
            pending = module._abort_capture_task(label)
            self.assertIs(pending["released"], False)
            self.assertIs(pending["pending"], True)
            self.assertIn(label, tasks)
            self.assertFalse(module._release_expired_capture(
                label, module.CAPTURE_TASK_EXPIRY_NS + 1))
            self.assertIn(label, tasks)

            task.done = True
            terminal = module._abort_capture_task(label)
            self.assertIs(terminal["released"], True)
            self.assertIs(terminal["pending"], False)
            self.assertNotIn(label, tasks)
        finally:
            tasks.pop(label, None)


class B1AcceptanceRunnerContractTests(unittest.TestCase):
    def _runner(self):
        return _load_module(RUNNER_PATH, "_gamexxk_b1_acceptance_runner_test")

    @staticmethod
    def _outer(relative_path, argv, marker, payload, *, outer_success=True):
        stdout = f"noise\n{marker} {json.dumps(payload, separators=(',', ':'))}\n"
        return json.dumps({
            "success": outer_success,
            "relative_path": relative_path,
            "argv": argv,
            "run_as_main": True,
            "stdout": stdout,
        }, separators=(",", ":"))

    def test_marked_script_gate_rejects_outer_missing_marker_and_inner_failure(self):
        runner = self._runner()
        argv = []

        class Client:
            def __init__(self, response):
                self.response = response

            def call_tool(self, *args, **kwargs):
                return self.response

        valid = {"success": True, "error": "", "live_scene_manifest_sha256": "a" * 64}
        cases = (
            self._outer(VALIDATOR_SCRIPT, argv, VALIDATION_MARKER, valid, outer_success=False),
            json.dumps({
                "success": True, "relative_path": VALIDATOR_SCRIPT, "argv": argv,
                "run_as_main": True, "stdout": "no marker",
            }),
            self._outer(
                VALIDATOR_SCRIPT, argv, VALIDATION_MARKER,
                {"success": False, "error": "live validation failed"},
            ),
            self._outer(
                VALIDATOR_SCRIPT, argv, VALIDATION_MARKER,
                {"success": True, "live_scene_manifest_sha256": "a" * 64},
            ),
        )
        for response in cases:
            with self.subTest(response=response):
                with self.assertRaises(RuntimeError):
                    runner._call_marked_project_script(
                        Client(response), relative_path=VALIDATOR_SCRIPT,
                        argv=argv, marker=VALIDATION_MARKER, timeout_seconds=2.0,
                    )

    def test_editor_memory_probe_uses_typed_64bit_win32_handles(self):
        source = MCP_TDD_TOOLSET_PATH.read_text(encoding="utf-8")
        body = _function_body(source, "_get_process_working_set_mb")
        for token in (
            'ctypes.WinDLL("kernel32", use_last_error=True)',
            'ctypes.WinDLL("psapi", use_last_error=True)',
            "GetCurrentProcess.restype = ctypes.c_void_p",
            "GetProcessMemoryInfo.argtypes",
            "GetProcessMemoryInfo.restype = ctypes.c_int",
            "ctypes.get_last_error()",
            "raise OSError",
        ):
            self.assertIn(token, body)

    def test_capture_wait_rejects_stale_file_and_accepts_fresh_1920x1080_png(self):
        runner = self._runner()
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "capture.png"
            path.write_bytes(_valid_rgba_png(1920, 1080))
            os.utime(path, ns=(1_000_000_000, 1_000_000_000))
            clock = FakeClock()
            with self.assertRaises(TimeoutError):
                runner._wait_for_fresh_capture(
                    path, not_before_ns=2_000_000_000, timeout_seconds=0.25,
                    monotonic=clock.monotonic, sleep=clock.sleep,
                )
            os.utime(path, ns=(3_000_000_000, 3_000_000_000))
            metadata = runner._wait_for_fresh_capture(
                path, not_before_ns=2_000_000_000, timeout_seconds=0.25,
                monotonic=lambda: 0.0, sleep=lambda _: None,
            )
            self.assertEqual(metadata["dimensions"], [1920, 1080])
            self.assertGreater(metadata["size_bytes"], 0)

            path.write_bytes(_valid_rgba_png(1920, 1080)[:-4])
            os.utime(path, ns=(4_000_000_000, 4_000_000_000))
            with self.assertRaises(RuntimeError):
                runner._wait_for_fresh_capture(
                    path, not_before_ns=2_000_000_000, timeout_seconds=0.25,
                    monotonic=lambda: 0.0, sleep=lambda _: None,
                )

    def test_capture_runner_rejects_preexisting_stale_evidence_without_mcp_call(self):
        runner = self._runner()

        class Client:
            def __init__(self):
                self.calls = 0

            def call_tool(self, *args, **kwargs):
                self.calls += 1
                raise AssertionError("stale capture must fail before MCP")

        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "stale.png"
            path.write_bytes(_valid_rgba_png(1920, 1080))
            client = Client()
            with self.assertRaisesRegex(RuntimeError, "stale"):
                runner._capture_one_camera(
                    client,
                    camera_label="CAM_QS_B1_GATE_ARRIVAL",
                    path=path,
                    timeout_seconds=1.0,
                    monotonic=lambda: 0.0,
                    wall_time_ns=lambda: 1,
                    sleep=lambda _: None,
                )
            self.assertEqual(client.calls, 0)

    def test_full_acceptance_rejects_any_formal_output_before_assembly_or_write(self):
        runner = self._runner()
        formal_names = (
            "live-manifest-run-1.json",
            "live-manifest-run-2.json",
            "captures.json",
            "camera-gate-arrival.png",
            "camera-town-core.png",
            "camera-main-bridge.png",
            "camera-south-dock.png",
            "performance.json",
            "acceptance-summary.json",
            "qingshan-b1-performance.csv",
            "performance-foreground.json",
            "acceptance-summary-final.json",
            "qingshan-b1-performance-foreground.csv",
        )

        with tempfile.TemporaryDirectory() as temp_dir:
            for index, name in enumerate(formal_names):
                with self.subTest(name=name):
                    root = (Path(temp_dir) / str(index)).resolve()
                    root.mkdir(parents=True)
                    existing = root / name
                    existing.write_bytes(("immutable:" + name).encode("utf-8"))
                    before = {
                        path.relative_to(root): path.read_bytes()
                        for path in root.rglob("*") if path.is_file()
                    }
                    with (
                        mock.patch.object(runner, "EVIDENCE_ROOT", root),
                        mock.patch.object(
                            runner, "run_dress_b1",
                            side_effect=AssertionError("assembly must not start"),
                        ) as assemble,
                        mock.patch.object(
                            runner, "_call_marked_project_script",
                            side_effect=AssertionError("MCP must not be called"),
                        ) as mcp_call,
                        mock.patch.object(
                            runner, "_write_json",
                            side_effect=AssertionError("evidence must not be written"),
                        ) as write_json,
                    ):
                        with self.assertRaisesRegex(RuntimeError, "formal.*already exist"):
                            runner.run_b1_acceptance(object(), evidence_dir=root)
                    assemble.assert_not_called()
                    mcp_call.assert_not_called()
                    write_json.assert_not_called()
                    after = {
                        path.relative_to(root): path.read_bytes()
                        for path in root.rglob("*") if path.is_file()
                    }
                    self.assertEqual(after, before)

    def test_full_acceptance_stale_gate_runs_before_client_connection(self):
        runner = self._runner()
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir).resolve()
            existing = root / "live-manifest-run-1.json"
            existing.write_bytes(b"immutable formal manifest")
            before = existing.read_bytes()
            with (
                mock.patch.object(runner, "EVIDENCE_ROOT", root),
                mock.patch.object(
                    runner, "_load_client_class",
                    side_effect=AssertionError("client must not be loaded"),
                ) as load_client,
            ):
                self.assertEqual(runner.main(["--acceptance"]), 1)
            load_client.assert_not_called()
            self.assertEqual(existing.read_bytes(), before)

    def test_capture_runner_focuses_verifies_and_restores_unreal_editor_window(self):
        runner = self._runner()

        class FakeWin32:
            def __init__(self, *, focus_succeeds=True):
                self.foreground = 101
                self.focus_succeeds = focus_succeeds
                self.events = []

            def get_foreground_window(self):
                return self.foreground

            def visible_unreal_editor_windows(self):
                return [202]

            def restore_window(self, handle):
                self.events.append(("restore", handle))
                return True

            def set_foreground_window(self, handle):
                self.events.append(("foreground", handle))
                if handle == 202 and not self.focus_succeeds:
                    return False
                self.foreground = handle
                return True

            def is_window(self, handle):
                return handle in (101, 202)

        backend = FakeWin32()
        with self.assertRaisesRegex(RuntimeError, "primary capture failure"):
            with runner._foreground_unreal_editor_window(
                    backend, sleep=lambda _seconds: None):
                self.assertEqual(backend.foreground, 202)
                raise RuntimeError("primary capture failure")
        self.assertEqual(backend.foreground, 101)
        self.assertEqual(backend.events, [
            ("restore", 202), ("foreground", 202), ("foreground", 101),
        ])

        with self.assertRaisesRegex(RuntimeError, "foreground"):
            with runner._foreground_unreal_editor_window(
                    FakeWin32(focus_succeeds=False), sleep=lambda _seconds: None):
                self.fail("fail-closed focus gate must not enter capture")

        acceptance = _function_body(
            RUNNER_PATH.read_text(encoding="utf-8"), "run_b1_acceptance")
        focus_index = acceptance.index("with _foreground_unreal_editor_window():")
        capture_index = acceptance.index("_capture_one_camera(")
        self.assertLess(focus_index, capture_index)

    def test_foreground_gate_waits_for_async_win32_activation_before_verifying(self):
        runner = self._runner()
        clock = FakeClock()

        class DelayedWin32:
            def __init__(self):
                self.foreground = 101
                self.pending = None
                self.sleep_calls = 0

            def get_foreground_window(self):
                return self.foreground

            def visible_unreal_editor_windows(self):
                return [202]

            def restore_window(self, _handle):
                return True

            def set_foreground_window(self, handle):
                self.pending = handle
                return True

            def is_window(self, handle):
                return handle in (101, 202)

            def sleep(self, seconds):
                self.sleep_calls += 1
                clock.sleep(seconds)
                if self.sleep_calls % 3 == 0:
                    self.foreground = self.pending

        backend = DelayedWin32()
        with runner._foreground_unreal_editor_window(
                backend, monotonic=clock.monotonic, sleep=backend.sleep):
            self.assertEqual(backend.foreground, 202)
        self.assertEqual(backend.foreground, 101)
        self.assertEqual(backend.sleep_calls, 6)

        never_clock = FakeClock()
        never = DelayedWin32()
        never.sleep = never_clock.sleep
        with self.assertRaisesRegex(RuntimeError, "did not become foreground"):
            with runner._foreground_unreal_editor_window(
                    never, monotonic=never_clock.monotonic,
                    sleep=never_clock.sleep):
                self.fail("foreground timeout must fail closed")

    def test_foreground_restore_retries_transient_setforegroundwindow_failure(self):
        runner = self._runner()
        clock = FakeClock()

        class TransientWin32:
            def __init__(self):
                self.foreground = 101
                self.restore_attempts = 0

            def get_foreground_window(self):
                return self.foreground

            def visible_unreal_editor_windows(self):
                return [202]

            def restore_window(self, _handle):
                return True

            def set_foreground_window(self, handle):
                if handle == 101:
                    self.restore_attempts += 1
                    if self.restore_attempts < 3:
                        return False
                self.foreground = handle
                return True

            def is_window(self, handle):
                return handle in (101, 202)

        backend = TransientWin32()
        with runner._foreground_unreal_editor_window(
                backend, monotonic=clock.monotonic, sleep=clock.sleep):
            self.assertEqual(backend.foreground, 202)
        self.assertEqual(backend.foreground, 101)
        self.assertEqual(backend.restore_attempts, 3)

    def test_partial_pie_start_failure_runs_unconditional_cleanup_without_masking_error(self):
        runner = self._runner()

        class Client:
            def __init__(self):
                self.is_pie_calls = 0
                self.commands = []
                self.stop_calls = 0

            def is_in_pie(self):
                self.is_pie_calls += 1
                return self.is_pie_calls > 1

            def start_pie(self, **_kwargs):
                raise RuntimeError("primary start failure")

            def execute_console_command(self, command):
                self.commands.append(command)

            def stop_pie(self):
                self.stop_calls += 1

        client = Client()
        with tempfile.TemporaryDirectory() as temp_dir:
            with self.assertRaisesRegex(RuntimeError, "primary start failure"):
                runner._collect_performance_evidence(
                    client,
                    evidence_dir=Path(temp_dir),
                    duration_seconds=5.0,
                    monotonic=lambda: 0.0,
                    wall_time_ns=lambda: 1,
                    sleep=lambda _: None,
                )
        self.assertIn("csvprofile stop", client.commands)
        self.assertIn("stat none", client.commands)
        self.assertEqual(client.stop_calls, 1)

    def test_fresh_csv_gate_rejects_multiple_candidates(self):
        runner = self._runner()
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            first = root / "first.csv"
            second = root / "second.csv"
            first.write_text("FrameTime\n16\n", encoding="utf-8")
            second.write_text("FrameTime\n17\n", encoding="utf-8")
            now = 10_000_000_000
            os.utime(first, ns=(now, now))
            os.utime(second, ns=(now, now))
            with mock.patch.object(runner, "CSV_PROFILE_ROOT", root):
                with self.assertRaisesRegex(RuntimeError, "ambiguous"):
                    runner._wait_for_fresh_csv(
                        {}, not_before_ns=1, timeout_seconds=1.0,
                        monotonic=lambda: 0.0, sleep=lambda _: None,
                    )

    def test_pie_world_package_normalization_is_exact(self):
        module = _load_module(ACCEPTANCE_PATH, "_gamexxk_b1_acceptance_pie_map_test")

        class Package:
            def get_name(self):
                return "/Game/GameXXK/Maps/Dev/UEDPIE_3_L_Qingshan_PCG_Dress_B1"

        class World:
            def get_outermost(self):
                return Package()

        self.assertEqual(
            module._normalized_world_package(World()),
            "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1",
        )

    def test_two_run_manifest_digest_must_be_equal_lowercase_sha256(self):
        runner = self._runner()
        digest = "a" * 64
        self.assertEqual(runner._require_matching_manifest_hashes([digest, digest]), digest)
        for values in ([digest, "b" * 64], [digest], ["not-a-sha", "not-a-sha"]):
            with self.subTest(values=values):
                with self.assertRaises(RuntimeError):
                    runner._require_matching_manifest_hashes(values)

    def test_resume_manifest_normalizes_only_transient_edit_layer_activation(self):
        runner = self._runner()
        first = {
            "quickroad": {"edit_layer_active": True, "digest": "same"},
            "landscape": {"edit_layer_active": True, "height": "same"},
            "population_counts": {"buildings": 26},
        }
        second = json.loads(json.dumps(first))
        second["quickroad"]["edit_layer_active"] = False
        second["landscape"]["edit_layer_active"] = False
        self.assertEqual(
            runner._stable_resume_manifest(first),
            runner._stable_resume_manifest(second),
        )
        second["landscape"]["height"] = "drift"
        self.assertNotEqual(
            runner._stable_resume_manifest(first),
            runner._stable_resume_manifest(second),
        )

    def test_resume_requires_exact_b1_world_and_current_level_packages(self):
        runner = self._runner()
        exact = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1"
        runner._require_exact_b1_map_manifest({
            "map": {"world_package": exact, "current_level_package": exact},
        })
        for manifest in (
            {"map": exact},
            {"map": {"world_package": exact, "current_level_package": "/Game/Other"}},
            {"map": {"world_package": "/Game/Other", "current_level_package": exact}},
        ):
            with self.subTest(manifest=manifest):
                with self.assertRaises(RuntimeError):
                    runner._require_exact_b1_map_manifest(manifest)

    def test_resume_capture_loader_revalidates_every_existing_png_and_preflight(self):
        runner = self._runner()
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir).resolve()
            records = []
            for index, label in enumerate(runner.CAMERA_LABELS):
                path = root / runner.CAPTURE_FILENAMES[label]
                path.write_bytes(_valid_rgba_png(1920, 1080))
                # Windows file times use 100 ns ticks.  Keep the fixture on
                # that boundary so the strict mtime_ns equality check tests
                # drift detection rather than filesystem rounding.
                timestamp = 10_000_000_000 + index * 100
                os.utime(path, ns=(timestamp, timestamp))
                data = path.read_bytes()
                import hashlib
                records.append({
                    "camera_label": label,
                    "path": str(path),
                    "dimensions": [1920, 1080],
                    "size_bytes": len(data),
                    "mtime_ns": timestamp,
                    "sha256": hashlib.sha256(data).hexdigest(),
                    "viewport_preflight": {
                        "success": True, "error": "", "invalidated": True,
                        "level_editor_tab_foreground": True,
                        "level_editor_tab_visible": True,
                        "slate_window_active": True, "viewport_visible": True,
                        "viewport_focused": True,
                        "viewport_width": 100, "viewport_height": 100,
                    },
                })
            (root / "captures.json").write_text(
                json.dumps(records), encoding="utf-8")
            with mock.patch.object(runner, "EVIDENCE_ROOT", root):
                loaded = runner._load_existing_capture_evidence(root)
                self.assertEqual([item["camera_label"] for item in loaded], list(
                    runner.CAMERA_LABELS))
                (root / runner.CAPTURE_FILENAMES[runner.CAMERA_LABELS[0]]).write_bytes(
                    b"tampered")
                with self.assertRaises(RuntimeError):
                    runner._load_existing_capture_evidence(root)

    def test_resume_performance_mode_never_reassembles_or_recaptures(self):
        source = RUNNER_PATH.read_text(encoding="utf-8")
        self.assertIn('mode.add_argument("--resume-performance"', source)
        body = _function_body(source, "resume_b1_performance")
        manifest_loader = _function_body(source, "_load_existing_manifest_evidence")
        for token in (
            "_load_existing_capture_evidence", "_collect_performance_evidence",
            "performance.json", "acceptance-summary.json",
        ):
            self.assertIn(token, body)
        for token in ("live-manifest-run-1.json", "live-manifest-run-2.json"):
            self.assertIn(token, manifest_loader)
        self.assertNotIn("run_dress_b1", body)
        self.assertNotIn("_capture_one_camera", body)

    def test_every_performance_window_holds_unreal_editor_foreground(self):
        source = RUNNER_PATH.read_text(encoding="utf-8")
        for function_name in ("run_b1_acceptance", "resume_b1_performance"):
            with self.subTest(function_name=function_name):
                body = _function_body(source, function_name)
                self.assertIn(
                    "with _foreground_unreal_editor_window():\n"
                    "        performance = _collect_performance_evidence(",
                    body,
                )

    def test_foreground_performance_gate_rejects_throttled_samples(self):
        runner = self._runner()
        accepted = runner._require_unthrottled_performance(
            [12.0, 16.0, 20.0],
            {"median_frame_time_ms": 17.0, "average_fps": 58.0},
            limit_ms=100.0,
        )
        self.assertEqual(accepted["limit_ms"], 100.0)
        self.assertLess(accepted["sample_median_frame_time_ms"], 100.0)
        for frame_times, csv_metrics in (
            ([100.0, 100.0], {"median_frame_time_ms": 16.0, "average_fps": 60.0}),
            ([16.0, 17.0], {"median_frame_time_ms": 100.0, "average_fps": 10.0}),
            ([16.0, float("nan")], {"median_frame_time_ms": 16.0, "average_fps": 60.0}),
        ):
            with self.subTest(frame_times=frame_times, csv_metrics=csv_metrics):
                with self.assertRaises(RuntimeError):
                    runner._require_unthrottled_performance(
                        frame_times, csv_metrics, limit_ms=100.0)

    def test_recheck_performance_mode_is_append_only_and_never_recaptures(self):
        source = RUNNER_PATH.read_text(encoding="utf-8")
        self.assertIn('mode.add_argument("--recheck-performance"', source)
        body = _function_body(source, "recheck_b1_performance")
        for token in (
            "_load_existing_manifest_evidence",
            "_load_existing_capture_evidence",
            "_load_existing_performance_evidence",
            "with _foreground_unreal_editor_window():",
            'csv_filename="qingshan-b1-performance-foreground.csv"',
            "max_median_frame_time_ms=100.0",
            'performance-foreground.json',
            'acceptance-summary-final.json',
            "_require_evidence_unchanged",
        ):
            self.assertIn(token, body)
        self.assertNotIn("run_dress_b1", body)
        self.assertNotIn("_capture_one_camera", body)
        self.assertNotIn("live-manifest-run-1.json\", validated", body)

    def test_validator_digest_is_recomputed_from_returned_manifest(self):
        runner = self._runner()
        manifest = {"z": [2, 1], "a": {"x": 1}}
        encoded = json.dumps(
            manifest, ensure_ascii=False, sort_keys=True,
            separators=(",", ":"), allow_nan=False,
        ).encode("utf-8")
        import hashlib
        digest = hashlib.sha256(encoded).hexdigest()
        self.assertEqual(runner._validate_live_payload({
            "live_scene_manifest_sha256": digest,
            "manifest": manifest,
        }), digest)
        with self.assertRaisesRegex(RuntimeError, "recomputed"):
            runner._validate_live_payload({
                "live_scene_manifest_sha256": "a" * 64,
                "manifest": manifest,
            })

    def test_uncertain_capture_begin_failure_still_requests_abort(self):
        runner = self._runner()

        class Client:
            def __init__(self):
                self.phases = []

            def call_tool(self, _tool_name, arguments, **_kwargs):
                argv = json.loads(arguments["argv_json"])
                phase = argv[-1]
                self.phases.append(phase)
                payload = {
                    "success": True,
                    "error": "",
                    "pending": False,
                    "complete": False,
                }
                return B1AcceptanceRunnerContractTests._outer(
                    ACCEPTANCE_SCRIPT,
                    argv,
                    ACCEPTANCE_MARKER,
                    payload,
                    outer_success=(phase == "abort"),
                )

        with tempfile.TemporaryDirectory() as temp_dir:
            client = Client()
            with self.assertRaisesRegex(RuntimeError, "outer"):
                runner._capture_one_camera(
                    client,
                    camera_label="CAM_QS_B1_GATE_ARRIVAL",
                    path=Path(temp_dir) / "fresh.png",
                    timeout_seconds=1.0,
                    monotonic=lambda: 0.0,
                    wall_time_ns=lambda: 1,
                    sleep=lambda _: None,
                )
            self.assertEqual(client.phases, ["begin", "abort"])

    def test_acceptance_cli_failure_returns_nonzero(self):
        runner = self._runner()

        class Client:
            endpoint = "fake"

            def __init__(self, **_kwargs):
                pass

            def connect(self):
                return True

        failed = {"success": False, "terminal_state": "validation_error", "error": "bad"}
        output = io.StringIO()
        with mock.patch.object(runner, "_load_client_class", return_value=Client):
            with mock.patch.object(runner, "run_b1_acceptance", return_value=failed):
                with contextlib.redirect_stdout(output):
                    code = runner.main(["--acceptance", "--timeout-seconds", "2"])
        self.assertNotEqual(code, 0)
        self.assertFalse(json.loads(output.getvalue())["success"])


if __name__ == "__main__":
    unittest.main()
