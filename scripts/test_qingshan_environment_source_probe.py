from __future__ import annotations

import math
import sys
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from qingshan_environment_source_probe import (  # noqa: E402
    PROBE_PATH,
    SHOP,
    build_source_metrics,
    parse_probe,
)


UE_PROBE = PROJECT_ROOT / PROBE_PATH
HOST_PROBE = PROJECT_ROOT / "scripts" / "qingshan_environment_source_probe.py"
EXPECTED_SHOP = (
    "/Game/GameXXK/Environment/TownPCG/Prototype/QingshanShopA/"
    "SM_Qingshan_Shop_A_HQ_Retop50K"
)


class UnrealProbeContractTests(unittest.TestCase):
    def test_probe_is_read_only_and_emits_one_json_object(self):
        source = UE_PROBE.read_text(encoding="utf-8")

        self.assertEqual(SHOP, EXPECTED_SHOP)
        self.assertIn(EXPECTED_SHOP, source)
        self.assertIn("get_bounds", source)
        self.assertEqual(source.count("print(json.dumps("), 1)
        self.assertNotIn("save_", source.lower())
        self.assertNotIn("set_editor_property", source)

    def test_host_uses_project_mcp_and_never_saves(self):
        source = HOST_PROBE.read_text(encoding="utf-8")

        self.assertIn("run_project_python_file(PROBE_PATH)", source)
        self.assertIn("parse_stdout_json", source)
        self.assertNotIn("save_dirty_packages", source)


class HostParserTests(unittest.TestCase):
    def valid_probe(self) -> dict:
        return {
            "ok": True,
            "asset_path": EXPECTED_SHOP,
            "bounds_size_m": [8.25, 6.5, 7.75],
            "material_slot_count": 1,
        }

    def test_parse_accepts_positive_finite_bounds(self):
        self.assertEqual(parse_probe(self.valid_probe()), self.valid_probe())

    def test_parse_rejects_zero_bounds(self):
        payload = self.valid_probe()
        payload["bounds_size_m"] = [0, 1, 1]

        with self.assertRaisesRegex(ValueError, "positive"):
            parse_probe(payload)

    def test_parse_rejects_negative_or_non_finite_bounds(self):
        for invalid in (-1.0, math.nan, math.inf, -math.inf):
            with self.subTest(invalid=invalid):
                payload = self.valid_probe()
                payload["bounds_size_m"] = [1, invalid, 1]
                with self.assertRaisesRegex(ValueError, "positive finite"):
                    parse_probe(payload)

    def test_parse_rejects_failed_probe_or_wrong_asset(self):
        failed = self.valid_probe()
        failed["ok"] = False
        with self.assertRaisesRegex(ValueError, "ok=true"):
            parse_probe(failed)

        wrong_asset = self.valid_probe()
        wrong_asset["asset_path"] = "/Game/Wrong/Asset"
        with self.assertRaisesRegex(ValueError, "asset_path"):
            parse_probe(wrong_asset)

    def test_parse_rejects_invalid_material_slot_count(self):
        for invalid in (-1, 1.5, True):
            with self.subTest(invalid=invalid):
                payload = self.valid_probe()
                payload["material_slot_count"] = invalid
                with self.assertRaisesRegex(ValueError, "material_slot_count"):
                    parse_probe(payload)

    def test_build_source_metrics_adds_capture_metadata(self):
        captured_at = "2026-07-10T12:34:56Z"
        result = build_source_metrics(
            self.valid_probe(),
            endpoint="http://127.0.0.1:18765/mcp",
            captured_at=captured_at,
        )

        self.assertEqual(
            result,
            {
                **self.valid_probe(),
                "captured_at": captured_at,
                "editor_endpoint": "http://127.0.0.1:18765/mcp",
                "probe_path": PROBE_PATH,
            },
        )


if __name__ == "__main__":
    unittest.main()
