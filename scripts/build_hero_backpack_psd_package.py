"""Build the first GameXXK v3 Hero/Backpack layered PSD source package."""

from __future__ import annotations

import argparse
import json
import random
import shutil
from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageFont, ImageOps


PROJECT_ROOT = Path(__file__).resolve().parents[1]
CANONICAL_ROOT = PROJECT_ROOT / "SourceArt" / "UI" / "PSD" / "gamexxk-v3" / "hero-backpack"
HERO_SOURCE = (
    PROJECT_ROOT
    / "SourceAssets"
    / "AnimationProcessing"
    / "Production"
    / "character_00_hero_idle"
    / "frames"
    / "frame_0000.png"
)
OLD_ASSET_ROOT = PROJECT_ROOT / "SourceArt" / "UI" / "PSD" / "town-v2" / "clean_assets"
CANVAS = (1920, 1080)
INK = (42, 40, 34, 255)
PAPER = (238, 228, 205, 248)


def load_json(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_json(path: Path, payload: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def paper_texture(size: tuple[int, int], *, alpha: int = 248) -> Image.Image:
    noise = Image.effect_noise(size, 18).convert("L")
    low = Image.new("RGB", size, (224, 210, 183))
    high = Image.new("RGB", size, (248, 241, 224))
    texture = Image.composite(high, low, noise).convert("RGBA")
    texture.putalpha(alpha)
    return texture


def rounded_paper(
    size: tuple[int, int],
    *,
    radius: int = 28,
    outline: tuple[int, int, int, int] = INK,
    outline_width: int = 4,
) -> Image.Image:
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((2, 2, size[0] - 3, size[1] - 3), radius, fill=255)
    image.alpha_composite(Image.composite(paper_texture(size), Image.new("RGBA", size), mask))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle(
        (3, 3, size[0] - 4, size[1] - 4),
        radius,
        outline=outline,
        width=outline_width,
    )
    draw.rounded_rectangle(
        (9, 8, size[0] - 10, size[1] - 9),
        max(8, radius - 6),
        outline=(74, 67, 54, 125),
        width=2,
    )
    return image


def make_button(size: tuple[int, int], fill: tuple[int, int, int, int]) -> Image.Image:
    image = rounded_paper(size, radius=12, outline_width=3)
    tint = Image.new("RGBA", size, fill)
    image = Image.alpha_composite(image, tint)
    draw = ImageDraw.Draw(image)
    draw.line((12, size[1] - 8, size[0] - 12, size[1] - 8), fill=(36, 33, 28, 130), width=2)
    return image


def save_asset(root: Path, name: str, image: Image.Image) -> str:
    destination = root / "Assets" / name
    destination.parent.mkdir(parents=True, exist_ok=True)
    image.convert("RGBA").save(destination)
    return destination.relative_to(root).as_posix()


def copy_asset(root: Path, name: str, source: Path) -> str:
    destination = root / "Assets" / name
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    return destination.relative_to(root).as_posix()


def fit_world_context(source: Path) -> Image.Image:
    with Image.open(source) as image:
        rgba = image.convert("RGBA")
        if rgba.width == 1288 and rgba.height >= 760:
            rgba = rgba.crop((3, 43, 1285, 766))
        fitted = ImageOps.fit(rgba, CANVAS, method=Image.Resampling.LANCZOS)
    return ImageEnhance.Color(fitted).enhance(0.84)


def preview_font(size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    candidates = [
        PROJECT_ROOT / "SourceArt" / "UI" / "PSD" / "gamexxk-v3" / "Fonts" / "NotoSansSC" / "NotoSansSC-Variable.ttf",
        Path("C:/Windows/Fonts/msyh.ttc"),
        Path("C:/Windows/Fonts/simkai.ttf"),
    ]
    for candidate in candidates:
        if candidate.is_file():
            return ImageFont.truetype(str(candidate), size=size)
    return ImageFont.load_default()


def build_package(root: Path) -> dict[str, object]:
    screen_spec = load_json(CANONICAL_ROOT / "screen-spec.json")
    source_lock = load_json(CANONICAL_ROOT / "source-lock.json")
    reference_source = CANONICAL_ROOT / str(screen_spec["reference"])
    world_source = CANONICAL_ROOT / "Reference" / "town_world_context.png"
    if not reference_source.is_file():
        raise FileNotFoundError(f"missing approved reference: {reference_source}")
    if not world_source.is_file():
        raise FileNotFoundError(f"missing town world context: {world_source}")
    if not HERO_SOURCE.is_file():
        raise FileNotFoundError(f"missing final runtime idle frame: {HERO_SOURCE}")

    for directory in ("Assets", "Previews", "Reference", "RuntimeAssets"):
        (root / directory).mkdir(parents=True, exist_ok=True)
    if root.resolve() != CANONICAL_ROOT.resolve():
        shutil.copy2(CANONICAL_ROOT / "screen-spec.json", root / "screen-spec.json")
        shutil.copy2(CANONICAL_ROOT / "source-lock.json", root / "source-lock.json")
        shutil.copy2(reference_source, root / str(screen_spec["reference"]))
        shutil.copy2(world_source, root / "Reference" / "town_world_context.png")

    assets: dict[str, str] = {}
    assets["world"] = save_asset(root, "world_context.png", fit_world_context(world_source))
    assets["scrim"] = save_asset(root, "screen_scrim.png", Image.new("RGBA", CANVAS, (0, 0, 0, 72)))
    assets["panel"] = save_asset(root, "paper_panel.png", rounded_paper((1560, 790), radius=30, outline_width=5))
    assets["divider"] = save_asset(root, "ink_divider.png", Image.new("RGBA", (5, 650), (46, 42, 35, 205)))
    underline = Image.new("RGBA", (180, 12), (0, 0, 0, 0))
    underline_draw = ImageDraw.Draw(underline)
    underline_draw.line((4, 5, 176, 6), fill=(47, 43, 36, 235), width=5)
    underline_draw.line((22, 9, 146, 9), fill=(112, 55, 36, 170), width=2)
    assets["underline"] = save_asset(root, "title_underline.png", underline)
    assets["tab_normal"] = save_asset(root, "tab_normal.png", make_button((140, 52), (214, 198, 164, 35)))
    assets["tab_selected"] = save_asset(root, "tab_selected.png", make_button((140, 52), (151, 73, 48, 82)))
    assets["close"] = save_asset(root, "close_button.png", make_button((54, 54), (198, 186, 156, 30)))
    assets["equipment_slot"] = copy_asset(root, "equipment_slot.png", OLD_ASSET_ROOT / "037.png")
    assets["item_slot"] = copy_asset(root, "item_slot.png", OLD_ASSET_ROOT / "101.png")
    for index, old_index in enumerate(range(43, 49), start=1):
        assets[f"equipment_{index}"] = copy_asset(root, f"equipment_{index}.png", OLD_ASSET_ROOT / f"{old_index:03}.png")
    for index, old_index in enumerate(range(116, 121), start=1):
        assets[f"item_{index}"] = copy_asset(root, f"item_{index}.png", OLD_ASSET_ROOT / f"{old_index:03}.png")
    assets["hero"] = copy_asset(root, "hero_runtime_idle_frame_0000.png", HERO_SOURCE)
    shadow = Image.new("RGBA", (500, 90), (0, 0, 0, 0))
    shadow_draw = ImageDraw.Draw(shadow)
    shadow_draw.ellipse((25, 30, 475, 76), fill=(58, 55, 50, 50))
    for offset in range(0, 26, 5):
        shadow_draw.arc((25 + offset, 22, 475 - offset, 82), 8, 176, fill=(70, 67, 60, 52), width=3)
    assets["hero_shadow"] = save_asset(root, "hero_ink_shadow.png", shadow)
    assets["progress_track"] = save_asset(root, "progress_track.png", rounded_paper((360, 18), radius=8, outline_width=2))
    assets["progress_fill"] = save_asset(root, "progress_fill.png", Image.new("RGBA", (238, 8), (96, 136, 104, 255)))
    assets["detail_panel"] = save_asset(root, "item_detail_panel.png", rounded_paper((620, 120), radius=18, outline_width=3))
    assets["button_neutral"] = save_asset(root, "button_neutral.png", make_button((150, 54), (79, 129, 118, 86)))
    assets["button_primary"] = save_asset(root, "button_primary.png", make_button((150, 54), (161, 82, 48, 105)))
    assets["button_destructive"] = save_asset(root, "button_destructive.png", make_button((150, 54), (117, 63, 54, 115)))

    image_layers: list[dict[str, object]] = []

    def layer(
        name: str,
        asset: str,
        x: int,
        y: int,
        width: int,
        height: int,
        group: str,
        *,
        visible: bool = True,
    ) -> None:
        image_layers.append(
            {
                "name": name,
                "path": asset,
                "x": x,
                "y": y,
                "width": width,
                "height": height,
                "group": group,
                "visible": visible,
            }
        )

    reference_path = (root / str(screen_spec["reference"])).relative_to(root).as_posix()
    layer("approved_reference_hidden", reference_path, 0, 0, 1920, 1080, "00_Reference", visible=False)
    layer("world_context", assets["world"], 0, 0, 1920, 1080, "10_WorldContext")
    layer("screen_scrim", assets["scrim"], 0, 0, 1920, 1080, "10_WorldContext")
    layer("hero_backpack_panel", assets["panel"], 290, 150, 1560, 790, "20_Shell")
    layer("title_underline", assets["underline"], 338, 242, 180, 12, "20_Shell")
    layer("content_divider", assets["divider"], 1110, 258, 5, 620, "20_Shell")
    layer("close_button", assets["close"], 1760, 178, 54, 54, "20_Shell")
    layer("hero_ink_shadow", assets["hero_shadow"], 458, 704, 500, 90, "30_Hero")
    layer("hero_runtime_idle_frame", assets["hero"], 455, 276, 490, 490, "30_Hero")
    layer("hero_progress_track", assets["progress_track"], 520, 782, 360, 18, "30_Hero")
    layer("hero_progress_fill", assets["progress_fill"], 525, 787, 238, 8, "30_Hero")

    equipment_positions = [(342, 318), (342, 480), (342, 642), (962, 318), (962, 480), (962, 642)]
    for index, (x, y) in enumerate(equipment_positions, start=1):
        layer(f"equipment_slot_{index:02}", assets["equipment_slot"], x, y, 112, 118, "40_Equipment")
        layer(f"equipment_icon_{index:02}", assets[f"equipment_{index}"], x + 17, y + 18, 78, 82, "40_Equipment")

    category_x = [1148, 1273, 1398, 1523, 1648]
    for index, x in enumerate(category_x):
        layer(
            f"inventory_category_tab_{index + 1:02}",
            assets["tab_selected"] if index == 0 else assets["tab_normal"],
            x,
            246,
            116,
            43,
            "50_Inventory",
        )
    slot_positions: list[tuple[int, int]] = []
    for row in range(4):
        for column in range(5):
            slot_positions.append((1155 + column * 112, 314 + row * 102))
    for index, (x, y) in enumerate(slot_positions, start=1):
        layer(f"inventory_slot_{index:02}", assets["item_slot"], x, y, 92, 92, "50_Inventory")
    for index, (x, y) in enumerate(slot_positions[:5], start=1):
        layer(f"inventory_item_{index:02}", assets[f"item_{index}"], x + 15, y + 14, 62, 64, "50_Inventory")

    layer("item_detail_panel", assets["detail_panel"], 1148, 735, 620, 120, "60_Detail")
    layer("button_sort", assets["button_neutral"], 1270, 868, 150, 54, "60_Detail")
    layer("button_use", assets["button_primary"], 1605, 770, 150, 54, "60_Detail")
    layer("button_disassemble", assets["button_destructive"], 1450, 868, 150, 54, "60_Detail")

    text_specs = [
        ("title", "主角", 340, 178, 44, True),
        ("tab_attribute", "属性", 514, 184, 25, False),
        ("tab_equipment", "装备", 645, 184, 25, True),
        ("tab_skill", "技能", 776, 184, 25, False),
        ("tab_talent", "天赋", 907, 184, 25, False),
        ("tab_title", "称号", 1038, 184, 25, False),
        ("hero_name", "小侠客", 620, 250, 29, True),
        ("hero_level", "Lv. 1", 470, 818, 24, True),
        ("hero_exp", "0 / 100", 785, 818, 19, False),
        ("hero_attack", "攻击 33", 484, 860, 22, True),
        ("hero_health", "气血 120", 650, 860, 22, True),
        ("hero_defense", "防御 18", 830, 860, 22, True),
        ("category_all", "全部", 1206, 253, 20, True),
        ("category_consumable", "消耗", 1330, 253, 20, False),
        ("category_material", "材料", 1455, 253, 20, False),
        ("category_quest", "任务", 1580, 253, 20, False),
        ("category_other", "其他", 1705, 253, 20, False),
        ("inventory_capacity", "背包  5 / 80", 1150, 188, 26, True),
        ("resource_coin", "铜钱 50", 1530, 192, 22, False),
        ("detail_name", "小布袋", 1248, 760, 27, True),
        ("detail_count", "拥有：2", 1480, 764, 19, False),
        ("detail_description", "普通的布袋，能装下一些小物件。", 1248, 806, 19, False),
        ("button_sort_text", "整理", 1320, 879, 21, True),
        ("button_use_text", "使用", 1655, 781, 21, True),
        ("button_disassemble_text", "分解", 1500, 879, 21, True),
        ("close_text", "×", 1777, 182, 30, True),
    ]
    text_layers: list[dict[str, object]] = []
    for name, text, x, y, font_size, bold in text_specs:
        text_layers.append(
            {
                "name": name,
                "text": text,
                "x": x,
                "y": y,
                "fontSize": font_size,
                "font": "STKaiti",
                "color": "#2a2822",
                "bold": bold,
                "group": "70_RuntimeText",
            }
        )

    output_psd = (PROJECT_ROOT / str(screen_spec["outputPsd"])).resolve().as_posix()
    manifest = {
        "document": {
            "name": "GameXXK_HeroBackpack_V1",
            "width": 1920,
            "height": 1080,
            "resolution": 72,
            "scale": 1,
            "outputPsd": output_psd,
        },
        "sourceLock": source_lock,
        "imageLayers": image_layers,
        "textLayers": text_layers,
    }
    write_json(root / "manifest.json", manifest)

    runtime_asset_sources = {
        "T_UIV3_HeroBackpack_PaperPanel": "panel",
        "T_UIV3_Tab_Normal": "tab_normal",
        "T_UIV3_Tab_Selected": "tab_selected",
        "T_UIV3_EquipmentSlot": "equipment_slot",
        "T_UIV3_ItemSlot": "item_slot",
        "T_UIV3_ItemDetailPanel": "detail_panel",
        "T_UIV3_Button_Neutral": "button_neutral",
        "T_UIV3_Button_Primary": "button_primary",
        "T_UIV3_Button_Destructive": "button_destructive",
        "T_UIV3_ProgressTrack": "progress_track",
    }
    runtime_assets: list[dict[str, object]] = []
    semantics = {
        "T_UIV3_Button_Neutral": "neutral",
        "T_UIV3_Button_Primary": "primary",
        "T_UIV3_Button_Destructive": "destructive",
    }
    for ue_name, asset_key in runtime_asset_sources.items():
        source = root / assets[asset_key]
        destination = root / "RuntimeAssets" / f"{ue_name}.png"
        shutil.copy2(source, destination)
        record: dict[str, object] = {
            "ueAssetName": ue_name,
            "file": destination.relative_to(root).as_posix(),
        }
        if ue_name in semantics:
            record["buttonSemantic"] = semantics[ue_name]
        runtime_assets.append(record)
    semantic_map = {
        "screenId": screen_spec["screenId"],
        "textBaked": False,
        "heroRuntimeAtlas": source_lock["runtimeAtlasAsset"],
        "assets": runtime_assets,
        "runtimeAssets": runtime_assets,
    }
    write_json(root / "semantic-map.json", semantic_map)

    preview = Image.new("RGBA", CANVAS, (0, 0, 0, 255))
    for item in image_layers:
        if not item.get("visible", True):
            continue
        with Image.open(root / str(item["path"])) as source_image:
            rendered = source_image.convert("RGBA").resize(
                (int(item["width"]), int(item["height"])),
                Image.Resampling.LANCZOS,
            )
        preview.alpha_composite(rendered, (int(item["x"]), int(item["y"])))
    preview_draw = ImageDraw.Draw(preview)
    for item in text_layers:
        preview_draw.text(
            (int(item["x"]), int(item["y"])),
            str(item["text"]),
            font=preview_font(int(item["fontSize"])),
            fill=(42, 40, 34, 255),
            stroke_width=0,
        )
    preview_path = root / "Previews" / "GameXXK_HeroBackpack_V1.png"
    preview.convert("RGB").save(preview_path, quality=95)

    report = {
        "ok": True,
        "screenId": screen_spec["screenId"],
        "canvas": list(CANVAS),
        "imageLayers": len(image_layers),
        "editableTextLayers": len(text_layers),
        "runtimeAssets": len(runtime_assets),
        "heroIdleSource": source_lock["heroIdleSource"],
        "preview": preview_path.as_posix(),
        "outputPsd": output_psd,
    }
    write_json(root / "package-build-report.json", report)
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-root", type=Path, default=CANONICAL_ROOT)
    args = parser.parse_args()
    report = build_package(args.output_root.resolve())
    print(json.dumps(report, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
