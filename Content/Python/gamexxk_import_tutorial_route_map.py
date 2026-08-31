"""Import the approved Xu Xiake route-map inspection texture with exact guards."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE = PROJECT_ROOT / "SourceArt/Narrative/Tutorial/XuXiakeTravelRoute.jpg"
DESTINATION_PATH = "/Game/GameXXK/Narrative/Items"
DESTINATION_NAME = "T_Tutorial_XuXiakeTravelRouteInspect"
DESTINATION = f"{DESTINATION_PATH}/{DESTINATION_NAME}"
EXPECTED_SHA256 = "195cb969ebe91691d585934f1265d81b3e285a36b81efaa723e14b2cf52d36bc"
EXPECTED_SIZE = (1279, 1706)


def _jpeg_size(data: bytes) -> tuple[int, int]:
    if len(data) < 4 or data[:2] != b"\xff\xd8":
        raise RuntimeError("approved route-map source is not a JPEG")
    index = 2
    start_of_frame = {
        0xC0,
        0xC1,
        0xC2,
        0xC3,
        0xC5,
        0xC6,
        0xC7,
        0xC9,
        0xCA,
        0xCB,
        0xCD,
        0xCE,
        0xCF,
    }
    while index + 4 <= len(data):
        if data[index] != 0xFF:
            index += 1
            continue
        while index < len(data) and data[index] == 0xFF:
            index += 1
        if index >= len(data):
            break
        marker = data[index]
        index += 1
        if marker in {0xD8, 0xD9}:
            continue
        if index + 2 > len(data):
            break
        length = int.from_bytes(data[index : index + 2], "big")
        if length < 2 or index + length > len(data):
            break
        if marker in start_of_frame and length >= 7:
            height = int.from_bytes(data[index + 3 : index + 5], "big")
            width = int.from_bytes(data[index + 5 : index + 7], "big")
            return width, height
        index += length
    raise RuntimeError("could not read approved route-map JPEG dimensions")


def import_texture() -> dict[str, object]:
    data = SOURCE.read_bytes()
    source_hash = hashlib.sha256(data).hexdigest()
    if source_hash != EXPECTED_SHA256:
        raise RuntimeError("Xu Xiake route-map source hash drifted")
    source_size = _jpeg_size(data)
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

    texture = unreal.EditorAssetLibrary.load_asset(DESTINATION)
    if texture is None or not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"tutorial route-map texture import failed: {DESTINATION}")
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("srgb", True)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False):
        raise RuntimeError(f"could not save tutorial route-map texture: {DESTINATION}")

    report = {
        "ok": True,
        "source": str(SOURCE.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "source_sha256": source_hash,
        "source_size": list(source_size),
        "asset": texture.get_path_name(),
    }
    print(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True))
    return report


if __name__ == "__main__":
    import_texture()
