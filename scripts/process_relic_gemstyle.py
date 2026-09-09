"""User-authorized relic cleanup: background removal, sizing and review layout only.

Never redraw, recolor, sharpen, add ornament, or otherwise change the generated object.
The original generator output is copied unchanged and hashed before cleanup.
"""
import argparse
import hashlib
import json
import shutil
from collections import deque
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "SourceArt/UI/Relics/gemstyle-20260910"


def connected_background(candidate):
    height, width = candidate.shape
    pixels = bytearray(candidate.astype(np.uint8).tobytes())
    pending = deque()
    def enqueue(index):
        if pixels[index] == 1:
            pixels[index] = 2
            pending.append(index)
    for x in range(width):
        enqueue(x)
        enqueue((height - 1) * width + x)
    for y in range(height):
        enqueue(y * width)
        enqueue(y * width + width - 1)
    while pending:
        index = pending.popleft()
        x = index % width
        if x: enqueue(index - 1)
        if x + 1 < width: enqueue(index + 1)
        if index >= width: enqueue(index - width)
        if index + width < len(pixels): enqueue(index + width)
    return np.frombuffer(pixels, dtype=np.uint8).reshape(height, width) == 2


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def process(source, slug, background, group="relic"):
    if not slug.isascii() or not slug.isalnum():
        raise ValueError("A relic slug must be an ASCII alphanumeric identifier")
    output_root = OUT if group == "relic" else ROOT / "SourceArt/UI/Hunt/gemstyle-20260910"
    prefix = "T_Relic_" if group == "relic" else "T_Item_"
    raw_dir, icon_dir = output_root / "raw", output_root / "icons"
    raw_dir.mkdir(parents=True, exist_ok=True)
    icon_dir.mkdir(parents=True, exist_ok=True)
    raw_path = raw_dir / f"{prefix}{slug}.png"
    if raw_path.exists() and digest(raw_path) != digest(source):
        raise ValueError(f"Raw output already exists with different bytes: {raw_path}")
    if source.resolve() != raw_path.resolve():
        shutil.copy2(source, raw_path)
    original = Image.open(raw_path)
    rgba = np.array(original.convert("RGBA"))
    rgb = rgba[:, :, :3].copy()
    if "A" in original.getbands() and rgba[:, :, 3].min() == 0:
        removed = int(np.count_nonzero(rgba[:, :, 3] == 0))
        background = "existing-alpha"
    else:
        colors = rgb.astype(np.int16)
        if background == "magenta":
            # Chroma-key is deliberately absent from the generated relic palette.
            mask = (colors[:, :, 0] > 175) & (colors[:, :, 2] > 150) & (colors[:, :, 1] < 110)
        elif background == "neutral":
            # Only border-connected neutral checkerboard pixels. Dark outlines and
            # any enclosed pale highlights on the object remain untouched.
            high, low = colors.max(axis=2), colors.min(axis=2)
            candidate = (high - low <= 27) & (low >= 55)
            mask = connected_background(candidate)
        else:
            raise ValueError("RGB input needs an explicit supported background mask")
        removed = int(mask.sum())
        rgba[:, :, 3][mask] = 0
    if removed == 0:
        raise ValueError("No background was removed; do not import an opaque background")
    if not np.array_equal(rgb, rgba[:, :, :3]):
        raise AssertionError("Cleanup must not recolor the object")
    cleaned = Image.fromarray(rgba)
    bounds = cleaned.getchannel("A").getbbox()
    if not bounds:
        raise ValueError("The cleanup removed the whole image")
    cropped = cleaned.crop(bounds)
    scale = 452 / max(cropped.size)
    size = tuple(max(1, round(v * scale)) for v in cropped.size)
    resized = cropped.resize(size, Image.Resampling.LANCZOS)
    final = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
    final.alpha_composite(resized, ((512 - size[0]) // 2, (512 - size[1]) // 2))
    output = icon_dir / f"{prefix}{slug}.png"
    final.save(output)
    alpha = np.array(final.getchannel("A"))
    if any(np.any(edge) for edge in (alpha[0], alpha[-1], alpha[:, 0], alpha[:, -1])):
        raise AssertionError("All four image edges must be transparent")
    record = {"slug": slug, "raw": str(raw_path.relative_to(ROOT)).replace("\\", "/"),
              "sourceGeneratedPath": str(source.resolve()), "rawSha256": digest(raw_path),
              "icon": str(output.relative_to(ROOT)).replace("\\", "/"), "sha256": digest(output),
              "background": background, "removedBackgroundPixels": removed,
              "sourceAlphaBounds": list(bounds), "size": [512, 512], "alphaBounds": list(final.getchannel("A").getbbox()),
              "subjectPixelsRecoloredBeforeResize": 0, "visualReview": "pending", "imported": False}
    manifest_path = output_root / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {
        "schemaVersion": 1, "styleReference": "SourceArt/UI/Items/Gems/contour-review-20260909/type-icons-v5",
        "backgroundAndResizeAuthorized": True, "expectedUniqueIcons": 45 if group == "relic" else 5, "icons": []}
    manifest["icons"] = [r for r in manifest["icons"] if r["slug"] != slug] + [record]
    temporary = manifest_path.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(manifest_path)
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--slug", required=True)
    parser.add_argument("--background", choices=["neutral", "magenta"], required=True)
    parser.add_argument("--group", choices=["relic", "hunt"], default="relic")
    args = parser.parse_args()
    print(json.dumps(process(args.input, args.slug, args.background, args.group), ensure_ascii=False))


if __name__ == "__main__":
    main()
