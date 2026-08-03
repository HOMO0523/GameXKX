#!/usr/bin/env python3
"""Pure contracts for the fullscreen battle HUD asset importer."""

from __future__ import annotations

import contextlib
import hashlib
import io
import json
import sys
import unittest
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
IMPORTER_PATH = (
    PROJECT_ROOT
    / "Content"
    / "Python"
    / "gamexxk_import_fullscreen_battle_hud_assets.py"
)
sys.path.insert(0, str(IMPORTER_PATH.parent))

import gamexxk_import_fullscreen_battle_hud_assets as importer


class FullscreenBattleHudAssetImportTests(unittest.TestCase):
    def test_asset_contract_is_exact(self) -> None:
        self.assertEqual(
            importer.SOURCE_IMAGE,
            PROJECT_ROOT
            / "SourceAssets"
            / "PartyDeck"
            / "battle-backdrop"
            / "battle_arena_riverside_source_v1.png",
        )
        self.assertEqual(
            importer.SOURCE_IMAGE_SHA256,
            "ab8b882de676b74693cb2d3c279ca11f2126aaa4fdf382dedaa810b34ac1a3a8",
        )
        self.assertEqual(importer.SOURCE_IMAGE_SIZE, (1672, 941))
        self.assertEqual(importer.SOURCE_IMAGE_MODE, "RGB")
        self.assertEqual(
            importer.BACKDROP_PACKAGE,
            "/Game/GameXXK/UI/Battle/Textures/T_BattleArena_Riverside_GeneratedV1",
        )
        self.assertEqual(
            importer.MATERIAL_PACKAGE,
            "/Game/GameXXK/UI/Battle/Materials/M_BattleUnitAtlasUI",
        )
        self.assertEqual(importer.ATLAS_COLUMNS, 8)
        self.assertEqual(importer.ATLAS_ROWS, 8)

    def test_source_is_locked_opaque_rgb(self) -> None:
        self.assertTrue(importer.SOURCE_IMAGE.is_file())
        self.assertEqual(
            hashlib.sha256(importer.SOURCE_IMAGE.read_bytes()).hexdigest(),
            importer.SOURCE_IMAGE_SHA256,
        )
        with Image.open(importer.SOURCE_IMAGE) as image:
            self.assertEqual(image.size, importer.SOURCE_IMAGE_SIZE)
            self.assertEqual(image.mode, importer.SOURCE_IMAGE_MODE)
            self.assertNotIn("A", image.getbands())
            self.assertNotIn("transparency", image.info)

        self.assertEqual(
            importer.validate_source_image(),
            {
                "path": str(importer.SOURCE_IMAGE),
                "sha256": importer.SOURCE_IMAGE_SHA256,
                "size": [1672, 941],
                "mode": "RGB",
                "opaque": True,
            },
        )

    def test_asset_registry_dimensions_are_parsed_without_runtime_fallback_size(self) -> None:
        self.assertEqual(importer.parse_dimensions_tag("1672x941"), (1672, 941))
        self.assertEqual(importer.parse_dimensions_tag(" 1672 x 941 "), (1672, 941))
        for invalid in ("", "32", "1672*941", "0x941", "1672x0"):
            with self.subTest(invalid=invalid):
                with self.assertRaises(ValueError):
                    importer.parse_dimensions_tag(invalid)

    def test_policy_contracts_are_ui_safe(self) -> None:
        self.assertEqual(
            importer.TEXTURE_POLICY,
            {
                "lod_group": "TEXTUREGROUP_UI",
                "mip_gen_settings": "TMGS_NO_MIPMAPS",
                "filter": "TF_BILINEAR",
                "srgb": True,
                "address_x": "TA_CLAMP",
                "address_y": "TA_CLAMP",
            },
        )

    def test_only_the_exact_engine_default_texture_is_allowed(self) -> None:
        self.assertTrue(
            importer.is_allowed_default_texture(importer.ENGINE_DEFAULT_TEXTURE)
        )
        self.assertFalse(
            importer.is_allowed_default_texture(
                "/Engine/EngineResources/DefaultNormal.DefaultNormal"
            )
        )
        self.assertFalse(
            importer.is_allowed_default_texture(
                "/Game/GameXXK/UI/Battle/Textures/T_AnyAtlas.T_AnyAtlas"
            )
        )

    def test_material_input_candidates_never_fall_back_to_unrelated_pins(self) -> None:
        self.assertEqual(
            importer.material_input_candidates(
                ("UVs", "Coordinates"),
                ("Coordinates", "TextureObject", "MipValue"),
            ),
            ["Coordinates", "UVs"],
        )
        self.assertNotIn(
            "MipValue",
            importer.material_input_candidates(
                ("UVs", "Coordinates"),
                ("TextureObject", "MipValue"),
            ),
        )

    def test_material_input_bindings_preserve_pin_names_and_unwired_slots(self) -> None:
        add_node = object()
        bindings = importer.material_input_bindings(
            ("UVs", "TextureObject", "MipValue"),
            (add_node, None, None),
        )
        self.assertEqual(
            bindings,
            [("UVs", add_node), ("TextureObject", None), ("MipValue", None)],
        )
        self.assertEqual(
            importer.MATERIAL_POLICY,
            {
                "material_domain": "MD_UI",
                "blend_mode": "BLEND_TRANSLUCENT",
                "texture_parameter": "AtlasTexture",
                "scalar_parameters": {
                    "FrameColumn": 0.0,
                    "FrameRow": 0.0,
                },
                "atlas_columns": 8,
                "atlas_rows": 8,
                "color_output": "MP_EMISSIVE_COLOR",
                "alpha_output": "MP_OPACITY",
            },
        )

    def test_asset_plan_is_deterministic_and_idempotent(self) -> None:
        expected_new = {
            "create": [importer.BACKDROP_PACKAGE, importer.MATERIAL_PACKAGE],
            "validate": [],
            "write_count": 2,
        }
        self.assertEqual(importer.build_asset_plan(False, False), expected_new)
        self.assertEqual(importer.build_asset_plan(False, False), expected_new)
        self.assertEqual(
            importer.build_asset_plan(True, True),
            {
                "create": [],
                "validate": [importer.BACKDROP_PACKAGE, importer.MATERIAL_PACKAGE],
                "write_count": 0,
            },
        )
        self.assertEqual(
            importer.build_asset_plan(True, False),
            {
                "create": [importer.MATERIAL_PACKAGE],
                "validate": [importer.BACKDROP_PACKAGE],
                "write_count": 1,
            },
        )

    def test_only_unpersisted_in_memory_targets_are_creation_recovery(self) -> None:
        self.assertEqual(importer.classify_target_action(False, False), "create")
        self.assertEqual(importer.classify_target_action(False, True), "recover")
        self.assertEqual(importer.classify_target_action(True, True), "validate")
        self.assertEqual(importer.classify_target_action(True, False), "validate")

    def test_writes_require_explicit_execute_flag(self) -> None:
        self.assertFalse(importer.parse_arguments([]).execute_import)
        self.assertTrue(
            importer.parse_arguments(["--execute-import"]).execute_import
        )

        stdout = io.StringIO()
        with contextlib.redirect_stdout(stdout):
            exit_code = importer.main([])
        report = json.loads(stdout.getvalue())
        self.assertEqual(exit_code, 0)
        self.assertTrue(report["ok"])
        self.assertEqual(report["mode"], "plan-only")
        self.assertFalse(report["execute_import"])

    def test_import_and_material_authoring_contract_is_collision_safe(self) -> None:
        source = IMPORTER_PATH.read_text(encoding="utf-8")

        for required in (
            "try:\n    import unreal",
            "unreal.AssetImportTask",
            "task.replace_existing = False",
            "task.replace_existing_settings = False",
            "task.save = False",
            "unreal.TextureGroup.TEXTUREGROUP_UI",
            "unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS",
            "unreal.TextureFilter.TF_BILINEAR",
            "unreal.TextureAddress.TA_CLAMP",
            "unreal.MaterialDomain.MD_UI",
            "unreal.BlendMode.BLEND_TRANSLUCENT",
            "unreal.MaterialExpressionTextureCoordinate",
            "unreal.MaterialExpressionScalarParameter",
            "unreal.MaterialExpressionAppendVector",
            "unreal.MaterialExpressionDivide",
            "unreal.MaterialExpressionAdd",
            "unreal.MaterialExpressionTextureSampleParameter2D",
            '"AtlasTexture"',
            '"FrameColumn"',
            '"FrameRow"',
            "unreal.MaterialProperty.MP_EMISSIVE_COLOR",
            "unreal.MaterialProperty.MP_OPACITY",
            "unreal.MaterialEditingLibrary.recompile_material(material) or []",
            "unreal.EditorAssetLibrary.save_loaded_asset",
        ):
            self.assertIn(required, source)

        self.assertEqual(source.count("unreal.EditorAssetLibrary.save_loaded_asset("), 2)

        for forbidden in (
            "gamexxk_import_battle_backdrop",
            "M_BattlePsdResourceMask",
            "L_BattleScene",
            "L_BattleTown",
            "TEXTUREGROUP_World",
            "TF_TRILINEAR",
            "MD_SURFACE",
            "replace_existing = True",
            "replace_existing_settings = True",
            "delete_asset(",
            "delete_material_expression(",
            "save_directory(",
            "save_dirty_packages(",
            "save_current_level(",
            "load_map(",
            "get_all_level_actors(",
            "spawn_actor",
            "destroy_actor",
            "set_actor_",
            "StaticMeshActor",
        ):
            self.assertNotIn(forbidden, source)


if __name__ == "__main__":
    unittest.main()
