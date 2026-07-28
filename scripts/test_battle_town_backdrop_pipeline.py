#!/usr/bin/env python3
"""Static contracts for the isolated Qingshan battle-town backdrop pipeline."""

from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path
from types import ModuleType


PROJECT_ROOT = Path(__file__).resolve().parents[1]
BACKDROP_SCRIPT = PROJECT_ROOT / "Content" / "Python" / "gamexxk_build_battle_town_backdrop.py"
ROUTE_VALIDATOR_SCRIPT = PROJECT_ROOT / "Content" / "Python" / "gamexxk_validate_route_encounter_maps.py"
BATTLE_PRESENTER_CPP = PROJECT_ROOT / "Source" / "GameXXK" / "Private" / "MVP" / "GameXXKBattleScenePresenter.cpp"


def load_backdrop_module() -> ModuleType:
    spec = importlib.util.spec_from_file_location("gamexxk_build_battle_town_backdrop", BACKDROP_SCRIPT)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load backdrop module from {BACKDROP_SCRIPT}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class BattleTownBackdropPipelineTest(unittest.TestCase):
    def test_backdrop_uses_live_town_as_read_only_source(self) -> None:
        backdrop = load_backdrop_module()

        self.assertEqual(
            backdrop.SOURCE_MAP,
            "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo",
        )
        self.assertEqual(
            backdrop.TARGET_BATTLE_MAP,
            "/Game/GameXXK/Maps/L_BattleTown",
        )
        self.assertNotEqual(backdrop.SOURCE_MAP, backdrop.TARGET_BATTLE_MAP)
        self.assertEqual(
            backdrop.KEEP_MESH_PREFIXES,
            (
                "/Game/Asian_Village/meshes/trees/",
                "/Game/Asian_Village/meshes/plants/",
                "/Game/Asian_Village/meshes/cliff/",
            ),
        )

    def test_mesh_policy_keeps_only_town_vegetation_and_cliffs(self) -> None:
        backdrop = load_backdrop_module()

        self.assertTrue(backdrop.should_keep_mesh_path("/Game/Asian_Village/meshes/plants/SM_grass_01"))
        self.assertTrue(backdrop.should_keep_mesh_path("/Game/Asian_Village/meshes/trees/SM_tree_01"))
        self.assertTrue(backdrop.should_keep_mesh_path("/Game/Asian_Village/meshes/cliff/SM_cliff_01"))
        self.assertFalse(backdrop.should_keep_mesh_path("/Game/Asian_Village/meshes/building/SM_house_01"))
        self.assertFalse(backdrop.should_keep_mesh_path("/Game/Asian_Village/meshes/props/SM_cart_01"))
        self.assertFalse(backdrop.should_keep_mesh_path("/Game/Asian_Village/meshes/water/SM_water_01"))

    def test_actor_policy_only_keeps_landscape_or_approved_meshes(self) -> None:
        backdrop = load_backdrop_module()

        self.assertTrue(backdrop.should_keep_actor_class("Landscape"))
        self.assertTrue(backdrop.should_keep_actor_class("LandscapeStreamingProxy"))
        self.assertFalse(backdrop.should_keep_actor_class("PlayerStart"))
        self.assertFalse(backdrop.should_keep_actor_class("CameraActor"))
        self.assertFalse(backdrop.should_keep_actor_class("StaticMeshActor"))

    def test_cleanup_policy_rejects_mixed_foliage_before_any_actor_is_deleted(self) -> None:
        backdrop = load_backdrop_module()

        self.assertEqual(backdrop.classify_actor_for_cleanup("Landscape", ()), "keep_landscape")
        self.assertEqual(
            backdrop.classify_actor_for_cleanup(
                "StaticMeshActor", ("/Game/Asian_Village/meshes/plants/SM_grass_01",)
            ),
            "keep_mesh",
        )
        self.assertEqual(
            backdrop.classify_actor_for_cleanup(
                "StaticMeshActor", ("/Game/Asian_Village/meshes/building/SM_house_01",)
            ),
            "delete",
        )
        self.assertEqual(
            backdrop.classify_actor_for_cleanup(
                "InstancedFoliageActor",
                (
                    "/Game/Asian_Village/meshes/plants/SM_grass_01",
                    "/Game/Asian_Village/meshes/trees/SM_tree_01",
                ),
            ),
            "keep_foliage",
        )
        self.assertEqual(
            backdrop.classify_actor_for_cleanup(
                "InstancedFoliageActor",
                (
                    "/Game/Asian_Village/meshes/plants/SM_grass_01",
                    "/Game/Asian_Village/meshes/props/SM_cart_01",
                ),
            ),
            "reject_mixed_foliage",
        )
        self.assertEqual(backdrop.classify_actor_for_cleanup("BP_NpcCharacter", ()), "delete")

    def test_battle_scaffolding_has_no_floor_and_uses_the_new_battle_map(self) -> None:
        backdrop = load_backdrop_module()

        self.assertEqual(backdrop.LEGACY_BATTLE_MAP, "/Game/GameXXK/Maps/L_BattleScene")
        self.assertEqual(backdrop.TARGET_BATTLE_MAP, "/Game/GameXXK/Maps/L_BattleTown")
        self.assertEqual(backdrop.PRESENTER_LABEL, "GameXXK_BattleScene_Presenter")
        self.assertEqual(backdrop.CAMERA_LABEL, "GameXXK_BattleScene_Camera")
        self.assertEqual(backdrop.PLAYER_START_LABEL, "GameXXK_Encounter_PlayerStart")
        self.assertEqual(backdrop.LIGHT_LABEL, "GameXXK_Encounter_Light")

        plan = backdrop.scaffold_plan()
        self.assertEqual(plan["map"], backdrop.TARGET_BATTLE_MAP)
        self.assertEqual(set(plan["actors"]), {
            backdrop.PRESENTER_LABEL,
            backdrop.CAMERA_LABEL,
            backdrop.PLAYER_START_LABEL,
            backdrop.LIGHT_LABEL,
        })
        self.assertNotIn("GameXXK_Encounter_Floor", plan["actors"])

    def test_route_validator_targets_the_verified_battle_town_without_loading_it(self) -> None:
        source = ROUTE_VALIDATOR_SCRIPT.read_text(encoding="utf-8")
        battle_validator = source.split("def _validate_battle_scene", 1)[1].split(
            "def _validate_enemy_visual_assets", 1
        )[0]

        self.assertIn('"map": "/Game/GameXXK/Maps/L_BattleTown"', source)
        self.assertIn("battle map must already be open", source)
        self.assertNotIn("load_map(map_path)", battle_validator)

    def test_source_map_is_never_loaded_or_saved_by_the_pipeline(self) -> None:
        source = BACKDROP_SCRIPT.read_text(encoding="utf-8")

        self.assertNotIn("_load_map(SOURCE_MAP)", source)
        self.assertNotIn("save_current_level(SOURCE_MAP)", source)
        self.assertNotIn("gamexxk_ensure_route_encounter_maps", source)

    def test_duplicate_stage_never_loads_a_landscape_map_through_mcp(self) -> None:
        source = BACKDROP_SCRIPT.read_text(encoding="utf-8")

        self.assertIn("def duplicate_battle_map", source)
        self.assertIn("duplicate_asset(SOURCE_MAP, TARGET_BATTLE_MAP)", source)
        self.assertIn("save_loaded_asset(duplicated, True)", source)
        self.assertNotIn("EditorLoadingAndSavingUtils.load_map", source)
        self.assertNotIn("EditorLevelLibrary.load_level", source)
        self.assertNotIn("save_dirty_packages", source)

    def test_cleanup_guard_accepts_only_a_visible_target_battle_map(self) -> None:
        backdrop = load_backdrop_module()

        self.assertTrue(backdrop.is_target_battle_map(backdrop.TARGET_BATTLE_MAP))
        self.assertTrue(backdrop.is_target_battle_map("/Game/GameXXK/Maps/UEDPIE_0_L_BattleTown"))
        self.assertFalse(backdrop.is_target_battle_map(backdrop.SOURCE_MAP))
        self.assertFalse(backdrop.is_target_battle_map(backdrop.LEGACY_BATTLE_MAP))

    def test_command_parser_exposes_duplicate_only_stage(self) -> None:
        backdrop = load_backdrop_module()

        args = backdrop.parse_args(["--duplicate-only"])
        self.assertTrue(args.duplicate_only)

        args = backdrop.parse_args(["--finalize-existing-duplicate"])
        self.assertTrue(args.finalize_existing_duplicate)

        args = backdrop.parse_args(["--preflight-current-map"])
        self.assertTrue(args.preflight_current_map)

        args = backdrop.parse_args(["--audit-current-map"])
        self.assertTrue(args.audit_current_map)

        args = backdrop.parse_args(["--cleanup-current-map", "--limit", "50"])
        self.assertTrue(args.cleanup_current_map)
        self.assertEqual(args.limit, 50)

        args = backdrop.parse_args(["--add-battle-scaffolding"])
        self.assertTrue(args.add_battle_scaffolding)

        args = backdrop.parse_args(["--validate-final-map"])
        self.assertTrue(args.validate_final_map)

    def test_recovery_persists_only_the_existing_target_asset(self) -> None:
        source = BACKDROP_SCRIPT.read_text(encoding="utf-8")

        self.assertIn("def finalize_existing_duplicate", source)
        self.assertIn("load_asset(TARGET_BATTLE_MAP)", source)
        self.assertIn("save_loaded_asset(target_asset, True)", source)

    def test_package_path_normalization_guards_current_editor_map(self) -> None:
        backdrop = load_backdrop_module()

        self.assertEqual(
            backdrop.package_path_from_object_path("/Game/GameXXK/Maps/L_BattleTown.L_BattleTown"),
            backdrop.TARGET_BATTLE_MAP,
        )
        self.assertEqual(
            backdrop.package_path_from_object_path(backdrop.TARGET_BATTLE_MAP),
            backdrop.TARGET_BATTLE_MAP,
        )

    def test_cleanup_summary_is_read_only_and_blocks_mixed_foliage(self) -> None:
        backdrop = load_backdrop_module()
        records = [
            {"label": "Landscape", "class_name": "Landscape", "mesh_paths": ()},
            {
                "label": "Grass",
                "class_name": "InstancedFoliageActor",
                "mesh_paths": ("/Game/Asian_Village/meshes/plants/SM_grass_01",),
            },
            {
                "label": "House",
                "class_name": "StaticMeshActor",
                "mesh_paths": ("/Game/Asian_Village/meshes/building/SM_house_01",),
            },
            {
                "label": "MixedFoliage",
                "class_name": "InstancedFoliageActor",
                "mesh_paths": (
                    "/Game/Asian_Village/meshes/plants/SM_grass_01",
                    "/Game/Asian_Village/meshes/props/SM_cart_01",
                ),
            },
        ]

        summary = backdrop.summarize_cleanup_records(records)
        self.assertEqual(summary["actor_count"], 4)
        self.assertEqual(summary["actions"]["keep_landscape"], 1)
        self.assertEqual(summary["actions"]["keep_foliage"], 1)
        self.assertEqual(summary["actions"]["delete"], 1)
        self.assertEqual(summary["actions"]["reject_mixed_foliage"], 1)
        self.assertEqual(summary["mixed_foliage_labels"], ["MixedFoliage"])

    def test_delete_batch_selection_is_deterministic_and_refuses_mixed_foliage(self) -> None:
        backdrop = load_backdrop_module()
        records = [
            {"label": "Z House", "class_name": "StaticMeshActor", "mesh_paths": ("/Game/Asian_Village/meshes/building/SM_house",)},
            {"label": "Landscape", "class_name": "Landscape", "mesh_paths": ()},
            {"label": "A Prop", "class_name": "StaticMeshActor", "mesh_paths": ("/Game/Asian_Village/meshes/props/SM_cart",)},
        ]
        selected = backdrop.select_delete_records(records, limit=1)
        self.assertEqual([record["label"] for record in selected], ["A Prop"])

        mixed = records + [
            {
                "label": "MixedFoliage",
                "class_name": "InstancedFoliageActor",
                "mesh_paths": (
                    "/Game/Asian_Village/meshes/plants/SM_grass",
                    "/Game/Asian_Village/meshes/props/SM_cart",
                ),
            }
        ]
        with self.assertRaises(RuntimeError):
            backdrop.select_delete_records(mixed, limit=50)

    def test_battle_scaffold_spec_uses_existing_wide_camera_without_a_floor(self) -> None:
        backdrop = load_backdrop_module()

        spec = backdrop.battle_scaffold_spec()
        self.assertEqual(spec["map"], backdrop.TARGET_BATTLE_MAP)
        self.assertEqual(spec["anchor_xy"], (20400.0, 4580.0))
        self.assertEqual(spec["camera"]["label"], backdrop.CAMERA_LABEL)
        self.assertEqual(spec["camera"]["offset"], (-420.0, 0.0, 720.0))
        self.assertEqual(spec["camera"]["rotation"], (-60.0, 0.0, 0.0))
        self.assertEqual(spec["camera"]["fov"], 63.0)
        self.assertEqual(spec["presenter"]["label"], backdrop.PRESENTER_LABEL)
        self.assertEqual(spec["player_start"]["label"], backdrop.PLAYER_START_LABEL)
        self.assertEqual(spec["light"]["label"], backdrop.LIGHT_LABEL)
        self.assertNotIn("floor", spec)

    def test_scaffold_locations_are_grounded_on_the_retained_town_landscape(self) -> None:
        backdrop = load_backdrop_module()

        locations = backdrop.battle_scaffold_locations_for_ground(1490.602469100486)
        self.assertEqual(locations["presenter"], (20400.0, 4580.0, 1490.602469100486))
        self.assertEqual(locations["camera"], (19980.0, 4580.0, 2210.602469100486))
        self.assertEqual(locations["player_start"], (20620.0, 4580.0, 1570.602469100486))
        self.assertEqual(locations["light"], (20100.0, 4280.0, 1990.602469100486))

    def test_presenter_offsets_runtime_unit_formation_from_its_scene_anchor(self) -> None:
        source = BATTLE_PRESENTER_CPP.read_text(encoding="utf-8")

        self.assertIn("BuildUnitPlacementsForStateAtAnchor", source)
        self.assertIn("GetActorLocation()", source)

    def test_grounded_scaffold_marks_transform_actors_dirty_and_forces_target_map_save(self) -> None:
        source = BACKDROP_SCRIPT.read_text(encoding="utf-8")
        transform_helper = source.split("def _set_actor_transform", 1)[1].split(
            "def _actors_with_label", 1
        )[0]

        self.assertIn("actor.modify()", transform_helper)
        self.assertIn("root_component.modify()", transform_helper)
        self.assertIn("def _root_component", source)
        self.assertIn('get_editor_property("root_component")', source)
        self.assertIn("root_component = _root_component(actor)", transform_helper)
        self.assertIn("save_asset(TARGET_BATTLE_MAP, only_if_is_dirty=False)", source)
        self.assertIn("transform changed in memory but target map hash did not change", source)

    def test_scaffolding_uses_only_the_required_battle_runtime_actors(self) -> None:
        source = BACKDROP_SCRIPT.read_text(encoding="utf-8")

        self.assertIn("def add_battle_scaffolding", source)
        self.assertIn("GameXXKFlowMapGameMode.static_class()", source)
        self.assertIn("GameXXKBattleScenePresenter.static_class()", source)
        self.assertIn("unreal.CameraActor", source)
        self.assertIn("unreal.PlayerStart", source)
        self.assertIn("unreal.DirectionalLight", source)
        self.assertIn("field_of_view", source)
        self.assertIn("save_current_level", source)
        self.assertIn("def _camera_component", source)
        self.assertIn('get_editor_property("camera_component")', source)

    def test_final_validation_allows_nature_plus_exact_battle_scaffold_only(self) -> None:
        backdrop = load_backdrop_module()
        records = [
            {"label": "Landscape", "class_name": "Landscape", "mesh_paths": ()},
            {
                "label": "Foliage",
                "class_name": "InstancedFoliageActor",
                "mesh_paths": ("/Game/Asian_Village/meshes/plants/SM_grass",),
            },
            {
                "label": "Cliff",
                "class_name": "StaticMeshActor",
                "mesh_paths": ("/Game/Asian_Village/meshes/cliff/SM_cliff",),
            },
            {"label": backdrop.PRESENTER_LABEL, "class_name": "GameXXKBattleScenePresenter", "mesh_paths": ()},
            {"label": backdrop.CAMERA_LABEL, "class_name": "CameraActor", "mesh_paths": ()},
            {"label": backdrop.PLAYER_START_LABEL, "class_name": "PlayerStart", "mesh_paths": ()},
            {"label": backdrop.LIGHT_LABEL, "class_name": "DirectionalLight", "mesh_paths": ()},
        ]

        result = backdrop.validate_final_battle_town_records(records)
        self.assertTrue(result["ok"])
        self.assertEqual(result["unexpected_labels"], [])

        invalid = records + [
            {
                "label": "House",
                "class_name": "StaticMeshActor",
                "mesh_paths": ("/Game/Asian_Village/meshes/building/SM_house",),
            }
        ]
        result = backdrop.validate_final_battle_town_records(invalid)
        self.assertFalse(result["ok"])
        self.assertEqual(result["unexpected_labels"], ["House"])

    def test_map_package_paths_cannot_escape_project_content(self) -> None:
        backdrop = load_backdrop_module()

        self.assertEqual(
            backdrop.map_file_for_package(backdrop.SOURCE_MAP),
            PROJECT_ROOT / "Content" / "GameXXK" / "Maps" / "Prototype" / "L_Qingshan_AsianVillage_Demo.umap",
        )
        with self.assertRaises(ValueError):
            backdrop.map_file_for_package("/Engine/Maps/Entry")

    def test_baseline_records_source_and_legacy_battle_hashes(self) -> None:
        backdrop = load_backdrop_module()

        baseline = backdrop.map_hash_baseline()
        self.assertEqual(baseline["source_map"], backdrop.SOURCE_MAP)
        self.assertEqual(baseline["legacy_battle_map"], backdrop.LEGACY_BATTLE_MAP)
        self.assertEqual(len(baseline["source_sha256"]), 64)
        self.assertEqual(len(baseline["legacy_battle_sha256"]), 64)

    def test_duplicate_manifest_rejects_any_protected_map_change(self) -> None:
        backdrop = load_backdrop_module()
        before = {
            "source_map": backdrop.SOURCE_MAP,
            "source_sha256": "a" * 64,
            "legacy_battle_map": backdrop.LEGACY_BATTLE_MAP,
            "legacy_battle_sha256": "b" * 64,
        }
        after = dict(before)

        manifest = backdrop.build_duplicate_manifest(before, after, "c" * 64)
        self.assertEqual(manifest["stage"], "duplicate_only")
        self.assertEqual(manifest["target_map"], backdrop.TARGET_BATTLE_MAP)
        self.assertEqual(manifest["target_sha256"], "c" * 64)

        changed_source = dict(after, source_sha256="d" * 64)
        with self.assertRaises(RuntimeError):
            backdrop.build_duplicate_manifest(before, changed_source, "c" * 64)

        changed_legacy = dict(after, legacy_battle_sha256="e" * 64)
        with self.assertRaises(RuntimeError):
            backdrop.build_duplicate_manifest(before, changed_legacy, "c" * 64)


if __name__ == "__main__":
    unittest.main()
