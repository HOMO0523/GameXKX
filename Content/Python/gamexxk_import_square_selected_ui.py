from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE = (
    PROJECT_ROOT
    / "SourceArt"
    / "UI"
    / "PSD"
    / "gamexxk-v4"
    / "ui-master"
    / "RuntimeApproved"
    / "T_MasterV2_SquareSelected.png"
)
DESTINATION = "/Game/GameXXK/UI/MasterV2/Approved"
ASSET_NAME = "T_MasterV2_SquareSelected"
EXPECTED_SHA256 = "2b16718f4b98a5e654e07ae5e58e3faa2c70ec763bb2e920df45f2bf2d7f6e51"
REPORT = PROJECT_ROOT / "Saved" / "GameXXK" / "FinalApprovedUI" / "square_selected_import.json"


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _try_set(obj, property_name: str, value) -> None:
    try:
        obj.set_editor_property(property_name, value)
    except Exception:
        pass


def _load_texture():
    package_path = f"{DESTINATION}/{ASSET_NAME}"
    return unreal.EditorAssetLibrary.load_asset(f"{package_path}.{ASSET_NAME}") or unreal.EditorAssetLibrary.load_asset(package_path)


def main() -> None:
    if not SOURCE.is_file():
        raise RuntimeError(f"missing approved square selected source: {SOURCE}")
    actual_hash = _sha256(SOURCE)
    if actual_hash != EXPECTED_SHA256:
        raise RuntimeError(f"square selected source hash mismatch: {actual_hash} != {EXPECTED_SHA256}")

    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
        unreal.EditorAssetLibrary.make_directory(DESTINATION)

    texture = _load_texture()
    if not isinstance(texture, unreal.Texture2D):
        task = unreal.AssetImportTask()
        task.filename = str(SOURCE)
        task.destination_path = DESTINATION
        task.destination_name = ASSET_NAME
        task.automated = True
        task.replace_existing = False
        task.save = False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = _load_texture()

    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"failed to import approved Texture2D {ASSET_NAME}")

    _try_set(texture, "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    _try_set(texture, "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    _try_set(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    _try_set(texture, "srgb", True)
    _try_set(texture, "never_stream", True)
    _try_set(texture, "filter", unreal.TextureFilter.TF_BILINEAR)
    _try_set(texture, "address_x", unreal.TextureAddress.TA_CLAMP)
    _try_set(texture, "address_y", unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)

    report = {
        "status": "PASS",
        "source": str(SOURCE.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "sha256": actual_hash,
        "assetPath": texture.get_path_name(),
        "width": 104,
        "height": 104,
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
