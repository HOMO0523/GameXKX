import hashlib
import unittest
from pathlib import Path

from scripts.validate_guide_json import DEFAULT_CATALOGS, validate_file, validate_guide


PROJECT_ROOT = Path(__file__).resolve().parents[1]
GUIDE_DIR = PROJECT_ROOT / "SourceAssets" / "Narrative" / "Guides"


class Tutorial01GuideJsonValidationTests(unittest.TestCase):
    def test_multi_target_schema_vocabulary_is_supported(self) -> None:
        payload = {
            "schemaVersion": 1,
            "guideId": "Guide.Battle.Tutorial01.NewPlayer",
            "guideVersion": 1,
            "entryStep": "Guide.Battle.Tutorial01.Qi",
            "steps": {
                "Guide.Battle.Tutorial01.Qi": {
                    "triggerEvent": "Event.Battle.Opened",
                    "target": "Battle.Hud.PartyQi",
                    "additionalTargets": ["Battle.Unit.Hero.Health"],
                    "bubbleAnchor": "Battle.Unit.YueBai.Visual",
                    "missingTargetPolicy": "abort",
                    "inputPolicy": "forced",
                    "text": "气力值：每回合出牌时消耗的点数。",
                    "allowedActions": ["Action.Guide.Continue"],
                    "completionEvent": "Event.Tutorial01.Continue",
                }
            },
        }
        self.assertEqual([], validate_guide(payload, DEFAULT_CATALOGS))

        duplicate = payload.copy()
        duplicate["steps"] = {
            key: dict(value) for key, value in payload["steps"].items()
        }
        duplicate["steps"]["Guide.Battle.Tutorial01.Qi"][
            "additionalTargets"
        ] = ["Battle.Unit.Hero.Health", "Battle.Unit.Hero.Health"]
        self.assertTrue(
            any(
                "additionalTargets must be unique" in error
                for error in validate_guide(duplicate, DEFAULT_CATALOGS)
            )
        )

    def test_exact_nine_step_tutorial_source_and_legacy_hash(self) -> None:
        path = GUIDE_DIR / "Guide.Battle.Tutorial01.NewPlayer.guide.json"
        self.assertTrue(path.is_file(), f"missing tutorial guide source: {path}")
        payload = validate_file(path, DEFAULT_CATALOGS)
        expected_steps = [
            "Guide.Battle.Tutorial01.Qi",
            "Guide.Battle.Tutorial01.HeroVitals",
            "Guide.Battle.Tutorial01.EnemyIntent",
            "Guide.Battle.Tutorial01.HengJian",
            "Guide.Battle.Tutorial01.SuiYan",
            "Guide.Battle.Tutorial01.FengShen",
            "Guide.Battle.Tutorial01.ForcedDiscard",
            "Guide.Battle.Tutorial01.EndTurn",
            "Guide.Battle.Tutorial01.AutoBattle",
        ]
        cursor = payload["entryStep"]
        actual_steps = []
        while cursor:
            actual_steps.append(cursor)
            cursor = payload["steps"][cursor].get("next")
        self.assertEqual(expected_steps, actual_steps)
        self.assertEqual(9, len(payload["steps"]))

        legacy_path = GUIDE_DIR / "Guide.Battle.Basic.guide.json"
        self.assertEqual(
            "b7d1e5be97578b662a13392130a015ef7da7448dcb28c14ad1c028392ab40d1d",
            hashlib.sha256(legacy_path.read_bytes()).hexdigest(),
        )

    def test_importer_maps_multi_target_fields(self) -> None:
        importer_path = (
            PROJECT_ROOT / "Content" / "Python" / "gamexxk_import_guide_json.py"
        )
        source = importer_path.read_text(encoding="utf-8")
        for required in (
            "additional_target_ids",
            "bubble_anchor_target_id",
            "missing_target_policy",
            "GameXXKGuideMissingTargetPolicy",
        ):
            self.assertIn(required, source)


if __name__ == "__main__":
    unittest.main()
