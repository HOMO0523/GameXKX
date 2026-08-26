from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).resolve().parents[1]
WORKBENCH_HEADER = PROJECT_ROOT / "Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h"
WORKBENCH_CPP = PROJECT_ROOT / "Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp"
BATTLE_BOARD_CPP = PROJECT_ROOT / "Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp"
MVP_SUBSYSTEM_HEADER = PROJECT_ROOT / "Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h"


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
        # The desktop Challenge opens its own generated route map and lets the
        # player pick every battle node. It must never bypass into a fixed
        # battle, accept the town quest, or auto-select a route/party entry.
        self.assertIn("StartTrainingChallenge", action)
        self.assertIn("NotifyPlayerFlowStateChanged", action)
        for forbidden in (
            "AcceptQuest",
            "SelectDungeonNode",
            "SelectRouteNodeById",
            "SelectTownQuestNpcForParty",
        ):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, action)

    def test_auto_battle_uses_only_board_owned_combat_actions(self) -> None:
        source = BATTLE_BOARD_CPP.read_text(encoding="utf-8")
        action = source[
            source.index("bool UGameXXKBattleBoardWidget::AdvanceAutoBattleStep()") :
            source.index("bool UGameXXKBattleBoardWidget::SubmitPendingInsightChoice")
        ]
        for required in (
            "ClickCardInHand",
            "ConfirmTargetingUnit",
            "SubmitPendingForcedDiscards",
            "SubmitPendingInsightChoice",
            "SubmitPendingHeroTaskSearchChoice",
            "EndCardPlayerPhase",
        ):
            with self.subTest(required=required):
                self.assertIn(required, action)
        for forbidden in (
            "SelectRouteNodeById",
            "SelectDungeonNode",
            "ResolveRouteEncounterChoice",
            "ResolveEventReward",
            "ChoosePendingBattleRewardOption",
            "SkipPendingRouteReward",
            "FailDungeonToTown",
        ):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, action)

    def test_auto_battle_preference_is_transient_not_save_state(self) -> None:
        header = MVP_SUBSYSTEM_HEADER.read_text(encoding="utf-8")
        field = "bool bBattleAutoPlayEnabled = false;"
        self.assertIn(field, header)
        prefix = header[max(0, header.index(field) - 120) : header.index(field)]
        self.assertIn("UPROPERTY(Transient)", prefix)
        self.assertNotIn("SaveGame", prefix)


if __name__ == "__main__":
    unittest.main()
