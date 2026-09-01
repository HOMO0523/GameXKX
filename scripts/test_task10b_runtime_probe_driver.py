from __future__ import annotations

import importlib.util
import json
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest import mock


DRIVER = (
    Path(__file__).resolve().parents[1]
    / "Content/Python/gamexxk_probe_task10b_game_runtime.py"
)


class _FakeWorld:
    def get_path_name(self) -> str:
        return "/Game/GameXXK/Maps/L_DesktopTrainingHUD"


class FakeGameXXKMVPPlayerController:
    def __init__(self) -> None:
        self.actions: list[tuple[str, str]] = []

    def get_path_name(self) -> str:
        return "/Game/Test.GameXXKMVPPlayerController_0"

    def get_world(self) -> _FakeWorld:
        return _FakeWorld()

    def get_desktop_story_probe_state(self) -> dict[str, str]:
        return {"activeSaveSlot": "GameXXK_MVP_SaveSlot_1"}

    def execute_desktop_story_probe_action(self, action: str, argument: str) -> bool:
        self.actions.append((str(action), str(argument)))
        return str(action) != "load_slot"


class RuntimeDriverFailFastTest(unittest.TestCase):
    def test_rejected_load_never_dispatches_reopen_or_primary(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            controller = FakeGameXXKMVPPlayerController()
            callbacks: list = []
            fake_unreal = types.SimpleNamespace(
                SystemLibrary=types.SimpleNamespace(
                    get_command_line=lambda: (
                        "-GameXXKTask10BProbe "
                        "-GameXXKTask10BActions=load_slot|GameXXK_Task10B_Missing,reopen,primary "
                        "-GameXXKTask10BReport=fail-fast.json"
                    )
                ),
                Paths=types.SimpleNamespace(project_saved_dir=lambda: temp_dir),
                ObjectIterator=lambda: iter([controller]),
                Name=lambda value: value,
                register_slate_post_tick_callback=lambda callback: callbacks.append(callback) or 1,
                unregister_slate_post_tick_callback=lambda _handle: None,
            )
            module_name = "_gamexxk_task10b_runtime_driver_fail_fast_test"
            spec = importlib.util.spec_from_file_location(module_name, DRIVER)
            self.assertIsNotNone(spec)
            module = importlib.util.module_from_spec(spec)
            with mock.patch.dict(sys.modules, {"unreal": fake_unreal}):
                assert spec and spec.loader
                spec.loader.exec_module(module)
            self.assertEqual(1, len(callbacks))
            callbacks[0](0.016)
            self.assertEqual(
                [("load_slot", "GameXXK_Task10B_Missing")],
                controller.actions,
            )
            report = json.loads(
                (Path(temp_dir) / "HarnessReports/fail-fast.json").read_text(
                    encoding="utf-8"
                )
            )
            self.assertFalse(report["ok"])
            self.assertEqual("action_rejected", report["reason"])
            self.assertEqual("load_slot", report["failedAction"])


if __name__ == "__main__":
    unittest.main()
