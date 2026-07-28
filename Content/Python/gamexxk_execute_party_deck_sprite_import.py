"""UE commandlet entry point for explicitly importing reviewed PartyDeck textures.

Run this only after the offline manifest/preflight tests are green:

    UnrealEditor-Cmd.exe GameXXK.uproject -run=pythonscript \
        -script=.../gamexxk_execute_party_deck_sprite_import.py

The wrapped pipeline keeps its default mode read-only; this dedicated file is the
explicit execute boundary for the commandlet invocation.

Pass the ``-script`` value with forward slashes on Windows (for example
``D:/UE5 demo/GameXXK/...``).  PythonScriptCommandlet treats backslash escapes
in this argument and can otherwise misread a path beginning with ``D:\\UE5``.
"""

from __future__ import annotations

import importlib.util
from pathlib import Path


PIPELINE_PATH = Path(__file__).with_name("gamexxk_import_party_deck_sprite_atlases.py")


def load_pipeline_module():
    spec = importlib.util.spec_from_file_location("gamexxk_import_party_deck_sprite_atlases", PIPELINE_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable to load PartyDeck texture import pipeline: {PIPELINE_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> dict[str, object]:
    pipeline = load_pipeline_module()
    return pipeline.main(["--execute-import"])


if __name__ == "__main__":
    main()
