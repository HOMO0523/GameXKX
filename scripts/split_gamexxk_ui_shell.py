"""Split the approved GameXXK shop shell into reusable transparent layers."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageOps


SOURCE_CANVAS = (1672, 941)
TARGET_CANVAS = (1920, 1080)
COMPONENTS = (
    {"name": "01_主角身份条", "file": "identity_panel.png", "box": (0, 0, 500, 180), "seed": (100, 90)},
    {"name": "02_顶部铜钱条", "file": "currency_panel.png", "box": (975, 0, 1672, 130), "seed": (1320, 60)},
    {"name": "03_导航圆底_背包", "file": "nav_disc_backpack.png", "box": (10, 170, 180, 330), "seed": (85, 245)},
    {"name": "04_导航圆底_伙伴", "file": "nav_disc_companion.png", "box": (10, 300, 180, 455), "seed": (85, 375)},
    {"name": "05_导航圆底_图鉴", "file": "nav_disc_codex.png", "box": (10, 425, 180, 585), "seed": (85, 505)},
    {"name": "06_导航圆底_任务", "file": "nav_disc_task.png", "box": (10, 550, 180, 715), "seed": (85, 635)},
    {"name": "07_导航圆底_路线", "file": "nav_disc_route.png", "box": (10, 680, 180, 845), "seed": (85, 765)},
    {"name": "08_中央商店窗口", "file": "main_shop_panel.png", "box": (260, 140, 1545, 900), "seed": (800, 500)},
)


def write_json(path: Path, payload: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def load_rgb(path: Path) -> np.ndarray:
    if not path.is_file():
        raise FileNotFoundError(path)
    with Image.open(path) as opened:
        if opened.size != SOURCE_CANVAS:
            raise ValueError(f"expected {SOURCE_CANVAS} source, got {opened.size}: {path}")
        return np.asarray(opened.convert("RGB"), dtype=np.uint8)


def fit_background(
    shell: np.ndarray,
    clean: np.ndarray,
) -> tuple[np.ndarray, list[dict[str, float]], dict[str, float]]:
    safe = np.ones(shell.shape[:2], dtype=bool)
    for record in COMPONENTS:
        left, top, right, bottom = record["box"]
        safe[top:bottom, left:right] = False
    brightness = clean.astype(np.float32).mean(axis=2)
    safe &= (brightness > 30.0) & (brightness < 220.0)
    indices = np.flatnonzero(safe.ravel())[::20]
    if indices.size < 1_000:
        raise ValueError("not enough safe background samples")

    source = clean.reshape(-1, 3).astype(np.float32)[indices]
    target = shell.reshape(-1, 3).astype(np.float32)[indices]
    matched = np.empty_like(clean, dtype=np.float32)
    parameters: list[dict[str, float]] = []
    for channel in range(3):
        slope, intercept = np.polyfit(source[:, channel], target[:, channel], 1)
        matched[:, :, channel] = clean[:, :, channel] * slope + intercept
        parameters.append({"slope": float(slope), "intercept": float(intercept)})
    matched_u8 = np.clip(matched, 0, 255).astype(np.uint8)

    error = np.abs(matched_u8.astype(np.int16) - shell.astype(np.int16))[safe]
    metrics = {
        "meanAbsoluteError": float(error.mean()),
        "p95AbsoluteError": float(np.percentile(error, 95)),
        "p99AbsoluteError": float(np.percentile(error, 99)),
    }
    if metrics["meanAbsoluteError"] > 9.0 or metrics["p95AbsoluteError"] > 20.0:
        raise ValueError(f"background match exceeds limits: {metrics}")
    return matched_u8, parameters, metrics


def nearest_visible(mask: np.ndarray, seed: tuple[int, int]) -> tuple[int, int]:
    seed_x, seed_y = seed
    if 0 <= seed_x < mask.shape[1] and 0 <= seed_y < mask.shape[0] and mask[seed_y, seed_x]:
        return seed
    ys, xs = np.nonzero(mask)
    if xs.size == 0:
        raise ValueError("paper core is empty")
    distances = (xs - seed_x) ** 2 + (ys - seed_y) ** 2
    index = int(np.argmin(distances))
    if distances[index] > 60 * 60:
        raise ValueError(f"paper seed is too far from visible core: {seed}")
    return int(xs[index]), int(ys[index])


def connected_paper_core(
    shell_crop: np.ndarray,
    local_seed: tuple[int, int],
) -> Image.Image:
    red = shell_crop[:, :, 0].astype(np.int16)
    green = shell_crop[:, :, 1].astype(np.int16)
    blue = shell_crop[:, :, 2].astype(np.int16)
    raw = (red > 135) & (green > 110) & (blue > 85) & ((red - blue) > 18)
    closed = Image.fromarray(raw.astype(np.uint8) * 255)
    closed = closed.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.MinFilter(5))
    closed_array = np.asarray(closed) > 0
    flood_seed = nearest_visible(closed_array, local_seed)
    flood = closed.copy()
    ImageDraw.floodfill(flood, flood_seed, 128, thresh=0)
    component = np.asarray(flood) == 128
    if int(component.sum()) < 1_000:
        raise ValueError(f"paper component is unexpectedly small: {int(component.sum())}")
    return Image.fromarray(component.astype(np.uint8) * 255)


def uncomposite_foreground(
    composite: np.ndarray,
    background: np.ndarray,
    alpha: np.ndarray,
) -> np.ndarray:
    alpha_float = alpha.astype(np.float32)[:, :, None] / 255.0
    composite_float = composite.astype(np.float32)
    background_float = background.astype(np.float32)
    denominator = np.maximum(alpha_float, 1.0 / 255.0)
    foreground = (composite_float - background_float * (1.0 - alpha_float)) / denominator
    foreground[alpha_float.repeat(3, axis=2) <= 0.0] = 0.0
    return np.clip(foreground, 0, 255).astype(np.uint8)


def component_layer(
    shell: np.ndarray,
    background: np.ndarray,
    record: dict[str, object],
) -> tuple[Image.Image, dict[str, object]]:
    left, top, right, bottom = (int(value) for value in record["box"])
    seed_x, seed_y = (int(value) for value in record["seed"])
    shell_crop = shell[top:bottom, left:right]
    background_crop = background[top:bottom, left:right]
    local_seed = (seed_x - left, seed_y - top)

    core = connected_paper_core(shell_crop, local_seed)
    support = core.filter(ImageFilter.MaxFilter(19))
    core_array = np.asarray(core, dtype=np.uint8) > 0
    support_array = np.asarray(support, dtype=np.uint8) > 0

    # Background-fit colour residuals are not component pixels. Treating their
    # absolute RGB difference as alpha makes uncompositing amplify them into
    # cyan/magenta fringes. Outside the opaque paper core, retain only genuine
    # darkening and reconstruct it as a neutral ink-coloured shadow.
    luminance_weights = np.array([0.2126, 0.7152, 0.0722], dtype=np.float32)
    shell_luminance = shell_crop.astype(np.float32) @ luminance_weights
    background_luminance = background_crop.astype(np.float32) @ luminance_weights
    shadow_luminance = 18.0
    shadow_alpha = np.clip(
        (background_luminance - shell_luminance - 2.0)
        / np.maximum(background_luminance - shadow_luminance, 1.0)
        * 255.0,
        0,
        255,
    ).astype(np.uint8)
    shadow_alpha[~support_array | core_array] = 0
    shadow_alpha[shadow_alpha < 5] = 0
    alpha = np.where(core_array, 255, shadow_alpha).astype(np.uint8)
    edge_values = np.concatenate((alpha[0], alpha[-1], alpha[:, 0], alpha[:, -1]))
    if np.any(edge_values > 2):
        raise ValueError(f"component alpha touches source crop: {record['name']}")

    foreground = np.empty_like(shell_crop)
    foreground[core_array] = shell_crop[core_array]
    foreground[~core_array] = np.array([22, 18, 14], dtype=np.uint8)
    source_crop = Image.fromarray(np.dstack((foreground, alpha)))
    source_canvas = Image.new("RGBA", SOURCE_CANVAS, (0, 0, 0, 0))
    source_canvas.alpha_composite(source_crop, (left, top))
    target_canvas = source_canvas.resize(TARGET_CANVAS, Image.Resampling.LANCZOS)
    target_alpha = target_canvas.getchannel("A")
    target_bounds = target_alpha.point(lambda value: 255 if value > 2 else 0).getbbox()
    if target_bounds is None:
        raise ValueError(f"component became empty: {record['name']}")
    trim_left = max(0, target_bounds[0] - 2)
    trim_top = max(0, target_bounds[1] - 2)
    trim_right = min(TARGET_CANVAS[0], target_bounds[2] + 2)
    trim_bottom = min(TARGET_CANVAS[1], target_bounds[3] + 2)
    trimmed = target_canvas.crop((trim_left, trim_top, trim_right, trim_bottom))
    manifest_record = {
        "name": record["name"],
        "file": record["file"],
        "sourceBox": [left, top, right, bottom],
        "seed": [seed_x, seed_y],
        "box": [trim_left, trim_top, trimmed.width, trimmed.height],
        "alphaBounds": list(target_bounds),
        "opaquePixelCount": int(np.count_nonzero(np.asarray(trimmed.getchannel("A")) > 250)),
        "visiblePixelCount": int(np.count_nonzero(np.asarray(trimmed.getchannel("A")) > 2)),
    }
    return trimmed, manifest_record


def place_trimmed(canvas: Image.Image, image: Image.Image, box: list[int]) -> None:
    canvas.alpha_composite(image, (int(box[0]), int(box[1])))


def make_difference(
    original: Image.Image,
    recomposed: Image.Image,
    focus_mask: Image.Image,
) -> Image.Image:
    source = np.asarray(original.convert("RGB"), dtype=np.int16)
    rebuilt = np.asarray(recomposed.convert("RGB"), dtype=np.int16)
    difference = np.max(np.abs(source - rebuilt), axis=2).astype(np.uint8)
    focus = np.asarray(focus_mask.filter(ImageFilter.MaxFilter(17)), dtype=np.uint8) > 0
    difference[~focus] = 0
    heat = np.zeros((TARGET_CANVAS[1], TARGET_CANVAS[0], 3), dtype=np.uint8)
    heat[:, :, 0] = np.clip(difference.astype(np.int16) * 6, 0, 255).astype(np.uint8)
    heat[:, :, 1] = np.clip(difference.astype(np.int16) * 2, 0, 140).astype(np.uint8)
    return Image.fromarray(heat)


def build_shell_components(
    shell_path: Path,
    background_path: Path,
    output_root: Path,
    review_root: Path,
) -> dict[str, object]:
    shell = load_rgb(shell_path)
    clean = load_rgb(background_path)
    matched, parameters, metrics = fit_background(shell, clean)
    output_root.mkdir(parents=True, exist_ok=True)
    review_root.mkdir(parents=True, exist_ok=True)

    background_target = Image.fromarray(matched).resize(TARGET_CANVAS, Image.Resampling.LANCZOS)
    background_filename = "town_background_dimmed.png"
    background_target.save(output_root / background_filename)
    recomposed = background_target.convert("RGBA")
    difference_focus = Image.new("L", TARGET_CANVAS, 0)

    component_records: list[dict[str, object]] = []
    for record in COMPONENTS:
        layer, manifest_record = component_layer(shell, matched, record)
        layer.save(output_root / str(record["file"]))
        place_trimmed(recomposed, layer, manifest_record["box"])
        box_left, box_top, _, _ = manifest_record["box"]
        difference_focus.paste(layer.getchannel("A"), (box_left, box_top))
        component_records.append(manifest_record)

    original_target = Image.fromarray(shell).resize(TARGET_CANVAS, Image.Resampling.LANCZOS)
    recomposed_path = review_root / "07_商店交易_壳体拆分复合.png"
    difference_path = review_root / "07_商店交易_壳体拆分差异.png"
    recomposed.convert("RGB").save(recomposed_path)
    make_difference(original_target, recomposed, difference_focus).save(difference_path)

    manifest = {
        "version": 1,
        "sourceCanvas": list(SOURCE_CANVAS),
        "targetCanvas": list(TARGET_CANVAS),
        "sourceShell": str(shell_path.resolve()),
        "sourceBackground": str(background_path.resolve()),
        "backgroundFit": {"parameters": parameters, **metrics},
        "background": {
            "name": "00_城镇背景",
            "file": background_filename,
            "box": [0, 0, TARGET_CANVAS[0], TARGET_CANVAS[1]],
        },
        "components": component_records,
        "review": {
            "recomposed": str(recomposed_path.resolve()),
            "difference": str(difference_path.resolve()),
        },
    }
    write_json(output_root / "shell-components-manifest.json", manifest)
    return {
        "ok": True,
        "componentCount": len(component_records),
        "backgroundSize": list(background_target.size),
        "backgroundMetrics": metrics,
        "manifest": str((output_root / "shell-components-manifest.json").resolve()),
        "recomposedReview": str(recomposed_path.resolve()),
        "differenceReview": str(difference_path.resolve()),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--shell", type=Path, required=True)
    parser.add_argument("--background", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--review-root", type=Path, required=True)
    args = parser.parse_args()
    report = build_shell_components(
        args.shell,
        args.background,
        args.output_root,
        args.review_root,
    )
    print(json.dumps(report, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
