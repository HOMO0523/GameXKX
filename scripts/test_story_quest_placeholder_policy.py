#!/usr/bin/env python3
from __future__ import annotations

import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
WORKBENCH_CPP = (
    PROJECT_ROOT
    / "Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp"
)


class StoryQuestPlaceholderPolicyTest(unittest.TestCase):
    def test_button_builder_has_no_story_or_stateful_dependencies(self) -> None:
        source = WORKBENCH_CPP.read_text(encoding="utf-8")
        marker = (
            "void UGameXXKDesktopTrainingWorkbenchWidget::"
            "BuildStoryQuestButton()"
        )
        self.assertIn(marker, source)
        body = source.split(marker, 1)[1].split(
            "void UGameXXKDesktopTrainingWorkbenchWidget::"
            "BuildBackpackTabToggle()",
            1,
        )[0]
        self.assertIn("ActionStoryQuest", body)
        self.assertIn("DesktopStoryQuestButtonTexturePath", body)
        for forbidden in (
            "StoryTaskDrawer",
            "Narrative",
            "Guide",
            "SetNotice",
            "RefreshLayout",
            "RequestTownToggle",
        ):
            self.assertNotIn(forbidden, body)

    def test_action_654_is_an_explicit_no_op(self) -> None:
        source = WORKBENCH_CPP.read_text(encoding="utf-8")
        self.assertRegex(
            source,
            re.compile(
                r"if\s*\(ActionId\s*==\s*ActionStoryQuest\)\s*"
                r"\{\s*return;\s*\}",
                re.MULTILINE,
            ),
        )


if __name__ == "__main__":
    unittest.main()
