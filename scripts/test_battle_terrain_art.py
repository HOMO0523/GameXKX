from __future__ import annotations

import hashlib
import json
import unittest
from pathlib import Path

from PIL import Image


PROJECT_ROOT = Path(__file__).resolve().parents[1]
TERRAIN_ROOT = (
    PROJECT_ROOT
    / "SourceAssets"
    / "PartyDeck"
    / "battle-backdrop"
    / "terrain-v2"
)
MANIFEST_PATH = TERRAIN_ROOT / "battle-terrain-manifest-v2.json"
EXPECTED_TERRAINS = {
    "WaterShore",
    "Plain",
    "Cliff",
    "Forest",
    "Ferry",
    "Village",
    "Cave",
}
EXPECTED_SIZE = (1672, 941)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class BattleTerrainArtTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))

    def test_manifest_declares_the_complete_approved_terrain_background_set(self) -> None:
        assets = self.manifest["assets"]
        self.assertEqual(len(assets), 7)
        self.assertEqual({asset["terrain"] for asset in assets}, EXPECTED_TERRAINS)
        self.assertEqual(tuple(self.manifest["shared_contract"]["size"]), EXPECTED_SIZE)
        self.assertEqual(self.manifest["shared_contract"]["mode"], "RGB")
        self.assertEqual(
            self.manifest["integration_status"],
            "approved_source_art_pending_runtime_import",
        )

    def test_each_approved_background_is_opaque_exact_size_and_hash_locked(self) -> None:
        for asset in self.manifest["assets"]:
            with self.subTest(terrain=asset["terrain"]):
                path = TERRAIN_ROOT / asset["source_image"]
                self.assertTrue(path.is_file())
                with Image.open(path) as image:
                    self.assertEqual(image.size, EXPECTED_SIZE)
                    self.assertEqual(image.mode, "RGB")
                    self.assertNotIn("A", image.getbands())
                self.assertEqual(sha256(path), asset["sha256"])

    def test_review_artifacts_are_present(self) -> None:
        review = self.manifest["review_artifacts"]
        contact_sheet = (TERRAIN_ROOT / review["background_contact_sheet"]).resolve()
        readability = (TERRAIN_ROOT / review["character_readability_preview"]).resolve()
        self.assertTrue(contact_sheet.is_file())
        self.assertTrue(readability.is_file())
        with Image.open(contact_sheet) as image:
            self.assertEqual(image.size, (1920, 1080))
        with Image.open(readability) as image:
            self.assertEqual(image.size, (1920, 1080))


if __name__ == "__main__":
    unittest.main()
