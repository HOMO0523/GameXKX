import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
BUILDER = ROOT / "scripts/build_gamexxk_ui_master.py"


class GameXXKUiMasterBuildTests(unittest.TestCase):
    def test_cli_builds_master_manifest_previews_and_contact_sheet(self):
        with tempfile.TemporaryDirectory() as td:
            result = subprocess.run(
                [sys.executable, str(BUILDER), "--output-root", td],
                cwd=ROOT,
                capture_output=True,
                text=True,
                check=False,
                encoding="utf-8",
                errors="replace",
            )
            self.assertEqual(0, result.returncode, result.stderr)
            report = json.loads(result.stdout)
            self.assertEqual([10080, 4680], report["masterCanvas"])
            self.assertEqual(18, report["pageGroups"])
            output = Path(td)
            manifest = json.loads(
                (output / "master-manifest.json").read_text(encoding="utf-8")
            )
            self.assertEqual(18, len(manifest["pages"]))
            self.assertEqual(18, len(list((output / "Previews").glob("*.png"))))
            self.assertTrue(manifest["imageLayers"])
            self.assertTrue(manifest["textLayers"])
            with Image.open(output / "GameXXK_UI_Master_ContactSheet.png") as sheet:
                self.assertEqual((2400, 1080), sheet.size)


if __name__ == "__main__":
    unittest.main()
