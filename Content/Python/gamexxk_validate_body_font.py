from __future__ import annotations

"""Read-only validation of the GameXXK two-font rollout assets.

Run through the UE 5.8 MCP toolset after gamexxk_import_body_font.py. Fails unless
both project fonts exist, the body font carries a 'Default' typeface for
FSlateFontInfo, and its CJK fallback still points at the shared Source Han face.
"""

import json
import traceback
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
RESULT_PATH = (
    PROJECT_ROOT
    / "Saved"
    / "FontPreview"
    / "20260912-keinann-maru-pop"
    / "ue-validate-result.json"
)
TITLE_FACE_PATH = "/Game/GameXXK/UI/Fonts/Trial/FF_Trial_ZhHans_JiangHuGuFeng"
TITLE_FONT_PATH = f"{TITLE_FACE_PATH}_Font"
BODY_FACE_PATH = "/Game/GameXXK/UI/Fonts/Body/FF_Body_KeinannMaruPOP"
BODY_FONT_PATH = f"{BODY_FACE_PATH}_Font"
FALLBACK_FACE_PATH = "/Game/GameXXK/UI/Fonts/Trial/FF_Trial_Fallback_SourceHanSansCN_Regular"


def emit(payload: dict) -> None:
    """Always report through stdout; the MCP caller reads this marker."""
    try:
        RESULT_PATH.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    except Exception as exc:  # noqa: BLE001 - reporting must never mask the checks
        payload.setdefault("warnings", []).append(f"could not write {RESULT_PATH}: {exc}")
    print("GAMEXXK_BODY_FONT_VALIDATE=" + json.dumps(payload, ensure_ascii=True))


def composite_font(font_path: str) -> dict:
    font = unreal.EditorAssetLibrary.load_asset(font_path)
    if font is None or font.get_class().get_name() != "Font":
        raise RuntimeError(f"Missing or wrong-class composite font: {font_path}")
    properties = json.loads(unreal.ToolsetLibrary.get_object_properties(font, ["CompositeFont"]))
    return properties["CompositeFont"]


def run_checks() -> dict:
    checks: dict[str, bool] = {}
    for path, expected in (
        (TITLE_FACE_PATH, "FontFace"),
        (TITLE_FONT_PATH, "Font"),
        (BODY_FACE_PATH, "FontFace"),
        (BODY_FONT_PATH, "Font"),
        (FALLBACK_FACE_PATH, "FontFace"),
    ):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        actual = asset.get_class().get_name() if asset else None
        checks[f"{path.rsplit('/', 1)[-1]} is a {expected}"] = actual == expected
        if actual != expected:
            raise RuntimeError(f"{path} is {actual}, expected {expected}")

    body = composite_font(BODY_FONT_PATH)
    title = composite_font(TITLE_FONT_PATH)
    body_entries = body["defaultTypeface"]["fonts"]
    title_entries = title["defaultTypeface"]["fonts"]
    body_typefaces = [entry.get("name") for entry in body_entries]
    checks["body default typeface is 'Default'"] = "Default" in body_typefaces
    body_face_object = body_entries[0]["font"]["fontFaceAsset"]["refPath"]
    title_face_object = title_entries[0]["font"]["fontFaceAsset"]["refPath"]
    checks["body face is the Keinann Maru POP face"] = body_face_object.endswith(
        "FF_Body_KeinannMaruPOP"
    )
    checks["title face is still the JiangHu face"] = title_face_object.endswith(
        "FF_Trial_ZhHans_JiangHuGuFeng"
    )
    checks["body and title faces differ"] = body_face_object != title_face_object
    fallback = body["fallbackTypeface"]["typeface"]["fonts"][0]["font"]["fontFaceAsset"]["refPath"]
    checks["body CJK fallback is the shared Source Han face"] = fallback.endswith(
        "FF_Trial_Fallback_SourceHanSansCN_Regular"
    )
    return {
        "ok": False,
        "checks": checks,
        "body_typeface_names": body_typefaces,
        "body_face": body_face_object,
        "title_face": title_face_object,
        "fallback_face": fallback,
    }


def main() -> None:
    try:
        result = run_checks()
        result["ok"] = all(result["checks"].values())
        emit(result)
    except Exception as exc:  # noqa: BLE001 - the MCP caller reads the JSON result
        emit({"ok": False, "error": str(exc), "traceback": traceback.format_exc()})


if __name__ == "__main__":
    main()
