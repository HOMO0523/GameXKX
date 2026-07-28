#!/usr/bin/env python3
"""Regression contract for source-preserving PartyDeck PPT reference extraction."""

from __future__ import annotations

import json
import subprocess
import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
EXTRACTOR = PROJECT_ROOT / "scripts" / "extract_party_deck_ppt_references.py"


class PartyDeckPptReferenceExtractionTests(unittest.TestCase):
    def test_dry_run_lists_only_the_six_approved_named_npc_references(self) -> None:
        self.assertTrue(EXTRACTOR.exists(), f"PPT reference extractor is missing: {EXTRACTOR}")
        completed = subprocess.run(
            [sys.executable, str(EXTRACTOR), "--json"],
            cwd=PROJECT_ROOT,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        payload = json.loads(completed.stdout)
        self.assertFalse(payload["write_mode"])
        self.assertEqual(payload["reference_count"], 6)
        self.assertEqual([entry["id"] for entry in payload["references"]], [
            "Npc.TusiChief",
            "Npc.SongJinBao",
            "Npc.YueBai",
            "Npc.ZhouGuangZu",
            "Npc.JinGui",
            "Npc.QiongMeiEr",
        ])
        self.assertNotIn("Npc.NiuHuan", [entry["id"] for entry in payload["references"]])
        self.assertNotIn("Npc.SiQingNiang", [entry["id"] for entry in payload["references"]])


if __name__ == "__main__":
    unittest.main()
