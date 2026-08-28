import copy
import json
import tempfile
import unittest
from pathlib import Path

from scripts.validate_dialogue_json import (
    CatalogSnapshot,
    canonicalize_dialogue,
    validate_dialogue,
    validate_file,
)


def make_catalogs() -> CatalogSnapshot:
    return CatalogSnapshot(
        speakers=frozenset({"Hero", "YueBai"}),
        roles=frozenset({"Hero", "YueBai"}),
        outcomes=frozenset(
            {
                "Outcome.Test.Left",
                "Outcome.Test.Right",
                "Outcome.Test.LeftDone",
                "Outcome.Test.RightDone",
            }
        ),
    )


def make_valid_dialogue() -> dict:
    return {
        "schemaVersion": 1,
        "dialogueId": "Dialogue.Test.Branching",
        "dialogueVersion": 1,
        "entryNode": "start",
        "nodes": {
            "start": {
                "type": "line",
                "presentation": "bubble",
                "speaker": "Hero",
                "textId": "test.start",
                "text": "开始",
                "next": "choice",
            },
            "choice": {
                "type": "choice",
                "presentation": "dialogue",
                "options": [
                    {
                        "optionId": "left",
                        "textId": "test.left",
                        "text": "向左",
                        "outcomeId": "Outcome.Test.Left",
                        "next": "left.line",
                    },
                    {
                        "optionId": "right",
                        "textId": "test.right",
                        "text": "向右",
                        "outcomeId": "Outcome.Test.Right",
                        "next": "right.line",
                    },
                ],
            },
            "left.line": {
                "type": "line",
                "presentation": "dialogue",
                "speaker": "YueBai",
                "textId": "test.left.line",
                "text": "左路",
                "next": "left.end",
            },
            "left.end": {"type": "end", "outcomeId": "Outcome.Test.LeftDone"},
            "right.line": {
                "type": "line",
                "presentation": "dialogue",
                "speaker": "YueBai",
                "textId": "test.right.line",
                "text": "右路",
                "next": "right.end",
            },
            "right.end": {"type": "end", "outcomeId": "Outcome.Test.RightDone"},
        },
    }


class DialogueJsonValidationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.catalogs = make_catalogs()

    def assert_has_error(self, payload: dict, expected: str) -> None:
        errors = validate_dialogue(payload, self.catalogs)
        self.assertTrue(
            any(expected in error for error in errors),
            f"expected error containing {expected!r}, got {errors!r}",
        )

    def test_valid_branching_dialogue_is_canonical(self) -> None:
        payload = make_valid_dialogue()
        self.assertEqual([], validate_dialogue(payload, self.catalogs))
        canonical = canonicalize_dialogue(payload)
        self.assertEqual(sorted(payload["nodes"]), list(canonical["nodes"]))
        self.assertEqual(payload, canonical)

    def test_dangling_and_unreachable_nodes_are_rejected(self) -> None:
        dangling = make_valid_dialogue()
        dangling["nodes"]["start"]["next"] = "missing"
        self.assert_has_error(dangling, "missing target missing")

        unreachable = make_valid_dialogue()
        unreachable["nodes"]["orphan"] = {
            "type": "end",
            "outcomeId": "Outcome.Test.LeftDone",
        }
        self.assert_has_error(unreachable, "unreachable node: orphan")

    def test_unknown_condition_and_missing_speaker_are_rejected(self) -> None:
        unknown_condition = make_valid_dialogue()
        unknown_condition["nodes"]["choice"]["options"][0]["conditions"] = {
            "notRegistered": "value"
        }
        self.assert_has_error(unknown_condition, "unknown condition notRegistered")

        unknown_speaker = make_valid_dialogue()
        unknown_speaker["nodes"]["start"]["speaker"] = "MissingRole"
        self.assert_has_error(unknown_speaker, "unknown speaker or role MissingRole")

    def test_choice_count_and_option_ids_are_rejected(self) -> None:
        too_many = make_valid_dialogue()
        template = too_many["nodes"]["choice"]["options"][0]
        too_many["nodes"]["choice"]["options"] = []
        for index in range(5):
            option = copy.deepcopy(template)
            option["optionId"] = f"option.{index}"
            option["outcomeId"] = f"Outcome.Test.Option{index}"
            too_many["nodes"]["choice"]["options"].append(option)
        catalogs = CatalogSnapshot(
            speakers=self.catalogs.speakers,
            roles=self.catalogs.roles,
            outcomes=self.catalogs.outcomes
            | frozenset(f"Outcome.Test.Option{index}" for index in range(5)),
        )
        self.assertTrue(
            any("one to four options" in error for error in validate_dialogue(too_many, catalogs))
        )

        duplicate_option = make_valid_dialogue()
        duplicate_option["nodes"]["choice"]["options"][1]["optionId"] = "left"
        self.assert_has_error(duplicate_option, "duplicate option id left")

    def test_empty_duplicate_and_unregistered_outcomes_are_rejected(self) -> None:
        empty = make_valid_dialogue()
        empty["nodes"]["choice"]["options"][0]["outcomeId"] = ""
        self.assert_has_error(empty, "outcome id must not be empty")

        duplicate = make_valid_dialogue()
        duplicate["nodes"]["choice"]["options"][1]["outcomeId"] = "Outcome.Test.Left"
        self.assert_has_error(duplicate, "duplicate outcome id Outcome.Test.Left")

        unknown = make_valid_dialogue()
        unknown["nodes"]["right.end"]["outcomeId"] = "Outcome.NotRegistered"
        self.assert_has_error(unknown, "unknown outcome Outcome.NotRegistered")

    def test_exitless_cycle_is_rejected_but_choice_exit_is_allowed(self) -> None:
        cycle = make_valid_dialogue()
        cycle["nodes"]["start"]["next"] = "loop.a"
        cycle["nodes"]["loop.a"] = {
            "type": "line",
            "presentation": "dialogue",
            "speaker": "Hero",
            "textId": "loop.a",
            "text": "甲",
            "next": "loop.b",
        }
        cycle["nodes"]["loop.b"] = {
            "type": "line",
            "presentation": "dialogue",
            "speaker": "Hero",
            "textId": "loop.b",
            "text": "乙",
            "next": "loop.a",
        }
        self.assert_has_error(cycle, "exitless cycle")

        conditional_exit = make_valid_dialogue()
        conditional_exit["nodes"]["start"]["next"] = "loop.choice"
        conditional_exit["nodes"]["loop.choice"] = {
            "type": "choice",
            "presentation": "dialogue",
            "options": [
                {
                    "optionId": "repeat",
                    "textId": "loop.repeat",
                    "text": "继续",
                    "outcomeId": "Outcome.Test.Left",
                    "next": "loop.choice",
                },
                {
                    "optionId": "leave",
                    "textId": "loop.leave",
                    "text": "离开",
                    "outcomeId": "Outcome.Test.Right",
                    "next": "right.line",
                },
            ],
        }
        del conditional_exit["nodes"]["choice"]
        del conditional_exit["nodes"]["left.line"]
        del conditional_exit["nodes"]["left.end"]
        self.assertEqual([], validate_dialogue(conditional_exit, self.catalogs))

    def test_bubble_more_than_two_authored_lines_is_rejected(self) -> None:
        payload = make_valid_dialogue()
        payload["nodes"]["start"]["text"] = "第一行\n第二行\n第三行"
        self.assert_has_error(payload, "bubble text exceeds two lines")

    def test_validate_file_rejects_duplicate_json_keys(self) -> None:
        duplicate_json = """{
          "schemaVersion": 1,
          "dialogueId": "Dialogue.Test.Duplicate",
          "dialogueVersion": 1,
          "entryNode": "start",
          "nodes": {
            "start": {"type":"end","outcomeId":"Outcome.Test.LeftDone"},
            "start": {"type":"end","outcomeId":"Outcome.Test.RightDone"}
          }
        }"""
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "duplicate.dialogue.json"
            path.write_text(duplicate_json, encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate JSON key: start"):
                validate_file(path, self.catalogs)

    def test_validate_file_returns_canonical_payload(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "valid.dialogue.json"
            path.write_text(
                json.dumps(make_valid_dialogue(), ensure_ascii=False),
                encoding="utf-8",
            )
            canonical = validate_file(path, self.catalogs)
            self.assertEqual("Dialogue.Test.Branching", canonical["dialogueId"])


if __name__ == "__main__":
    unittest.main()
