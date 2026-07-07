#!/usr/bin/env python3
"""Prepare clipboard concept art into GameXXK source PNG sprites.

This keeps the original references under Content/GameXXK/SourceArt and writes
transparent, cropped battle-ready PNGs under Content/GameXXK/Sprites/Generated.
"""

from __future__ import annotations

import argparse
import shutil
from collections import deque
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ART_DIR = PROJECT_ROOT / "Content" / "GameXXK" / "SourceArt" / "Concepts"
GENERATED_DIR = PROJECT_ROOT / "Content" / "GameXXK" / "Sprites" / "Generated"


def _color_distance_sq(a: tuple[int, int, int], b: tuple[int, int, int]) -> int:
    return sum((int(a[index]) - int(b[index])) ** 2 for index in range(3))


def _average_corner_color(image: Image.Image) -> tuple[int, int, int]:
    width, height = image.size
    sample_points = [
        (0, 0),
        (width - 1, 0),
        (0, height - 1),
        (width - 1, height - 1),
    ]
    colors = [image.getpixel(point)[:3] for point in sample_points]
    return tuple(sum(color[index] for color in colors) // len(colors) for index in range(3))


def _clear_connected_background(image: Image.Image, threshold: int = 48) -> Image.Image:
    image = image.convert("RGBA")
    width, height = image.size
    pixels = image.load()
    background = _average_corner_color(image)
    threshold_sq = threshold * threshold

    queue: deque[tuple[int, int]] = deque()
    visited: set[tuple[int, int]] = set()
    for x in range(width):
        queue.append((x, 0))
        queue.append((x, height - 1))
    for y in range(height):
        queue.append((0, y))
        queue.append((width - 1, y))

    while queue:
        x, y = queue.popleft()
        if (x, y) in visited or x < 0 or y < 0 or x >= width or y >= height:
            continue
        visited.add((x, y))
        color = pixels[x, y][:3]
        if _color_distance_sq(color, background) > threshold_sq:
            continue
        pixels[x, y] = (pixels[x, y][0], pixels[x, y][1], pixels[x, y][2], 0)
        queue.append((x + 1, y))
        queue.append((x - 1, y))
        queue.append((x, y + 1))
        queue.append((x, y - 1))

    return image


def _trim_alpha(image: Image.Image, pad: int = 10) -> Image.Image:
    alpha = image.getchannel("A")
    bounds = alpha.getbbox()
    if not bounds:
        return image
    left, top, right, bottom = bounds
    left = max(0, left - pad)
    top = max(0, top - pad)
    right = min(image.width, right + pad)
    bottom = min(image.height, bottom + pad)
    return image.crop((left, top, right, bottom))


def _resize_to_height(image: Image.Image, max_height: int) -> Image.Image:
    if image.height <= max_height:
        return image
    width = max(1, round(image.width * max_height / image.height))
    return image.resize((width, max_height), Image.Resampling.LANCZOS)


def _prepare_sprite(
    source: Path,
    crop_box: tuple[int, int, int, int],
    output: Path,
    max_height: int,
    background_threshold: int = 58,
) -> dict:
    image = Image.open(source).convert("RGBA").crop(crop_box)
    image = _clear_connected_background(image, threshold=background_threshold)
    image = _trim_alpha(image)
    image = _resize_to_height(image, max_height=max_height)
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output)
    return {"path": str(output), "size": {"x": image.width, "y": image.height}}


def main() -> int:
    parser = argparse.ArgumentParser(description="Prepare GameXXK enemy concept art")
    parser.add_argument("--cover", required=True)
    parser.add_argument("--money-mouse", required=True)
    parser.add_argument("--bear-tiger", required=True)
    parser.add_argument("--niu-huan", required=True)
    args = parser.parse_args()

    SOURCE_ART_DIR.mkdir(parents=True, exist_ok=True)
    GENERATED_DIR.mkdir(parents=True, exist_ok=True)

    raw_refs = {
        "main_menu_cover_tiger_duel.png": Path(args.cover),
        "enemy_money_mouse_ref.png": Path(args.money_mouse),
        "enemy_bear_tiger_ref.png": Path(args.bear_tiger),
        "enemy_niu_huan_ref.png": Path(args.niu_huan),
    }
    for target_name, source in raw_refs.items():
        if not source.exists():
            raise FileNotFoundError(source)
        shutil.copy2(source, SOURCE_ART_DIR / target_name)

    results = {
        "money_mouse": _prepare_sprite(
            Path(args.money_mouse),
            (165, 45, 765, 765),
            GENERATED_DIR / "enemy_money_mouse.png",
            max_height=235,
            background_threshold=62,
        ),
        "black_bear": _prepare_sprite(
            Path(args.bear_tiger),
            (48, 0, 262, 250),
            GENERATED_DIR / "enemy_black_bear.png",
            max_height=245,
            background_threshold=74,
        ),
        "tiger_boss": _prepare_sprite(
            Path(args.bear_tiger),
            (56, 300, 255, 552),
            GENERATED_DIR / "enemy_tiger_boss.png",
            max_height=285,
            background_threshold=70,
        ),
        "niu_huan": _prepare_sprite(
            Path(args.niu_huan),
            (120, 80, 650, 540),
            GENERATED_DIR / "enemy_niu_huan.png",
            max_height=250,
            background_threshold=66,
        ),
    }

    print(results)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
