"""Import the isolated Hero battle-idle atlas as a 60-frame PaperFlipbook."""

from __future__ import annotations

import json
from pathlib import Path

try:
    import unreal
except ImportError:  # Allows source contract tests outside the editor.
    unreal = None


ASSET_ROOT = "/Game/GameXXK/Characters/BattleAnimationPilot/Hero/Idle"
TEXTURE_DIR = f"{ASSET_ROOT}/Textures"
SPRITE_DIR = f"{ASSET_ROOT}/Sprites"
FLIPBOOK_DIR = f"{ASSET_ROOT}/Flipbooks"
TEXTURE_NAME = "T_Pilot_Hero_Idle_Atlas"
FLIPBOOK_NAME = "FB_Pilot_Hero_Idle"
FRAME_COUNT = 60
CELL_SIZE = 512
ATLAS_SIZE = 4096
ATLAS_COLUMNS = 8
FRAMES_PER_SECOND = 12.0
# 512 / 2.5 ~= 205 UU, matching the production Hero's 171x205 battle cell.
PIXELS_PER_UNREAL_UNIT = 2.5
MANIFEST_RELATIVE_PATH = Path("SourceAssets/AnimationProcessing/Pilot/Hero/Idle/manifest.json")
ATLAS_FILE_NAME = "hero_idle_atlas.png"


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("battle animation pilot import requires UE editor Python")


def _project_root() -> Path:
    return Path(str(unreal.Paths.project_dir())).resolve()


def _load_manifest() -> tuple[dict, Path]:
    manifest_path = _project_root() / MANIFEST_RELATIVE_PATH
    payload = json.loads(manifest_path.read_text(encoding="utf-8"))
    atlas_path = Path(str(payload.get("atlas", ""))).resolve()
    if atlas_path.name != ATLAS_FILE_NAME or not atlas_path.is_file():
        raise RuntimeError(f"pilot atlas is missing or unexpected: {atlas_path}")
    expected = {
        "frameCount": FRAME_COUNT,
        "canvasSize": CELL_SIZE,
        "fps": int(FRAMES_PER_SECOND),
    }
    for key, value in expected.items():
        if int(payload.get(key, -1)) != value:
            raise RuntimeError(f"pilot manifest {key} drifted: {payload.get(key)!r} != {value}")
    return payload, atlas_path


def _ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        if not unreal.EditorAssetLibrary.make_directory(path):
            raise RuntimeError(f"could not create pilot asset directory: {path}")


def _create_or_load(name: str, directory: str, asset_class: object, factory: object):
    path = f"{directory}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if asset is None:
            raise RuntimeError(f"could not load pilot asset: {path}")
        return asset, False
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, directory, asset_class, factory)
    if asset is None:
        raise RuntimeError(f"could not create pilot asset: {path}")
    return asset, True


def _save(asset: object) -> None:
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"could not save pilot asset: {asset.get_path_name()}")


def _texture_size(texture: object) -> tuple[int, int]:
    for width_name, height_name in (("blueprint_get_size_x", "blueprint_get_size_y"), ("get_size_x", "get_size_y")):
        width = getattr(texture, width_name, None)
        height = getattr(texture, height_name, None)
        if callable(width) and callable(height):
            return int(width()), int(height())
    raise RuntimeError("could not query pilot texture dimensions")


def _import_texture(atlas_path: Path):
    asset_path = f"{TEXTURE_DIR}/{TEXTURE_NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    else:
        task = unreal.AssetImportTask()
        task.filename = str(atlas_path)
        task.destination_path = TEXTURE_DIR
        task.destination_name = TEXTURE_NAME
        task.automated = True
        task.save = False
        task.replace_existing = False
        task.replace_existing_settings = False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if texture is None or not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"could not import pilot Texture2D: {asset_path}")
    if _texture_size(texture) != (ATLAS_SIZE, ATLAS_SIZE):
        raise RuntimeError(f"pilot atlas must be {ATLAS_SIZE}x{ATLAS_SIZE}")
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
    _save(texture)
    return texture


def _configure_sprite(sprite: object, texture: object, frame_index: int) -> None:
    column = frame_index % ATLAS_COLUMNS
    row = frame_index // ATLAS_COLUMNS
    sprite.set_editor_property("source_texture", texture)
    sprite.set_editor_property("source_uv", unreal.Vector2D(float(column * CELL_SIZE), float(row * CELL_SIZE)))
    sprite.set_editor_property("source_dimension", unreal.Vector2D(float(CELL_SIZE), float(CELL_SIZE)))
    sprite.set_editor_property("source_texture_dimension", unreal.Vector2D(float(ATLAS_SIZE), float(ATLAS_SIZE)))
    sprite.set_editor_property("pixels_per_unreal_unit", PIXELS_PER_UNREAL_UNIT)
    sprite.set_editor_property("pivot_mode", unreal.SpritePivotMode.BOTTOM_CENTER)
    sprite.set_editor_property("custom_pivot_point", unreal.Vector2D(CELL_SIZE * 0.5, float(CELL_SIZE)))


def _configure_flipbook(flipbook: object, sprites: list[object]) -> None:
    keyframes = []
    for sprite in sprites:
        keyframe = unreal.PaperFlipbookKeyFrame()
        keyframe.set_editor_property("sprite", sprite)
        keyframe.set_editor_property("frame_run", 1)
        keyframes.append(keyframe)
    flipbook.set_editor_property("frames_per_second", FRAMES_PER_SECOND)
    flipbook.set_editor_property("key_frames", keyframes)
    invalidate = getattr(flipbook, "invalidate_cached_data", None)
    if callable(invalidate):
        invalidate()


def import_pilot() -> dict:
    _require_unreal()
    manifest, atlas_path = _load_manifest()
    for directory in (ASSET_ROOT, TEXTURE_DIR, SPRITE_DIR, FLIPBOOK_DIR):
        _ensure_directory(directory)
    texture = _import_texture(atlas_path)

    sprites = []
    created_sprites = 0
    for frame_index in range(FRAME_COUNT):
        sprite, created = _create_or_load(
            f"SPR_Pilot_Hero_Idle_{frame_index:02d}",
            SPRITE_DIR,
            unreal.PaperSprite,
            unreal.PaperSpriteFactory(),
        )
        _configure_sprite(sprite, texture, frame_index)
        _save(sprite)
        sprites.append(sprite)
        created_sprites += int(created)

    flipbook, created_flipbook = _create_or_load(
        FLIPBOOK_NAME,
        FLIPBOOK_DIR,
        unreal.PaperFlipbook,
        unreal.PaperFlipbookFactory(),
    )
    _configure_flipbook(flipbook, sprites)
    _save(flipbook)
    return {
        "ok": True,
        "battle_only": True,
        "asset_root": ASSET_ROOT,
        "texture": texture.get_path_name(),
        "flipbook": flipbook.get_path_name(),
        "frame_count": len(sprites),
        "fps": FRAMES_PER_SECOND,
        "created_sprite_count": created_sprites,
        "created_flipbook": bool(created_flipbook),
        "manifest": str((_project_root() / MANIFEST_RELATIVE_PATH).resolve()),
        "source_atlas": str(atlas_path),
        "source_frame_count": int(manifest["frameCount"]),
    }


def main() -> None:
    print(json.dumps(import_pilot(), ensure_ascii=False))


if __name__ == "__main__":
    main()
