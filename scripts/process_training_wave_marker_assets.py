from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = PROJECT_ROOT / "SourceArt" / "UI" / "Training" / "IdleStrip" / "final"
ASSET_ORDER = (
    "T_TrainingWaveMarkerNormal",
    "T_TrainingWaveMarkerElite",
    "T_TrainingWaveMarkerBoss",
)


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _extract_icon(segment: np.ndarray) -> Image.Image:
    intensity = segment.max(axis=2)
    foreground = intensity > 6
    ys, xs = np.where(foreground)
    if xs.size == 0 or ys.size == 0:
        raise RuntimeError("generated marker segment contains no foreground")

    margin = max(8, int(round(max(xs.max() - xs.min(), ys.max() - ys.min()) * 0.025)))
    x0 = max(0, int(xs.min()) - margin)
    x1 = min(segment.shape[1], int(xs.max()) + margin + 1)
    y0 = max(0, int(ys.min()) - margin)
    y1 = min(segment.shape[0], int(ys.max()) + margin + 1)
    crop = segment[y0:y1, x0:x1].astype(np.float32)

    # The approved GPT preview was delivered over pure black. Reconstruct a
    # genuine alpha channel from its antialiased edge intensity, while keeping
    # the authored low-saturation body and eye colours intact.
    crop_intensity = crop.max(axis=2)
    alpha = np.clip((crop_intensity - 3.0) * (255.0 / 29.0), 0.0, 255.0)
    alpha[alpha < 8.0] = 0.0
    rgba = np.dstack((crop, alpha)).clip(0, 255).astype(np.uint8)

    height, width = rgba.shape[:2]
    square = max(height, width)
    canvas = np.zeros((square, square, 4), dtype=np.uint8)
    offset_x = (square - width) // 2
    offset_y = (square - height) // 2
    canvas[offset_y : offset_y + height, offset_x : offset_x + width] = rgba
    image = Image.fromarray(canvas).resize((256, 256), Image.Resampling.LANCZOS)
    return image


def main() -> None:
    parser = argparse.ArgumentParser(description="Split approved GPT wave-marker sheet into real-alpha PNG assets.")
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    source = args.input.resolve()
    output = args.output.resolve()
    if not source.is_file():
        raise FileNotFoundError(source)
    output.mkdir(parents=True, exist_ok=True)

    sheet = np.asarray(Image.open(source).convert("RGB"))
    segments = np.array_split(sheet, len(ASSET_ORDER), axis=1)
    records: list[dict[str, object]] = []
    for asset_name, segment in zip(ASSET_ORDER, segments, strict=True):
        image = _extract_icon(segment)
        destination = output / f"{asset_name}.png"
        image.save(destination, format="PNG", optimize=True)
        alpha = np.asarray(image.getchannel("A"))
        if alpha.min() != 0 or alpha.max() != 255:
            raise RuntimeError(f"{asset_name} did not produce a complete alpha range")
        if any(image.getpixel(point)[3] != 0 for point in ((0, 0), (255, 0), (0, 255), (255, 255))):
            raise RuntimeError(f"{asset_name} has a non-transparent corner")
        records.append(
            {
                "asset": asset_name,
                "file": destination.name,
                "width": image.width,
                "height": image.height,
                "alphaCoverage": round(float((alpha > 0).mean()), 6),
                "sha256": _sha256(destination),
            }
        )

    manifest = {
        "status": "PASS",
        "source": str(source),
        "sourceSha256": _sha256(source),
        "assets": records,
    }
    manifest_path = output / "training_wave_marker_manifest.json"
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(manifest, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
