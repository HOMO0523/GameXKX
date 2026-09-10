"""Unit tests for the UE TDD pipeline subprocess command contracts."""

import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock, patch


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import ue_tdd_pipeline as pipeline


class UETDDPipelineCommandTests(unittest.TestCase):
    def test_active_pie_keeps_editor_and_runtime_session_open(self) -> None:
        client = Mock()
        client.connect.return_value = True
        client.is_in_pie.return_value = True
        with patch.object(pipeline, "is_editor_running", return_value=True), patch.object(
            pipeline, "UnrealMCPClient", return_value=client
        ), patch.object(pipeline, "kill_editor") as close, patch.object(pipeline, "build_project") as build:
            result = pipeline.run_tdd_cycle(build=True, launch=True)
        self.assertFalse(result["success"])
        client.save_dirty_packages.assert_not_called()
        close.assert_not_called()
        build.assert_not_called()

    def test_stopped_pie_still_saves_dirty_packages_before_close(self) -> None:
        client = Mock()
        client.connect.return_value = True
        client.is_in_pie.return_value = False
        client.save_dirty_packages.return_value = {"save_result": True, "dirty_after": []}
        with patch.object(pipeline, "is_editor_running", return_value=True), patch.object(
            pipeline, "UnrealMCPClient", return_value=client
        ):
            self.assertTrue(pipeline.save_running_editor_before_close())
        client.save_dirty_packages.assert_called_once()

    def test_build_project_uses_project_and_disables_hot_reload(self) -> None:
        with patch.object(
            pipeline.subprocess,
            "run",
            return_value=SimpleNamespace(returncode=0),
        ) as mock_run:
            self.assertTrue(pipeline.build_project())

        command = mock_run.call_args.args[0]
        self.assertEqual(command[0], str(pipeline.UE_BUILD_BAT))
        self.assertIn(f"-Project={pipeline.UPROJECT.as_posix()}", command)
        self.assertIn("-NoHotReload", command)

    def test_launch_editor_uses_project_mcp_and_disk_ddc(self) -> None:
        fake_process = SimpleNamespace(pid=12345)
        with patch.object(
            pipeline.subprocess,
            "Popen",
            return_value=fake_process,
        ) as mock_popen:
            self.assertIs(pipeline.launch_editor(mcp_port=18765), fake_process)

        command = mock_popen.call_args.args[0]
        self.assertEqual(command[0], str(pipeline.UE_EDITOR))
        self.assertIn(pipeline.UPROJECT.as_posix(), command)
        self.assertIn("-ModelContextProtocolStartServer", command)
        self.assertIn("-ModelContextProtocolPort=18765", command)
        self.assertNotIn("-DDC-ForceMemoryCache", command)
        self.assertIn(f"-LocalDataCachePath={(pipeline.PROJECT_ROOT / 'Saved' / 'DerivedDataCache').as_posix()}", command)

    def test_launch_editor_bypasses_sdk_probes_for_automation_only(self) -> None:
        fake_process = SimpleNamespace(pid=12345)
        with patch.object(
            pipeline.subprocess,
            "Popen",
            return_value=fake_process,
        ) as mock_popen:
            self.assertIs(pipeline.launch_editor(mcp_port=18765), fake_process)

        command = mock_popen.call_args.args[0]
        child_environment = mock_popen.call_args.kwargs["env"]
        self.assertIn("-Unattended", command)
        self.assertIn("-UnattendedInput", command)
        self.assertEqual(child_environment["UE_SKIP_UBT_SDK_SETUP"], "1")


if __name__ == "__main__":
    unittest.main()
