from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
REPORT_PATH = PROJECT_ROOT / "Saved" / "HarnessReports" / "gamexxk-testmap-compat-validation.json"


def main() -> None:
    library_class = unreal.load_class(None, "/Script/TestMap.MyBlueprintFunctionLibrary")
    has_python_wrapper = hasattr(unreal, "MyBlueprintFunctionLibrary")
    result_value = None
    call_error = ""
    if has_python_wrapper:
        try:
            result_value = int(unreal.MyBlueprintFunctionLibrary.fun_cl_get_random_array_to_index([0, 0, 0]))
        except Exception as exc:
            call_error = str(exc)

    result = {
        "ok": library_class is not None and has_python_wrapper and result_value == 0,
        "library_class": library_class.get_path_name() if library_class else "",
        "has_python_wrapper": has_python_wrapper,
        "all_zero_result": result_value,
        "call_error": call_error,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))
    if not result["ok"]:
        raise RuntimeError("TestMap compatibility BlueprintFunctionLibrary is not available")


if __name__ == "__main__":
    main()
