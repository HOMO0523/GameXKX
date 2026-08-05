"""Build the manifest for non-destructive GameXXK master-UI family corrections."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


TARGET_PAGES = (
    "01_主菜单",
    "02_城镇HUD",
    "03_主角背包",
    "04_伙伴编队",
    "05_图鉴",
    "06_任务日志",
    "08_路线图",
    "09_路线事件",
    "10_战斗HUD",
    "11_战斗奖励结算",
    "12_系统菜单",
    "13_主角背包_物品选中",
    "14_伙伴编队_角色选中",
    "15_图鉴_怪物选中",
    "16_路线图_节点选中",
    "17_战斗HUD_卡牌选中目标",
)

FAMILIES = {
    "character": [
        "03_主角背包",
        "04_伙伴编队",
        "05_图鉴",
        "06_任务日志",
        "13_主角背包_物品选中",
        "14_伙伴编队_角色选中",
        "15_图鉴_怪物选中",
    ],
    "route": ["08_路线图", "09_路线事件", "16_路线图_节点选中"],
    "battle": ["10_战斗HUD", "11_战斗奖励结算", "17_战斗HUD_卡牌选中目标"],
    "menu_hud": ["01_主菜单", "02_城镇HUD", "12_系统菜单"],
}

LEGACY_GLOBAL_TEXT = [
    "主角  Lv. 1",
    "经验  0 / 100     战力 33",
    "铜钱 10,000      青玉 2,000      金锭 500",
]

BACKPACK_GLOBAL_TEXT = ["Lv. 1", "0 / 100", "33", "10,000", "2,000", "500"]


def load_json(path: Path) -> dict[str, Any]:
    if not path.is_file():
        raise FileNotFoundError(path)
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_json(path: Path, payload: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def text_top_left(content: str, x: int, y: int) -> dict[str, object]:
    return {"content": content, "mode": "topLeft", "target": [x, y], "justification": "left"}


def text_center(content: str, x: int, y: int) -> dict[str, object]:
    return {"content": content, "mode": "center", "target": [x, y], "justification": "center"}


def cluster(anchor: str, members: list[str], x: int, y: int) -> dict[str, object]:
    return {"anchorGroup": anchor, "members": members, "targetCenter": [x, y]}


def page_configuration(name: str) -> dict[str, object]:
    common: dict[str, object] = {
        "name": name,
        "hiddenTextContents": [],
        "textRules": [],
        "clusterRules": [],
    }
    if name == "01_主菜单":
        common.update(
            family="menu_hud",
            preset="main_menu",
            hideGroups=["10_World", "11_WorldOverlay", "20_Shell"],
            preserveGroups=["30_Menu", "70_RuntimeText"],
            duplicateShopGlobal=False,
            clusterRules=[cluster("30_Menu", ["30_Menu"], 960, 620)],
            textRules=[
                text_center("行旅异闻", 960, 255),
                text_center("山川入墨，众生入局", 960, 320),
                text_center("继续旅程", 960, 465),
                text_center("新的旅程", 960, 565),
                text_center("设置", 960, 665),
                text_center("退出", 960, 765),
            ],
        )
    elif name == "02_城镇HUD":
        common.update(
            family="menu_hud",
            preset="town_hud",
            hideGroups=["10_World", "20_Shell", "21_Navigation"],
            preserveGroups=["30_Context", "70_RuntimeText"],
            duplicateShopGlobal=True,
            hiddenTextContents=LEGACY_GLOBAL_TEXT,
            clusterRules=[cluster("30_Context", ["30_Context"], 560, 875)],
            textRules=[
                text_top_left("青山镇 · 客栈前街", 260, 790),
                text_top_left("F  与掌柜交谈     城镇按钮进入路线图", 260, 835),
            ],
        )
    elif name == "03_主角背包":
        common.update(
            family="character",
            preset="town_full",
            hideGroups=["10_ApprovedV2Shell", "15_HudContent"],
            preserveGroups=[
                "20_Tabs",
                "30_Character",
                "31_EquipmentFrames",
                "32_EquipmentIcons",
                "40_InventorySlots",
                "41_InventoryItems",
                "42_InventoryScrollbar",
                "70_RuntimeText",
            ],
            duplicateShopGlobal=True,
            hiddenTextContents=BACKPACK_GLOBAL_TEXT,
            textRules=[text_top_left("主角", 390, 215)],
        )
    elif name in ("04_伙伴编队", "14_伙伴编队_角色选中"):
        common.update(
            family="character",
            preset="town_full",
            hideGroups=["10_World", "11_WorldOverlay", "20_Shell", "21_Navigation"],
            preserveGroups=["30_Cards", "31_Portraits", "32_FormationSlots", "40_Actions", "70_RuntimeText"],
            duplicateShopGlobal=True,
            hiddenTextContents=LEGACY_GLOBAL_TEXT,
            clusterRules=[
                cluster("30_Cards", ["30_Cards", "31_Portraits", "32_FormationSlots"], 960, 535),
                cluster("40_Actions", ["40_Actions"], 1460, 880),
            ],
            textRules=[
                text_top_left("伙伴编队", 390, 215),
                text_top_left("拖动卡片调整前后排；主角入口不再与伙伴混淆", 565, 225),
                text_center("保存编队", 1460, 880),
            ],
        )
    elif name in ("05_图鉴", "15_图鉴_怪物选中"):
        preserved = ["30_MonsterCards", "31_Portraits", "70_RuntimeText"]
        if name == "15_图鉴_怪物选中":
            preserved += ["40_Detail", "41_DetailPortrait"]
        common.update(
            family="character",
            preset="town_full",
            hideGroups=["10_World", "11_WorldOverlay", "20_Shell", "21_Navigation"],
            preserveGroups=preserved,
            duplicateShopGlobal=True,
            hiddenTextContents=LEGACY_GLOBAL_TEXT,
            clusterRules=[cluster("30_MonsterCards", ["30_MonsterCards", "31_Portraits"], 850, 510)],
            textRules=[
                text_top_left("图鉴", 390, 215),
                text_top_left("怪物卡片、掉落与已发现状态", 520, 225),
            ],
        )
    elif name == "06_任务日志":
        common.update(
            family="character",
            preset="town_full",
            hideGroups=["10_World", "11_WorldOverlay", "20_Shell", "21_Navigation"],
            preserveGroups=["30_QuestList", "31_QuestDetail", "40_Actions", "70_RuntimeText"],
            duplicateShopGlobal=True,
            hiddenTextContents=LEGACY_GLOBAL_TEXT,
            clusterRules=[
                cluster("30_QuestList", ["30_QuestList"], 650, 560),
                cluster("31_QuestDetail", ["31_QuestDetail", "40_Actions"], 1220, 560),
            ],
            textRules=[
                text_top_left("任务日志", 390, 215),
                text_top_left("当前、可接取、已完成", 565, 225),
                text_center("追踪任务", 1460, 880),
            ],
        )
    elif name in ("08_路线图", "16_路线图_节点选中"):
        preserved = ["30_Route", "70_RuntimeText"]
        if name == "16_路线图_节点选中":
            preserved.append("40_Selection")
        common.update(
            family="route",
            preset="route_full",
            hideGroups=["10_World", "11_WorldOverlay", "20_Shell"],
            preserveGroups=preserved,
            duplicateShopGlobal=False,
            clusterRules=[cluster("30_Route", ["30_Route"], 960, 580)],
            textRules=[
                text_top_left("山路路线", 330, 190),
                text_top_left("选择下一个节点：战斗、事件、商店或休整", 520, 205),
            ] + ([text_center("进入节点", 1450, 420)] if name == "16_路线图_节点选中" else []),
        )
    elif name == "09_路线事件":
        common.update(
            family="route",
            preset="route_full",
            hideGroups=["10_World", "11_WorldOverlay", "20_Shell"],
            preserveGroups=["30_Illustration", "40_Choices", "70_RuntimeText"],
            duplicateShopGlobal=False,
            clusterRules=[
                cluster("30_Illustration", ["30_Illustration"], 960, 430),
                cluster("40_Choices", ["40_Choices"], 960, 735),
            ],
            textRules=[
                text_top_left("山泉旁的旅人", 330, 190),
                text_top_left("一名旅人守着清泉，似乎在等待同行者。", 330, 255),
                text_center("分享干粮", 960, 640),
                text_center("询问山路", 960, 735),
                text_center("悄悄离开", 960, 830),
            ],
        )
    elif name in ("10_战斗HUD", "17_战斗HUD_卡牌选中目标"):
        preserved = ["30_Characters", "31_FootBars", "40_HandCards", "70_RuntimeText"]
        if name == "17_战斗HUD_卡牌选中目标":
            preserved.append("50_Targeting")
        common.update(
            family="battle",
            preset="battle",
            hideGroups=["10_World"],
            preserveGroups=preserved,
            duplicateShopGlobal=False,
            clusterRules=[
                cluster("30_Characters", ["30_Characters", "31_FootBars"], 960, 560),
                cluster("40_HandCards", ["40_HandCards"], 960, 900),
            ],
            textRules=[
                text_top_left("回合 1", 70, 55),
                {"content": "能量 3 / 3", "mode": "topRight", "target": [1850, 55], "justification": "right"},
            ] + ([text_center("选择目标", 960, 760)] if name == "17_战斗HUD_卡牌选中目标" else []),
        )
    elif name == "11_战斗奖励结算":
        common.update(
            family="battle",
            preset="reward",
            hideGroups=["10_World", "11_WorldOverlay", "20_Shell"],
            preserveGroups=["30_Rewards", "40_Actions", "70_RuntimeText"],
            duplicateShopGlobal=False,
            clusterRules=[
                cluster("30_Rewards", ["30_Rewards"], 960, 530),
                cluster("40_Actions", ["40_Actions"], 960, 850),
            ],
            textRules=[
                text_center("战斗胜利", 960, 235),
                text_center("选择一项奖励", 960, 300),
                text_center("确认奖励", 960, 850),
            ],
        )
    elif name == "12_系统菜单":
        common.update(
            family="menu_hud",
            preset="system_menu",
            hideGroups=["10_World", "11_WorldOverlay", "20_Shell"],
            preserveGroups=["30_Menu", "70_RuntimeText"],
            duplicateShopGlobal=False,
            clusterRules=[cluster("30_Menu", ["30_Menu"], 960, 620)],
            textRules=[
                text_center("系统菜单", 960, 270),
                text_center("继续游戏", 960, 465),
                text_center("音量与显示", 960, 565),
                text_center("操作设置", 960, 665),
                text_center("返回主菜单", 960, 765),
            ],
        )
    elif name == "13_主角背包_物品选中":
        common.update(
            family="character",
            preset="town_full",
            hideGroups=["10_World", "11_WorldOverlay", "20_Shell", "21_Navigation"],
            preserveGroups=[
                "22_Tabs",
                "30_Character",
                "31_Equipment",
                "35_Grid",
                "40_Actions",
                "45_Selection",
                "70_RuntimeText",
            ],
            duplicateShopGlobal=True,
            hiddenTextContents=LEGACY_GLOBAL_TEXT,
            textRules=[
                text_top_left("主角", 390, 215),
                text_top_left("当前最终 Idle · 保持 512×512 透明画布比例", 520, 225),
                text_center("使用", 1460, 880),
                text_center("分解", 1515, 775),
            ],
        )
    else:
        raise KeyError(name)
    return common


def build_manifest(master_path: Path, shell_path: Path, output_path: Path) -> dict[str, object]:
    master = load_json(master_path)
    shell = load_json(shell_path)
    pages_by_name = {page["group"]: page for page in master.get("pages", [])}
    if set(TARGET_PAGES) - set(pages_by_name):
        raise ValueError(f"master manifest is missing pages: {sorted(set(TARGET_PAGES) - set(pages_by_name))}")

    shell_root = shell_path.resolve().parent
    component_by_name = {record["name"]: record for record in shell["components"]}
    asset_records = {
        "background": {
            "name": "00_城镇背景",
            "path": str((shell_root / shell["background"]["file"]).resolve()),
            "box": [0, 0, 1920, 1080],
        }
    }
    component_keys = {
        "identity": "01_主角身份条",
        "currency": "02_顶部铜钱条",
        "nav_backpack": "03_导航圆底_背包",
        "nav_companion": "04_导航圆底_伙伴",
        "nav_codex": "05_导航圆底_图鉴",
        "nav_task": "06_导航圆底_任务",
        "nav_route": "07_导航圆底_路线",
        "main_panel": "08_中央商店窗口",
    }
    for key, component_name in component_keys.items():
        record = component_by_name[component_name]
        asset_records[key] = {
            "name": component_name,
            "path": str((shell_root / record["file"]).resolve()),
            "box": list(record["box"]),
        }
    for asset in asset_records.values():
        if not Path(str(asset["path"])).is_file():
            raise FileNotFoundError(asset["path"])

    presets = {
        "town_full": [
            {**asset_records["background"]},
            {**asset_records["identity"]},
            {**asset_records["currency"]},
            {**asset_records["nav_backpack"]},
            {**asset_records["nav_companion"]},
            {**asset_records["nav_codex"]},
            {**asset_records["nav_task"]},
            {**asset_records["nav_route"]},
            {**asset_records["main_panel"]},
        ],
        "town_hud": [
            {**asset_records["background"]},
            {**asset_records["identity"]},
            {**asset_records["currency"]},
            {**asset_records["nav_backpack"]},
            {**asset_records["nav_companion"]},
            {**asset_records["nav_codex"]},
            {**asset_records["nav_task"]},
            {**asset_records["nav_route"]},
        ],
        "route_full": [
            {**asset_records["background"]},
            {**asset_records["main_panel"], "box": [230, 150, 1460, 800]},
        ],
        "battle": [{**asset_records["background"]}],
        "reward": [
            {**asset_records["background"]},
            {**asset_records["main_panel"], "box": [480, 140, 960, 820]},
        ],
        "main_menu": [
            {**asset_records["background"]},
            {**asset_records["main_panel"], "box": [580, 130, 760, 820]},
        ],
        "system_menu": [
            {**asset_records["background"]},
            {**asset_records["main_panel"], "box": [660, 170, 600, 740]},
        ],
    }

    page_records = []
    for name in TARGET_PAGES:
        page = pages_by_name[name]
        config = page_configuration(name)
        origin = list(page["origin"])
        if len(origin) != 2 or page.get("size") != [1920, 1080]:
            raise ValueError(f"unexpected page geometry for {name}: {page}")
        config["origin"] = origin
        config["size"] = [1920, 1080]
        config["assets"] = presets[str(config["preset"])]
        for asset in config["assets"]:
            left, top, width, height = (int(value) for value in asset["box"])
            if left < 0 or top < 0 or width <= 0 or height <= 0 or left + width > 1920 or top + height > 1080:
                raise ValueError(f"asset box exceeds page for {name}: {asset}")
        page_records.append(config)

    state_pairs = [
        ["03_主角背包", "13_主角背包_物品选中"],
        ["04_伙伴编队", "14_伙伴编队_角色选中"],
        ["05_图鉴", "15_图鉴_怪物选中"],
        ["08_路线图", "16_路线图_节点选中"],
        ["10_战斗HUD", "17_战斗HUD_卡牌选中目标"],
    ]
    pages_lookup = {record["name"]: record for record in page_records}
    for base_name, state_name in state_pairs:
        if pages_lookup[base_name]["preset"] != pages_lookup[state_name]["preset"]:
            raise ValueError(f"base/state preset mismatch: {base_name}, {state_name}")
        if pages_lookup[base_name]["assets"] != pages_lookup[state_name]["assets"]:
            raise ValueError(f"base/state asset mismatch: {base_name}, {state_name}")

    manifest = {
        "version": 1,
        "sourceMasterManifest": str(master_path.resolve()),
        "sourceShellManifest": str(shell_path.resolve()),
        "shopReference": {"name": "07_商店交易", "origin": [4080, 1200], "globalGroup": "20_GlobalShell"},
        "targetCanvas": [1920, 1080],
        "families": FAMILIES,
        "statePairs": state_pairs,
        "pages": page_records,
    }
    write_json(output_path, manifest)
    return {"pageCount": len(page_records), "familyCount": len(FAMILIES), "validation": "ok", "output": str(output_path.resolve())}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--master-manifest", type=Path, required=True)
    parser.add_argument("--shell-manifest", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = build_manifest(args.master_manifest, args.shell_manifest, args.output)
    print(json.dumps(report, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
