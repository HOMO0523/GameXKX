from __future__ import annotations

import json
from pathlib import Path

from gamexxk_import_dialogue_json import import_dialogues
from gamexxk_import_narrative_sequence_json import import_sequences


PROJECT_ROOT = Path(__file__).resolve().parents[2]
CATALOG_PATH = PROJECT_ROOT / "SourceAssets" / "Narrative" / "runtime-catalog.json"
DIALOGUE_ROOT = PROJECT_ROOT / "SourceAssets" / "Narrative" / "Dialogues"
SEQUENCE_ROOT = PROJECT_ROOT / "SourceAssets" / "Narrative" / "Sequences"


def main() -> None:
    dialogue_paths = sorted(DIALOGUE_ROOT.glob("Dialogue.Npc.*.Default.dialogue.json"))
    sequence_paths = sorted(SEQUENCE_ROOT.glob("Sequence.Npc.*.Default.sequence.json"))
    result = {
        "dialogues": import_dialogues(dialogue_paths, CATALOG_PATH),
        "sequences": import_sequences(sequence_paths, CATALOG_PATH),
    }
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
