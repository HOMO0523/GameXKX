#!/usr/bin/env python3
"""Build deterministic retry-toggle UI textures from approved generated sources."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

from PIL import Image, ImageOps


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = PROJECT_ROOT / "SourceArt" / "UI" / "Training" / "IdleStrip" / "GeneratedSource"
OUTPUT_ROOT = PROJECT_ROOT / "SourceArt" / "UI" / "Training" / "IdleStrip" / "final"
OUTPUT_SIZE = 256


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _normalize(source: Path, content_ratio: float) -> Image.Image:
    image = Image.open(source).convert("RGBA")
    alpha = image.getchannel("A").point(lambda value: 0 if value < 16 else value)
    image.putalpha(alpha)
    bounds = alpha.getbbox()
    if not bounds:
        raise RuntimeError(f"source has no visible alpha content: {source}")
    cropped = image.crop(bounds)
    target_extent = max(1, int(round(OUTPUT_SIZE * content_ratio)))
    scale = min(target_extent / cropped.width, target_extent / cropped.height)
    resized = cropped.resize(
        (max(1, int(round(cropped.width * scale))), max(1, int(round(cropped.height * scale)))),
        Image.Resampling.LANCZOS,
    )
    canvas = Image.new("RGBA", (OUTPUT_SIZE, OUTPUT_SIZE), (0, 0, 0, 0))
    offset = ((OUTPUT_SIZE - resized.width) // 2, (OUTPUT_SIZE - resized.height) // 2)
    canvas.alpha_composite(resized, offset)
    cleaned_alpha = canvas.getchannel("A").point(lambda value: 0 if value < 16 else value)
    canvas.putalpha(cleaned_alpha)
    return canvas


def _disabled_variant(enabled: Image.Image) -> Image.Image:
    alpha = enabled.getchannel("A")
    luminance = ImageOps.grayscale(enabled.convert("RGB"))
    disabled_rgb = ImageOps.colorize(
        luminance,
        black=(58, 57, 54),
        white=(188, 186, 179),
    )
    clean_rgb = Image.new("RGB", enabled.size, (0, 0, 0))
    clean_rgb.paste(disabled_rgb, (0, 0), alpha)
    disabled = clean_rgb.convert("RGBA")
    disabled.putalpha(alpha)
    return disabled


def main() -> None:
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)
    base = _normalize(SOURCE_ROOT / "retry_button_base_source.png", 0.96)
    enabled = _normalize(SOURCE_ROOT / "retry_icon_enabled_source.png", 0.90)
    disabled = _disabled_variant(enabled)

    outputs = {
        "T_TrainingRetryButtonBase.png": base,
        "T_TrainingRetryIconEnabled.png": enabled,
        "T_TrainingRetryIconDisabled.png": disabled,
    }
    for name, image in outputs.items():
        image.save(OUTPUT_ROOT / name, format="PNG", optimize=True)

    enabled_alpha = enabled.getchannel("A").tobytes()
    disabled_alpha = disabled.getchannel("A").tobytes()
    if enabled_alpha != disabled_alpha:
        raise RuntimeError("enabled and disabled retry icons do not share an identical alpha mask")

    report = {
        "status": "PASS",
        "size": [OUTPUT_SIZE, OUTPUT_SIZE],
        "sharedIconAlphaSha256": hashlib.sha256(enabled_alpha).hexdigest(),
        "outputs": {
            name: {
                "sha256": _sha256(OUTPUT_ROOT / name),
                "alphaExtrema": list(image.getchannel("A").getextrema()),
            }
            for name, image in outputs.items()
        },
    }
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
