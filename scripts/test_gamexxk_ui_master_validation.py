import json
import tempfile
import unittest
from pathlib import Path

from scripts.build_gamexxk_ui_master import build_package
from scripts.gamexxk_ui_master_pages import PACKAGE, ROOT
from scripts.validate_gamexxk_ui_master import validate_package


class GameXXKUiMasterValidationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.package_root = Path(self.temporary.name) / "package"
        build_package(self.package_root)
        self.project_root = ROOT
        self.manifest_path = self.package_root / "master-manifest.json"
        self.manifest = json.loads(self.manifest_path.read_text(encoding="utf-8"))
        self.psd_path = self.package_root / "GameXXK_UI_Master_V1.psd"
        self.psd_path.write_bytes(b"8BPS-test-fixture")
        self.manifest["document"]["outputPsd"] = self.psd_path.as_posix()
        self.manifest_path.write_text(
            json.dumps(self.manifest, ensure_ascii=False, indent=2),
            encoding="utf-8",
        )
        groups = [page["group"] for page in self.manifest["pages"]]
        self.validation_path = self.psd_path.with_suffix(".validation.json")
        self.validation = {
            "width": 10080,
            "height": 4680,
            "artLayerCount": len(self.manifest["imageLayers"]) + len(self.manifest["textLayers"]),
            "expectedImageLayers": len(self.manifest["imageLayers"]),
            "expectedTopLevelGroups": 18,
            "actualTopLevelGroups": groups,
            "expectedTextLayers": len(self.manifest["textLayers"]),
            "actualTextLayers": len(self.manifest["textLayers"]),
            "textRoundTripMatch": True,
            "outputPsd": self.psd_path.as_posix(),
        }
        self._write_validation()

    def tearDown(self):
        self.temporary.cleanup()

    def _write_validation(self):
        self.validation_path.write_text(
            json.dumps(self.validation, ensure_ascii=False, indent=2),
            encoding="utf-8",
        )

    def test_validator_accepts_complete_phase_a_package(self):
        report = validate_package(self.package_root, self.project_root)
        self.assertTrue(report["ok"], report)

    def test_validator_rejects_missing_page_group(self):
        self.validation["actualTopLevelGroups"].remove("05_图鉴")
        self._write_validation()
        report = validate_package(self.package_root, self.project_root)
        self.assertIn("missing top-level group: 05_图鉴", report["errors"])

    def test_validator_rejects_changed_idle_hash(self):
        source_lock_path = self.package_root / "source-lock.json"
        source_lock = json.loads(source_lock_path.read_text(encoding="utf-8"))
        source_lock["heroIdle"]["sha256"] = "0" * 64
        source_lock_path.write_text(
            json.dumps(source_lock, ensure_ascii=False, indent=2),
            encoding="utf-8",
        )
        report = validate_package(self.package_root, self.project_root)
        self.assertIn("source hash mismatch: heroIdle", report["errors"])


if __name__ == "__main__":
    unittest.main()
