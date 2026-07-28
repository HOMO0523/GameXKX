"""Preflight and explicitly import the reviewed PartyDeck sprite texture atlases.

This importer is intentionally limited to the twenty-four packed atlases in
``SourceAssets/PartyDeck/character-references/packed``.  It never generates
art, creates sprites/flipbooks, modifies a character package, replaces an
existing texture, or deletes any UE asset.  Without ``--execute-import`` it
only performs the same source/manifest checks that protect the commandlet
write path.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import re
import struct
from pathlib import Path
from typing import Any

try:
    import unreal
except ImportError:  # Allows the preflight contract to run outside Unreal.
    unreal = None


PROJECT_ROOT = Path(__file__).resolve().parents[2]
MANIFEST_PATH = (
    PROJECT_ROOT
    / "SourceAssets"
    / "PartyDeck"
    / "character-references"
    / "character-sheet-manifest.json"
)
PACKED_ROOT = PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "character-references" / "packed"
VALIDATOR_PATH = PROJECT_ROOT / "scripts" / "verify_party_deck_sprite_sources.py"
DESTINATION_ROOT = "/Game/GameXXK/Sprites/Generated/PartyDeck"
EXPECTED_TEXTURE_SETTINGS = {"filter": "nearest", "mipmaps": "none"}
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
ATLAS_SPECS = (
    ("idle_atlas", "Idle8Dir", (171, 1640)),
    ("walk_atlas", "Walk8Dir", (1026, 1640)),
)


def _read_png_dimensions(source: Path) -> tuple[int, int]:
    with source.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != PNG_SIGNATURE or header[12:16] != b"IHDR":
        raise ValueError(f"not a readable PNG with an IHDR header: {source}")
    return struct.unpack(">II", header[16:24])


def _load_validator_module():
    spec = importlib.util.spec_from_file_location("gamexxk_party_deck_manifest_validator", VALIDATOR_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"unable to load PartyDeck manifest validator: {VALIDATOR_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _safe_source_path(raw_path: object) -> Path:
    if not isinstance(raw_path, str) or not raw_path:
        raise RuntimeError("PartyDeck atlas path is missing from the manifest")
    source = (PROJECT_ROOT / raw_path).resolve()
    if not source.is_relative_to(PACKED_ROOT.resolve()):
        raise RuntimeError(f"PartyDeck atlas must remain under packed root {PACKED_ROOT}: {source}")
    return source


def _asset_slug(target_id: object) -> str:
    if not isinstance(target_id, str) or not target_id:
        raise RuntimeError("PartyDeck target id is missing")
    slug = re.sub(r"[^A-Za-z0-9]+", "_", target_id).strip("_")
    if not slug:
        raise RuntimeError(f"PartyDeck target id cannot become an asset name: {target_id!r}")
    return slug


def _manifest_targets() -> list[dict[str, Any]]:
    try:
        manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise RuntimeError(f"PartyDeck manifest is missing: {MANIFEST_PATH}") from exc
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"PartyDeck manifest is not valid JSON: {exc}") from exc
    targets = manifest.get("production_targets") if isinstance(manifest, dict) else None
    if not isinstance(targets, list) or len(targets) != 12 or not all(isinstance(target, dict) for target in targets):
        raise RuntimeError("PartyDeck manifest must contain exactly twelve production targets")
    return targets


def _run_manifest_preflight() -> None:
    validator = _load_validator_module()
    report = validator.validate_manifest(MANIFEST_PATH)
    if not report.get("ok"):
        errors = report.get("errors", [])
        raise RuntimeError(f"PartyDeck manifest validation failed: {errors}")
    if not report.get("production_ready") or report.get("ready_blocker_count") != 0:
        blockers = report.get("ready_blockers", [])
        raise RuntimeError(
            "PartyDeck import requires all twelve targets to be reviewed and packed; "
            f"blockers={blockers}"
        )


def validate_import_plan() -> dict[str, Any]:
    """Return a write-free, deterministic plan for the exact twenty-four textures."""
    _run_manifest_preflight()
    textures: list[dict[str, Any]] = []
    asset_paths: set[str] = set()
    for target in _manifest_targets():
        target_id = target.get("id")
        if target.get("production_state") != "reviewed_ready_for_import":
            raise RuntimeError(f"PartyDeck target is not reviewed_ready_for_import: {target_id}")
        output = target.get("output")
        if not isinstance(output, dict) or output.get("texture_root") != DESTINATION_ROOT:
            raise RuntimeError(f"PartyDeck target has an unsafe texture root: {target_id}")
        asset_prefix = f"T_PartyDeck_{_asset_slug(target_id)}"
        for atlas_key, suffix, expected_pixels in ATLAS_SPECS:
            source = _safe_source_path(output.get(atlas_key))
            if not source.is_file():
                raise RuntimeError(f"PartyDeck packed atlas is missing: {source}")
            actual_pixels = _read_png_dimensions(source)
            if actual_pixels != expected_pixels:
                raise RuntimeError(
                    f"PartyDeck atlas dimensions are wrong for {target_id}/{atlas_key}: "
                    f"got {actual_pixels}, expected {expected_pixels}"
                )
            asset_name = f"{asset_prefix}_{suffix}"
            asset_path = f"{DESTINATION_ROOT}/{asset_name}"
            if asset_path in asset_paths:
                raise RuntimeError(f"PartyDeck target ids produce duplicate texture asset path: {asset_path}")
            asset_paths.add(asset_path)
            textures.append({
                "target_id": target_id,
                "atlas_key": atlas_key,
                "source_path": source,
                "expected_pixels": expected_pixels,
                "asset_name": asset_name,
                "asset_path": asset_path,
            })
    if len(textures) != 24:
        raise RuntimeError(f"PartyDeck import plan must contain 24 textures, got {len(textures)}")
    return {
        "ok": True,
        "manifest": MANIFEST_PATH,
        "destination_root": DESTINATION_ROOT,
        "texture_settings": dict(EXPECTED_TEXTURE_SETTINGS),
        "texture_count": len(textures),
        "textures": textures,
    }


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError(
            "UE Python is required for PartyDeck texture import; run preflight outside UE or "
            "use UnrealEditor-Cmd with the execute wrapper."
        )


def _ensure_destination_root() -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION_ROOT):
        if not unreal.EditorAssetLibrary.make_directory(DESTINATION_ROOT):
            raise RuntimeError(f"failed to create isolated PartyDeck texture root: {DESTINATION_ROOT}")


def _texture_size(texture: object) -> tuple[int, int]:
    for method_name in ("blueprint_get_size_x", "get_size_x"):
        method = getattr(texture, method_name, None)
        if callable(method):
            width = int(method())
            height_method = getattr(texture, method_name.replace("_x", "_y"), None)
            if callable(height_method):
                return width, int(height_method())
    source = texture.get_editor_property("source")
    get_size_x = getattr(source, "get_size_x", None)
    get_size_y = getattr(source, "get_size_y", None)
    if callable(get_size_x) and callable(get_size_y):
        return int(get_size_x()), int(get_size_y())
    raise RuntimeError(f"cannot read imported Texture2D dimensions: {texture}")


def _configure_texture(texture: object) -> None:
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)


def _object_path_is_expected(texture: object, asset_path: str) -> bool:
    actual_path = str(texture.get_path_name())
    return actual_path == asset_path or actual_path.startswith(f"{asset_path}.")


def _import_source_matches(texture: object, source_path: Path) -> bool:
    try:
        import_data = texture.get_editor_property("asset_import_data")
        get_first_filename = getattr(import_data, "get_first_filename", None)
        imported_filename = str(get_first_filename()) if callable(get_first_filename) else ""
    except (AttributeError, RuntimeError):
        return False
    if not imported_filename:
        return False
    try:
        return Path(imported_filename).resolve() == source_path.resolve()
    except OSError:
        return False


def _validate_imported_texture(texture: object, record: dict[str, Any]) -> None:
    asset_path = str(record["asset_path"])
    source_path = Path(record["source_path"])
    expected_pixels = tuple(record["expected_pixels"])
    if not isinstance(texture, unreal.Texture2D):
        actual_class = texture.get_class().get_name() if texture else "None"
        raise RuntimeError(f"PartyDeck asset is not a Texture2D at {asset_path}: {actual_class}")
    if not _object_path_is_expected(texture, asset_path):
        raise RuntimeError(f"PartyDeck texture resolved outside its approved root: {texture.get_path_name()}")
    if _texture_size(texture) != expected_pixels:
        raise RuntimeError(
            f"PartyDeck imported texture has wrong dimensions at {asset_path}: "
            f"got {_texture_size(texture)}, expected {expected_pixels}"
        )
    if texture.get_editor_property("mip_gen_settings") != unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS:
        raise RuntimeError(f"PartyDeck texture is not configured with no mipmaps: {asset_path}")
    if texture.get_editor_property("filter") != unreal.TextureFilter.TF_NEAREST:
        raise RuntimeError(f"PartyDeck texture is not configured with nearest filtering: {asset_path}")
    if not _import_source_matches(texture, source_path):
        raise RuntimeError(f"PartyDeck texture source does not match reviewed packed atlas: {asset_path}")


def _import_new_texture(record: dict[str, Any]) -> object:
    source_path = Path(record["source_path"])
    asset_path = str(record["asset_path"])
    task = unreal.AssetImportTask()
    task.filename = str(source_path)
    task.destination_path = DESTINATION_ROOT
    task.destination_name = str(record["asset_name"])
    task.automated = True
    task.save = False
    task.replace_existing = False
    task.replace_existing_settings = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if texture is None:
        raise RuntimeError(f"failed to import PartyDeck Texture2D: {asset_path}")
    _configure_texture(texture)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError(f"failed to save imported PartyDeck Texture2D: {asset_path}")
    return texture


def import_verified_party_deck_textures() -> dict[str, Any]:
    """Import only missing reviewed textures; validate but never rewrite existing ones."""
    plan = validate_import_plan()
    _require_unreal()
    _ensure_destination_root()
    imported: list[dict[str, str]] = []
    validated_existing: list[dict[str, str]] = []
    for record in plan["textures"]:
        asset_path = str(record["asset_path"])
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            texture = unreal.EditorAssetLibrary.load_asset(asset_path)
            _validate_imported_texture(texture, record)
            validated_existing.append({"asset_path": asset_path, "action": "validated_existing"})
            continue
        texture = _import_new_texture(record)
        _validate_imported_texture(texture, record)
        imported.append({"asset_path": asset_path, "action": "imported"})
    return {
        "ok": True,
        "destination_root": DESTINATION_ROOT,
        "texture_settings": dict(EXPECTED_TEXTURE_SETTINGS),
        "imported_count": len(imported),
        "validated_existing_count": len(validated_existing),
        "imported": imported,
        "validated_existing": validated_existing,
        "expected_texture_count": plan["texture_count"],
    }


def _jsonable(value: Any) -> Any:
    if isinstance(value, Path):
        return str(value)
    if isinstance(value, dict):
        return {key: _jsonable(item) for key, item in value.items()}
    if isinstance(value, list):
        return [_jsonable(item) for item in value]
    if isinstance(value, tuple):
        return [_jsonable(item) for item in value]
    return value


def main(argv: list[str] | None = None) -> dict[str, Any]:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--execute-import",
        action="store_true",
        help="After preflight, import only missing PartyDeck textures through UE Python.",
    )
    args = parser.parse_args(argv)
    result = import_verified_party_deck_textures() if args.execute_import else validate_import_plan()
    print(json.dumps(_jsonable(result), ensure_ascii=False, indent=2))
    return result


if __name__ == "__main__":
    main()
