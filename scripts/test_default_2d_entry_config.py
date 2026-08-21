import configparser
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ENGINE = PROJECT_ROOT / "Config" / "DefaultEngine.ini"
DEFAULT_GAME = PROJECT_ROOT / "Config" / "DefaultGame.ini"
DESKTOP_MAP = "/Game/GameXXK/Maps/L_DesktopTrainingHUD"


def read_unreal_ini(path: Path) -> configparser.ConfigParser:
    parser = configparser.ConfigParser(strict=False, interpolation=None)
    parser.optionxform = str
    parser.read(path, encoding="utf-8-sig")
    return parser


class Default2DEntryConfigTest(unittest.TestCase):
    def test_editor_and_game_start_on_the_desktop_training_hud(self) -> None:
        config = read_unreal_ini(DEFAULT_ENGINE)
        maps = config["/Script/EngineSettings.GameMapsSettings"]
        self.assertEqual(maps["GameDefaultMap"], DESKTOP_MAP)
        self.assertEqual(maps["EditorStartupMap"], DESKTOP_MAP)

    def test_desktop_training_hud_is_explicitly_cooked(self) -> None:
        text = DEFAULT_GAME.read_text(encoding="utf-8-sig")
        self.assertIn(
            f'+MapsToCook=(FilePath="{DESKTOP_MAP}")',
            text,
        )


if __name__ == "__main__":
    unittest.main()
