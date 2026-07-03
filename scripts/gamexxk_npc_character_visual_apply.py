#!/usr/bin/env python3
"""Build and import copied NPC character visuals through UE MCP."""

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
GENERATED_DIR = PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated"
ASSEMBLER = "Content/Python/gamexxk_assemble_npc_character_visuals.py"
VALIDATOR = "Content/Python/gamexxk_validate_npc_character_visuals.py"

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

NPC_SHEETS = [
    {
        "role": "merchant",
        "source": GENERATED_DIR / "merchant_teal_robed_walk_8dir.png",
        "expanded": GENERATED_DIR / "merchant_teal_robed_walk_8dir_expanded.png",
        "idle": GENERATED_DIR / "merchant_teal_robed_idle_8dir.png",
        "idle_source": PROJECT_ROOT / "Saved" / "AssetAnalysis" / "TownNpcIdle" / "merchant_teal_robed_idle_8dir_imagegen.png",
    },
    {
        "role": "person",
        "source": GENERATED_DIR / "follower_blue_scholar_walk_8dir.png",
        "expanded": GENERATED_DIR / "follower_blue_scholar_walk_8dir_expanded.png",
        "idle": GENERATED_DIR / "follower_blue_scholar_idle_8dir.png",
        "idle_source": PROJECT_ROOT / "Saved" / "AssetAnalysis" / "TownNpcIdle" / "follower_blue_scholar_idle_8dir_imagegen.png",
    },
]


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


def _build_expanded_sheet(spec: dict[str, Path | str]) -> dict[str, Any]:
    try:
        from PIL import Image, ImageOps
    except ImportError as exc:
        raise RuntimeError("Pillow is required to build deterministic NPC walk sheets") from exc

    source_file = Path(spec["source"])
    expanded_file = Path(spec["expanded"])
    if not source_file.exists():
        raise RuntimeError(f"Missing NPC source sheet: {source_file}")

    source = Image.open(source_file).convert("RGBA")
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

    previous_bytes = expanded_file.read_bytes() if expanded_file.exists() else None
    expanded_file.parent.mkdir(parents=True, exist_ok=True)
    expanded.save(expanded_file)
    new_bytes = expanded_file.read_bytes()
    return {
        "role": spec["role"],
        "source": str(source_file),
        "expanded": str(expanded_file),
        "changed": previous_bytes != new_bytes,
        "size": [expanded.size[0], expanded.size[1]],
        "cell_size": [CELL_WIDTH, CELL_HEIGHT],
        "transparent_pixels": expanded.getchannel("A").tobytes().count(0),
        "directions": DIRECTION_ROWS,
    }


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
        heights: list[float] = []
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
            heights.append(float(metrics["height"]))
        references[direction_name] = {
            "target_bottom_gap": float(median(bottom_gaps)),
            "target_center_offset": float(median(center_offsets)),
            "target_height": float(median(heights)),
        }
    return references


def _is_idle_background_color(
    color: tuple[int, int, int, int],
    key: tuple[int, int, int] = (255, 0, 255),
    threshold: int = 72,
) -> bool:
    r, g, b, _a = color
    if max(abs(r - key[0]), abs(g - key[1]), abs(b - key[2])) <= threshold:
        return True
    return r >= 210 and b >= 210 and g <= 120 and abs(r - b) <= 80


def _remove_chroma_key(image, key: tuple[int, int, int] = (255, 0, 255), threshold: int = 72):
    cleaned = image.convert("RGBA")
    pixels = cleaned.load()
    width, height = cleaned.size
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            if a == 0:
                continue
            if _is_idle_background_color((r, g, b, a), key, threshold):
                pixels[x, y] = (r, g, b, 0)

    for _iteration in range(5):
        to_clear: list[tuple[int, int]] = []
        for y in range(height):
            for x in range(width):
                r, g, b, a = pixels[x, y]
                if a == 0 or not (r >= 120 and b >= 140 and g <= 135 and abs(r - b) <= 95):
                    continue
                touches_transparent = False
                for neighbor_y in range(max(0, y - 1), min(height, y + 2)):
                    for neighbor_x in range(max(0, x - 1), min(width, x + 2)):
                        if neighbor_x == x and neighbor_y == y:
                            continue
                        if pixels[neighbor_x, neighbor_y][3] == 0:
                            touches_transparent = True
                            break
                    if touches_transparent:
                        break
                if touches_transparent:
                    to_clear.append((x, y))
        if not to_clear:
            break
        for x, y in to_clear:
            r, g, b, _a = pixels[x, y]
            pixels[x, y] = (r, g, b, 0)
    return cleaned


def _idle_source_components(source_sheet, min_pixels: int = 128) -> list[dict[str, Any]]:
    width, height = source_sheet.size
    alpha_bytes = source_sheet.getchannel("A").tobytes()
    visited = bytearray(width * height)
    components: list[dict[str, Any]] = []

    for start_index, alpha in enumerate(alpha_bytes):
        if alpha == 0 or visited[start_index]:
            continue
        stack = [start_index]
        visited[start_index] = 1
        count = 0
        min_x = width
        min_y = height
        max_x = 0
        max_y = 0
        while stack:
            index = stack.pop()
            x = index % width
            y = index // width
            count += 1
            min_x = min(min_x, x)
            min_y = min(min_y, y)
            max_x = max(max_x, x)
            max_y = max(max_y, y)

            for neighbor_y in range(max(0, y - 1), min(height, y + 2)):
                row_index = neighbor_y * width
                for neighbor_x in range(max(0, x - 1), min(width, x + 2)):
                    if neighbor_x == x and neighbor_y == y:
                        continue
                    neighbor_index = row_index + neighbor_x
                    if visited[neighbor_index] or alpha_bytes[neighbor_index] == 0:
                        continue
                    visited[neighbor_index] = 1
                    stack.append(neighbor_index)

        if count < min_pixels:
            continue
        components.append({
            "bbox": [min_x, min_y, max_x + 1, max_y + 1],
            "count": count,
            "center": [(min_x + max_x + 1) / 2.0, (min_y + max_y + 1) / 2.0],
        })

    return sorted(components, key=lambda component: int(component["count"]), reverse=True)


def _idle_source_direction_crops(source_sheet, source_file: Path) -> dict[str, dict[str, Any]]:
    components = _idle_source_components(source_sheet)
    if len(components) < len(DIRECTION_ROWS):
        raise RuntimeError(
            f"Expected at least {len(DIRECTION_ROWS)} idle components, found {len(components)}: {source_file}"
        )

    components_by_y = sorted(components[: len(DIRECTION_ROWS)], key=lambda component: float(component["center"][1]))
    main_components: list[dict[str, Any]] = []
    for row_start in range(0, len(DIRECTION_ROWS), 2):
        row_components = components_by_y[row_start : row_start + 2]
        main_components.extend(sorted(row_components, key=lambda component: float(component["center"][0])))
    crops: dict[str, dict[str, Any]] = {}
    for direction, component in zip(DIRECTION_ROWS, main_components):
        direction_name = str(direction["name"])
        left, top, right, bottom = [int(value) for value in component["bbox"]]
        padding = 8
        padded_bbox = [
            max(0, left - padding),
            max(0, top - padding),
            min(source_sheet.width, right + padding),
            min(source_sheet.height, bottom + padding),
        ]
        crop = source_sheet.crop(tuple(padded_bbox))
        crop_metrics = _alpha_bbox_metrics(crop, crop.width, crop.height)
        if crop_metrics is None:
            raise RuntimeError(f"Idle component crop is empty for {direction_name}: {source_file}")
        crops[direction_name] = {
            "crop": crop,
            "component_bbox": [left, top, right, bottom],
            "source_bbox": [
                padded_bbox[0] + int(crop_metrics["left"]),
                padded_bbox[1] + int(crop_metrics["top"]),
                padded_bbox[0] + int(crop_metrics["right"]),
                padded_bbox[1] + int(crop_metrics["bottom"]),
            ],
            "component_pixels": int(component["count"]),
        }
    return crops


def _build_idle_sheet(spec: dict[str, Path | str]) -> dict[str, Any]:
    try:
        from PIL import Image
    except ImportError as exc:
        raise RuntimeError("Pillow is required to build deterministic NPC idle sheets") from exc

    expanded_file = Path(spec["expanded"])
    idle_file = Path(spec["idle"])
    idle_source_file = Path(spec["idle_source"])
    if not expanded_file.exists():
        raise RuntimeError(f"Missing expanded NPC walk sheet: {expanded_file}")
    if not idle_source_file.exists():
        raise RuntimeError(f"Missing independent NPC idle source sheet: {idle_source_file}")

    expanded = Image.open(expanded_file).convert("RGBA")
    expected_size = (CELL_WIDTH * 6, CELL_HEIGHT * len(DIRECTION_ROWS))
    if expanded.size != expected_size:
        raise RuntimeError(f"Unexpected expanded sheet size {expanded.size}; expected {expected_size}")

    idle_source = _remove_chroma_key(Image.open(idle_source_file).convert("RGBA"))
    if idle_source.width < 2 or idle_source.height < 4:
        raise RuntimeError(f"Unexpected idle source size {idle_source.size}: {idle_source_file}")

    references = _walk_reference_metrics(expanded)
    direction_crops = _idle_source_direction_crops(idle_source, idle_source_file)
    idle = Image.new("RGBA", (CELL_WIDTH, CELL_HEIGHT * len(DIRECTION_ROWS)), (0, 0, 0, 0))
    placements: dict[str, dict[str, float | int | list[int]]] = {}
    for target_row, direction in enumerate(DIRECTION_ROWS):
        direction_name = str(direction["name"])
        source_entry = direction_crops[direction_name]
        source_cell = source_entry["crop"]
        source_metrics = _alpha_bbox_metrics(source_cell, source_cell.width, source_cell.height)
        if source_metrics is None:
            raise RuntimeError(f"Idle source cell is empty for {direction_name}: {idle_source_file}")

        body = source_cell.crop((
            int(source_metrics["left"]),
            int(source_metrics["top"]),
            int(source_metrics["right"]),
            int(source_metrics["bottom"]),
        ))
        reference = references[direction_name]
        scale = min(
            float(reference["target_height"]) / max(1.0, float(source_metrics["height"])),
            (CELL_WIDTH - 4.0) / max(1.0, body.width),
            (CELL_HEIGHT - 4.0) / max(1.0, body.height),
        )
        if scale <= 0.0:
            raise RuntimeError(f"Invalid idle scale for {direction_name}: {scale}")
        resized_width = max(1, round(body.width * scale))
        resized_height = max(1, round(body.height * scale))
        if (resized_width, resized_height) != body.size:
            body = body.resize((resized_width, resized_height), Image.Resampling.NEAREST)

        desired_center_x = ((CELL_WIDTH - 1) / 2.0) + float(reference["target_center_offset"])
        paste_x = round(desired_center_x - ((body.width - 1) / 2.0))
        paste_x = max(0, min(CELL_WIDTH - body.width, paste_x))

        desired_bottom_gap = float(reference["target_bottom_gap"])
        paste_y_in_cell = round(CELL_HEIGHT - desired_bottom_gap - body.height)
        paste_y_in_cell = max(0, min(CELL_HEIGHT - body.height, paste_y_in_cell))
        paste_y = target_row * CELL_HEIGHT + paste_y_in_cell
        idle.alpha_composite(body, (paste_x, paste_y))
        placements[direction_name] = {
            "source_bbox": source_entry["source_bbox"],
            "component_bbox": source_entry["component_bbox"],
            "component_pixels": int(source_entry["component_pixels"]),
            "paste": [paste_x, paste_y_in_cell],
            "scale": float(scale),
            "target_height": float(reference["target_height"]),
            "target_bottom_gap": float(reference["target_bottom_gap"]),
            "target_center_offset": float(reference["target_center_offset"]),
        }

    previous_bytes = idle_file.read_bytes() if idle_file.exists() else None
    idle_file.parent.mkdir(parents=True, exist_ok=True)
    idle.save(idle_file)
    new_bytes = idle_file.read_bytes()
    return {
        "role": spec["role"],
        "source": str(idle_source_file),
        "idle": str(idle_file),
        "changed": previous_bytes != new_bytes,
        "size": [idle.size[0], idle.size[1]],
        "cell_size": [CELL_WIDTH, CELL_HEIGHT],
        "transparent_pixels": idle.getchannel("A").tobytes().count(0),
        "placements": placements,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--path", default=DEFAULT_PATH)
    parser.add_argument("--report", type=Path, default=PROJECT_ROOT / "Saved" / "HarnessReports" / "gamexxk-npc-character-visual-apply.json")
    args = parser.parse_args()

    client = UnrealMCPClient(host=args.host, port=args.port, path=args.path, timeout=180.0)
    result: dict[str, Any] = {
        "ok": False,
        "endpoint": client.endpoint,
        "assembler": ASSEMBLER,
        "validator": VALIDATOR,
    }
    try:
        result["expanded_sheets"] = [_build_expanded_sheet(spec) for spec in NPC_SHEETS]
        result["idle_sheets"] = [_build_idle_sheet(spec) for spec in NPC_SHEETS]
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
