import json
import unittest

import export_game_analysis_html as analysis


class GameAnalysisHtmlTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = analysis.build_data()

    def test_uses_certified_runtime_dimensions(self):
        self.assertEqual(self.data["meta"]["card_count"], 173)
        self.assertEqual(self.data["meta"]["intent_cases"], 351)
        self.assertEqual(self.data["meta"]["formations"], 189)
        self.assertEqual(self.data["meta"]["save_version"], 36)
        self.assertEqual(self.data["stage_end"]["raw_phase_hp"], 11178)
        self.assertEqual([unit["phases"] for unit in self.data["stage_end"]["units"]], [1, 3, 1])

    def test_each_recommendation_has_one_legal_sixteen_card_loadout(self):
        pending = set(self.data["meta"]["pending"])
        self.assertEqual(len(self.data["builds"]), 6)
        for build in self.data["builds"]:
            cards = build["hero_cards"] + build["partner_cards"] + build["npc_cards_detail"]
            self.assertEqual(len(build["hero_cards"]), 8, build["name"])
            self.assertEqual(len(build["partner_cards"]), 5, build["name"])
            self.assertEqual(len(build["npc_cards_detail"]), 3, build["name"])
            self.assertEqual(len({card["id"] for card in cards}), 16, build["name"])
            self.assertFalse({card["id"] for card in cards} & pending, build["name"])
            self.assertEqual(build["timing"]["completion"], 1.0, build["name"])
            self.assertGreater(build["timing"]["mana_floor_pct"], 0, build["name"])

    def test_phase_packet_clamping_is_accounted_for(self):
        saw_waste = False
        for build in self.data["builds"]:
            self.assertEqual(build["cycle_damage"], sum(step["damage"] for step in build["steps"]))
            self.assertEqual(build["raw_cycle_damage"], build["cycle_damage"] + build["phase_waste"])
            self.assertLessEqual(build["conservative_dpr"], build["dpr"])
            self.assertGreater(build["turns_to_stage_end"], 0)
            saw_waste |= build["phase_waste"] > 0
        self.assertTrue(saw_waste)
        guard = next(build for build in self.data["builds"] if build["id"] == "guard_release")
        self.assertGreater(guard["phase_waste"], 0)
        self.assertLess(guard["cycle_damage"], guard["raw_cycle_damage"])
        healer = next(build for build in self.data["builds"] if build["id"] == "healer_toxic")
        poison_boundary = next(step for step in healer["steps"] if step["name"] == "中毒边界")
        self.assertAlmostEqual(poison_boundary["raw_damage"], healer["timing"]["avg"] * 200, delta=1)

    def test_enemy_pressure_covers_every_white_ape_phase(self):
        for build in self.data["builds"]:
            self.assertEqual([row["phase"] for row in build["pressure"]], [1, 2, 3])
            self.assertTrue(all(row["incoming_before_armor"] > 0 for row in build["pressure"]))

    def test_equipment_resource_hooks_match_runtime_owners(self):
        self.assertEqual(self.data["meta"]["samples_per_npc_loadout"], 4000)
        builds = {build["id"]: build for build in self.data["builds"]}
        self.assertGreater(builds["guard_release"]["timing"]["qingnang_triggers"], 0)
        self.assertGreater(builds["healer_toxic"]["timing"]["qingnang_triggers"], 0)
        self.assertGreater(builds["formation_assault"]["timing"]["shanhe_triggers"], 0)
        self.assertGreater(builds["sorcerer_fire"]["timing"]["shanhe_triggers"], 0)
        self.assertEqual(builds["blade_momentum"]["sets"]["npc"], "玄甲6")
        self.assertEqual(builds["blade_momentum"]["timing"]["qingnang_triggers"], 0)
        self.assertEqual(builds["hunter_heavy"]["sets"]["hero"], "蚀骨6")
        self.assertEqual(builds["hunter_heavy"]["sets"]["partner"], "追风6")
        self.assertEqual(builds["hunter_heavy"]["timing"]["qingnang_triggers"], 0)
        self.assertEqual(builds["hunter_heavy"]["timing"]["shanhe_triggers"], 0)

    def test_serialized_page_has_no_stale_implementation_claims(self):
        payload = json.dumps(self.data, ensure_ascii=False, separators=(",", ":"))
        page = analysis.HTML.replace("__DATA__", payload)
        self.assertNotIn("__DATA__", page)
        self.assertNotIn("消费者未实装", page)
        self.assertNotIn("尚未进入完整战斗模拟", page)
        self.assertIn("逐伤害包阶段截断", page)


if __name__ == "__main__":
    unittest.main()
