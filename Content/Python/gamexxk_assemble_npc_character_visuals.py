from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
TEXTURE_DIR = "/Game/GameXXK/Sprites/Generated"
HERO_BLUEPRINT_ASSET = "/Game/GameXXK/Characters/Hero/BP_HeroCharacter"
NPC_NATIVE_CLASS_PATH = "/Script/GameXXK.GameXXKTownNpcCharacter"

CELL_WIDTH = 171.0
CELL_HEIGHT = 205.0
WALK_FRAME_COUNT = 6
WALK_TEXTURE_WIDTH = int(CELL_WIDTH * WALK_FRAME_COUNT)
WALK_TEXTURE_HEIGHT = int(CELL_HEIGHT * 8)
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

NPC_VISUALS = [
    {
        "role": "merchant",
        "prefix": "Merchant",
        "walk_source": PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "merchant_teal_robed_walk_8dir_expanded.png",
        "idle_source": PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "merchant_teal_robed_idle_8dir.png",
        "walk_texture_name": "merchant_teal_robed_walk_8dir_expanded",
        "idle_texture_name": "merchant_teal_robed_idle_8dir",
        "sprite_dir": "/Game/GameXXK/Characters/Merchant/Sprites",
        "flipbook_dir": "/Game/GameXXK/Characters/Merchant/Flipbooks",
        "blueprint_asset": "/Game/GameXXK/Characters/Merchant/BP_MerchantCharacter",
    },
    {
        "role": "person",
        "prefix": "Npc",
        "walk_source": PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "follower_blue_scholar_walk_8dir_expanded.png",
        "idle_source": PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "follower_blue_scholar_idle_8dir.png",
        "walk_texture_name": "follower_blue_scholar_walk_8dir_expanded",
        "idle_texture_name": "follower_blue_scholar_idle_8dir",
        "sprite_dir": "/Game/GameXXK/Characters/Follower/Sprites",
        "flipbook_dir": "/Game/GameXXK/Characters/Follower/Flipbooks",
        "blueprint_asset": "/Game/GameXXK/Characters/Follower/BP_NpcCharacter",
    },
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


def _set_prop(obj, prop_name: str, value) -> None:
    try:
        obj.set_editor_property(prop_name, value)
    except Exception as exc:
        raise RuntimeError(f"Could not set {prop_name} on {obj}: {exc}") from exc


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
        raise RuntimeError(f"Missing NPC texture source: {source_file}")
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


def _configure_walk_sprite(sprite, texture, direction_row: int, frame_index: int) -> None:
    _set_prop(sprite, "source_texture", texture)
    _set_prop(sprite, "source_uv", unreal.Vector2D(CELL_WIDTH * frame_index, CELL_HEIGHT * direction_row))
    _set_prop(sprite, "source_dimension", unreal.Vector2D(CELL_WIDTH, CELL_HEIGHT))
    _set_prop(sprite, "source_texture_dimension", unreal.Vector2D(WALK_TEXTURE_WIDTH, WALK_TEXTURE_HEIGHT))
    _set_prop(sprite, "pixels_per_unreal_unit", PIXELS_PER_UNREAL_UNIT)
    _set_prop(sprite, "pivot_mode", unreal.SpritePivotMode.BOTTOM_CENTER)
    _set_prop(sprite, "custom_pivot_point", unreal.Vector2D(CELL_WIDTH * 0.5, CELL_HEIGHT))


def _configure_idle_sprite(sprite, texture, direction_row: int) -> None:
    _set_prop(sprite, "source_texture", texture)
    _set_prop(sprite, "source_uv", unreal.Vector2D(0.0, CELL_HEIGHT * direction_row))
    _set_prop(sprite, "source_dimension", unreal.Vector2D(CELL_WIDTH, CELL_HEIGHT))
    _set_prop(sprite, "source_texture_dimension", unreal.Vector2D(IDLE_TEXTURE_WIDTH, IDLE_TEXTURE_HEIGHT))
    _set_prop(sprite, "pixels_per_unreal_unit", PIXELS_PER_UNREAL_UNIT)
    _set_prop(sprite, "pivot_mode", unreal.SpritePivotMode.BOTTOM_CENTER)
    _set_prop(sprite, "custom_pivot_point", unreal.Vector2D(CELL_WIDTH * 0.5, CELL_HEIGHT))


def _create_walk_sprites(prefix: str, sprite_dir: str, texture) -> dict[str, list[object]]:
    result: dict[str, list[object]] = {}
    for direction in DIRECTIONS:
        direction_name = str(direction["name"])
        direction_row = int(direction["row"])
        result[direction_name] = []
        for frame_index in range(WALK_FRAME_COUNT):
            sprite_name = f"SPR_{prefix}_Walk_{direction_name}_{frame_index:02d}"
            sprite, _created = _create_or_load_asset(sprite_name, sprite_dir, unreal.PaperSprite, unreal.PaperSpriteFactory())
            _configure_walk_sprite(sprite, texture, direction_row, frame_index)
            result[direction_name].append(sprite)
    return result


def _create_idle_sprites(prefix: str, sprite_dir: str, texture) -> dict[str, object]:
    result: dict[str, object] = {}
    for direction in DIRECTIONS:
        direction_name = str(direction["name"])
        direction_row = int(direction["row"])
        sprite_name = f"SPR_{prefix}_Idle_{direction_name}_00"
        sprite, _created = _create_or_load_asset(sprite_name, sprite_dir, unreal.PaperSprite, unreal.PaperSpriteFactory())
        _configure_idle_sprite(sprite, texture, direction_row)
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


def _create_walk_flipbooks(prefix: str, flipbook_dir: str, sprites_by_direction: dict[str, list[object]]) -> dict[str, object]:
    flipbooks = {}
    for direction_name, sprites in sprites_by_direction.items():
        flipbook_name = f"FB_{prefix}_Walk_{direction_name}"
        flipbook, _created = _create_or_load_asset(flipbook_name, flipbook_dir, unreal.PaperFlipbook, unreal.PaperFlipbookFactory())
        _set_flipbook_keyframes(flipbook, sprites, WALK_FPS)
        flipbooks[direction_name] = flipbook
    return flipbooks


def _create_idle_flipbooks(prefix: str, flipbook_dir: str, sprites_by_direction: dict[str, object]) -> dict[str, object]:
    flipbooks = {}
    for direction_name, sprite in sprites_by_direction.items():
        flipbook_name = f"FB_{prefix}_Idle_{direction_name}"
        flipbook, _created = _create_or_load_asset(flipbook_name, flipbook_dir, unreal.PaperFlipbook, unreal.PaperFlipbookFactory())
        _set_flipbook_keyframes(flipbook, [sprite], IDLE_FPS)
        flipbooks[direction_name] = flipbook
    return flipbooks


def _compile_blueprint(asset) -> None:
    if hasattr(unreal, "BlueprintEditorLibrary"):
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)
        return
    if hasattr(unreal, "KismetEditorUtilities"):
        unreal.KismetEditorUtilities.compile_blueprint(asset)
        return
    raise RuntimeError("No Blueprint compile API is available")


def _load_npc_parent_class():
    parent_class = unreal.load_class(None, NPC_NATIVE_CLASS_PATH)
    if parent_class is not None:
        return parent_class

    parent_type = getattr(unreal, "GameXXKTownNpcCharacter", None)
    if parent_type is None:
        raise RuntimeError("GameXXKTownNpcCharacter is not loaded; rebuild GameXXKEditor first")
    return parent_type.static_class()


def _is_child_of(candidate, parent) -> bool:
    if candidate is None or parent is None:
        return False
    try:
        return bool(candidate.is_child_of(parent))
    except Exception:
        pass
    current = candidate
    while current is not None:
        if current == parent:
            return True
        getter = getattr(current, "get_super_struct", None)
        current = getter() if getter else None
    return False


def _class_identity(value) -> str:
    if value is None:
        return ""
    for function_name in ("get_path_name", "get_name"):
        function = getattr(value, function_name, None)
        if function is None:
            continue
        try:
            return str(function())
        except Exception:
            continue
    return str(value)


def _get_blueprint_parent_class(asset):
    if not hasattr(unreal, "BlueprintEditorLibrary"):
        raise RuntimeError("BlueprintEditorLibrary is required to inspect NPC character blueprint parents")
    return unreal.BlueprintEditorLibrary.get_blueprint_parent_class(asset)


def _blueprint_parent_matches(asset, parent_class) -> bool:
    return _class_identity(_get_blueprint_parent_class(asset)) == _class_identity(parent_class)


def _reparent_blueprint(asset, parent_class) -> None:
    if not hasattr(unreal, "BlueprintEditorLibrary"):
        raise RuntimeError("BlueprintEditorLibrary is required to reparent NPC character blueprints")
    unreal.BlueprintEditorLibrary.reparent_blueprint(asset, parent_class)


def _load_generated_class(blueprint_asset_path: str):
    blueprint_name = blueprint_asset_path.rsplit("/", 1)[-1]
    return unreal.load_object(None, f"{blueprint_asset_path}.{blueprint_name}_C")


def _get_generated_class(asset, blueprint_asset_path: str):
    try:
        generated_class = asset.get_editor_property("generated_class")
        if generated_class is not None:
            return generated_class
    except Exception:
        pass
    return _load_generated_class(blueprint_asset_path)


def _ensure_blueprint_copy(blueprint_asset_path: str):
    package_path, asset_name = blueprint_asset_path.rsplit("/", 1)
    _ensure_directory(package_path)
    if unreal.EditorAssetLibrary.does_asset_exist(blueprint_asset_path):
        asset = _load_asset(blueprint_asset_path)
        if asset is None:
            raise RuntimeError(f"Could not load existing blueprint {blueprint_asset_path}")
        return asset, False
    asset = unreal.EditorAssetLibrary.duplicate_asset(HERO_BLUEPRINT_ASSET, blueprint_asset_path)
    if asset is None:
        raise RuntimeError(f"Could not duplicate {HERO_BLUEPRINT_ASSET} to {blueprint_asset_path}")
    return asset, True


def _enum_type():
    for name in ("GameXXKTownFacingDirection", "EGameXXKTownFacingDirection"):
        enum_type = getattr(unreal, name, None)
        if enum_type is not None:
            return enum_type
    raise RuntimeError("GameXXKTownFacingDirection enum is not available to Python")


def _enum_candidate_names(direction: str) -> list[str]:
    snake = []
    for index, char in enumerate(direction):
        if index > 0 and char.isupper():
            snake.append("_")
        snake.append(char.upper())
    return [direction, direction.upper(), "".join(snake)]


def _direction_value(direction: str):
    enum_type = _enum_type()
    for candidate in _enum_candidate_names(direction):
        if hasattr(enum_type, candidate):
            return getattr(enum_type, candidate)
    try:
        return enum_type([item["name"] for item in DIRECTIONS].index(direction))
    except Exception as exc:
        raise RuntimeError(f"Could not resolve direction enum for {direction}") from exc


def _asset_object_path(asset) -> str:
    return asset.get_path_name()


def _soft_object_path(asset):
    return unreal.SoftObjectPath(_asset_object_path(asset))


def _set_soft_object_property(cdo, prop_name: str, asset) -> None:
    failures = []
    for value in (asset, _soft_object_path(asset), _asset_object_path(asset)):
        try:
            cdo.set_editor_property(prop_name, value)
            return
        except Exception as exc:
            failures.append(str(exc))
    raise RuntimeError(f"Could not set {prop_name}: {failures}")


def _set_soft_object_map_property(cdo, prop_name: str, flipbooks_by_direction: dict[str, object]) -> None:
    enum_map = {
        _direction_value(direction): flipbook
        for direction, flipbook in flipbooks_by_direction.items()
    }
    soft_map = {
        _direction_value(direction): _soft_object_path(flipbook)
        for direction, flipbook in flipbooks_by_direction.items()
    }
    string_map = {
        _direction_value(direction): _asset_object_path(flipbook)
        for direction, flipbook in flipbooks_by_direction.items()
    }
    failures = []
    for value in (enum_map, soft_map, string_map):
        try:
            cdo.set_editor_property(prop_name, value)
            return
        except Exception as exc:
            failures.append(str(exc))
    raise RuntimeError(f"Could not set {prop_name}: {failures}")


def _disable_auto_possess(cdo) -> None:
    auto_receive = getattr(unreal, "AutoReceiveInput", None)
    if auto_receive is not None:
        for candidate in ("DISABLED", "Disabled"):
            if hasattr(auto_receive, candidate):
                try:
                    cdo.set_editor_property("auto_possess_player", getattr(auto_receive, candidate))
                    break
                except Exception:
                    pass
    auto_ai = getattr(unreal, "AutoPossessAI", None)
    if auto_ai is not None:
        for candidate in ("DISABLED", "Disabled"):
            if hasattr(auto_ai, candidate):
                try:
                    cdo.set_editor_property("auto_possess_ai", getattr(auto_ai, candidate))
                    break
                except Exception:
                    pass


def _configure_blueprint_defaults(blueprint_asset_path: str, walk_flipbooks: dict[str, object], idle_flipbooks: dict[str, object]) -> dict[str, object]:
    asset, created = _ensure_blueprint_copy(blueprint_asset_path)
    parent_class = _load_npc_parent_class()
    if not _blueprint_parent_matches(asset, parent_class):
        _reparent_blueprint(asset, parent_class)
        _compile_blueprint(asset)
        if not _blueprint_parent_matches(asset, parent_class):
            raise RuntimeError(
                f"{blueprint_asset_path} was not reparented to GameXXKTownNpcCharacter; "
                f"parent={_class_identity(_get_blueprint_parent_class(asset))}"
            )
    _compile_blueprint(asset)
    generated_class = _get_generated_class(asset, blueprint_asset_path)
    if generated_class is None:
        raise RuntimeError(f"Could not load generated class for {blueprint_asset_path}")
    cdo = unreal.get_default_object(generated_class)
    _set_soft_object_property(cdo, "default_town_flipbook_asset", idle_flipbooks["South"])
    _set_soft_object_map_property(cdo, "town_direction_flipbook_assets", walk_flipbooks)
    _set_soft_object_map_property(cdo, "town_idle_direction_flipbook_assets", idle_flipbooks)
    _disable_auto_possess(cdo)
    cdo.reset_town_movement_input()
    _compile_blueprint(asset)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(asset)
    return {
        "asset": blueprint_asset_path,
        "generated_class": generated_class.get_path_name(),
        "parent_class": _class_identity(_get_blueprint_parent_class(asset)),
        "created": created,
        "saved": bool(saved),
        "default_flipbook": cdo.get_default_town_flipbook_path_string(),
    }


def _assemble_visual(spec: dict[str, object]) -> dict[str, object]:
    prefix = str(spec["prefix"])
    sprite_dir = str(spec["sprite_dir"])
    flipbook_dir = str(spec["flipbook_dir"])
    blueprint_asset = str(spec["blueprint_asset"])
    created_directories = [
        path
        for path in (sprite_dir, flipbook_dir)
        if _ensure_directory(path)
    ]
    walk_texture_path = f"{TEXTURE_DIR}/{spec['walk_texture_name']}"
    idle_texture_path = f"{TEXTURE_DIR}/{spec['idle_texture_name']}"
    walk_texture = _import_texture(Path(spec["walk_source"]), str(spec["walk_texture_name"]), walk_texture_path)
    idle_texture = _import_texture(Path(spec["idle_source"]), str(spec["idle_texture_name"]), idle_texture_path)
    walk_sprites = _create_walk_sprites(prefix, sprite_dir, walk_texture)
    idle_sprites = _create_idle_sprites(prefix, sprite_dir, idle_texture)
    walk_flipbooks = _create_walk_flipbooks(prefix, flipbook_dir, walk_sprites)
    idle_flipbooks = _create_idle_flipbooks(prefix, flipbook_dir, idle_sprites)
    blueprint = _configure_blueprint_defaults(blueprint_asset, walk_flipbooks, idle_flipbooks)
    unreal.EditorAssetLibrary.save_directory(sprite_dir)
    unreal.EditorAssetLibrary.save_directory(flipbook_dir)
    return {
        "role": spec["role"],
        "created_directories": created_directories,
        "walk_texture": walk_texture_path,
        "idle_texture": idle_texture_path,
        "sprite_count": sum(len(items) for items in walk_sprites.values()),
        "idle_sprite_count": len(idle_sprites),
        "walk_flipbook_count": len(walk_flipbooks),
        "idle_flipbook_count": len(idle_flipbooks),
        "blueprint": blueprint,
    }


def main() -> None:
    role_reports = [_assemble_visual(spec) for spec in NPC_VISUALS]
    unreal.EditorAssetLibrary.save_directory(TEXTURE_DIR)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    report = {
        "ok": all(bool(role["blueprint"].get("saved")) for role in role_reports),
        "roles": role_reports,
        "directions": [direction["name"] for direction in DIRECTIONS],
    }
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
