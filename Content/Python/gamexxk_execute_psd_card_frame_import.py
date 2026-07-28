"""UE commandlet entry point for the one approved PSD card-frame import.

Run only through the UE Python commandlet after the read-only source contract is green:

    UnrealEditor-Cmd.exe GameXXK.uproject -run=pythonscript \
        -script=.../gamexxk_execute_psd_card_frame_import.py

The implementation stays in ``gamexxk_import_psd_card_frame`` so that the same source/hash
validation is used for both preflight and the asset write.
"""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path


PIPELINE_PATH = Path(__file__).with_name("gamexxk_import_psd_card_frame.py")


def load_pipeline_module():
    spec = importlib.util.spec_from_file_location("gamexxk_import_psd_card_frame", PIPELINE_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable to load PSD card-frame pipeline: {PIPELINE_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> dict[str, object]:
    pipeline = load_pipeline_module()
    result = pipeline.import_verified_card_frame()
    print(json.dumps({"ok": True, **result}, ensure_ascii=False))
    return result


if __name__ == "__main__":
    main()
