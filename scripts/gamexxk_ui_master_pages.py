"""Functional grayscale page previews for the GameXXK UI Master Phase A."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re

from PIL import Image, ImageDraw, ImageFont

from scripts.gamexxk_ui_master_assets import build_component_assets
from scripts.gamexxk_ui_master_contract import load_contract


ROOT = Path(__file__).resolve().parents[1]
PACKAGE = ROOT / "SourceArt/UI/PSD/gamexxk-v4/ui-master"
PAGE_SIZE = (1920, 1080)
INK = (43, 40, 34, 255)
MUTED = (93, 85, 72, 220)
PAPER = (232, 215, 179, 255)
HERO = ROOT / "SourceAssets/AnimationProcessing/Production/character_00_hero_idle/frames/frame_0000.png"
PARTNER = ROOT / "SourceAssets/AnimationProcessing/Production/character_01_blade_idle/frames/frame_0000.png"
MONSTER = ROOT / "SourceAssets/AnimationProcessing/Production/enemy_01_rooster_idle/frames/frame_0000.png"
CALIBRATION_V2 = ROOT / "SourceArt/UI/PSD/gamexxk-v4/calibration-v2"
V2_COMPONENT_REVIEW = CALIBRATION_V2 / "Review/GameXXK_HeroBackpack_V2_components.png"
V2_BACKPACK_PREVIEW = CALIBRATION_V2 / "Previews/GameXXK_HeroBackpack_V2.png"
V2_LARGE_PANEL = CALIBRATION_V2 / "Generated/hero_backpack_large_panel_clean.png"
V2_COMPONENTS = CALIBRATION_V2 / "Components"
V2_CONTENT = CALIBRATION_V2 / "Content"
V2_MASTER_PAGE_INDICES = frozenset({0, 3, 7})

META_SHOP_PRODUCTS = (
    ("破军装备包", "Equipment/pojun_weapon.png", "100"),
    ("玄甲装备包", "Equipment/xuanjia_weapon.png", "100"),
    ("青囊装备包", "Equipment/qingnang_weapon.png", "100"),
    ("追风装备包", "Equipment/zhuifeng_weapon.png", "100"),
    ("蚀骨装备包", "Equipment/shigu_weapon.png", "100"),
    ("山河装备包", "Equipment/shanhe_weapon.png", "100"),
    ("伙伴包", "nav_companion.png", "500"),
)

META_SHOP_REVIEW_STATES = (
    "selected",
    "insufficient_funds",
    "confirmation",
    "result",
)

META_SHOP_NAVIGATION = (
    ("nav_backpack", "nav_backpack.png", (62, 244, 86, 86)),
    ("nav_companion", "nav_companion.png", (62, 392, 86, 86)),
    ("nav_codex", "nav_codex.png", (62, 542, 86, 86)),
    ("nav_task", "nav_scroll.png", (62, 691, 86, 86)),
    ("nav_route", "nav_route.png", (62, 841, 86, 86)),
)

META_SHOP_CARD_POSITIONS = (
    (410, 300),
    (630, 300),
    (850, 300),
    (1070, 300),
    (520, 610),
    (740, 610),
    (960, 610),
)


@dataclass(frozen=True)
class CanvasPlacement:
    scale_x: float
    scale_y: float
    content_width: float
    content_height: float


def contain_canvas(
    source_size: tuple[int, int],
    target_size: tuple[int, int],
    alpha_bbox: tuple[int, int, int, int],
) -> CanvasPlacement:
    source_width, source_height = source_size
    target_width, target_height = target_size
    if min(source_width, source_height, target_width, target_height) <= 0:
        raise ValueError("source and target sizes must be positive")
    alpha_left, alpha_top, alpha_right, alpha_bottom = alpha_bbox
    if alpha_right < alpha_left or alpha_bottom < alpha_top:
        raise ValueError("alpha bounding box is invalid")
    scale = min(target_width / source_width, target_height / source_height)
    return CanvasPlacement(
        scale_x=scale,
        scale_y=scale,
        content_width=(alpha_right - alpha_left) * scale,
        content_height=(alpha_bottom - alpha_top) * scale,
    )


def _font(size: int, *, bold: bool = False) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    candidates = (
        Path("C:/Windows/Fonts/msyhbd.ttc") if bold else Path("C:/Windows/Fonts/msyh.ttc"),
        Path("C:/Windows/Fonts/simhei.ttf"),
        Path("C:/Windows/Fonts/simkai.ttf"),
    )
    for candidate in candidates:
        if candidate.is_file():
            return ImageFont.truetype(str(candidate), size)
    return ImageFont.load_default()


def _safe_name(value: str) -> str:
    return re.sub(r"[\\/:*?\"<>|]", "_", value)


def _world_asset(path: Path) -> None:
    if path.is_file():
        return
    image = Image.new("RGBA", PAGE_SIZE, (116, 123, 118, 255))
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 0, 1920, 500), fill=(102, 113, 112, 255))
    draw.polygon(((0, 390), (520, 170), (1030, 370), (1530, 140), (1920, 330), (1920, 0), (0, 0)), fill=(77, 87, 88, 255))
    for x, y, width in ((60, 250, 520), (610, 215, 560), (1220, 245, 620)):
        draw.rectangle((x, y, x + width, y + 310), fill=(91, 91, 86, 255))
        draw.polygon(((x - 50, y + 35), (x + width // 2, y - 95), (x + width + 55, y + 35)), fill=(60, 65, 66, 255))
        for window_x in range(x + 70, x + width - 40, 130):
            draw.rectangle((window_x, y + 100, window_x + 64, y + 188), fill=(151, 143, 123, 255))
    draw.polygon(((0, 760), (710, 570), (1350, 600), (1920, 520), (1920, 1080), (0, 1080)), fill=(133, 128, 113, 255))
    for index in range(18):
        x = (index * 137) % 1920
        y = 620 + (index * 53) % 330
        draw.ellipse((x - 90, y - 60, x + 110, y + 90), fill=(82, 101, 82, 210))
    image.save(path)


class PageBuilder:
    def __init__(
        self,
        group: str,
        asset_root: Path,
        component_records: dict[str, dict],
        package_root: Path,
    ) -> None:
        self.group = group
        self.asset_root = asset_root
        self.package_root = package_root
        self.components = component_records
        self.canvas = Image.new("RGBA", PAGE_SIZE, (0, 0, 0, 0))
        self.image_layers: list[dict] = []
        self.text_layers: list[dict] = []
        self._layer_index = 0
        self._text_index = 0

    def _manifest_path(self, path: Path) -> str:
        resolved = path.resolve()
        try:
            return resolved.relative_to(self.package_root.resolve()).as_posix()
        except ValueError:
            return resolved.as_posix()

    def add_image(
        self,
        name: str,
        path: Path,
        box: tuple[int, int, int, int],
        subgroup: str,
        *,
        fit_mode: str = "stretch",
        opacity: int = 255,
        visible: bool = True,
    ) -> None:
        x, y, width, height = box
        with Image.open(path) as opened:
            source = opened.convert("RGBA")
        if fit_mode == "contain_canvas":
            scale = min(width / source.width, height / source.height)
            rendered_size = (
                max(1, round(source.width * scale)),
                max(1, round(source.height * scale)),
            )
            rendered = source.resize(rendered_size, Image.Resampling.LANCZOS)
            paste_x = x + (width - rendered.width) // 2
            paste_y = y + (height - rendered.height) // 2
        elif fit_mode == "stretch":
            rendered = source.resize((width, height), Image.Resampling.LANCZOS)
            paste_x, paste_y = x, y
        else:
            rendered = source
            paste_x, paste_y = x, y
        if opacity < 255:
            alpha = rendered.getchannel("A").point(lambda value: value * opacity // 255)
            rendered.putalpha(alpha)
        if visible:
            self.canvas.alpha_composite(rendered, (paste_x, paste_y))
        self._layer_index += 1
        layer_record = {
                "name": f"{self._layer_index:03d}_{name}",
                "path": self._manifest_path(path),
                "x": x,
                "y": y,
                "width": width,
                "height": height,
                "fitMode": fit_mode,
                "group": f"{self.group}/{subgroup}",
                "visible": visible,
            }
        if fit_mode == "contain_canvas":
            layer_record["scaleX"] = scale
            layer_record["scaleY"] = scale
        self.image_layers.append(layer_record)

    def add_component(
        self,
        key: str,
        box: tuple[int, int, int, int],
        subgroup: str,
        *,
        name: str | None = None,
        visible: bool = True,
    ) -> None:
        record = self.components[key]
        self.add_image(
            name or key,
            self.asset_root / record["file"],
            box,
            subgroup,
            fit_mode="stretch",
            visible=visible,
        )

    def add_text(
        self,
        text: str,
        position: tuple[int, int],
        size: int,
        subgroup: str = "70_RuntimeText",
        *,
        bold: bool = False,
        color: tuple[int, int, int, int] = INK,
        name: str | None = None,
        visible: bool = True,
    ) -> None:
        x, y = position
        if visible:
            self.canvas.alpha_composite(Image.new("RGBA", PAGE_SIZE, (0, 0, 0, 0)))
            draw = ImageDraw.Draw(self.canvas)
            draw.text((x, y), text, font=_font(size, bold=bold), fill=color)
        self._text_index += 1
        self.text_layers.append(
            {
                "name": name or f"text_{self._text_index:03d}",
                "text": text,
                "x": x,
                "y": y,
                "fontSize": size,
                "font": "MicrosoftYaHei",
                "color": "#2B2822",
                "bold": bold,
                "group": f"{self.group}/{subgroup}",
                "visible": visible,
            }
        )

    def add_centered_text(
        self,
        text: str,
        box: tuple[int, int, int, int],
        size: int,
        subgroup: str = "70_RuntimeText",
        *,
        bold: bool = False,
        color: tuple[int, int, int, int] = INK,
        name: str | None = None,
        visible: bool = True,
    ) -> None:
        x, y, width, height = box
        font = _font(size, bold=bold)
        bounds = ImageDraw.Draw(Image.new("RGBA", (1, 1))).textbbox((0, 0), text, font=font)
        text_width = bounds[2] - bounds[0]
        text_height = bounds[3] - bounds[1]
        draw_x = round(x + (width - text_width) / 2 - bounds[0])
        draw_y = round(y + (height - text_height) / 2 - bounds[1])
        self.add_text(
            text,
            (draw_x, draw_y),
            size,
            subgroup,
            bold=bold,
            color=color,
            name=name,
            visible=visible,
        )

    def add_overlay(self, name: str, image: Image.Image, subgroup: str) -> None:
        overlay_root = self.asset_root / "LayoutAssets"
        overlay_root.mkdir(parents=True, exist_ok=True)
        path = overlay_root / f"{_safe_name(self.group)}_{_safe_name(name)}.png"
        image.save(path)
        self.add_image(name, path, (0, 0, 1920, 1080), subgroup, fit_mode="stretch")

    def save(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        self.canvas.convert("RGB").save(path, quality=94)


def _add_world(builder: PageBuilder, world_path: Path, *, dim: bool = False) -> None:
    builder.add_image("town_world", world_path, (0, 0, 1920, 1080), "10_World")
    if dim:
        overlay = Image.new("RGBA", PAGE_SIZE, (27, 27, 25, 108))
        builder.add_overlay("world_dim", overlay, "11_WorldOverlay")


def _add_town_hud(builder: PageBuilder) -> None:
    builder.add_component("panel_small", (24, 20, 445, 142), "20_Shell", name="hero_status_panel")
    builder.add_text("主角  Lv. 1", (132, 42), 28, bold=True)
    builder.add_text("经验  0 / 100     战力 33", (132, 91), 20)
    builder.add_component("nav_normal", (34, 205, 96, 96), "20_Shell", name="nav_backpack_disc")
    builder.add_component("nav_backpack", (46, 217, 72, 72), "21_Navigation")
    for index, kind in enumerate(("companion", "codex", "task", "route"), start=1):
        y = 205 + index * 118
        builder.add_component("nav_normal", (34, y, 96, 96), "20_Shell", name=f"nav_{kind}_disc")
        builder.add_component(f"nav_{kind}", (46, y + 12, 72, 72), "21_Navigation")
    builder.add_component("resource_strip", (1215, 20, 675, 82), "20_Shell")
    builder.add_text("铜钱 10,000      青玉 2,000      金锭 500", (1290, 42), 23)


def _add_panel_title(builder: PageBuilder, title: str, subtitle: str = "") -> None:
    builder.add_text(title, (332, 186), 42, bold=True)
    if subtitle:
        builder.add_text(subtitle, (332, 242), 20, color=MUTED)


def _add_main_panel(builder: PageBuilder) -> None:
    builder.add_component("panel_large", (278, 150, 1390, 850), "20_Shell", name="main_panel")


def _add_tabs(builder: PageBuilder, selected: int = 0) -> None:
    labels = ("属性", "装备", "技能", "天赋", "称号")
    for index, label in enumerate(labels):
        x = 510 + index * 170
        key = "tab_selected" if index == selected else "tab_normal"
        builder.add_component(key, (x, 184, 150, 54), "22_Tabs", name=f"tab_{index}")
        builder.add_text(label, (x + 45, 195), 21)


def _add_item_grid(builder: PageBuilder, origin: tuple[int, int], columns: int, rows: int) -> None:
    ox, oy = origin
    for row in range(rows):
        for column in range(columns):
            key = "item_slot_hover" if row == 0 and column == 0 else "item_slot_empty"
            builder.add_component(key, (ox + column * 112, oy + row * 112, 100, 100), "35_Grid")


def _draw_route_overlay(selected: bool = False) -> Image.Image:
    image = Image.new("RGBA", PAGE_SIZE, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    points = ((520, 790), (700, 640), (890, 735), (1060, 540), (1230, 625), (1420, 400))
    draw.line(tuple(value for point in points for value in point), fill=(43, 40, 34, 190), width=10)
    for index, (x, y) in enumerate(points):
        radius = 34 if selected and index == 3 else 26
        fill = (232, 215, 179, 255)
        draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=fill, outline=INK, width=7)
        if selected and index == 3:
            draw.ellipse((x - 48, y - 48, x + 48, y + 48), outline=(161, 79, 54, 255), width=6)
    return image


def _draw_backpack_scrollbar_overlay() -> Image.Image:
    image = Image.new("RGBA", PAGE_SIZE, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((1642, 332, 1660, 930), radius=9, fill=(77, 69, 57, 82), outline=(43, 40, 34, 150), width=2)
    draw.rounded_rectangle((1640, 350, 1662, 520), radius=10, fill=(226, 205, 164, 255), outline=(43, 40, 34, 220), width=3)
    draw.line((1647, 377, 1655, 377), fill=(94, 82, 65, 210), width=2)
    draw.line((1647, 405, 1655, 405), fill=(94, 82, 65, 210), width=2)
    draw.line((1647, 433, 1655, 433), fill=(94, 82, 65, 210), width=2)
    draw.line((1647, 461, 1655, 461), fill=(94, 82, 65, 210), width=2)
    draw.line((1647, 489, 1655, 489), fill=(94, 82, 65, 210), width=2)
    return image


def _page_common(builder: PageBuilder, world: Path) -> None:
    _add_world(builder, world, dim=True)
    builder.add_component("panel_large", (90, 82, 1740, 920), "20_Shell")
    builder.add_text("00  公共组件 / 纸墨通用控件", (135, 112), 38, bold=True)
    button_states = ("normal", "hover", "pressed", "primary", "danger", "disabled")
    for index, state in enumerate(button_states):
        x = 140 + index * 270
        builder.add_component(f"button_{state}", (x, 190, 220, 72), "30_Buttons")
        builder.add_text(state, (x + 48, 278), 17)
    tab_states = ("normal", "hover", "pressed", "selected", "disabled")
    for index, state in enumerate(tab_states):
        x = 140 + index * 210
        builder.add_component(f"tab_{state}", (x, 340, 160, 58), "31_Tabs")
        builder.add_text(state, (x + 28, 412), 16)
    nav_kinds = ("backpack", "companion", "codex", "task", "route")
    for index, kind in enumerate(nav_kinds):
        x = 132 + index * 170
        builder.add_component("nav_normal", (x, 480, 112, 112), "32_Navigation")
        builder.add_component(f"nav_{kind}", (x + 20, 500, 72, 72), "32_Navigation")
    for index, family in enumerate(("role", "monster", "general", "terrain", "rare", "boss")):
        builder.add_component(f"card_frame_{family}", (1045 + index * 117, 460, 104, 146), "33_CardFrames")
    for index, key in enumerate(("item_slot_empty", "item_slot_hover", "item_slot_selected", "item_slot_locked")):
        builder.add_component(key, (140 + index * 128, 675, 104, 104), "34_Slots")
    builder.add_component("equipment_slot_selected", (700, 660, 124, 130), "34_Slots")
    builder.add_component("progress_track", (140, 850, 420, 24), "35_Progress")
    builder.add_component("progress_fill", (150, 857, 280, 10), "35_Progress")
    builder.add_component("tooltip_panel", (920, 690, 520, 240), "36_Tooltip")
    builder.add_text("按钮仅用纸底与墨线区分；危险态只使用小朱砂印。", (965, 750), 20)
    builder.add_text("动态文字始终保持独立图层", (965, 800), 24, bold=True)


def _page_common_v2(builder: PageBuilder) -> None:
    builder.add_image(
        "approved_v2_component_review",
        V2_COMPONENT_REVIEW,
        (0, 0, 1920, 1080),
        "10_ApprovedV2Reference",
    )


def _page_main_menu(builder: PageBuilder, world: Path) -> None:
    _add_world(builder, world, dim=True)
    builder.add_component("panel_medium", (580, 130, 760, 820), "20_Shell")
    builder.add_text("行旅异闻", (785, 235), 76, bold=True)
    builder.add_text("山川入墨，众生入局", (795, 330), 25)
    for index, label in enumerate(("继续旅程", "新的旅程", "设置", "退出")):
        key = "button_primary" if index == 0 else "button_normal"
        builder.add_component(key, (820, 440 + index * 100, 280, 76), "30_Menu")
        builder.add_text(label, (888, 458 + index * 100), 26)


def _page_town_hud(builder: PageBuilder, world: Path) -> None:
    _add_world(builder, world)
    _add_town_hud(builder)
    builder.add_component("tooltip_panel", (150, 720, 520, 220), "30_Context")
    builder.add_text("青山镇 · 客栈前街", (190, 755), 28, bold=True)
    builder.add_text("F  与掌柜交谈     城镇按钮进入路线图", (190, 815), 21)


def _page_backpack(builder: PageBuilder, world: Path, *, selected: bool = False) -> None:
    _add_world(builder, world, dim=True)
    _add_town_hud(builder)
    _add_main_panel(builder)
    _add_panel_title(builder, "主角", "当前最终 Idle · 保持 512×512 透明画布比例")
    _add_tabs(builder, selected=1)
    builder.add_image("hero_idle", HERO, (420, 300, 540, 540), "30_Character", fit_mode="contain_canvas")
    equipment = ((340, 340), (340, 500), (340, 660), (890, 340), (890, 500), (890, 660))
    for index, (x, y) in enumerate(equipment):
        key = "equipment_slot_selected" if selected and index == 1 else "equipment_slot_empty"
        builder.add_component(key, (x, y, 112, 118), "31_Equipment")
    _add_item_grid(builder, (1080, 340), 4, 4)
    builder.add_text("背包  5 / 80", (1080, 285), 26, bold=True)
    builder.add_text("Lv. 1       攻击 33       气血 120       防御 18", (430, 872), 22)
    builder.add_component("button_primary", (1290, 860, 210, 70), "40_Actions")
    builder.add_text("使用", (1365, 878), 25)
    if selected:
        builder.add_component("tooltip_panel", (1050, 650, 520, 200), "45_Selection")
        builder.add_text("小布袋", (1090, 680), 26, bold=True)
        builder.add_text("普通布袋，能装下一些小物件。", (1090, 730), 18)
        builder.add_component("button_danger", (1330, 760, 190, 62), "45_Selection")
        builder.add_text("分解", (1395, 775), 22)


def _page_backpack_v2(builder: PageBuilder) -> None:
    builder.add_image("approved_v2_backpack_shell", V2_LARGE_PANEL, (0, 0, 1920, 1080), "10_ApprovedV2Shell")
    builder.add_image("hero_portrait", V2_CONTENT / "hero_portrait.png", (48, 32, 132, 132), "15_HudContent", fit_mode="contain_canvas")
    builder.add_text("Lv. 1", (205, 56), 28, bold=True)
    builder.add_text("0 / 100", (310, 98), 18)
    builder.add_text("33", (248, 140), 24, color=(156, 69, 45, 255))
    builder.add_text("10,000", (1268, 53), 25)
    builder.add_text("2,000", (1508, 53), 25)
    builder.add_text("500", (1782, 53), 25)
    builder.add_text("主角", (395, 213), 42, bold=True)

    tab_labels = ("属性", "装备", "技能", "天赋", "称号")
    tab_files = (
        "tab_01_attribute.png",
        "tab_02_equipment_selected.png",
        "tab_03_skill.png",
        "tab_04_talent.png",
        "tab_05_title.png",
    )
    for index, (label, filename) in enumerate(zip(tab_labels, tab_files)):
        x = 570 + index * 115
        builder.add_image(f"tab_{index + 1}", V2_COMPONENTS / filename, (x, 220, 105, 62), "20_Tabs")
        builder.add_centered_text(label, (x, 220, 105, 62), 20, bold=index == 1)

    builder.add_image("hero_idle", HERO, (490, 305, 510, 510), "30_Character", fit_mode="contain_canvas")
    equipment_frames = (
        ("equipment_slot_left_01.png", 420, 340, "starter_weapon.png"),
        ("equipment_slot_left_02.png", 420, 515, "starter_head.png"),
        ("equipment_slot_left_03.png", 420, 690, "starter_armor.png"),
        ("equipment_slot_right_01.png", 930, 340, "starter_belt.png"),
        ("equipment_slot_right_02.png", 930, 515, "starter_shoes.png"),
        ("equipment_slot_right_03.png", 930, 690, "starter_accessory.png"),
    )
    for index, (frame_name, x, y, icon_name) in enumerate(equipment_frames):
        builder.add_image(f"equipment_frame_{index + 1}", V2_COMPONENTS / frame_name, (x, y, 118, 124), "31_EquipmentFrames")
        builder.add_image(
            f"equipment_icon_{index + 1}",
            V2_CONTENT / "StarterEquipment" / icon_name,
            (x + 15, y + 15, 88, 88),
            "32_EquipmentIcons",
            fit_mode="contain_canvas",
        )

    category_labels = ("全部", "装备", "材料", "任务", "其他")
    for index, label in enumerate(category_labels):
        builder.add_centered_text(label, (1128 + index * 80, 232, 70, 50), 21 if index == 0 else 19, bold=index == 0)
    builder.add_text("背包  3 / 200", (1525, 250), 18, bold=True)

    slot_files = sorted(V2_COMPONENTS.glob("inventory_slot_*.png"))
    if len(slot_files) < 16:
        raise FileNotFoundError(f"expected 16 V2 inventory slot components in {V2_COMPONENTS}")
    item_icons = (
        V2_CONTENT / "Items/strengthening_stone.png",
        V2_CONTENT / "Items/refinement_sand.png",
        V2_CONTENT / "Items/qingshan_suppression_token.png",
    )
    slot_index = 0
    for row in range(5):
        for column in range(4):
            x = 1135 + column * 122
            y = 300 + row * 130
            builder.add_image(
                f"inventory_slot_{row + 1}_{column + 1}",
                slot_files[slot_index % len(slot_files)],
                (x, y, 110, 116),
                "40_InventorySlots",
            )
            if slot_index < len(item_icons):
                builder.add_image(
                    f"inventory_item_{slot_index + 1}",
                    item_icons[slot_index],
                    (x + 14, y + 12, 82, 82),
                    "41_InventoryItems",
                    fit_mode="contain_canvas",
                )
            slot_index += 1
    builder.add_overlay("inventory_scrollbar_right", _draw_backpack_scrollbar_overlay(), "42_InventoryScrollbar")
    for index, label in enumerate(("Lv. 1", "攻击 33", "气血 120", "防御 18")):
        builder.add_centered_text(label, (445 + index * 130, 884, 130, 44), 20)


def _page_party(builder: PageBuilder, world: Path, *, selected: bool = False) -> None:
    _add_world(builder, world, dim=True)
    _add_town_hud(builder)
    _add_main_panel(builder)
    _add_panel_title(builder, "伙伴编队", "拖动卡片调整前后排；主角入口不再与伙伴混淆")
    portraits = ((HERO, "主角", 410), (PARTNER, "剑客", 760))
    for index, (path, label, x) in enumerate(portraits):
        family = "card_frame_role"
        builder.add_component(family, (x, 320, 290, 405), "30_Cards")
        builder.add_image(f"party_{index}", path, (x + 15, 350, 260, 260), "31_Portraits", fit_mode="contain_canvas")
        builder.add_text(label, (x + 100, 650), 24, bold=True)
    for index in range(3):
        key = "card_frame_rare" if selected and index == 0 else "card_frame_general"
        builder.add_component(key, (1110 + index * 150, 350, 130, 182), "32_FormationSlots")
    builder.add_text("前排", (1140, 560), 20)
    builder.add_text("后排", (1375, 560), 20)
    builder.add_component("button_primary", (1260, 820, 240, 72), "40_Actions")
    builder.add_text("保存编队", (1322, 838), 24)


def _page_codex(builder: PageBuilder, world: Path, *, selected: bool = False) -> None:
    _add_world(builder, world, dim=True)
    _add_town_hud(builder)
    _add_main_panel(builder)
    _add_panel_title(builder, "图鉴", "怪物卡片、掉落与已发现状态")
    for index in range(6):
        x = 360 + index * 205
        key = "card_frame_monster" if index < 3 else "card_frame_general"
        builder.add_component(key, (x, 340, 170, 238), "30_MonsterCards")
        if index == 0:
            builder.add_image("rooster_idle", MONSTER, (x + 10, 360, 150, 150), "31_Portraits", fit_mode="contain_canvas")
            builder.add_text("山野公鸡", (x + 28, 525), 18, bold=True)
        else:
            builder.add_text("未发现" if index > 2 else f"怪物 {index + 1}", (x + 45, 525), 17)
    if selected:
        builder.add_component("panel_medium", (900, 610, 620, 300), "40_Detail")
        builder.add_image("rooster_detail", MONSTER, (930, 645, 230, 230), "41_DetailPortrait", fit_mode="contain_canvas")
        builder.add_text("山野公鸡", (1180, 660), 28, bold=True)
        builder.add_text("普通怪物 · 啄击 · 易怒", (1180, 715), 19)
        builder.add_text("可能掉落：羽毛、铜钱", (1180, 770), 18)


def _page_quest(builder: PageBuilder, world: Path) -> None:
    _add_world(builder, world, dim=True)
    _add_town_hud(builder)
    _add_main_panel(builder)
    _add_panel_title(builder, "任务日志", "当前、可接取、已完成")
    for index, label in enumerate(("掌柜的请求", "青山镇来客", "山路异响", "已完成任务")):
        key = "button_primary" if index == 0 else "button_normal"
        builder.add_component(key, (350, 320 + index * 105, 420, 74), "30_QuestList")
        builder.add_text(label, (405, 337 + index * 105), 23)
    builder.add_component("panel_medium", (835, 305, 700, 520), "31_QuestDetail")
    builder.add_text("掌柜的请求", (900, 360), 30, bold=True)
    builder.add_text("前往镇外山路，调查最近出现的怪物。", (900, 430), 21)
    builder.add_text("目标：进入路线图并完成一次战斗", (900, 500), 20)
    builder.add_text("奖励：铜钱 200 · 青玉 10", (900, 565), 20)
    builder.add_component("button_primary", (1230, 710, 230, 70), "40_Actions")
    builder.add_text("追踪任务", (1290, 728), 23)


def _page_shop(builder: PageBuilder, world: Path) -> None:
    _add_world(builder, world, dim=True)
    _add_town_hud(builder)
    _add_main_panel(builder)
    _add_panel_title(builder, "商店交易", "货架与背包并排，价格和数量独立显示")
    _add_item_grid(builder, (360, 340), 5, 4)
    _add_item_grid(builder, (1030, 340), 4, 4)
    builder.add_text("货架", (360, 290), 26, bold=True)
    builder.add_text("我的背包", (1030, 290), 26, bold=True)
    builder.add_component("tooltip_panel", (690, 725, 520, 190), "40_TradeDetail")
    builder.add_text("草药包   单价 80", (740, 760), 23, bold=True)
    builder.add_component("button_primary", (970, 820, 190, 62), "40_TradeDetail")
    builder.add_text("购买", (1035, 835), 22)


def _add_meta_shop_global_shell(builder: PageBuilder) -> None:
    builder.add_image(
        "approved_v2_shop_shell",
        V2_LARGE_PANEL,
        (0, 0, 1920, 1080),
        "10_Background",
    )
    builder.add_image(
        "hero_portrait",
        V2_CONTENT / "hero_portrait.png",
        (41, 37, 132, 132),
        "20_GlobalShell",
        fit_mode="contain_canvas",
    )
    builder.add_text("主角  Lv. 1", (198, 50), 26, "20_GlobalShell", bold=True)
    builder.add_text("行旅者", (198, 88), 19, "20_GlobalShell", color=MUTED)
    builder.add_text("战力 33", (198, 122), 20, "20_GlobalShell", bold=True)
    for name, filename, box in META_SHOP_NAVIGATION:
        builder.add_image(
            name,
            V2_CONTENT / filename,
            box,
            "20_GlobalShell",
            fit_mode="contain_canvas",
        )
    _add_coin_price(
        builder,
        "500",
        (1370, 36, 310, 60),
        "20_GlobalShell",
        text_size=27,
        icon_size=42,
        gap=12,
        bold=True,
    )


def _add_coin_price(
    builder: PageBuilder,
    value: str,
    box: tuple[int, int, int, int],
    subgroup: str,
    *,
    text_size: int,
    icon_size: int,
    gap: int = 6,
    prefix: str = "",
    bold: bool = False,
    visible: bool = True,
) -> None:
    x, y, width, height = box
    font = _font(text_size, bold=bold)
    measure = ImageDraw.Draw(Image.new("RGBA", (1, 1)))
    value_bounds = measure.textbbox((0, 0), value, font=font)
    value_width = value_bounds[2] - value_bounds[0]
    value_height = value_bounds[3] - value_bounds[1]
    prefix_bounds = measure.textbbox((0, 0), prefix, font=font) if prefix else (0, 0, 0, 0)
    prefix_width = prefix_bounds[2] - prefix_bounds[0]
    prefix_height = prefix_bounds[3] - prefix_bounds[1]
    prefix_gap = gap if prefix else 0
    content_width = prefix_width + prefix_gap + icon_size + gap + value_width
    cursor_x = round(x + (width - content_width) / 2)
    if prefix:
        prefix_y = round(y + (height - prefix_height) / 2 - prefix_bounds[1])
        builder.add_text(
            prefix,
            (cursor_x, prefix_y),
            text_size,
            subgroup,
            bold=bold,
            visible=visible,
        )
        cursor_x += prefix_width + prefix_gap
    icon_y = round(y + (height - icon_size) / 2)
    builder.add_image(
        f"coin_{subgroup.replace('/', '_')}_{value}",
        V2_CONTENT / "resource_coin.png",
        (cursor_x, icon_y, icon_size, icon_size),
        subgroup,
        fit_mode="contain_canvas",
        visible=visible,
    )
    cursor_x += icon_size + gap
    value_y = round(y + (height - value_height) / 2 - value_bounds[1])
    builder.add_text(
        value,
        (cursor_x, value_y),
        text_size,
        subgroup,
        bold=bold,
        color=MUTED if not bold else INK,
        visible=visible,
    )


def _add_meta_shop_state(builder: PageBuilder, state: str, *, visible: bool) -> None:
    groups = {
        "selected": "71_State_Selected",
        "insufficient_funds": "72_State_InsufficientFunds",
        "confirmation": "73_State_Confirmation",
        "result": "74_State_Result",
    }
    subgroup = groups[state]
    builder.add_image(
        f"{state}_selection_ink",
        V2_CONTENT / "category_selected_ink.png",
        (398, 276, 194, 66),
        subgroup,
        visible=visible,
    )
    if state == "selected":
        builder.add_image(
            "purchase_button",
            V2_COMPONENTS / "tab_02_equipment_selected.png",
            (1375, 870, 210, 72),
            subgroup,
            visible=visible,
        )
        _add_coin_price(
            builder,
            "100",
            (1375, 870, 210, 72),
            subgroup,
            text_size=23,
            icon_size=28,
            gap=8,
            prefix="购买",
            bold=True,
            visible=visible,
        )
        builder.add_text("点击后再次确认", (1412, 955), 16, subgroup, color=MUTED, visible=visible)
    elif state == "insufficient_funds":
        builder.add_component(
            "button_disabled",
            (1375, 870, 210, 72),
            subgroup,
            name="purchase_disabled",
            visible=visible,
        )
        _add_coin_price(
            builder,
            "100",
            (1375, 870, 210, 72),
            subgroup,
            text_size=23,
            icon_size=28,
            gap=8,
            prefix="购买",
            bold=True,
            visible=visible,
        )
        builder.add_centered_text(
            "铜钱不足，还需 50",
            (1330, 952, 310, 28),
            16,
            subgroup,
            color=(156, 69, 45, 255),
            visible=visible,
        )
    elif state == "confirmation":
        builder.add_component(
            "tooltip_panel",
            (1280, 725, 400, 245),
            subgroup,
            name="confirmation_panel",
            visible=visible,
        )
        builder.add_centered_text(
            "确认购买破军装备包？",
            (1310, 752, 350, 36),
            23,
            subgroup,
            bold=True,
            visible=visible,
        )
        _add_coin_price(
            builder,
            "100",
            (1310, 792, 340, 54),
            subgroup,
            text_size=19,
            icon_size=24,
            gap=7,
            prefix="将消耗",
            bold=True,
            visible=visible,
        )
        builder.add_component(
            "button_primary",
            (1315, 868, 150, 58),
            subgroup,
            name="confirm_purchase",
            visible=visible,
        )
        builder.add_centered_text(
            "确认购买",
            (1315, 868, 150, 58),
            19,
            subgroup,
            bold=True,
            visible=visible,
        )
        builder.add_component(
            "button_normal",
            (1495, 868, 150, 58),
            subgroup,
            name="cancel_purchase",
            visible=visible,
        )
        builder.add_centered_text("取消", (1495, 868, 150, 58), 19, subgroup, visible=visible)
    elif state == "result":
        builder.add_component(
            "tooltip_panel",
            (1280, 725, 400, 245),
            subgroup,
            name="result_panel",
            visible=visible,
        )
        builder.add_centered_text(
            "购买成功",
            (1310, 752, 350, 38),
            25,
            subgroup,
            bold=True,
            visible=visible,
        )
        builder.add_centered_text(
            "获得：破军·武器（普通）",
            (1310, 812, 350, 32),
            19,
            subgroup,
            visible=visible,
        )
        builder.add_component(
            "button_primary",
            (1400, 875, 160, 58),
            subgroup,
            name="accept_result",
            visible=visible,
        )
        builder.add_centered_text(
            "收下",
            (1400, 875, 160, 58),
            19,
            subgroup,
            bold=True,
            visible=visible,
        )


def _page_meta_shop_v2(
    builder: PageBuilder,
    *,
    review_state: str = "selected",
    include_hidden_states: bool = True,
) -> None:
    if review_state not in META_SHOP_REVIEW_STATES:
        raise ValueError(f"unsupported meta-shop review state: {review_state}")

    _add_meta_shop_global_shell(builder)
    builder.add_text("新商店", (390, 205), 42, "30_ShopPaper", bold=True)
    builder.add_text("套装装备包与伙伴包", (565, 221), 22, "30_ShopPaper", color=MUTED)

    frame_paths = tuple(V2_COMPONENTS.glob("inventory_slot_*.png"))
    if not frame_paths:
        raise FileNotFoundError(f"missing V2 inventory slot components in {V2_COMPONENTS}")
    frame_path = sorted(frame_paths)[0]
    for index, ((label, relative_icon, price), (x, y)) in enumerate(
        zip(META_SHOP_PRODUCTS, META_SHOP_CARD_POSITIONS)
    ):
        builder.add_image(
            f"product_card_{index + 1}",
            frame_path,
            (x, y, 170, 170),
            "40_ProductGrid",
        )
        builder.add_image(
            f"product_icon_{index + 1}",
            V2_CONTENT / relative_icon,
            (x + 20, y + 18, 130, 130),
            "40_ProductGrid",
            fit_mode="contain_canvas",
        )
        builder.add_centered_text(
            label,
            (x - 10, y + 178, 190, 34),
            20,
            "40_ProductGrid",
            bold=index == 0,
        )
        _add_coin_price(
            builder,
            price,
            (x, y + 210, 170, 38),
            "40_ProductGrid",
            text_size=18,
            icon_size=22,
        )

    builder.add_image(
        "selected_product_detail_slot",
        V2_COMPONENTS / "detail_item_slot.png",
        (1370, 330, 220, 220),
        "50_ProductDetail",
    )
    builder.add_image(
        "selected_product_detail_icon",
        V2_CONTENT / "Equipment/pojun_weapon.png",
        (1405, 365, 150, 150),
        "50_ProductDetail",
        fit_mode="contain_canvas",
    )
    builder.add_centered_text(
        "破军装备包",
        (1305, 575, 350, 44),
        30,
        "50_ProductDetail",
        bold=True,
    )
    builder.add_text("随机获得破军套装的一个部位", (1305, 640), 19, "50_ProductDetail")
    builder.add_text("装备等级：当前主角等级", (1305, 680), 19, "50_ProductDetail")
    builder.add_text("普通 70%  ·  稀有 25%  ·  珍稀 5%", (1305, 720), 17, "50_ProductDetail")
    builder.add_text("可能部位：武器 / 头部 / 衣甲", (1305, 770), 17, "50_ProductDetail")
    builder.add_text("腰带 / 鞋 / 饰品", (1305, 804), 17, "50_ProductDetail")

    states = META_SHOP_REVIEW_STATES if include_hidden_states else (review_state,)
    for state in states:
        _add_meta_shop_state(builder, state, visible=state == review_state)


def _page_route(builder: PageBuilder, world: Path, *, selected: bool = False) -> None:
    _add_world(builder, world, dim=True)
    builder.add_component("panel_large", (220, 100, 1480, 900), "20_Shell")
    builder.add_text("山路路线", (290, 145), 42, bold=True)
    builder.add_text("选择下一个节点：战斗、事件、商店或休整", (290, 205), 21)
    builder.add_overlay("route_nodes", _draw_route_overlay(selected), "30_Route")
    if selected:
        builder.add_component("tooltip_panel", (1050, 280, 500, 210), "40_Selection")
        builder.add_text("怪物战斗", (1100, 320), 27, bold=True)
        builder.add_text("遭遇 2–3 只普通怪物", (1100, 375), 19)
        builder.add_component("button_primary", (1280, 410, 210, 62), "40_Selection")
        builder.add_text("进入节点", (1335, 425), 21)


def _page_event(builder: PageBuilder, world: Path) -> None:
    _add_world(builder, world, dim=True)
    builder.add_component("panel_large", (270, 120, 1380, 840), "20_Shell")
    builder.add_text("山泉旁的旅人", (360, 185), 39, bold=True)
    builder.add_component("panel_medium", (360, 270, 620, 540), "30_Illustration")
    builder.add_text("一名旅人守着清泉，似乎在等待同行者。", (1030, 320), 22)
    for index, label in enumerate(("分享干粮", "询问山路", "悄悄离开")):
        key = "button_primary" if index == 0 else "button_normal"
        builder.add_component(key, (1060, 430 + index * 105, 380, 72), "40_Choices")
        builder.add_text(label, (1150, 447 + index * 105), 24)


def _page_battle(builder: PageBuilder, world: Path, *, target_selected: bool = False) -> None:
    _add_world(builder, world, dim=False)
    builder.add_image("hero_battle_idle", HERO, (130, 220, 540, 540), "30_Characters", fit_mode="contain_canvas")
    builder.add_image("rooster_battle_idle", MONSTER, (1250, 220, 540, 540), "30_Characters", fit_mode="contain_canvas")
    builder.add_component("progress_track", (210, 760, 360, 22), "31_FootBars")
    builder.add_component("progress_fill", (220, 766, 240, 10), "31_FootBars")
    builder.add_component("progress_track", (1340, 760, 360, 22), "31_FootBars")
    builder.add_component("progress_fill", (1350, 766, 190, 10), "31_FootBars")
    builder.add_text("主角 120 / 120", (300, 790), 20)
    builder.add_text("山野公鸡 68 / 68", (1430, 790), 20)
    for index, family in enumerate(("general", "role", "terrain", "rare")):
        x = 560 + index * 205
        key = f"card_frame_{family}"
        width, height = (190, 266) if target_selected and index == 1 else (160, 224)
        y = 790 if target_selected and index == 1 else 825
        builder.add_component(key, (x, y, width, height), "40_HandCards")
    builder.add_text("回合 1", (40, 30), 28, bold=True)
    builder.add_text("能量 3 / 3", (1600, 35), 24, bold=True)
    if target_selected:
        overlay = Image.new("RGBA", PAGE_SIZE, (20, 20, 18, 78))
        draw = ImageDraw.Draw(overlay)
        draw.ellipse((1270, 225, 1770, 725), outline=(232, 215, 179, 245), width=10)
        builder.add_overlay("target_focus", overlay, "50_Targeting")
        builder.add_text("选择目标", (890, 730), 30, bold=True)


def _page_rewards(builder: PageBuilder, world: Path) -> None:
    _add_world(builder, world, dim=True)
    builder.add_component("panel_large", (330, 130, 1260, 820), "20_Shell")
    builder.add_text("战斗胜利", (790, 200), 54, bold=True)
    builder.add_text("选择一项奖励", (850, 280), 24)
    for index, family in enumerate(("general", "rare", "role")):
        x = 540 + index * 310
        builder.add_component(f"card_frame_{family}", (x, 350, 250, 350), "30_Rewards")
        builder.add_text(("铜钱 120", "新卡牌", "伙伴经验")[index], (x + 70, 730), 22, bold=True)
    builder.add_component("button_primary", (840, 820, 240, 72), "40_Actions")
    builder.add_text("确认奖励", (902, 838), 24)


def _page_system(builder: PageBuilder, world: Path) -> None:
    _add_world(builder, world, dim=True)
    builder.add_component("panel_medium", (580, 130, 760, 820), "20_Shell")
    builder.add_text("系统菜单", (820, 220), 45, bold=True)
    labels = ("继续游戏", "音量与显示", "操作设置", "返回主菜单")
    for index, label in enumerate(labels):
        key = "button_primary" if index == 0 else ("button_danger" if index == 3 else "button_normal")
        builder.add_component(key, (770, 345 + index * 112, 380, 76), "30_Menu")
        builder.add_text(label, (870, 363 + index * 112), 25)


def _build_page(builder: PageBuilder, index: int, world: Path) -> None:
    if index == 0:
        _page_common_v2(builder)
    elif index == 1:
        _page_main_menu(builder, world)
    elif index == 2:
        _page_town_hud(builder, world)
    elif index == 3:
        _page_backpack_v2(builder)
    elif index == 4:
        _page_party(builder, world)
    elif index == 5:
        _page_codex(builder, world)
    elif index == 6:
        _page_quest(builder, world)
    elif index == 7:
        _page_meta_shop_v2(builder)
    elif index == 8:
        _page_route(builder, world)
    elif index == 9:
        _page_event(builder, world)
    elif index == 10:
        _page_battle(builder, world)
    elif index == 11:
        _page_rewards(builder, world)
    elif index == 12:
        _page_system(builder, world)
    elif index == 13:
        _page_backpack(builder, world, selected=True)
    elif index == 14:
        _page_party(builder, world, selected=True)
    elif index == 15:
        _page_codex(builder, world, selected=True)
    elif index == 16:
        _page_route(builder, world, selected=True)
    elif index == 17:
        _page_battle(builder, world, target_selected=True)
    else:
        raise ValueError(f"unsupported page index: {index}")


def build_page_previews(
    output_root: Path,
    *,
    asset_root: Path | None = None,
    package_root: Path | None = None,
) -> list[dict]:
    output_root.mkdir(parents=True, exist_ok=True)
    package_root = package_root or output_root
    asset_root = asset_root or output_root / "_assets"
    component_records = build_component_assets(asset_root)
    world_path = asset_root / "UIV4_world_town_gray.png"
    _world_asset(world_path)
    contract = load_contract(PACKAGE / "ui-master-spec.json")
    records: list[dict] = []
    for index, page in enumerate(contract.pages):
        builder = PageBuilder(page.name, asset_root, component_records, package_root)
        _build_page(builder, index, world_path)
        filename = f"{page.name}.png"
        builder.save(output_root / filename)
        records.append(
            {
                "index": index,
                "group": page.name,
                "file": filename,
                "size": [1920, 1080],
                "status": "v2_master" if index in V2_MASTER_PAGE_INDICES else "pending_visual_review",
                "sourceFamily": "approved_v2" if index in V2_MASTER_PAGE_INDICES else "phase_a_draft",
                "imageLayers": builder.image_layers,
                "textLayers": builder.text_layers,
            }
        )
    return records
