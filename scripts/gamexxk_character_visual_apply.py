#!/usr/bin/env python3
"""Build and import MVP hero Paper2D walk visuals through UE MCP."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from statistics import median
from typing import Any

from gamexxk_content_assembly_check import parse_stdout_json
from ue_mcp_client import DEFAULT_HOST, DEFAULT_PATH, DEFAULT_PORT, UnrealMCPClient


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_SHEET = PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "hero_deepblue_high_ponytail_walk_8dir.png"
EXPANDED_SHEET = PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "hero_deepblue_high_ponytail_walk_8dir_expanded.png"
IDLE_SOURCE_DIR = PROJECT_ROOT / "Saved" / "AssetAnalysis" / "HeroDeepBluePonytail" / "imagegen_idle_canonical5" / "idle_8dir_scaled_to_walk_height"
IDLE_SHEET = PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "hero_deepblue_high_ponytail_idle_8dir.png"
ASSEMBLER = "Content/Python/gamexxk_assemble_character_visuals.py"
VALIDATOR = "Content/Python/gamexxk_validate_character_visuals.py"

ORIGINAL_X_EDGES = [0, 171, 341, 512, 683, 853, 1024]
ORIGINAL_Y_EDGES = [0, 205, 410, 614, 819, 1024]
CELL_WIDTH = 171
CELL_HEIGHT = 205
BACKGROUND_DISTANCE_THRESHOLD = 46
DIRECTION_ROWS = [
    {"name": "South", "source_row": 5, "mirrored": False},
    {"name": "SouthWest", "source_row": 1, "mirrored": False},
    {"name": "West", "source_row": 2, "mirrored": False},
    {"name": "NorthWest", "source_row": 3, "mirrored": False},
    {"name": "North", "source_row": 4, "mirrored": False},
    {"name": "NorthEast", "source_row": 3, "mirrored": True},
    {"name": "East", "source_row": 2, "mirrored": True},
    {"name": "SouthEast", "source_row": 1, "mirrored": True},
]
IDLE_SOURCE_FILES = {
    "South": "HeroDeepBlue_Idle_South_h190.png",
    "SouthWest": "HeroDeepBlue_Idle_SouthWest_h190.png",
    "West": "HeroDeepBlue_Idle_West_h190.png",
    "NorthWest": "HeroDeepBlue_Idle_NorthWest_h190.png",
    "North": "HeroDeepBlue_Idle_North_h190.png",
    "NorthEast": "HeroDeepBlue_Idle_NorthEast_h190.png",
    "East": "HeroDeepBlue_Idle_East_h190.png",
    "SouthEast": "HeroDeepBlue_Idle_SouthEast_h190.png",
}


def _color_distance(a: tuple[int, int, int, int], b: tuple[int, int, int, int]) -> int:
    return max(abs(int(a[index]) - int(b[index])) for index in range(3))


def _remove_connected_background(image, background):
    transparent = image.copy()
    pixels = transparent.load()
    width, height = transparent.size
    visited: set[tuple[int, int]] = set()
    stack: list[tuple[int, int]] = []

    for x in range(width):
        stack.append((x, 0))
        stack.append((x, height - 1))
    for y in range(height):
        stack.append((0, y))
        stack.append((width - 1, y))

    while stack:
        x, y = stack.pop()
        if x < 0 or y < 0 or x >= width or y >= height or (x, y) in visited:
            continue
        visited.add((x, y))
        if _color_distance(pixels[x, y], background) > BACKGROUND_DISTANCE_THRESHOLD:
            continue
        r, g, b, _a = pixels[x, y]
        pixels[x, y] = (r, g, b, 0)
        stack.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))

    return transparent


def _alpha_bbox_metrics(image, cell_width: int = CELL_WIDTH, cell_height: int = CELL_HEIGHT) -> dict[str, float | int] | None:
    bbox = image.getchannel("A").getbbox()
    if bbox is None:
        return None
    left, top, right, bottom = bbox
    return {
        "left": left,
        "top": top,
        "right": right,
        "bottom": bottom,
        "width": right - left,
        "height": bottom - top,
        "bottom_gap": cell_height - bottom,
        "center_offset": ((left + right - 1) / 2.0) - ((cell_width - 1) / 2.0),
    }


def _walk_reference_metrics(walk_sheet) -> dict[str, dict[str, float]]:
    expected_size = (CELL_WIDTH * 6, CELL_HEIGHT * len(DIRECTION_ROWS))
    if walk_sheet.size != expected_size:
        raise RuntimeError(f"Unexpected expanded walk sheet size {walk_sheet.size}; expected {expected_size}")

    references: dict[str, dict[str, float]] = {}
    for target_row, direction in enumerate(DIRECTION_ROWS):
        direction_name = str(direction["name"])
        bottom_gaps: list[float] = []
        center_offsets: list[float] = []
        for column in range(6):
            crop = walk_sheet.crop((
                column * CELL_WIDTH,
                target_row * CELL_HEIGHT,
                (column + 1) * CELL_WIDTH,
                (target_row + 1) * CELL_HEIGHT,
            ))
            metrics = _alpha_bbox_metrics(crop)
            if metrics is None:
                raise RuntimeError(f"Walk reference cell is empty for {direction_name} frame {column}")
            bottom_gaps.append(float(metrics["bottom_gap"]))
            center_offsets.append(float(metrics["center_offset"]))
        references[direction_name] = {
            "target_bottom_gap": float(median(bottom_gaps)),
            "target_center_offset": float(median(center_offsets)),
        }
    return references


def _build_expanded_sheet() -> dict[str, Any]:
    try:
        from PIL import Image, ImageOps
    except ImportError as exc:
        raise RuntimeError("Pillow is required to build the deterministic mirrored hero walk sheet") from exc

    if not SOURCE_SHEET.exists():
        raise RuntimeError(f"Missing source hero walk sheet: {SOURCE_SHEET}")

    source = Image.open(SOURCE_SHEET).convert("RGBA")
    if source.size != (1024, 1024):
        raise RuntimeError(f"Unexpected source sheet size {source.size}; expected 1024x1024")

    bg = source.getpixel((0, 0))
    expanded = Image.new("RGBA", (CELL_WIDTH * 6, CELL_HEIGHT * len(DIRECTION_ROWS)), (0, 0, 0, 0))
    for target_row, direction in enumerate(DIRECTION_ROWS):
        source_row_index = int(direction["source_row"]) - 1
        for col in range(6):
            crop = source.crop((
                ORIGINAL_X_EDGES[col],
                ORIGINAL_Y_EDGES[source_row_index],
                ORIGINAL_X_EDGES[col + 1],
                ORIGINAL_Y_EDGES[source_row_index + 1],
            ))
            if bool(direction["mirrored"]):
                crop = ImageOps.mirror(crop)
            crop = _remove_connected_background(crop, bg)
            paste_x = col * CELL_WIDTH + (CELL_WIDTH - crop.size[0]) // 2
            paste_y = target_row * CELL_HEIGHT + (CELL_HEIGHT - crop.size[1]) // 2
            expanded.alpha_composite(crop, (paste_x, paste_y))

    previous_bytes = EXPANDED_SHEET.read_bytes() if EXPANDED_SHEET.exists() else None
    EXPANDED_SHEET.parent.mkdir(parents=True, exist_ok=True)
    expanded.save(EXPANDED_SHEET)
    new_bytes = EXPANDED_SHEET.read_bytes()
    alpha = expanded.getchannel("A")
    transparent_pixels = alpha.tobytes().count(0)
    return {
        "source": str(SOURCE_SHEET),
        "expanded": str(EXPANDED_SHEET),
        "changed": previous_bytes != new_bytes,
        "size": [expanded.size[0], expanded.size[1]],
        "cell_size": [CELL_WIDTH, CELL_HEIGHT],
        "transparent_pixels": transparent_pixels,
        "directions": DIRECTION_ROWS,
    }


def _build_idle_sheet() -> dict[str, Any]:
    try:
        from PIL import Image
    except ImportError as exc:
        raise RuntimeError("Pillow is required to build the deterministic hero idle sheet") from exc

    missing_sources = [
        str(IDLE_SOURCE_DIR / file_name)
        for file_name in IDLE_SOURCE_FILES.values()
        if not (IDLE_SOURCE_DIR / file_name).exists()
    ]
    if missing_sources:
        if IDLE_SHEET.exists():
            return {
                "source_dir": str(IDLE_SOURCE_DIR),
                "idle_sheet": str(IDLE_SHEET),
                "changed": False,
                "used_existing_sheet": True,
                "missing_sources": missing_sources,
            }
        raise RuntimeError(f"Missing idle source images: {missing_sources}")

    if not EXPANDED_SHEET.exists():
        raise RuntimeError(f"Missing expanded walk reference sheet: {EXPANDED_SHEET}")

    walk_reference = Image.open(EXPANDED_SHEET).convert("RGBA")
    walk_references = _walk_reference_metrics(walk_reference)
    idle_sheet = Image.new("RGBA", (CELL_WIDTH, CELL_HEIGHT * len(DIRECTION_ROWS)), (0, 0, 0, 0))
    placements: dict[str, dict[str, float | int | list[int]]] = {}
    for target_row, direction in enumerate(DIRECTION_ROWS):
        direction_name = str(direction["name"])
        source_image = Image.open(IDLE_SOURCE_DIR / IDLE_SOURCE_FILES[direction_name]).convert("RGBA")
        if source_image.width > CELL_WIDTH or source_image.height > CELL_HEIGHT:
            raise RuntimeError(
                f"Idle source {IDLE_SOURCE_FILES[direction_name]} size {source_image.size} exceeds cell {(CELL_WIDTH, CELL_HEIGHT)}"
            )
        source_metrics = _alpha_bbox_metrics(source_image, source_image.width, source_image.height)
        if source_metrics is None:
            raise RuntimeError(f"Idle source {IDLE_SOURCE_FILES[direction_name]} has no visible alpha")

        reference = walk_references[direction_name]
        desired_center_x = ((CELL_WIDTH - 1) / 2.0) + float(reference["target_center_offset"])
        source_center_x = (float(source_metrics["left"]) + float(source_metrics["right"]) - 1.0) / 2.0
        paste_x = round(desired_center_x - source_center_x)
        paste_x = max(0, min(CELL_WIDTH - source_image.width, paste_x))

        desired_bottom_gap = float(reference["target_bottom_gap"])
        paste_y_in_cell = round(CELL_HEIGHT - desired_bottom_gap - float(source_metrics["bottom"]))
        paste_y_in_cell = max(0, min(CELL_HEIGHT - source_image.height, paste_y_in_cell))
        paste_y = target_row * CELL_HEIGHT + paste_y_in_cell
        idle_sheet.alpha_composite(source_image, (paste_x, paste_y))
        placements[direction_name] = {
            "paste": [paste_x, paste_y_in_cell],
            "target_bottom_gap": float(reference["target_bottom_gap"]),
            "target_center_offset": float(reference["target_center_offset"]),
            "source_bottom_gap": float(source_metrics["bottom_gap"]),
            "source_center_offset": float(source_metrics["center_offset"]),
        }

    previous_bytes = IDLE_SHEET.read_bytes() if IDLE_SHEET.exists() else None
    IDLE_SHEET.parent.mkdir(parents=True, exist_ok=True)
    idle_sheet.save(IDLE_SHEET)
    new_bytes = IDLE_SHEET.read_bytes()
    alpha = idle_sheet.getchannel("A")
    transparent_pixels = alpha.tobytes().count(0)
    return {
        "source_dir": str(IDLE_SOURCE_DIR),
        "idle_sheet": str(IDLE_SHEET),
        "changed": previous_bytes != new_bytes,
        "used_existing_sheet": False,
        "size": [idle_sheet.size[0], idle_sheet.size[1]],
        "cell_size": [CELL_WIDTH, CELL_HEIGHT],
        "transparent_pixels": transparent_pixels,
        "directions": [direction["name"] for direction in DIRECTION_ROWS],
        "placements": placements,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--path", default=DEFAULT_PATH)
    parser.add_argument("--report", type=Path, default=PROJECT_ROOT / "Saved" / "HarnessReports" / "gamexxk-character-visual-apply.json")
    args = parser.parse_args()

    client = UnrealMCPClient(host=args.host, port=args.port, path=args.path, timeout=180.0)
    result: dict[str, Any] = {
        "ok": False,
        "endpoint": client.endpoint,
        "expanded_sheet_builder": str(EXPANDED_SHEET),
        "idle_sheet_builder": str(IDLE_SHEET),
        "assembler": ASSEMBLER,
        "validator": VALIDATOR,
    }
    try:
        result["expanded_sheet"] = _build_expanded_sheet()
        result["idle_sheet"] = _build_idle_sheet()
        if not client.connect():
            raise RuntimeError(f"Cannot connect to UE MCP at {client.endpoint}")
        assemble_response = client.run_project_python_file(ASSEMBLER)
        validate_response = client.run_project_python_file(VALIDATOR)
        result["assembly"] = parse_stdout_json(str(assemble_response.get("stdout", "")))
        validation = parse_stdout_json(str(validate_response.get("stdout", "")))
        result["validation"] = validation
        result["ok"] = bool(validation.get("ok"))
    except Exception as exc:
        result["error"] = str(exc)

    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result.get("ok") else 1


if __name__ == "__main__":
    sys.exit(main())
