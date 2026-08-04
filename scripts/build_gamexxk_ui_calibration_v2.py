"""Assemble the high-fidelity GameXXK Hero/Backpack V2 calibration preview."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import statistics
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont, ImageOps


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


def split_transparent_icon_sheet(
    source_path: Path,
    icon_names: list[str],
    output_dir: Path,
    *,
    canvas_size: tuple[int, int] = (512, 512),
    subject_fill: float = 0.90,
    keep_largest_component: bool = True,
) -> list[Path]:
    """Split an evenly spaced transparent icon sheet into normalized square assets."""
    if not icon_names:
        raise ValueError("icon_names must not be empty")
    if not 0.0 < subject_fill <= 1.0:
        raise ValueError("subject_fill must be in the range (0, 1]")

    output_dir.mkdir(parents=True, exist_ok=True)
    with Image.open(source_path) as opened:
        sheet = opened.convert("RGBA")

    target_extent = max(1, round(min(canvas_size) * subject_fill))
    output_paths: list[Path] = []
    for index, icon_name in enumerate(icon_names):
        cell_left = round(index * sheet.width / len(icon_names))
        cell_right = round((index + 1) * sheet.width / len(icon_names))
        cell = sheet.crop((cell_left, 0, cell_right, sheet.height))
        if keep_largest_component:
            cell = keep_largest_alpha_component(cell)
        bounds = cell.getchannel("A").getbbox()
        if bounds is None:
            raise ValueError(f"icon cell has no visible subject: {icon_name}")
        subject = cell.crop(bounds)
        normalized, _ = contain_canvas(subject, target_extent, target_extent)
        canvas = Image.new("RGBA", canvas_size, (0, 0, 0, 0))
        canvas.alpha_composite(
            normalized,
            (
                (canvas.width - normalized.width) // 2,
                (canvas.height - normalized.height) // 2,
            ),
        )
        output_path = output_dir / f"{icon_name}.png"
        canvas.save(output_path)
        output_paths.append(output_path)

    return output_paths


def resolve_chroma_key_helper() -> Path:
    configured_root = os.environ.get("CODEX_HOME")
    codex_root = Path(configured_root) if configured_root else Path.home() / ".codex"
    helper = codex_root / "skills" / ".system" / "imagegen" / "scripts" / "remove_chroma_key.py"
    if not helper.is_file():
        raise FileNotFoundError(f"missing imagegen chroma-key helper: {helper}")
    return helper


def remove_chroma_key_sheet(source_path: Path, output_path: Path) -> Path:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    command = [
        sys.executable,
        str(resolve_chroma_key_helper()),
        "--input",
        str(source_path),
        "--out",
        str(output_path),
        "--key-color",
        "#ff00ff",
        "--tolerance",
        "90",
        "--edge-contract",
        "1",
        "--force",
    ]
    result = subprocess.run(command, check=False, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(result.stderr or result.stdout)
    with Image.open(output_path) as opened:
        cleaned = opened.convert("RGBA")
    pixels = cleaned.load()
    for y in range(cleaned.height):
        for x in range(cleaned.width):
            red, green, blue, alpha = pixels[x, y]
            if alpha > 0 and red > 160 and blue > 160 and green < 100 and min(red, blue) - green > 70:
                pixels[x, y] = (0, 0, 0, 0)
    cleaned.save(output_path)
    if cleaned.getchannel("A").getbbox() is None:
        raise ValueError(f"chroma-key removal produced an empty sheet: {source_path}")
    return output_path


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
        {"name": "detail_name", "text": "青山讨伐令", "x": 1320, "y": 826, "size": 28, "fill": ink},
        {"name": "detail_count", "text": "拥有：1", "x": 1586, "y": 829, "size": 22, "fill": ink},
        {"name": "detail_description", "text": "青山镇讨伐任务的通行凭证。", "x": 1320, "y": 872, "size": 20, "fill": ink},
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


def copy_asset(source: Path, destination: Path) -> Path:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if source.resolve() != destination.resolve():
        shutil.copy2(source, destination)
    return destination


def make_component_sheet(
    components: list[tuple[dict[str, object], Image.Image]],
) -> Image.Image:
    sheet = Image.new("RGB", CANVAS, (49, 43, 35))
    draw = ImageDraw.Draw(sheet)
    draw.text(
        (64, 34),
        "GameXXK V2 独立控件拆层预览",
        font=load_font(38),
        fill=(239, 226, 198),
    )

    layout = {
        "tab": {"label": "页签框（5）", "y": 105, "columns": 5, "cell": (340, 130)},
        "equipment_slot": {"label": "装备槽（6）", "y": 270, "columns": 6, "cell": (280, 155)},
        "inventory_slot": {"label": "背包格（16）", "y": 465, "columns": 8, "cell": (215, 180)},
        "detail_slot": {"label": "详情物品槽（1）", "y": 875, "columns": 1, "cell": (300, 155)},
    }
    for role, record in layout.items():
        role_components = [entry for entry in components if entry[0]["role"] == role]
        y = int(record["y"])
        cell_width, cell_height = record["cell"]
        draw.text((64, y), str(record["label"]), font=load_font(25), fill=(211, 195, 164))
        for index, (crop, component) in enumerate(role_components):
            column = index % int(record["columns"])
            row = index // int(record["columns"])
            x = 64 + column * cell_width
            top = y + 40 + row * cell_height
            preview = ImageOps.contain(
                component.convert("RGBA"),
                (cell_width - 24, cell_height - 42),
                Image.Resampling.LANCZOS,
            )
            sheet.paste(preview, (x, top), preview)
            draw.text(
                (x, top + preview.height + 4),
                str(crop["name"]),
                font=load_font(15),
                fill=(186, 171, 145),
            )
    return sheet


def apply_component_alpha(component: Image.Image, role: str) -> Image.Image:
    radius_by_role = {
        "tab": 5,
        "equipment_slot": 8,
        "inventory_slot": 8,
        "detail_slot": 7,
    }
    inset = 2
    mask = Image.new("L", component.size, 0)
    draw = ImageDraw.Draw(mask)
    draw.rounded_rectangle(
        (inset, inset, component.width - inset - 1, component.height - inset - 1),
        radius=radius_by_role[role],
        fill=255,
    )
    mask = mask.filter(ImageFilter.GaussianBlur(0.35))
    separated = component.copy()
    separated.putalpha(mask)
    return separated


def component_trim_box(component: Image.Image, padding: int = 3) -> tuple[int, int, int, int]:
    ink = component.convert("L").point(lambda value: 255 if value < 145 else 0)
    bounds = ink.getbbox()
    if bounds is None:
        return (0, 0, component.width, component.height)
    left, top, right, bottom = bounds
    return (
        max(0, left - padding),
        max(0, top - padding),
        min(component.width, right + padding),
        min(component.height, bottom + padding),
    )


def tighten_component_crop(component: Image.Image, padding: int = 3) -> Image.Image:
    return component.crop(component_trim_box(component, padding))


def remove_paper_background(
    component: Image.Image,
) -> tuple[Image.Image, tuple[int, int]]:
    rgba = component.convert("RGBA")
    border: list[tuple[int, int, int]] = []
    for x in range(rgba.width):
        border.append(rgba.getpixel((x, 0))[:3])
        border.append(rgba.getpixel((x, rgba.height - 1))[:3])
    for y in range(1, rgba.height - 1):
        border.append(rgba.getpixel((0, y))[:3])
        border.append(rgba.getpixel((rgba.width - 1, y))[:3])
    background = tuple(
        int(statistics.median(pixel[channel] for pixel in border))
        for channel in range(3)
    )
    alpha_values: list[int] = []
    for red, green, blue, source_alpha in rgba.getdata():
        distance = (
            (red - background[0]) ** 2
            + (green - background[1]) ** 2
            + (blue - background[2]) ** 2
        ) ** 0.5
        alpha = max(0, min(255, round((distance - 16.0) * 5.4)))
        alpha_values.append(alpha * source_alpha // 255)
    alpha = Image.new("L", rgba.size)
    alpha.putdata(alpha_values)
    alpha = alpha.filter(ImageFilter.GaussianBlur(0.45))
    rgba.putalpha(alpha)

    subject_mask = alpha.point(lambda value: 255 if value > 36 else 0)
    bounds = subject_mask.getbbox()
    if bounds is None:
        return rgba, (0, 0)
    left, top, right, bottom = bounds
    subject = rgba.crop((left, top, right, bottom))
    padded = Image.new("RGBA", (subject.width + 6, subject.height + 6), (0, 0, 0, 0))
    padded.alpha_composite(subject, (3, 3))
    return padded, (left - 3, top - 3)


def make_hero_portrait(hero_path: Path) -> Image.Image:
    with Image.open(hero_path) as image:
        portrait = image.convert("RGBA").crop((166, 42, 361, 237))
    mask = Image.new("L", portrait.size, 0)
    ImageDraw.Draw(mask).ellipse((2, 2, portrait.width - 3, portrait.height - 3), fill=255)
    mask = ImageChops.multiply(portrait.getchannel("A"), mask)
    portrait.putalpha(mask)
    return portrait


def normalize_transparent_asset(
    image: Image.Image,
    output_size: tuple[int, int],
    subject_size: tuple[int, int],
) -> Image.Image:
    rgba = image.convert("RGBA")
    bounds = rgba.getchannel("A").getbbox()
    if bounds is None:
        raise ValueError("transparent asset has no visible subject")
    subject = rgba.crop(bounds)
    subject = ImageOps.contain(subject, subject_size, Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", output_size, (0, 0, 0, 0))
    canvas.alpha_composite(
        subject,
        (
            (output_size[0] - subject.width) // 2,
            (output_size[1] - subject.height) // 2,
        ),
    )
    return canvas


def keep_largest_alpha_component(
    image: Image.Image,
    threshold: int = 36,
) -> Image.Image:
    rgba = image.convert("RGBA")
    alpha = rgba.getchannel("A")
    visible = {
        (x, y)
        for y in range(rgba.height)
        for x in range(rgba.width)
        if alpha.getpixel((x, y)) > threshold
    }
    components: list[set[tuple[int, int]]] = []
    while visible:
        component = {visible.pop()}
        frontier = list(component)
        while frontier:
            x, y = frontier.pop()
            for neighbor in (
                (x - 1, y),
                (x + 1, y),
                (x, y - 1),
                (x, y + 1),
            ):
                if neighbor in visible:
                    visible.remove(neighbor)
                    component.add(neighbor)
                    frontier.append(neighbor)
        components.append(component)
    if not components:
        raise ValueError("content crop has no visible subject")

    largest = max(components, key=len)
    keep = Image.new("L", rgba.size, 0)
    keep_pixels = keep.load()
    for x, y in largest:
        keep_pixels[x, y] = 255
    keep = keep.filter(ImageFilter.MaxFilter(5))
    rgba.putalpha(ImageChops.multiply(alpha, keep))
    return rgba


def export_separated_components(
    spec: dict[str, object],
    ui_shell_path: Path,
    output_root: Path,
) -> tuple[list[Path], Path, Path]:
    component_root = output_root / str(spec["componentOutputDir"])
    component_root.mkdir(parents=True, exist_ok=True)
    components: list[tuple[dict[str, object], Image.Image]] = []
    component_paths: list[Path] = []
    layout_records: list[dict[str, object]] = []
    with Image.open(ui_shell_path) as image:
        shell = image.convert("RGBA")
        for crop in spec["componentCrops"]:
            source_box = tuple(int(value) for value in crop["box"])
            source_component = shell.crop(source_box)
            trim_box = component_trim_box(source_component)
            component = source_component.crop(trim_box)
            component = apply_component_alpha(component, str(crop["role"]))
            component_path = component_root / f"{crop['name']}.png"
            component.save(component_path)
            component_paths.append(component_path)
            components.append((crop, component))
            source_left = source_box[0] + trim_box[0]
            source_top = source_box[1] + trim_box[1]
            layout_records.append(
                {
                    "name": crop["name"],
                    "role": crop["role"],
                    "file": component_path.relative_to(output_root).as_posix(),
                    "sourceBox": list(source_box),
                    "trimBox": list(trim_box),
                    "sourcePlacement": [
                        source_left,
                        source_top,
                        source_left + component.width,
                        source_top + component.height,
                    ],
                    "size": [component.width, component.height],
                }
            )

    component_sheet_path = output_root / str(spec["componentSheet"])
    component_sheet_path.parent.mkdir(parents=True, exist_ok=True)
    make_component_sheet(components).save(component_sheet_path, quality=95)
    component_layout_path = output_root / str(spec["componentLayout"])
    write_json(
        component_layout_path,
        {"sourceCanvas": [1672, 941], "components": layout_records},
    )
    return component_paths, component_sheet_path, component_layout_path


def validate_approved_content_asset(path: Path) -> None:
    with Image.open(path) as opened:
        image = opened.convert("RGBA")
    if image.size != (512, 512):
        raise ValueError(f"approved content asset has wrong size: {path}: {image.size}")
    alpha = image.getchannel("A")
    if alpha.getbbox() is None:
        raise ValueError(f"approved content asset is empty: {path}")
    edge_alpha = list(alpha.crop((0, 0, image.width, 1)).getdata())
    edge_alpha += list(alpha.crop((0, image.height - 1, image.width, image.height)).getdata())
    edge_alpha += list(alpha.crop((0, 0, 1, image.height)).getdata())
    edge_alpha += list(alpha.crop((image.width - 1, 0, image.width, image.height)).getdata())
    if any(value > 0 for value in edge_alpha):
        raise ValueError(f"approved content asset touches canvas edge: {path}")
    magenta_spill = sum(
        1
        for red, green, blue, pixel_alpha in image.getdata()
        if pixel_alpha > 16 and red > 180 and blue > 180 and green < 100
    )
    if magenta_spill:
        raise ValueError(f"approved content asset retains magenta spill: {path}: {magenta_spill}")


def strip_visible_magenta_spill(path: Path) -> None:
    with Image.open(path) as opened:
        image = opened.convert("RGBA")
    pixels = image.load()
    changed = False
    for y in range(image.height):
        for x in range(image.width):
            red, green, blue, alpha = pixels[x, y]
            if alpha > 0 and red > 180 and blue > 180 and green < 100:
                pixels[x, y] = (0, 0, 0, 0)
                changed = True
    if changed:
        image.save(path)


def make_approved_content_review(
    records: list[dict[str, object]],
    output_path: Path,
) -> Path:
    canvas = Image.new("RGB", CANVAS, (48, 42, 34))
    draw = ImageDraw.Draw(canvas)
    draw.text((64, 24), "GameXXK UI V2 批准装备与道具（45）", font=load_font(34), fill=(239, 226, 198))

    set_records = [record for record in records if record["category"] == "set_equipment"]
    starter_records = [record for record in records if record["category"] == "starter_equipment"]
    item_records = [record for record in records if record["category"] == "core_item"]

    draw.text((64, 78), "六套装备 36 件", font=load_font(22), fill=(209, 193, 165))
    for index, record in enumerate(set_records):
        row, column = divmod(index, 6)
        x = 64 + column * 296
        y = 112 + row * 112
        with Image.open(record["absolutePath"]) as opened:
            icon = ImageOps.contain(opened.convert("RGBA"), (86, 86), Image.Resampling.LANCZOS)
        canvas.paste(icon, (x, y), icon)
        draw.text((x + 92, y + 29), str(record["name"]), font=load_font(14), fill=(220, 207, 181))

    draw.text((64, 800), "普通初始装备 6 件", font=load_font(22), fill=(209, 193, 165))
    for index, record in enumerate(starter_records):
        x = 64 + index * 296
        y = 834
        with Image.open(record["absolutePath"]) as opened:
            icon = ImageOps.contain(opened.convert("RGBA"), (86, 86), Image.Resampling.LANCZOS)
        canvas.paste(icon, (x, y), icon)
        draw.text((x + 92, y + 29), str(record["name"]), font=load_font(14), fill=(220, 207, 181))

    draw.text((64, 950), "核心道具 3 件", font=load_font(22), fill=(209, 193, 165))
    for index, record in enumerate(item_records):
        x = 64 + index * 500
        y = 978
        with Image.open(record["absolutePath"]) as opened:
            icon = ImageOps.contain(opened.convert("RGBA"), (76, 76), Image.Resampling.LANCZOS)
        canvas.paste(icon, (x, y), icon)
        draw.text((x + 84, y + 24), str(record["name"]), font=load_font(14), fill=(220, 207, 181))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(output_path, quality=95)
    return output_path


def export_approved_content(
    spec: dict[str, object],
    locked_paths: dict[str, Path],
    output_root: Path,
) -> tuple[list[Path], list[dict[str, object]], Path]:
    canvas_size = tuple(int(value) for value in spec["approvedContentCanvas"])
    if canvas_size != (512, 512):
        raise ValueError(f"approved content canvas must remain 512x512: {canvas_size}")
    subject_fill = float(spec["approvedContentSubjectFill"])
    set_order = [str(value) for value in spec["equipmentSetOrder"]]
    slot_order = [str(value) for value in spec["equipmentSlotOrder"]]
    core_item_order = [str(value) for value in spec["coreItemOrder"]]
    sheet_locks = {str(key): str(value) for key, value in spec["equipmentSheetLocks"].items()}

    derived_root = output_root / "Derived" / "ApprovedContentSheets"
    equipment_root = output_root / "Content" / "Equipment"
    starter_root = output_root / "Content" / "StarterEquipment"
    item_root = output_root / "Content" / "Items"
    records: list[dict[str, object]] = []
    paths: list[Path] = []

    def export_sheet(
        lock_name: str,
        names: list[str],
        category: str,
        destination: Path,
    ) -> None:
        alpha_sheet = remove_chroma_key_sheet(
            locked_paths[lock_name],
            derived_root / f"{lock_name}_alpha.png",
        )
        exported = split_transparent_icon_sheet(
            alpha_sheet,
            names,
            destination,
            canvas_size=canvas_size,
            subject_fill=subject_fill,
            keep_largest_component=True,
        )
        if len(exported) != len(names):
            raise ValueError(f"approved content split count mismatch for {lock_name}")
        for index, (name, path) in enumerate(zip(names, exported, strict=True)):
            strip_visible_magenta_spill(path)
            validate_approved_content_asset(path)
            records.append(
                {
                    "name": name,
                    "category": category,
                    "sourceLock": lock_name,
                    "cellIndex": index,
                    "file": path.relative_to(output_root).as_posix(),
                    "size": [path_image_size for path_image_size in canvas_size],
                    "absolutePath": str(path.resolve()),
                }
            )
            paths.append(path)

    for slot in slot_order:
        export_sheet(
            sheet_locks[slot],
            [f"{set_name}_{slot}" for set_name in set_order],
            "set_equipment",
            equipment_root,
        )
    export_sheet(
        str(spec["starterEquipmentLock"]),
        [f"starter_{slot}" for slot in slot_order],
        "starter_equipment",
        starter_root,
    )
    export_sheet(
        str(spec["coreItemsLock"]),
        core_item_order,
        "core_item",
        item_root,
    )

    names = [str(record["name"]) for record in records]
    if len(records) != 45 or len(names) != len(set(names)):
        raise ValueError(f"approved content must contain 45 unique assets: {len(records)}")
    review_path = make_approved_content_review(
        records,
        output_root / str(spec["approvedContentReview"]),
    )
    for record in records:
        record.pop("absolutePath", None)
    return paths, records, review_path


def export_content_layers(
    spec: dict[str, object],
    reference_path: Path,
    generated_base: Path,
    generated_nav_icons: Path,
    hero_path: Path,
    approved_records: list[dict[str, object]],
    output_root: Path,
) -> tuple[list[Path], Path]:
    content_root = output_root / str(spec["contentOutputDir"])
    content_root.mkdir(parents=True, exist_ok=True)
    source_paths = {
        "generatedBase": generated_base,
        "approvedReference": reference_path,
        "generatedNavIcons": generated_nav_icons,
    }
    source_images: dict[str, Image.Image] = {}
    records: list[dict[str, object]] = []
    content_paths: list[Path] = []
    try:
        for source_name, source_path in source_paths.items():
            with Image.open(source_path) as opened:
                source_images[source_name] = opened.convert("RGBA")
        for crop in spec["contentCrops"]:
            source_name = str(crop["source"])
            source_box = tuple(int(value) for value in crop["box"])
            source_crop = source_images[source_name].crop(source_box)
            if source_name == "generatedNavIcons":
                content = normalize_transparent_asset(
                    source_crop,
                    tuple(int(value) for value in crop["outputSize"]),
                    tuple(int(value) for value in crop["subjectSize"]),
                )
                trim_offset = (0, 0)
            else:
                content, trim_offset = remove_paper_background(source_crop)
                if bool(crop.get("keepLargestComponent", False)):
                    content = keep_largest_alpha_component(content)
                if "outputSize" in crop:
                    content = normalize_transparent_asset(
                        content,
                        tuple(int(value) for value in crop["outputSize"]),
                        tuple(int(value) for value in crop["subjectSize"]),
                    )
                    trim_offset = (0, 0)
            content_path = content_root / f"{crop['name']}.png"
            content.save(content_path)
            base_x, base_y = (int(value) for value in crop["placement"])
            placement_x = base_x + trim_offset[0]
            placement_y = base_y + trim_offset[1]
            display_width, display_height = (
                tuple(int(value) for value in crop["displaySize"])
                if "displaySize" in crop
                else content.size
            )
            record = {
                "name": crop["name"],
                "role": crop["role"],
                "file": content_path.relative_to(output_root).as_posix(),
                "source": source_name,
                "sourceBox": list(source_box),
                "sourcePlacement": [
                    placement_x,
                    placement_y,
                    placement_x + display_width,
                    placement_y + display_height,
                ],
                "size": [content.width, content.height],
                "displaySize": [display_width, display_height],
            }
            records.append(record)
            content_paths.append(content_path)
    finally:
        source_images.clear()

    portrait = make_hero_portrait(hero_path)
    portrait_path = content_root / "hero_portrait.png"
    portrait.save(portrait_path)
    content_paths.append(portrait_path)
    records.append(
        {
            "name": "hero_portrait",
            "role": "portrait",
            "file": portrait_path.relative_to(output_root).as_posix(),
            "source": "heroIdle",
            "sourceBox": [166, 42, 361, 237],
            "sourcePlacement": [27, 22, 151, 150],
            "size": [portrait.width, portrait.height],
        }
    )
    content_manifest_path = output_root / str(spec["contentManifest"])
    records.extend(approved_records)
    write_json(
        content_manifest_path,
        {
            "sourceCanvas": [1672, 941],
            "approvedCategoryCounts": {
                "set_equipment": sum(record.get("category") == "set_equipment" for record in approved_records),
                "starter_equipment": sum(record.get("category") == "starter_equipment" for record in approved_records),
                "core_item": sum(record.get("category") == "core_item" for record in approved_records),
            },
            "content": records,
        },
    )
    return content_paths, content_manifest_path


def paste_asset_contained(
    canvas: Image.Image,
    asset_path: Path,
    box: tuple[int, int, int, int],
    fill: float = 0.78,
) -> None:
    left, top, right, bottom = box
    target_width = max(1, round((right - left) * fill))
    target_height = max(1, round((bottom - top) * fill))
    with Image.open(asset_path) as opened:
        asset = ImageOps.contain(opened.convert("RGBA"), (target_width, target_height), Image.Resampling.LANCZOS)
    x = left + ((right - left) - asset.width) // 2
    y = top + ((bottom - top) - asset.height) // 2
    canvas.alpha_composite(asset, (x, y))


def compose_v2_source_canvas(
    large_panel_source: Path,
    component_layout_path: Path,
    content_manifest_path: Path,
    output_root: Path,
) -> Image.Image:
    with Image.open(large_panel_source) as opened:
        canvas = opened.convert("RGBA")
    layout = load_json(component_layout_path)
    manifest = load_json(content_manifest_path)

    component_by_name = {str(record["name"]): record for record in layout["components"]}
    for record in layout["components"]:
        placement = tuple(int(value) for value in record["sourcePlacement"])
        component_path = output_root / str(record["file"])
        with Image.open(component_path) as opened:
            component = opened.convert("RGBA")
        canvas.alpha_composite(component, (placement[0], placement[1]))

    content_by_name = {str(record["name"]): record for record in manifest["content"]}
    for record in manifest["content"]:
        if "sourcePlacement" not in record or "category" in record:
            continue
        placement = tuple(int(value) for value in record["sourcePlacement"])
        asset_path = output_root / str(record["file"])
        paste_asset_contained(canvas, asset_path, placement, fill=1.0)

    starter_assignments = [
        ("starter_weapon", "equipment_slot_left_01"),
        ("starter_head", "equipment_slot_left_02"),
        ("starter_armor", "equipment_slot_left_03"),
        ("starter_belt", "equipment_slot_right_01"),
        ("starter_shoes", "equipment_slot_right_02"),
        ("starter_accessory", "equipment_slot_right_03"),
    ]
    for asset_name, component_name in starter_assignments:
        asset_record = content_by_name[asset_name]
        component_record = component_by_name[component_name]
        box = tuple(int(value) for value in component_record["sourcePlacement"])
        paste_asset_contained(canvas, output_root / str(asset_record["file"]), box, fill=0.70)

    item_assignments = [
        ("strengthening_stone", "inventory_slot_r1_c1"),
        ("refinement_sand", "inventory_slot_r1_c2"),
        ("qingshan_suppression_token", "inventory_slot_r1_c3"),
    ]
    for asset_name, component_name in item_assignments:
        asset_record = content_by_name[asset_name]
        component_record = component_by_name[component_name]
        box = tuple(int(value) for value in component_record["sourcePlacement"])
        paste_asset_contained(canvas, output_root / str(asset_record["file"]), box, fill=0.68)

    detail_record = component_by_name["detail_item_slot"]
    detail_box = tuple(int(value) for value in detail_record["sourcePlacement"])
    token_record = content_by_name["qingshan_suppression_token"]
    paste_asset_contained(canvas, output_root / str(token_record["file"]), detail_box, fill=0.64)
    return canvas


def build(output_root: Path) -> dict[str, object]:
    spec = load_json(CANONICAL_ROOT / "calibration-spec.json")
    source_lock = load_json(CANONICAL_ROOT / "source-lock.json")
    locked_paths = {
        str(name): validate_locked_source(record)
        for name, record in source_lock.items()
    }
    reference_path = locked_paths["approvedReference"]
    hero_path = locked_paths["heroIdle"]
    generated_nav_icons = locked_paths["generatedNavIcons"]
    generated_base = CANONICAL_ROOT / str(spec["generatedBase"])
    if not generated_base.is_file():
        raise FileNotFoundError(f"missing generated V2 base: {generated_base}")
    ui_shell_path = CANONICAL_ROOT / str(spec["uiShellNoIcons"])
    if not ui_shell_path.is_file():
        raise FileNotFoundError(f"missing V2 UI shell: {ui_shell_path}")
    large_panel_source = CANONICAL_ROOT / str(spec["largePanelClean"])
    if not large_panel_source.is_file():
        raise FileNotFoundError(f"missing clean large-panel base: {large_panel_source}")

    large_panel_path = copy_asset(
        large_panel_source,
        output_root / str(spec["largePanelClean"]),
    )
    component_paths, component_sheet_path, component_layout_path = export_separated_components(
        spec,
        ui_shell_path,
        output_root,
    )
    approved_paths, approved_records, approved_review_path = export_approved_content(
        spec,
        locked_paths,
        output_root,
    )
    content_paths, content_manifest_path = export_content_layers(
        spec,
        reference_path,
        generated_base,
        generated_nav_icons,
        hero_path,
        approved_records,
        output_root,
    )

    source_preview = compose_v2_source_canvas(
        large_panel_source,
        component_layout_path,
        content_manifest_path,
        output_root,
    )
    preview = ImageOps.fit(source_preview, CANVAS, method=Image.Resampling.LANCZOS)

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
        ui_shell_path.relative_to(PROJECT_ROOT).as_posix(),
        large_panel_source.relative_to(PROJECT_ROOT).as_posix(),
        generated_nav_icons.relative_to(PROJECT_ROOT).as_posix(),
        hero_path.relative_to(PROJECT_ROOT).as_posix(),
    ]
    consumed_sources.extend(
        locked_paths[str(lock_name)].relative_to(PROJECT_ROOT).as_posix()
        for lock_name in [
            *spec["equipmentSheetLocks"].values(),
            spec["starterEquipmentLock"],
            spec["coreItemsLock"],
        ]
    )
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
        "largePanelClean": str(large_panel_path.resolve()),
        "componentAssets": [str(path.resolve()) for path in component_paths],
        "componentSheet": str(component_sheet_path.resolve()),
        "componentLayout": str(component_layout_path.resolve()),
        "contentAssets": [str(path.resolve()) for path in [*content_paths, *approved_paths]],
        "contentManifest": str(content_manifest_path.resolve()),
        "approvedContentAssets": [str(path.resolve()) for path in approved_paths],
        "approvedContentCount": len(approved_paths),
        "approvedContentReview": str(approved_review_path.resolve()),
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
