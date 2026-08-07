from __future__ import annotations

import hashlib
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE = PROJECT_ROOT / "outputs" / "UI_PSD" / "Candidates" / "00_TigerHero_MainMenu_Illustration.png"
DESTINATION = "/Game/GameXXK/UI/MainMenu/Textures"
ASSET_NAME = "T_MainMenuCover"


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


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


def main() -> None:
    if not SOURCE.is_file():
        print(f"missing source: {SOURCE}")
        return

    task = unreal.AssetImportTask()
    task.filename = str(SOURCE)
    task.destination_path = DESTINATION
    task.destination_name = ASSET_NAME
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    package_path = f"{DESTINATION}/{ASSET_NAME}"
    texture = unreal.EditorAssetLibrary.load_asset(f"{package_path}.{ASSET_NAME}") or unreal.EditorAssetLibrary.load_asset(package_path)
    if isinstance(texture, unreal.Texture2D):
        _configure_texture(texture)
        unreal.EditorAssetLibrary.save_loaded_asset(texture)
        print(f"main menu cover replaced: {ASSET_NAME} size={texture.blueprint_get_size_x()}x{texture.blueprint_get_size_y()} sha={_sha256(SOURCE)[:16]}")
    else:
        print("import failed: texture not loaded")


if __name__ == "__main__":
    main()
