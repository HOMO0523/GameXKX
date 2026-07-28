"""Explicit UE commandlet boundary for PartyDeck card portrait import.

Run the Pillow-backed source preparation in the workspace before invoking this
UE-only script; UE's embedded Python imports the already prepared PNGs and does
not mutate the approved source or alpha artifacts:

    python Content/Python/gamexxk_import_party_deck_card_portraits.py --prepare
    UnrealEditor-Cmd.exe GameXXK.uproject -run=pythonscript \
        -script=.../gamexxk_execute_party_deck_card_portrait_import.py
"""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


PIPELINE_PATH = Path(__file__).with_name("gamexxk_import_party_deck_card_portraits.py")


def main() -> dict[str, object]:
    spec = importlib.util.spec_from_file_location("gamexxk_import_party_deck_card_portraits", PIPELINE_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable to load PartyDeck card portrait importer: {PIPELINE_PATH}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module.main(["--execute-import"])


if __name__ == "__main__":
    main()
