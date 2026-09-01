"""Import the approved ten-state horizontal town hero animation set."""

from __future__ import annotations

import json
import math
from pathlib import Path

import unreal


PROJECT_ROOT = Path(str(unreal.Paths.project_dir())).resolve()
MANIFEST_PATH = PROJECT_ROOT / "SourceAssets/AnimationProcessing/TownHeroHorizontal/manifest.json"
ASSET_ROOT = "/Game/GameXXK/Characters/Hero/TownHorizontal"
ATLAS_DIR = f"{ASSET_ROOT}/Atlases"
SPRITE_ROOT = f"{ASSET_ROOT}/Sprites"
FLIPBOOK_DIR = f"{ASSET_ROOT}/Flipbooks"
ATLAS_SIZE = 2048
GRID_COLUMNS = 8
CELL_SIZE = 256
PIXELS_PER_UNREAL_UNIT = 1.0

SUPPLEMENTAL_CLIPS = (
    {
        "candidateId": "cinematic_hero_adjust_backpack",
        "flipbook": "FB_Hero_Town_AdjustBackpack_Left",
        "paperzd": "PZD_Hero_Town_AdjustBackpack",
        "loop": False,
    },
    {
        "candidateId": "cinematic_hero_deep_breath",
        "flipbook": "FB_Hero_Town_DeepBreath_Left",
        "paperzd": "PZD_Hero_Town_DeepBreath",
        "loop": False,
    },
    {
        "candidateId": "cinematic_hero_collect_item_with_ui",
        "flipbook": "FB_Hero_Town_CollectItem_Left",
        "paperzd": "PZD_Hero_Town_CollectItem",
        "loop": False,
    },
    {
        "candidateId": "character_00_hero_combat_idle_candidate",
        "flipbook": "FB_Hero_Town_CombatIdle_Left",
        "paperzd": "PZD_Hero_Town_CombatIdle",
        "loop": True,
    },
    {
        "candidateId": "character_00_hero_attack_punch_candidate",
        "flipbook": "FB_Hero_Town_Punch_Left",
        "paperzd": "PZD_Hero_Town_Punch",
        "loop": False,
    },
    {
        "candidateId": "character_00_hero_attack_kick_candidate",
        "flipbook": "FB_Hero_Town_Kick_Left",
        "paperzd": "PZD_Hero_Town_Kick",
        "loop": False,
    },
)


def ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        if not unreal.EditorAssetLibrary.make_directory(path):
            raise RuntimeError(f"could not create directory: {path}")


def save(asset: object) -> None:
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"could not save asset: {asset.get_path_name()}")


def create_or_load(name: str, directory: str, asset_class: object, factory: object):
    path = f"{directory}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if asset is None:
            raise RuntimeError(f"could not load asset: {path}")
        return asset, False
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, directory, asset_class, factory
    )
    if asset is None:
        raise RuntimeError(f"could not create asset: {path}")
    return asset, True


def texture_size(texture: object) -> tuple[int, int]:
    for width_name, height_name in (
        ("blueprint_get_size_x", "blueprint_get_size_y"),
        ("get_size_x", "get_size_y"),
    ):
        width = getattr(texture, width_name, None)
        height = getattr(texture, height_name, None)
        if callable(width) and callable(height):
            return int(width()), int(height())
    raise RuntimeError("could not query texture dimensions")


def configure_texture(texture: object) -> None:
    texture.set_editor_property(
        "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
    )
    texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    texture.set_editor_property(
        "compression_settings", unreal.TextureCompressionSettings.TC_BC7
    )
    texture.set_editor_property("srgb", True)
    for prop_name, value in (
        ("never_stream", True),
        ("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI),
    ):
        try:
            texture.set_editor_property(prop_name, value)
        except Exception:
            pass


def import_texture(source: Path, texture_name: str):
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = ATLAS_DIR
    task.destination_name = texture_name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.replace_existing_settings = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    path = f"{ATLAS_DIR}/{texture_name}"
    texture = unreal.EditorAssetLibrary.load_asset(path)
    if texture is None or not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"could not import Texture2D: {path}")
    if texture_size(texture) != (ATLAS_SIZE, ATLAS_SIZE):
        raise RuntimeError(
            f"{texture_name}: expected {ATLAS_SIZE}x{ATLAS_SIZE}, got {texture_size(texture)}"
        )
    configure_texture(texture)
    save(texture)
    return texture


def configure_sprite(sprite: object, texture: object, frame_index: int) -> None:
    column = frame_index % GRID_COLUMNS
    row = frame_index // GRID_COLUMNS
    sprite.set_editor_property("source_texture", texture)
    sprite.set_editor_property(
        "source_uv", unreal.Vector2D(float(column * CELL_SIZE), float(row * CELL_SIZE))
    )
    sprite.set_editor_property(
        "source_dimension", unreal.Vector2D(float(CELL_SIZE), float(CELL_SIZE))
    )
    sprite.set_editor_property(
        "source_texture_dimension", unreal.Vector2D(float(ATLAS_SIZE), float(ATLAS_SIZE))
    )
    sprite.set_editor_property("pixels_per_unreal_unit", PIXELS_PER_UNREAL_UNIT)
    sprite.set_editor_property("pivot_mode", unreal.SpritePivotMode.BOTTOM_CENTER)
    sprite.set_editor_property(
        "custom_pivot_point", unreal.Vector2D(CELL_SIZE * 0.5, float(CELL_SIZE))
    )


def build_flipbook(clip: dict, texture: object) -> dict:
    frame_count = int(clip["frameCount"])
    fps = float(clip["fps"])
    if frame_count <= 0 or frame_count > 64:
        raise RuntimeError(f"{clip['id']}: invalid frame count {frame_count}")
    if not math.isfinite(fps) or fps <= 0.0 or fps > 240.0:
        raise RuntimeError(f"{clip['id']}: invalid playback fps {fps}")

    sprite_dir = f"{SPRITE_ROOT}/{clip['id']}"
    ensure_directory(sprite_dir)
    sprites = []
    created_sprites = 0
    for frame_index in range(frame_count):
        sprite_name = f"SPR_{clip['flipbook'][3:]}_{frame_index:02d}"
        sprite, created = create_or_load(
            sprite_name,
            sprite_dir,
            unreal.PaperSprite,
            unreal.PaperSpriteFactory(),
        )
        configure_sprite(sprite, texture, frame_index)
        save(sprite)
        sprites.append(sprite)
        created_sprites += int(created)

    flipbook, created_flipbook = create_or_load(
        str(clip["flipbook"]),
        FLIPBOOK_DIR,
        unreal.PaperFlipbook,
        unreal.PaperFlipbookFactory(),
    )
    keyframes = []
    for sprite in sprites:
        keyframe = unreal.PaperFlipbookKeyFrame()
        keyframe.set_editor_property("sprite", sprite)
        keyframe.set_editor_property("frame_run", 1)
        keyframes.append(keyframe)
    flipbook.set_editor_property("frames_per_second", fps)
    flipbook.set_editor_property("key_frames", keyframes)
    invalidate = getattr(flipbook, "invalidate_cached_data", None)
    if callable(invalidate):
        invalidate()
    save(flipbook)
    return {
        "id": clip["id"],
        "texture": texture.get_path_name(),
        "flipbook": flipbook.get_path_name(),
        "frameCount": frame_count,
        "fps": fps,
        "loopPolicy": bool(clip["loop"]),
        "createdSprites": created_sprites,
        "createdFlipbook": bool(created_flipbook),
    }


def supplemental_clip_payloads() -> list[dict]:
    clips = []
    staging_root = PROJECT_ROOT / "SourceAssets/AnimationProduction/upgrade_20260827_corrected"
    for spec in SUPPLEMENTAL_CLIPS:
        candidate_root = staging_root / spec["candidateId"]
        manifest_path = candidate_root / "candidate_atlas/manifest.json"
        if not manifest_path.is_file():
            raise FileNotFoundError(manifest_path)
        candidate = json.loads(manifest_path.read_text(encoding="utf-8"))
        clips.append(
            {
                "id": str(spec["candidateId"]),
                "flipbook": str(spec["flipbook"]),
                "paperzd": str(spec["paperzd"]),
                "loop": bool(spec["loop"]),
                "frameCount": int(candidate["frameCount"]),
                "fps": float(candidate["fps"]),
                "duration": float(candidate["durationSeconds"]),
                "atlas": str(candidate["atlases"]["2K"]["atlas"]),
            }
        )
    return clips


def main() -> None:
    if not MANIFEST_PATH.is_file():
        raise FileNotFoundError(MANIFEST_PATH)
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    if manifest.get("directionPolicy") != "source_faces_left; right_uses_horizontal_component_mirror":
        raise RuntimeError("town hero direction policy is not approved")
    clip_ids = {str(clip.get("id", "")) for clip in manifest.get("clips", [])}
    if "hero_town_walk_stop_left" not in clip_ids:
        raise RuntimeError("town hero manifest must include the approved 主角急停 clip")

    for directory in (ASSET_ROOT, ATLAS_DIR, SPRITE_ROOT, FLIPBOOK_DIR):
        ensure_directory(directory)

    approved_clips = list(manifest.get("clips", [])) + supplemental_clip_payloads()
    results = []
    for clip in approved_clips:
        atlas = PROJECT_ROOT / str(clip["atlas"])
        if not atlas.is_file():
            raise FileNotFoundError(atlas)
        texture_name = f"T_{clip['flipbook'][3:]}_Atlas"
        texture = import_texture(atlas, texture_name)
        results.append(build_flipbook(clip, texture))

    if len(results) != 10:
        raise RuntimeError(f"expected exactly ten town hero state clips, got {len(results)}")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    report = {
        "ok": True,
        "assetRoot": ASSET_ROOT,
        "clipCount": len(results),
        "clips": results,
    }
    report_path = PROJECT_ROOT / "Saved/HarnessReports/town-hero-horizontal/import-report.json"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
