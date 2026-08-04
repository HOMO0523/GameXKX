"""Assemble the high-fidelity GameXXK Hero/Backpack V2 calibration preview."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont, ImageOps


PROJECT_ROOT = Path(__file__).resolve().parents[1]
CANONICAL_ROOT = PROJECT_ROOT / "SourceArt" / "UI" / "PSD" / "gamexxk-v4" / "calibration-v2"
CANVAS = (1920, 1080)


def load_json(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, payload: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_locked_source(record: dict[str, object]) -> Path:
    source = PROJECT_ROOT / str(record["path"])
    if not source.is_file():
        raise FileNotFoundError(f"missing locked source: {source}")
    actual_hash = sha256(source)
    if actual_hash != record["sha256"]:
        raise ValueError(f"source hash drift for {source}: {actual_hash}")
    with Image.open(source) as image:
        if list(image.size) != record["dimensions"]:
            raise ValueError(f"source dimensions drift for {source}: {image.size}")
    return source


def load_font(size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    candidates = [
        Path("C:/Windows/Fonts/simkai.ttf"),
        Path("C:/Windows/Fonts/STKAITI.TTF"),
        Path("C:/Windows/Fonts/msyh.ttc"),
    ]
    for candidate in candidates:
        if candidate.is_file():
            return ImageFont.truetype(str(candidate), size=size)
    return ImageFont.load_default()


def contain_canvas(
    source: Image.Image,
    target_width: int,
    target_height: int,
) -> tuple[Image.Image, float]:
    scale = min(target_width / source.width, target_height / source.height)
    size = (round(source.width * scale), round(source.height * scale))
    return source.resize(size, Image.Resampling.LANCZOS), scale


def preview_text_specs() -> list[dict[str, object]]:
    ink = (44, 39, 31, 255)
    cinnabar = (146, 57, 35, 255)
    return [
        {"name": "hud_level", "text": "Lv. 1", "x": 202, "y": 53, "size": 34, "fill": ink},
        {"name": "hud_exp", "text": "0 / 100", "x": 312, "y": 93, "size": 19, "fill": ink},
        {"name": "hud_power", "text": "33", "x": 248, "y": 133, "size": 27, "fill": cinnabar},
        {"name": "resource_coin", "text": "10,000", "x": 1268, "y": 50, "size": 30, "fill": ink},
        {"name": "resource_jade", "text": "2,000", "x": 1510, "y": 50, "size": 30, "fill": ink},
        {"name": "resource_gold", "text": "500", "x": 1785, "y": 50, "size": 30, "fill": ink},
        {"name": "title", "text": "主角", "x": 388, "y": 214, "size": 48, "fill": ink},
        {"name": "tab_attribute", "text": "属性", "x": 577, "y": 220, "size": 27, "fill": ink},
        {"name": "tab_equipment", "text": "装备", "x": 696, "y": 218, "size": 29, "fill": ink},
        {"name": "tab_skill", "text": "技能", "x": 827, "y": 220, "size": 27, "fill": ink},
        {"name": "tab_talent", "text": "天赋", "x": 943, "y": 220, "size": 27, "fill": ink},
        {"name": "tab_title", "text": "称号", "x": 1062, "y": 220, "size": 27, "fill": ink},
        {"name": "category_all", "text": "全部", "x": 1215, "y": 221, "size": 27, "fill": (236, 222, 188, 255)},
        {"name": "category_consumable", "text": "消耗", "x": 1322, "y": 221, "size": 25, "fill": ink},
        {"name": "category_material", "text": "材料", "x": 1433, "y": 221, "size": 25, "fill": ink},
        {"name": "category_quest", "text": "任务", "x": 1545, "y": 221, "size": 25, "fill": ink},
        {"name": "category_other", "text": "其他", "x": 1655, "y": 221, "size": 25, "fill": ink},
        {"name": "hero_level", "text": "Lv. 1", "x": 470, "y": 892, "size": 25, "fill": ink},
        {"name": "hero_exp", "text": "0 / 100", "x": 886, "y": 892, "size": 20, "fill": ink},
        {"name": "stat_attack", "text": "33", "x": 550, "y": 940, "size": 24, "fill": ink},
        {"name": "stat_health", "text": "120", "x": 744, "y": 940, "size": 24, "fill": ink},
        {"name": "stat_defense", "text": "18", "x": 922, "y": 940, "size": 24, "fill": ink},
        {"name": "detail_name", "text": "小布袋", "x": 1320, "y": 826, "size": 28, "fill": ink},
        {"name": "detail_count", "text": "拥有：2", "x": 1586, "y": 829, "size": 22, "fill": ink},
        {"name": "detail_description", "text": "普通的布袋，能装下一些小物件。", "x": 1320, "y": 872, "size": 20, "fill": ink},
        {"name": "button_use", "text": "使用", "x": 1585, "y": 934, "size": 28, "fill": (239, 224, 196, 255)},
    ]


def add_preview_text(preview: Image.Image) -> None:
    draw = ImageDraw.Draw(preview)
    for record in preview_text_specs():
        draw.text(
            (int(record["x"]), int(record["y"])),
            str(record["text"]),
            font=load_font(int(record["size"])),
            fill=record["fill"],
        )


def make_comparison(reference: Image.Image, preview: Image.Image) -> Image.Image:
    canvas = Image.new("RGB", CANVAS, (37, 35, 30))
    left = ImageOps.contain(reference.convert("RGB"), (900, 506), Image.Resampling.LANCZOS)
    right = ImageOps.contain(preview.convert("RGB"), (900, 506), Image.Resampling.LANCZOS)
    canvas.paste(left, (40, 170))
    canvas.paste(right, (980, 170))
    draw = ImageDraw.Draw(canvas)
    draw.text((40, 88), "批准参考", font=load_font(34), fill=(236, 224, 196))
    draw.text((980, 88), "V2 校准稿", font=load_font(34), fill=(236, 224, 196))
    draw.text(
        (40, 735),
        "核对：城镇实景、宣纸纤维、撕边墨线、彩色图标、信息密度、Hero 比例与落脚位置",
        font=load_font(24),
        fill=(210, 198, 174),
    )
    return canvas


def build(output_root: Path) -> dict[str, object]:
    spec = load_json(CANONICAL_ROOT / "calibration-spec.json")
    source_lock = load_json(CANONICAL_ROOT / "source-lock.json")
    reference_path = validate_locked_source(source_lock["approvedReference"])
    hero_path = validate_locked_source(source_lock["heroIdle"])
    generated_base = CANONICAL_ROOT / str(spec["generatedBase"])
    if not generated_base.is_file():
        raise FileNotFoundError(f"missing generated V2 base: {generated_base}")

    with Image.open(generated_base) as image:
        preview = ImageOps.fit(image.convert("RGBA"), CANVAS, method=Image.Resampling.LANCZOS)

    placement = spec["heroPlacement"]
    with Image.open(hero_path) as image:
        hero_source = image.convert("RGBA")
        hero_source_canvas = list(hero_source.size)
        hero, hero_scale = contain_canvas(
            hero_source,
            int(placement["width"]),
            int(placement["height"]),
        )
    hero_x = int(placement["x"]) + (int(placement["width"]) - hero.width) // 2
    hero_y = int(placement["y"]) + (int(placement["height"]) - hero.height) // 2
    preview.alpha_composite(hero, (hero_x, hero_y))
    add_preview_text(preview)

    preview_path = output_root / str(spec["preview"])
    comparison_path = output_root / str(spec["comparison"])
    preview_path.parent.mkdir(parents=True, exist_ok=True)
    comparison_path.parent.mkdir(parents=True, exist_ok=True)
    preview.convert("RGB").save(preview_path, quality=96)

    with Image.open(reference_path) as image:
        reference = ImageOps.fit(image.convert("RGB"), CANVAS, method=Image.Resampling.LANCZOS)
    make_comparison(reference, preview).save(comparison_path, quality=94)

    consumed_sources = [
        reference_path.relative_to(PROJECT_ROOT).as_posix(),
        generated_base.relative_to(PROJECT_ROOT).as_posix(),
        hero_path.relative_to(PROJECT_ROOT).as_posix(),
    ]
    report = {
        "ok": True,
        "candidateName": spec["candidateName"],
        "canvas": list(CANVAS),
        "heroSourceCanvas": hero_source_canvas,
        "heroScale": hero_scale,
        "heroScaleRatioXToY": 1.0,
        "heroPlacement": {
            "x": hero_x,
            "y": hero_y,
            "width": hero.width,
            "height": hero.height,
        },
        "consumedSources": consumed_sources,
        "preview": str(preview_path.resolve()),
        "comparison": str(comparison_path.resolve()),
    }
    write_json(output_root / "package-build-report.json", report)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-root", type=Path, default=CANONICAL_ROOT)
    args = parser.parse_args()
    report = build(args.output_root.resolve())
    print(json.dumps(report, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
