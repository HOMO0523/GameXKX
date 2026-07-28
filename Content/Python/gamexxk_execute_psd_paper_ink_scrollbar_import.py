"""Explicit UE commandlet boundary for paper/ink scrollbar texture import."""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


PIPELINE_PATH = Path(__file__).with_name("gamexxk_import_psd_paper_ink_scrollbar.py")


def main() -> dict[str, object]:
    spec = importlib.util.spec_from_file_location("gamexxk_import_psd_paper_ink_scrollbar", PIPELINE_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable to load paper/ink scrollbar importer: {PIPELINE_PATH}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module.main(["--execute-import"])


if __name__ == "__main__":
    main()
