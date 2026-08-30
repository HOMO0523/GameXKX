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


if __name__ == "__main__":
    unittest.main()
