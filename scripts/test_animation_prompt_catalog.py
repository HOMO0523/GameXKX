#!/usr/bin/env python3
"""Contract tests for the generated 34-unit, five-action prompt catalog."""

from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROMPT_ROOT = ROOT / "SourceAssets/AnimationProduction/prompts_v1"
MANIFEST = PROMPT_ROOT / "generation_manifest.json"
ACTIONS = {"idle", "attack", "hit", "buff", "death"}
HARD_PREFIX = (
    "\u6700\u9ad8\u4f18\u5148\u7ea7\u786c\u6027\u6784\u56fe"
    "\u9650\u5236\uff1a"
)


class AnimationPromptCatalogTests(unittest.TestCase):
    def test_manifest_has_five_actions_for_all_units(self) -> None:
        data = json.loads(MANIFEST.read_text(encoding="utf-8"))
        entries = data["entries"]
        self.assertEqual(170, len(entries))
        self.assertEqual(170, len({entry["asset_id"] for entry in entries}))
        by_unit: dict[str, set[str]] = {}
        for entry in entries:
            by_unit.setdefault(entry["unit_id"], set()).add(entry["action"])
        self.assertEqual(34, len(by_unit))
        self.assertTrue(all(actions == ACTIONS for actions in by_unit.values()))

    def test_every_prompt_starts_with_frame_hard_limit(self) -> None:
        data = json.loads(MANIFEST.read_text(encoding="utf-8"))
        for entry in data["entries"]:
            prompt_path = ROOT / entry["prompt_file"]
            prompt = prompt_path.read_text(encoding="utf-8")
            self.assertTrue(prompt.startswith(HARD_PREFIX), entry["asset_id"])
            self.assertIn("#FF00FF", prompt, entry["asset_id"])
            self.assertIn("1.10", prompt, entry["asset_id"])
            self.assertEqual(1, entry["max_submissions"])
            self.assertFalse(entry["automatic_retry"])

    def test_models_directions_and_budget_are_explicit(self) -> None:
        data = json.loads(MANIFEST.read_text(encoding="utf-8"))
        for entry in data["entries"]:
            expected_model = (
                "seedance2.0_vip"
                if entry["action"] == "attack"
                else "seedance1.5pro"
            )
            self.assertEqual(expected_model, entry["model"], entry["asset_id"])
            expected_facing = "left" if entry["side"] == "character" else "right"
            self.assertEqual(expected_facing, entry["facing"], entry["asset_id"])
            self.assertEqual("720p", entry["resolution"])
            self.assertEqual(5, entry["duration_seconds"])
        self.assertEqual(7820, data["unit_action_credit_total"])
        self.assertFalse(data["budget_preflight"]["can_submit_all_five_actions"])


if __name__ == "__main__":
    unittest.main()
