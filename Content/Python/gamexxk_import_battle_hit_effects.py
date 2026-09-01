from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(str(unreal.Paths.project_dir())).resolve()
SOURCE_ROOT = PROJECT_ROOT / "SourceAssets/AnimationProcessing/HitEffects"
DESTINATION = "/Game/GameXXK/UI/Battle/HitEffects"
REPORT = PROJECT_ROOT / "Saved/HarnessReports/battle-hit-effects/import-report.json"
ASSETS = (
    ("battle_hit_effect_01", "T_BattleHitEffect_01"),
    ("battle_hit_effect_02", "T_BattleHitEffect_02"),
    ("battle_hit_effect_03", "T_BattleHitEffect_03"),
    ("battle_hit_effect_04", "T_BattleHitEffect_04"),
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def texture_size(texture: unreal.Texture2D) -> tuple[int, int]:
    for width_name, height_name in (
        ("blueprint_get_size_x", "blueprint_get_size_y"),
        ("get_size_x", "get_size_y"),
    ):
        width = getattr(texture, width_name, None)
        height = getattr(texture, height_name, None)
        if callable(width) and callable(height):
            return int(width()), int(height())
    raise RuntimeError("could not read hit-effect texture size")


def configure(texture: unreal.Texture2D) -> None:
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)


def main() -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
        unreal.EditorAssetLibrary.make_directory(DESTINATION)
    imported = []
    for source_id, texture_name in ASSETS:
        manifest_path = SOURCE_ROOT / source_id / "manifest.json"
        payload = json.loads(manifest_path.read_text(encoding="utf-8"))
        variant = payload["variants"]["2K"]
        atlas_path = PROJECT_ROOT / variant["atlas"]
        actual_hash = sha256(atlas_path)
        if actual_hash != variant["sha256"]:
            raise RuntimeError(f"{source_id}: atlas hash drifted")
        task = unreal.AssetImportTask()
        task.filename = str(atlas_path)
        task.destination_path = DESTINATION
        task.destination_name = texture_name
        task.automated = True
        task.save = False
        task.replace_existing = True
        task.replace_existing_settings = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        asset_path = f"{DESTINATION}/{texture_name}"
        texture = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not isinstance(texture, unreal.Texture2D):
            raise RuntimeError(f"failed to import {asset_path}")
        if texture_size(texture) != (2048, 2048):
            raise RuntimeError(f"{asset_path}: expected 2048 square")
        configure(texture)
        if not unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False):
            raise RuntimeError(f"failed to save {asset_path}")
        imported.append(
            {
                "sourceId": source_id,
                "assetPath": texture.get_path_name(),
                "sourceSha256": actual_hash,
                "frameCount": int(payload["frameCount"]),
                "size": [2048, 2048],
            }
        )
    report = {"status": "PASS", "destination": DESTINATION, "assets": imported}
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
