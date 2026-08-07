from __future__ import annotations

from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PRODUCTION = PROJECT_ROOT / "SourceAssets" / "AnimationProcessing" / "Production"
DESTINATION = "/Game/GameXXK/Characters/Follower/Textures"

ROLES = [
    ("07_tusi_chief", "TusiChief"),
    ("08_song_jin_bao", "SongJinBao"),
    ("09_yue_bai", "YueBai"),
    ("10_zhou_guang_zu", "ZhouGuangZu"),
    ("11_jin_gui", "JinGui"),
    ("12_qiong_mei_er", "QiongMeiEr"),
]


def _try_set(obj, property_name: str, value) -> None:
    try:
        obj.set_editor_property(property_name, value)
    except Exception:
        pass


def _configure_texture(texture: unreal.Texture2D) -> None:
    _try_set(texture, "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    _try_set(texture, "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    _try_set(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    _try_set(texture, "srgb", True)
    _try_set(texture, "never_stream", True)
    _try_set(texture, "filter", unreal.TextureFilter.TF_BILINEAR)


def _load_texture(asset_name: str):
    package_path = f"{DESTINATION}/{asset_name}"
    return unreal.EditorAssetLibrary.load_asset(f"{package_path}.{asset_name}") or unreal.EditorAssetLibrary.load_asset(package_path)


def main() -> None:
    unreal.EditorAssetLibrary.make_directory(DESTINATION)
    results = []
    for folder, suffix in ROLES:
        source = PRODUCTION / f"character_{folder}_idle" / "frames" / "frame_0000.png"
        asset_name = f"T_Npc_{suffix}_IdleFirst"
        if not source.is_file():
            results.append((asset_name, "missing-source"))
            continue
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = DESTINATION
        task.destination_name = asset_name
        task.automated = True
        task.replace_existing = True
        task.save = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        package_path = f"{DESTINATION}/{asset_name}"
        print("asset_exists:", unreal.EditorAssetLibrary.does_asset_exist(package_path), package_path)
        texture = _load_texture(asset_name)
        if not isinstance(texture, unreal.Texture2D):
            texture = unreal.EditorAssetLibrary.load_asset(package_path)
        if isinstance(texture, unreal.Texture2D):
            _configure_texture(texture)
            unreal.EditorAssetLibrary.save_loaded_asset(texture)
            results.append((asset_name, f"ok {texture.blueprint_get_size_x()}x{texture.blueprint_get_size_y()}"))
        else:
            results.append((asset_name, "import-failed"))
    for name, status in results:
        print(name, status)


if __name__ == "__main__":
    main()
