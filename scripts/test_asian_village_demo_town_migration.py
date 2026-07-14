import importlib.util
import sys
import types
import unittest
from pathlib import Path
from unittest.mock import patch


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_migrate_asian_village_demo_town.py"


def migration_module():
    spec = importlib.util.spec_from_file_location("gamexxk_migrate_asian_village_demo_town", MODULE_PATH)
    if spec is None or spec.loader is None:
        raise AssertionError(f"town migration module is unavailable: {MODULE_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class AsianVillageDemoTownMigrationContractTests(unittest.TestCase):
    def test_gate_rotator_puts_the_configured_angle_on_yaw(self):
        class FakeRotator:
            def __init__(self, roll=0.0, pitch=0.0, yaw=0.0):
                self.roll = float(roll)
                self.pitch = float(pitch)
                self.yaw = float(yaw)

        fake_unreal = types.ModuleType("unreal")
        fake_unreal.Rotator = FakeRotator
        with patch.dict(sys.modules, {"unreal": fake_unreal}):
            module = migration_module()

        rotation = module._rotator(module.GATE_VISUAL["yaw"])
        self.assertEqual(rotation.pitch, 0.0)
        self.assertEqual(rotation.yaw, module.GATE_VISUAL["yaw"])

    def test_blueprint_class_path_comparison_accepts_the_exact_generated_class(self):
        module = migration_module()

        expected = "/Game/GameXXK/Characters/Follower/BP_NpcCharacter.BP_NpcCharacter_C"
        self.assertTrue(module.class_paths_match(expected, expected))
        self.assertFalse(module.class_paths_match(
            "/Game/GameXXK/Characters/Merchant/BP_MerchantCharacter.BP_MerchantCharacter_C",
            expected,
        ))

    def test_known_stock_player_start_is_the_only_removal_candidate(self):
        module = migration_module()

        self.assertTrue(module.is_known_stock_player_start(
            "Player Start", (15740.0, 5240.0, 1162.000732421875)))
        self.assertFalse(module.is_known_stock_player_start(
            "PlayerStart_QingshanAsianVillage", (20170.0, 1700.0, 1592.000732)))
        self.assertFalse(module.is_known_stock_player_start(
            "Player Start", (15741.0, 5240.0, 1162.000732421875)))

    def test_target_only_contract_preserves_user_demo_transforms(self):
        module = migration_module()

        self.assertEqual(module.TARGET_MAP, "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo")
        self.assertEqual(module.QUEST_NPC["location"], (20410.0, 4580.0, 1558.0))
        self.assertEqual(module.MERCHANT_NPC["location"], (20360.0, 5910.0, 1598.0))
        self.assertEqual(module.GATE_VISUAL["location"], (19930.0, 5240.0, 1610.0))
        self.assertEqual(module.GATE_VISUAL["yaw"], 80.000061)
        self.assertEqual(module.PLAYER_START["location"], (20170.0, 1700.0, 1592.000732))
        self.assertNotIn("Asian_Village_Demo", module.SOURCE_MAP)

    def test_gate_keeps_visual_and_f_interaction_as_separate_actors(self):
        module = migration_module()

        self.assertEqual(module.GATE_VISUAL["mesh"], "/Game/Asian_Village/meshes/props/SM_statue_01.SM_statue_01")
        self.assertEqual(module.GATE_PROXY["class_path"], "/Script/GameXXK.GameXXKTownExitActor")
        self.assertEqual(module.GATE_PROXY["label"], "QingshanInn_TownExit")
        self.assertEqual(module.GATE_PROXY["location"], module.GATE_VISUAL["location"])
        self.assertEqual(module.GATE_PROXY["yaw"], module.GATE_VISUAL["yaw"])

    def test_gate_orientation_repair_is_scoped_to_the_two_gate_actors(self):
        module = migration_module()

        self.assertTrue(hasattr(module, "gate_orientation_specs"))
        self.assertEqual(
            [spec["label"] for spec in module.gate_orientation_specs()],
            ["QingshanTown_GateVisual", "QingshanInn_TownExit"],
        )

    def test_gate_orientation_repair_uses_the_ue_vector_equals_signature(self):
        source = MODULE_PATH.read_text(encoding="utf-8")

        self.assertNotIn("location_after.equals(location_before, 0.01)", source)

    def test_required_actor_specs_are_unique_and_idempotent(self):
        module = migration_module()

        specs = module.required_actor_specs()
        labels = [spec["label"] for spec in specs]
        self.assertEqual(labels, [
            "QingshanTown_QuestNpc",
            "QingshanTown_MerchantNpc",
            "QingshanTown_GateVisual",
            "QingshanInn_TownExit",
            "PlayerStart_QingshanAsianVillage",
        ])
        self.assertEqual(len(labels), len(set(labels)))
        self.assertEqual(specs[0]["npc_role"], "Quest")
        self.assertEqual(specs[1]["npc_role"], "Merchant")


if __name__ == "__main__":
    unittest.main()
