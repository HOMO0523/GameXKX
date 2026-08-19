"""Contract tests for the editor-only desktop-training HUD memory sampler."""

from __future__ import annotations

import json
import subprocess
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SAMPLER = PROJECT_ROOT / "scripts" / "measure_desktop_training_hud_memory.ps1"


class DesktopTrainingHudMemorySamplerTests(unittest.TestCase):
    def sampler_text(self) -> str:
        self.assertTrue(SAMPLER.is_file(), "HUD memory sampler script is missing")
        return SAMPLER.read_text(encoding="utf-8")

    def test_uses_argument_list_and_never_packages_the_project(self) -> None:
        text = self.sampler_text()

        self.assertIn("System.Diagnostics.ProcessStartInfo", text)
        self.assertIn("ArgumentList.Add", text)
        self.assertNotIn("Start-Process", text)
        self.assertNotIn("BuildCookRun", text)
        self.assertNotIn("-cook", text.lower())
        self.assertNotIn("-package", text.lower())

    def test_describe_only_reports_the_frozen_measurement_contract(self) -> None:
        completed = subprocess.run(
            [
                "pwsh.exe",
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(SAMPLER),
                "-DescribeOnly",
            ],
            cwd=PROJECT_ROOT,
            check=True,
            capture_output=True,
            text=True,
            encoding="utf-8-sig",
        )
        contract = json.loads(completed.stdout)

        self.assertEqual(contract["map"], "/Game/GameXXK/Maps/L_DesktopTrainingHUD")
        self.assertEqual(contract["resolution"], {"width": 1672, "height": 941})
        self.assertEqual(contract["sample_seconds"], [20, 50])
        self.assertEqual(Path(contract["project_file"]), PROJECT_ROOT / "GameXXK.uproject")
        self.assertTrue(contract["report_directory"].endswith("Saved\\HarnessReports"))


if __name__ == "__main__":
    unittest.main()
