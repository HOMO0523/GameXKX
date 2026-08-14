"""Unit tests for the UE automation index.json parser."""

import json
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from parse_automation_index import parse_report  # noqa: E402


def _write_index(directory: Path, payload: dict) -> Path:
    path = directory / "index.json"
    path.write_text(json.dumps(payload), encoding="utf-8")
    return path


def _test(name: str, state: str, warnings: int = 0, errors: int = 0) -> dict:
    return {
        "testDisplayName": name,
        "fullTestPath": f"GameXXK.{name}",
        "state": state,
        "warnings": warnings,
        "errors": errors,
    }


class ParseAutomationIndexTests(unittest.TestCase):
    def test_all_green_passes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            payload = {
                "succeeded": 2,
                "succeededWithWarnings": 0,
                "failed": 0,
                "notRun": 0,
                "inProcess": 0,
                "tests": [_test("Alpha", "Success"), _test("Beta", "Success")],
            }
            report = parse_report(_write_index(Path(directory), payload))
        self.assertTrue(report["ok"], report)
        self.assertEqual(report["counts"]["discovered"], 2)
        self.assertEqual(report["counts"]["succeeded"], 2)

    def test_fail_state_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            payload = {
                "succeeded": 1,
                "failed": 1,
                "notRun": 0,
                "inProcess": 0,
                "tests": [_test("Alpha", "Success"), _test("Beta", "Fail")],
            }
            report = parse_report(_write_index(Path(directory), payload))
        self.assertFalse(report["ok"])
        self.assertEqual(report["counts"]["failed"], 1)

    def test_not_run_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            payload = {
                "succeeded": 0,
                "failed": 0,
                "notRun": 1,
                "inProcess": 0,
                "tests": [_test("Alpha", "NotRun")],
            }
            report = parse_report(_write_index(Path(directory), payload))
        self.assertFalse(report["ok"])
        self.assertEqual(report["counts"]["not_run"], 1)

    def test_in_process_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            payload = {
                "succeeded": 0,
                "failed": 0,
                "notRun": 0,
                "inProcess": 1,
                "tests": [_test("Alpha", "InProcess")],
            }
            report = parse_report(_write_index(Path(directory), payload))
        self.assertFalse(report["ok"])

    def test_per_test_errors_fail_even_when_state_success(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            payload = {
                "succeeded": 1,
                "failed": 0,
                "notRun": 0,
                "inProcess": 0,
                "tests": [_test("Alpha", "Success", errors=1)],
            }
            report = parse_report(_write_index(Path(directory), payload))
        self.assertFalse(report["ok"])
        self.assertEqual(report["counts"]["errors"], 1)

    def test_warnings_pass_by_default_and_fail_when_strict(self) -> None:
        payload = {
            "succeeded": 0,
            "succeededWithWarnings": 1,
            "failed": 0,
            "notRun": 0,
            "inProcess": 0,
            "tests": [_test("Alpha", "SuccessWithWarnings", warnings=2)],
        }
        with tempfile.TemporaryDirectory() as directory:
            path = _write_index(Path(directory), payload)
            default_report = parse_report(path)
            strict_report = parse_report(path, fail_on_warnings=True)
        self.assertTrue(default_report["ok"])
        self.assertEqual(default_report["counts"]["warnings"], 2)
        self.assertFalse(strict_report["ok"])

    def test_unexpected_state_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            payload = {
                "succeeded": 0,
                "failed": 0,
                "notRun": 0,
                "inProcess": 0,
                "tests": [_test("Alpha", "Skipped")],
            }
            report = parse_report(_write_index(Path(directory), payload))
        self.assertFalse(report["ok"])
        self.assertEqual(report["counts"]["other"], 1)

    def test_top_level_counter_disagreement_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            payload = {
                "succeeded": 5,
                "succeededWithWarnings": 0,
                "failed": 0,
                "notRun": 0,
                "inProcess": 0,
                "tests": [_test("Alpha", "Success")],
            }
            report = parse_report(_write_index(Path(directory), payload))
        self.assertFalse(report["ok"])
        self.assertIn("disagrees", report["problems"][0])

    def test_missing_tests_array_fails(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            report = parse_report(_write_index(Path(directory), {"succeeded": 0}))
        self.assertFalse(report["ok"])
        self.assertIn("error", report)


if __name__ == "__main__":
    unittest.main()
