from __future__ import annotations

import hashlib
import json
import traceback
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = PROJECT_ROOT / "Saved" / "FontPreview" / "20260904" / "fonts"
RESULT_PATH = PROJECT_ROOT / "Saved" / "FontPreview" / "20260904" / "ue-import-result.json"
DESTINATION = "/Game/GameXXK/UI/Fonts/Trial"
ZH_FONT_PATH = f"{DESTINATION}/FF_Trial_ZhHans_JiangHuGuFeng_Font"
FALLBACK_FACE_PATH = f"{DESTINATION}/FF_Trial_Fallback_SourceHanSansCN_Regular"
REJECTED_RANCHO_ASSETS = (
    f"{DESTINATION}/FF_Trial_En_Rancho_Font",
    f"{DESTINATION}/FF_Trial_En_Rancho",
)
FONT_RECORDS = (
    {
        "culture": "zh-Hans",
        "source": SOURCE_DIR / "ZiKuJiangHuGuFeng.ttf",
        "sha256": "8d1cead1150f42e5ac4d5a454a7897ea34e7ab801e6ea12035acb73e54cf06d4",
        "asset_name": "FF_Trial_ZhHans_JiangHuGuFeng",
        "create_font": True,
    },
    {
        "culture": "fallback",
        "source": SOURCE_DIR / "SourceHanSansCN-Regular.otf",
        "sha256": "e2bc8a2e7f37474b774fff8db758681ece40bb6947a90d571bce9dd60671a8e4",
        "asset_name": "FF_Trial_Fallback_SourceHanSansCN_Regular",
        "create_font": False,
    },
)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _write_result(payload: dict) -> None:
    RESULT_PATH.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    print("GAMEXXK_FONT_TRIAL_RESULT=" + json.dumps(payload, ensure_ascii=True))


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


def _validate_preexisting_pair(face_path: str, font_path: str) -> None:
    face_exists = unreal.EditorAssetLibrary.does_asset_exist(face_path)
    font_exists = unreal.EditorAssetLibrary.does_asset_exist(font_path)
    if face_exists != font_exists:
        raise RuntimeError(f"Partial pre-existing trial pair: face={face_exists}, font={font_exists}")
    if face_exists:
        _require_asset(face_path, "FontFace")
        _require_asset(font_path, "Font")


def _import_record(record: dict) -> tuple[dict, object]:
    source = Path(record["source"])
    if not source.is_file():
        raise RuntimeError(f"Missing source font: {source}")
    actual_hash = _sha256(source)
    if actual_hash != record["sha256"]:
        raise RuntimeError(f"Source hash mismatch for {source.name}: {actual_hash}")

    face_path = f"{DESTINATION}/{record['asset_name']}"
    create_font = bool(record["create_font"])
    font_path = f"{face_path}_Font" if create_font else ""
    if create_font:
        _validate_preexisting_pair(face_path, font_path)
    elif unreal.EditorAssetLibrary.does_asset_exist(face_path):
        _require_asset(face_path, "FontFace")

    factory = unreal.FontFileImportFactory()
    factory.set_editor_property("batch_create_font_asset", _font_batch_mode(create_font))
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", DESTINATION)
    task.set_editor_property("destination_name", record["asset_name"])
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", False)
    task.set_editor_property("factory", factory)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    face = _require_asset(face_path, "FontFace")
    font = _require_asset(font_path, "Font") if create_font else None
    source_filename = str(face.get_editor_property("source_filename"))
    if Path(source_filename).resolve() != source.resolve():
        raise RuntimeError(f"Font Face source mismatch at {face_path}: {source_filename}")

    face_saved = bool(unreal.EditorAssetLibrary.save_asset(face_path, only_if_is_dirty=False))
    font_saved = bool(unreal.EditorAssetLibrary.save_asset(font_path, only_if_is_dirty=False)) if font else None
    if not face_saved or (font is not None and not font_saved):
        raise RuntimeError(f"Could not save trial pair: face={face_saved}, font={font_saved}")

    return (
        {
            "culture": record["culture"],
            "source": str(source),
            "source_sha256": actual_hash,
            "face_path": face_path,
            "face_class": _class_name(face),
            "font_path": font_path or None,
            "font_class": _class_name(font) or None,
            "source_filename": source_filename,
            "face_saved": face_saved,
            "font_saved": font_saved,
            "imported_object_paths": list(task.get_editor_property("imported_object_paths")),
        },
        font,
    )


def _configure_fallback(font_path: str) -> dict:
    font = _require_asset(font_path, "Font")
    fallback_face = _require_asset(FALLBACK_FACE_PATH, "FontFace")
    properties_json = unreal.ToolsetLibrary.get_object_properties(font, ["CompositeFont"])
    properties = json.loads(properties_json)
    composite = properties["CompositeFont"]
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
        raise RuntimeError(f"ToolsetLibrary did not set the Composite Font fallback at {font_path}")
    after = json.loads(unreal.ToolsetLibrary.get_object_properties(font, ["CompositeFont"]))
    actual = after["CompositeFont"]["fallbackTypeface"]["typeface"]["fonts"][0]["font"]["fontFaceAsset"]
    expected = {"refPath": fallback_object_path}
    if actual != expected:
        raise RuntimeError(f"Fallback mismatch at {font_path}: expected {expected}, got {actual}")
    saved = bool(unreal.EditorAssetLibrary.save_asset(font_path, only_if_is_dirty=False))
    if not saved:
        raise RuntimeError(f"Could not save configured Runtime Font: {font_path}")
    return {"font_path": font_path, "fallback_face_path": fallback_object_path, "saved": saved}


def _delete_rejected_rancho_assets() -> list[str]:
    editor = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
    deleted = []
    for asset_path in REJECTED_RANCHO_ASSETS:
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if asset is None:
            continue
        editor.close_all_editors_for_asset(asset)
        if not unreal.EditorAssetLibrary.delete_asset(asset_path):
            raise RuntimeError(f"Could not delete rejected Rancho trial asset: {asset_path}")
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            raise RuntimeError(f"Rejected Rancho trial asset still exists: {asset_path}")
        deleted.append(asset_path)
    return deleted


def main() -> None:
    result: dict = {
        "ok": False,
        "engine_version": unreal.SystemLibrary.get_engine_version(),
        "destination": DESTINATION,
        "assets": [],
        "opened_font_editors": False,
    }
    try:
        result["deleted_rejected_assets"] = _delete_rejected_rancho_assets()
        if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION):
            unreal.EditorAssetLibrary.make_directory(DESTINATION)
        fonts = []
        for record in FONT_RECORDS:
            asset_result, font = _import_record(record)
            result["assets"].append(asset_result)
            if font is not None:
                fonts.append(font)
        result["fallbacks"] = [_configure_fallback(ZH_FONT_PATH)]
        editor = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
        for font in fonts:
            editor.close_all_editors_for_asset(font)
        result["opened_font_editors"] = bool(editor.open_editor_for_assets(fonts))
        if not result["opened_font_editors"]:
            raise RuntimeError("AssetEditorSubsystem did not open the Runtime Font assets")
        result["ok"] = True
        _write_result(result)
    except Exception as exc:
        result["error"] = str(exc)
        result["traceback"] = traceback.format_exc()
        _write_result(result)
        unreal.log_error("GameXXK font trial failed: " + str(exc))
        raise


if __name__ == "__main__":
    main()
