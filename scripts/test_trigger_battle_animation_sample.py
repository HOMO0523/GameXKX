import importlib.util
import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SCRIPT_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_trigger_battle_animation_sample.py"


class FakeBoard:
    def __init__(self):
        self.calls = []

    def open_command_menu_for_party_unit(self, party_index, menu_position, unit_position):
        self.calls.append(("open", party_index, menu_position, unit_position))
        return True

    def execute_basic_attack_action(self):
        self.calls.append(("attack",))
        return True

    def confirm_targeting_enemy(self, enemy_index):
        self.calls.append(("confirm", enemy_index))
        return True

    def click_card_in_hand(self, card_instance_id):
        self.calls.append(("click_card", str(card_instance_id)))
        return True

    def is_card_targeting_active(self):
        self.calls.append(("targeting",))
        return True

    def confirm_targeting_unit(self, unit_id):
        self.calls.append(("confirm_unit", str(unit_id)))
        return True


class TriggerBattleAnimationSampleTest(unittest.TestCase):
    def test_mcp_entrypoint_never_raises_system_exit(self):
        self.assertNotIn("raise SystemExit", SCRIPT_PATH.read_text(encoding="utf-8"))

    def test_drives_real_board_targeting_path_to_rooster(self):
        spec = importlib.util.spec_from_file_location("gamexxk_trigger_battle_animation_sample", SCRIPT_PATH)
        module = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = module
        spec.loader.exec_module(module)
        board = FakeBoard()

        result = module.trigger_basic_attack(board, enemy_index=1)

        self.assertTrue(result["ok"])
        self.assertEqual([call[0] for call in board.calls], ["open", "attack", "confirm"])
        self.assertEqual(board.calls[-1], ("confirm", 1))

    def test_plays_preferred_hero_attack_card_against_stable_rooster_id(self):
        spec = importlib.util.spec_from_file_location("gamexxk_trigger_battle_animation_sample", SCRIPT_PATH)
        module = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = module
        spec.loader.exec_module(module)
        board = FakeBoard()
        cards = [
            {"card_id": "Hero.FengShenBu", "instance_id": "CardRun.1.002", "owner_unit_id": "Player"},
            {"card_id": "Route.General.PoJiaTuCi", "instance_id": "CardRun.1.016", "owner_unit_id": "Player"},
        ]

        result = module.trigger_card_attack(board, cards, "Enemy.Rooster.P3")

        self.assertTrue(result["ok"])
        self.assertEqual(result["card_id"], "Route.General.PoJiaTuCi")
        self.assertEqual(
            board.calls,
            [
                ("click_card", "CardRun.1.016"),
                ("targeting",),
                ("confirm_unit", "Enemy.Rooster.P3"),
            ],
        )

    def test_falls_back_to_initial_hero_attack_when_route_attack_is_not_in_hand(self):
        spec = importlib.util.spec_from_file_location("gamexxk_trigger_battle_animation_sample", SCRIPT_PATH)
        module = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = module
        spec.loader.exec_module(module)
        board = FakeBoard()
        cards = [
            {"card_id": "Hero.HeYuZhan", "instance_id": "CardRun.1.001", "owner_unit_id": "Player"},
        ]

        result = module.trigger_card_attack(board, cards, "Enemy.Rooster.P3")

        self.assertTrue(result["ok"])
        self.assertEqual(result["card_id"], "Hero.HeYuZhan")
        self.assertEqual(board.calls[-1], ("confirm_unit", "Enemy.Rooster.P3"))


if __name__ == "__main__":
    unittest.main()
