import ast
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
IMPORTER = ROOT / "Content" / "Python" / "gamexxk_import_battle_status_icons.py"


class BattleStatusIconImportTest(unittest.TestCase):
    def test_appended_status_sources_and_destinations_are_explicit(self) -> None:
        tree = ast.parse(IMPORTER.read_text(encoding="utf-8"))
        text = IMPORTER.read_text(encoding="utf-8")
        expected = {
            "T_BattleStatus_MedicineHerbs": "status_glyph_medicine_draft_v1_alpha.png",
            "T_BattleStatus_WeakBrokenBlade": "status_glyph_weak_drooping_broken_blade_draft_v1_alpha.png",
            "T_BattleStatus_WealthCoin": "status_glyph_wealth_square_coin_draft_v1_alpha.png",
            "T_BattleStatus_RageFlame": "status_glyph_rage_horn_flame_draft_v1_alpha.png",
            "T_BattleStatus_PreyTargetEye": "status_glyph_prey_ink_target_draft_v1_alpha.png",
            "T_BattleStatus_ChargeSpiralHorn": "status_glyph_charge_spiral_horn_draft_v1_alpha.png",
            "T_BattleStatus_CounterHookBlade": "status_glyph_counter_return_hook_blade_draft_v1_alpha.png",
        }
        self.assertIsNotNone(tree)
        for destination, source in expected.items():
            self.assertIn(destination, text)
            self.assertIn(source, text)
            self.assertTrue((ROOT / "SourceArt" / "Generated" / "Draft" / "V1" / "Status" / source).is_file())
        self.assertIn("--route-only", text)


if __name__ == "__main__":
    unittest.main()
