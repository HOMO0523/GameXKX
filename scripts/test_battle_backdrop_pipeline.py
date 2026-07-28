#!/usr/bin/env python3
"""Static provenance contract for the generated PSD-style battle backdrop."""

from __future__ import annotations

import hashlib
import json
import unittest
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
ASSET_ROOT = PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "battle-backdrop"
MANIFEST_PATH = ASSET_ROOT / "battle-arena-manifest-v1.json"
IMPORTER_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_import_battle_backdrop.py"
APPLIER_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_apply_battle_backdrop.py"
VALIDATOR_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_validate_battle_backdrop.py"


def _source(path: Path) -> str:
    return path.read_text(encoding="utf-8")


class BattleBackdropPipelineTests(unittest.TestCase):
    def test_manifest_locks_generated_source_and_psd_style_provenance(self) -> None:
        self.assertTrue(MANIFEST_PATH.is_file())
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))

        self.assertEqual(manifest["schema_version"], 1)
        self.assertEqual(manifest["generation_mode"], "built_in_imagegen_style_reference")
        self.assertEqual(manifest["asset_key"], "BattleArena.Riverside.GeneratedV1")
        self.assertEqual(manifest["source_alpha_policy"], "opaque_background")
        self.assertIn("000.png", manifest["style_reference"]["path"])
        self.assertIn("style reference only", manifest["style_reference"]["role"])
        self.assertIn("no UI", manifest["generation_prompt"])
        self.assertIn("no words", manifest["generation_prompt"])

        source = ASSET_ROOT / manifest["source_image"]
        self.assertTrue(source.is_file())
        self.assertEqual(
            hashlib.sha256(source.read_bytes()).hexdigest(), manifest["source_sha256"]
        )
        with Image.open(source) as image:
            self.assertEqual(list(image.size), manifest["source_size"])
            self.assertGreaterEqual(image.width / image.height, 1.70)
            self.assertLessEqual(image.width / image.height, 1.80)
            self.assertEqual(image.mode, "RGB")

    def test_destination_names_are_isolated_battle_assets(self) -> None:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
        self.assertEqual(
            manifest["planned_unreal_asset"],
            "/Game/GameXXK/UI/Battle/Textures/T_BattleArena_Riverside_GeneratedV1",
        )
        self.assertEqual(
            manifest["planned_material_asset"],
            "/Game/GameXXK/UI/Battle/Materials/M_BattleArena_Riverside_GeneratedV1",
        )

    def test_importer_builds_an_isolated_world_texture_and_substrate_unlit_material(self) -> None:
        self.assertTrue(IMPORTER_PATH.is_file())
        source = _source(IMPORTER_PATH)

        self.assertIn("TEXTURE_ASSET_PATH = \"/Game/GameXXK/UI/Battle/Textures/T_BattleArena_Riverside_GeneratedV1\"", source)
        self.assertIn("MATERIAL_ASSET_PATH = \"/Game/GameXXK/UI/Battle/Materials/M_BattleArena_Riverside_GeneratedV1\"", source)
        self.assertIn("unreal.AssetImportTask", source)
        self.assertIn("task.replace_existing = False", source)
        self.assertIn("unreal.TextureGroup.TEXTUREGROUP_World", source)
        self.assertIn("unreal.TextureFilter.TF_TRILINEAR", source)
        self.assertIn("unreal.TextureAddress.TA_CLAMP", source)
        self.assertIn("unreal.TextureCompressionSettings.TC_DEFAULT", source)
        self.assertIn("unreal.MaterialFactoryNew", source)
        self.assertIn("unreal.MaterialExpressionTextureSampleParameter2D", source)
        self.assertIn("BattleBackdropTexture", source)
        self.assertIn("unreal.MaterialExpressionSubstrateUnlitBSDF", source)
        self.assertIn("Emissive Color", source)
        self.assertIn("unreal.MaterialProperty.MP_FRONT_MATERIAL", source)
        self.assertIn("unreal.MaterialDomain.MD_SURFACE", source)
        self.assertIn("unreal.BlendMode.BLEND_OPAQUE", source)
        self.assertIn("validate_backdrop_plan", source)
        self.assertIn("source_sha256", source)
        self.assertNotIn("task.replace_existing = True", source)
        self.assertNotIn("delete_asset(", source)
        self.assertNotIn("duplicate_asset(", source)

    def test_applier_only_allows_the_generated_battle_floor_override(self) -> None:
        self.assertTrue(APPLIER_PATH.is_file())
        source = _source(APPLIER_PATH)

        self.assertIn("MAP_PATH = \"/Game/GameXXK/Maps/L_BattleScene\"", source)
        self.assertIn("FLOOR_LABEL = \"GameXXK_Encounter_Floor\"", source)
        self.assertIn("/Engine/BasicShapes/Plane", source)
        self.assertIn("/Engine/EngineMaterials/WorldGridMaterial", source)
        self.assertIn("MATERIAL_ASSET_PATH", source)
        self.assertIn("unreal.StaticMeshActor", source)
        self.assertIn("get_num_materials() != 1", source)
        self.assertIn("allowed_materials = {WORLD_GRID_MATERIAL_PATH, MATERIAL_ASSET_PATH}", source)
        self.assertIn("if current_material_path not in allowed_materials", source)
        self.assertIn("set_material(0, target_material)", source)
        self.assertIn("before_transform", source)
        self.assertIn("after_transform", source)
        self.assertIn("transform changed", source)
        self.assertIn("save_current_level", source)
        self.assertIn("current map is not the guarded battle scene", source)

        for forbidden in (
            "spawn_actor",
            "destroy_actor",
            "set_actor_location",
            "set_actor_rotation",
            "set_actor_scale3d",
            "set_static_mesh",
            "set_editor_property",
            "CameraActor",
            "camera_component",
        ):
            self.assertNotIn(forbidden, source)

    def test_validator_is_read_only_and_reuses_the_guarded_contract(self) -> None:
        self.assertTrue(VALIDATOR_PATH.is_file())
        source = _source(VALIDATOR_PATH)

        self.assertIn("validate_backdrop_plan", source)
        self.assertIn("validate_imported_backdrop", source)
        self.assertIn("inspect_floor", source)
        self.assertIn("MATERIAL_ASSET_PATH", source)
        for forbidden in (
            "AssetImportTask",
            "set_editor_property",
            "save_loaded_asset",
            "save_current_level",
            "set_material(",
        ):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
