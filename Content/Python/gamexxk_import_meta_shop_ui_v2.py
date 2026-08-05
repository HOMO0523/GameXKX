from __future__ import annotations

import json
from pathlib import Path

import unreal

PROJECT_ROOT = Path(__file__).resolve().parents[2]
CALIBRATION_ROOT = PROJECT_ROOT / "SourceArt/UI/PSD/gamexxk-v4/calibration-v2"
MANIFEST_PATH = CALIBRATION_ROOT / "content-manifest.json"
DESTINATION = "/Game/GameXXK/UI/MetaShop/V2"

IMPORTS = {
    "pojun_weapon": "T_MetaShop_PoJunPack",
    "xuanjia_weapon": "T_MetaShop_XuanJiaPack",
    "qingnang_weapon": "T_MetaShop_QingNangPack",
    "zhuifeng_weapon": "T_MetaShop_ZhuiFengPack",
    "shigu_weapon": "T_MetaShop_ShiGuPack",
    "shanhe_weapon": "T_MetaShop_ShanHePack",
    "nav_companion": "T_MetaShop_CompanionPack",
}


def _configure(texture: unreal.Texture2D) -> None:
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)


def _verify(texture: unreal.Texture2D, asset_path: str) -> None:
    expected = {
        "compression_settings": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
        "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        "srgb": True,
        "never_stream": True,
    }
    for property_name, expected_value in expected.items():
        actual = texture.get_editor_property(property_name)
        if actual != expected_value:
            raise RuntimeError(f"{asset_path} has invalid {property_name}: {actual}")


def main() -> None:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    indexed = {entry["name"]: entry for entry in manifest["content"]}
    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
        unreal.EditorAssetLibrary.make_directory(DESTINATION)

    imported: list[str] = []
    for source_name, asset_name in IMPORTS.items():
        entry = indexed.get(source_name)
        if not entry:
            raise RuntimeError(f"approved content manifest is missing {source_name}")
        source = (CALIBRATION_ROOT / entry["file"]).resolve()
        if not source.is_relative_to(CALIBRATION_ROOT.resolve()) or not source.is_file():
            raise RuntimeError(f"approved source is missing or invalid: {source_name}")

        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = DESTINATION
        task.destination_name = asset_name
        task.automated = True
        task.replace_existing = True
        task.save = False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

        asset_path = f"{DESTINATION}/{asset_name}"
        texture = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not isinstance(texture, unreal.Texture2D):
            raise RuntimeError(f"failed to import Texture2D: {asset_path}")
        _configure(texture)
        if not unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False):
            raise RuntimeError(f"failed to save Texture2D: {asset_path}")
        _verify(texture, asset_path)
        imported.append(texture.get_path_name())

    print(json.dumps({"ok": True, "imported_count": len(imported), "imported": imported}, ensure_ascii=False))


if __name__ == "__main__":
    main()
