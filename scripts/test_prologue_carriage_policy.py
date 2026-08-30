from __future__ import annotations

import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
RUNTIME_FILES = (
    "Source/GameXXK/Public/Prologue/GameXXKPrologueCarriageTypes.h",
    "Source/GameXXK/Public/Prologue/GameXXKPrologueCarriageRules.h",
    "Source/GameXXK/Private/Prologue/GameXXKPrologueCarriageRules.cpp",
    "Source/GameXXK/Public/UI/GameXXKPrologueCarriageWidget.h",
    "Source/GameXXK/Private/UI/GameXXKPrologueCarriageWidget.cpp",
    "Source/GameXXK/Public/UI/GameXXKProloguePauseWidget.h",
    "Source/GameXXK/Private/UI/GameXXKProloguePauseWidget.cpp",
    "Source/GameXXK/Public/Town/GameXXKPrologueCarriageRig.h",
    "Source/GameXXK/Private/Town/GameXXKPrologueCarriageRig.cpp",
)
FORBIDDEN_TOKENS = (
    "SetWindowsHookEx",
    "SendInput",
    "mouse_event",
    "HideWindow",
    "ShowWindow",
    "bIdleStripFolded",
    "OrderedFormation",
    "NarrativeProgress",
    "GuideProgress",
)
PROBE_FILE = "Content/Python/gamexxk_probe_prologue_carriage_preview.py"
PROBE_FORBIDDEN_TOKENS = (
    "click",
    "sendinput",
    "send_input",
    "key_down",
    "key_up",
    "open_level",
    "load_level",
    "save_dirty_packages",
    "save_current_level",
    "set_actor_location",
    "set_actor_transform",
    "interact()",
)


class PrologueCarriagePolicyTests(unittest.TestCase):
    def test_runtime_files_exist(self) -> None:
        missing = [
            relative
            for relative in RUNTIME_FILES
            if not (PROJECT_ROOT / relative).is_file()
        ]
        self.assertEqual(missing, [])

    def test_runtime_is_free_of_global_input_and_gameplay_state_coupling(self) -> None:
        combined = "\n".join(
            (PROJECT_ROOT / relative).read_text(encoding="utf-8")
            for relative in RUNTIME_FILES
            if (PROJECT_ROOT / relative).is_file()
        )
        leaked = [token for token in FORBIDDEN_TOKENS if token in combined]
        self.assertEqual(leaked, [])

    def test_probe_is_present_and_read_only(self) -> None:
        path = PROJECT_ROOT / PROBE_FILE
        self.assertTrue(path.is_file(), PROBE_FILE)
        source = path.read_text(encoding="utf-8").lower()
        leaked = [token for token in PROBE_FORBIDDEN_TOKENS if token in source]
        self.assertEqual(leaked, [])
        self.assertNotIn("or -1", source, "frame zero must not collapse to the sentinel")


if __name__ == "__main__":
    unittest.main()
