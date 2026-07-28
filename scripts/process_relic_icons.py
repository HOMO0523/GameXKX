#!/usr/bin/env python3
"""Key, normalize, and validate the 30 generated route-relic icons."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


EXPECTED_SLUGS = (
    "AncientCoin",
    "JadeBell",
    "BambooTally",
    "TigerSeal",
    "MedicineGourd",
    "InkTalisman",
    "CloudMirror",
    "StoneBead",
    "CraneFeather",
    "IronKnot",
    "TeaBrick",
    "Compass",
    "RedCord",
    "BronzeNeedle",
    "RainCape",
    "ChessStone",
    "DrumCharm",
    "LotusSeed",
    "SwordGuard",
    "OldMap",
    "PineCone",
    "RiverPearl",
    "CandleStub",
    "FoxMask",
    "StoneLion",
    "WineCup",
    "HerbBasket",
    "PaperCrane",
    "BrokenArrow",
    "MoonDisc",
)


def parse_args() -> argparse.Namespace:
    project_root = Path(__file__).resolve().parents[1]
    default_chroma_tool = (
        Path.home()
        / ".codex"
        / "skills"
        / ".system"
        / "imagegen"
        / "scripts"
        / "remove_chroma_key.py"
    )
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input-dir",
        type=Path,
        default=project_root / "SourceArt" / "UI" / "Relics" / "chroma",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=project_root / "SourceArt" / "UI" / "Relics" / "final",
    )
    parser.add_argument(
        "--report",
        type=Path,
        default=project_root / "SourceArt" / "UI" / "Relics" / "relic_icon_report.json",
    )
    parser.add_argument(
        "--contact-sheet",
        type=Path,
        default=project_root / "SourceArt" / "UI" / "Relics" / "relic_icon_contact_sheet.png",
    )
    parser.add_argument("--chroma-tool", type=Path, default=default_chroma_tool)
    parser.add_argument("--size", type=int, default=512)
    parser.add_argument("--subject-fill", type=float, default=0.86)
    return parser.parse_args()


def alpha_bbox(image: Image.Image, threshold: int = 16) -> tuple[int, int, int, int] | None:
    alpha = image.getchannel("A")
    binary = alpha.point(lambda value: 255 if value >= threshold else 0)
    return binary.getbbox()


def normalize_icon(source: Path, destination: Path, size: int, subject_fill: float) -> dict[str, object]:
    with Image.open(source) as opened:
        image = opened.convert("RGBA")
    bbox = alpha_bbox(image)
    if bbox is None:
        raise RuntimeError(f"{source.name} is fully transparent after chroma removal")
    cropped = image.crop(bbox)
    target_extent = max(1, round(size * subject_fill))
    scale = min(target_extent / cropped.width, target_extent / cropped.height)
    resized = cropped.resize(
        (max(1, round(cropped.width * scale)), max(1, round(cropped.height * scale))),
        Image.Resampling.LANCZOS,
    )
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    offset = ((size - resized.width) // 2, (size - resized.height) // 2)
    canvas.alpha_composite(resized, offset)
    destination.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(destination, optimize=True)

    normalized_bbox = alpha_bbox(canvas)
    assert normalized_bbox is not None
    left, top, right, bottom = normalized_bbox
    longest_fill = max(right - left, bottom - top) / size
    corner_alpha = max(
        canvas.getpixel((0, 0))[3],
        canvas.getpixel((size - 1, 0))[3],
        canvas.getpixel((0, size - 1))[3],
        canvas.getpixel((size - 1, size - 1))[3],
    )
    opaque_pixels = 0
    magenta_pixels = 0
    for red, green, blue, alpha in canvas.getdata():
        if alpha <= 32:
            continue
        opaque_pixels += 1
        if red >= 180 and blue >= 180 and green <= 105:
            magenta_pixels += 1
    magenta_ratio = magenta_pixels / max(1, opaque_pixels)
    if corner_alpha != 0:
        raise RuntimeError(f"{destination.name} retains non-transparent canvas corners")
    if not 0.82 <= longest_fill <= 0.89:
        raise RuntimeError(
            f"{destination.name} subject fill {longest_fill:.3f} is outside the 0.82-0.89 HUD contract"
        )
    if magenta_ratio > 0.0025:
        raise RuntimeError(
            f"{destination.name} retains too much chroma spill ({magenta_ratio:.4%})"
        )
    return {
        "file": destination.name,
        "size": [size, size],
        "subject_fill": round(longest_fill, 4),
        "corner_alpha": corner_alpha,
        "magenta_spill_ratio": round(magenta_ratio, 6),
    }


def build_contact_sheet(icon_paths: list[Path], output: Path, icon_size: int) -> None:
    columns = 5
    rows = 6
    cell_width = 220
    cell_height = 242
    sheet = Image.new("RGB", (columns * cell_width, rows * cell_height), (235, 229, 213))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    for index, path in enumerate(icon_paths):
        row, column = divmod(index, columns)
        with Image.open(path) as opened:
            icon = opened.convert("RGBA")
        preview = icon.resize((190, 190), Image.Resampling.LANCZOS)
        x = column * cell_width + (cell_width - 190) // 2
        y = row * cell_height + 8
        sheet.paste(preview, (x, y), preview)
        label = path.stem.removeprefix("T_Relic_")
        text_box = draw.textbbox((0, 0), label, font=font)
        text_width = text_box[2] - text_box[0]
        draw.text(
            (column * cell_width + (cell_width - text_width) // 2, row * cell_height + 208),
            label,
            fill=(57, 60, 55),
            font=font,
        )
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output, optimize=True)


def main() -> int:
    args = parse_args()
    if not args.chroma_tool.is_file():
        raise FileNotFoundError(f"imagegen chroma tool not found: {args.chroma_tool}")
    if not 0.5 <= args.subject_fill <= 0.95:
        raise ValueError("--subject-fill must be between 0.5 and 0.95")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    records: list[dict[str, object]] = []
    icon_paths: list[Path] = []
    with tempfile.TemporaryDirectory(prefix="gamexxk_relic_key_") as temporary:
        temporary_dir = Path(temporary)
        for slug in EXPECTED_SLUGS:
            source = args.input_dir / f"T_Relic_{slug}.png"
            if not source.is_file():
                raise FileNotFoundError(f"missing generated relic source: {source}")
            keyed = temporary_dir / source.name
            subprocess.run(
                [
                    sys.executable,
                    str(args.chroma_tool),
                    "--input",
                    str(source),
                    "--out",
                    str(keyed),
                    "--key-color",
                    "#ff00ff",
                    "--soft-matte",
                    "--transparent-threshold",
                    "20",
                    "--opaque-threshold",
                    "115",
                    "--edge-contract",
                    "1",
                    "--edge-feather",
                    "0.5",
                    "--spill-cleanup",
                    "--force",
                ],
                check=True,
            )
            destination = args.output_dir / source.name
            records.append(normalize_icon(keyed, destination, args.size, args.subject_fill))
            icon_paths.append(destination)
    build_contact_sheet(icon_paths, args.contact_sheet, args.size)
    report = {
        "icon_count": len(records),
        "expected_icon_count": len(EXPECTED_SLUGS),
        "canvas_size": args.size,
        "subject_fill_target": args.subject_fill,
        "icons": records,
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": "ok", "icon_count": len(records), "report": str(args.report)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
