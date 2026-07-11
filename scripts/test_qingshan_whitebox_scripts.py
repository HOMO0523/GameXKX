import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
IMPLEMENTATION = ROOT / "Source/GameXXKEditor/Private/GameXXKTownPCGAutomationLibrary.cpp"


class QingshanWhiteboxRootSafetyTests(unittest.TestCase):
    def test_editor_automation_uses_only_approved_graph_and_map_roots(self):
        source = IMPLEMENTATION.read_text(encoding="utf-8")

        self.assertIn('/Game/GameXXK/Environment/TownPCG/VerticalSlice/', source)
        self.assertIn('/Game/GameXXK/Environment/TownPCG/B0R/', source)
        self.assertIn('/Game/GameXXK/Maps/Prototype/', source)
        self.assertIn('/Game/GameXXK/Maps/Dev/', source)
        self.assertIn('IsManagedGraphPath', source)
        self.assertIn('IsManagedPrototypeMapPath', source)

        unrestricted_roots = (
            "/Game/",
            "/Game/GameXXK/",
            "/Game/GameXXK/Maps/",
        )
        for root in unrestricted_roots:
            with self.subTest(root=root):
                self.assertIsNone(
                    re.search(
                        rf"StartsWith\(\s*(?:TEXT\()?['\"]{re.escape(root)}['\"]",
                        source,
                    )
                )


if __name__ == "__main__":
    unittest.main()
