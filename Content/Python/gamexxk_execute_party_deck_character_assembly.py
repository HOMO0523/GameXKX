"""UE commandlet entry point for PartyDeck PaperSprite and PaperFlipbook assembly.

Invoke this file through UnrealEditor-Cmd with the PythonScript commandlet and
use forward slashes in the script path on Windows.
"""

from __future__ import annotations

import importlib.util
from pathlib import Path


ASSEMBLER_PATH = Path(__file__).with_name("gamexxk_assemble_party_deck_characters.py")


def _load_assembler():
    spec = importlib.util.spec_from_file_location("gamexxk_assemble_party_deck_characters", ASSEMBLER_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable to load PartyDeck character assembler: {ASSEMBLER_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> dict[str, object]:
    return _load_assembler().main(["--execute"])


if __name__ == "__main__":
    main()
