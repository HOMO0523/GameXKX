from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
FRAME_ROOT = PROJECT_ROOT / "SourceAssets" / "AnimationProcessing" / "Production"
DESTINATION = "/Game/GameXXK/UI/MasterV2/Approved"
REPORT_PATH = PROJECT_ROOT / "Saved" / "GameXXK" / "PartnerFullBodies" / "import_probe.json"

# Role -> (character directory, asset suffix)
ROLE_FRAMES = [
    ("blade", "character_01_blade_idle", "Blade"),
    ("guard", "character_02_guard_idle", "Guard"),
    ("healer", "character_03_healer_idle", "Healer"),
    ("hunter", "character_04_hunter_idle", "Hunter"),
    ("sorcerer", "character_05_sorcerer_idle", "Sorcerer"),
    ("formation_master", "character_06_formation_master_idle", "FormationMaster"),
]


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


def _load_texture(asset_name: str):
    package_path = f"{DESTINATION}/{asset_name}"
    object_path = f"{package_path}.{asset_name}"
    return (
        unreal.EditorAssetLibrary.load_asset(object_path)
        or unreal.EditorAssetLibrary.load_asset(package_path)
    )


def _import_texture(source: Path, asset_name: str) -> unreal.Texture2D:
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
    return _load_texture(asset_name)


def main() -> None:
    records = []
    for role, directory, suffix in ROLE_FRAMES:
        source = FRAME_ROOT / directory / "frames" / "frame_0000.png"
        asset_name = f"T_MasterV2_CompanionFullBody_{suffix}"
        if not source.is_file():
            records.append({"role": role, "asset": asset_name, "status": "missing-source", "path": str(source)})
            continue
        texture = _import_texture(source, asset_name)
        if not isinstance(texture, unreal.Texture2D):
            records.append({"role": role, "asset": asset_name, "status": "import-failed"})
            continue
        _configure_texture(texture)
        unreal.EditorAssetLibrary.save_loaded_asset(texture)
        records.append({
            "role": role,
            "asset": asset_name,
            "status": "ok",
            "sourceSha256": _sha256(source),
            "size": [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()],
        })

    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(
        json.dumps({"records": records, "count": len(records)}, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    failed = [r for r in records if r.get("status") != "ok"]
    print(f"partner full bodies: {len(records) - len(failed)} ok, {len(failed)} failed")
    if failed:
        print(json.dumps(failed, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
