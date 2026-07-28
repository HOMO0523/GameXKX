"""Build traceable battle HP/MP Track + Fill PNGs from the approved PSD cuts.

The original 420x32 character bars are composited examples (049 is green and
partially filled; 050 is vermilion and full).  They cannot be used as a UMG
ProgressBar background.  This script keeps the original source images in the
project, derives an empty parchment Track, and derives a transparent Fill-only
texture using only pixels sampled from the same PSD cuts.  GameXXK's semantic
mapping is deliberate: health uses the vermilion 050 cut and mana uses the
green 049 cut.

Run from the GameXXK project root:
    python scripts/build_battle_resource_psd_cuts.py
"""

from __future__ import annotations

import json
import shutil
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
OUTPUT_ROOT = PROJECT_ROOT / "SourceArt" / "UI" / "Battle" / "ResourceBars"
SOURCE_ROOT = OUTPUT_ROOT / "PSDSource"
EXTERNAL_PSD_ROOT = Path(
    r"C:\Users\shxuw\Downloads\nw-studio-nwueball-https-github-com"
    r"\nw-studio-nwueball-https-github-com\work\psd_rebuild\clean_assets_v2"
)

BAR_SIZE = (420, 32)
# Coordinates are inclusive-left/exclusive-right in the original 420x32 V2
# cuts.  The two PSD bars have slightly different left/right ink caps.
RESOURCE_SPECS = {
    "health": {
        "inner_rect": (8, 11, 407, 19),
        "fill_sample_rect": (8, 11, 407, 19),
        # 050 is fully filled, so its empty-paper sample is deliberately
        # borrowed from the matching 049 PSD paper channel.
        "empty_sample_rect": (323, 11, 408, 19),
    },
    "mana": {
        "inner_rect": (13, 11, 408, 19),
        "fill_sample_rect": (13, 11, 323, 19),
        "empty_sample_rect": (323, 11, 408, 19),
    },
}

SOURCES = {
    "health": ("050.png", "psd_character_mana_full.png"),
    "mana": ("049.png", "psd_character_health_full.png"),
}


def ensure_project_source(external_name: str, local_name: str) -> Path:
    """Keep a local, versionable copy of the exact reviewed source cut."""
    SOURCE_ROOT.mkdir(parents=True, exist_ok=True)
    local_path = SOURCE_ROOT / local_name
    if local_path.is_file():
        return local_path
    external_path = EXTERNAL_PSD_ROOT / external_name
    if not external_path.is_file():
        raise RuntimeError(
            "Missing approved PSD cut. Expected either "
            f"{local_path} or {external_path}"
        )
    shutil.copy2(external_path, local_path)
    return local_path


def load_rgba(path: Path) -> Image.Image:
    image = Image.open(path).convert("RGBA")
    if image.size != BAR_SIZE:
        raise RuntimeError(f"Unexpected PSD bar size for {path}: {image.size}; expected {BAR_SIZE}")
    return image


def stretch_sample(
    image: Image.Image,
    source_rect: tuple[int, int, int, int],
    target_rect: tuple[int, int, int, int],
) -> Image.Image:
    left, top, right, bottom = target_rect
    return image.crop(source_rect).resize((right - left, bottom - top), Image.Resampling.LANCZOS)


def build_track(
    bar_source: Image.Image,
    empty_sample: Image.Image,
    inner_rect: tuple[int, int, int, int],
    empty_sample_rect: tuple[int, int, int, int],
) -> Image.Image:
    track = bar_source.copy()
    left, top, _right, _bottom = inner_rect
    track.paste(stretch_sample(empty_sample, empty_sample_rect, inner_rect), (left, top))
    return track


def build_fill(
    bar_source: Image.Image,
    source_rect: tuple[int, int, int, int],
    inner_rect: tuple[int, int, int, int],
) -> Image.Image:
    fill = Image.new("RGBA", BAR_SIZE, (0, 0, 0, 0))
    left, top, _right, _bottom = inner_rect
    fill.paste(stretch_sample(bar_source, source_rect, inner_rect), (left, top))
    return fill


def compose_full(track: Image.Image, fill: Image.Image) -> Image.Image:
    """Reproduce the right-most, full-state preview tile from Track + inner Fill."""
    result = track.copy()
    result.alpha_composite(fill)
    return result


def build_preview(tracks: dict[str, Image.Image], fills: dict[str, Image.Image]) -> Path:
    percentages = (0.0, 0.35, 0.72, 1.0)
    row_height = 52
    preview = Image.new("RGBA", (BAR_SIZE[0] * len(percentages), row_height * 2), (245, 238, 222, 255))
    for row, key in enumerate(("health", "mana")):
        for column, percent in enumerate(percentages):
            composite = tracks[key].copy()
            clipped_width = round(BAR_SIZE[0] * percent)
            if clipped_width > 0:
                composite.alpha_composite(fills[key].crop((0, 0, clipped_width, BAR_SIZE[1])))
            preview.alpha_composite(composite, (column * BAR_SIZE[0], row * row_height + 10))
    preview_path = OUTPUT_ROOT / "battle_resource_bar_cut_preview.png"
    preview.save(preview_path)
    return preview_path


def main() -> None:
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    source_paths = {
        kind: ensure_project_source(external_name, local_name)
        for kind, (external_name, local_name) in SOURCES.items()
    }
    source_images = {kind: load_rgba(path) for kind, path in source_paths.items()}

    health_spec = RESOURCE_SPECS["health"]
    mana_spec = RESOURCE_SPECS["mana"]
    tracks = {
        "health": build_track(
            source_images["health"],
            source_images["mana"],
            health_spec["inner_rect"],
            health_spec["empty_sample_rect"],
        ),
        "mana": build_track(
            source_images["mana"],
            source_images["mana"],
            mana_spec["inner_rect"],
            mana_spec["empty_sample_rect"],
        ),
    }
    fills = {
        "health": build_fill(source_images["health"], health_spec["fill_sample_rect"], health_spec["inner_rect"]),
        "mana": build_fill(source_images["mana"], mana_spec["fill_sample_rect"], mana_spec["inner_rect"]),
    }
    fulls = {key: compose_full(tracks[key], fills[key]) for key in tracks}

    output_files = {
        "health_track": OUTPUT_ROOT / "battle_psd_health_track.png",
        "health_fill": OUTPUT_ROOT / "battle_psd_health_fill.png",
        "health_full": OUTPUT_ROOT / "battle_psd_health_full.png",
        "mana_track": OUTPUT_ROOT / "battle_psd_mana_track.png",
        "mana_fill": OUTPUT_ROOT / "battle_psd_mana_fill.png",
        "mana_full": OUTPUT_ROOT / "battle_psd_mana_full.png",
    }
    tracks["health"].save(output_files["health_track"])
    fills["health"].save(output_files["health_fill"])
    fulls["health"].save(output_files["health_full"])
    tracks["mana"].save(output_files["mana_track"])
    fills["mana"].save(output_files["mana_fill"])
    fulls["mana"].save(output_files["mana_full"])
    preview_path = build_preview(tracks, fills)

    manifest = {
        "source": {kind: str(path) for kind, path in source_paths.items()},
        "output": {kind: str(path) for kind, path in output_files.items()},
        "preview": str(preview_path),
        "bar_size": BAR_SIZE,
        "resource_specs": RESOURCE_SPECS,
        "note": "All color, paper, and ink pixels are sampled from the approved PSD 049/050 cuts.",
    }
    manifest_path = OUTPUT_ROOT / "battle_resource_bar_cut_manifest.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"ok": True, "output": manifest["output"], "preview": str(preview_path)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
