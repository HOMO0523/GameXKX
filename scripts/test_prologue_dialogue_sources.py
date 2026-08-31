from __future__ import annotations

import json
import unittest
from pathlib import Path

from scripts.validate_dialogue_json import CatalogSnapshot, validate_file


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = PROJECT_ROOT / "SourceAssets" / "Narrative" / "Dialogues"
CATALOG_PATH = PROJECT_ROOT / "SourceAssets" / "Narrative" / "runtime-catalog.json"


class PrologueDialogueSourceTests(unittest.TestCase):
    def _catalogs(self) -> CatalogSnapshot:
        payload = json.loads(CATALOG_PATH.read_text(encoding="utf-8"))
        return CatalogSnapshot(
            speakers=frozenset(payload["speakers"]),
            roles=frozenset(payload["roles"]),
            outcomes=frozenset(payload["outcomes"]),
        )

    def test_carriage_notice_is_the_approved_single_line(self) -> None:
        payload = validate_file(
            SOURCE_DIR / "Dialogue.Tutorial.CarriageNotice.dialogue.json",
            self._catalogs(),
        )
        self.assertEqual("Dialogue.Tutorial.CarriageNotice", payload["dialogueId"])
        self.assertEqual("notice", payload["entryNode"])
        self.assertEqual(
            "再往前就是天台山了……这是什么？",
            payload["nodes"]["notice"]["text"],
        )
        self.assertEqual("dialogue", payload["nodes"]["notice"]["presentation"])
        self.assertEqual("Character.Hero", payload["nodes"]["notice"]["speaker"])
        self.assertEqual(
            "Outcome.Tutorial.MapReady",
            payload["nodes"]["end"]["outcomeId"],
        )

    def test_yuebai_first_meeting_keeps_the_approved_line_order(self) -> None:
        payload = validate_file(
            SOURCE_DIR / "Dialogue.Tutorial.YueBaiFirstMeeting.dialogue.json",
            self._catalogs(),
        )
        self.assertEqual(
            "Dialogue.Tutorial.YueBaiFirstMeeting",
            payload["dialogueId"],
        )
        ordered_text = []
        node_id = payload["entryNode"]
        while payload["nodes"][node_id]["type"] == "line":
            node = payload["nodes"][node_id]
            self.assertEqual("dialogue", node["presentation"])
            ordered_text.append(node["text"])
            node_id = node["next"]
        self.assertEqual(
            [
                "你……可有吃的？本座已经好几日没吃东西了。",
                "你从地图里钻出来，第一件事就是讨饭？",
                "……有便给些，没有便罢。",
                "行了，拿去吧。就剩这点干粮，省着吃。",
                "多谢恩公。救命赠食之恩，本座定会报答。",
                "恩公欲往何处？",
                "天台山。听说前面有活干，运气好还能混口饭吃。",
                "天台山……此图上正有一条路通往那里。",
                "你看得懂这玩意？",
                "自然。",
                "那正好，我不识字。你替我认路，就算报恩了。",
                "……也罢。本座随你同行，替你指路。",
                "那说好了，我在前面走，你在后面指。",
                "……恩公倒是安排得明白。",
            ],
            ordered_text,
        )
        self.assertEqual("end", node_id)
        self.assertEqual(
            "Outcome.Tutorial.YueBaiFollowing",
            payload["nodes"]["end"]["outcomeId"],
        )


if __name__ == "__main__":
    unittest.main()
