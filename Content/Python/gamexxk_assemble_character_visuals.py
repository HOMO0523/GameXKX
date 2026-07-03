from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
EXPANDED_SHEET_FILE = PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "hero_deepblue_high_ponytail_walk_8dir_expanded.png"
IDLE_SHEET_FILE = PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "hero_deepblue_high_ponytail_idle_8dir.png"
TEXTURE_DIR = "/Game/GameXXK/Sprites/Generated"
EXPANDED_TEXTURE = f"{TEXTURE_DIR}/hero_deepblue_high_ponytail_walk_8dir_expanded"
IDLE_TEXTURE = f"{TEXTURE_DIR}/hero_deepblue_high_ponytail_idle_8dir"
HERO_SPRITE_DIR = "/Game/GameXXK/Characters/Hero/Sprites"
HERO_FLIPBOOK_DIR = "/Game/GameXXK/Characters/Hero/Flipbooks"

CELL_WIDTH = 171.0
CELL_HEIGHT = 205.0
TEXTURE_WIDTH = int(CELL_WIDTH * 6)
TEXTURE_HEIGHT = int(CELL_HEIGHT * 8)
IDLE_TEXTURE_WIDTH = int(CELL_WIDTH)
IDLE_TEXTURE_HEIGHT = int(CELL_HEIGHT * 8)
WALK_FPS = 8.0
IDLE_FPS = 1.0
PIXELS_PER_UNREAL_UNIT = 1.0

DIRECTIONS = [
    {"name": "South", "row": 0},
    {"name": "SouthWest", "row": 1},
    {"name": "West", "row": 2},
    {"name": "NorthWest", "row": 3},
    {"name": "North", "row": 4},
    {"name": "NorthEast", "row": 5},
    {"name": "East", "row": 6},
    {"name": "SouthEast", "row": 7},
]


def _ensure_directory(path: str) -> bool:
    if unreal.EditorAssetLibrary.does_directory_exist(path):
        return False
    unreal.EditorAssetLibrary.make_directory(path)
    return True


def _load_asset(path: str):
    return unreal.EditorAssetLibrary.load_asset(path)


def _create_or_load_asset(asset_name: str, package_path: str, asset_class, factory):
    asset_path = f"{package_path}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        asset = _load_asset(asset_path)
        if asset is None:
            raise RuntimeError(f"Could not load existing asset {asset_path}")
        if asset.get_class().get_name() != asset_class.static_class().get_name():
            raise RuntimeError(f"Existing asset has wrong class: {asset_path}")
        return asset, False
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(asset_name, package_path, asset_class, factory)
    if asset is None:
        raise RuntimeError(f"Could not create asset {asset_path}")
    return asset, True


def _configure_texture(texture) -> None:
    texture_settings = {
        "mip_gen_settings": ("TextureMipGenSettings", "TMGS_NO_MIPMAPS"),
        "filter": ("TextureFilter", "TF_NEAREST"),
    }
    for prop, enum_info in texture_settings.items():
        enum_name, value_name = enum_info
        enum_type = getattr(unreal, enum_name, None)
        if enum_type is None or not hasattr(enum_type, value_name):
            raise RuntimeError(f"Missing Unreal enum value {enum_name}.{value_name}")
        texture.set_editor_property(prop, getattr(enum_type, value_name))


def _import_texture(source_file: Path, destination_name: str, asset_path: str) -> object:
    if not source_file.exists():
        raise RuntimeError(f"Missing hero texture source: {source_file}")
    if not unreal.EditorAssetLibrary.does_directory_exist(TEXTURE_DIR):
        unreal.EditorAssetLibrary.make_directory(TEXTURE_DIR)
    task = unreal.AssetImportTask()
    task.filename = str(source_file)
    task.destination_path = TEXTURE_DIR
    task.destination_name = destination_name
    task.automated = True
    task.save = False
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = _load_asset(asset_path)
    if texture is None:
        raise RuntimeError(f"Could not load imported texture {asset_path}")
    _configure_texture(texture)
    return texture


def _import_expanded_texture() -> object:
    return _import_texture(EXPANDED_SHEET_FILE, "hero_deepblue_high_ponytail_walk_8dir_expanded", EXPANDED_TEXTURE)


def _import_idle_texture() -> object:
    return _import_texture(IDLE_SHEET_FILE, "hero_deepblue_high_ponytail_idle_8dir", IDLE_TEXTURE)


def _set_prop(obj, prop_name: str, value) -> None:
    try:
        obj.set_editor_property(prop_name, value)
    except Exception as exc:
        raise RuntimeError(f"Could not set {prop_name} on {obj}: {exc}") from exc


def _configure_sprite(sprite, texture, direction_row: int, frame_index: int) -> None:
    _set_prop(sprite, "source_texture", texture)
    _set_prop(sprite, "source_uv", unreal.Vector2D(CELL_WIDTH * frame_index, CELL_HEIGHT * direction_row))
    _set_prop(sprite, "source_dimension", unreal.Vector2D(CELL_WIDTH, CELL_HEIGHT))
    _set_prop(sprite, "source_texture_dimension", unreal.Vector2D(TEXTURE_WIDTH, TEXTURE_HEIGHT))
    _set_prop(sprite, "pixels_per_unreal_unit", PIXELS_PER_UNREAL_UNIT)
    _set_prop(sprite, "pivot_mode", unreal.SpritePivotMode.BOTTOM_CENTER)
    _set_prop(sprite, "custom_pivot_point", unreal.Vector2D(CELL_WIDTH * 0.5, CELL_HEIGHT))


def _create_sprites(texture) -> dict[str, list[object]]:
    result: dict[str, list[object]] = {}
    for direction in DIRECTIONS:
        direction_name = str(direction["name"])
        direction_row = int(direction["row"])
        result[direction_name] = []
        for frame_index in range(6):
            sprite_name = f"SPR_Hero_Walk_{direction_name}_{frame_index:02d}"
            sprite, _created = _create_or_load_asset(sprite_name, HERO_SPRITE_DIR, unreal.PaperSprite, unreal.PaperSpriteFactory())
            _configure_sprite(sprite, texture, direction_row, frame_index)
            result[direction_name].append(sprite)
    return result


def _create_idle_sprites(texture) -> dict[str, object]:
    result: dict[str, object] = {}
    for direction in DIRECTIONS:
        direction_name = str(direction["name"])
        direction_row = int(direction["row"])
        sprite_name = f"SPR_Hero_Idle_{direction_name}_00"
        sprite, _created = _create_or_load_asset(sprite_name, HERO_SPRITE_DIR, unreal.PaperSprite, unreal.PaperSpriteFactory())
        _set_prop(sprite, "source_texture", texture)
        _set_prop(sprite, "source_uv", unreal.Vector2D(0.0, CELL_HEIGHT * direction_row))
        _set_prop(sprite, "source_dimension", unreal.Vector2D(CELL_WIDTH, CELL_HEIGHT))
        _set_prop(sprite, "source_texture_dimension", unreal.Vector2D(IDLE_TEXTURE_WIDTH, IDLE_TEXTURE_HEIGHT))
        _set_prop(sprite, "pixels_per_unreal_unit", PIXELS_PER_UNREAL_UNIT)
        _set_prop(sprite, "pivot_mode", unreal.SpritePivotMode.BOTTOM_CENTER)
        _set_prop(sprite, "custom_pivot_point", unreal.Vector2D(CELL_WIDTH * 0.5, CELL_HEIGHT))
        result[direction_name] = sprite
    return result


def _set_flipbook_keyframes(flipbook, sprites: list[object], frames_per_second: float) -> None:
    keyframes = []
    for sprite in sprites:
        keyframe = unreal.PaperFlipbookKeyFrame()
        keyframe.set_editor_property("sprite", sprite)
        keyframe.set_editor_property("frame_run", 1)
        keyframes.append(keyframe)
    flipbook.set_editor_property("frames_per_second", frames_per_second)
    flipbook.set_editor_property("key_frames", keyframes)
    invalidate = getattr(flipbook, "invalidate_cached_data", None)
    if invalidate:
        invalidate()


def _create_walk_flipbooks(sprites_by_direction: dict[str, list[object]]) -> dict[str, object]:
    flipbooks = {}
    for direction_name, sprites in sprites_by_direction.items():
        flipbook_name = f"FB_Hero_Walk_{direction_name}"
        flipbook, _created = _create_or_load_asset(flipbook_name, HERO_FLIPBOOK_DIR, unreal.PaperFlipbook, unreal.PaperFlipbookFactory())
        _set_flipbook_keyframes(flipbook, sprites, WALK_FPS)
        flipbooks[direction_name] = flipbook
    return flipbooks


def _create_idle_flipbooks(sprites_by_direction: dict[str, object]) -> dict[str, object]:
    flipbooks = {}
    for direction_name, sprite in sprites_by_direction.items():
        flipbook_name = f"FB_Hero_Idle_{direction_name}"
        flipbook, _created = _create_or_load_asset(flipbook_name, HERO_FLIPBOOK_DIR, unreal.PaperFlipbook, unreal.PaperFlipbookFactory())
        _set_flipbook_keyframes(flipbook, [sprite], IDLE_FPS)
        flipbooks[direction_name] = flipbook
    return flipbooks


def main() -> None:
    created_directories = [
        path
        for path in (HERO_SPRITE_DIR, HERO_FLIPBOOK_DIR)
        if _ensure_directory(path)
    ]
    texture = _import_expanded_texture()
    idle_texture = _import_idle_texture()
    sprites = _create_sprites(texture)
    idle_sprites = _create_idle_sprites(idle_texture)
    walk_flipbooks = _create_walk_flipbooks(sprites)
    idle_flipbooks = _create_idle_flipbooks(idle_sprites)

    unreal.EditorAssetLibrary.save_directory(TEXTURE_DIR)
    unreal.EditorAssetLibrary.save_directory(HERO_SPRITE_DIR)
    unreal.EditorAssetLibrary.save_directory(HERO_FLIPBOOK_DIR)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)

    report = {
        "ok": True,
        "expanded_texture": EXPANDED_TEXTURE,
        "idle_texture": IDLE_TEXTURE,
        "created_directories": created_directories,
        "sprite_count": sum(len(direction_sprites) for direction_sprites in sprites.values()),
        "idle_sprite_count": len(idle_sprites),
        "walk_flipbook_count": len(walk_flipbooks),
        "idle_flipbook_count": len(idle_flipbooks),
        "directions": [direction["name"] for direction in DIRECTIONS],
    }
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
