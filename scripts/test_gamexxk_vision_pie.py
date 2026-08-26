"""Headless unit tests for the PIE vision-flow report contract (no UE MCP)."""

import json
import sys
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import gamexxk_vision as vision  # noqa: E402
import gamexxk_vision_pie as pie  # noqa: E402


class VisionPieReportTests(unittest.TestCase):
    def test_build_vision_report_contract(self) -> None:
        result = vision.VisionResult(
            content="画面正常",
            model=vision.DEFAULT_MODEL,
            finish_reason="stop",
            usage={"total_tokens": 12},
            raw={},
        )
        report = pie.build_vision_report(
            Path("Saved/Codex/vision_pie_test.png"),
            (1280, 720),
            result,
            prompt=pie.DEFAULT_PIE_PROMPT,
            detail="low",
        )
        self.assertTrue(report["ok"])
        self.assertEqual(report["image_size"], [1280, 720])
        self.assertEqual(report["model"], vision.DEFAULT_MODEL)
        self.assertEqual(report["analysis"], "画面正常")
        self.assertEqual(report["usage"], {"total_tokens": 12})
        self.assertEqual(report["detail"], "low")
        # Report must be JSON-serializable evidence without the raw payload.
        json.dumps(report, ensure_ascii=False)

    def test_build_parser_exposes_documented_modes(self) -> None:
        parser = pie.build_parser()
        args = parser.parse_args(["--capture-only", "--ocr", "--json"])
        self.assertTrue(args.capture_only)
        self.assertTrue(args.ocr)
        self.assertTrue(args.json)


if __name__ == "__main__":
    unittest.main()
