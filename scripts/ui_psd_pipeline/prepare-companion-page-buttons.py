from __future__ import annotations

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


PROJECT_ROOT = Path(__file__).resolve().parents[2]
OUTPUT_ROOT = (
    PROJECT_ROOT
    / "SourceArt"
    / "UI"
    / "PSD"
    / "gamexxk-v4"
    / "ui-master"
    / "ManualEditing"
    / "PartnerSwitchControls"
)
REFERENCE_PATH = (
    PROJECT_ROOT
    / "SourceArt"
    / "UI"
    / "PSD"
    / "gamexxk-v4"
    / "ui-master"
    / "Assets"
    / "Controls"
    / "close_button_ink_v2.png"
)
SOURCE_CHROMA_PATH = OUTPUT_ROOT / "companion_page_left_source_chroma.png"
SOURCE_CUTOUT_PATH = OUTPUT_ROOT / "companion_page_left_source_cutout.png"
PROMPT_PATH = OUTPUT_ROOT / "companion_page_arrow_generation_prompt.txt"
LEFT_PATH = OUTPUT_ROOT / "companion_page_left_Button.png"
RIGHT_PATH = OUTPUT_ROOT / "companion_page_right_Button.png"
TARGET_SIZE = (36, 62)
MAX_INK_SIZE = (24, 44)
INK_RGB = (31, 28, 24)
CONSTRUCTION_VERSION = "imagegen_two_stroke_chevron_no_horizontal_stem_v2"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def resize_premultiplied(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    data = np.asarray(image.convert("RGBA")).astype(np.float32)
    alpha = data[..., 3:4] / 255.0
    premultiplied = data[..., :3] * alpha

    resized_rgb = Image.fromarray(
        np.clip(premultiplied, 0, 255).astype(np.uint8)
    ).resize(size, Image.Resampling.LANCZOS)
    resized_alpha = Image.fromarray(data[..., 3].astype(np.uint8)).resize(
        size, Image.Resampling.LANCZOS
    )

    premultiplied_resized = np.asarray(resized_rgb).astype(np.float32)
    alpha_resized = np.asarray(resized_alpha).astype(np.float32) / 255.0
    output_rgb = np.zeros_like(premultiplied_resized)
    visible = alpha_resized > 0
    output_rgb[visible] = premultiplied_resized[visible] / alpha_resized[visible, None]

    output = np.zeros((size[1], size[0], 4), dtype=np.uint8)
    output[..., :3] = np.clip(output_rgb, 0, 255).astype(np.uint8)
    output[..., 3] = np.asarray(resized_alpha)
    return Image.fromarray(output)


def normalize_imagegen_cutout() -> Image.Image:
    source = Image.open(SOURCE_CUTOUT_PATH).convert("RGBA")
    data = np.asarray(source).astype(np.float32)
    rgb = data[..., :3]
    source_alpha = data[..., 3] / 255.0
    luma = rgb[..., 0] * 0.299 + rgb[..., 1] * 0.587 + rgb[..., 2] * 0.114

    # Preserve the generated ink density while turning bright keyed edge residue
    # into transparency. All remaining visible pixels use the UI kit's warm ink.
    darkness = np.clip((238.0 - luma) / 238.0, 0.0, 1.0)
    clean_alpha = source_alpha * np.power(darkness, 0.58)
    clean_alpha[clean_alpha < 0.012] = 0.0

    cleaned = np.zeros_like(data, dtype=np.uint8)
    cleaned[..., :3] = INK_RGB
    cleaned[..., 3] = np.clip(clean_alpha * 255.0, 0, 255).astype(np.uint8)
    cleaned_image = Image.fromarray(cleaned)

    crop_mask = cleaned_image.getchannel("A").point(lambda value: 255 if value >= 8 else 0)
    bounds = crop_mask.getbbox()
    if bounds is None:
        raise RuntimeError("generated chevron cutout has no visible ink")
    crop = cleaned_image.crop(bounds)

    scale = min(MAX_INK_SIZE[0] / crop.width, MAX_INK_SIZE[1] / crop.height)
    resized_size = (
        max(1, round(crop.width * scale)),
        max(1, round(crop.height * scale)),
    )
    resized = resize_premultiplied(crop, resized_size)
    canvas = Image.new("RGBA", TARGET_SIZE, (*INK_RGB, 0))
    position = (
        (TARGET_SIZE[0] - resized.width) // 2,
        (TARGET_SIZE[1] - resized.height) // 2,
    )
    canvas.alpha_composite(resized, position)
    return canvas


def alpha_metrics(image: Image.Image, direction: str) -> dict[str, object]:
    alpha = image.getchannel("A")
    bounds = alpha.getbbox()
    if bounds is None:
        raise RuntimeError(f"{direction}: alpha is empty")
    extrema = alpha.getextrema()
    if extrema[0] != 0 or extrema[1] == 0:
        raise RuntimeError(f"{direction}: expected transparent and visible pixels")

    center_points: list[tuple[int, int]] = []
    for y in range(28, 35):
        for x in range(TARGET_SIZE[0]):
            if alpha.getpixel((x, y)) >= 28:
                center_points.append((x, y))
    if not center_points:
        raise RuntimeError(f"{direction}: center vertex is missing")
    center_x = [point[0] for point in center_points]
    center_span = max(center_x) - min(center_x) + 1
    if center_span > 14:
        raise RuntimeError(
            f"{direction}: center span {center_span}px suggests a horizontal stem"
        )

    upper_pixels = 0
    lower_pixels = 0
    visible_pixels = 0
    for y in range(TARGET_SIZE[1]):
        for x in range(TARGET_SIZE[0]):
            if alpha.getpixel((x, y)) < 28:
                continue
            visible_pixels += 1
            if y < 28:
                upper_pixels += 1
            elif y > 34:
                lower_pixels += 1
    if min(upper_pixels, lower_pixels) < 30:
        raise RuntimeError(f"{direction}: expected two substantial diagonal strokes")

    return {
        "alphaBounds": list(bounds),
        "alphaExtrema": list(extrema),
        "visiblePixelsAtAlpha28": visible_pixels,
        "centerBandVisibleSpan": center_span,
        "upperStrokePixels": upper_pixels,
        "lowerStrokePixels": lower_pixels,
        "geometry": "two generated diagonal brush strokes; no horizontal stem",
    }


def validate_button(path: Path, direction: str) -> dict[str, object]:
    image = Image.open(path)
    if image.mode != "RGBA":
        raise RuntimeError(f"{path.name}: expected RGBA, got {image.mode}")
    if image.size != TARGET_SIZE:
        raise RuntimeError(f"{path.name}: expected {TARGET_SIZE}, got {image.size}")
    metrics = alpha_metrics(image, direction)
    metrics.update(
        {
            "path": path.relative_to(PROJECT_ROOT).as_posix(),
            "sha256": sha256_file(path),
            "width": image.width,
            "height": image.height,
            "mode": image.mode,
            "direction": direction,
        }
    )
    return metrics


def checkerboard(size: tuple[int, int], tile: int = 15) -> Image.Image:
    image = Image.new("RGBA", size, (238, 231, 217, 255))
    draw = ImageDraw.Draw(image)
    alternate = (217, 207, 190, 255)
    for y in range(0, size[1], tile):
        for x in range(0, size[0], tile):
            if (x // tile + y // tile) % 2:
                draw.rectangle(
                    (x, y, min(x + tile - 1, size[0] - 1), min(y + tile - 1, size[1] - 1)),
                    fill=alternate,
                )
    return image


def make_contact_sheet(left: Image.Image, right: Image.Image) -> Path:
    sheet = checkerboard((680, 450))
    draw = ImageDraw.Draw(sheet)
    draw.rectangle((12, 12, 667, 437), outline=(73, 64, 53, 180), width=2)
    draw.text((100, 24), "LEFT  <", fill=(*INK_RGB, 255))
    draw.text((430, 24), "RIGHT  >", fill=(*INK_RGB, 255))

    scale = 6
    enlarged_size = (TARGET_SIZE[0] * scale, TARGET_SIZE[1] * scale)
    for index, button in enumerate((left, right)):
        enlarged = button.resize(enlarged_size, Image.Resampling.NEAREST)
        x = 36 + index * 330
        y = 54
        sheet.alpha_composite(enlarged, (x, y))
        actual_x = x + enlarged_size[0] + 14
        actual_y = y + (enlarged_size[1] - TARGET_SIZE[1]) // 2
        sheet.alpha_composite(button, (actual_x, actual_y))
        draw.rectangle(
            (actual_x - 1, actual_y - 1, actual_x + TARGET_SIZE[0], actual_y + TARGET_SIZE[1]),
            outline=(73, 64, 53, 120),
            width=1,
        )

    path = OUTPUT_ROOT / "companion_page_buttons_contact_sheet.png"
    sheet.save(path, optimize=True)
    return path


def main() -> None:
    for required_path in (
        REFERENCE_PATH,
        SOURCE_CHROMA_PATH,
        SOURCE_CUTOUT_PATH,
        PROMPT_PATH,
    ):
        if not required_path.is_file():
            raise FileNotFoundError(required_path)
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)

    left = normalize_imagegen_cutout()
    right = left.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    left.save(LEFT_PATH, optimize=True)
    right.save(RIGHT_PATH, optimize=True)

    left_record = validate_button(LEFT_PATH, "left")
    right_record = validate_button(RIGHT_PATH, "right")
    saved_left = Image.open(LEFT_PATH).convert("RGBA")
    saved_right = Image.open(RIGHT_PATH).convert("RGBA")
    strict_mirror = (
        saved_left.transpose(Image.Transpose.FLIP_LEFT_RIGHT).tobytes()
        == saved_right.tobytes()
    )
    if not strict_mirror:
        raise RuntimeError("right button is not an exact horizontal mirror of left")

    contact_sheet = make_contact_sheet(saved_left, saved_right)
    manifest = {
        "status": "PASS",
        "generationMode": "built-in imagegen with chroma-key removal",
        "constructionVersion": CONSTRUCTION_VERSION,
        "targetSize": list(TARGET_SIZE),
        "maxInkSize": list(MAX_INK_SIZE),
        "geometryRule": "pure < / >; exactly two diagonal brush strokes; no horizontal stem",
        "reference": {
            "path": REFERENCE_PATH.relative_to(PROJECT_ROOT).as_posix(),
            "usage": "ink character only; circle and X geometry excluded",
            "sha256": sha256_file(REFERENCE_PATH),
        },
        "generatedSource": {
            "chromaPath": SOURCE_CHROMA_PATH.relative_to(PROJECT_ROOT).as_posix(),
            "chromaSha256": sha256_file(SOURCE_CHROMA_PATH),
            "cutoutPath": SOURCE_CUTOUT_PATH.relative_to(PROJECT_ROOT).as_posix(),
            "cutoutSha256": sha256_file(SOURCE_CUTOUT_PATH),
            "promptPath": PROMPT_PATH.relative_to(PROJECT_ROOT).as_posix(),
            "promptSha256": sha256_file(PROMPT_PATH),
        },
        "strictHorizontalMirror": strict_mirror,
        "records": [left_record, right_record],
        "contactSheet": {
            "path": contact_sheet.relative_to(PROJECT_ROOT).as_posix(),
            "sha256": sha256_file(contact_sheet),
            "width": Image.open(contact_sheet).width,
            "height": Image.open(contact_sheet).height,
        },
    }
    manifest_path = OUTPUT_ROOT / "companion_page_buttons_manifest.json"
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print("Prepared ImageGen companion page buttons at 36x62")
    print(OUTPUT_ROOT)


if __name__ == "__main__":
    main()
