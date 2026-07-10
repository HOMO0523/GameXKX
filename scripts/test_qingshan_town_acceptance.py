from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

try:
    from scripts.qingshan_town_acceptance import (
        canonical_transform_hash,
        parse_csv_profile,
    )
except ModuleNotFoundError:
    canonical_transform_hash = None
    parse_csv_profile = None


class QingshanTownAcceptanceTests(unittest.TestCase):
    def test_editor_probe_uses_public_viewport_api_and_separate_actions(self):
        path = Path(__file__).resolve().parents[1] / "Content" / "Python" / "gamexxk_qingshan_town_acceptance.py"
        self.assertTrue(path.is_file())
        source = path.read_text(encoding="utf-8")
        self.assertIn("set_level_viewport_camera_info", source)
        self.assertIn('"snapshot"', source)
        self.assertIn('"clear"', source)
        self.assertIn('"metadata"', source)
        self.assertIn("get_size_x", source)
        self.assertNotIn("tick_editor", source)

    def test_transform_hash_is_stable_across_input_order(self):
        self.assertIsNotNone(canonical_transform_hash)
        transforms = [
            {"location_cm": [900.0, -1600.0, 0.0], "rotation_degrees": [0.0, 180.0, 0.0], "scale": [1.0, 1.0, 1.0]},
            {"location_cm": [-900.0, -1600.0, 0.0], "rotation_degrees": [0.0, 0.0, 0.0], "scale": [1.0, 1.0, 1.0]},
        ]
        first = canonical_transform_hash(transforms)
        second = canonical_transform_hash(list(reversed(transforms)))
        self.assertEqual(first["sha256"], second["sha256"])
        self.assertEqual(first["transforms"], second["transforms"])

    def test_transform_hash_rounds_to_declared_tolerance(self):
        self.assertIsNotNone(canonical_transform_hash)
        base = [{"location_cm": [1.00001, 2, 3], "rotation_degrees": [0, 0, 0], "scale": [1, 1, 1]}]
        nearby = [{"location_cm": [1.00002, 2, 3], "rotation_degrees": [0, 0, 0], "scale": [1, 1, 1]}]
        self.assertEqual(
            canonical_transform_hash(base, decimals=3)["sha256"],
            canonical_transform_hash(nearby, decimals=3)["sha256"],
        )

    def test_csv_profile_reports_frame_time_percentiles(self):
        self.assertIsNotNone(parse_csv_profile)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.csv"
            path.write_text(
                "Frame,FrameTime,GameThreadTime,GPUTime,RHI/DrawCalls,GPUSceneInstanceCount,TextureStreaming/StreamingPool,GPUMem/LocalUsedMB\n"
                "0,10,7,8,100,50,256,1024\n1,20,8,9,120,50,256,1030\n2,30,9,10,140,50,256,1040\n3,40,10,11,160,50,256,1050\n",
                encoding="utf-8",
            )
            metrics = parse_csv_profile(path)
        self.assertEqual(metrics["frame_count"], 4)
        self.assertEqual(metrics["average_frame_time_ms"], 25.0)
        self.assertEqual(metrics["median_frame_time_ms"], 25.0)
        self.assertEqual(metrics["p95_frame_time_ms"], 40.0)
        self.assertEqual(metrics["p99_frame_time_ms"], 40.0)
        self.assertEqual(metrics["hitch_count_over_50ms"], 0)
        self.assertEqual(metrics["counters"]["RHI/DrawCalls"]["average"], 130.0)
        self.assertEqual(metrics["counters"]["GPUSceneInstanceCount"]["max"], 50.0)


if __name__ == "__main__":
    unittest.main()
