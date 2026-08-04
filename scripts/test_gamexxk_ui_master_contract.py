import json
import unittest
from pathlib import Path

from scripts.gamexxk_ui_master_contract import load_contract, validate_source_lock


ROOT = Path(__file__).resolve().parents[1]
PACKAGE = ROOT / "SourceArt/UI/PSD/gamexxk-v4/ui-master"

EXPECTED = [
    "00_公共组件",
    "01_主菜单",
    "02_城镇HUD",
    "03_主角背包",
    "04_伙伴编队",
    "05_图鉴",
    "06_任务日志",
    "07_商店交易",
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
]


class GameXXKUiMasterContractTests(unittest.TestCase):
    def test_contract_has_exact_grid_and_page_roster(self):
        contract = load_contract(PACKAGE / "ui-master-spec.json")
        self.assertEqual((10080, 4680), contract.master_size)
        self.assertEqual((1920, 1080), contract.page_size)
        self.assertEqual(5, contract.columns)
        self.assertEqual(120, contract.gap)
        self.assertEqual(EXPECTED, [page.name for page in contract.pages])
        self.assertEqual((0, 0), contract.page_origin(0))
        self.assertEqual((8160, 0), contract.page_origin(4))
        self.assertEqual((0, 1200), contract.page_origin(5))

    def test_source_lock_accepts_final_idle_and_rejects_retired_portraits(self):
        result = validate_source_lock(PACKAGE / "source-lock.json", ROOT)
        self.assertTrue(result["ok"], result)
        lock = json.loads(
            (PACKAGE / "source-lock.json").read_text(encoding="utf-8")
        )
        self.assertEqual(
            "SourceAssets/AnimationProcessing/Production/character_00_hero_idle/frames/frame_0000.png",
            lock["heroIdle"]["path"],
        )
        self.assertEqual(
            "SourceAssets/AnimationProcessing/Production/character_01_blade_idle/frames/frame_0000.png",
            lock["partnerIdle"]["path"],
        )
        self.assertEqual(
            "SourceAssets/AnimationProcessing/Production/enemy_01_rooster_idle/frames/frame_0000.png",
            lock["monsterIdle"]["path"],
        )
        self.assertNotIn(
            "PartyDeck/card-portraits/generated", lock["heroIdle"]["path"]
        )


if __name__ == "__main__":
    unittest.main()
