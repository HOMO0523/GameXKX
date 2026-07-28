#!/usr/bin/env python3
"""Contract tests for the affordable 139-asset production manifest."""

from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "SourceAssets/AnimationProduction/production_v1/manifest.json"


class AnimationProductionManifestTests(unittest.TestCase):
    def test_affordable_scope_and_cost(self) -> None:
        data = json.loads(MANIFEST.read_text(encoding="utf-8"))
        entries = data["entries"]
        self.assertEqual(139, len(entries))
        self.assertEqual(139, len({entry["asset_id"] for entry in entries}))
        self.assertEqual(6580, sum(entry["expected_credits"] for entry in entries))
        self.assertEqual(136, sum(entry["kind"] == "unit_action" for entry in entries))
        self.assertEqual(3, sum(entry["kind"] == "generic_effect" for entry in entries))
        self.assertNotIn("buff", {entry.get("action") for entry in entries})

    def test_every_entry_is_preflight_ready_and_single_attempt(self) -> None:
        data = json.loads(MANIFEST.read_text(encoding="utf-8"))
        for entry in data["entries"]:
            self.assertTrue((ROOT / entry["first_frame"]).is_file(), entry["asset_id"])
            self.assertTrue((ROOT / entry["last_frame"]).is_file(), entry["asset_id"])
            self.assertTrue((ROOT / entry["prompt_file"]).is_file(), entry["asset_id"])
            self.assertEqual(1, entry["max_submissions"])
            self.assertFalse(entry["automatic_retry"])
            self.assertEqual("not_submitted", entry["status"])
            self.assertEqual(0, entry["submission_count"])

    def test_generic_effects_are_module_only(self) -> None:
        data = json.loads(MANIFEST.read_text(encoding="utf-8"))
        generic_ids = {
            entry["asset_id"]
            for entry in data["entries"]
            if entry["kind"] == "generic_effect"
        }
        self.assertEqual(
            {"status_buff_generic", "status_debuff_generic", "impact_ink_generic"},
            generic_ids,
        )
        self.assertEqual("idle_plus_generic_overlay", data["buff_runtime_policy"])


if __name__ == "__main__":
    unittest.main()
