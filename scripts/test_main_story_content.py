"""Contracts for the authored main-story content consumed by the game."""
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
CONTENT = ROOT / "SourceAssets/Narrative/MainStory/campaign.json"


class MainStoryContentTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(CONTENT.read_text(encoding="utf-8")) if CONTENT.is_file() else None
        cls.nodes = {n["id"]: n for c in cls.data["chapters"] for n in c["nodes"]} if cls.data else {}

    def setUp(self):
        if self.data is None and self._testMethodName != "test_authored_campaign_is_present":
            self.skipTest("Campaign not authored yet")

    def test_authored_campaign_is_present(self):
        self.assertTrue(CONTENT.is_file(), "The complete authored six-chapter campaign is required")

    def test_complete_scope_and_rewards(self):
        chapters = self.data["chapters"]
        self.assertEqual([len(c["nodes"]) for c in chapters], [7, 10, 8, 13, 12, 11])
        self.assertEqual(len(self.nodes), 61)
        self.assertEqual([c["stage_number"] for c in chapters], list(range(1, 7)))
        self.assertEqual(sum(n["reward"]["gold"] for n in self.nodes.values()), 6100000)
        self.assertEqual(sum(n["reward"]["advanced_boxes"] for n in self.nodes.values()), 60)
        self.assertEqual(sum(n["reward"]["normal_boxes"] for n in self.nodes.values()), 60)
        self.assertEqual(sum(n["kind"] == "JourneyBattle" for n in self.nodes.values()), 4)
        self.assertEqual(sum(n["kind"].startswith("Journey") for n in self.nodes.values()), 6)
        for chapter in chapters:
            for node in chapter["nodes"]:
                self.assertEqual(node["reward"]["box_level"], chapter["stage_number"] * 5)
                self.assertEqual(node["reward"]["gold"], 100000)

    def test_graph_is_reachable_and_has_no_dangling_dependencies(self):
        for chapter in self.data["chapters"]:
            nodes = {n["id"]: n for n in chapter["nodes"]}
            self.assertIn(chapter["mainline_end"], nodes)
            complete = set()
            while True:
                available = {key for key, n in nodes.items() if
                    set(n["requires_all"]).issubset(complete)
                    and (not n["requires_any"] or set(n["requires_any"]) & complete)}
                following = complete | available
                if following == complete:
                    break
                complete = following
            self.assertEqual(complete, set(nodes), chapter["id"])
            for n in nodes.values():
                for parent in n["requires_all"] + n["requires_any"]:
                    self.assertIn(parent, nodes)
                    self.assertNotEqual(parent, n["id"])
            visiting, visited = set(), set()
            def visit(key):
                self.assertNotIn(key, visiting, f"cycle at {key}")
                if key in visited:
                    return
                visiting.add(key)
                for parent in nodes[key]["requires_all"] + nodes[key]["requires_any"]:
                    visit(parent)
                visiting.remove(key)
                visited.add(key)
            for key in nodes:
                visit(key)

    def test_authored_dialogue_investigation_and_art_are_complete(self):
        actors = self.data["characters"]
        self.assertEqual(actors["you_bai"]["name"], "幽白")
        self.assertEqual(actors["qiong_yao_er"]["name"], "琼幺儿")
        images = set()
        for node in self.nodes.values():
            with self.subTest(node=node["id"]):
                self.assertGreaterEqual(len(node["lines"]), 4)
                self.assertTrue(node["summary"] and node["objective"] and node["result"])
                self.assertEqual(len(node["hints"]), 3)
                self.assertGreaterEqual(len(node["art"]["scene"]), 45)
                self.assertGreaterEqual(len(node["art"]["facts"]), 2)
                self.assertEqual(node["art"]["display_phase"], "story_preview_and_result")
                self.assertEqual(node["art"]["style_version"], "clean_ink_v2")
                self.assertEqual(node["art"]["node_id"], node["id"])
                self.assertNotIn(node["art"]["file"], images)
                images.add(node["art"]["file"])
                for speaker, line in node["lines"]:
                    self.assertIn(speaker, actors)
                    self.assertTrue(line.strip())
                for actor in node["art"]["cast"]:
                    self.assertIn(actor, actors)
                if node["kind"] in ("Investigation", "JourneyInvestigation"):
                    self.assertGreaterEqual(len(node["options"]), 2)
                    self.assertEqual(sum(o["correct"] for o in node["options"]), 1)
                    for option in node["options"]:
                        self.assertTrue(option["feedback"])
                else:
                    self.assertEqual(node["options"], [])
        self.assertEqual(len(images), 61)


if __name__ == "__main__":
    unittest.main()
