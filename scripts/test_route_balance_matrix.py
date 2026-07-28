"""Offline contracts for the immutable three-chapter route balance matrix."""

from __future__ import annotations

from collections import Counter
from pathlib import Path
import unittest

from run_route_balance_matrix import expand_full_cases, load_matrix


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MATRIX_PATH = PROJECT_ROOT / "SourceAssets" / "Balance" / "route-balance-matrix-v1.json"


class RouteBalanceMatrixTests(unittest.TestCase):
    def test_full_profile_expands_to_exactly_2400_cases(self) -> None:
        matrix = load_matrix(MATRIX_PATH)
        cases = expand_full_cases(matrix)

        self.assertEqual(len(cases), 2400)
        self.assertEqual(
            {case["node_kind"] for case in cases}, {"Battle", "Elite", "Boss"}
        )
        self.assertEqual(
            {case["cohort_id"] for case in cases},
            {
                "NakedBaseline",
                "PoJunSong",
                "XuanJiaYueBai",
                "QingNangZhou",
                "ZhuiFengJinGui",
                "ShiGuQiong",
                "ShanHeTusi",
                "MixedMaxRegression",
            },
        )

    def test_chapter_assignment_covers_every_chapter_node_gate(self) -> None:
        cases = expand_full_cases(load_matrix(MATRIX_PATH))
        buckets = Counter((case["chapter"], case["node_kind"]) for case in cases)

        self.assertEqual(
            set(buckets),
            {(chapter, kind) for chapter in (1, 2, 3) for kind in ("Battle", "Elite", "Boss")},
        )
        self.assertTrue(all(count >= 264 for count in buckets.values()))


if __name__ == "__main__":
    unittest.main(verbosity=2)
