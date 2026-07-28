#!/usr/bin/env python3
"""Regression contract for the PartyDeck eight-direction sprite preparation manifest."""

from __future__ import annotations

import json
import subprocess
import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = PROJECT_ROOT / "scripts" / "verify_party_deck_sprite_sources.py"
MANIFEST = PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "character-references" / "character-sheet-manifest.json"


def _run_validator(*args: str) -> tuple[subprocess.CompletedProcess[str], dict[str, object]]:
    if not VALIDATOR.exists():
        raise AssertionError(f"sprite manifest validator is missing: {VALIDATOR}")
    completed = subprocess.run(
        [sys.executable, str(VALIDATOR), "--json", *args],
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )
    payload = json.loads(completed.stdout)
    return completed, payload


class PartyDeckSpriteManifestTests(unittest.TestCase):
    def test_manifest_marks_all_twelve_packed_atlases_reviewed_and_ready_for_import(self) -> None:
        completed, payload = _run_validator("--require-ready")

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertTrue(payload["ok"])
        self.assertEqual(payload["production_target_count"], 12)
        self.assertTrue(payload["production_ready"])
        self.assertEqual(payload["ready_blocker_count"], 0)
        self.assertEqual(payload["target_ids"], [
            "Npc.TusiChief",
            "Npc.SongJinBao",
            "Npc.YueBai",
            "Npc.ZhouGuangZu",
            "Npc.JinGui",
            "Npc.QiongMeiEr",
            "PartnerRole.Blade",
            "PartnerRole.Guard",
            "PartnerRole.Healer",
            "PartnerRole.Hunter",
            "PartnerRole.Sorcerer",
            "PartnerRole.FormationMaster",
        ])
        self.assertIn("Npc.NiuHuan", payload["excluded_ids"])
        self.assertIn("Npc.SiQingNiang", payload["excluded_ids"])

        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        target_states = [target["production_state"] for target in manifest["production_targets"]]
        self.assertEqual(target_states, ["reviewed_ready_for_import"] * 12)

    def test_ready_manifest_has_no_packed_atlas_blockers(self) -> None:
        completed, payload = _run_validator("--require-ready")

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertTrue(payload["ok"])
        self.assertTrue(payload["production_ready"])
        self.assertEqual(payload["ready_blocker_count"], 0)
        self.assertEqual(payload["ready_blockers"], [])


if __name__ == "__main__":
    unittest.main()
