"""Import the approved Xu Xiake route-map inspection texture with exact guards."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE = PROJECT_ROOT / "SourceArt/Narrative/Tutorial/XuXiakeTravelRoute.png"
DESTINATION_PATH = "/Game/GameXXK/Narrative/Items"
DESTINATION_NAME = "T_Tutorial_XuXiakeTravelRouteInspect"
DESTINATION = f"{DESTINATION_PATH}/{DESTINATION_NAME}"
DESTINATION_OBJECT = f"{DESTINATION}.{DESTINATION_NAME}"
EXPECTED_SHA256 = "3f4deb047abe7f73dd1a4ee4c29bff527524b4b95ed153fc09080a73cc82782a"
EXPECTED_SIZE = (2388, 1668)


def _png_size(data: bytes) -> tuple[int, int]:
    if (
        len(data) < 24
        or data[:8] != b"\x89PNG\r\n\x1a\n"
        or data[12:16] != b"IHDR"
    ):
        raise RuntimeError("approved route-map source is not a PNG with IHDR")
    return (
        int.from_bytes(data[16:20], "big"),
        int.from_bytes(data[20:24], "big"),
    )


def import_texture() -> dict[str, object]:
    data = SOURCE.read_bytes()
    source_hash = hashlib.sha256(data).hexdigest()
    if source_hash != EXPECTED_SHA256:
        raise RuntimeError("Xu Xiake route-map source hash drifted")
    source_size = _png_size(data)
    if source_size != EXPECTED_SIZE:
        raise RuntimeError(f"Xu Xiake route-map dimensions drifted: {source_size}")

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(SOURCE))
    task.set_editor_property("destination_path", DESTINATION_PATH)
    task.set_editor_property("destination_name", DESTINATION_NAME)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = unreal.EditorAssetLibrary.load_asset(DESTINATION_OBJECT)
    if texture is None:
        texture = unreal.load_asset(DESTINATION_OBJECT)
    if texture is None or not isinstance(texture, unreal.Texture2D):
        imported_paths = [str(value) for value in task.get_editor_property("imported_object_paths")]
        raise RuntimeError(
            "tutorial route-map texture import failed: "
            f"object={DESTINATION_OBJECT}, exists="
            f"{unreal.EditorAssetLibrary.does_asset_exist(DESTINATION)}, "
            f"imported={imported_paths}, loaded_type="
            f"{type(texture).__name__ if texture is not None else 'None'}"
        )
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    save_result = bool(
        unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    )
    dirty_packages = {
        str(package.get_name())
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    }
    if DESTINATION in dirty_packages:
        raise RuntimeError(
            f"tutorial route-map texture remains dirty after save: {DESTINATION}"
        )
    texture_size = (
        int(texture.blueprint_get_size_x()),
        int(texture.blueprint_get_size_y()),
    )
    if texture_size != EXPECTED_SIZE:
        raise RuntimeError(
            f"imported route-map texture dimensions drifted: {texture_size}"
        )

    report = {
        "ok": True,
        "source": str(SOURCE.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "source_sha256": source_hash,
        "source_size": list(source_size),
        "asset": texture.get_path_name(),
        "asset_size": list(texture_size),
        "save_api_result": save_result,
    }
    print(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True))
    return report


if __name__ == "__main__":
    import_texture()
