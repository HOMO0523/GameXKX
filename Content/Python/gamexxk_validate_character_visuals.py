from __future__ import annotations

import hashlib
import json
import struct
import zlib
from pathlib import Path
from statistics import median

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
HERO_WALK_FILE = PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "hero_deepblue_high_ponytail_walk_8dir_expanded.png"
HERO_IDLE_FILE = PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "hero_deepblue_high_ponytail_idle_8dir.png"
HERO_WALK_UASSET_FILE = PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "hero_deepblue_high_ponytail_walk_8dir_expanded.uasset"
HERO_IDLE_UASSET_FILE = PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "hero_deepblue_high_ponytail_idle_8dir.uasset"
HERO_SOURCE_TEXTURE = "/Game/GameXXK/Sprites/Generated/hero_deepblue_high_ponytail_walk_8dir"
HERO_WALK_TEXTURE = "/Game/GameXXK/Sprites/Generated/hero_deepblue_high_ponytail_walk_8dir_expanded"
HERO_IDLE_TEXTURE = "/Game/GameXXK/Sprites/Generated/hero_deepblue_high_ponytail_idle_8dir"
HERO_SPRITE_DIR = "/Game/GameXXK/Characters/Hero/Sprites"
HERO_FLIPBOOK_DIR = "/Game/GameXXK/Characters/Hero/Flipbooks"
HERO_WALK_FLIPBOOK_PREFIX = "FB_Hero_Walk"
HERO_IDLE_FLIPBOOK_PREFIX = "FB_Hero_Idle"
HERO_WALK_SPRITE_PREFIX = "SPR_Hero_Walk"
HERO_IDLE_SPRITE_PREFIX = "SPR_Hero_Idle"
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
HERO_IDLE_FPS = 1.0
HERO_WALK_CELL_WIDTH = 171.0
HERO_WALK_CELL_HEIGHT = 205.0
HERO_WALK_TEXTURE_WIDTH = int(HERO_WALK_CELL_WIDTH * HERO_WALK_FRAME_COUNT)
HERO_WALK_TEXTURE_HEIGHT = int(HERO_WALK_CELL_HEIGHT * len(HERO_WALK_DIRECTIONS))
HERO_IDLE_TEXTURE_WIDTH = int(HERO_WALK_CELL_WIDTH)
HERO_IDLE_TEXTURE_HEIGHT = int(HERO_WALK_CELL_HEIGHT * len(HERO_WALK_DIRECTIONS))
HERO_IDLE_ALIGNMENT_BOTTOM_TOLERANCE = 2.0
HERO_IDLE_ALIGNMENT_CENTER_TOLERANCE = 3.0
HERO_WALK_SOUTH_FLIPBOOK = f"{HERO_FLIPBOOK_DIR}/{HERO_WALK_FLIPBOOK_PREFIX}_South"
HERO_WALK_SOUTH_OBJECT_PATH = f"{HERO_WALK_SOUTH_FLIPBOOK}.FB_Hero_Walk_South"
HERO_IDLE_SOUTH_FLIPBOOK = f"{HERO_FLIPBOOK_DIR}/{HERO_IDLE_FLIPBOOK_PREFIX}_South"
HERO_IDLE_SOUTH_OBJECT_PATH = f"{HERO_IDLE_SOUTH_FLIPBOOK}.FB_Hero_Idle_South"
HERO_CHARACTER_CLASS = "/Game/GameXXK/Characters/Hero/BP_HeroCharacter.BP_HeroCharacter_C"
HERO_NATIVE_CLASS = "/Script/GameXXK.GameXXKHeroCharacter"
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


def _paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def _read_png_rgba_alpha_rows(file_path: Path) -> tuple[int, int, list[bytearray]] | None:
    data = file_path.read_bytes()
    if len(data) < 33 or data[:8] != b"\x89PNG\r\n\x1a\n":
        return None

    offset = 8
    width = 0
    height = 0
    bit_depth = 0
    color_type = 0
    compressed = bytearray()
    while offset + 8 <= len(data):
        length = struct.unpack(">I", data[offset : offset + 4])[0]
        chunk_type = data[offset + 4 : offset + 8]
        payload = data[offset + 8 : offset + 8 + length]
        offset += 12 + length
        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type = struct.unpack(">IIBB", payload[:10])
        elif chunk_type == b"IDAT":
            compressed.extend(payload)
        elif chunk_type == b"IEND":
            break

    if width <= 0 or height <= 0 or bit_depth != 8 or color_type != 6:
        return None

    raw = zlib.decompress(bytes(compressed))
    bytes_per_pixel = 4
    stride = width * bytes_per_pixel
    previous = bytearray(stride)
    rows: list[bytearray] = []
    raw_offset = 0
    for _row in range(height):
        filter_type = raw[raw_offset]
        raw_offset += 1
        current = bytearray(raw[raw_offset : raw_offset + stride])
        raw_offset += stride
        for index in range(stride):
            left = current[index - bytes_per_pixel] if index >= bytes_per_pixel else 0
            up = previous[index]
            upper_left = previous[index - bytes_per_pixel] if index >= bytes_per_pixel else 0
            if filter_type == 1:
                current[index] = (current[index] + left) & 0xFF
            elif filter_type == 2:
                current[index] = (current[index] + up) & 0xFF
            elif filter_type == 3:
                current[index] = (current[index] + ((left + up) // 2)) & 0xFF
            elif filter_type == 4:
                current[index] = (current[index] + _paeth(left, up, upper_left)) & 0xFF
        rows.append(bytearray(current[3:stride:bytes_per_pixel]))
        previous = current

    return width, height, rows


def _read_png_rgba_alpha_stats(file_path: Path) -> dict[str, int | bool]:
    image = _read_png_rgba_alpha_rows(file_path)
    if image is None:
        return {"supports_alpha": False, "transparent_pixels": 0, "translucent_pixels": 0}

    _width, _height, alpha_rows = image
    transparent = 0
    translucent = 0
    for row in alpha_rows:
        for alpha in row:
            if alpha == 0:
                transparent += 1
            elif alpha < 255:
                translucent += 1

    return {
        "supports_alpha": True,
        "transparent_pixels": transparent,
        "translucent_pixels": translucent,
    }


def _read_png_alpha_cell_metrics(
    file_path: Path,
    cell_width: int,
    cell_height: int,
    columns: int,
    rows: int,
) -> list[list[dict[str, float | int | bool]]]:
    image = _read_png_rgba_alpha_rows(file_path)
    if image is None:
        raise ValueError("not an 8-bit RGBA PNG")

    width, height, alpha_rows = image
    expected_size = (cell_width * columns, cell_height * rows)
    if (width, height) != expected_size:
        raise ValueError(f"unexpected alpha grid size {(width, height)}; expected {expected_size}")

    cell_center_x = (cell_width - 1) / 2.0
    metrics: list[list[dict[str, float | int | bool]]] = []
    for row_index in range(rows):
        row_metrics: list[dict[str, float | int | bool]] = []
        for column_index in range(columns):
            min_x = cell_width
            min_y = cell_height
            max_x = -1
            max_y = -1
            base_x = column_index * cell_width
            base_y = row_index * cell_height
            for y in range(cell_height):
                alpha_row = alpha_rows[base_y + y]
                for x in range(cell_width):
                    if alpha_row[base_x + x] <= 0:
                        continue
                    min_x = min(min_x, x)
                    min_y = min(min_y, y)
                    max_x = max(max_x, x)
                    max_y = max(max_y, y)
            if max_x < 0 or max_y < 0:
                row_metrics.append({"has_alpha": False})
                continue
            row_metrics.append({
                "has_alpha": True,
                "left": min_x,
                "top": min_y,
                "right": max_x + 1,
                "bottom": max_y + 1,
                "width": max_x - min_x + 1,
                "height": max_y - min_y + 1,
                "bottom_gap": cell_height - (max_y + 1),
                "center_offset": ((min_x + max_x) / 2.0) - cell_center_x,
            })
        metrics.append(row_metrics)
    return metrics


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
    idle_alignment_metrics: dict[str, dict[str, float | int]] = {}

    expected_walk_hash = ""
    expected_idle_hash = ""
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
            alpha_stats = _read_png_rgba_alpha_stats(HERO_WALK_FILE)
            if not alpha_stats["supports_alpha"] or alpha_stats["transparent_pixels"] <= 0:
                invalid_files.append({
                    "file": str(HERO_WALK_FILE),
                    "reason": "missing_transparent_background",
                    "expected": "RGBA PNG with transparent background pixels",
                    "actual": alpha_stats,
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

    if not HERO_IDLE_FILE.exists():
        missing_files.append(str(HERO_IDLE_FILE))
    else:
        expected_idle_hash = _sha256(HERO_IDLE_FILE)
        try:
            width, height = _read_png_dimensions(HERO_IDLE_FILE)
        except ValueError as exc:
            invalid_files.append({"file": str(HERO_IDLE_FILE), "reason": str(exc)})
        else:
            if width != HERO_IDLE_TEXTURE_WIDTH or height != HERO_IDLE_TEXTURE_HEIGHT:
                invalid_files.append({
                    "file": str(HERO_IDLE_FILE),
                    "reason": "idle_dimension_mismatch",
                    "expected": [HERO_IDLE_TEXTURE_WIDTH, HERO_IDLE_TEXTURE_HEIGHT],
                    "actual": [width, height],
                })
            alpha_stats = _read_png_rgba_alpha_stats(HERO_IDLE_FILE)
            if not alpha_stats["supports_alpha"] or alpha_stats["transparent_pixels"] <= 0:
                invalid_files.append({
                    "file": str(HERO_IDLE_FILE),
                    "reason": "idle_missing_transparent_background",
                    "expected": "RGBA PNG with transparent background pixels",
                    "actual": alpha_stats,
                })
        if not HERO_IDLE_UASSET_FILE.exists():
            missing_files.append(str(HERO_IDLE_UASSET_FILE))
        else:
            png_mtime = HERO_IDLE_FILE.stat().st_mtime
            uasset_mtime = HERO_IDLE_UASSET_FILE.stat().st_mtime
            if uasset_mtime + 1.0 < png_mtime:
                invalid_files.append({
                    "file": str(HERO_IDLE_UASSET_FILE),
                    "reason": "idle_texture_asset_older_than_png",
                    "expected": f"mtime >= {png_mtime}",
                    "actual": uasset_mtime,
                })

    if HERO_WALK_FILE.exists() and HERO_IDLE_FILE.exists():
        try:
            walk_alpha_cells = _read_png_alpha_cell_metrics(
                HERO_WALK_FILE,
                int(HERO_WALK_CELL_WIDTH),
                int(HERO_WALK_CELL_HEIGHT),
                HERO_WALK_FRAME_COUNT,
                len(HERO_WALK_DIRECTIONS),
            )
            idle_alpha_cells = _read_png_alpha_cell_metrics(
                HERO_IDLE_FILE,
                int(HERO_WALK_CELL_WIDTH),
                int(HERO_WALK_CELL_HEIGHT),
                1,
                len(HERO_WALK_DIRECTIONS),
            )
        except ValueError as exc:
            invalid_files.append({
                "file": f"{HERO_WALK_FILE};{HERO_IDLE_FILE}",
                "reason": "idle_walk_alpha_metric_error",
                "actual": str(exc),
            })
        else:
            for direction_index, direction_entry in enumerate(HERO_WALK_DIRECTIONS):
                direction_name = str(direction_entry["name"])
                walk_cells = walk_alpha_cells[direction_index]
                idle_cell = idle_alpha_cells[direction_index][0]
                if not idle_cell.get("has_alpha"):
                    invalid_files.append({
                        "file": str(HERO_IDLE_FILE),
                        "reason": "idle_cell_empty",
                        "direction": direction_name,
                    })
                    continue
                walk_bottom_gaps = [float(cell["bottom_gap"]) for cell in walk_cells if cell.get("has_alpha")]
                walk_center_offsets = [float(cell["center_offset"]) for cell in walk_cells if cell.get("has_alpha")]
                if len(walk_bottom_gaps) != HERO_WALK_FRAME_COUNT or len(walk_center_offsets) != HERO_WALK_FRAME_COUNT:
                    invalid_files.append({
                        "file": str(HERO_WALK_FILE),
                        "reason": "walk_cell_empty",
                        "direction": direction_name,
                    })
                    continue

                target_bottom_gap = float(median(walk_bottom_gaps))
                target_center_offset = float(median(walk_center_offsets))
                idle_bottom_gap = float(idle_cell["bottom_gap"])
                idle_center_offset = float(idle_cell["center_offset"])
                idle_alignment_metrics[direction_name] = {
                    "target_bottom_gap": target_bottom_gap,
                    "actual_bottom_gap": idle_bottom_gap,
                    "target_center_offset": target_center_offset,
                    "actual_center_offset": idle_center_offset,
                    "bottom_delta": idle_bottom_gap - target_bottom_gap,
                    "center_delta": idle_center_offset - target_center_offset,
                }
                if abs(idle_bottom_gap - target_bottom_gap) > HERO_IDLE_ALIGNMENT_BOTTOM_TOLERANCE:
                    invalid_files.append({
                        "file": str(HERO_IDLE_FILE),
                        "reason": "idle_walk_baseline_mismatch",
                        "direction": direction_name,
                        "expected": target_bottom_gap,
                        "actual": idle_bottom_gap,
                        "tolerance": HERO_IDLE_ALIGNMENT_BOTTOM_TOLERANCE,
                    })
                if abs(idle_center_offset - target_center_offset) > HERO_IDLE_ALIGNMENT_CENTER_TOLERANCE:
                    invalid_files.append({
                        "file": str(HERO_IDLE_FILE),
                        "reason": "idle_walk_center_mismatch",
                        "direction": direction_name,
                        "expected": target_center_offset,
                        "actual": idle_center_offset,
                        "tolerance": HERO_IDLE_ALIGNMENT_CENTER_TOLERANCE,
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

    idle_texture = _load(HERO_IDLE_TEXTURE)
    if idle_texture is None:
        missing_assets.append(HERO_IDLE_TEXTURE)
    elif _class_name(idle_texture) != "Texture2D":
        invalid_assets.append({"path": HERO_IDLE_TEXTURE, "reason": "wrong_class", "expected": "Texture2D", "actual": _class_name(idle_texture)})
    else:
        texture_size = _texture_size(idle_texture)
        if texture_size != [HERO_IDLE_TEXTURE_WIDTH, HERO_IDLE_TEXTURE_HEIGHT]:
            invalid_assets.append({
                "path": HERO_IDLE_TEXTURE,
                "reason": "wrong_idle_texture_size",
                "expected": [HERO_IDLE_TEXTURE_WIDTH, HERO_IDLE_TEXTURE_HEIGHT],
                "actual": texture_size,
            })
        mip_text = _enum_text(_get_prop(idle_texture, "mip_gen_settings"))
        if "NO_MIPMAPS" not in mip_text.upper():
            invalid_assets.append({
                "path": HERO_IDLE_TEXTURE,
                "reason": "wrong_idle_mip_gen_settings",
                "expected": "TMGS_NO_MIPMAPS",
                "actual": mip_text,
            })
        filter_text = _enum_text(_get_prop(idle_texture, "filter"))
        if "NEAREST" not in filter_text.upper():
            invalid_assets.append({
                "path": HERO_IDLE_TEXTURE,
                "reason": "wrong_idle_texture_filter",
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

        idle_sprite_path = f"{HERO_SPRITE_DIR}/{HERO_IDLE_SPRITE_PREFIX}_{direction}_00"
        idle_sprite = _load(idle_sprite_path)
        if idle_sprite is None:
            missing_assets.append(idle_sprite_path)
        elif _class_name(idle_sprite) != "PaperSprite":
            invalid_assets.append({"path": idle_sprite_path, "reason": "wrong_class", "expected": "PaperSprite", "actual": _class_name(idle_sprite)})
        else:
            idle_source_texture = _get_prop(idle_sprite, "source_texture")
            if _object_identity(idle_source_texture) != _object_identity(idle_texture):
                invalid_assets.append({
                    "path": idle_sprite_path,
                    "reason": "wrong_idle_source_texture",
                    "expected": HERO_IDLE_TEXTURE,
                    "actual": _object_identity(idle_source_texture),
                })
            expected_idle_uv_y = HERO_WALK_CELL_HEIGHT * row_index
            if not _vector2d_matches(_get_prop(idle_sprite, "source_uv"), 0.0, expected_idle_uv_y):
                actual = _get_prop(idle_sprite, "source_uv")
                invalid_assets.append({
                    "path": idle_sprite_path,
                    "reason": "wrong_idle_source_uv",
                    "expected": [0.0, expected_idle_uv_y],
                    "actual": [float(actual.x), float(actual.y)] if actual else None,
                })
            if not _vector2d_matches(_get_prop(idle_sprite, "source_dimension"), HERO_WALK_CELL_WIDTH, HERO_WALK_CELL_HEIGHT):
                actual = _get_prop(idle_sprite, "source_dimension")
                invalid_assets.append({
                    "path": idle_sprite_path,
                    "reason": "wrong_idle_source_dimension",
                    "expected": [HERO_WALK_CELL_WIDTH, HERO_WALK_CELL_HEIGHT],
                    "actual": [float(actual.x), float(actual.y)] if actual else None,
                })
            if not _vector2d_matches(_get_prop(idle_sprite, "source_texture_dimension"), HERO_IDLE_TEXTURE_WIDTH, HERO_IDLE_TEXTURE_HEIGHT):
                actual = _get_prop(idle_sprite, "source_texture_dimension")
                invalid_assets.append({
                    "path": idle_sprite_path,
                    "reason": "wrong_idle_source_texture_dimension",
                    "expected": [HERO_IDLE_TEXTURE_WIDTH, HERO_IDLE_TEXTURE_HEIGHT],
                    "actual": [float(actual.x), float(actual.y)] if actual else None,
                })

        idle_flipbook_path = f"{HERO_FLIPBOOK_DIR}/{HERO_IDLE_FLIPBOOK_PREFIX}_{direction}"
        idle_flipbook = _load(idle_flipbook_path)
        if idle_flipbook is None:
            missing_assets.append(idle_flipbook_path)
            continue
        if _class_name(idle_flipbook) != "PaperFlipbook":
            invalid_assets.append({"path": idle_flipbook_path, "reason": "wrong_class", "expected": "PaperFlipbook", "actual": _class_name(idle_flipbook)})
            continue
        idle_keyframe_count = _flipbook_keyframe_count(idle_flipbook)
        if idle_keyframe_count != 1:
            invalid_assets.append({
                "path": idle_flipbook_path,
                "reason": "wrong_idle_keyframe_count",
                "expected": 1,
                "actual": idle_keyframe_count,
            })
        idle_fps = _get_prop(idle_flipbook, "frames_per_second", 0.0)
        if abs(float(idle_fps) - HERO_IDLE_FPS) > 0.01:
            invalid_assets.append({
                "path": idle_flipbook_path,
                "reason": "wrong_idle_fps",
                "expected": HERO_IDLE_FPS,
                "actual": idle_fps,
            })
        if idle_sprite:
            idle_keyframe_sprites = [
                _object_identity(_get_prop(keyframe, "sprite"))
                for keyframe in _flipbook_keyframes(idle_flipbook)
            ]
            expected_idle_sprites = [_object_identity(idle_sprite)]
            if idle_keyframe_sprites != expected_idle_sprites:
                invalid_assets.append({
                    "path": idle_flipbook_path,
                    "reason": "wrong_idle_keyframe_sprite",
                    "expected": expected_idle_sprites,
                    "actual": idle_keyframe_sprites,
                })

    hero_class = unreal.load_class(None, HERO_CHARACTER_CLASS)
    hero_native_class = unreal.load_class(None, HERO_NATIVE_CLASS)
    if hero_class is None:
        missing_assets.append(HERO_CHARACTER_CLASS)
    else:
        if hero_native_class is None:
            missing_assets.append(HERO_NATIVE_CLASS)
        elif not unreal.MathLibrary.class_is_child_of(hero_class, hero_native_class):
            invalid_assets.append({
                "path": HERO_CHARACTER_CLASS,
                "reason": "wrong_parent_class",
                "expected": HERO_NATIVE_CLASS,
                "actual": _object_identity(_call_noarg(hero_class, ["get_super_struct"], None)),
            })

        hero_cdo = unreal.get_default_object(hero_class)
        default_path = _call_noarg(hero_cdo, ["get_default_town_flipbook_path_string"], None)
        default_path_text = str(default_path)
        if default_path_text != HERO_IDLE_SOUTH_OBJECT_PATH:
            invalid_assets.append({
                "path": HERO_CHARACTER_CLASS,
                "reason": "wrong_default_town_flipbook_path",
                "expected": HERO_IDLE_SOUTH_OBJECT_PATH,
                "actual": default_path_text,
            })
        cdo_flipbook = _call_noarg(
            hero_cdo,
            ["get_default_town_flipbook"],
            _get_prop(hero_cdo, "default_town_flipbook"),
        )
        south_flipbook = _load(HERO_IDLE_SOUTH_FLIPBOOK)
        if _object_identity(cdo_flipbook) != _object_identity(south_flipbook):
            invalid_assets.append({
                "path": HERO_CHARACTER_CLASS,
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
        "hero_idle_file": str(HERO_IDLE_FILE),
        "hero_walk_file_sha256": expected_walk_hash,
        "hero_idle_file_sha256": expected_idle_hash,
        "hero_source_texture": HERO_SOURCE_TEXTURE,
        "hero_walk_texture": HERO_WALK_TEXTURE,
        "hero_idle_texture": HERO_IDLE_TEXTURE,
        "hero_walk_south_flipbook": HERO_WALK_SOUTH_FLIPBOOK,
        "hero_idle_south_flipbook": HERO_IDLE_SOUTH_FLIPBOOK,
        "hero_character_class": HERO_CHARACTER_CLASS,
        "hero_native_class": HERO_NATIVE_CLASS,
        "expected_direction_count": len(HERO_WALK_DIRECTIONS),
        "expected_sprite_count": len(HERO_WALK_DIRECTIONS) * HERO_WALK_FRAME_COUNT,
        "expected_walk_flipbook_count": len(HERO_WALK_DIRECTIONS),
        "expected_idle_flipbook_count": len(HERO_WALK_DIRECTIONS),
        "mirror_strategy": "deterministic_expanded_sheet",
        "idle_alignment_metrics": idle_alignment_metrics,
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
