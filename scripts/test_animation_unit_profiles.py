#!/usr/bin/env python3
"""Contract tests for the full battle-animation unit profile catalog."""

from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "SourceAssets/AnimationProduction/unit_profiles.json"


class AnimationUnitProfileTests(unittest.TestCase):
    def test_catalog_has_every_final_unit(self) -> None:
        data = json.loads(CATALOG.read_text(encoding="utf-8"))
        units = data["units"]
        self.assertEqual(34, len(units))
        self.assertEqual(34, len({unit["id"] for unit in units}))
        self.assertEqual(13, sum(unit["side"] == "character" for unit in units))
        self.assertEqual(21, sum(unit["side"] == "enemy" for unit in units))

    def test_each_profile_is_generation_ready(self) -> None:
        data = json.loads(CATALOG.read_text(encoding="utf-8"))
        required = {
            "personality",
            "silhouette",
            "signature_props",
            "idle_behavior",
            "signature_attack",
            "hit_behavior",
            "buff_behavior",
            "death_behavior",
        }
        for unit in data["units"]:
            self.assertEqual(
                "left" if unit["side"] == "character" else "right",
                unit["facing"],
                unit["id"],
            )
            self.assertTrue((ROOT / unit["source_1600"]).is_file(), unit["id"])
            self.assertTrue(required.issubset(unit), unit["id"])
            for field in required:
                self.assertTrue(unit[field].strip(), f"{unit['id']}:{field}")

    def test_single_submission_policy_is_explicit(self) -> None:
        data = json.loads(CATALOG.read_text(encoding="utf-8"))
        self.assertEqual(1, data["max_submissions_per_unit_action"])
        self.assertFalse(data["automatic_retry"])
        self.assertEqual(1600, data["source_canvas_px"])
        self.assertEqual("#FF00FF", data["source_background"])


if __name__ == "__main__":
    unittest.main()
