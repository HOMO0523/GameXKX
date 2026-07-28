from __future__ import annotations

import json
import math
import shutil
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

from town_psd_image_ops import clean_mask, export_runtime_backgrounds


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PACKAGE_ROOT = PROJECT_ROOT / "SourceArt" / "UI" / "PSD" / "town-v2"
SOURCE = PACKAGE_ROOT / "generated"
OUT = PACKAGE_ROOT / "clean_assets"
RUNTIME_BACKGROUNDS = PACKAGE_ROOT / "runtime_backgrounds"
MANIFEST_SEED = PACKAGE_ROOT / "manifest.seed.json"
MANIFEST = PACKAGE_ROOT / "manifest.json"
PREVIEW = PACKAGE_ROOT / "previews" / "town-clean-preview.png"
REPORT = PACKAGE_ROOT / "clean-report.json"
OUTPUT_PSD = PROJECT_ROOT / "outputs" / "UI_PSD" / "GameXXK_Town_4K.psd"
SCALE = 4
CANVAS = 4096
PAGE_BACKGROUND_RECTS = {
    "hud": (0, 0, 4096, 1720),
    "character": (0, 1720, 2048, 2892),
    "companion": (2048, 1720, 4096, 2892),
    "task": (0, 2892, 1396, 4096),
    "map": (1396, 2892, 2572, 4096),
    "backpack": (2572, 2892, 4096, 4096),
}


def open_rgba(name: str) -> Image.Image:
    return Image.open(SOURCE / name).convert("RGBA")


def cut_white(image: Image.Image, threshold: float = 10.0, min_area: int = 18) -> Image.Image:
    """Remove only the white sheet while keeping enclosed light details opaque."""
    rgba = np.asarray(image.convert("RGBA")).copy()
    rgb = rgba[:, :, :3].astype(np.float32)
    distance = np.sqrt(np.mean((255 - rgb) ** 2, axis=2))
    kept = clean_mask((distance > threshold).astype(np.uint8), min_area=min_area)
    alpha = np.asarray(
        Image.fromarray(kept.astype(np.uint8) * 255, "L").filter(ImageFilter.GaussianBlur(radius=0.65)),
        dtype=np.float32,
    )
    alpha[kept == 1] = 255
    rgba[:, :, 3] = np.clip(alpha, 0, 255).astype(np.uint8)
    ys, xs = np.where(rgba[:, :, 3] > 4)
    if len(xs) == 0:
        return Image.new("RGBA", (1, 1), (0, 0, 0, 0))
    pad = 4
    left = max(0, int(xs.min()) - pad)
    top = max(0, int(ys.min()) - pad)
    right = min(rgba.shape[1], int(xs.max()) + pad + 1)
    bottom = min(rgba.shape[0], int(ys.max()) + pad + 1)
    return Image.fromarray(rgba[top:bottom, left:right], "RGBA")


def grid_cells(image: Image.Image, cols: int, rows: int, inset: float = 0.025) -> list[Image.Image]:
    result: list[Image.Image] = []
    for row in range(rows):
        for col in range(cols):
            x0 = round(image.width * col / cols)
            x1 = round(image.width * (col + 1) / cols)
            y0 = round(image.height * row / rows)
            y1 = round(image.height * (row + 1) / rows)
            dx = round((x1 - x0) * inset)
            dy = round((y1 - y0) * inset)
            result.append(cut_white(image.crop((x0 + dx, y0 + dy, x1 - dx, y1 - dy))))
    return result


def opaque_map(image: Image.Image) -> Image.Image:
    rgb = np.asarray(image.convert("RGB")).astype(np.float32)
    distance = np.sqrt(np.mean((255 - rgb) ** 2, axis=2))
    ys, xs = np.where(distance > 10)
    left, right = int(xs.min()), int(xs.max()) + 1
    top, bottom = int(ys.min()), int(ys.max()) + 1
    tile = image.crop((left, top, right, bottom)).convert("RGB")
    side = min(tile.width, tile.height)
    x = (tile.width - side) // 2
    y = (tile.height - side) // 2
    return tile.crop((x, y, x + side, y + side)).convert("RGBA")


def resize_contain(image: Image.Image, width: int, height: int) -> Image.Image:
    ratio = min(width / image.width, height / image.height)
    size = (max(1, round(image.width * ratio)), max(1, round(image.height * ratio)))
    return image.resize(size, Image.Resampling.LANCZOS)


def resize_stretch(image: Image.Image, width: int, height: int) -> Image.Image:
    return image.resize((width, height), Image.Resampling.LANCZOS)


OUT.mkdir(parents=True, exist_ok=True)
PREVIEW.parent.mkdir(parents=True, exist_ok=True)
for old in OUT.glob("*.png"):
    old.unlink()

background = open_rgba("background_clean.png").resize((CANVAS, CANVAS), Image.Resampling.LANCZOS)
runtime_backgrounds = export_runtime_backgrounds(background, RUNTIME_BACKGROUNDS, PAGE_BACKGROUND_RECTS)
hero = cut_white(open_rgba("hero.png"))
banner_art = cut_white(open_rgba("banner_art.png"), threshold=8)
ui = grid_cells(open_rgba("ui_atlas.png"), 6, 4)
inventory = grid_cells(open_rgba("inventory_atlas.png"), 5, 3, inset=0.04)
partners = grid_cells(open_rgba("partners_atlas.png"), 3, 2)
controls = grid_cells(open_rgba("controls.png"), 4, 2, inset=0.045)
map_tile = opaque_map(open_rgba("map.png"))

frames_source = open_rgba("large_frames.png")
frame_boxes = [
    (20, 95, 845, 455),
    (875, 35, 1110, 490),
    (1135, 75, 1525, 475),
    (20, 560, 380, 955),
    (385, 600, 1145, 945),
    (1140, 560, 1530, 965),
]
frames = [cut_white(frames_source.crop(box), threshold=8) for box in frame_boxes]

detail = open_rgba("character_detail.png")
bars = [
    cut_white(detail.crop((840, 455, 1185, 520)), threshold=5),
    cut_white(detail.crop((840, 545, 1185, 615)), threshold=5),
    cut_white(detail.crop((840, 630, 1185, 710)), threshold=5),
]

layers: list[dict] = []
layer_index = 0


def add(name: str, image: Image.Image, box: tuple[float, float, float, float], mode: str = "contain") -> None:
    global layer_index
    x, y, width, height = box
    x4, y4 = round(x * SCALE), round(y * SCALE)
    w4, h4 = max(1, round(width * SCALE)), max(1, round(height * SCALE))
    asset = resize_stretch(image, w4, h4) if mode == "stretch" else resize_contain(image, w4, h4)
    if mode == "contain":
        x4 += (w4 - asset.width) // 2
        y4 += (h4 - asset.height) // 2
    filename = f"{layer_index:03d}.png"
    path = OUT / filename
    asset.save(path)
    layers.append({"name": name, "path": path.as_posix(), "x": x4, "y": y4})
    layer_index += 1


add("背景_纯净总底图", background, (0, 0, 1024, 1024), "stretch")

# Top scene.
add("角色_主角_顶部", hero, (304, 39, 176, 389))
add("图标_头像_顶部", ui[0], (17, 11, 75, 75))
for name, asset, box in zip(
    ["图标_导航_任务", "图标_导航_背包", "图标_导航_修炼", "图标_导航_地图", "图标_导航_好友"],
    [ui[1], ui[2], ui[10], ui[4], ui[5]],
    [(17, 111, 56, 59), (17, 178, 56, 61), (17, 245, 56, 60), (17, 313, 56, 60), (17, 381, 56, 54)],
):
    add(name, asset, box)

top_hud = [
    ("图标_顶部_铜钱", ui[6], (563, 17, 34, 34)),
    ("图标_顶部_加号一", ui[11], (646, 20, 25, 25)),
    ("图标_顶部_玉币", ui[7], (688, 17, 35, 35)),
    ("图标_顶部_加号二", ui[11], (764, 20, 25, 25)),
    ("图标_顶部_元宝", ui[8], (806, 17, 36, 34)),
    ("图标_顶部_加号三", ui[11], (879, 20, 25, 25)),
    ("图标_顶部_邮件", ui[9], (928, 18, 31, 29)),
    ("图标_顶部_设置", ui[10], (970, 16, 33, 33)),
]
for item in top_hud:
    add(*item)
add("进度条_顶部经验", bars[0], (183, 54, 93, 10), "stretch")

add("框_江湖行横幅", frames[0], (620, 71, 377, 137), "stretch")
add("插画_江湖行横幅", banner_art, (791, 82, 198, 108))
mode_boxes = [(619, 215, 90, 208), (715, 215, 90, 208), (811, 215, 90, 208), (907, 215, 90, 208)]
for i, box in enumerate(mode_boxes):
    add(f"框_玩法入口_{i + 1}", frames[1], box, "stretch")
for name, asset, box in zip(
    ["图标_玩法_探索", "图标_玩法_奇遇", "图标_玩法_挑战", "图标_玩法_论剑"],
    ui[12:16],
    [(632, 337, 63, 72), (733, 332, 49, 70), (831, 345, 55, 58), (928, 343, 58, 62)],
):
    add(name, asset, box)
for i, box in enumerate([(983, 73, 12, 12), (790, 212, 10, 10), (886, 212, 10, 10), (982, 212, 10, 10)]):
    add(f"红点_顶部_{i + 1}", controls[7], box)

# Character detail panel.
tab_y = [477, 516, 555, 594, 633]
for i, y in enumerate(tab_y):
    add(f"标签_角色分类_{i + 1}", controls[0] if i == 0 else controls[1], (18, y, 66, 31), "stretch")
add("角色_主角_详情", hero, (161, 466, 116, 227))
slot_positions = [(110, 474), (110, 541), (110, 608), (292, 474), (292, 541), (292, 608)]
for i, (x, y) in enumerate(slot_positions):
    add(f"框_装备槽_{i + 1}", frames[2], (x, y, 42, 44), "stretch")
for i, ((x, y), asset) in enumerate(zip(slot_positions, ui[18:24])):
    add(f"装备_{i + 1}", asset, (x + 5, y + 5, 32, 34))
add("进度条_角色气血", bars[0], (386, 521, 105, 8), "stretch")
add("进度条_角色内力", bars[1], (386, 551, 105, 8), "stretch")
add("按钮_详细属性", controls[3], (403, 666, 89, 32), "stretch")

# Partner panel.
for i, y in enumerate(tab_y):
    add(f"标签_伙伴分类_{i + 1}", controls[0] if i == 0 else controls[1], (526, y, 66, 31), "stretch")
card_positions = [(617, 471), (744, 471), (871, 471), (617, 604), (744, 604), (871, 604)]
for i, (x, y) in enumerate(card_positions):
    card_height = 129 if i < 3 else 113
    add(f"框_伙伴卡片_{i + 1}", frames[3] if i == 5 else frames[2], (x, y, 113, card_height), "stretch")
for i, ((x, y), asset) in enumerate(zip(card_positions, partners)):
    add(f"伙伴_角色_{i + 1}", asset, (x + 7, y + 7, 99, 76))
for i, (x, y) in enumerate(card_positions):
    add(f"红点_伙伴_{i + 1}", controls[7], (x + 105, y + 3, 9, 9))

# Quest panel.
quest_tabs = [(18, 767), (18, 808), (18, 849), (18, 890)]
for i, (x, y) in enumerate(quest_tabs):
    add(f"标签_任务分类_{i + 1}", controls[0] if i == 0 else controls[1], (x, y, 69, 31), "stretch")
quest_rows = [(96, 758), (96, 836), (96, 914)]
for i, (x, y) in enumerate(quest_rows):
    add(f"框_任务行_{i + 1}", frames[4], (x, y, 235, 70), "stretch")
for i, y in enumerate([805, 884, 962]):
    add(f"图标_任务铜钱_{i + 1}", ui[6], (104, y, 17, 17))
    add(f"底板_任务经验_{i + 1}", controls[6], (153, y + 1, 23, 14), "stretch")
    add(f"图标_任务玉币_{i + 1}", ui[7], (222, y, 17, 17))
for i, (asset, box) in enumerate(zip([controls[3], controls[4], controls[4]], [(270, 786, 56, 28), (270, 864, 56, 28), (270, 942, 56, 28)])):
    add(f"按钮_任务_{i + 1}", asset, box, "stretch")

# Map panel: one complete map plus a separate player marker.
add("地图_完整底图", map_tile, (349, 725, 294, 293), "stretch")
add("地图_玩家头像节点", ui[0], (427, 778, 31, 31))

# Inventory panel.
inventory_tab_x = [667, 738, 810, 879, 949]
for i, x in enumerate(inventory_tab_x):
    add(f"标签_背包分类_{i + 1}", controls[2], (x, 761, 55, 24), "stretch")
slot_x = [675, 738, 801, 864, 927]
slot_y = [791, 851, 911]
inventory_positions = [(x, y) for y in slot_y for x in slot_x]
for i, (x, y) in enumerate(inventory_positions):
    add(f"框_背包物品格_{i + 1}", frames[5], (x, y, 56, 56), "stretch")
for i, ((x, y), asset) in enumerate(zip(inventory_positions, inventory)):
    add(f"物品_{i + 1}", asset, (x + 7, y + 6, 42, 43))
add("按钮_背包整理", controls[4], (824, 978, 76, 31), "stretch")
add("按钮_背包分解", controls[5], (913, 978, 76, 31), "stretch")

# Build a clean no-text preview using the same coordinates as Photoshop.
preview = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
for item in layers:
    layer = Image.open(item["path"]).convert("RGBA")
    preview.alpha_composite(layer, (item["x"], item["y"]))
preview.convert("RGB").save(PREVIEW, quality=95)

manifest = json.loads(MANIFEST_SEED.read_text(encoding="utf-8-sig"))
manifest["document"]["width"] = CANVAS
manifest["document"]["height"] = CANVAS
manifest["document"]["scale"] = SCALE
manifest["document"]["name"] = "GameXXK_Town_4K"
manifest["document"]["outputPsd"] = OUTPUT_PSD.resolve().as_posix()
manifest["imageLayers"] = layers
text_layers = [
    item for item in manifest.get("textLayers", [])
    if not item.get("name", "").startswith("文字_伙伴星级_")
    and not item.get("name", "").startswith("文字_任务经验数值_")
    and item.get("name") not in {"文字_地图符号_断剑崖", "文字_地图符号_云雾山"}
]

center_specs = {
    "文字_标签_属性": (51, 484),
    "文字_标签_装备": (51, 523),
    "文字_标签_技能": (51, 562),
    "文字_标签_天赋": (51, 601),
    "文字_标签_称号": (51, 640),
    "文字_伙伴标签_全部": (559, 484),
    "文字_伙伴标签_仙灵": (559, 523),
    "文字_伙伴标签_妖怪": (559, 562),
    "文字_伙伴标签_侠客": (559, 601),
    "文字_伙伴标签_珍兽": (559, 640),
    "文字_任务标签_主线": (52.5, 774),
    "文字_任务标签_支线": (52.5, 815),
    "文字_任务标签_日常": (52.5, 856),
    "文字_任务标签_奇遇": (52.5, 897),
    "文字_背包标签_全部": (694.5, 765),
    "文字_背包标签_装备": (765.5, 765),
    "文字_背包标签_道具": (837.5, 765),
    "文字_背包标签_材料": (906.5, 765),
    "文字_背包标签_任务": (976.5, 765),
    "文字_按钮_详细属性": (447.5, 674),
    "文字_按钮_追踪": (298, 788),
    "文字_按钮_前往_任务2": (298, 866),
    "文字_按钮_前往_任务3": (298, 945),
    "文字_按钮_整理": (862, 980),
    "文字_按钮_分解": (951, 980),
}

inventory_quantity_specs = {
    "文字_数量_绿玉12": (916, 833),
    "文字_数量_秘籍5": (979, 833),
    "文字_数量_草药26": (727, 893),
    "文字_数量_花8": (790, 893),
    "文字_数量_水晶3": (853, 893),
    "文字_数量_肉6": (916, 893),
    "文字_数量_卷轴2": (979, 893),
    "文字_数量_铜钱12560": (727, 953),
    "文字_数量_玉币860": (790, 953),
    "文字_数量_元宝120": (853, 953),
    "文字_数量_蓝宝石2": (916, 953),
    "文字_数量_药水2": (979, 953),
}

reward_specs = {
    "文字_任务奖励_500": ("500", 123, 808, 11),
    "文字_任务奖励_800": ("800", 123, 887, 11),
    "文字_任务奖励_300": ("300", 123, 965, 11),
    "文字_任务奖励_10": ("10", 242, 808, 11),
    "文字_任务奖励_20": ("20", 242, 887, 11),
    "文字_任务奖励_5": ("5", 242, 965, 11),
}

exp_badge_specs = {
    "文字_任务奖励_EXP1200": (164.5, 808, "1200"),
    "文字_任务奖励_EXP1500": (164.5, 887, "1500"),
    "文字_任务奖励_EXP800": (164.5, 965, "800"),
}

new_exp_values = []
for item in text_layers:
    name = item.get("name")
    if name in center_specs:
        item["x"], item["y"] = center_specs[name]
        item["justify"] = "center"
    elif name in inventory_quantity_specs:
        item["x"], item["y"] = inventory_quantity_specs[name]
        item["justify"] = "right"
    elif name in reward_specs:
        item["text"], item["x"], item["y"], item["fontSize"] = reward_specs[name]
        item.pop("justify", None)
    elif name in exp_badge_specs:
        center_x, top_y, value = exp_badge_specs[name]
        item["text"] = "EXP"
        item["x"] = center_x
        item["y"] = top_y
        item["fontSize"] = 7
        item["justify"] = "center"
        new_exp_values.append({
            "name": f"文字_任务经验数值_{value}",
            "text": value,
            "x": 180,
            "y": top_y,
            "fontSize": 10,
            "font": "Arial-BoldMT",
            "color": "#25341d",
            "bold": True,
        })
    elif name == "文字_容量_32/80":
        item["y"] = 1000
text_layers.extend(new_exp_values)
star_specs = [
    ("文字_伙伴星级_1", "★★☆☆☆", 623, 582, "#b58a3b"),
    ("文字_伙伴星级_2", "★★★☆☆", 750, 582, "#b58a3b"),
    ("文字_伙伴星级_3", "★★☆☆☆", 877, 582, "#b58a3b"),
    ("文字_伙伴星级_4", "★★☆☆☆", 623, 696, "#b58a3b"),
    ("文字_伙伴星级_5", "★★☆☆☆", 750, 696, "#b58a3b"),
    ("文字_伙伴星级_6", "☆☆☆☆☆", 877, 696, "#77746e"),
]
for name, value, x, y, color in star_specs:
    text_layers.append({
        "name": name,
        "text": value,
        "x": x,
        "y": y,
        "fontSize": 9,
        "font": "SegoeUISymbol",
        "color": color,
    })
manifest["textLayers"] = text_layers
MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")

report = {
    "canvas": [CANVAS, CANVAS],
    "imageLayers": len(layers),
    "textLayers": len(manifest.get("textLayers", [])),
    "preview": PREVIEW.as_posix(),
    "runtimeBackgrounds": {name: path.as_posix() for name, path in runtime_backgrounds.items()},
    "outputPsd": manifest["document"]["outputPsd"],
}
REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(report, ensure_ascii=False))
