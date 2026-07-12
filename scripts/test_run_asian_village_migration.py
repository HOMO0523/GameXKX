from __future__ import annotations

import json
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import scripts.run_asian_village_migration as runner


class OrchestratorTests(unittest.TestCase):
    def test_direct_cli_imports_from_project_root(self):
        completed = subprocess.run(
            ["python", str(Path(runner.__file__)), "--help"],
            cwd=str(Path(runner.__file__).resolve().parents[1]),
            capture_output=True,
            text=True,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn("--preflight", completed.stdout)

    def test_phase_order_is_locked(self):
        self.assertEqual(
            runner.PHASES,
            (
                "preflight",
                "source_inventory",
                "save_target_editor",
                "close_target_editor",
                "source_ue54_audit",
                "copy_and_verify",
                "target_ue58_upgrade",
                "target_ue58_verify",
                "relaunch_target_editor",
            ),
        )

    def test_commandlet_requires_exit_zero_and_ok_report(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / "report.json"
            with mock.patch.object(runner, "LOG_ROOT", root / "logs"), mock.patch.object(
                runner, "AUDIT_SCRIPT", root / "audit.py"
            ):
                def success(command, **kwargs):
                    output.write_text(json.dumps({"mode": "source-readonly", "ok": True}), encoding="utf-8")
                    return subprocess.CompletedProcess(command, 0, "ok", "")

                report = runner.run_commandlet(Path("ue.exe"), Path("p.uproject"), "source-readonly", output, runner=success)
                self.assertTrue(report["ok"])

                failed_output = root / "failed.json"
                def failure(command, **kwargs):
                    return subprocess.CompletedProcess(command, 3, "", "bad")
                with self.assertRaisesRegex(runner.OrchestrationError, "exit 3"):
                    runner.run_commandlet(Path("ue.exe"), Path("p.uproject"), "source-readonly", failed_output, runner=failure)

    def test_commandlet_uses_python_commandlet_mode(self):
        captured = {}
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / "report.json"
            with mock.patch.object(runner, "LOG_ROOT", root / "logs"), mock.patch.object(
                runner, "AUDIT_SCRIPT", root / "audit.py"
            ):
                def execute(command, **kwargs):
                    captured["command"] = command
                    output.write_text(json.dumps({"mode": "source-readonly", "ok": True}), encoding="utf-8")
                    return subprocess.CompletedProcess(command, 0, "", "")

                runner.run_commandlet(Path("ue.exe"), Path("p.uproject"), "source-readonly", output, runner=execute)
        self.assertIn("-run=pythonscript", captured["command"])
        script_arg = next(item for item in captured["command"] if item.startswith("-script="))
        self.assertNotIn("\\", script_arg)

    def test_existing_manifest_checkpoint_must_match(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "manifest.json"
            manifest = {"schema_version": 1, "counts": {"files": 0, "uasset": 0, "umap": 0}, "files": []}
            runner.ensure_manifest(path, manifest)
            runner.ensure_manifest(path, manifest)
            with self.assertRaisesRegex(runner.OrchestrationError, "checkpoint drifted"):
                runner.ensure_manifest(path, {**manifest, "counts": {"files": 1, "uasset": 1, "umap": 0}})

    def test_save_gate_refuses_unavailable_mcp(self):
        fake_client = mock.Mock()
        fake_client.connect.return_value = False
        with mock.patch.object(runner, "is_editor_running", return_value=True), mock.patch.object(
            runner, "UnrealMCPClient", return_value=fake_client
        ):
            with self.assertRaisesRegex(runner.OrchestrationError, "MCP is unavailable"):
                runner.save_and_close_editor(timeout_seconds=0)

    def test_save_gate_refuses_remaining_dirty_packages(self):
        fake_client = mock.Mock()
        fake_client.connect.return_value = True
        fake_client.save_dirty_packages.return_value = {"save_result": True, "dirty_after": ["/Game/X"]}
        with mock.patch.object(runner, "is_editor_running", return_value=True), mock.patch.object(
            runner, "UnrealMCPClient", return_value=fake_client
        ):
            with self.assertRaisesRegex(runner.OrchestrationError, "not safely saved"):
                runner.save_and_close_editor(timeout_seconds=0)
            fake_client.execute_console_command.assert_not_called()

    def test_preflight_rejects_existing_target_before_mutation(self):
        with mock.patch.object(runner, "TARGET_ASSET_DIR", Path(__file__).parent):
            with self.assertRaisesRegex(runner.OrchestrationError, "target already exists"):
                runner.preflight(require_target_absent=True)


if __name__ == "__main__":
    unittest.main()
