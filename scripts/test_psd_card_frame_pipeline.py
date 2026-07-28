"""Pure-Python checks for the approved PSD card-frame import contract."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PIPELINE_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_import_psd_card_frame.py"
SOURCE_ROOT = (
    Path(r"C:\Users\shxuw\Downloads\nw-studio-nwueball-https-github-com")
    / "nw-studio-nwueball-https-github-com"
    / "work"
    / "psd_rebuild"
    / "clean_assets_v2"
)


def load_pipeline_module():
    spec = importlib.util.spec_from_file_location("gamexxk_import_psd_card_frame", PIPELINE_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable to load card-frame pipeline: {PIPELINE_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class PsdCardFramePipelineTests(unittest.TestCase):
    def test_approved_057_source_matches_locked_contract(self) -> None:
        pipeline = load_pipeline_module()

        metadata = pipeline.verify_approved_source(SOURCE_ROOT / "057.png")

        self.assertEqual(metadata["name"], "057.png")
        self.assertEqual(metadata["width"], 452)
        self.assertEqual(metadata["height"], 516)
        self.assertEqual(
            metadata["sha256"],
            "c9b0333eca9a21c45f79450db5c4f940eb23c4ffbb4290807d4194cb44025209",
        )
        self.assertEqual(metadata["runtime_draw_size"], [113, 129])
        self.assertEqual(pipeline.RUNTIME_DRAW_SIZE, (113, 129))

    def test_second_row_source_is_explicitly_rejected(self) -> None:
        pipeline = load_pipeline_module()

        with self.assertRaisesRegex(ValueError, "057.png"):
            pipeline.verify_approved_source(SOURCE_ROOT / "060.png")


if __name__ == "__main__":
    unittest.main()
