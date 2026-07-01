from __future__ import annotations

import hashlib
import json
import struct
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
HERO_WALK_FILE = PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "hero_deepblue_high_ponytail_walk_8dir_expanded.png"
HERO_WALK_UASSET_FILE = PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "hero_deepblue_high_ponytail_walk_8dir_expanded.uasset"
HERO_SOURCE_TEXTURE = "/Game/GameXXK/Sprites/Generated/hero_deepblue_high_ponytail_walk_8dir"
HERO_WALK_TEXTURE = "/Game/GameXXK/Sprites/Generated/hero_deepblue_high_ponytail_walk_8dir_expanded"
HERO_SPRITE_DIR = "/Game/GameXXK/Characters/Hero/Sprites"
HERO_FLIPBOOK_DIR = "/Game/GameXXK/Characters/Hero/Flipbooks"
HERO_WALK_FLIPBOOK_PREFIX = "FB_Hero_Walk"
HERO_WALK_SPRITE_PREFIX = "SPR_Hero_Walk"
HERO_WALK_DIRECTIONS = [
    {"name": "South", "row": 0, "source_row": 5, "mirrored": False},
    {"name": "SouthWest", "row": 1, "source_row": 1, "mirrored": False},
    {"name": "West", "row": 2, "source_row": 2, "mirrored": False},
    {"name": "NorthWest", "row": 3, "source_row": 3, "mirrored": False},
    {"name": "North", "row": 4, "source_row": 4, "mirrored": False},
    {"name": "NorthEast", "row": 5, "source_row": 3, "mirrored": True},
    {"name": "East", "row": 6, "source_row": 2, "mirrored": True},
    {"name": "SouthEast", "row": 7, "source_row": 1, "mirrored": True},
]
HERO_WALK_FRAME_COUNT = 6
HERO_WALK_FPS = 8.0
HERO_WALK_CELL_WIDTH = 171.0
HERO_WALK_CELL_HEIGHT = 205.0
HERO_WALK_TEXTURE_WIDTH = int(HERO_WALK_CELL_WIDTH * HERO_WALK_FRAME_COUNT)
HERO_WALK_TEXTURE_HEIGHT = int(HERO_WALK_CELL_HEIGHT * len(HERO_WALK_DIRECTIONS))
HERO_WALK_SOUTH_FLIPBOOK = f"{HERO_FLIPBOOK_DIR}/{HERO_WALK_FLIPBOOK_PREFIX}_South"
HERO_WALK_SOUTH_OBJECT_PATH = f"{HERO_WALK_SOUTH_FLIPBOOK}.FB_Hero_Walk_South"
TOWN_PLAYER_CLASS = "/Script/GameXXK.GameXXKTownPlayerPawn"
EXPECTED_COOK_DIRECTORIES = [
    "/Game/GameXXK/Characters",
    "/Game/GameXXK/Sprites/Generated",
]


def _load(path: str):
    return unreal.EditorAssetLibrary.load_asset(path)


def _class_name(asset) -> str:
    try:
        return asset.get_class().get_name()
    except Exception:
        return ""


def _object_identity(value) -> str:
    if value is None:
        return ""
    for function_name in ("get_path_name", "get_name"):
        function = getattr(value, function_name, None)
        if function is None:
            continue
        try:
            return function()
        except Exception:
            continue
    return str(value)


def _get_prop(obj, name: str, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def _call_noarg(obj, function_names: list[str], default=None):
    for function_name in function_names:
        function = getattr(obj, function_name, None)
        if function is None:
            continue
        try:
            return function()
        except Exception:
            continue
    return default


def _read_png_dimensions(file_path: Path) -> tuple[int, int]:
    with file_path.open("rb") as file:
        header = file.read(24)
    if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG file")
    return struct.unpack(">II", header[16:24])


def _sha256(file_path: Path) -> str:
    digest = hashlib.sha256()
    with file_path.open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _texture_size(texture) -> list[int] | None:
    size_x = _call_noarg(texture, ["blueprint_get_size_x", "get_size_x"], None)
    size_y = _call_noarg(texture, ["blueprint_get_size_y", "get_size_y"], None)
    if size_x is None or size_y is None:
        return None
    try:
        return [int(size_x), int(size_y)]
    except (TypeError, ValueError):
        return None


def _enum_text(value) -> str:
    if value is None:
        return ""
    try:
        return str(value)
    except Exception:
        return ""


def _config_has_cook_directory(path: str) -> bool:
    for config_name in ("DefaultGame.ini", "DefaultEngine.ini"):
        config_path = PROJECT_ROOT / "Config" / config_name
        if not config_path.exists():
            continue
        text = config_path.read_text(encoding="utf-8", errors="ignore")
        if f'(Path="{path}")' in text:
            return True
    return False


def _vector2d_matches(value, expected_x: float, expected_y: float, tolerance: float = 0.01) -> bool:
    return (
        value is not None
        and abs(float(value.x) - expected_x) <= tolerance
        and abs(float(value.y) - expected_y) <= tolerance
    )


def _flipbook_keyframe_count(flipbook) -> int:
    get_num = getattr(flipbook, "get_num_key_frames", None)
    if get_num is not None:
        try:
            return int(get_num())
        except Exception:
            pass
    keyframes = _get_prop(flipbook, "key_frames", [])
    try:
        return len(keyframes)
    except Exception:
        return 0


def _flipbook_keyframes(flipbook) -> list:
    keyframes = _get_prop(flipbook, "key_frames", [])
    try:
        return list(keyframes)
    except Exception:
        return []


def main() -> None:
    missing_files: list[str] = []
    invalid_files: list[dict[str, object]] = []
    missing_assets: list[str] = []
    invalid_assets: list[dict[str, object]] = []
    config_errors: list[dict[str, object]] = []

    expected_walk_hash = ""
    if not HERO_WALK_FILE.exists():
        missing_files.append(str(HERO_WALK_FILE))
    else:
        expected_walk_hash = _sha256(HERO_WALK_FILE)
        try:
            width, height = _read_png_dimensions(HERO_WALK_FILE)
        except ValueError as exc:
            invalid_files.append({"file": str(HERO_WALK_FILE), "reason": str(exc)})
        else:
            if width != HERO_WALK_TEXTURE_WIDTH or height != HERO_WALK_TEXTURE_HEIGHT:
                invalid_files.append({
                    "file": str(HERO_WALK_FILE),
                    "reason": "dimension_mismatch",
                    "expected": [HERO_WALK_TEXTURE_WIDTH, HERO_WALK_TEXTURE_HEIGHT],
                    "actual": [width, height],
                })
        if not HERO_WALK_UASSET_FILE.exists():
            missing_files.append(str(HERO_WALK_UASSET_FILE))
        else:
            png_mtime = HERO_WALK_FILE.stat().st_mtime
            uasset_mtime = HERO_WALK_UASSET_FILE.stat().st_mtime
            if uasset_mtime + 1.0 < png_mtime:
                invalid_files.append({
                    "file": str(HERO_WALK_UASSET_FILE),
                    "reason": "texture_asset_older_than_png",
                    "expected": f"mtime >= {png_mtime}",
                    "actual": uasset_mtime,
                })

    source_texture = _load(HERO_SOURCE_TEXTURE)
    if source_texture is None:
        missing_assets.append(HERO_SOURCE_TEXTURE)
    elif _class_name(source_texture) != "Texture2D":
        invalid_assets.append({"path": HERO_SOURCE_TEXTURE, "reason": "wrong_class", "expected": "Texture2D", "actual": _class_name(source_texture)})

    walk_texture = _load(HERO_WALK_TEXTURE)
    if walk_texture is None:
        missing_assets.append(HERO_WALK_TEXTURE)
    elif _class_name(walk_texture) != "Texture2D":
        invalid_assets.append({"path": HERO_WALK_TEXTURE, "reason": "wrong_class", "expected": "Texture2D", "actual": _class_name(walk_texture)})
    else:
        texture_size = _texture_size(walk_texture)
        if texture_size != [HERO_WALK_TEXTURE_WIDTH, HERO_WALK_TEXTURE_HEIGHT]:
            invalid_assets.append({
                "path": HERO_WALK_TEXTURE,
                "reason": "wrong_texture_size",
                "expected": [HERO_WALK_TEXTURE_WIDTH, HERO_WALK_TEXTURE_HEIGHT],
                "actual": texture_size,
            })
        mip_text = _enum_text(_get_prop(walk_texture, "mip_gen_settings"))
        if "NO_MIPMAPS" not in mip_text.upper():
            invalid_assets.append({
                "path": HERO_WALK_TEXTURE,
                "reason": "wrong_mip_gen_settings",
                "expected": "TMGS_NO_MIPMAPS",
                "actual": mip_text,
            })
        filter_text = _enum_text(_get_prop(walk_texture, "filter"))
        if "NEAREST" not in filter_text.upper():
            invalid_assets.append({
                "path": HERO_WALK_TEXTURE,
                "reason": "wrong_texture_filter",
                "expected": "TF_NEAREST",
                "actual": filter_text,
            })

    for direction_info in HERO_WALK_DIRECTIONS:
        direction = str(direction_info["name"])
        row_index = int(direction_info["row"])
        expected_sprite_paths = [
            f"{HERO_SPRITE_DIR}/{HERO_WALK_SPRITE_PREFIX}_{direction}_{index:02d}"
            for index in range(HERO_WALK_FRAME_COUNT)
        ]
        loaded_sprites = []
        for index, sprite_path in enumerate(expected_sprite_paths):
            sprite = _load(sprite_path)
            if sprite is None:
                missing_assets.append(sprite_path)
                continue
            loaded_sprites.append(sprite)
            if _class_name(sprite) != "PaperSprite":
                invalid_assets.append({"path": sprite_path, "reason": "wrong_class", "expected": "PaperSprite", "actual": _class_name(sprite)})
                continue
            sprite_source_texture = _get_prop(sprite, "source_texture")
            if _object_identity(sprite_source_texture) != _object_identity(walk_texture):
                invalid_assets.append({
                    "path": sprite_path,
                    "reason": "wrong_source_texture",
                    "expected": HERO_WALK_TEXTURE,
                    "actual": _object_identity(sprite_source_texture),
                })
            expected_uv_x = HERO_WALK_CELL_WIDTH * index
            expected_uv_y = HERO_WALK_CELL_HEIGHT * row_index
            if not _vector2d_matches(_get_prop(sprite, "source_uv"), expected_uv_x, expected_uv_y):
                actual = _get_prop(sprite, "source_uv")
                invalid_assets.append({
                    "path": sprite_path,
                    "reason": "wrong_source_uv",
                    "expected": [expected_uv_x, expected_uv_y],
                    "actual": [float(actual.x), float(actual.y)] if actual else None,
                })
            if not _vector2d_matches(_get_prop(sprite, "source_dimension"), HERO_WALK_CELL_WIDTH, HERO_WALK_CELL_HEIGHT):
                actual = _get_prop(sprite, "source_dimension")
                invalid_assets.append({
                    "path": sprite_path,
                    "reason": "wrong_source_dimension",
                    "expected": [HERO_WALK_CELL_WIDTH, HERO_WALK_CELL_HEIGHT],
                    "actual": [float(actual.x), float(actual.y)] if actual else None,
                })
            if not _vector2d_matches(_get_prop(sprite, "source_texture_dimension"), HERO_WALK_TEXTURE_WIDTH, HERO_WALK_TEXTURE_HEIGHT):
                actual = _get_prop(sprite, "source_texture_dimension")
                invalid_assets.append({
                    "path": sprite_path,
                    "reason": "wrong_source_texture_dimension",
                    "expected": [HERO_WALK_TEXTURE_WIDTH, HERO_WALK_TEXTURE_HEIGHT],
                    "actual": [float(actual.x), float(actual.y)] if actual else None,
                })
            pivot_mode = str(_get_prop(sprite, "pivot_mode", ""))
            if "BOTTOM_CENTER" not in pivot_mode:
                invalid_assets.append({
                    "path": sprite_path,
                    "reason": "wrong_pivot",
                    "expected": "BOTTOM_CENTER",
                    "actual": pivot_mode,
                })

        flipbook_path = f"{HERO_FLIPBOOK_DIR}/{HERO_WALK_FLIPBOOK_PREFIX}_{direction}"
        flipbook = _load(flipbook_path)
        if flipbook is None:
            missing_assets.append(flipbook_path)
            continue
        if _class_name(flipbook) != "PaperFlipbook":
            invalid_assets.append({"path": flipbook_path, "reason": "wrong_class", "expected": "PaperFlipbook", "actual": _class_name(flipbook)})
            continue
        keyframe_count = _flipbook_keyframe_count(flipbook)
        if keyframe_count != HERO_WALK_FRAME_COUNT:
            invalid_assets.append({
                "path": flipbook_path,
                "reason": "wrong_keyframe_count",
                "expected": HERO_WALK_FRAME_COUNT,
                "actual": keyframe_count,
            })
        fps = _get_prop(flipbook, "frames_per_second", 0.0)
        if abs(float(fps) - HERO_WALK_FPS) > 0.01:
            invalid_assets.append({
                "path": flipbook_path,
                "reason": "wrong_fps",
                "expected": HERO_WALK_FPS,
                "actual": fps,
            })
        if len(loaded_sprites) == HERO_WALK_FRAME_COUNT:
            actual_keyframe_sprites = [
                _object_identity(_get_prop(keyframe, "sprite"))
                for keyframe in _flipbook_keyframes(flipbook)
            ]
            expected_keyframe_sprites = [_object_identity(sprite) for sprite in loaded_sprites]
            if actual_keyframe_sprites != expected_keyframe_sprites:
                invalid_assets.append({
                    "path": flipbook_path,
                    "reason": "wrong_keyframe_sprites",
                    "expected": expected_keyframe_sprites,
                    "actual": actual_keyframe_sprites,
                })

    player_class = unreal.load_class(None, TOWN_PLAYER_CLASS)
    if player_class is None:
        missing_assets.append(TOWN_PLAYER_CLASS)
    else:
        player_cdo = unreal.get_default_object(player_class)
        default_path = _call_noarg(player_cdo, ["get_default_town_flipbook_path_string"], None)
        default_path_text = str(default_path)
        if default_path_text != HERO_WALK_SOUTH_OBJECT_PATH:
            invalid_assets.append({
                "path": TOWN_PLAYER_CLASS,
                "reason": "wrong_default_town_flipbook_path",
                "expected": HERO_WALK_SOUTH_OBJECT_PATH,
                "actual": default_path_text,
            })
        cdo_flipbook = _call_noarg(
            player_cdo,
            ["get_current_town_flipbook", "get_default_town_flipbook"],
            _get_prop(player_cdo, "default_town_flipbook"),
        )
        south_flipbook = _load(HERO_WALK_SOUTH_FLIPBOOK)
        if _object_identity(cdo_flipbook) != _object_identity(south_flipbook):
            invalid_assets.append({
                "path": TOWN_PLAYER_CLASS,
                "reason": "wrong_default_town_flipbook",
                "expected": _object_identity(south_flipbook),
                "actual": _object_identity(cdo_flipbook),
            })

    for cook_directory in EXPECTED_COOK_DIRECTORIES:
        if not _config_has_cook_directory(cook_directory):
            config_errors.append({
                "path": cook_directory,
                "reason": "missing_cook_directory",
                "expected": "configured under [/Script/UnrealEd.ProjectPackagingSettings]",
            })

    report = {
        "ok": not missing_files and not invalid_files and not missing_assets and not invalid_assets and not config_errors,
        "hero_walk_file": str(HERO_WALK_FILE),
        "hero_walk_file_sha256": expected_walk_hash,
        "hero_source_texture": HERO_SOURCE_TEXTURE,
        "hero_walk_texture": HERO_WALK_TEXTURE,
        "hero_walk_south_flipbook": HERO_WALK_SOUTH_FLIPBOOK,
        "town_player_class": TOWN_PLAYER_CLASS,
        "expected_direction_count": len(HERO_WALK_DIRECTIONS),
        "expected_sprite_count": len(HERO_WALK_DIRECTIONS) * HERO_WALK_FRAME_COUNT,
        "expected_flipbook_count": len(HERO_WALK_DIRECTIONS),
        "mirror_strategy": "deterministic_expanded_sheet",
        "walk_grid": {
            "columns": HERO_WALK_FRAME_COUNT,
            "rows": len(HERO_WALK_DIRECTIONS),
            "cell_size": [HERO_WALK_CELL_WIDTH, HERO_WALK_CELL_HEIGHT],
            "texture_size": [HERO_WALK_TEXTURE_WIDTH, HERO_WALK_TEXTURE_HEIGHT],
            "directions": HERO_WALK_DIRECTIONS,
        },
        "expected_cook_directories": EXPECTED_COOK_DIRECTORIES,
        "missing_files": missing_files,
        "invalid_files": invalid_files,
        "missing_assets": missing_assets,
        "invalid_assets": invalid_assets,
        "config_errors": config_errors,
    }
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
