import copy
import json
import tempfile
import unittest
from pathlib import Path

from scripts.validate_narrative_sequence_json import (
    NarrativeCatalogSnapshot,
    canonicalize_sequence,
    validate_character_catalog,
    validate_file,
    validate_sequence,
)


PROJECT_ROOT = Path(__file__).resolve().parents[1]


def make_catalogs() -> NarrativeCatalogSnapshot:
    return NarrativeCatalogSnapshot(
        character_ids=frozenset({"Character.Hero", "Npc.YueBai"}),
        action_ids_by_character={
            "Character.Hero": frozenset({"Town.Idle"}),
            "Npc.YueBai": frozenset({"Narrative.Appear", "Narrative.Idle"}),
        },
        dialogue_ids=frozenset({"Dialogue.Test.Choice"}),
        slot_ids_by_stage={"Stage.Test": frozenset({"Slot.YueBaiSpawn", "Slot.Safe"})},
        command_types=frozenset({"moveToSlot", "playAction", "grantItem"}),
        wait_types=frozenset({"seconds", "dialogue"}),
        outcome_ids=frozenset({"Outcome.Accept", "Outcome.Reject"}),
    )


def make_valid_sequence() -> dict:
    return {
        "schemaVersion": 1,
        "sequenceId": "Sequence.Test.AcceptOrReject",
        "sequenceVersion": 1,
        "stageContractId": "Stage.Test",
        "entryStep": "dialogue",
        "roles": {"Hero": "Character.Hero", "YueBai": "Npc.YueBai"},
        "steps": {
            "dialogue": {
                "type": "dialogue",
                "dialogueId": "Dialogue.Test.Choice",
                "next": "branch",
            },
            "branch": {
                "type": "branchOnOutcome",
                "outcomes": {
                    "Outcome.Accept": "move",
                    "Outcome.Reject": "end",
                },
            },
            "move": {
                "type": "command",
                "commandId": "move_yuebai",
                "commandType": "moveToSlot",
                "arguments": {"role": "YueBai", "slotId": "Slot.YueBaiSpawn"},
                "optional": False,
                "next": "action",
            },
            "action": {
                "type": "command",
                "commandId": "show_yuebai",
                "commandType": "playAction",
                "arguments": {"role": "YueBai", "actionId": "Narrative.Appear"},
                "optional": False,
                "next": "end",
            },
            "end": {"type": "end"},
        },
    }


class NarrativeSequenceJsonValidationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.catalogs = make_catalogs()

    def assert_has_error(self, payload: dict, expected: str) -> None:
        errors = validate_sequence(payload, self.catalogs)
        self.assertTrue(
            any(expected in error for error in errors),
            f"expected {expected!r}, got {errors!r}",
        )

    def test_valid_sequence_is_canonical(self) -> None:
        payload = make_valid_sequence()
        self.assertEqual([], validate_sequence(payload, self.catalogs))
        canonical = canonicalize_sequence(payload)
        self.assertEqual(sorted(payload["steps"]), list(canonical["steps"]))
        self.assertEqual(payload, canonical)

    def test_dangling_unreachable_and_duplicate_commands_are_rejected(self) -> None:
        dangling = make_valid_sequence()
        dangling["steps"]["move"]["next"] = "missing"
        self.assert_has_error(dangling, "missing target missing")

        unreachable = make_valid_sequence()
        unreachable["steps"]["orphan"] = {"type": "end"}
        self.assert_has_error(unreachable, "unreachable step: orphan")

        duplicate_command = make_valid_sequence()
        duplicate_command["steps"]["action"]["commandId"] = "move_yuebai"
        self.assert_has_error(duplicate_command, "duplicate command id move_yuebai")

    def test_unknown_resources_are_rejected(self) -> None:
        unknown_character = make_valid_sequence()
        unknown_character["roles"]["YueBai"] = "Character.Missing"
        self.assert_has_error(unknown_character, "unknown character Character.Missing")

        unknown_dialogue = make_valid_sequence()
        unknown_dialogue["steps"]["dialogue"]["dialogueId"] = "Dialogue.Missing"
        self.assert_has_error(unknown_dialogue, "unknown dialogue Dialogue.Missing")

        unknown_slot = make_valid_sequence()
        unknown_slot["steps"]["move"]["arguments"]["slotId"] = "Slot.Missing"
        self.assert_has_error(unknown_slot, "unknown slot Slot.Missing")

        unknown_action = make_valid_sequence()
        unknown_action["steps"]["action"]["arguments"]["actionId"] = "Narrative.Missing"
        self.assert_has_error(unknown_action, "does not support action Narrative.Missing")

        unknown_command = make_valid_sequence()
        unknown_command["steps"]["move"]["commandType"] = "arbitraryFunction"
        self.assert_has_error(unknown_command, "unknown command type arbitraryFunction")

        unknown_outcome = make_valid_sequence()
        unknown_outcome["steps"]["branch"]["outcomes"]["Outcome.Missing"] = "end"
        self.assert_has_error(unknown_outcome, "unknown outcome Outcome.Missing")

    def test_map_and_numeric_coordinate_fields_are_rejected_recursively(self) -> None:
        map_bound = make_valid_sequence()
        map_bound["mapPath"] = "/Game/Maps/Forbidden"
        self.assert_has_error(map_bound, "forbidden scene field mapPath")

        coordinate_bound = make_valid_sequence()
        coordinate_bound["steps"]["move"]["arguments"]["x"] = "100"
        self.assert_has_error(coordinate_bound, "forbidden scene field x")

        transform_bound = make_valid_sequence()
        transform_bound["steps"]["move"]["arguments"]["transform"] = "anything"
        self.assert_has_error(transform_bound, "forbidden scene field transform")

    def test_exitless_immediate_cycle_is_rejected(self) -> None:
        cycle = make_valid_sequence()
        cycle["entryStep"] = "cycle.a"
        cycle["steps"]["cycle.a"] = {
            "type": "branchOnOutcome",
            "outcomes": {"Outcome.Accept": "cycle.b"},
        }
        cycle["steps"]["cycle.b"] = {
            "type": "branchOnOutcome",
            "outcomes": {"Outcome.Accept": "cycle.a"},
        }
        self.assert_has_error(cycle, "exitless immediate cycle")

    def test_validate_file_rejects_duplicate_step_keys(self) -> None:
        duplicate = """{
          "schemaVersion":1,
          "sequenceId":"Sequence.Test.Duplicate",
          "sequenceVersion":1,
          "stageContractId":"Stage.Test",
          "entryStep":"end",
          "roles":{},
          "steps":{"end":{"type":"end"},"end":{"type":"end"}}
        }"""
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "duplicate.sequence.json"
            path.write_text(duplicate, encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate JSON key: end"):
                validate_file(path, self.catalogs)

    def test_character_catalog_has_required_identities_without_placement(self) -> None:
        path = PROJECT_ROOT / "SourceAssets" / "Narrative" / "characters.json"
        self.assertTrue(path.is_file(), f"missing character source: {path}")
        payload = json.loads(path.read_text(encoding="utf-8"))
        self.assertEqual([], validate_character_catalog(payload))
        ids = {entry["characterId"] for entry in payload["characters"]}
        self.assertTrue(
            {
                "Character.Hero",
                "Character.Horse",
                "Character.Carriage",
                "Npc.TusiChief",
                "Npc.SongJinBao",
                "Npc.YueBai",
                "Npc.ZhouGuangZu",
                "Npc.JinGui",
                "Npc.QiongMeiEr",
            }.issubset(ids)
        )

    def test_importers_have_validate_before_mutate_contract(self) -> None:
        for relative_path, report_name in (
            ("Content/Python/gamexxk_import_character_catalog.py", "character-catalog-import-report.json"),
            ("Content/Python/gamexxk_import_narrative_sequence_json.py", "narrative-sequence-import-report.json"),
        ):
            path = PROJECT_ROOT / relative_path
            self.assertTrue(path.is_file(), f"missing importer: {path}")
            source = path.read_text(encoding="utf-8")
            compile(source, str(path), "exec")
            self.assertIn("save_loaded_asset", source)
            self.assertIn(report_name, source)
            self.assertLess(source.index("validate_"), source.index("create_asset"))

    def test_desktop_story_sequences_are_semantic_and_catalog_complete(self) -> None:
        catalog_payload = json.loads(
            (PROJECT_ROOT / "SourceAssets" / "Narrative" / "runtime-catalog.json").read_text(
                encoding="utf-8"
            )
        )
        catalogs = NarrativeCatalogSnapshot(
            character_ids=frozenset(catalog_payload["characterIds"]),
            action_ids_by_character={
                character_id: frozenset(action_ids)
                for character_id, action_ids in catalog_payload[
                    "actionIdsByCharacter"
                ].items()
            },
            dialogue_ids=frozenset(catalog_payload["dialogueIds"]),
            slot_ids_by_stage={
                stage_id: frozenset(slot_ids)
                for stage_id, slot_ids in catalog_payload["slotIdsByStage"].items()
            },
            command_types=frozenset(catalog_payload["commandTypes"]),
            wait_types=frozenset(catalog_payload["waitTypes"]),
            outcome_ids=frozenset(catalog_payload["outcomeIds"]),
        )
        expected = {
            "Sequence.Main.XuXiake.CarriageArrival.sequence.json": (
                "Sequence.Main.XuXiake.CarriageArrival",
                "Stage.Tutorial.River",
            ),
            "Sequence.Main.XuXiake.CombatBriefing.sequence.json": (
                "Sequence.Main.XuXiake.CombatBriefing",
                "Stage.Desktop.Briefing",
            ),
            "Sequence.Main.XuXiake.FirstJourneyBriefing.sequence.json": (
                "Sequence.Main.XuXiake.FirstJourneyBriefing",
                "Stage.Desktop.Briefing",
            ),
        }
        desktop_commands = {
            "stageShowRole",
            "stageHideRole",
            "stageMoveRole",
            "stageSetFacing",
            "stageShowProp",
            "stageHideProp",
            "stagePlayAction",
            "stagePlayVfx",
            "stageFlash",
            "showToast",
            "dialogue",
        }
        source_dir = PROJECT_ROOT / "SourceAssets" / "Narrative" / "Sequences"
        for filename, (sequence_id, stage_id) in expected.items():
            with self.subTest(filename=filename):
                payload = validate_file(source_dir / filename, catalogs)
                self.assertEqual(sequence_id, payload["sequenceId"])
                self.assertEqual(stage_id, payload["stageContractId"])
                self.assertTrue({"Hero", "YueBai"}.issubset(payload["roles"]))
                command_types = {
                    step["commandType"]
                    for step in payload["steps"].values()
                    if step["type"] == "command"
                }
                self.assertTrue(command_types.issubset(desktop_commands))
                serialized = json.dumps(payload, ensure_ascii=False).lower()
                for forbidden in (
                    "/maps/",
                    "sceneprofile",
                    "worldlocation",
                    "transform",
                    "spawnactor",
                    "npcunlock",
                    "recruit",
                ):
                    self.assertNotIn(forbidden, serialized)

        carriage = validate_file(
            source_dir / "Sequence.Main.XuXiake.CarriageArrival.sequence.json",
            catalogs,
        )
        command_types = {
            step["commandType"]
            for step in carriage["steps"].values()
            if step["type"] == "command"
        }
        self.assertIn("stageShowRole", command_types)
        self.assertIn("stageShowProp", command_types)
        self.assertIn("stagePlayAction", command_types)
        self.assertIn(
            "Dialogue.Tutorial.001",
            {
                step["dialogueId"]
                for step in carriage["steps"].values()
                if step["type"] == "dialogue"
            },
        )


if __name__ == "__main__":
    unittest.main()
