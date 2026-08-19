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

    def test_describe_only_reports_the_four_profile_matrix(self) -> None:
        completed = subprocess.run(
            [
                "pwsh.exe",
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(SAMPLER),
                "-DescribeOnly",
                "-Profile",
                "all",
            ],
            cwd=PROJECT_ROOT,
            check=True,
            capture_output=True,
            text=True,
            encoding="utf-8-sig",
        )
        contract = json.loads(completed.stdout)

        self.assertEqual(contract["schema_version"], 2)
        self.assertEqual(contract["resolution"], {"width": 1672, "height": 941})
        self.assertEqual(contract["sample_seconds"], [20, 50])
        self.assertEqual(Path(contract["project_file"]), PROJECT_ROOT / "GameXXK.uproject")
        self.assertTrue(contract["report_directory"].endswith("Saved\\HarnessReports"))
        profiles = {profile["name"]: profile for profile in contract["profiles"]}
        self.assertEqual(list(profiles), ["empty", "travel", "challenge", "town3d"])
        self.assertEqual(profiles["empty"]["map"], "/Game/GameXXK/Maps/L_DesktopTrainingHUD")
        self.assertIn("-GameXXKPerfProfile=empty", profiles["empty"]["arguments"])
        self.assertIn("-GameXXKPerfProfile=travel", profiles["travel"]["arguments"])
        self.assertIn("-GameXXKPerfProfile=challenge", profiles["challenge"]["arguments"])
        self.assertIn("surface", profiles["challenge"])
        self.assertEqual(profiles["challenge"]["surface"], "existing-fullscreen-battle")
        self.assertEqual(
            profiles["town3d"]["map"],
            "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo",
        )
        self.assertFalse(
            any("GameXXKPerfProfile" in argument for argument in profiles["town3d"]["arguments"])
        )

    def test_sampler_records_cpu_and_gpu_process_metrics(self) -> None:
        text = self.sampler_text()

        self.assertIn("TotalProcessorTime", text)
        self.assertIn("Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine", text)
        self.assertIn("Win32_PerfFormattedData_GPUPerformanceCounters_GPUProcessMemory", text)
        for field in (
            "cpu_percent",
            "gpu_engine_percent",
            "gpu_dedicated_mib",
            "gpu_shared_mib",
        ):
            self.assertIn(field, text)

    def test_schema_v2_keeps_each_profile_process_isolated(self) -> None:
        text = self.sampler_text()

        self.assertIn("profile_results", text)
        self.assertIn("profile_name", text)
        self.assertIn("closed-launched-process", text)
        self.assertIn("killed-launched-process-tree", text)
        self.assertIn("gpu_metric_error", text)
        self.assertIn("foreach ($profileDefinition in $selectedProfiles)", text)


if __name__ == "__main__":
    unittest.main()
