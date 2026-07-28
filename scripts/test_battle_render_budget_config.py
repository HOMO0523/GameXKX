"""Regression checks for the battle-friendly UE render-memory budget."""

from __future__ import annotations

import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SCALABILITY_CONFIG = PROJECT_ROOT / "Config" / "DefaultScalability.ini"


class BattleRenderBudgetConfigTest(unittest.TestCase):
    def test_epic_shadow_quality_caps_virtual_shadow_page_pool(self) -> None:
        self.assertTrue(
            SCALABILITY_CONFIG.is_file(),
            "DefaultScalability.ini must reserve a battle-friendly VSM memory budget.",
        )
        contents = SCALABILITY_CONFIG.read_text(encoding="utf-8")
        epic_shadow_section = re.search(
            r"^\[ShadowQuality@3\]\s*$([\s\S]*?)(?=^\[|\Z)",
            contents,
            flags=re.MULTILINE,
        )
        self.assertIsNotNone(epic_shadow_section, "Epic shadow quality needs its own project override section.")
        self.assertRegex(
            epic_shadow_section.group(1) if epic_shadow_section else "",
            r"(?m)^r\.Shadow\.Virtual\.MaxPhysicalPages\s*=\s*1024\s*$",
            "Epic shadow quality must cap VSM pages at 1024 instead of the 4096-page default.",
        )


if __name__ == "__main__":
    unittest.main()
