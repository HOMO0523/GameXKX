import copy
import json
import tempfile
import unittest
from pathlib import Path

from scripts.validate_guide_json import (
    GuideCatalogSnapshot,
    canonicalize_guide,
    validate_file,
    validate_guide,
)


PROJECT_ROOT = Path(__file__).resolve().parents[1]


def make_catalogs() -> GuideCatalogSnapshot:
    return GuideCatalogSnapshot(
        target_ids=frozenset({"Route.Tutorial.NextNode", "Battle.Hud.PartyQi"}),
        trigger_event_ids=frozenset({"Event.Route.Opened", "Event.Battle.Opened"}),
        completion_event_ids=frozenset(
            {"Event.Route.NextNodeSelected", "Event.Guide.Done"}
        ),
        action_ids=frozenset({"Action.Route.SelectNext"}),
    )


def make_valid_guide() -> dict:
    return {
        "schemaVersion": 1,
        "guideId": "Guide.Test.Route",
        "guideVersion": 1,
        "entryStep": "forced",
        "steps": {
            "forced": {
                "triggerEvent": "Event.Route.Opened",
                "target": "Route.Tutorial.NextNode",
                "inputPolicy": "forced",
                "text": "选择下一个节点",
                "allowedActions": ["Action.Route.SelectNext"],
                "completionEvent": "Event.Route.NextNodeSelected",
                "next": "soft",
            },
            "soft": {
                "triggerEvent": "Event.Battle.Opened",
                "target": "Battle.Hud.PartyQi",
                "inputPolicy": "soft",
                "text": "观察全队气力",
                "allowedActions": [],
                "completionEvent": "Event.Guide.Done",
            },
        },
    }


class GuideJsonValidationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.catalogs = make_catalogs()

    def assert_has_error(self, payload: dict, expected: str) -> None:
        errors = validate_guide(payload, self.catalogs)
        self.assertTrue(
            any(expected in error for error in errors),
            f"expected {expected!r}, got {errors!r}",
        )

    def test_valid_guide_is_canonical(self) -> None:
        payload = make_valid_guide()
        self.assertEqual([], validate_guide(payload, self.catalogs))
        canonical = canonicalize_guide(payload)
        self.assertEqual(sorted(payload["steps"]), list(canonical["steps"]))
        self.assertEqual(payload, canonical)

    def test_dangling_and_unreachable_steps_are_rejected(self) -> None:
        dangling = make_valid_guide()
        dangling["steps"]["forced"]["next"] = "missing"
        self.assert_has_error(dangling, "missing target missing")

        unreachable = make_valid_guide()
        unreachable["steps"]["orphan"] = copy.deepcopy(unreachable["steps"]["soft"])
        self.assert_has_error(unreachable, "unreachable step: orphan")

    def test_unknown_semantic_ids_are_rejected(self) -> None:
        cases = (
            ("triggerEvent", "Event.Unknown", "unknown trigger event Event.Unknown"),
            ("completionEvent", "Event.Unknown", "unknown completion event Event.Unknown"),
            ("target", "Widget.InternalName", "unknown target Widget.InternalName"),
        )
        for field, value, expected in cases:
            with self.subTest(field=field):
                payload = make_valid_guide()
                payload["steps"]["forced"][field] = value
                self.assert_has_error(payload, expected)

        unknown_action = make_valid_guide()
        unknown_action["steps"]["forced"]["allowedActions"] = ["Action.Unknown"]
        self.assert_has_error(unknown_action, "unknown allowed action Action.Unknown")

    def test_forced_step_requires_target_and_allowed_action(self) -> None:
        missing_target = make_valid_guide()
        missing_target["steps"]["forced"]["target"] = ""
        self.assert_has_error(missing_target, "forced target must not be empty")

        missing_action = make_valid_guide()
        missing_action["steps"]["forced"]["allowedActions"] = []
        self.assert_has_error(missing_action, "forced step requires an allowed action")

    def test_more_than_one_terminal_path_is_rejected(self) -> None:
        payload = make_valid_guide()
        payload["steps"]["orphan.terminal"] = {
            "triggerEvent": "Event.Battle.Opened",
            "target": "Battle.Hud.PartyQi",
            "inputPolicy": "soft",
            "text": "不应存在的第二终点",
            "allowedActions": [],
            "completionEvent": "Event.Guide.Done",
        }
        self.assert_has_error(payload, "exactly one terminal step")

    def test_validate_file_rejects_duplicate_json_keys(self) -> None:
        duplicate_json = """{
          "schemaVersion": 1,
          "guideId": "Guide.Test.Duplicate",
          "guideVersion": 1,
          "entryStep": "start",
          "steps": {
            "start": {"triggerEvent":"Event.Route.Opened","target":"Route.Tutorial.NextNode","inputPolicy":"forced","text":"甲","allowedActions":["Action.Route.SelectNext"],"completionEvent":"Event.Route.NextNodeSelected"},
            "start": {"triggerEvent":"Event.Route.Opened","target":"Route.Tutorial.NextNode","inputPolicy":"forced","text":"乙","allowedActions":["Action.Route.SelectNext"],"completionEvent":"Event.Route.NextNodeSelected"}
          }
        }"""
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "duplicate.guide.json"
            path.write_text(duplicate_json, encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate JSON key: start"):
                validate_file(path, self.catalogs)

    def test_validate_file_returns_canonical_payload(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "valid.guide.json"
            path.write_text(
                json.dumps(make_valid_guide(), ensure_ascii=False), encoding="utf-8"
            )
            canonical = validate_file(path, self.catalogs)
            self.assertEqual("Guide.Test.Route", canonical["guideId"])

    def test_unreal_importer_validates_before_mutation(self) -> None:
        importer_path = (
            PROJECT_ROOT / "Content" / "Python" / "gamexxk_import_guide_json.py"
        )
        self.assertTrue(importer_path.is_file(), f"missing importer: {importer_path}")
        source = importer_path.read_text(encoding="utf-8")
        compile(source, str(importer_path), "exec")
        for required in (
            "validate_file",
            "AssetToolsHelpers.get_asset_tools",
            "EditorAssetLibrary.load_asset",
            "create_asset",
            "GameXXKGuideAsset",
            "set_editor_property",
            "save_loaded_asset",
            "guide-import-report.json",
        ):
            self.assertIn(required, source)
        self.assertLess(source.index("validate_file"), source.index("create_asset"))
        self.assertIn("does_asset_exist", source)
        self.assertLess(source.index("does_asset_exist"), source.index("load_asset"))


if __name__ == "__main__":
    unittest.main()
