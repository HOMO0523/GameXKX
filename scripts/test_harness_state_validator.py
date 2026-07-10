from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from scripts.harness_state_validator import validate


class HarnessStateValidatorTests(unittest.TestCase):
    def test_evidence_root_is_not_a_production_unit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "evidence" / "qingshan-town-pcg").mkdir(parents=True)
            report = validate(root, require_units=False)
        self.assertTrue(report["ok"])
        self.assertEqual(report["units"], [])
        self.assertEqual(report["findings"], [])


if __name__ == "__main__":
    unittest.main()
