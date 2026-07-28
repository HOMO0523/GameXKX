"""Explicit UE commandlet boundary for the missing-only Town hero-detail PSD import."""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


PIPELINE_PATH = Path(__file__).with_name("gamexxk_import_town_ui_assets.py")


def main() -> None:
    spec = importlib.util.spec_from_file_location("gamexxk_import_town_ui_assets", PIPELINE_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable to load Town UI importer: {PIPELINE_PATH}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    module.main(["--assets", "T_TownCharacter_HeroDetail036"])


if __name__ == "__main__":
    main()
