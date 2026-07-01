#!/usr/bin/env python3
"""Build and import MVP hero Paper2D walk visuals through UE MCP."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from gamexxk_content_assembly_check import parse_stdout_json
from ue_mcp_client import DEFAULT_HOST, DEFAULT_PATH, DEFAULT_PORT, UnrealMCPClient


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_SHEET = PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "hero_deepblue_high_ponytail_walk_8dir.png"
EXPANDED_SHEET = PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated" / "hero_deepblue_high_ponytail_walk_8dir_expanded.png"
ASSEMBLER = "Content/Python/gamexxk_assemble_character_visuals.py"
VALIDATOR = "Content/Python/gamexxk_validate_character_visuals.py"

ORIGINAL_X_EDGES = [0, 171, 341, 512, 683, 853, 1024]
ORIGINAL_Y_EDGES = [0, 205, 410, 614, 819, 1024]
CELL_WIDTH = 171
CELL_HEIGHT = 205
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
    expanded = Image.new("RGBA", (CELL_WIDTH * 6, CELL_HEIGHT * len(DIRECTION_ROWS)), bg)
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
            paste_x = col * CELL_WIDTH + (CELL_WIDTH - crop.size[0]) // 2
            paste_y = target_row * CELL_HEIGHT + (CELL_HEIGHT - crop.size[1]) // 2
            expanded.paste(crop, (paste_x, paste_y))

    previous_bytes = EXPANDED_SHEET.read_bytes() if EXPANDED_SHEET.exists() else None
    EXPANDED_SHEET.parent.mkdir(parents=True, exist_ok=True)
    expanded.save(EXPANDED_SHEET)
    new_bytes = EXPANDED_SHEET.read_bytes()
    return {
        "source": str(SOURCE_SHEET),
        "expanded": str(EXPANDED_SHEET),
        "changed": previous_bytes != new_bytes,
        "size": [expanded.size[0], expanded.size[1]],
        "cell_size": [CELL_WIDTH, CELL_HEIGHT],
        "directions": DIRECTION_ROWS,
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
        "assembler": ASSEMBLER,
        "validator": VALIDATOR,
    }
    try:
        result["expanded_sheet"] = _build_expanded_sheet()
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
