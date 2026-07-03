from __future__ import annotations

import json
import struct
import zlib
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
NPC_NATIVE_CLASS = "/Script/GameXXK.GameXXKTownNpcCharacter"

SOURCE_TEXTURE_DIR = PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated"
CELL_WIDTH = 171.0
CELL_HEIGHT = 205.0
WALK_FRAME_COUNT = 6
WALK_FPS = 8.0
IDLE_FPS = 1.0

DIRECTIONS = [
    "South",
    "SouthWest",
    "West",
    "NorthWest",
    "North",
    "NorthEast",
    "East",
    "SouthEast",
]

NPC_VISUALS = [
    {
        "role": "merchant",
        "source_png": SOURCE_TEXTURE_DIR / "merchant_teal_robed_walk_8dir.png",
        "expanded_png": SOURCE_TEXTURE_DIR / "merchant_teal_robed_walk_8dir_expanded.png",
        "idle_png": SOURCE_TEXTURE_DIR / "merchant_teal_robed_idle_8dir.png",
        "blueprint_class": "/Game/GameXXK/Characters/Merchant/BP_MerchantCharacter.BP_MerchantCharacter_C",
        "sprite_dir": "/Game/GameXXK/Characters/Merchant/Sprites",
        "flipbook_dir": "/Game/GameXXK/Characters/Merchant/Flipbooks",
        "prefix": "Merchant",
    },
    {
        "role": "person",
        "source_png": SOURCE_TEXTURE_DIR / "follower_blue_scholar_walk_8dir.png",
        "expanded_png": SOURCE_TEXTURE_DIR / "follower_blue_scholar_walk_8dir_expanded.png",
        "idle_png": SOURCE_TEXTURE_DIR / "follower_blue_scholar_idle_8dir.png",
        "blueprint_class": "/Game/GameXXK/Characters/Follower/BP_NpcCharacter.BP_NpcCharacter_C",
        "sprite_dir": "/Game/GameXXK/Characters/Follower/Sprites",
        "flipbook_dir": "/Game/GameXXK/Characters/Follower/Flipbooks",
        "prefix": "Npc",
    },
]


def _load(path: str):
    return unreal.EditorAssetLibrary.load_asset(path)


def _object_identity(value) -> str:
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


def _blueprint_asset_path_from_class_path(class_path: str) -> str:
    return class_path.split(".", 1)[0]


def _get_blueprint_parent_class(asset):
    if not asset or not hasattr(unreal, "BlueprintEditorLibrary"):
        return None
    try:
        return unreal.BlueprintEditorLibrary.get_blueprint_parent_class(asset)
    except Exception:
        return None


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


def _get_prop(obj, name: str, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def _class_name(asset) -> str:
    try:
        return asset.get_class().get_name()
    except Exception:
        return ""


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


def _read_png_rgba_rows(file_path: Path) -> tuple[int, int, list[bytearray]] | None:
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
        rows.append(current)
        previous = current

    return width, height, rows


def _rgba_crop_bytes(rows: list[bytearray], x: int, y: int, width: int, height: int) -> bytes:
    stride_start = x * 4
    stride_end = (x + width) * 4
    cropped = bytearray()
    for row_index in range(y, y + height):
        cropped.extend(rows[row_index][stride_start:stride_end])
    return bytes(cropped)


def _enum_type():
    for name in ("GameXXKTownFacingDirection", "EGameXXKTownFacingDirection"):
        enum_type = getattr(unreal, name, None)
        if enum_type is not None:
            return enum_type
    return None


def _enum_candidate_names(direction: str) -> list[str]:
    snake = []
    for index, char in enumerate(direction):
        if index > 0 and char.isupper():
            snake.append("_")
        snake.append(char.upper())
    return [direction, direction.upper(), "".join(snake)]


def _direction_value(direction: str):
    enum_type = _enum_type()
    if enum_type is None:
        return None
    for candidate in _enum_candidate_names(direction):
        if hasattr(enum_type, candidate):
            return getattr(enum_type, candidate)
    try:
        return enum_type(DIRECTIONS.index(direction))
    except Exception:
        return None


def _soft_path_text(value) -> str:
    if value is None:
        return ""
    for method_name in ("export_text", "get_asset_path_string", "to_string", "get_path_name"):
        method = getattr(value, method_name, None)
        if method is None:
            continue
        try:
            text = str(method())
        except Exception:
            continue
        if text:
            return text
    for method_name in ("to_tuple", "to_dict"):
        method = getattr(value, method_name, None)
        if method is None:
            continue
        try:
            converted = method()
        except Exception:
            continue
        if isinstance(converted, tuple) and converted:
            return str(converted[0])
        if isinstance(converted, dict) and converted.get("path_string"):
            return str(converted["path_string"])
    for attr_name in ("asset_path_name", "asset_path", "sub_path_string"):
        try:
            text = str(getattr(value, attr_name))
        except Exception:
            continue
        if text and not text.startswith("<"):
            return text
    return str(value)


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


def _is_child_of(candidate, parent) -> bool:
    if candidate is None or parent is None:
        return False
    if hasattr(unreal, "MathLibrary"):
        try:
            return bool(unreal.MathLibrary.class_is_child_of(candidate, parent))
        except Exception:
            pass
    current = candidate
    while current:
        if current == parent:
            return True
        try:
            current = current.get_super_struct()
        except Exception:
            return False
    return False


def main() -> None:
    missing_files: list[str] = []
    invalid_files: list[dict[str, object]] = []
    missing_assets: list[str] = []
    invalid_assets: list[dict[str, object]] = []
    role_reports: list[dict[str, object]] = []

    npc_native_class = unreal.load_class(None, NPC_NATIVE_CLASS)
    if npc_native_class is None:
        missing_assets.append(NPC_NATIVE_CLASS)

    for spec in NPC_VISUALS:
        role = str(spec["role"])
        prefix = str(spec["prefix"])
        flipbook_dir = str(spec["flipbook_dir"])
        sprite_dir = str(spec["sprite_dir"])
        source_png = Path(spec["source_png"])
        expanded_png = Path(spec["expanded_png"])
        idle_png = Path(spec["idle_png"])
        role_report = {
            "role": role,
            "blueprint_class": spec["blueprint_class"],
            "source_png": str(source_png),
            "expanded_png": str(expanded_png),
            "idle_png": str(idle_png),
        }

        for file_path, expected_size in (
            (source_png, (1024, 1024)),
            (expanded_png, (int(CELL_WIDTH * WALK_FRAME_COUNT), int(CELL_HEIGHT * len(DIRECTIONS)))),
            (idle_png, (int(CELL_WIDTH), int(CELL_HEIGHT * len(DIRECTIONS)))),
        ):
            if not file_path.exists():
                missing_files.append(str(file_path))
                continue
            try:
                actual_size = _read_png_dimensions(file_path)
            except ValueError as exc:
                invalid_files.append({"file": str(file_path), "reason": str(exc)})
                continue
            if actual_size != expected_size:
                invalid_files.append({
                    "file": str(file_path),
                    "reason": "dimension_mismatch",
                    "expected": list(expected_size),
                    "actual": list(actual_size),
                })

        if expanded_png.exists() and idle_png.exists():
            expanded_rgba = _read_png_rgba_rows(expanded_png)
            idle_rgba = _read_png_rgba_rows(idle_png)
            if expanded_rgba is None or idle_rgba is None:
                invalid_files.append({
                    "file": f"{expanded_png};{idle_png}",
                    "reason": "rgba_decode_failed",
                })
            else:
                _walk_width, _walk_height, walk_rows = expanded_rgba
                _idle_width, _idle_height, idle_rows = idle_rgba
                for row_index, direction in enumerate(DIRECTIONS):
                    y = row_index * int(CELL_HEIGHT)
                    walk_frame0 = _rgba_crop_bytes(walk_rows, 0, y, int(CELL_WIDTH), int(CELL_HEIGHT))
                    idle_cell = _rgba_crop_bytes(idle_rows, 0, y, int(CELL_WIDTH), int(CELL_HEIGHT))
                    if walk_frame0 == idle_cell:
                        invalid_files.append({
                            "file": str(idle_png),
                            "reason": "idle_reuses_walk_frame0_pose",
                            "direction": direction,
                            "expected": "independent standing idle art",
                            "actual": "pixel-identical to walk frame 0",
                        })

        for direction in DIRECTIONS:
            for state, frame_count, fps in (
                ("Walk", WALK_FRAME_COUNT, WALK_FPS),
                ("Idle", 1, IDLE_FPS),
            ):
                flipbook_path = f"{flipbook_dir}/FB_{prefix}_{state}_{direction}"
                flipbook = _load(flipbook_path)
                if flipbook is None:
                    missing_assets.append(flipbook_path)
                    continue
                if _class_name(flipbook) != "PaperFlipbook":
                    invalid_assets.append({"path": flipbook_path, "reason": "wrong_class", "expected": "PaperFlipbook", "actual": _class_name(flipbook)})
                    continue
                if _flipbook_keyframe_count(flipbook) != frame_count:
                    invalid_assets.append({
                        "path": flipbook_path,
                        "reason": "wrong_keyframe_count",
                        "expected": frame_count,
                        "actual": _flipbook_keyframe_count(flipbook),
                    })
                actual_fps = float(_get_prop(flipbook, "frames_per_second", 0.0))
                if abs(actual_fps - fps) > 0.01:
                    invalid_assets.append({
                        "path": flipbook_path,
                        "reason": "wrong_fps",
                        "expected": fps,
                        "actual": actual_fps,
                    })
            expected_sprite_count = WALK_FRAME_COUNT + 1
            existing_sprite_count = 0
            for frame_index in range(WALK_FRAME_COUNT):
                if _load(f"{sprite_dir}/SPR_{prefix}_Walk_{direction}_{frame_index:02d}"):
                    existing_sprite_count += 1
            if _load(f"{sprite_dir}/SPR_{prefix}_Idle_{direction}_00"):
                existing_sprite_count += 1
            if existing_sprite_count != expected_sprite_count:
                invalid_assets.append({
                    "path": sprite_dir,
                    "reason": "missing_direction_sprites",
                    "direction": direction,
                    "expected": expected_sprite_count,
                    "actual": existing_sprite_count,
                })

        blueprint_class_path = str(spec["blueprint_class"])
        blueprint_asset_path = _blueprint_asset_path_from_class_path(blueprint_class_path)
        blueprint_asset = _load(blueprint_asset_path)
        if blueprint_asset is None:
            missing_assets.append(blueprint_asset_path)
        else:
            blueprint_parent_class = _get_blueprint_parent_class(blueprint_asset)
            if _object_identity(blueprint_parent_class) != NPC_NATIVE_CLASS:
                invalid_assets.append({
                    "path": blueprint_asset_path,
                    "reason": "wrong_parent_class",
                    "expected": NPC_NATIVE_CLASS,
                    "actual": _object_identity(blueprint_parent_class),
                })

        npc_class = unreal.load_class(None, blueprint_class_path)
        if npc_class is None:
            missing_assets.append(blueprint_class_path)
        else:
            npc_cdo = unreal.get_default_object(npc_class)
            expected_default = f"{flipbook_dir}/FB_{prefix}_Idle_South.FB_{prefix}_Idle_South"
            actual_default = str(_call_noarg(npc_cdo, ["get_default_town_flipbook_path_string"], ""))
            role_report["default_flipbook"] = actual_default
            if actual_default != expected_default:
                invalid_assets.append({
                    "path": str(spec["blueprint_class"]),
                    "reason": "wrong_default_town_flipbook_path",
                    "expected": expected_default,
                    "actual": actual_default,
                })
            for direction in DIRECTIONS:
                direction_value = _direction_value(direction)
                if direction_value is None:
                    invalid_assets.append({
                        "path": str(spec["blueprint_class"]),
                        "reason": "missing_python_direction_enum",
                        "direction": direction,
                    })
                    continue
                expected_walk = f"{flipbook_dir}/FB_{prefix}_Walk_{direction}.FB_{prefix}_Walk_{direction}"
                expected_idle = f"{flipbook_dir}/FB_{prefix}_Idle_{direction}.FB_{prefix}_Idle_{direction}"
                actual_walk = _soft_path_text(npc_cdo.get_town_walk_flipbook_path_for_direction(direction_value))
                actual_idle = _soft_path_text(npc_cdo.get_town_idle_flipbook_path_for_direction(direction_value))
                if actual_walk != expected_walk:
                    invalid_assets.append({
                        "path": str(spec["blueprint_class"]),
                        "reason": "wrong_walk_direction_flipbook_path",
                        "direction": direction,
                        "expected": expected_walk,
                        "actual": actual_walk,
                    })
                if actual_idle != expected_idle:
                    invalid_assets.append({
                        "path": str(spec["blueprint_class"]),
                        "reason": "wrong_idle_direction_flipbook_path",
                        "direction": direction,
                        "expected": expected_idle,
                        "actual": actual_idle,
                    })

        role_reports.append(role_report)

    report = {
        "ok": not missing_files and not invalid_files and not missing_assets and not invalid_assets,
        "npc_native_class": NPC_NATIVE_CLASS,
        "directions": DIRECTIONS,
        "roles": role_reports,
        "missing_files": missing_files,
        "invalid_files": invalid_files,
        "missing_assets": missing_assets,
        "invalid_assets": invalid_assets,
    }
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
