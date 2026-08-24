#!/usr/bin/env python3
"""Normalize and validate the fixed 3 x 10 GameXXK gem-icon matrix.

This processor deliberately requires genuine source alpha.  It never derives
alpha from source colors and never chroma-keys a generated image.
"""

from __future__ import annotations

import argparse
from array import array
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys
from typing import Iterable, Sequence

from PIL import Image, ImageDraw, ImageFont


GEM_TYPES = ("Attack", "Defense", "MaxHealth")
QUALITIES = (
    "Common",
    "Rare",
    "Epic",
    "Legendary",
    "Immortal",
    "Treasure",
    "Transcendent",
    "Celestial",
    "Ascendant",
    "Cosmic",
)
ASSET_MATRIX = tuple(
    (gem_type, quality) for gem_type in GEM_TYPES for quality in QUALITIES
)

CANVAS_SIZE = 512
SUBJECT_FILL = 0.75
TARGET_SUBJECT_EXTENT = round(CANVAS_SIZE * SUBJECT_FILL)
ALPHA_THRESHOLD = 16

CHECKER_COLOR_BUCKET = 8
CHECKER_MAX_CHANNEL_SPREAD = 24
CHECKER_MIN_DOMINANT_COVERAGE = 0.45
CHECKER_MIN_BBOX_LABEL_COVERAGE = 0.45
CHECKER_COLOR_TOLERANCE = 20
CHECKER_MIN_CELL_LABEL_COVERAGE = 0.75
CHECKER_MIN_CELL_PURITY = 0.90
CHECKER_MIN_GRID_CELL_COVERAGE = 0.50
CHECKER_MIN_GRID_EXTENT_COVERAGE = 0.75
CHECKER_MIN_PERIPHERAL_CELL_COVERAGE = 0.70
CHECKER_MIN_PARITY_SCORE = 0.95
CHECKER_MIN_BOUNDARY_EVIDENCE = 0.40
CHECKER_MIN_BOUNDARY_FLIP_RATIO = 0.85
CHECKER_MIN_STABLE_BOUNDARY_RATIO = 0.35
CHECKER_MAX_PHASE_CANDIDATES = 4
CHECKER_MAX_TILE_SIZE = 128

CONTACT_PREVIEW_SIZE = 160
CONTACT_INSET_SIZE = 48
CONTACT_CELL_WIDTH = 192
CONTACT_CELL_HEIGHT = 240
CONTACT_PREVIEW_OFFSET = (16, 20)
CONTACT_INSET_OFFSET = (8, 184)
PAPER_BACKGROUND = (235, 229, 213)
PAPER_INK = (57, 60, 55)

PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INPUT_DIR = PROJECT_ROOT / "SourceArt" / "UI" / "Items" / "Gems" / "generated"
DEFAULT_OUTPUT_DIR = PROJECT_ROOT / "SourceArt" / "UI" / "Items" / "Gems" / "final"
DEFAULT_MANIFEST = (
    PROJECT_ROOT / "SourceArt" / "UI" / "Items" / "Gems" / "gem_icon_manifest.json"
)
DEFAULT_CONTACT_SHEET = (
    PROJECT_ROOT
    / "SourceArt"
    / "UI"
    / "Items"
    / "Gems"
    / "review"
    / "gem-quality-progression-contact-sheet.png"
)


def _validate_axis_value(value: str, allowed: tuple[str, ...], axis: str) -> None:
    if value not in allowed:
        raise ValueError(f"unknown gem {axis} {value!r}; expected one of {allowed!r}")


def stem(gem_type: str, quality: str) -> str:
    _validate_axis_value(gem_type, GEM_TYPES, "type")
    _validate_axis_value(quality, QUALITIES, "quality")
    return f"T_Item_Gem_{gem_type}_{quality}"


def item_id(gem_type: str, quality: str) -> str:
    _validate_axis_value(gem_type, GEM_TYPES, "type")
    _validate_axis_value(quality, QUALITIES, "quality")
    return f"Item.Gem.{gem_type}.{quality}"


def texture_path(gem_type: str, quality: str) -> str:
    name = stem(gem_type, quality)
    return f"/Game/GameXXK/UI/Items/Gems/{name}.{name}"


def alpha_bbox(
    image: Image.Image, threshold: int = ALPHA_THRESHOLD
) -> tuple[int, int, int, int] | None:
    """Return the bounding box of alpha values that meet the visibility threshold."""

    if "A" not in image.getbands():
        raise ValueError("alpha_bbox requires an image with an alpha channel")
    alpha = image.getchannel("A")
    binary = alpha.point(lambda value: 255 if value >= threshold else 0)
    return binary.getbbox()


def _quantized_rgb(pixel: tuple[int, int, int, int]) -> tuple[int, int, int]:
    return tuple(
        channel // CHECKER_COLOR_BUCKET for channel in pixel[:3]
    )  # type: ignore[return-value]


def _is_low_saturation(pixel: tuple[int, int, int, int]) -> bool:
    return max(pixel[:3]) - min(pixel[:3]) <= CHECKER_MAX_CHANNEL_SPREAD


def _bucket_center(bucket: tuple[int, int, int]) -> tuple[int, int, int]:
    half_bucket = CHECKER_COLOR_BUCKET // 2
    return tuple(
        min(255, channel * CHECKER_COLOR_BUCKET + half_bucket) for channel in bucket
    )  # type: ignore[return-value]


def _dominant_checker_colors(
    image: Image.Image,
) -> tuple[tuple[int, int, int], tuple[int, int, int]] | None:
    """Find two neutral colors that dominate an opaque alpha-bbox crop."""

    opaque_count = 0
    counts: Counter[tuple[int, int, int]] = Counter()
    for pixel in image.getdata():
        if pixel[3] < 250:
            continue
        opaque_count += 1
        if _is_low_saturation(pixel):
            counts[_quantized_rgb(pixel)] += 1
    if opaque_count < 64:
        return None
    if len(counts) < 2:
        return None

    ((first, first_count), (second, second_count)) = counts.most_common(2)
    if (
        (first_count + second_count) / opaque_count < CHECKER_MIN_DOMINANT_COVERAGE
    ):
        return None
    if min(first_count, second_count) / opaque_count < 0.15:
        return None

    first_color = _bucket_center(first)
    second_color = _bucket_center(second)
    luminance_delta = abs(sum(first_color) / 3 - sum(second_color) / 3)
    if not 12 <= luminance_delta <= 96:
        return None
    return first_color, second_color


def _color_distance_squared(
    pixel: tuple[int, int, int, int], color: tuple[int, int, int]
) -> int:
    return sum(
        (pixel_channel - color_channel) ** 2
        for pixel_channel, color_channel in zip(pixel[:3], color)
    )


def _checker_color_labels(
    image: Image.Image,
    colors: tuple[tuple[int, int, int], tuple[int, int, int]],
) -> tuple[array, int]:
    """Classify only confident opaque pixels as one of the two neutral colors."""

    labels = array("b")
    labeled_count = 0
    maximum_distance = 3 * CHECKER_COLOR_TOLERANCE**2
    for pixel in image.getdata():
        label = -1
        if pixel[3] >= 250 and _is_low_saturation(pixel):
            first_distance = _color_distance_squared(pixel, colors[0])
            second_distance = _color_distance_squared(pixel, colors[1])
            nearest_distance = min(first_distance, second_distance)
            if (
                nearest_distance <= maximum_distance
                and abs(first_distance - second_distance) >= 12**2
            ):
                label = 0 if first_distance < second_distance else 1
                labeled_count += 1
        labels.append(label)
    return labels, labeled_count


def _label_integrals(
    labels: Sequence[int], width: int, height: int
) -> tuple[array, array]:
    """Build known-label and label-one summed-area tables."""

    stride = width + 1
    table_size = stride * (height + 1)
    known_integral = array("I", [0]) * table_size
    one_integral = array("I", [0]) * table_size
    for y in range(height):
        known_row_sum = 0
        one_row_sum = 0
        source_row = y * width
        previous_row = y * stride
        current_row = (y + 1) * stride
        for x in range(width):
            label = labels[source_row + x]
            known_row_sum += label >= 0
            one_row_sum += label == 1
            known_integral[current_row + x + 1] = (
                known_integral[previous_row + x + 1] + known_row_sum
            )
            one_integral[current_row + x + 1] = (
                one_integral[previous_row + x + 1] + one_row_sum
            )
    return known_integral, one_integral


def _integral_rect_sum(
    integral: Sequence[int],
    stride: int,
    left: int,
    top: int,
    right: int,
    bottom: int,
) -> int:
    return (
        integral[bottom * stride + right]
        - integral[top * stride + right]
        - integral[bottom * stride + left]
        + integral[top * stride + left]
    )


def _grid_axis_starts(extent: int, tile_size: int, boundary_phase: int) -> list[int]:
    """Return full cells plus clipped edge cells that retain at least half a tile."""

    starts: list[int] = []
    for start in range(boundary_phase - tile_size, extent, tile_size):
        clipped_extent = min(extent, start + tile_size) - max(0, start)
        if clipped_extent * 2 >= tile_size:
            starts.append(start)
    return starts


def _classify_grid_cells(
    known_integral: Sequence[int],
    one_integral: Sequence[int],
    width: int,
    height: int,
    tile_size: int,
    x_starts: Sequence[int],
    y_starts: Sequence[int],
) -> array:
    """Classify cells only when one source color fills nearly all known area."""

    cells = array("b")
    stride = width + 1
    for raw_top in y_starts:
        for raw_left in x_starts:
            left = max(0, raw_left)
            top = max(0, raw_top)
            right = min(width, raw_left + tile_size)
            bottom = min(height, raw_top + tile_size)
            cell_area = (right - left) * (bottom - top)
            known_count = _integral_rect_sum(
                known_integral, stride, left, top, right, bottom
            )
            one_count = _integral_rect_sum(
                one_integral, stride, left, top, right, bottom
            )
            zero_count = known_count - one_count
            dominant_count = max(zero_count, one_count)
            if (
                known_count / cell_area >= CHECKER_MIN_CELL_LABEL_COVERAGE
                and dominant_count / known_count >= CHECKER_MIN_CELL_PURITY
            ):
                cells.append(1 if one_count > zero_count else 0)
            else:
                cells.append(-1)
    return cells


def _indices_match_parity(
    cells: Sequence[int],
    indices: Iterable[int],
    columns: int,
    polarity: int,
    minimum_coverage: float,
) -> bool:
    selected = tuple(indices)
    valid_count = 0
    matching_count = 0
    for index in selected:
        label = cells[index]
        if label < 0:
            continue
        valid_count += 1
        row, column = divmod(index, columns)
        matching_count += label == (((row + column) % 2) ^ polarity)
    return (
        bool(selected)
        and valid_count / len(selected) >= minimum_coverage
        and matching_count / valid_count >= CHECKER_MIN_PARITY_SCORE
    )


def _strong_grid_parity(cells: Sequence[int], rows: int, columns: int) -> bool:
    """Require global parity plus multiple strong peripheral rows and columns."""

    total_cells = rows * columns
    valid_indices = [index for index, label in enumerate(cells) if label >= 0]
    if len(valid_indices) < 16:
        return False
    if len(valid_indices) / total_cells < CHECKER_MIN_GRID_CELL_COVERAGE:
        return False

    label_counts = Counter(cells[index] for index in valid_indices)
    if min(label_counts.get(0, 0), label_counts.get(1, 0)) / len(valid_indices) < 0.25:
        return False
    zero_polarity_matches = sum(
        cells[index] == ((sum(divmod(index, columns))) % 2)
        for index in valid_indices
    )
    polarity = 0 if zero_polarity_matches >= len(valid_indices) / 2 else 1
    if (
        max(zero_polarity_matches, len(valid_indices) - zero_polarity_matches)
        / len(valid_indices)
        < CHECKER_MIN_PARITY_SCORE
    ):
        return False

    band_depth = 2 if min(rows, columns) >= 6 else 1
    peripheral_indices = (
        index
        for index in range(total_cells)
        if (
            index // columns < band_depth
            or index // columns >= rows - band_depth
            or index % columns < band_depth
            or index % columns >= columns - band_depth
        )
    )
    if not _indices_match_parity(
        cells,
        peripheral_indices,
        columns,
        polarity,
        CHECKER_MIN_PERIPHERAL_CELL_COVERAGE,
    ):
        return False

    sides = (
        range(0, columns),
        range((rows - 1) * columns, rows * columns),
        range(0, total_cells, columns),
        range(columns - 1, total_cells, columns),
    )
    return all(
        _indices_match_parity(
            cells,
            side,
            columns,
            polarity,
            CHECKER_MIN_PERIPHERAL_CELL_COVERAGE,
        )
        for side in sides
    )


def _strong_axis_boundaries(
    labels: Sequence[int], width: int, height: int, *, vertical: bool
) -> tuple[int, ...]:
    """Locate real full-axis color flips before considering a tile phase."""

    strong: list[int] = []
    axis_extent = width if vertical else height
    cross_extent = height if vertical else width
    for coordinate in range(1, axis_extent):
        known_pairs = 0
        flipped_pairs = 0
        for cross_coordinate in range(cross_extent):
            if vertical:
                first = labels[cross_coordinate * width + coordinate - 1]
                second = labels[cross_coordinate * width + coordinate]
            else:
                first = labels[(coordinate - 1) * width + cross_coordinate]
                second = labels[coordinate * width + cross_coordinate]
            if first < 0 or second < 0:
                continue
            known_pairs += 1
            flipped_pairs += first != second
        if (
            known_pairs / cross_extent >= CHECKER_MIN_BOUNDARY_EVIDENCE
            and flipped_pairs / known_pairs >= CHECKER_MIN_BOUNDARY_FLIP_RATIO
        ):
            strong.append(coordinate)
    return tuple(strong)


def _candidate_boundary_phases(
    strong_boundaries: Sequence[int], tile_size: int
) -> tuple[int, ...]:
    """Infer a small phase set from repeated real-boundary residues."""

    counts = Counter(boundary % tile_size for boundary in strong_boundaries)
    return tuple(
        phase
        for phase, count in counts.most_common(CHECKER_MAX_PHASE_CANDIDATES)
        if count >= 3
    )


def _axis_grid_coverage(starts: Sequence[int], extent: int, tile_size: int) -> float:
    covered = sum(
        max(0, min(extent, start + tile_size) - max(0, start)) for start in starts
    )
    return covered / extent


def _stable_candidate_boundaries(
    strong_boundaries: set[int], starts: Sequence[int], extent: int
) -> bool:
    expected = tuple(start for start in starts[1:] if 0 < start < extent)
    if len(expected) < 3:
        return False
    stable_count = sum(boundary in strong_boundaries for boundary in expected)
    return (
        stable_count >= 3
        and stable_count / len(expected) >= CHECKER_MIN_STABLE_BOUNDARY_RATIO
    )


def _has_checker_grid_period(labels: Sequence[int], width: int, height: int) -> bool:
    """Validate coherent axis-aligned cells, boundaries, and two-color parity."""

    vertical_boundaries = _strong_axis_boundaries(
        labels, width, height, vertical=True
    )
    horizontal_boundaries = _strong_axis_boundaries(
        labels, width, height, vertical=False
    )
    if len(vertical_boundaries) < 3 or len(horizontal_boundaries) < 3:
        return False

    vertical_boundary_set = set(vertical_boundaries)
    horizontal_boundary_set = set(horizontal_boundaries)
    known_integral, one_integral = _label_integrals(labels, width, height)
    maximum_tile_size = min(CHECKER_MAX_TILE_SIZE, min(width, height) // 4)
    for tile_size in range(2, maximum_tile_size + 1):
        x_phases = _candidate_boundary_phases(vertical_boundaries, tile_size)
        y_phases = _candidate_boundary_phases(horizontal_boundaries, tile_size)
        for phase_y in y_phases:
            y_starts = _grid_axis_starts(height, tile_size, phase_y)
            if len(y_starts) < 4:
                continue
            if not _stable_candidate_boundaries(
                horizontal_boundary_set, y_starts, height
            ):
                continue
            for phase_x in x_phases:
                x_starts = _grid_axis_starts(width, tile_size, phase_x)
                if len(x_starts) < 4:
                    continue
                if not _stable_candidate_boundaries(
                    vertical_boundary_set, x_starts, width
                ):
                    continue
                if (
                    _axis_grid_coverage(x_starts, width, tile_size)
                    < CHECKER_MIN_GRID_EXTENT_COVERAGE
                    or _axis_grid_coverage(y_starts, height, tile_size)
                    < CHECKER_MIN_GRID_EXTENT_COVERAGE
                ):
                    continue
                cells = _classify_grid_cells(
                    known_integral,
                    one_integral,
                    width,
                    height,
                    tile_size,
                    x_starts,
                    y_starts,
                )
                if _strong_grid_parity(cells, len(y_starts), len(x_starts)):
                    return True
    return False


def has_baked_checkerboard(image: Image.Image) -> bool:
    """Detect a baked neutral checker pattern anywhere inside the alpha bounds."""

    if image.mode != "RGBA":
        raise ValueError("checkerboard validation requires RGBA pixels")
    visible_bbox = alpha_bbox(image)
    if visible_bbox is None:
        return False
    cropped = image.crop(visible_bbox)
    if min(cropped.size) < 8:
        return False

    colors = _dominant_checker_colors(cropped)
    if colors is None:
        return False
    labels, labeled_count = _checker_color_labels(cropped, colors)
    if (
        labeled_count / (cropped.width * cropped.height)
        < CHECKER_MIN_BBOX_LABEL_COVERAGE
    ):
        return False
    return _has_checker_grid_period(labels, cropped.width, cropped.height)


def _corner_points(size: tuple[int, int]) -> tuple[tuple[int, int], ...]:
    width, height = size
    return ((0, 0), (width - 1, 0), (0, height - 1), (width - 1, height - 1))


def transparent_border_ratio(image: Image.Image) -> float:
    """Return the fraction of unique outer-border pixels whose alpha is zero."""

    if "A" not in image.getbands():
        raise ValueError("transparent_border_ratio requires an alpha channel")
    width, height = image.size
    border = [(x, 0) for x in range(width)]
    if height > 1:
        border.extend((x, height - 1) for x in range(width))
    if width > 1 and height > 2:
        border.extend((0, y) for y in range(1, height - 1))
        border.extend((width - 1, y) for y in range(1, height - 1))
    transparent = sum(image.getpixel(point)[3] == 0 for point in border)
    return transparent / len(border)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _load_genuine_alpha(source: Path) -> Image.Image:
    with Image.open(source) as opened:
        opened.load()
        if opened.format != "PNG":
            raise RuntimeError(f"{source.name} is not a PNG image")
        if "A" not in opened.getbands():
            raise RuntimeError(f"{source.name} has no alpha channel")
        image = opened.convert("RGBA")

    if has_baked_checkerboard(image):
        raise RuntimeError(f"{source.name} contains a baked checkerboard background")

    alpha_minimum, alpha_maximum = image.getchannel("A").getextrema()
    if alpha_maximum == 0:
        raise RuntimeError(f"{source.name} is fully transparent")
    if alpha_minimum == 255:
        raise RuntimeError(f"{source.name} has no transparent pixels (fully opaque canvas)")

    if max(image.getpixel(point)[3] for point in _corner_points(image.size)) != 0:
        raise RuntimeError(f"{source.name} has an opaque canvas corner")
    return image


def normalize_icon(source: Path, destination: Path) -> dict[str, object]:
    """Normalize one genuine-alpha source onto a centered 512-square canvas."""

    image = _load_genuine_alpha(source)
    source_bbox = alpha_bbox(image)
    if source_bbox is None:
        raise RuntimeError(
            f"{source.name} has no visible alpha at threshold {ALPHA_THRESHOLD}"
        )

    cropped = image.crop(source_bbox)
    if cropped.width >= cropped.height:
        resized_size = (
            TARGET_SUBJECT_EXTENT,
            max(1, round(cropped.height * TARGET_SUBJECT_EXTENT / cropped.width)),
        )
    else:
        resized_size = (
            max(1, round(cropped.width * TARGET_SUBJECT_EXTENT / cropped.height)),
            TARGET_SUBJECT_EXTENT,
        )
    resized = cropped.resize(resized_size, Image.Resampling.LANCZOS)

    canvas = Image.new("RGBA", (CANVAS_SIZE, CANVAS_SIZE), (0, 0, 0, 0))
    offset = (
        (CANVAS_SIZE - resized.width) // 2,
        (CANVAS_SIZE - resized.height) // 2,
    )
    canvas.alpha_composite(resized, offset)

    destination.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(destination, format="PNG", optimize=True)

    final_bbox = alpha_bbox(canvas)
    if final_bbox is None:
        raise RuntimeError(f"{destination.name} became fully transparent during normalization")
    if transparent_border_ratio(canvas) != 1.0:
        raise RuntimeError(f"{destination.name} has a non-transparent normalized border")

    return {
        "size": [CANVAS_SIZE, CANVAS_SIZE],
        "mode": "RGBA",
        "alpha_bbox": list(final_bbox),
        "transparent_border_ratio": round(transparent_border_ratio(canvas), 6),
        "sha256": sha256_file(destination),
    }


def _centered_text_x(
    draw: ImageDraw.ImageDraw, text: str, left: int, width: int, font: ImageFont.ImageFont
) -> int:
    bounds = draw.textbbox((0, 0), text, font=font)
    return left + (width - (bounds[2] - bounds[0])) // 2


def build_contact_sheet(icon_paths: Sequence[Path], output: Path) -> None:
    """Build the ordered 3 x 10 paper-background progression review sheet."""

    if len(icon_paths) != len(ASSET_MATRIX):
        raise ValueError(
            f"contact sheet requires {len(ASSET_MATRIX)} icons, got {len(icon_paths)}"
        )

    sheet = Image.new(
        "RGB",
        (
            len(QUALITIES) * CONTACT_CELL_WIDTH,
            len(GEM_TYPES) * CONTACT_CELL_HEIGHT,
        ),
        PAPER_BACKGROUND,
    )
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()

    for index, (path, (gem_type, quality)) in enumerate(zip(icon_paths, ASSET_MATRIX)):
        row, column = divmod(index, len(QUALITIES))
        cell_x = column * CONTACT_CELL_WIDTH
        cell_y = row * CONTACT_CELL_HEIGHT
        with Image.open(path) as opened:
            icon = opened.convert("RGBA")

        preview = icon.resize(
            (CONTACT_PREVIEW_SIZE, CONTACT_PREVIEW_SIZE), Image.Resampling.LANCZOS
        )
        inset = icon.resize(
            (CONTACT_INSET_SIZE, CONTACT_INSET_SIZE), Image.Resampling.LANCZOS
        )
        preview_position = (
            cell_x + CONTACT_PREVIEW_OFFSET[0],
            cell_y + CONTACT_PREVIEW_OFFSET[1],
        )
        inset_position = (
            cell_x + CONTACT_INSET_OFFSET[0],
            cell_y + CONTACT_INSET_OFFSET[1],
        )
        sheet.paste(preview, preview_position, preview)
        sheet.paste(inset, inset_position, inset)

        quality_x = _centered_text_x(draw, quality, cell_x, CONTACT_CELL_WIDTH, font)
        draw.text((quality_x, cell_y + 4), quality, fill=PAPER_INK, font=font)
        draw.text(
            (cell_x + CONTACT_INSET_OFFSET[0] + CONTACT_INSET_SIZE + 8, cell_y + 201),
            gem_type,
            fill=PAPER_INK,
            font=font,
        )

    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output, format="PNG", optimize=True)


def _relative_manifest_path(path: Path, project_root: Path) -> str:
    resolved = path.resolve()
    try:
        return resolved.relative_to(project_root.resolve()).as_posix()
    except ValueError:
        return resolved.as_posix()


def _expected_names() -> tuple[str, ...]:
    return tuple(f"{stem(gem_type, quality)}.png" for gem_type, quality in ASSET_MATRIX)


def _raise_for_missing_inputs(input_dir: Path) -> None:
    missing = [name for name in _expected_names() if not (input_dir / name).is_file()]
    if missing:
        joined = ", ".join(missing)
        raise FileNotFoundError(
            f"missing generated gem inputs ({len(missing)} of {len(ASSET_MATRIX)}): {joined}"
        )


def _raise_for_unexpected_final_pngs(output_dir: Path) -> None:
    if not output_dir.is_dir():
        return
    expected = set(_expected_names())
    unexpected = sorted(path.name for path in output_dir.glob("*.png") if path.name not in expected)
    if unexpected:
        raise RuntimeError(
            "unexpected PNGs in gem final directory; refusing to delete or ignore them: "
            + ", ".join(unexpected)
        )


def _duplicate_hashes(records: Iterable[dict[str, object]]) -> list[str]:
    hashes = [str(record["sha256"]) for record in records]
    counts = Counter(hashes)
    return sorted(digest for digest, count in counts.items() if count > 1)


def process_icon_set(
    input_dir: Path = DEFAULT_INPUT_DIR,
    output_dir: Path = DEFAULT_OUTPUT_DIR,
    manifest_path: Path = DEFAULT_MANIFEST,
    contact_sheet_path: Path = DEFAULT_CONTACT_SHEET,
    *,
    project_root: Path = PROJECT_ROOT,
) -> dict[str, object]:
    """Process the complete matrix or fail; partial/missing sets never count as OK."""

    _raise_for_missing_inputs(input_dir)
    _raise_for_unexpected_final_pngs(output_dir)

    records: list[dict[str, object]] = []
    icon_paths: list[Path] = []
    for gem_type, quality in ASSET_MATRIX:
        name = stem(gem_type, quality)
        source = input_dir / f"{name}.png"
        destination = output_dir / f"{name}.png"
        metrics = normalize_icon(source, destination)
        records.append(
            {
                "item_id": item_id(gem_type, quality),
                "source_png": _relative_manifest_path(destination, project_root),
                "texture_path": texture_path(gem_type, quality),
                **metrics,
            }
        )
        icon_paths.append(destination)

    duplicates = _duplicate_hashes(records)
    if duplicates:
        raise RuntimeError(
            "all 30 final gem PNG hashes must be unique; duplicate SHA-256 values: "
            + ", ".join(duplicates)
        )

    actual_names = {path.name for path in output_dir.glob("*.png")}
    expected_names = set(_expected_names())
    if actual_names != expected_names:
        raise RuntimeError(
            f"final gem PNG set is not exact: expected {len(expected_names)}, got {len(actual_names)}"
        )

    build_contact_sheet(icon_paths, contact_sheet_path)
    manifest = {"icon_count": len(records), "records": records}
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )

    return {
        "ok": True,
        "icon_count": len(records),
        "manifest": _relative_manifest_path(manifest_path, project_root),
        "contact_sheet": _relative_manifest_path(contact_sheet_path, project_root),
    }


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Normalize the exact 30 genuine-alpha GameXXK gem icons."
    )
    parser.add_argument("--input-dir", type=Path, default=DEFAULT_INPUT_DIR)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--contact-sheet", type=Path, default=DEFAULT_CONTACT_SHEET)
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        result = process_icon_set(
            args.input_dir,
            args.output_dir,
            args.manifest,
            args.contact_sheet,
        )
    except (OSError, RuntimeError, ValueError) as error:
        print(json.dumps({"ok": False, "error": str(error)}), file=sys.stderr)
        return 1
    print(json.dumps(result))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
