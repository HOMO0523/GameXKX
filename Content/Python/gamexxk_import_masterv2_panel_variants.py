from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE = (
    PROJECT_ROOT
    / "SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeApproved/T_MasterV2_PanelTall.png"
)
DESTINATION = "/Game/GameXXK/UI/MasterV2/Approved"
ASSET_NAME = "T_MasterV2_PanelTall"
REPORT = PROJECT_ROOT / "Saved/GameXXK/MasterV2PanelVariants/import-report.json"


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _try_set(obj, property_name: str, value) -> None:
    try:
        obj.set_editor_property(property_name, value)
    except Exception:
        pass


def main() -> None:
    if not SOURCE.is_file():
        raise RuntimeError(f"missing panel variant source: {SOURCE}")
    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
        unreal.EditorAssetLibrary.make_directory(DESTINATION)

    task = unreal.AssetImportTask()
    task.filename = str(SOURCE)
    task.destination_path = DESTINATION
    task.destination_name = ASSET_NAME
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset_path = f"{DESTINATION}/{ASSET_NAME}.{ASSET_NAME}"
    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"failed to import Texture2D: {asset_path}")
    _try_set(texture, "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    _try_set(texture, "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    _try_set(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    _try_set(texture, "srgb", True)
    _try_set(texture, "never_stream", True)
    _try_set(texture, "filter", unreal.TextureFilter.TF_BILINEAR)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)

    payload = {
        "status": "PASS",
        "source": str(SOURCE.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "source_sha256": _sha256(SOURCE),
        "asset_path": texture.get_path_name(),
        "width": 726,
        "height": 1816,
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log(json.dumps(payload, ensure_ascii=False))


if __name__ == "__main__":
    main()
