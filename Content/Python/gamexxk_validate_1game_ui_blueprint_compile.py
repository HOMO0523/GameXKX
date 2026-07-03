from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
REPORT_PATH = PROJECT_ROOT / "Saved" / "HarnessReports" / "gamexxk-1game-ui-blueprint-compile-validation.json"

ASSETS = [
    "/Game/1Game/UI/UI_地图选择",
    "/Game/1Game/UI/UI_地图选择-关卡",
    "/Game/1Game/UI/UI_地图选择-关卡-线",
    "/Game/1Game/UI/UI_地图选择-Boss",
]


def _status(asset) -> str:
    for name in ("status", "Status"):
        try:
            return str(asset.get_editor_property(name))
        except Exception:
            pass
    return ""


def _generated_class_path(asset_path: str) -> str:
    asset_name = asset_path.rsplit("/", 1)[-1]
    return f"{asset_path}.{asset_name}_C"


def _compile_asset(asset_path: str) -> dict:
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    result = {
        "asset": asset_path,
        "exists": bool(asset),
        "status_before": _status(asset) if asset else "",
        "compiled": False,
        "status_after": "",
        "generated_class_loaded": False,
    }
    if not asset:
        return result

    if hasattr(unreal, "BlueprintEditorLibrary"):
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)
        result["compiled"] = True
    elif hasattr(unreal, "KismetEditorUtilities"):
        unreal.KismetEditorUtilities.compile_blueprint(asset)
        result["compiled"] = True
    else:
        raise RuntimeError("No Blueprint compile API is available")

    result["status_after"] = _status(asset)
    result["generated_class_loaded"] = unreal.load_class(None, _generated_class_path(asset_path)) is not None
    return result


def main() -> None:
    checks = [_compile_asset(asset_path) for asset_path in ASSETS]
    result = {
        "ok": all(
            check.get("exists")
            and check.get("compiled")
            and check.get("generated_class_loaded")
            and "ERROR" not in str(check.get("status_after", "")).upper()
            for check in checks
        ),
        "checks": checks,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False))
    if not result["ok"]:
        raise RuntimeError("1Game UI Blueprint compile validation failed")


if __name__ == "__main__":
    main()
