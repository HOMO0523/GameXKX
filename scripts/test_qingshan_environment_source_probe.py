from __future__ import annotations

import json
import math
import os
import subprocess
import sys
import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path
from unittest.mock import patch


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from qingshan_environment_source_probe import (  # noqa: E402
    APPROVED_ROOT,
    PROBE_PATH,
    SHOP,
    build_source_metrics,
    parse_probe,
    write_source_metrics,
)


UE_PROBE = PROJECT_ROOT / PROBE_PATH
HOST_PROBE = PROJECT_ROOT / "scripts" / "qingshan_environment_source_probe.py"
SOURCE_METRICS = APPROVED_ROOT / "source_metrics.json"
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
        self.assertIn("write_source_metrics(args.output, metrics)", source)
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
        for invalid in (-1, 0, 1.5, True):
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


class AtomicWriterTests(unittest.TestCase):
    def valid_metrics(self) -> dict:
        return {
            "ok": True,
            "asset_path": EXPECTED_SHOP,
            "bounds_size_m": [8.25, 6.5, 7.75],
            "material_slot_count": 1,
            "captured_at": "2026-07-10T12:34:56Z",
            "editor_endpoint": "http://127.0.0.1:18765/mcp",
            "probe_path": PROBE_PATH,
        }

    def test_writer_flushes_and_fsyncs_before_atomic_replace(self):
        with tempfile.TemporaryDirectory(dir=APPROVED_ROOT) as directory:
            root = Path(directory)
            output = root / "metrics.json"

            with patch(
                "qingshan_environment_source_probe.os.fsync",
                wraps=os.fsync,
            ) as fsync:
                write_source_metrics(output, self.valid_metrics())

            self.assertEqual(json.loads(output.read_text(encoding="utf-8")), self.valid_metrics())
            fsync.assert_called_once()
            self.assertEqual(list(root.iterdir()), [output])

    def test_replace_failure_preserves_old_file_and_cleans_temporary_file(self):
        with tempfile.TemporaryDirectory(dir=APPROVED_ROOT) as directory:
            root = Path(directory)
            output = root / "metrics.json"
            old_bytes = b'{"old": true}\n'
            output.write_bytes(old_bytes)

            with patch(
                "qingshan_environment_source_probe.os.replace",
                side_effect=OSError("replace failed"),
            ):
                with self.assertRaisesRegex(OSError, "replace failed"):
                    write_source_metrics(output, self.valid_metrics())

            self.assertEqual(output.read_bytes(), old_bytes)
            self.assertEqual(list(root.iterdir()), [output])


class OutputPathSafetyTests(unittest.TestCase):
    def valid_metrics(self) -> dict:
        return {
            "ok": True,
            "asset_path": EXPECTED_SHOP,
            "bounds_size_m": [8.25, 6.5, 7.75],
            "material_slot_count": 1,
            "captured_at": "2026-07-10T12:34:56Z",
            "editor_endpoint": "http://127.0.0.1:18765/mcp",
            "probe_path": PROBE_PATH,
        }

    def test_writer_rejects_traversal_and_absolute_escape(self):
        with tempfile.TemporaryDirectory(dir=APPROVED_ROOT.parent) as outside_directory:
            outside_root = Path(outside_directory)
            candidates = (
                APPROVED_ROOT / ".." / outside_root.name / "traversal.json",
                outside_root / "absolute.json",
            )
            for output in candidates:
                with self.subTest(output=output):
                    with self.assertRaisesRegex(ValueError, "approved root"):
                        write_source_metrics(output, self.valid_metrics())
                    self.assertFalse(output.exists())

    def test_writer_rejects_symlink_or_junction_escape(self):
        with (
            tempfile.TemporaryDirectory(dir=APPROVED_ROOT) as inside_directory,
            tempfile.TemporaryDirectory() as outside_directory,
        ):
            link = Path(inside_directory) / "outside-link"
            outside_target = Path(outside_directory) / "outside.json"
            old_bytes = b'{"outside": "must stay unchanged"}\n'
            outside_target.write_bytes(old_bytes)
            junction_created = False
            try:
                link.symlink_to(Path(outside_directory), target_is_directory=True)
            except OSError as exc:
                if os.name != "nt":
                    self.skipTest(f"platform cannot create test symlink: {exc}")
                junction = subprocess.run(
                    ["cmd", "/c", "mklink", "/J", str(link), str(outside_directory)],
                    capture_output=True,
                    text=True,
                    check=False,
                )
                if junction.returncode != 0:
                    self.skipTest(
                        "platform cannot create test symlink or junction: "
                        f"{junction.stderr or junction.stdout}"
                    )
                junction_created = True

            output = link / outside_target.name
            try:
                with self.assertRaisesRegex(ValueError, "approved root"):
                    write_source_metrics(output, self.valid_metrics())

                self.assertEqual(outside_target.read_bytes(), old_bytes)
            finally:
                if junction_created and link.exists():
                    os.rmdir(link)


class CommittedArtifactContractTests(unittest.TestCase):
    def test_source_metrics_records_valid_measured_source_facts(self):
        metrics = json.loads(SOURCE_METRICS.read_text(encoding="utf-8"))

        self.assertIs(metrics["ok"], True)
        self.assertEqual(metrics["asset_path"], EXPECTED_SHOP)
        bounds = metrics["bounds_size_m"]
        self.assertEqual(len(bounds), 3)
        for value in bounds:
            self.assertNotIsInstance(value, bool)
            self.assertIsInstance(value, (int, float))
            self.assertTrue(math.isfinite(float(value)))
            self.assertGreater(value, 0)

        material_slot_count = metrics["material_slot_count"]
        self.assertNotIsInstance(material_slot_count, bool)
        self.assertIsInstance(material_slot_count, int)
        self.assertGreater(material_slot_count, 0)

        captured_at_text = metrics["captured_at"]
        self.assertTrue(captured_at_text.endswith("Z"))
        captured_at = datetime.fromisoformat(captured_at_text.replace("Z", "+00:00"))
        self.assertEqual(captured_at.utcoffset(), timezone.utc.utcoffset(captured_at))
        self.assertEqual(metrics["editor_endpoint"], "http://127.0.0.1:18765/mcp")
        self.assertEqual(metrics["probe_path"], PROBE_PATH)


if __name__ == "__main__":
    unittest.main()
