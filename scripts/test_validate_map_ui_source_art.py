"""Regression checks for the WorldMap source-art contract."""

from __future__ import annotations

import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = PROJECT_ROOT / "scripts" / "validate_map_ui_source_art.py"
SOURCE_ROOT = PROJECT_ROOT / "docs" / "ui" / "maps" / "source_art" / "WorldMap"


class WorldMapSourceArtContractTest(unittest.TestCase):
    def run_validator(self, root: Path = SOURCE_ROOT) -> tuple[int, dict[str, object], str]:
        result = subprocess.run(
            [sys.executable, str(VALIDATOR), "--check", "--root", str(root)],
            cwd=PROJECT_ROOT,
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
        try:
            report = json.loads(result.stdout)
        except json.JSONDecodeError:
            report = {"ok": False, "layers": [], "errors": [result.stderr or result.stdout]}
        return result.returncode, report, result.stderr

    def make_fixture(self) -> tuple[tempfile.TemporaryDirectory[str], Path, dict[str, object]]:
        temporary_directory = tempfile.TemporaryDirectory()
        fixture_root = Path(temporary_directory.name) / "WorldMap"
        shutil.copytree(SOURCE_ROOT, fixture_root)
        manifest_path = fixture_root / "manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        return temporary_directory, fixture_root, manifest

    @staticmethod
    def save_manifest(root: Path, manifest: dict[str, object]) -> None:
        (root / "manifest.json").write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

    def test_current_world_map_source_art_passes_the_contract(self) -> None:
        return_code, report, stderr = self.run_validator()

        self.assertEqual(return_code, 0, stderr or json.dumps(report, ensure_ascii=False))
        self.assertTrue(report["ok"])
        self.assertEqual(len(report["layers"]), 6)
        self.assertFalse(any(layer["file"] == "094.png" for layer in report["layers"]))

    def test_rejects_the_baked_psd_map_as_a_runtime_layer(self) -> None:
        temporary_directory, fixture_root, manifest = self.make_fixture()
        self.addCleanup(temporary_directory.cleanup)
        manifest["layers"][0]["file"] = "094.png"
        self.save_manifest(fixture_root, manifest)

        return_code, report, _ = self.run_validator(fixture_root)

        self.assertNotEqual(return_code, 0)
        self.assertIn("baked PSD map", "\n".join(report["errors"]))

    def test_rejects_missing_required_layer(self) -> None:
        temporary_directory, fixture_root, manifest = self.make_fixture()
        self.addCleanup(temporary_directory.cleanup)
        manifest["layers"] = manifest["layers"][1:]
        self.save_manifest(fixture_root, manifest)

        return_code, report, _ = self.run_validator(fixture_root)

        self.assertNotEqual(return_code, 0)
        self.assertIn("missing required layers", "\n".join(report["errors"]))

    def test_rejects_a_marker_without_an_rgba_alpha_channel(self) -> None:
        temporary_directory, fixture_root, manifest = self.make_fixture()
        self.addCleanup(temporary_directory.cleanup)
        shutil.copy2(
            fixture_root / "world_map_terrain.png",
            fixture_root / "world_map_qingshan_marker.png",
        )

        return_code, report, _ = self.run_validator(fixture_root)

        self.assertNotEqual(return_code, 0)
        self.assertIn("must be RGBA", "\n".join(report["errors"]))

    def test_rejects_an_empty_fully_transparent_marker(self) -> None:
        temporary_directory, fixture_root, _ = self.make_fixture()
        self.addCleanup(temporary_directory.cleanup)
        marker_path = fixture_root / "world_map_qingshan_marker.png"
        with Image.open(marker_path) as raw:
            empty_marker = raw.convert("RGBA")
            empty_marker.putalpha(0)
            empty_marker.save(marker_path)

        return_code, report, _ = self.run_validator(fixture_root)

        self.assertNotEqual(return_code, 0)
        self.assertIn("no visible pixels", "\n".join(report["errors"]))

    def test_rejects_an_invalid_design_canvas(self) -> None:
        temporary_directory, fixture_root, manifest = self.make_fixture()
        self.addCleanup(temporary_directory.cleanup)
        manifest["canvas"]["width"] = 1600
        self.save_manifest(fixture_root, manifest)

        return_code, report, _ = self.run_validator(fixture_root)

        self.assertNotEqual(return_code, 0)
        self.assertIn("canvas must be 1920x1080", "\n".join(report["errors"]))

    def test_rejects_missing_reference_provenance_path(self) -> None:
        temporary_directory, fixture_root, manifest = self.make_fixture()
        self.addCleanup(temporary_directory.cleanup)
        del manifest["sourceReference"]["path"]
        self.save_manifest(fixture_root, manifest)

        return_code, report, _ = self.run_validator(fixture_root)

        self.assertNotEqual(return_code, 0)
        self.assertIn("sourceReference path", "\n".join(report["errors"]))

    def test_rejects_a_label_plate_without_text_restrictions(self) -> None:
        temporary_directory, fixture_root, manifest = self.make_fixture()
        self.addCleanup(temporary_directory.cleanup)
        label_plate = next(layer for layer in manifest["layers"] if layer["name"] == "world_map_label_plate")
        label_plate["forbiddenContent"].remove("text")
        self.save_manifest(fixture_root, manifest)

        return_code, report, _ = self.run_validator(fixture_root)

        self.assertNotEqual(return_code, 0)
        self.assertIn("forbiddenContent", "\n".join(report["errors"]))

    def test_emits_utf8_json_when_the_source_root_has_a_unicode_name(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            fixture_root = Path(temporary_directory) / "世界地图"
            shutil.copytree(SOURCE_ROOT, fixture_root)
            result = subprocess.run(
                [sys.executable, str(VALIDATOR), "--check", "--root", str(fixture_root)],
                cwd=PROJECT_ROOT,
                check=False,
                capture_output=True,
            )

        try:
            output = result.stdout.decode("utf-8")
            is_utf8 = True
        except UnicodeDecodeError:
            output = ""
            is_utf8 = False

        self.assertTrue(is_utf8, result.stdout.decode(errors="replace"))
        report = json.loads(output)
        self.assertEqual(result.returncode, 0, result.stderr.decode(errors="replace"))
        self.assertTrue(report["ok"])


if __name__ == "__main__":
    unittest.main()
