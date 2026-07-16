"""Offline tests for the WorldMap UE import and validation scripts."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import types
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
IMPORTER_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_import_map_ui_assets.py"
VALIDATOR_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_validate_map_ui_assets.py"

EXPECTED_IMPORTS = (
    ("WorldMap", "world_map_terrain.png", "T_WorldMap_Terrain"),
    ("WorldMap", "world_map_region_paths.png", "T_WorldMap_RegionPaths"),
    ("WorldMap", "world_map_qingshan_marker.png", "T_WorldMap_QingshanMarker"),
    ("WorldMap", "world_map_locked_marker.png", "T_WorldMap_LockedMarker"),
    ("WorldMap", "world_map_player_marker.png", "T_WorldMap_PlayerMarker"),
    ("WorldMap", "world_map_label_plate.png", "T_WorldMap_RegionLabelPlate"),
)


class FakeTexture:
    def __init__(self, properties: dict[str, object] | None = None) -> None:
        self.properties = dict(properties or {})
        self.saved_properties: dict[str, object] = {}

    def set_editor_property(self, name: str, value: object) -> None:
        self.saved_properties[name] = value
        self.properties[name] = value

    def get_editor_property(self, name: str) -> object:
        if name not in self.properties:
            raise RuntimeError(f"missing property: {name}")
        return self.properties[name]

    def get_path_name(self) -> str:
        return str(self.properties.get(
            "path_name",
            "/Game/GameXXK/UI/Maps/Textures/WorldMap/T_Test.T_Test",
        ))


class FakeImportData:
    def __init__(self, source_filename: str) -> None:
        self.source_filename = source_filename

    def get_first_filename(self) -> str:
        return self.source_filename


def fake_unreal_module() -> types.SimpleNamespace:
    return types.SimpleNamespace(
        Texture2D=FakeTexture,
        TextureMipGenSettings=types.SimpleNamespace(TMGS_NO_MIPMAPS="TMGS_NO_MIPMAPS"),
        TextureCompressionSettings=types.SimpleNamespace(TC_EDITOR_ICON="TC_EDITOR_ICON"),
        TextureGroup=types.SimpleNamespace(TEXTUREGROUP_UI="TEXTUREGROUP_UI"),
        TextureFilter=types.SimpleNamespace(TF_BILINEAR="TF_BILINEAR"),
        TextureAddress=types.SimpleNamespace(TA_CLAMP="TA_CLAMP"),
    )


def load_script(path: Path, module_name: str) -> types.ModuleType:
    if not path.is_file():
        raise AssertionError(f"missing script: {path}")
    original_unreal = sys.modules.get("unreal")
    sys.modules["unreal"] = fake_unreal_module()
    try:
        spec = importlib.util.spec_from_file_location(module_name, path)
        if spec is None or spec.loader is None:
            raise AssertionError(f"cannot load script: {path}")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    finally:
        if original_unreal is None:
            del sys.modules["unreal"]
        else:
            sys.modules["unreal"] = original_unreal


class WorldMapImportScriptTest(unittest.TestCase):
    def test_importer_has_exact_owned_assets_and_selection_contract(self) -> None:
        importer = load_script(IMPORTER_PATH, "gamexxk_import_map_ui_assets_test")

        self.assertEqual(importer.SOURCE_ROOT, PROJECT_ROOT / "docs" / "ui" / "maps" / "source_art")
        self.assertEqual(importer.DESTINATION_ROOT, "/Game/GameXXK/UI/Maps/Textures")
        self.assertEqual(importer.IMPORTS, EXPECTED_IMPORTS)
        self.assertTrue(all("/Game/1Game/" not in asset_name for _, _, asset_name in importer.IMPORTS))
        self.assertEqual(
            importer.select_imports(None, ["T_WorldMap_Terrain"]),
            (EXPECTED_IMPORTS[0],),
        )
        with self.assertRaisesRegex(RuntimeError, "either --groups or --assets"):
            importer.select_imports(["WorldMap"], ["T_WorldMap_Terrain"])
        with self.assertRaisesRegex(RuntimeError, "unknown UI source assets"):
            importer.select_imports(None, ["T_WorldMap_Unknown"])

    def test_importer_configures_ui_texture_properties(self) -> None:
        importer = load_script(IMPORTER_PATH, "gamexxk_import_map_ui_assets_config_test")
        texture = FakeTexture()

        importer.configure_texture(texture)

        self.assertEqual(texture.saved_properties, {
            "mip_gen_settings": "TMGS_NO_MIPMAPS",
            "compression_settings": "TC_EDITOR_ICON",
            "lod_group": "TEXTUREGROUP_UI",
            "filter": "TF_BILINEAR",
            "address_x": "TA_CLAMP",
            "address_y": "TA_CLAMP",
            "srgb": True,
            "never_stream": True,
            "compression_no_alpha": False,
        })

    def test_importer_stops_when_configured_texture_cannot_be_saved(self) -> None:
        importer = load_script(IMPORTER_PATH, "gamexxk_import_map_ui_assets_save_test")
        importer.unreal.EditorAssetLibrary = types.SimpleNamespace(
            save_loaded_asset=lambda _texture: False,
        )

        with self.assertRaisesRegex(RuntimeError, "failed to save imported Texture2D"):
            importer.save_texture(FakeTexture(), "/Game/GameXXK/UI/Maps/Textures/WorldMap/T_Test")

    def test_validator_accepts_owned_ui_texture_and_rejects_unsafe_paths(self) -> None:
        validator = load_script(VALIDATOR_PATH, "gamexxk_validate_map_ui_assets_test")
        owned_path = "/Game/GameXXK/UI/Maps/Textures/WorldMap/T_WorldMap_Terrain"
        valid_texture = FakeTexture({
            "mip_gen_settings": "TMGS_NO_MIPMAPS",
            "compression_settings": "TC_EDITOR_ICON",
            "lod_group": "TEXTUREGROUP_UI",
            "filter": "TF_BILINEAR",
            "address_x": "TA_CLAMP",
            "address_y": "TA_CLAMP",
            "srgb": True,
            "never_stream": True,
            "compression_no_alpha": False,
            "asset_import_data": FakeImportData("D:/GameXXK/docs/ui/maps/source_art/WorldMap/world_map_terrain.png"),
        })

        self.assertEqual(validator.validate_texture(owned_path, valid_texture), [])
        self.assertIn(
            "must remain under /Game/GameXXK/UI/Maps/Textures",
            "\n".join(validator.validate_texture("/Game/1Game/Texture/Map", valid_texture)),
        )
        bad_source = FakeTexture({
            **valid_texture.properties,
            "asset_import_data": FakeImportData("D:/reference/094.png"),
        })
        self.assertIn("baked PSD map", "\n".join(validator.validate_texture(owned_path, bad_source)))
        uppercase_baked_source = FakeTexture({
            **valid_texture.properties,
            "asset_import_data": FakeImportData("D:/reference/094.PNG"),
        })
        self.assertIn(
            "baked PSD map",
            "\n".join(validator.validate_texture(owned_path, uppercase_baked_source)),
        )
        wrong_group = FakeTexture({
            **valid_texture.properties,
            "lod_group": "TEXTUREGROUP_WORLD",
        })
        self.assertIn("lod_group", "\n".join(validator.validate_texture(owned_path, wrong_group)))
        redirected_texture = FakeTexture({
            **valid_texture.properties,
            "path_name": "/Game/1Game/Texture/LegacyMap.LegacyMap",
        })
        self.assertIn(
            "actual object path must remain under /Game/GameXXK/UI/Maps/Textures/WorldMap",
            "\n".join(validator.validate_texture(owned_path, redirected_texture)),
        )
        wrong_subroot_texture = FakeTexture({
            **valid_texture.properties,
            "path_name": "/Game/GameXXK/UI/Maps/Textures/RouteMap/T_Route.T_Route",
        })
        self.assertIn(
            "actual object path must remain under /Game/GameXXK/UI/Maps/Textures/WorldMap",
            "\n".join(validator.validate_texture(owned_path, wrong_subroot_texture)),
        )


if __name__ == "__main__":
    unittest.main()
