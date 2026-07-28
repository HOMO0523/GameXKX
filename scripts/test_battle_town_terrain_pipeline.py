#!/usr/bin/env python3
"""Static safety checks for the approved battle-town-terrain pipeline."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import os
import tempfile
import unittest
from unittest import mock
from pathlib import Path
from types import ModuleType, SimpleNamespace


PROJECT_ROOT = Path(__file__).resolve().parents[1]
AUDIT_SCRIPT = PROJECT_ROOT / "Content" / "Python" / "gamexxk_audit_battle_town_terrain.py"
FUTURE_BAKE_SCRIPT = PROJECT_ROOT / "Content" / "Python" / "gamexxk_bake_battle_town_terrain.py"
V2_BAKE_SCRIPT = PROJECT_ROOT / "Content" / "Python" / "gamexxk_bake_battle_town_terrain_v2.py"
V2_COMMANDLET_DRY_RUN_SCRIPT = (
    PROJECT_ROOT / "Content" / "Python" / "gamexxk_execute_battle_town_terrain_v2_dryrun.py"
)
V3_LAYER_MATERIAL_SCRIPT = (
    PROJECT_ROOT / "Content" / "Python" / "gamexxk_apply_battle_town_texture_layers.py"
)
RUNTIME_API_PROBE = PROJECT_ROOT / "Content" / "Python" / "gamexxk_probe_battle_town_terrain_api.py"
RUNTIME_SLICE_PROBE = PROJECT_ROOT / "Content" / "Python" / "gamexxk_probe_battle_town_terrain_slice.py"
TERRAIN_SPEC = PROJECT_ROOT / "docs" / "superpowers" / "specs" / "2026-07-17-battle-town-terrain-design.md"
TERRAIN_PLAN = PROJECT_ROOT / "docs" / "superpowers" / "plans" / "2026-07-17-battle-town-terrain-transfer.md"
MANIFEST = (
    PROJECT_ROOT
    / "SourceAssets"
    / "PartyDeck"
    / "battle-town-terrain"
    / "battle-town-terrain-manifest-v1.json"
)
V2_MANIFEST = (
    PROJECT_ROOT
    / "SourceAssets"
    / "PartyDeck"
    / "battle-town-terrain"
    / "battle-town-terrain-manifest-v2.json"
)

TOWN_MAP = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"
BATTLE_MAP = "/Game/GameXXK/Maps/L_BattleScene"
LEGACY_RESET_SCRIPT = "gamexxk_ensure_route_encounter_maps"


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def load_audit_module() -> ModuleType:
    spec = importlib.util.spec_from_file_location("gamexxk_audit_battle_town_terrain", AUDIT_SCRIPT)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load audit module from {AUDIT_SCRIPT}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class _FakeActor:
    def __init__(
        self,
        label: str,
        class_path: str,
        class_name: str,
        tags: list[str] | None = None,
        components: list[object] | None = None,
    ) -> None:
        self._label = label
        self._class = _FakeClass(class_path, class_name)
        self._tags = tags or []
        self._components = components or []

    def get_actor_label(self) -> str:
        return self._label

    def get_editor_property(self, name: str) -> list[str]:
        if name != "tags":
            raise AttributeError(name)
        return self._tags

    def get_class(self) -> object:
        return self._class

    def get_name(self) -> str:
        return self._label or "FakeActor"

    def get_components_by_class(self, _component_class: object) -> list[object]:
        return self._components

    def get_actor_transform(self) -> object:
        return _FakeTransform()


class _FakeClass:
    def __init__(self, path: str, name: str) -> None:
        self._path = path
        self._name = name

    def get_path_name(self) -> str:
        return self._path

    def get_name(self) -> str:
        return self._name


class _FakeAsset:
    def __init__(self, path: str) -> None:
        self._path = path

    def get_path_name(self) -> str:
        return self._path


class _FakeVector:
    x = 0.0
    y = 0.0
    z = 0.0


class _FakeRotation:
    x = 0.0
    y = 0.0
    z = 0.0
    w = 1.0


class _FakeTransform:
    translation = _FakeVector()
    rotation = _FakeRotation()
    scale3d = _FakeVector()


class _FakeComponent:
    def __init__(self, class_path: str, class_name: str) -> None:
        self._class = _FakeClass(class_path, class_name)
        self._mesh = _FakeAsset("/Engine/BasicShapes/Plane.Plane")
        self._material = _FakeAsset("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial")

    def get_class(self) -> object:
        return self._class

    def get_name(self) -> str:
        return "FakeComponent"

    def get_editor_property(self, name: str) -> object:
        if name == "static_mesh":
            return self._mesh
        raise AttributeError(name)

    def get_num_materials(self) -> int:
        return 1

    def get_material(self, index: int) -> object:
        if index != 0:
            raise IndexError(index)
        return self._material


class BattleTownTerrainPipelineTest(unittest.TestCase):
    def test_audit_declares_source_and_target_maps(self) -> None:
        text = read_text(AUDIT_SCRIPT)

        self.assertIn(TOWN_MAP, text)
        self.assertIn(BATTLE_MAP, text)

    def test_audit_is_read_only(self) -> None:
        text = read_text(AUDIT_SCRIPT).lower()

        for forbidden in (
            "save_current_level",
            "save_loaded_asset",
            "save_package",
            "save_dirty_packages",
        ):
            self.assertNotIn(forbidden, text)

    def test_audit_has_output_entrypoint_and_no_worldgrid(self) -> None:
        text = read_text(AUDIT_SCRIPT)

        self.assertIn("--output", text)
        self.assertIn("def main(", text)
        self.assertNotIn("WorldGrid", text)

    def test_class_paths_and_floor_identity_are_exact_and_type_checked(self) -> None:
        text = read_text(AUDIT_SCRIPT)
        self.assertIn("def _class_name(", text)
        self.assertIn("StaticMeshActor", text)
        self.assertIn("StaticMeshComponent", text)
        self.assertIn("_actor_label(actor) == ENCOUNTER_FLOOR_ID", text)
        self.assertNotIn("ENCOUNTER_FLOOR_ID in _actor_tags(actor)", text)

        audit = load_audit_module()
        component = _FakeComponent(
            "/Script/Engine.StaticMeshComponent.StaticMeshComponent",
            "StaticMeshComponent",
        )
        self.assertEqual("/Script/Engine.StaticMeshComponent", audit._class_path(component))

        floor = _FakeActor(
            "GameXXK_Encounter_Floor",
            "/Script/Engine.StaticMeshActor.StaticMeshActor",
            "StaticMeshActor",
            components=[component],
        )
        audit.unreal = SimpleNamespace(ActorComponent=object)
        audit._all_level_actors = lambda: [floor]
        floor_snapshot = audit.encounter_floor_snapshot()
        self.assertEqual("/Script/Engine.StaticMeshActor", floor_snapshot["class"])
        self.assertEqual("/Script/Engine.StaticMeshComponent", floor_snapshot["components"][0]["class"])

        camera = _FakeActor("neutral", "/Script/Engine.CameraActor.CameraActor", "CameraActor")
        player_start = _FakeActor("neutral", "/Script/Engine.PlayerStart.PlayerStart", "PlayerStart")
        light = _FakeActor("neutral", "/Script/Engine.DirectionalLight.DirectionalLight", "DirectionalLight")
        self.assertIn("camera", audit._protection_reasons(camera))
        self.assertIn("player_start", audit._protection_reasons(player_start))
        self.assertIn("light", audit._protection_reasons(light))

        tag_only = _FakeActor(
            "Renamed Floor",
            "/Script/Engine.StaticMeshActor.StaticMeshActor",
            "StaticMeshActor",
            tags=["GameXXK_Encounter_Floor"],
            components=[component],
        )
        audit._all_level_actors = lambda: [tag_only]
        with self.assertRaises(RuntimeError):
            audit.find_single_encounter_floor()

        wrong_class = _FakeActor("GameXXK_Encounter_Floor", "/Script/Engine.Actor.Actor", "Actor")
        audit._all_level_actors = lambda: [wrong_class]
        with self.assertRaises(RuntimeError):
            audit.find_single_encounter_floor()

        audit._all_level_actors = lambda: [floor, _FakeActor(
            "GameXXK_Encounter_Floor",
            "/Script/Engine.StaticMeshActor.StaticMeshActor",
            "StaticMeshActor",
            components=[component],
        )]
        with self.assertRaises(RuntimeError):
            audit.find_single_encounter_floor()

    def test_audit_session_rejects_pie_and_restores_original_map(self) -> None:
        text = read_text(AUDIT_SCRIPT)
        main_body = text.split("def main(", 1)[1]
        self.assertIn("def _require_no_active_pie(", text)
        self.assertIn("def _capture_original_map(", text)
        self.assertIn("def _restore_original_map(", text)
        self.assertIn("try:", main_body)
        self.assertIn("finally:", main_body)
        self.assertIn("_restore_original_map(original_map)", main_body)

        audit = load_audit_module()
        active_pie = object()
        audit.unreal = SimpleNamespace(
            UnrealEditorSubsystem=object,
            get_editor_subsystem=lambda _type: SimpleNamespace(get_game_world=lambda: active_pie),
        )
        with self.assertRaises(RuntimeError):
            audit._require_no_active_pie()

        audit.unreal = SimpleNamespace()
        with self.assertRaises(RuntimeError):
            audit._require_no_active_pie()
        audit.unreal = SimpleNamespace(
            UnrealEditorSubsystem=object,
            get_editor_subsystem=lambda _type: SimpleNamespace(),
        )
        with self.assertRaises(RuntimeError):
            audit._require_no_active_pie()

        state = {"map": BATTLE_MAP}
        audit._current_map_path = lambda: state["map"]
        audit._require_no_active_pie = lambda: None
        audit._dirty_package_names = lambda: []
        audit._load_map_for_reading = lambda map_path: state.update(map=map_path) or object()
        audit._restore_original_map(TOWN_MAP)
        self.assertEqual(TOWN_MAP, state["map"])

    def test_restore_refuses_to_switch_maps_when_unexpected_packages_are_dirty(self) -> None:
        audit = load_audit_module()
        state = {"map": BATTLE_MAP}
        attempted_loads: list[str] = []
        audit._current_map_path = lambda: state["map"]
        audit._require_no_active_pie = lambda: None
        audit._dirty_package_names = lambda: ["/Game/GameXXK/Maps/L_BattleScene"]
        audit._load_map_for_reading = lambda map_path: attempted_loads.append(map_path) or object()

        with self.assertRaisesRegex(RuntimeError, "leaving the active map unchanged"):
            audit._restore_original_map(TOWN_MAP)
        self.assertEqual(BATTLE_MAP, state["map"])
        self.assertEqual([], attempted_loads)

    def test_map_loader_checks_session_safety_before_and_after_load(self) -> None:
        audit = load_audit_module()
        state = {"map": BATTLE_MAP}
        checks: list[str] = []
        audit.unreal = SimpleNamespace(
            EditorAssetLibrary=SimpleNamespace(does_asset_exist=lambda _path: True),
            EditorLoadingAndSavingUtils=SimpleNamespace(
                load_map=lambda map_path: state.update(map=map_path) or True
            ),
        )
        audit._require_audit_session_safe = lambda: checks.append("safe")
        audit._current_map_path = lambda: state["map"]
        audit._editor_world = lambda: object()

        audit._load_map_for_reading(TOWN_MAP)
        self.assertEqual(["safe", "safe"], checks)

    def test_output_is_atomic_and_refuses_existing_snapshot(self) -> None:
        audit = load_audit_module()
        with tempfile.TemporaryDirectory() as temporary_directory:
            saved_dir = Path(temporary_directory) / "Saved"
            output = saved_dir / "HarnessReports" / "before.json"
            audit._project_saved_dir = lambda: saved_dir.resolve()

            audit._write_report(output, {"schema": 1})
            self.assertEqual({"schema": 1}, json.loads(output.read_text(encoding="utf-8")))
            self.assertEqual([], list(saved_dir.rglob("*.tmp")))
            with self.assertRaises(RuntimeError):
                audit._write_report(output, {"schema": 2})

    def test_package_baseline_includes_detected_external_actor_and_sidecar_packages(self) -> None:
        audit = load_audit_module()
        with tempfile.TemporaryDirectory() as temporary_directory:
            content_dir = Path(temporary_directory) / "Content"
            map_file = content_dir / "GameXXK" / "Maps" / "Prototype" / "L_Qingshan_AsianVillage_Demo.umap"
            map_file.parent.mkdir(parents=True)
            map_file.write_bytes(b"map")
            external_actor = (
                content_dir
                / "__ExternalActors__"
                / "GameXXK"
                / "Maps"
                / "Prototype"
                / "L_Qingshan_AsianVillage_Demo"
                / "actor.uasset"
            )
            external_actor.parent.mkdir(parents=True)
            external_actor.write_bytes(b"external")
            external_payload = external_actor.with_suffix(".uexp")
            external_payload.write_bytes(b"external payload")
            external_bulk = external_actor.with_suffix(".ubulk")
            external_bulk.write_bytes(b"external bulk")
            external_optional = external_actor.with_suffix(".uptnl")
            external_optional.write_bytes(b"external optional")
            sidecar = map_file.parent / "L_Qingshan_AsianVillage_Demo_ExternalData" / "sidecar.uasset"
            sidecar.parent.mkdir(parents=True)
            sidecar.write_bytes(b"sidecar")
            unknown_sidecar = sidecar.with_suffix(".custom")
            unknown_sidecar.write_bytes(b"unknown sidecar")
            audit._project_content_dir = lambda: content_dir.resolve()

            baseline = audit._map_package_baseline(TOWN_MAP, map_file)
            self.assertEqual(4, len(baseline["external_actor_packages"]))
            self.assertEqual(str(external_actor.resolve()), baseline["external_actor_packages"][0]["file"])
            self.assertEqual(["actor.uasset", "actor.ubulk", "actor.uexp", "actor.uptnl"], [
                record["relative_path"] for record in baseline["external_actor_packages"]
            ])
            self.assertEqual(2, len(baseline["sidecar_packages"]))
            self.assertEqual(str(unknown_sidecar.resolve()), baseline["sidecar_packages"][0]["file"])
            self.assertEqual(["sidecar.custom", "sidecar.uasset"], [
                record["relative_path"] for record in baseline["sidecar_packages"]
            ])

    def test_design_documents_lock_floor_by_exact_actor_label(self) -> None:
        specification = read_text(TERRAIN_SPEC)
        plan = read_text(TERRAIN_PLAN)

        self.assertIn("精确 Actor Label `GameXXK_Encounter_Floor`", specification)
        self.assertIn("精确 Actor Label `GameXXK_Encounter_Floor`", plan)
        self.assertIn("find_single_actor_by_label", plan)
        self.assertNotIn("find_single_actor_by_tag", plan)
        self.assertIn("保留当前地图", specification)
        self.assertIn("保留当前地图", plan)

    def test_output_is_contained_in_project_saved_directory(self) -> None:
        text = read_text(AUDIT_SCRIPT)
        self.assertIn("def _project_saved_dir(", text)
        self.assertIn("relative_to(", text)

        audit = load_audit_module()
        expected = (PROJECT_ROOT / "Saved" / "HarnessReports" / "battle-town-before.json").resolve()
        parsed = audit._parse_args(["--output", str(expected)])
        self.assertEqual(expected, parsed.output)

        outside_project_saved = (PROJECT_ROOT.parent / "battle-town-before.json").resolve()
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                audit._parse_args(["--output", str(outside_project_saved)])

    def test_commandlet_can_supply_a_safe_output_path_through_environment(self) -> None:
        audit = load_audit_module()
        with tempfile.TemporaryDirectory() as temporary_directory:
            saved_dir = Path(temporary_directory) / "Saved"
            expected = saved_dir / "HarnessReports" / "before.json"
            audit._project_saved_dir = lambda: saved_dir.resolve()
            with mock.patch.dict(
                os.environ,
                {"GAMEXXK_BATTLE_TERRAIN_AUDIT_OUTPUT": str(expected)},
                clear=False,
            ):
                parsed = audit._parse_args([])
        self.assertEqual(expected.resolve(), parsed.output)

    def test_explicit_commandlet_mode_uses_battle_map_as_the_restore_anchor_from_temp_world(self) -> None:
        audit = load_audit_module()
        audit._require_audit_session_safe = lambda: None
        audit._current_map_path = lambda: "/Temp/Untitled_0"
        audit.unreal = SimpleNamespace(
            EditorAssetLibrary=SimpleNamespace(does_asset_exist=lambda path: path == BATTLE_MAP)
        )
        with mock.patch.dict(
            os.environ,
            {"GAMEXXK_BATTLE_TERRAIN_AUDIT_COMMANDLET": "1"},
            clear=False,
        ):
            self.assertEqual(BATTLE_MAP, audit._capture_original_map())

    def test_candidate_slice_uses_the_locked_24_by_14_meter_safe_terrain_contract(self) -> None:
        audit = load_audit_module()
        landscapes = [
            {
                "name": "Landscape_0",
                "bounds": {
                    "center": {"x": 0.0, "y": 0.0, "z": 0.0},
                    "extent": {"x": 4000.0, "y": 4000.0, "z": 800.0},
                },
            }
        ]

        candidate = audit.select_candidate_slice(
            landscapes,
            candidate_centers=[{"x": 0.0, "y": 0.0}],
            sample_height=lambda _x, _y: 42.0,
            overlaps_forbidden=lambda _bounds: False,
        )

        self.assertEqual("Landscape_0", candidate["source_landscape"])
        self.assertEqual(2400.0, candidate["size_cm"]["x"])
        self.assertEqual(1400.0, candidate["size_cm"]["y"])
        self.assertEqual(0.0, candidate["max_height_delta_cm"])
        self.assertFalse(candidate["overlaps_forbidden"])

    def test_baker_requires_an_explicit_execute_flag_and_owned_output_root(self) -> None:
        self.assertTrue(FUTURE_BAKE_SCRIPT.exists())
        text = read_text(FUTURE_BAKE_SCRIPT)

        self.assertIn("--execute", text)
        self.assertIn("/Game/GameXXK/Environment/Battle/TownTerrain", text)
        self.assertNotIn("gamexxk_ensure_route_encounter_maps", text)

        spec = importlib.util.spec_from_file_location("gamexxk_bake_battle_town_terrain", FUTURE_BAKE_SCRIPT)
        self.assertIsNotNone(spec)
        self.assertIsNotNone(spec.loader if spec else None)
        assert spec is not None and spec.loader is not None
        baker = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(baker)
        with self.assertRaisesRegex(RuntimeError, "Refusing to write"):
            baker.main(["--manifest", str(MANIFEST)])

    def test_baker_uses_sampled_landscape_geometry_and_render_capture_assets(self) -> None:
        text = read_text(FUTURE_BAKE_SCRIPT)

        for required in (
            "get_all_vertex_positions",
            "line_trace_single",
            "set_all_mesh_vertex_positions",
            "bake_texture_from_render_captures",
            "create_new_texture2d_asset",
            "create_new_static_mesh_asset_from_mesh",
            "GameXXK_Encounter_Floor",
            "set_static_mesh",
            "save_current_level",
        ):
            self.assertIn(required, text)
        self.assertNotIn("set_actor_transform", text)

    def test_baker_connects_the_captured_texture_through_its_owner_material(self) -> None:
        text = read_text(FUTURE_BAKE_SCRIPT)

        self.assertIn(
            '_connect_expression(material, texture_node, "RGB", unlit_node,',
            text,
        )

    def test_v2_requires_explicit_local_capture_cameras_and_rejects_invalid_gray_output(self) -> None:
        """The bad v1 RGB(173) fallback must never be silently applied again."""
        self.assertTrue(V2_BAKE_SCRIPT.exists())
        text = read_text(FUTURE_BAKE_SCRIPT)
        v2_text = read_text(V2_BAKE_SCRIPT)

        for required in (
            "compute_render_capture_cameras_for_box",
            "GeometryScriptRenderCaptureCamerasForBoxOptions",
            "cleanup_tolerance",
            "_validate_capture_preview",
            "_parse_png_rgb_statistics",
            "--dry-run-capture",
            "--capture-report",
            "INVALID_CAPTURE_SRGB",
        ):
            self.assertIn(required, text)
        self.assertIn("TownTerrainV2", v2_text)
        self.assertIn("_configure_v2_output_contract", v2_text)
        self.assertNotIn("spawn_actor_from_class", text)
        self.assertNotIn("SceneCapture2D", text)

    def test_v2_capture_validation_rejects_the_known_invalid_fallback_and_accepts_varied_pixels(self) -> None:
        spec = importlib.util.spec_from_file_location("gamexxk_bake_battle_town_terrain", FUTURE_BAKE_SCRIPT)
        self.assertIsNotNone(spec)
        self.assertIsNotNone(spec.loader if spec else None)
        assert spec is not None and spec.loader is not None
        baker = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(baker)

        invalid = baker._validate_capture_statistics(
            {"min_rgb": [173, 173, 173], "max_rgb": [173, 173, 173], "sample_count": 9}
        )
        self.assertFalse(invalid["ok"])
        self.assertEqual("invalid_capture_fallback", invalid["reason"])

        valid = baker._validate_capture_statistics(
            {"min_rgb": [42, 76, 31], "max_rgb": [191, 172, 126], "sample_count": 9}
        )
        self.assertTrue(valid["ok"])
        self.assertEqual("", valid["reason"])

    def test_baker_can_explicitly_rollback_only_its_three_owned_output_assets(self) -> None:
        spec = importlib.util.spec_from_file_location("gamexxk_bake_battle_town_terrain", FUTURE_BAKE_SCRIPT)
        self.assertIsNotNone(spec)
        self.assertIsNotNone(spec.loader if spec else None)
        assert spec is not None and spec.loader is not None
        baker = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(baker)

        class FakeAssetLibrary:
            def __init__(self) -> None:
                self.deleted: list[str] = []

            def does_asset_exist(self, path: str) -> bool:
                return path in (baker.OUTPUT_ALBEDO, baker.OUTPUT_MATERIAL)

            def delete_asset(self, path: str) -> bool:
                self.deleted.append(path)
                return True

        library = FakeAssetLibrary()
        baker.unreal = SimpleNamespace(EditorAssetLibrary=library)
        self.assertEqual(
            [baker.OUTPUT_MATERIAL, baker.OUTPUT_ALBEDO],
            baker._delete_owned_output_assets(),
        )
        self.assertEqual([baker.OUTPUT_MATERIAL, baker.OUTPUT_ALBEDO], library.deleted)

    def test_baker_supports_explicit_commandlet_environment_arguments(self) -> None:
        text = read_text(FUTURE_BAKE_SCRIPT)

        self.assertIn("GAMEXXK_BATTLE_TERRAIN_MANIFEST", text)
        self.assertIn("GAMEXXK_BATTLE_TERRAIN_EXECUTE", text)

    def test_runtime_geometry_api_probe_is_read_only(self) -> None:
        self.assertTrue(RUNTIME_API_PROBE.exists())
        text = read_text(RUNTIME_API_PROBE).lower()

        self.assertIn("systemlibrary", text)
        self.assertIn("staticmesheditorsubsystem", text)
        self.assertNotIn("save_current_level", text)
        self.assertNotIn("save_loaded_asset", text)
        self.assertNotIn("set_actor_transform", text)

    def test_runtime_slice_probe_samples_landscape_without_writing_assets_or_maps(self) -> None:
        self.assertTrue(RUNTIME_SLICE_PROBE.exists())
        text = read_text(RUNTIME_SLICE_PROBE).lower()

        self.assertIn("line_trace_single", text)
        self.assertIn("height_sample_grid", text)
        self.assertIn("candidate_centers", text)
        self.assertIn("_restore_original_map(original_map)", text)
        for forbidden in (
            "save_current_level",
            "save_loaded_asset",
            "save_package",
            "set_actor_transform",
            "create_asset",
        ):
            self.assertNotIn(forbidden, text)

    def test_manifest_records_the_audited_safe_slice_with_fixed_maps(self) -> None:
        manifest = json.loads(read_text(MANIFEST))

        self.assertEqual(1, manifest["schema"])
        self.assertEqual("baked", manifest["status"])
        self.assertEqual(TOWN_MAP, manifest["source_map"])
        self.assertEqual(BATTLE_MAP, manifest["target_map"])
        self.assertEqual("Landscape_0", manifest["source_landscape"])
        self.assertEqual(
            {"min": {"x": -1200.0, "y": -700.0}, "max": {"x": 1200.0, "y": 700.0}},
            manifest["slice_world_bounds_cm"],
        )
        self.assertEqual(3, len(manifest["output_assets"]))
        self.assertIn("town_package_sha256", manifest["source_hashes"])
        self.assertIn("battle_package_sha256", manifest["source_hashes"])
        self.assertIn("before_snapshot", manifest["validation"])

    def test_v2_manifest_keeps_v1_for_rollback_and_starts_audited(self) -> None:
        self.assertTrue(V2_MANIFEST.exists())
        manifest = json.loads(read_text(V2_MANIFEST))

        self.assertEqual(2, manifest["schema"])
        self.assertEqual("audited", manifest["status"])
        self.assertEqual(TOWN_MAP, manifest["source_map"])
        self.assertEqual(BATTLE_MAP, manifest["target_map"])
        self.assertEqual([], manifest["output_assets"])
        self.assertTrue(
            any("TownTerrain/SM_Battle_QingshanGround_01" in path for path in manifest["rollback_asset_contract"])
        )

    def test_v2_commandlet_entrypoint_is_isolated_and_dry_run_only(self) -> None:
        """The Landscape source may never be loaded by the user's live editor process."""
        self.assertTrue(V2_COMMANDLET_DRY_RUN_SCRIPT.exists())
        text = read_text(V2_COMMANDLET_DRY_RUN_SCRIPT)

        self.assertIn("gamexxk_bake_battle_town_terrain_v2", text)
        self.assertIn("GAMEXXK_BATTLE_TERRAIN_AUDIT_COMMANDLET", text)
        self.assertIn("--dry-run-capture", text)
        self.assertIn("--capture-report", text)
        self.assertNotIn('"--execute"', text)

    def test_v3_uses_a_verified_town_texture_layer_without_loading_the_landscape(self) -> None:
        """The fallback must be a static texture path, never another Landscape capture."""
        self.assertTrue(V3_LAYER_MATERIAL_SCRIPT.exists())
        text = read_text(V3_LAYER_MATERIAL_SCRIPT)

        self.assertIn("T_ground_BaseColor", text)
        self.assertIn("T_ground_Normal", text)
        self.assertIn("TownTerrainV3", text)
        self.assertIn("--preflight", text)
        self.assertIn("--execute", text)
        self.assertNotIn("L_Qingshan_AsianVillage_Demo", text)
        self.assertNotIn("_load_map_for_reading(audit.TOWN_MAP)", text)

        spec = importlib.util.spec_from_file_location("gamexxk_apply_battle_town_texture_layers", V3_LAYER_MATERIAL_SCRIPT)
        self.assertIsNotNone(spec)
        self.assertIsNotNone(spec.loader if spec else None)
        assert spec is not None and spec.loader is not None
        layers = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(layers)

        class FakeLibrary:
            def __init__(self, missing: str = "") -> None:
                self.missing = missing

            def does_asset_exist(self, path: str) -> bool:
                return path != self.missing

        self.assertEqual(
            layers.TOWN_TEXTURE_ASSETS,
            layers._require_town_texture_assets(FakeLibrary()),
        )
        with self.assertRaises(RuntimeError):
            layers._require_town_texture_assets(FakeLibrary(layers.TOWN_TEXTURE_ASSETS["ground_base_color"]))

    def test_audit_and_future_baker_never_call_legacy_map_reset_script(self) -> None:
        self.assertNotIn(LEGACY_RESET_SCRIPT, read_text(AUDIT_SCRIPT))
        if FUTURE_BAKE_SCRIPT.exists():
            self.assertNotIn(LEGACY_RESET_SCRIPT, read_text(FUTURE_BAKE_SCRIPT))


if __name__ == "__main__":
    unittest.main(verbosity=2)
