"""Contract tests for the user-facing GameXXK UE editor launcher."""

from __future__ import annotations

import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
LAUNCHER = PROJECT_ROOT / "Launch_GameXXK_Editor.cmd"


class InteractiveEditorLauncherTests(unittest.TestCase):
    def launcher_text(self) -> str:
        self.assertTrue(
            LAUNCHER.is_file(),
            "The root-level interactive GameXXK editor launcher is missing",
        )
        return LAUNCHER.read_text(encoding="utf-8")

    def test_isolates_turnkey_and_shader_writes_under_project_saved(self) -> None:
        text = self.launcher_text()

        for variable in (
            "USERPROFILE",
            "APPDATA",
            "LOCALAPPDATA",
            "TEMP",
            "TMP",
            "DOTNET_CLI_HOME",
            "NUGET_PACKAGES",
            "UE-LocalDataCachePath",
        ):
            self.assertIn(f'set "{variable}=', text)
        self.assertIn('-UserDir="%GAMEXXK_USER_DIR%"', text)
        self.assertIn('-ShaderWorkingDir="%GAMEXXK_SHADER_DIR%"', text)

    def test_launches_gamexxk_in_visible_ue58_with_mcp(self) -> None:
        text = self.launcher_text()

        self.assertIn(r"D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe", text)
        self.assertIn(r'"%GAMEXXK_ROOT%\GameXXK.uproject"', text)
        self.assertIn("-ModelContextProtocolStartServer", text)
        self.assertIn("-ModelContextProtocolPort=18765", text)
        self.assertNotIn("-Unattended", text)
        self.assertNotIn("-UnattendedInput", text)


if __name__ == "__main__":
    unittest.main()
