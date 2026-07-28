"""Unit tests for the UE TDD pipeline subprocess command contracts."""

import sys
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch


SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import ue_tdd_pipeline as pipeline


class UETDDPipelineCommandTests(unittest.TestCase):
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

    def test_launch_editor_uses_project_mcp_and_memory_ddc(self) -> None:
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
        self.assertEqual(command[-1], "-DDC-ForceMemoryCache")

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
