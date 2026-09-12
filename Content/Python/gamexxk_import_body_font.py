from __future__ import annotations

"""Import the GameXXK body (non-title) font and wire its CJK fallback.

Run through the UE 5.8 MCP toolset (see scripts/ue_mcp_client.py). The source TTF
lives in Saved/FontPreview/20260912-cheese-foam-oolong-song and is imported
unmodified; the SHA-256 check below fails the run if the file drifts.

Result: /Game/GameXXK/UI/Fonts/Body/FF_Body_CheeseFoamOolongSongLite_Bold_Font
"""

import hashlib
import json
import traceback
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = PROJECT_ROOT / "Saved" / "FontPreview" / "20260912-keinann-maru-pop" / "fonts"
RESULT_PATH = (
    PROJECT_ROOT
    / "Saved"
    / "FontPreview"
    / "20260912-keinann-maru-pop"
    / "ue-import-result.json"
)
DESTINATION = "/Game/GameXXK/UI/Fonts/Body"
FACE_NAME = "FF_Body_KeinannMaruPOP"
FACE_PATH = f"{DESTINATION}/{FACE_NAME}"
FONT_PATH = f"{FACE_PATH}_Font"
FALLBACK_FACE_PATH = "/Game/GameXXK/UI/Fonts/Trial/FF_Trial_Fallback_SourceHanSansCN_Regular"
SOURCE_FILE = SOURCE_DIR / "KeinannMaruPOP.ttf"
SOURCE_SHA256 = "30c3536a7459a963a0260ada5a84300a905dfb93db2822d1f243950505e59427"
EXPECTED_TYPEFACE = "Default"
# 2026-09-12: the Cheese Foam Oolong Song body face was rejected as too thin and
# replaced by Keinann Maru POP. Both of its assets are retired here.
SUPERSEDED_BODY_ASSETS = (
    f"{DESTINATION}/FF_Body_CheeseFoamOolongSongLite_Bold_Font",
    f"{DESTINATION}/FF_Body_CheeseFoamOolongSongLite_Bold",
)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _write_result(payload: dict) -> None:
    RESULT_PATH.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    print("GAMEXXK_BODY_FONT_RESULT=" + json.dumps(payload, ensure_ascii=True))


def _class_name(asset) -> str:
    return asset.get_class().get_name() if asset else ""


def _require_asset(path: str, expected_class: str):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if asset is None:
        raise RuntimeError(f"Missing imported asset: {path}")
    actual_class = _class_name(asset)
    if actual_class != expected_class:
        raise RuntimeError(f"Asset class mismatch at {path}: expected {expected_class}, got {actual_class}")
    return asset


def _font_batch_mode(create_font: bool):
    enum_type = getattr(unreal, "BatchCreateFontAsset", None)
    if enum_type is None:
        raise RuntimeError("Unreal Python does not expose BatchCreateFontAsset")
    member_name = "CREATE_IF_NO_FONT_EXISTS" if create_font else "NO"
    mode = getattr(enum_type, member_name, None)
    if mode is None:
        raise RuntimeError(f"BatchCreateFontAsset.{member_name} is unavailable")
    return mode


def _import_body_font() -> dict:
    if not SOURCE_FILE.is_file():
        raise RuntimeError(f"Missing source font: {SOURCE_FILE}")
    actual_hash = _sha256(SOURCE_FILE)
    if actual_hash != SOURCE_SHA256:
        raise RuntimeError(f"Source hash mismatch for {SOURCE_FILE.name}: {actual_hash}")

    for path, expected in ((FACE_PATH, "FontFace"), (FONT_PATH, "Font")):
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            _require_asset(path, expected)

    factory = unreal.FontFileImportFactory()
    factory.set_editor_property("batch_create_font_asset", _font_batch_mode(True))
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(SOURCE_FILE))
    task.set_editor_property("destination_path", DESTINATION)
    task.set_editor_property("destination_name", FACE_NAME)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    task.set_editor_property("factory", factory)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    face = _require_asset(FACE_PATH, "FontFace")
    font = _require_asset(FONT_PATH, "Font")
    source_filename = str(face.get_editor_property("source_filename"))
    if Path(source_filename).resolve() != SOURCE_FILE.resolve():
        raise RuntimeError(f"Font Face source mismatch at {FACE_PATH}: {source_filename}")

    return {
        "source": str(SOURCE_FILE),
        "source_sha256": actual_hash,
        "face_path": FACE_PATH,
        "face_class": _class_name(face),
        "font_path": FONT_PATH,
        "font_class": _class_name(font),
        "source_filename": source_filename,
        "imported_object_paths": list(task.get_editor_property("imported_object_paths")),
    }


def _configure_fallback() -> dict:
    font = _require_asset(FONT_PATH, "Font")
    fallback_face = _require_asset(FALLBACK_FACE_PATH, "FontFace")
    properties = json.loads(unreal.ToolsetLibrary.get_object_properties(font, ["CompositeFont"]))
    composite = properties["CompositeFont"]
    # A composite font spells the default typeface as {fonts:[...]} while the
    # fallback typeface nests it as {typeface:{fonts:[...]}}.
    typefaces = composite.get("defaultTypeface", {}).get("fonts", [])
    typeface_names = [entry.get("name") for entry in typefaces]
    fallback_object_path = fallback_face.get_path_name()
    composite["fallbackTypeface"] = {
        "typeface": {
            "fonts": [
                {
                    "name": "Fallback",
                    "font": {
                        "fontFilename": "",
                        "hinting": "Default",
                        "loadingPolicy": "LazyLoad",
                        "subFaceIndex": 0,
                        "fontFaceAsset": {"refPath": fallback_object_path},
                    },
                }
            ]
        },
        "scalingFactor": 1.0,
    }
    changed = bool(
        unreal.ToolsetLibrary.set_object_properties(
            font,
            json.dumps({"compositeFont": composite}),
        )
    )
    if not changed:
        raise RuntimeError(f"ToolsetLibrary did not set the Composite Font fallback at {FONT_PATH}")
    after = json.loads(unreal.ToolsetLibrary.get_object_properties(font, ["CompositeFont"]))
    actual = after["CompositeFont"]["fallbackTypeface"]["typeface"]["fonts"][0]["font"]["fontFaceAsset"]
    expected = {"refPath": fallback_object_path}
    if actual != expected:
        raise RuntimeError(f"Fallback mismatch at {FONT_PATH}: expected {expected}, got {actual}")
    if EXPECTED_TYPEFACE not in typeface_names:
        raise RuntimeError(
            f"{FONT_PATH} has no '{EXPECTED_TYPEFACE}' typeface for FSlateFontInfo; found {typeface_names}"
        )
    saved = bool(unreal.EditorAssetLibrary.save_asset(FONT_PATH, only_if_is_dirty=False))
    if not saved:
        raise RuntimeError(f"Could not save Runtime Font: {FONT_PATH}")
    return {
        "font_path": FONT_PATH,
        "fallback_face_path": fallback_object_path,
        "default_typeface_names": typeface_names,
        "saved": saved,
    }


def _delete_superseded_assets() -> dict:
    """Retire the replaced body face. Cleanup must never fail the import itself."""
    editor = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
    deleted: list[str] = []
    still_present: list[str] = []
    for asset_path in SUPERSEDED_BODY_ASSETS:
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if asset is None:
            continue
        editor.close_all_editors_for_asset(asset)
        unreal.EditorAssetLibrary.delete_asset(asset_path)
        deleted.append(asset_path)
    unreal.SystemLibrary.collect_garbage()
    for asset_path in SUPERSEDED_BODY_ASSETS:
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            still_present.append(asset_path)
    return {"requested": list(SUPERSEDED_BODY_ASSETS), "deleted": deleted, "still_present": still_present}


def main() -> None:
    result: dict = {
        "ok": False,
        "engine_version": unreal.SystemLibrary.get_engine_version(),
        "destination": DESTINATION,
        "assets": [],
    }
    try:
        if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
            unreal.EditorAssetLibrary.make_directory(DESTINATION)
        result["assets"].append(_import_body_font())
        result["fallback"] = _configure_fallback()
        result["deleted_superseded_assets"] = _delete_superseded_assets()
        face = _require_asset(FACE_PATH, "FontFace")
        font = _require_asset(FONT_PATH, "Font")
        result["face_saved"] = bool(unreal.EditorAssetLibrary.save_asset(FACE_PATH, only_if_is_dirty=False))
        editor = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
        for asset in (face, font):
            editor.close_all_editors_for_asset(asset)
        result["opened_font_editors"] = bool(editor.open_editor_for_assets([font]))
        result["ok"] = True
        _write_result(result)
    except Exception as exc:  # noqa: BLE001 - the MCP caller reads the JSON result
        result["error"] = str(exc)
        result["traceback"] = traceback.format_exc()
        _write_result(result)
        unreal.log_error("GameXXK body font import failed: " + str(exc))
        raise


if __name__ == "__main__":
    main()
