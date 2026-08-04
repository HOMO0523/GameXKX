"""Regression tests for the project-local authoritative town PSD package."""

from __future__ import annotations

import base64
import importlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PSD_COMPOSER_SCRIPT = PROJECT_ROOT / "scripts" / "ui_psd_pipeline" / "build-psd.js"
PSD_PHOTOSHOP_RUNNER = PROJECT_ROOT / "scripts" / "ui_psd_pipeline" / "run-photoshop.ps1"
if str(PROJECT_ROOT / "scripts") not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

try:
    validator_module = importlib.import_module("validate_town_psd_package")
    validate_package = validator_module.validate_package
except ModuleNotFoundError:
    validate_package = None


TRANSPARENT_PNG = base64.b64decode(
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVQIHWP4z8DwHwAFgAI/"
    "f2pZLQAAAABJRU5ErkJggg=="
)


def write_package(
    root: Path,
    *,
    width: int = 4096,
    height: int = 4096,
    scale: int = 4,
    image_name: str = "框_测试",
    image_path: str = "clean_assets/asset.png",
    semantic_buttons: list[str] | None = None,
    ue_asset_names: list[str] | None = None,
    text_layers: list[dict[str, object]] | None = None,
    output_psd: str = "outputs/UI_PSD/GameXXK_Town_4K.psd",
    include_runtime_backgrounds: bool = True,
) -> None:
    clean_assets = root / "clean_assets"
    clean_assets.mkdir(parents=True)
    (clean_assets / "asset.png").write_bytes(TRANSPARENT_PNG)
    (root / "manifest.json").write_text(
        json.dumps(
            {
                "document": {
                    "width": width,
                    "height": height,
                    "scale": scale,
                    "outputPsd": output_psd,
                },
                "imageLayers": [{"name": image_name, "path": image_path}],
                "textLayers": text_layers or [],
            },
            ensure_ascii=False,
        ),
        encoding="utf-8",
    )
    semantic_assets = [
        {
            "manifestLayer": f"按钮_{semantic}",
            "cleanAssetFile": "clean_assets/asset.png",
            "group": "Controls",
            "ueAssetName": (ue_asset_names or [])[index] if ue_asset_names else f"T_Button_{semantic}",
            "buttonSemantic": semantic,
        }
        for index, semantic in enumerate(semantic_buttons or [])
    ]
    runtime_backgrounds: list[dict[str, object]] = []
    if include_runtime_backgrounds:
        backgrounds_directory = root / "runtime_backgrounds"
        backgrounds_directory.mkdir()
        for page in ("hud", "character", "companion", "task", "map", "backpack"):
            filename = f"T_TownPsd_Background_{page.title()}.png"
            (backgrounds_directory / filename).write_bytes(TRANSPARENT_PNG)
            runtime_backgrounds.append(
                {
                    "page": page,
                    "file": f"runtime_backgrounds/{filename}",
                }
            )
    (root / "semantic-map.json").write_text(
        json.dumps(
            {"assets": semantic_assets, "runtimeBackgrounds": runtime_backgrounds},
            ensure_ascii=False,
        ),
        encoding="utf-8",
    )


class TownPsdPackageTest(unittest.TestCase):
    def test_rejects_non_4k_manifest(self) -> None:
        self.assertIsNotNone(validate_package, "validator module must export validate_package(root)")
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            write_package(root, width=1024, height=1024, scale=1)
            self.assertIn("document must be 4096x4096 at scale 4", validate_package(root))

    def test_rejects_text_baked_into_image_layer(self) -> None:
        self.assertIsNotNone(validate_package, "validator module must export validate_package(root)")
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            write_package(root, image_name="按钮_确认_文字")
            self.assertIn("image layer name must not declare baked runtime text", validate_package(root))

    def test_rejects_missing_semantic_button_family(self) -> None:
        self.assertIsNotNone(validate_package, "validator module must export validate_package(root)")
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            write_package(root, semantic_buttons=["neutral", "primary"])
            self.assertIn("missing semantic button family: destructive", validate_package(root))

    def test_rejects_missing_runtime_page_background(self) -> None:
        self.assertIsNotNone(validate_package, "validator module must export validate_package(root)")
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            write_package(
                root,
                semantic_buttons=["neutral", "primary", "destructive"],
                include_runtime_backgrounds=False,
            )
            self.assertTrue(
                any(
                    error.startswith("missing runtime page backgrounds")
                    for error in validate_package(root)
                )
            )

    def test_accepts_complete_minimum_package(self) -> None:
        self.assertIsNotNone(validate_package, "validator module must export validate_package(root)")
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            write_package(root, semantic_buttons=["neutral", "primary", "destructive"])
            self.assertEqual([], validate_package(root))

    def test_rejects_missing_image_layer_file(self) -> None:
        self.assertIsNotNone(validate_package, "validator module must export validate_package(root)")
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            write_package(
                root,
                image_path="clean_assets/missing.png",
                semantic_buttons=["neutral", "primary", "destructive"],
            )
            self.assertIn("missing image layer file: clean_assets/missing.png", validate_package(root))

    def test_rejects_duplicate_semantic_ue_asset_name(self) -> None:
        self.assertIsNotNone(validate_package, "validator module must export validate_package(root)")
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            write_package(
                root,
                semantic_buttons=["neutral", "primary", "destructive"],
                ue_asset_names=["T_ButtonShared", "T_ButtonShared", "T_ButtonDestructive"],
            )
            self.assertIn("duplicate ueAssetName: T_ButtonShared", validate_package(root))

    def test_rejects_incomplete_editable_text_layer(self) -> None:
        self.assertIsNotNone(validate_package, "validator module must export validate_package(root)")
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            write_package(
                root,
                semantic_buttons=["neutral", "primary", "destructive"],
                text_layers=[{"name": "文字_标题", "text": "青山镇"}],
            )
            self.assertIn("invalid editable text layer: 0", validate_package(root))

    def test_rejects_psd_output_outside_project_output_folder(self) -> None:
        self.assertIsNotNone(validate_package, "validator module must export validate_package(root)")
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            write_package(
                root,
                semantic_buttons=["neutral", "primary", "destructive"],
                output_psd="outputs/old/legacy.psd",
            )
            self.assertIn("document outputPsd must be under outputs/UI_PSD/", validate_package(root))

    def test_cli_reports_machine_readable_success(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            write_package(root, semantic_buttons=["neutral", "primary", "destructive"])
            result = subprocess.run(
                [sys.executable, str(PROJECT_ROOT / "scripts" / "validate_town_psd_package.py"), "--root", str(root)],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(0, result.returncode, result.stderr)
            self.assertTrue(result.stdout.strip(), "validator CLI must emit one JSON object")
            self.assertEqual({"ok": True, "errors": []}, json.loads(result.stdout))

    def test_composer_accepts_an_explicit_package_root(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            write_package(
                root,
                text_layers=[
                    {
                        "name": "text_title",
                        "text": "Town",
                        "x": 48,
                        "y": 72,
                        "fontSize": 24,
                        "font": "ArialMT",
                        "color": "#202020",
                    }
                ],
            )
            result = subprocess.run(
                ["node", str(PSD_COMPOSER_SCRIPT), "--root", str(root)],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(0, result.returncode, result.stderr)
            self.assertTrue((root / "compose.jsx").is_file())
            self.assertTrue((root / "svg_text" / "001_text_title.svg").is_file())

    def test_composer_derives_validation_path_from_output_psd_at_runtime(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            output_psd = root / "outputs" / "UI_PSD" / "GameXXK_Town_4K.psd"
            write_package(root, output_psd=output_psd.as_posix())

            result = subprocess.run(
                ["node", str(PSD_COMPOSER_SCRIPT), "--root", str(root)],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertEqual(0, result.returncode, result.stderr)
            compose = (root / "compose.jsx").read_text(encoding="utf-8-sig")
            self.assertIn(
                "var validationPath = String(spec.outputPsd).replace(/\\.psd$/i, '.validation.json');",
                compose,
            )
            self.assertIn("var validationFile = new File(validationPath);", compose)
            self.assertNotIn(output_psd.with_suffix(".validation.json").as_posix(), compose)

    def test_composer_supports_grouped_scaled_layers(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            root = Path(temporary_directory)
            write_package(root)
            manifest_path = root / "manifest.json"
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["document"]["overviewScale"] = 0.25
            manifest["pages"] = [{"group": "03_主角背包"}]
            manifest["imageLayers"][0].update(
                {
                    "name": "transparent_character",
                    "group": "03_主角背包/30_角色",
                    "x": 100,
                    "y": 80,
                    "width": 490,
                    "height": 490,
                    "fitMode": "contain_canvas",
                    "visible": False,
                }
            )
            master_manifest_path = root / "master-manifest.json"
            master_manifest_path.write_text(
                json.dumps(manifest, ensure_ascii=False),
                encoding="utf-8",
            )
            manifest_path.unlink()

            result = subprocess.run(
                [
                    "node",
                    str(PSD_COMPOSER_SCRIPT),
                    "--root",
                    str(root),
                    "--manifest",
                    "master-manifest.json",
                ],
                check=False,
                capture_output=True,
                text=True,
            )

            self.assertEqual(0, result.returncode, result.stderr)
            compose = (root / "compose.jsx").read_text(encoding="utf-8-sig")
            self.assertIn((root / "clean_assets" / "asset.png").as_posix(), compose)
            self.assertIn("sourceCanvasWidth", compose)
            self.assertIn("sourceCanvasHeight", compose)
            self.assertIn("item.fitMode == 'contain_canvas'", compose)
            self.assertIn(
                "Math.min(item.width / sourceCanvasWidth, item.height / sourceCanvasHeight)",
                compose,
            )
            self.assertIn(
                "duplicated.resize(canvasScale * 100, canvasScale * 100",
                compose,
            )
            self.assertIn("ensureGroupPath(doc, item.group)", compose)
            self.assertIn("function ensureGroupPath", compose)
            self.assertIn("topLevelGroups.push", compose)
            self.assertIn("actualTopLevelGroups", compose)
            self.assertIn("previewDoc.resizeImage", compose)
            self.assertIn("spec.overviewScale", compose)
            self.assertIn("doc.activeLayer = duplicated", compose)
            self.assertNotIn("resizeLayerTo(duplicated, item.width, item.height)", compose)
            self.assertIn("Failed to resize layer", compose)
            self.assertIn("duplicated.visible = item.visible !== false", compose)
            self.assertLess(
                compose.index("duplicated.resize(canvasScale * 100, canvasScale * 100"),
                compose.index("duplicated.visible = item.visible !== false"),
            )
            self.assertIn("walkLayers(reopened, actualTexts)", compose)
            rejected = subprocess.run(
                [
                    "node",
                    str(PSD_COMPOSER_SCRIPT),
                    "--root",
                    str(root),
                    "--manifest",
                    "../manifest.json",
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertNotEqual(0, rejected.returncode)
            self.assertIn(
                "manifest filename must stay inside package root", rejected.stderr
            )

    def test_photoshop_runner_exposes_a_safe_package_check(self) -> None:
        runner = PSD_PHOTOSHOP_RUNNER.read_text(encoding="utf-8")
        self.assertIn("[string]$Root", runner)
        self.assertIn("[switch]$CheckOnly", runner)
        self.assertIn("-not (Test-Path -LiteralPath $jsx)", runner)


if __name__ == "__main__":
    unittest.main()
