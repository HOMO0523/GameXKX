#!/usr/bin/env python3
"""Contract tests for the paid seven-asset Seedance pilot."""

from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PILOT_ROOT = ROOT / "SourceAssets/AnimationProduction/pilot_v1"
MANIFEST = PILOT_ROOT / "pilot_manifest.json"
LEDGER = PILOT_ROOT / "credit_ledger.json"
UNIT_PROMPTS = (
    "hero_attack.txt",
    "hero_hit.txt",
    "rooster_attack.txt",
    "rooster_hit.txt",
)


class SevenAssetPilotTests(unittest.TestCase):
    def test_manifest_contract(self) -> None:
        data = json.loads(MANIFEST.read_text(encoding="utf-8"))
        assets = data["assets"]
        self.assertEqual(7, len(assets))
        self.assertEqual(7, len({item["id"] for item in assets}))
        self.assertEqual(340, sum(item["expected_credits"] for item in assets))
        self.assertEqual(565, data["retry_credit_cap"])
        for item in assets:
            self.assertEqual("720p", item["resolution"])
            self.assertEqual(5, item["duration_seconds"])
            expected_model = (
                "seedance2.0_vip"
                if item["id"] in {"hero_attack", "rooster_attack"}
                else "seedance1.5pro"
            )
            self.assertEqual(expected_model, item["model"])
            self.assertTrue((ROOT / item["first_frame"]).is_file())
            self.assertTrue((ROOT / item["last_frame"]).is_file())
            self.assertTrue((ROOT / item["prompt_file"]).is_file())

    def test_credit_ledger_contract(self) -> None:
        data = json.loads(LEDGER.read_text(encoding="utf-8"))
        self.assertEqual(7145, data["budget_baseline"])
        self.assertEqual(6580, data["production_budget"])
        self.assertEqual(565, data["retry_credit_cap"])
        self.assertLessEqual(
            data["rejected_credit_spend"], data["retry_credit_cap"]
        )
        self.assertEqual(
            data["retry_credit_cap"] - data["rejected_credit_spend"],
            data["retry_credit_remaining"],
        )
        accounted = (
            data["accepted_credit_spend"]
            + data["rejected_credit_spend"]
            + data["pending_credit_spend"]
            + data["review_pending_credit_spend"]
        )
        self.assertEqual(
            sum(item["credit_count"] for item in data["submissions"]),
            accounted,
        )
        self.assertGreaterEqual(
            data["budget_baseline"] - data["production_budget"],
            data["retry_credit_cap"],
        )

    def test_unit_prompts_keep_every_part_inside_frame(self) -> None:
        prompt_root = PILOT_ROOT / "prompts"
        required_prefix = (
            "\u6700\u9ad8\u4f18\u5148\u7ea7\u786c\u6027\u6784\u56fe"
            "\u9650\u5236\uff1a"
        )
        for filename in UNIT_PROMPTS:
            prompt = (prompt_root / filename).read_text(encoding="utf-8")
            self.assertTrue(prompt.startswith(required_prefix), filename)
            self.assertIn("\u6bcf\u4e00\u5e27", prompt, filename)
            self.assertIn(
                "\u5b8c\u6574\u4f4d\u4e8e\u753b\u5e45\u5185", prompt, filename
            )
            self.assertIn("\u81f3\u5c11", prompt, filename)
            self.assertIn("%", prompt, filename)
            self.assertIn(
                "\u88ab\u753b\u9762\u8fb9\u7f18\u88c1\u5207",
                prompt,
                filename,
            )


if __name__ == "__main__":
    unittest.main()
