from __future__ import annotations

import unittest

from scripts.validate_meta_shop_ui_v2 import EXPECTED, validate_sources


class MetaShopUiV2ValidationTest(unittest.TestCase):
    def test_all_approved_sources_match_manifest_and_targets(self) -> None:
        assets = validate_sources()
        self.assertEqual(len(assets), 7)
        self.assertEqual([asset["name"] for asset in assets], list(EXPECTED))
        self.assertEqual([asset["target"] for asset in assets], [value[0] for value in EXPECTED.values()])
        self.assertTrue(all(len(asset["sha256"]) == 64 for asset in assets))


if __name__ == "__main__":
    unittest.main()
