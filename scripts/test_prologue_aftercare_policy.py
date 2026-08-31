from __future__ import annotations

import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
RUNTIME_FILES = (
    "Source/GameXXK/Public/Prologue/GameXXKPrologueAftermathTypes.h",
    "Source/GameXXK/Public/Prologue/GameXXKPrologueAftermathRules.h",
    "Source/GameXXK/Private/Prologue/GameXXKPrologueAftermathRules.cpp",
    "Source/GameXXK/Public/UI/GameXXKPrologueMapWidget.h",
    "Source/GameXXK/Private/UI/GameXXKPrologueMapWidget.cpp",
    "Source/GameXXK/Public/UI/GameXXKPrologueYueBaiWidget.h",
    "Source/GameXXK/Private/UI/GameXXKPrologueYueBaiWidget.cpp",
)
FORBIDDEN_TOKENS = (
    "SetWindowsHookEx",
    "SendInput",
    "mouse_event",
    "HideWindow",
    "ShowWindow",
    "bIdleStripFolded",
    "OrderedFormation",
    "StartTrainingChallenge",
    "GenerateChallengeRouteMap",
)


class PrologueAftercarePolicyTests(unittest.TestCase):
    def test_runtime_files_exist(self) -> None:
        missing = [
            relative
            for relative in RUNTIME_FILES
            if not (PROJECT_ROOT / relative).is_file()
        ]
        self.assertEqual(missing, [])

    def test_runtime_stays_free_of_global_input_and_ordinary_flow_coupling(self) -> None:
        combined = "\n".join(
            (PROJECT_ROOT / relative).read_text(encoding="utf-8")
            for relative in RUNTIME_FILES
            if (PROJECT_ROOT / relative).is_file()
        )
        leaked = [token for token in FORBIDDEN_TOKENS if token in combined]
        self.assertEqual(leaked, [])


if __name__ == "__main__":
    unittest.main()
