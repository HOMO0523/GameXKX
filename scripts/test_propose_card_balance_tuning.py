import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from scripts.propose_card_balance_tuning import (
    bounded_change,
    build_proposal,
    classify_card_usage,
    get_target_interval,
    validate_observation_pair,
    wilson_interval,
    write_proposal,
)


class BalanceTuningMathTests(unittest.TestCase):
    def test_wilson_interval_matches_known_95_percent_bounds(self):
        lower, upper = wilson_interval(50, 100)
        self.assertAlmostEqual(lower, 0.4038, places=4)
        self.assertAlmostEqual(upper, 0.5962, places=4)

        zero_lower, zero_upper = wilson_interval(0, 10)
        self.assertEqual(zero_lower, 0.0)
        self.assertAlmostEqual(zero_upper, 0.2775, places=4)

    def test_growth_tier_targets_follow_the_approved_specification(self):
        self.assertEqual(get_target_interval("Early", "Battle"), (0.70, 0.85))
        self.assertEqual(get_target_interval("Early", "Elite"), (0.35, 0.55))
        self.assertEqual(get_target_interval("Early", "Boss"), (0.15, 0.35))
        self.assertEqual(get_target_interval("Mid", "Battle"), (0.80, 0.92))
        self.assertEqual(get_target_interval("Mid", "Elite"), (0.55, 0.70))
        self.assertEqual(get_target_interval("Mid", "Boss"), (0.35, 0.55))
        self.assertEqual(get_target_interval("Late", "Battle"), (0.95, 1.0))
        self.assertEqual(get_target_interval("Late", "Elite"), (0.80, 0.95))
        self.assertEqual(get_target_interval("Late", "Boss"), (0.60, 0.80))
        with self.assertRaisesRegex(ValueError, "unknown growth target"):
            get_target_interval("Early", "Camp")

    def test_single_iteration_change_caps_are_bounded(self):
        self.assertEqual(bounded_change("attack_multiplier_pp", 100, 1), 15)
        self.assertEqual(bounded_change("attack_multiplier_pp", 100, -1), -15)
        self.assertEqual(bounded_change("fixed_value", 20, 1), 2)
        self.assertEqual(bounded_change("fixed_value", 8, -1), -1)
        self.assertEqual(bounded_change("energy", 2, -1), -1)
        self.assertEqual(bounded_change("mana", 12, 1), 3)
        self.assertEqual(bounded_change("enemy_percent", 100, -1), -10)


class BalanceTuningInputTests(unittest.TestCase):
    def make_record(self, sha="same", matrix="orthogonal", cases=2520):
        return {
            "schema_version": 3,
            "matrix": matrix,
            "csv_sha256": sha,
            "summary": {"case_count": cases},
        }

    def test_pair_requires_same_schema_matrix_case_count_and_sha(self):
        first = self.make_record()
        second = self.make_record()
        validate_observation_pair(first, second)

        for key, value in (
            ("csv_sha256", "different"),
            ("matrix", "locked"),
            ("schema_version", 2),
        ):
            changed = self.make_record()
            changed[key] = value
            with self.subTest(key=key):
                with self.assertRaisesRegex(ValueError, "observation pair"):
                    validate_observation_pair(first, changed)

        changed_count = self.make_record(cases=2400)
        with self.assertRaisesRegex(ValueError, "observation pair"):
            validate_observation_pair(first, changed_count)

    def test_direct_script_entrypoint_can_load_project_modules(self):
        project_root = Path(__file__).resolve().parents[1]
        process = subprocess.run(
            [sys.executable, "-B", "scripts/propose_card_balance_tuning.py", "--help"],
            cwd=project_root,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        self.assertEqual(process.returncode, 0, process.stderr)


class BalanceTuningClassificationTests(unittest.TestCase):
    def test_cards_are_classified_by_seen_played_rate_and_contribution(self):
        usage = {
            "Card.Unseen": {"seen": 0, "played": 0, "damage": 0, "healing": 0, "armor": 0},
            "Card.Never": {"seen": 40, "played": 0, "damage": 0, "healing": 0, "armor": 0},
            "Card.LowEmpty": {"seen": 100, "played": 5, "damage": 0, "healing": 0, "armor": 0},
            "Card.LowStrong": {"seen": 100, "played": 5, "damage": 500, "healing": 0, "armor": 0},
            "Card.HighStrong": {"seen": 100, "played": 80, "damage": 8000, "healing": 0, "armor": 0},
        }

        classified = classify_card_usage(usage)

        self.assertIn("Card.Unseen", classified["unseen"])
        self.assertIn("Card.Never", classified["never_played"])
        self.assertIn("Card.LowEmpty", classified["low_play_low_direct_contribution"])
        self.assertIn("Card.LowStrong", classified["low_play_high_direct_contribution"])
        self.assertIn("Card.HighStrong", classified["high_direct_contribution"])

    def test_proposal_has_no_production_write_authority(self):
        first = {
            "schema_version": 3,
            "matrix": "orthogonal",
            "csv_sha256": "stable",
            "summary": {
                "case_count": 2,
                "card_usage": {},
                "runtime_totals": {},
                "recurring_stalemates": [],
                "stranded_target_cases": [],
            },
        }
        second = json.loads(json.dumps(first))
        rows = [
            {
                "dimension": "Progression",
                "variant": "Early",
                "node": "Battle",
                "seed": 1,
                "outcome": "Victory",
                "rounds": 3,
                "remaining_party_health": 50,
            },
            {
                "dimension": "Progression",
                "variant": "Early",
                "node": "Battle",
                "seed": 2,
                "outcome": "Defeat",
                "rounds": 5,
                "remaining_party_health": 0,
            },
        ]

        proposal = build_proposal(first, second, rows)

        self.assertEqual(proposal["write_authority"], "none")
        self.assertNotIn("production_patch", proposal)
        self.assertTrue(proposal["candidates"])
        self.assertTrue(all("forbidden_parallel_changes" in item for item in proposal["candidates"]))

        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            write_proposal(proposal, output)
            self.assertTrue((output / "proposal.json").is_file())
            self.assertTrue((output / "proposal.md").is_file())


if __name__ == "__main__":
    unittest.main()
