"""Regression guard for the retired project-local vision integration."""

from __future__ import annotations

import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
RETIRED_FILES = (
    PROJECT_ROOT / "scripts/gamexxk_vision.py",
    PROJECT_ROOT / "scripts/gamexxk_vision_pie.py",
    PROJECT_ROOT / "scripts/test_gamexxk_vision.py",
    PROJECT_ROOT / "scripts/test_gamexxk_vision_pie.py",
)
TEXT_ROOTS = (
    PROJECT_ROOT / "scripts",
    PROJECT_ROOT / "docs/production",
    PROJECT_ROOT / "Content/Python",
    PROJECT_ROOT / "Config",
    PROJECT_ROOT / "Source",
)
TEXT_SUFFIXES = {
    ".md",
    ".txt",
    ".py",
    ".ps1",
    ".json",
    ".ini",
    ".cs",
    ".h",
    ".cpp",
}
SELF = Path(__file__).resolve()


class ProviderVisionRetirementTests(unittest.TestCase):
    def test_retired_modules_are_absent(self) -> None:
        existing = [
            str(path.relative_to(PROJECT_ROOT))
            for path in RETIRED_FILES
            if path.exists()
        ]
        self.assertEqual(existing, [])

    def test_live_project_text_has_no_provider_integration_reference(self) -> None:
        provider_token = "deep" + "seek"
        module_token = "gamexxk_" + "vision"
        hits: list[str] = []
        for root in TEXT_ROOTS:
            if not root.exists():
                continue
            for path in root.rglob("*"):
                if not path.is_file() or path.suffix.lower() not in TEXT_SUFFIXES:
                    continue
                if path.resolve() == SELF:
                    continue
                text = path.read_text(encoding="utf-8", errors="ignore").lower()
                if provider_token in text or module_token in text:
                    hits.append(str(path.relative_to(PROJECT_ROOT)))
        self.assertEqual(hits, [])

    def test_global_agent_policy_does_not_name_a_visual_reviewer(self) -> None:
        policy = (PROJECT_ROOT / "AGENTS.md").read_text(encoding="utf-8").lower()
        self.assertNotIn("lu" + "na", policy)
        self.assertNotIn("codex" + "-vision", policy)


if __name__ == "__main__":
    unittest.main()
