"""Import the single approved first-row PSD card frame after a strict source check.

This module intentionally imports no asset at module load.  Its command-line default is
verification only.  Call ``import_verified_card_frame()`` from UE Python only after the
source check succeeds and a reviewer has approved the resulting asset write.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
from typing import Any

try:
    import unreal
except ImportError:  # Allows the source contract to be tested outside the UE editor.
    unreal = None


PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOURCE_ROOT = (
    Path(r"C:\Users\shxuw\Downloads\nw-studio-nwueball-https-github-com")
    / "nw-studio-nwueball-https-github-com"
    / "work"
    / "psd_rebuild"
    / "clean_assets_v2"
)
APPROVED_SOURCE_NAME = "057.png"
APPROVED_SOURCE = SOURCE_ROOT / APPROVED_SOURCE_NAME
REJECTED_SECOND_ROW_SOURCES = frozenset({"060.png", "061.png", "062.png"})
EXPECTED_SOURCE_SIZE = (452, 516)
EXPECTED_SOURCE_SHA256 = "c9b0333eca9a21c45f79450db5c4f940eb23c4ffbb4290807d4194cb44025209"
RUNTIME_DRAW_SIZE = (113, 129)
DESTINATION = "/Game/GameXXK/UI/Cards/Textures"
ASSET_NAME = "T_CardFrame_PSD057"
ASSET_PATH = f"{DESTINATION}/{ASSET_NAME}"
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def _read_png_dimensions(source: Path) -> tuple[int, int]:
    """Return PNG IHDR dimensions without depending on Pillow or UE runtime modules."""
    with source.open("rb") as image_file:
        header = image_file.read(24)
    if len(header) != 24 or header[:8] != PNG_SIGNATURE or header[12:16] != b"IHDR":
        raise ValueError(f"source is not a readable PNG with an IHDR header: {source}")
    return struct.unpack(">II", header[16:24])


def _sha256(source: Path) -> str:
    digest = hashlib.sha256()
    with source.open("rb") as image_file:
        for chunk in iter(lambda: image_file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_approved_source(source: Path = APPROVED_SOURCE) -> dict[str, Any]:
    """Validate that *only* approved first-row atom 057 may enter the card pipeline."""
    source = Path(source)
    if source.name in REJECTED_SECOND_ROW_SOURCES:
        raise ValueError(
            f"second-row source {source.name} is forbidden; only {APPROVED_SOURCE_NAME} may be imported"
        )
    if source.name != APPROVED_SOURCE_NAME:
        raise ValueError(
            f"unapproved card-frame source {source.name}; only {APPROVED_SOURCE_NAME} may be imported"
        )
    if not source.is_file():
        raise FileNotFoundError(f"approved card-frame source is missing: {source}")

    width, height = _read_png_dimensions(source)
    if (width, height) != EXPECTED_SOURCE_SIZE:
        raise ValueError(
            f"unexpected {APPROVED_SOURCE_NAME} dimensions {(width, height)}; "
            f"expected {EXPECTED_SOURCE_SIZE}"
        )
    source_sha256 = _sha256(source)
    if source_sha256 != EXPECTED_SOURCE_SHA256:
        raise ValueError(
            f"unexpected {APPROVED_SOURCE_NAME} SHA-256 {source_sha256}; "
            f"expected {EXPECTED_SOURCE_SHA256}"
        )
    return {
        "name": source.name,
        "source": str(source),
        "width": width,
        "height": height,
        "sha256": source_sha256,
        "runtime_draw_size": list(RUNTIME_DRAW_SIZE),
        "runtime_draw_width": RUNTIME_DRAW_SIZE[0],
        "runtime_draw_height": RUNTIME_DRAW_SIZE[1],
        "asset_path": ASSET_PATH,
    }


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError(
            "UE Python is required for asset import; run verify_approved_source outside UE or "
            "call import_verified_card_frame through the running editor."
        )


def _ensure_directory(path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def _try_set(texture: object, property_name: str, value: object) -> None:
    try:
        texture.set_editor_property(property_name, value)
    except (AttributeError, RuntimeError):
        pass


def _configure_ui_texture(texture: object) -> None:
    _try_set(texture, "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    _try_set(texture, "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    _try_set(texture, "lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    _try_set(texture, "filter", unreal.TextureFilter.TF_BILINEAR)
    _try_set(texture, "address_x", unreal.TextureAddress.TA_CLAMP)
    _try_set(texture, "address_y", unreal.TextureAddress.TA_CLAMP)
    _try_set(texture, "srgb", True)
    _try_set(texture, "never_stream", True)
    _try_set(texture, "compression_no_alpha", False)


def _validate_existing_texture(texture: object) -> None:
    """Reject a pre-existing asset unless it is the exact reviewed source import.

    The PSD frame is a shared, user-visible UI primitive.  A rerun must never
    overwrite a manually tuned UE texture merely because its object path matches.
    """
    if not isinstance(texture, unreal.Texture2D):
        loaded_class = texture.get_class().get_name() if texture else "None"
        raise RuntimeError(f"existing card-frame asset is not Texture2D {ASSET_PATH}; class={loaded_class}")
    if (int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())) != EXPECTED_SOURCE_SIZE:
        raise RuntimeError(f"existing card-frame asset has unexpected dimensions: {ASSET_PATH}")
    import_data = texture.get_editor_property("asset_import_data")
    imported_filename = str(import_data.get_first_filename()) if import_data else ""
    if not imported_filename or Path(imported_filename).resolve() != APPROVED_SOURCE.resolve():
        raise RuntimeError(f"existing card-frame asset source mismatch; refusing to overwrite: {ASSET_PATH}")


def import_verified_card_frame() -> dict[str, Any]:
    """Import exactly one missing validated reusable UI texture, never overwriting an existing asset."""
    manifest = verify_approved_source()
    _require_unreal()
    _ensure_directory(DESTINATION)

    if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
        texture = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
        _validate_existing_texture(texture)
        manifest["imported_asset"] = texture.get_path_name()
        manifest["validated_existing"] = True
        return manifest

    task = unreal.AssetImportTask()
    task.filename = str(APPROVED_SOURCE)
    task.destination_path = DESTINATION
    task.destination_name = ASSET_NAME
    task.automated = True
    task.replace_existing = False
    task.replace_existing_settings = False
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    texture = (
        unreal.EditorAssetLibrary.load_asset(f"{ASSET_PATH}.{ASSET_NAME}")
        or unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    )
    if not isinstance(texture, unreal.Texture2D):
        loaded_class = texture.get_class().get_name() if texture else "None"
        raise RuntimeError(f"failed to import Texture2D {ASSET_PATH}; class={loaded_class}")
    _configure_ui_texture(texture)
    if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
        raise RuntimeError(f"failed to save imported Texture2D: {ASSET_PATH}")

    manifest["imported_asset"] = texture.get_path_name()
    return manifest


def main(argv: list[str] | None = None) -> None:
    parser = argparse.ArgumentParser(
        description="Verify or explicitly import the one approved first-row PSD card frame."
    )
    parser.add_argument(
        "--execute-import",
        action="store_true",
        help="After validation, import only T_CardFrame_PSD057 through a running UE editor.",
    )
    args = parser.parse_args(argv)
    result = import_verified_card_frame() if args.execute_import else verify_approved_source()
    print(json.dumps({"ok": True, **result}, ensure_ascii=False))


if __name__ == "__main__":
    main()
