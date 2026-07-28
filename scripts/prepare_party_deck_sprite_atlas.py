#!/usr/bin/env python3
"""Pack a 4x2 green-screen PartyDeck direction sheet into isolated UE-ready PNG atlases.

The source grid is read row-major as S, SW, W, NW, N, NE, E, SE.  This tool
never mirrors a direction and only writes beneath SourceAssets/PartyDeck/
character-references/packed.  It is deliberately local-only: no UE API,
asset import, image generation, or mutation of existing character assets.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import sys
from collections import deque
from pathlib import Path
from typing import Any

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PACKED_ROOT = PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "character-references" / "packed"

CELL_WIDTH = 171
CELL_HEIGHT = 205
GRID_COLUMNS = 4
GRID_ROWS = 2
WALK_FRAME_COUNT = 6
DIRECTIONS = ["S", "SW", "W", "NW", "N", "NE", "E", "SE"]
ENGINE_DIRECTIONS = ["South", "SouthWest", "West", "NorthWest", "North", "NorthEast", "East", "SouthEast"]

# No mirror transform is used.  These intentionally small pixel offsets create
# a deterministic loop from an approved single-pose source without changing an
# identity, costume, or facing.
IDLE_OFFSET = (0, -3)
WALK_OFFSETS = [(-2, 0), (-1, 2), (1, 3), (2, 1), (1, -1), (-1, 1)]
HORIZONTAL_MARGIN = 8
VERTICAL_MARGIN = 8


class SpritePackingError(RuntimeError):
    """A source sheet or destination does not satisfy the preparation contract."""


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _safe_output_dir(raw_path: Path) -> Path:
    output_dir = raw_path if raw_path.is_absolute() else PROJECT_ROOT / raw_path
    output_dir = output_dir.resolve()
    packed_root = PACKED_ROOT.resolve()
    if not output_dir.is_relative_to(packed_root):
        raise SpritePackingError(f"output directory must remain under {packed_root}: {output_dir}")
    return output_dir


def _grid_edges(length: int, count: int) -> list[int]:
    if length < count:
        raise SpritePackingError(f"source axis is too small for {count} grid cells: {length}")
    return [round(index * length / count) for index in range(count + 1)]


def _green_score(pixel: tuple[int, int, int, int]) -> int:
    red, green, blue, _alpha = pixel
    return int(green) - max(int(red), int(blue))


def _is_green_seed(pixel: tuple[int, int, int, int]) -> bool:
    red, green, blue, alpha = pixel
    # Screen green is deliberately much more saturated than a jade/green
    # costume: this permits clearing enclosed background pockets without
    # treating every isolated green foreground component as background.
    return alpha > 0 and green >= 180 and red <= 80 and blue <= 80 and green - max(red, blue) >= 150


def _is_connected_green(pixel: tuple[int, int, int, int]) -> bool:
    red, green, blue, alpha = pixel
    return alpha > 0 and green >= 70 and green - max(red, blue) >= 20


def _remove_connected_green_background(source: Image.Image) -> Image.Image:
    """Remove chroma-green components touching an edge or containing bright screen green."""
    image = source.convert("RGBA")
    width, height = image.size
    pixels = image.load()
    visited = bytearray(width * height)
    transparent = bytearray(width * height)
    for start_index in range(width * height):
        if visited[start_index]:
            continue
        start_x = start_index % width
        start_y = start_index // width
        if not _is_connected_green(pixels[start_x, start_y]):
            visited[start_index] = 1
            continue
        queue: deque[tuple[int, int]] = deque([(start_x, start_y)])
        visited[start_index] = 1
        component: list[int] = []
        touches_edge = False
        contains_screen_green = False
        while queue:
            x, y = queue.popleft()
            index = y * width + x
            component.append(index)
            touches_edge = touches_edge or x == 0 or y == 0 or x == width - 1 or y == height - 1
            contains_screen_green = contains_screen_green or _is_green_seed(pixels[x, y])
            for neighbor_x, neighbor_y in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                if neighbor_x < 0 or neighbor_y < 0 or neighbor_x >= width or neighbor_y >= height:
                    continue
                neighbor_index = neighbor_y * width + neighbor_x
                if visited[neighbor_index] or not _is_connected_green(pixels[neighbor_x, neighbor_y]):
                    continue
                visited[neighbor_index] = 1
                queue.append((neighbor_x, neighbor_y))
        if touches_edge or contains_screen_green:
            for index in component:
                transparent[index] = 1

    for index, was_background in enumerate(transparent):
        if not was_background:
            continue
        x = index % width
        y = index // width
        red, green, blue, _alpha = pixels[x, y]
        pixels[x, y] = (red, green, blue, 0)
    return image


def _extract_direction_bodies(source: Image.Image) -> list[Image.Image]:
    source = source.convert("RGBA")
    x_edges = _grid_edges(source.width, GRID_COLUMNS)
    y_edges = _grid_edges(source.height, GRID_ROWS)
    bodies: list[Image.Image] = []
    for row in range(GRID_ROWS):
        for column in range(GRID_COLUMNS):
            cleaned = _remove_connected_green_background(source.crop((
                x_edges[column], y_edges[row], x_edges[column + 1], y_edges[row + 1]
            )))
            bbox = cleaned.getchannel("A").getbbox()
            if bbox is None:
                direction = DIRECTIONS[len(bodies)]
                raise SpritePackingError(f"green-screen crop has no visible subject for {direction}")
            bodies.append(cleaned.crop(bbox))
    if len(bodies) != len(DIRECTIONS):
        raise SpritePackingError(f"expected {len(DIRECTIONS)} direction bodies, got {len(bodies)}")
    return bodies


def _common_scale(bodies: list[Image.Image]) -> float:
    max_width = max(body.width for body in bodies)
    max_height = max(body.height for body in bodies)
    horizontal_motion = max(abs(offset[0]) for offset in [IDLE_OFFSET, *WALK_OFFSETS])
    vertical_min = min(offset[1] for offset in [IDLE_OFFSET, *WALK_OFFSETS])
    vertical_max = max(offset[1] for offset in [IDLE_OFFSET, *WALK_OFFSETS])
    width_budget = CELL_WIDTH - HORIZONTAL_MARGIN * 2 - horizontal_motion * 2
    height_budget = CELL_HEIGHT - VERTICAL_MARGIN * 2 - (vertical_max - vertical_min)
    if width_budget <= 0 or height_budget <= 0:
        raise SpritePackingError("invalid atlas margin and movement budget")
    scale = min(width_budget / max_width, height_budget / max_height)
    if scale <= 0:
        raise SpritePackingError("computed non-positive body scale")
    return scale


def _scaled_bodies(bodies: list[Image.Image]) -> list[Image.Image]:
    scale = _common_scale(bodies)
    scaled: list[Image.Image] = []
    for body in bodies:
        target_size = (max(1, round(body.width * scale)), max(1, round(body.height * scale)))
        scaled.append(body.resize(target_size, Image.Resampling.NEAREST))
    return scaled


def _make_cell(body: Image.Image, offset_x: int, offset_y: int) -> Image.Image:
    canvas = Image.new("RGBA", (CELL_WIDTH, CELL_HEIGHT), (0, 0, 0, 0))
    x = (CELL_WIDTH - body.width) // 2 + offset_x
    y = CELL_HEIGHT - VERTICAL_MARGIN - body.height + offset_y
    if x < 0 or y < 0 or x + body.width > CELL_WIDTH or y + body.height > CELL_HEIGHT:
        raise SpritePackingError("normalized body would clip outside a 171x205 atlas cell")
    canvas.alpha_composite(body, (x, y))
    return canvas


def build_atlases(source_path: Path) -> tuple[Image.Image, Image.Image, dict[str, Any]]:
    if not source_path.is_file():
        raise SpritePackingError(f"input image is missing: {source_path}")
    with Image.open(source_path) as source_image:
        if source_image.width < GRID_COLUMNS or source_image.height < GRID_ROWS:
            raise SpritePackingError(f"input must be a usable 4x2 grid, got {source_image.size}")
        bodies = _scaled_bodies(_extract_direction_bodies(source_image))

    idle = Image.new("RGBA", (CELL_WIDTH, CELL_HEIGHT * len(DIRECTIONS)), (0, 0, 0, 0))
    walk = Image.new("RGBA", (CELL_WIDTH * WALK_FRAME_COUNT, CELL_HEIGHT * len(DIRECTIONS)), (0, 0, 0, 0))
    for row, body in enumerate(bodies):
        idle.alpha_composite(_make_cell(body, *IDLE_OFFSET), (0, row * CELL_HEIGHT))
        for frame, offset in enumerate(WALK_OFFSETS):
            walk.alpha_composite(_make_cell(body, *offset), (frame * CELL_WIDTH, row * CELL_HEIGHT))
    metadata = {
        "directions": DIRECTIONS,
        "engine_directions": ENGINE_DIRECTIONS,
        "source_grid": [
            {"direction": DIRECTIONS[index], "row": index // GRID_COLUMNS, "column": index % GRID_COLUMNS}
            for index in range(len(DIRECTIONS))
        ],
        "idle_offset": list(IDLE_OFFSET),
        "walk_offsets": [list(offset) for offset in WALK_OFFSETS],
        "mirroring": "never_used",
        "cell_pixels": [CELL_WIDTH, CELL_HEIGHT],
    }
    return idle, walk, metadata


def _cell_metrics(cell: Image.Image) -> dict[str, int | bool]:
    alpha = cell.getchannel("A")
    alpha_bytes = alpha.tobytes()
    return {
        "visible_pixels": sum(value > 0 for value in alpha_bytes),
        "transparent_pixels": alpha_bytes.count(0),
        "has_bbox": alpha.getbbox() is not None,
    }


def _validate_images(idle: Image.Image, walk: Image.Image) -> dict[str, Any]:
    errors: list[str] = []
    if idle.mode != "RGBA" or idle.size != (CELL_WIDTH, CELL_HEIGHT * len(DIRECTIONS)):
        errors.append(f"invalid idle atlas: mode={idle.mode} size={idle.size}")
    if walk.mode != "RGBA" or walk.size != (CELL_WIDTH * WALK_FRAME_COUNT, CELL_HEIGHT * len(DIRECTIONS)):
        errors.append(f"invalid walk atlas: mode={walk.mode} size={walk.size}")
    rows: list[dict[str, Any]] = []
    if not errors:
        for row, direction in enumerate(DIRECTIONS):
            idle_cell = idle.crop((0, row * CELL_HEIGHT, CELL_WIDTH, (row + 1) * CELL_HEIGHT))
            idle_metrics = _cell_metrics(idle_cell)
            if not idle_metrics["has_bbox"] or not idle_metrics["visible_pixels"] or not idle_metrics["transparent_pixels"]:
                errors.append(f"idle row {direction} has invalid alpha coverage")
            frames = [
                walk.crop((frame * CELL_WIDTH, row * CELL_HEIGHT, (frame + 1) * CELL_WIDTH, (row + 1) * CELL_HEIGHT))
                for frame in range(WALK_FRAME_COUNT)
            ]
            frame_metrics = [_cell_metrics(frame) for frame in frames]
            for frame, metrics in enumerate(frame_metrics):
                if not metrics["has_bbox"] or not metrics["visible_pixels"] or not metrics["transparent_pixels"]:
                    errors.append(f"walk row {direction} frame {frame} has invalid alpha coverage")
            frame_bytes = [frame.tobytes() for frame in frames]
            if len(set(frame_bytes)) != WALK_FRAME_COUNT:
                errors.append(f"walk row {direction} repeats a frame")
            if idle_cell.tobytes() in frame_bytes:
                errors.append(f"idle row {direction} repeats a walk frame")
            rows.append({
                "direction": direction,
                "idle": idle_metrics,
                "walk_frames": frame_metrics,
            })
    return {"ok": not errors, "directions": DIRECTIONS, "rows": rows, "errors": errors}


def validate_packed_atlases(idle_path: Path, walk_path: Path) -> dict[str, Any]:
    if not idle_path.is_file() or not walk_path.is_file():
        return {"ok": False, "errors": ["idle or walk atlas is missing"]}
    with Image.open(idle_path) as idle_source, Image.open(walk_path) as walk_source:
        return _validate_images(idle_source.convert("RGBA"), walk_source.convert("RGBA"))


def _png_bytes(image: Image.Image) -> bytes:
    buffer = io.BytesIO()
    image.save(buffer, format="PNG", compress_level=9)
    return buffer.getvalue()


def _write_new_or_identical(path: Path, data: bytes, replace_existing: bool) -> str:
    if path.exists():
        existing = path.read_bytes()
        if _sha256(existing) == _sha256(data):
            return "unchanged"
        if not replace_existing:
            raise FileExistsError(f"refusing to overwrite a non-identical packed atlas: {path}")
        path.write_bytes(data)
        return "replaced"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return "created"


def pack(source_path: Path, output_dir: Path, prefix: str, replace_existing: bool = False) -> dict[str, Any]:
    if not prefix or any(character not in "abcdefghijklmnopqrstuvwxyz0123456789_" for character in prefix):
        raise SpritePackingError("prefix must use lowercase ASCII letters, digits, and underscores")
    output_dir = _safe_output_dir(output_dir)
    idle, walk, metadata = build_atlases(source_path)
    validation = _validate_images(idle, walk)
    if not validation["ok"]:
        raise SpritePackingError("in-memory atlas validation failed: " + "; ".join(validation["errors"]))
    idle_path = output_dir / f"{prefix}_idle_8dir.png"
    walk_path = output_dir / f"{prefix}_walk_8dir.png"
    idle_action = _write_new_or_identical(idle_path, _png_bytes(idle), replace_existing)
    walk_action = _write_new_or_identical(walk_path, _png_bytes(walk), replace_existing)
    disk_validation = validate_packed_atlases(idle_path, walk_path)
    if not disk_validation["ok"]:
        raise SpritePackingError("written atlas validation failed: " + "; ".join(disk_validation["errors"]))
    return {
        "ok": True,
        "input": str(source_path),
        "outputs": {
            "idle": str(idle_path.relative_to(PROJECT_ROOT)).replace("\\", "/"),
            "walk": str(walk_path.relative_to(PROJECT_ROOT)).replace("\\", "/"),
            "idle_action": idle_action,
            "walk_action": walk_action,
        },
        "directions": DIRECTIONS,
        "metadata": metadata,
        "validation": disk_validation,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path, help="4x2 green-screen PNG/JPEG direction sheet")
    parser.add_argument("--output-dir", type=Path, default=PACKED_ROOT)
    parser.add_argument("--prefix", required=True, help="stable lowercase output prefix")
    parser.add_argument("--replace-existing", action="store_true", help="Explicitly replace only a changed atlas beneath the owned packed root.")
    parser.add_argument("--json", action="store_true", help="Kept for scripts; output is always JSON.")
    args = parser.parse_args()
    try:
        report = pack(args.input, args.output_dir, args.prefix, args.replace_existing)
        exit_code = 0
    except Exception as exc:
        report = {"ok": False, "error": str(exc)}
        exit_code = 1
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
