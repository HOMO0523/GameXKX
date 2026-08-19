from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
WORKBENCH_HEADER = PROJECT_ROOT / "Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h"
WORKBENCH_CPP = PROJECT_ROOT / "Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp"


class DesktopTrainingRouteOwnershipTest(unittest.TestCase):
    def combined_source(self) -> str:
        return WORKBENCH_HEADER.read_text(encoding="utf-8") + WORKBENCH_CPP.read_text(encoding="utf-8")

    def test_rejected_embedded_challenge_surface_is_retired(self) -> None:
        text = self.combined_source()
        for rejected in (
            "ChallengeViewport",
            "BuildChallengeViewport",
            "BuildChallengeCombatStrip",
            "ChallengeBattleBoard",
            "ChallengeAutoButton",
            "ChallengeAdvanceButton",
            "AutoBattleAccumulator",
            "bChallengeSidePanelsReadOnly",
            "TrainingNodeReadOnly_",
            "挑战中只读",
            "bReadOnly",
        ):
            with self.subTest(rejected=rejected):
                self.assertFalse(
                    rejected in text,
                    f"retired workbench symbol remains: {rejected}",
                )

    def test_challenge_action_delegates_without_choosing_route_or_party(self) -> None:
        source = WORKBENCH_CPP.read_text(encoding="utf-8")
        action = source[source.index("case 6:") : source.index("case 7:")]
        self.assertIn("OpenDungeonFromTownExit", action)
        self.assertIn("OpenMapForRuntimeState", action)
        for forbidden in (
            "AcceptQuest",
            "StartTrainingChallenge",
            "SelectDungeonNode",
            "SelectRouteNodeById",
            "SelectTownQuestNpcForParty",
        ):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, action)


if __name__ == "__main__":
    unittest.main()
