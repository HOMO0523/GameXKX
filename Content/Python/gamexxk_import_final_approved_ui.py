from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = (
    PROJECT_ROOT
    / "SourceArt"
    / "UI"
    / "PSD"
    / "gamexxk-v4"
    / "ui-master"
    / "final-approved-runtime-assets-manifest.json"
)
DESTINATION = "/Game/GameXXK/UI/MasterV2/Approved"
REPORT_PATH = PROJECT_ROOT / "Saved" / "GameXXK" / "FinalApprovedUI" / "asset_probe.json"


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


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
    object_path = f"{package_path}.{asset_name}"
    return (
        unreal.EditorAssetLibrary.load_asset(object_path)
        or unreal.EditorAssetLibrary.load_asset(package_path)
    )


def _import_texture(source: Path, asset_name: str) -> unreal.Texture2D:
    if not source.is_file():
        raise RuntimeError(f"missing approved source image: {source}")

    existing = _load_texture(asset_name)
    if isinstance(existing, unreal.Texture2D):
        _configure_texture(existing)
        unreal.EditorAssetLibrary.save_loaded_asset(existing)
        return existing

    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = DESTINATION
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = False
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = _load_texture(asset_name)
    if not isinstance(texture, unreal.Texture2D):
        loaded_class = texture.get_class().get_name() if texture else "None"
        raise RuntimeError(
            f"failed to import approved Texture2D {asset_name}; loaded_class={loaded_class}"
        )
    _configure_texture(texture)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
    return texture


def main() -> None:
    if not MANIFEST_PATH.is_file():
        raise RuntimeError(f"missing approved UI manifest: {MANIFEST_PATH}")
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8-sig"))
    records = manifest.get("assets", [])
    if not records:
        raise RuntimeError("approved UI manifest contains no assets")

    _ensure_directory(DESTINATION)
    imported = []
    for record in records:
        asset_name = str(record["name"])
        source = PROJECT_ROOT / "SourceArt" / "UI" / "PSD" / "gamexxk-v4" / "ui-master" / str(record["file"])
        actual_hash = _sha256(source)
        expected_hash = str(record["sha256"]).lower()
        if actual_hash != expected_hash:
            raise RuntimeError(
                f"approved source hash mismatch for {asset_name}: {actual_hash} != {expected_hash}"
            )
        texture = _import_texture(source, asset_name)
        imported.append(
            {
                "name": asset_name,
                "source": str(source.relative_to(PROJECT_ROOT)).replace("\\", "/"),
                "sourceSha256": actual_hash,
                "assetPath": texture.get_path_name(),
                "width": int(record["width"]),
                "height": int(record["height"]),
            }
        )

    unreal.EditorAssetLibrary.save_directory(DESTINATION, only_if_is_dirty=False, recursive=True)
    report = {
        "status": "PASS",
        "manifest": str(MANIFEST_PATH.relative_to(PROJECT_ROOT)).replace("\\", "/"),
        "manifestSha256": _sha256(MANIFEST_PATH),
        "destination": DESTINATION,
        "assetCount": len(imported),
        "assets": imported,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
