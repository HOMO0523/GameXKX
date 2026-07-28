"""OpenCV-free binary-mask operations for the local PSD cutting pipeline."""

from __future__ import annotations

from collections import deque
from pathlib import Path
from typing import Mapping

import numpy as np
from PIL import Image


_NEIGHBORS = (
    (-1, -1),
    (-1, 0),
    (-1, 1),
    (0, -1),
    (0, 1),
    (1, -1),
    (1, 0),
    (1, 1),
)


def _component(
    mask: np.ndarray,
    visited: np.ndarray,
    start_y: int,
    start_x: int,
    *,
    collect_points: bool,
) -> tuple[int, list[tuple[int, int]]]:
    height, width = mask.shape
    points: list[tuple[int, int]] = []
    area = 0
    queue: deque[tuple[int, int]] = deque([(start_y, start_x)])
    visited[start_y, start_x] = True
    while queue:
        y, x = queue.popleft()
        area += 1
        if collect_points:
            points.append((y, x))
        for offset_y, offset_x in _NEIGHBORS:
            neighbor_y = y + offset_y
            neighbor_x = x + offset_x
            if (
                0 <= neighbor_y < height
                and 0 <= neighbor_x < width
                and mask[neighbor_y, neighbor_x]
                and not visited[neighbor_y, neighbor_x]
            ):
                visited[neighbor_y, neighbor_x] = True
                queue.append((neighbor_y, neighbor_x))
    return area, points


def clean_mask(mask: np.ndarray, min_area: int) -> np.ndarray:
    """Remove small exterior components and retain enclosed white details."""
    foreground = np.asarray(mask, dtype=bool)
    kept = np.zeros_like(foreground, dtype=bool)
    visited_foreground = np.zeros_like(foreground, dtype=bool)

    for start_y, start_x in np.argwhere(foreground):
        if visited_foreground[start_y, start_x]:
            continue
        area, points = _component(
            foreground,
            visited_foreground,
            int(start_y),
            int(start_x),
            collect_points=True,
        )
        if area >= min_area:
            ys, xs = zip(*points)
            kept[np.asarray(ys), np.asarray(xs)] = True

    inverse = ~kept
    visited_background = np.zeros_like(inverse, dtype=bool)
    height, width = inverse.shape
    border_points = [
        *((0, x) for x in range(width)),
        *((height - 1, x) for x in range(width)),
        *((y, 0) for y in range(1, height - 1)),
        *((y, width - 1) for y in range(1, height - 1)),
    ]
    for start_y, start_x in border_points:
        if inverse[start_y, start_x] and not visited_background[start_y, start_x]:
            _component(
                inverse,
                visited_background,
                start_y,
                start_x,
                collect_points=False,
            )

    kept[inverse & ~visited_background] = True
    return kept.astype(np.uint8)


def export_runtime_backgrounds(
    sheet: Image.Image,
    destination: Path,
    rectangles: Mapping[str, tuple[int, int, int, int]],
) -> dict[str, Path]:
    """Split a composite PSD background into unscaled, screen-specific PNGs."""
    destination.mkdir(parents=True, exist_ok=True)
    for obsolete in destination.glob("T_TownPsd_Background_*.png"):
        obsolete.unlink()

    outputs: dict[str, Path] = {}
    for page, rectangle in rectangles.items():
        if len(rectangle) != 4:
            raise ValueError(f"invalid crop rectangle for {page}")
        left, top, right, bottom = rectangle
        if not (0 <= left < right <= sheet.width and 0 <= top < bottom <= sheet.height):
            raise ValueError(f"crop rectangle is outside the source sheet for {page}")
        filename = f"T_TownPsd_Background_{page[:1].upper()}{page[1:]}.png"
        output = destination / filename
        sheet.crop(rectangle).save(output)
        outputs[page] = output
    return outputs
