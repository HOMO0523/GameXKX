"""Import the reviewed desktop-Training visual MVP textures.

This is an explicit execute-boundary script.  The source files remain under
SourceAssets/SourceArt with their hashes/manifests; only the 2K/1K walkloop
atlases and the transparent seamless strip background are imported into the
runtime Content tree.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
WALK_ROOT = PROJECT_ROOT / "SourceAssets" / "AnimationProcessing" / "walkloop_pilot_v1" / "character_00_hero_walk_left"
BACKGROUND = PROJECT_ROOT / "SourceArt" / "UI" / "PSD" / "desktop-training-v1" / "generated" / "TrainingIdleStrip_Background_GPT_v003_Seamless_RGBA.png"
DEST_ROOT = "/Game/GameXXK/UI/Training/Generated/walkloop_pilot_v1"

IMPORTS = (
    (WALK_ROOT / "atlas_2K" / "character_00_hero_walk_left_atlas.png", f"{DEST_ROOT}/character_00_hero_walk_left/atlas_2K", "T_TrainingHeroWalkLeft_2K"),
    (WALK_ROOT / "atlas_1K" / "character_00_hero_walk_left_atlas.png", f"{DEST_ROOT}/character_00_hero_walk_left/atlas_1K", "T_TrainingHeroWalkLeft_1K"),
    (BACKGROUND, DEST_ROOT, "T_TrainingIdleStrip_Background"),
)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _try_set(obj: object, property_name: str, value: object) -> None:
    try:
        obj.set_editor_property(property_name, value)
    except Exception:
        pass


def _configure_texture(texture: unreal.Texture2D, *, nearest: bool) -> None:
    _try_set(texture, "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    _try_set(texture, "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    _try_set(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    _try_set(texture, "srgb", True)
    _try_set(texture, "never_stream", True)
    _try_set(texture, "filter", unreal.TextureFilter.TF_NEAREST if nearest else unreal.TextureFilter.TF_BILINEAR)


def _import_one(source: Path, destination: str, asset_name: str) -> dict[str, object]:
    if not source.is_file():
        raise RuntimeError(f"missing reviewed source: {source}")
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = destination
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    package_path = f"{destination}/{asset_name}"
    texture = unreal.EditorAssetLibrary.load_asset(package_path)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"import did not yield Texture2D: {package_path}")
    _configure_texture(texture, nearest="walkloop" in source.as_posix().casefold())
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError(f"could not save imported texture: {package_path}")
    return {
        "source": source.relative_to(PROJECT_ROOT).as_posix(),
        "sourceSha256": _sha256(source),
        "asset": f"{package_path}.{asset_name}",
        "size": [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()],
    }


def main() -> None:
    imported = [_import_one(source, destination, asset_name) for source, destination, asset_name in IMPORTS]
    report = {
        "schemaVersion": 1,
        "status": "runtime-mvp",
        "imports": imported,
    }
    report_path = WALK_ROOT / "runtime-import-manifest.json"
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"ok": True, **report}, ensure_ascii=False))


if __name__ == "__main__":
    main()
