from __future__ import annotations

import json
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = PROJECT_ROOT / "SourceAssets" / "Narrative"


class RetiredLegacyTownNarrativeTests(unittest.TestCase):
    def test_tusi_and_song_have_no_default_town_interaction(self) -> None:
        payload = json.loads((SOURCE_ROOT / "characters.json").read_text(encoding="utf-8"))
        by_id = {entry["characterId"]: entry for entry in payload["characters"]}
        for character_id in ("Npc.TusiChief", "Npc.SongJinBao"):
            self.assertNotIn("defaultInteractionSequenceId", by_id[character_id])

    def test_retired_dialogue_and_sequence_sources_are_absent(self) -> None:
        retired = (
            "Dialogues/Dialogue.Npc.TusiChief.Default.dialogue.json",
            "Dialogues/Dialogue.Npc.SongJinBao.Default.dialogue.json",
            "Sequences/Sequence.Npc.TusiChief.Default.sequence.json",
            "Sequences/Sequence.Npc.SongJinBao.Default.sequence.json",
        )
        self.assertEqual(
            [relative for relative in retired if (SOURCE_ROOT / relative).exists()],
            [],
        )

    def test_runtime_catalog_and_controller_expose_no_retired_commands(self) -> None:
        catalog = json.loads((SOURCE_ROOT / "runtime-catalog.json").read_text(encoding="utf-8"))
        serialized = json.dumps(catalog, ensure_ascii=False)
        controller_source = (
            PROJECT_ROOT
            / "Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp"
        ).read_text(encoding="utf-8")
        npc_sources = "\n".join(
            (PROJECT_ROOT / relative).read_text(encoding="utf-8")
            for relative in (
                "Source/GameXXK/Private/Town/GameXXKTownNpcActor.cpp",
                "Source/GameXXK/Private/Town/GameXXKTownNpcCharacter.cpp",
            )
        )
        for token in (
            "Dialogue.Npc.TusiChief.Default",
            "Dialogue.Npc.SongJinBao.Default",
            "Sequence.Npc.TusiChief.Default",
            "Sequence.Npc.SongJinBao.Default",
            "Outcome.OptionTask",
            "Outcome.OptionShop",
            "Outcome.Task",
            "Outcome.Shop",
            "openTaskOffer",
            "openShop",
        ):
            self.assertNotIn(token, serialized)
            self.assertNotIn(token, controller_source)
        self.assertNotIn("Subsystem->AcceptQuest()", npc_sources)
        self.assertNotIn("PlayerController->OpenMetaShopWindow()", npc_sources)


if __name__ == "__main__":
    unittest.main()
