"""Prepare and import the seventeen approved PartyDeck card portraits.

The cards reuse character identity art instead of generating one illustration for
each card: the hero and named task NPCs use their approved original art, while
the six recruitable roles use the reviewed south-facing cell of their generated
eight-direction atlas.  The four route categories use reviewed, non-text
watercolor crests generated on a chroma-key source and recorded in a provenance
manifest.  This keeps every one of the cards visually tied to its owner or route
category without duplicating UI frames or recolouring the approved PSD frame.

By default this module only verifies its source contract.  ``--prepare`` writes
derived 171 x 205 portrait PNGs under ``SourceAssets/PartyDeck``.  Only
``--execute-import`` may write the thirteen isolated UE Texture2D assets.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Any

try:
    import unreal
except ImportError:  # Lets the immutable source contract run outside UE.
    unreal = None


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PSD_ROOT = (
    Path(r"C:\Users\shxuw\Downloads\nw-studio-nwueball-https-github-com")
    / "nw-studio-nwueball-https-github-com"
    / "work"
    / "psd_rebuild"
    / "clean_assets_v2"
)
PPT_EXTRACT_ROOT = PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "character-references" / "ppt-extract"
PACKED_ROOT = PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "character-references" / "packed"
CARD_PORTRAIT_ROOT = PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "card-portraits"
ROUTE_SOURCE_ROOT = CARD_PORTRAIT_ROOT / "route-source"
ROUTE_ALPHA_ROOT = CARD_PORTRAIT_ROOT / "route-alpha"
ROUTE_ART_MANIFEST_PATH = CARD_PORTRAIT_ROOT / "route-card-art-manifest.json"
DERIVED_ROOT = CARD_PORTRAIT_ROOT / "generated"
DESTINATION_ROOT = "/Game/GameXXK/UI/PartyDeck/CardArt"
PORTRAIT_SIZE = (171, 205)
ROLE_IDLE_ATLAS_SIZE = (171, 1640)
ROUTE_SOURCE_SIZE = (1024, 1536)
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


@dataclass(frozen=True)
class PortraitRecord:
    key: str
    asset_name: str
    source: Path
    source_sha256: str
    source_mode: str
    derived_name: str

    @property
    def derived_path(self) -> Path:
        return DERIVED_ROOT / self.derived_name

    @property
    def asset_path(self) -> str:
        return f"{DESTINATION_ROOT}/{self.asset_name}"


def _psd(name: str) -> Path:
    return PSD_ROOT / name


def _ppt(name: str) -> Path:
    return PPT_EXTRACT_ROOT / name


def _role(name: str) -> Path:
    return PACKED_ROOT / name


def _route_alpha(name: str) -> Path:
    return ROUTE_ALPHA_ROOT / name


# Source hashes intentionally duplicate the locked identity ledger.  Keeping them
# beside the UI importer makes an accidental source swap fail before an asset write.
PORTRAITS: tuple[PortraitRecord, ...] = (
    PortraitRecord("Hero", "T_CardPortrait_Hero", _psd("001.png"), "e0efe919f19fbc8f5bcbe2c3075d168557899c5182804bf772c219d9301970e7", "original_alpha", "hero.png"),
    PortraitRecord("Npc.TusiChief", "T_CardPortrait_Npc_TusiChief", _psd("065.png"), "29af3b326d171b67f5621249faa4c3eeb148a6fbcc81041d336f550dbb79a8ce", "original_alpha", "npc_tusi_chief.png"),
    PortraitRecord("Npc.SongJinBao", "T_CardPortrait_Npc_SongJinBao", _ppt("npc_songjinbao_image37.jpeg"), "64206e9b8909c46d4aef209ab8abf0b8d2302f5470351bb2ec1e66a9a1d53cbd", "original_opaque", "npc_song_jin_bao.png"),
    PortraitRecord("Npc.YueBai", "T_CardPortrait_Npc_YueBai", _psd("064.png"), "5bdc7c284ae1f0469418eafab6a6d8a3714cc0ca75a7964f73474c772a956ddb", "original_alpha", "npc_yue_bai.png"),
    PortraitRecord("Npc.ZhouGuangZu", "T_CardPortrait_Npc_ZhouGuangZu", _psd("063.png"), "237bfe3fa5198edc786fcae24a3b9b16615acc6c6abffb614ed9a2b62b2c2540", "original_alpha", "npc_zhou_guang_zu.png"),
    PortraitRecord("Npc.JinGui", "T_CardPortrait_Npc_JinGui", _ppt("npc_jingui_image33.jpeg"), "941a2a56e62e09a7b5749e34d2b07e407a12e2894a51cc62599a4808a2568caf", "original_opaque", "npc_jin_gui.png"),
    PortraitRecord("Npc.QiongMeiEr", "T_CardPortrait_Npc_QiongMeiEr", _ppt("npc_qiongmeier_image34.jpeg"), "c6b72f3e50b78f37cbb0a6d77b71485a1bba4d9a742cfc803f48116fb198b529", "original_opaque", "npc_qiong_mei_er.png"),
    PortraitRecord("Role.Blade", "T_CardPortrait_Role_Blade", _role("partner_blade_idle_8dir.png"), "aad0eca86bffaea68c22091f37e8c6c80759ac19c43be1f23aed98a03e9b7034", "role_south_cell", "role_blade.png"),
    PortraitRecord("Role.Guard", "T_CardPortrait_Role_Guard", _role("partner_guard_idle_8dir.png"), "0d7c087403b85c24ae24594a713e9df6b055d8cc71df415909fa2fb48e060233", "role_south_cell", "role_guard.png"),
    PortraitRecord("Role.Healer", "T_CardPortrait_Role_Healer", _role("partner_healer_idle_8dir.png"), "50a52cc60e39dd00603c24de5c650744d736fb513b804674e535c748f73602f9", "role_south_cell", "role_healer.png"),
    PortraitRecord("Role.Hunter", "T_CardPortrait_Role_Hunter", _role("partner_hunter_idle_8dir.png"), "29de830ab49a89871338ea2464c9b6b4d56d677270bb0e2c9070b14c60cbeae3", "role_south_cell", "role_hunter.png"),
    PortraitRecord("Role.Sorcerer", "T_CardPortrait_Role_Sorcerer", _role("partner_sorcerer_idle_8dir.png"), "ba1a959b76432c8df58c3ff5b18454c9bf5b1e55bf8ff95d479b2e9a0bd3379d", "role_south_cell", "role_sorcerer.png"),
    PortraitRecord("Role.FormationMaster", "T_CardPortrait_Role_FormationMaster", _role("partner_formation_master_idle_8dir.png"), "a5646af2e2730e1e1b459fb9bdcc0c0fba77a0240ada9000d65c7e74f4ee0a9a", "role_south_cell", "role_formation_master.png"),
    PortraitRecord("Route.General", "T_CardPortrait_Route_General", _route_alpha("route_general_alpha_v1.png"), "f5b149e4685769dd1e15676ed22341f3d5f8e1d6c9671bb97ba3c2b7ed38f35b", "generated_alpha", "route_general.png"),
    PortraitRecord("Route.Terrain", "T_CardPortrait_Route_Terrain", _route_alpha("route_terrain_alpha_v1.png"), "af5287ea0b7f1f1dfd9cd89687517c890f8cc7ed5bd0aea5f72095cc5e5af130", "generated_alpha", "route_terrain.png"),
    PortraitRecord("Route.Rare", "T_CardPortrait_Route_Rare", _route_alpha("route_rare_alpha_v1.png"), "629a778b35a9803baa25e7e06d877278b1bbe211a2d40fc18d8cc207ae2603fc", "generated_alpha", "route_rare.png"),
    PortraitRecord("Route.Boss", "T_CardPortrait_Route_Boss", _route_alpha("route_boss_alpha_v1.png"), "4e9de62c549c4e2d63df0e5f5391f1ec5d2d78ad3a79e402b2d8de8abf2cdfd4", "generated_alpha", "route_boss.png"),
)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _png_size(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != PNG_SIGNATURE or header[12:16] != b"IHDR":
        raise ValueError(f"not a PNG with an IHDR header: {path}")
    return struct.unpack(">II", header[16:24])


def _png_has_alpha(path: Path) -> bool:
    with path.open("rb") as stream:
        header = stream.read(26)
    if len(header) != 26 or header[:8] != PNG_SIGNATURE or header[12:16] != b"IHDR":
        raise ValueError(f"not a PNG with an IHDR header: {path}")
    # PNG color type 4 is gray+alpha and 6 is RGBA.
    return header[25] in {4, 6}


def _manifest_path(relative_path: str, field_name: str) -> Path:
    candidate = (CARD_PORTRAIT_ROOT / relative_path).resolve()
    try:
        candidate.relative_to(CARD_PORTRAIT_ROOT.resolve())
    except ValueError as error:
        raise ValueError(f"route-card manifest {field_name} escapes the card-portrait root: {relative_path}") from error
    return candidate


def _validate_route_art_manifest(route_records: tuple[PortraitRecord, ...]) -> None:
    if not ROUTE_ART_MANIFEST_PATH.is_file():
        raise FileNotFoundError(f"PartyDeck route-card art manifest is missing: {ROUTE_ART_MANIFEST_PATH}")
    manifest = json.loads(ROUTE_ART_MANIFEST_PATH.read_text(encoding="utf-8"))
    if manifest.get("schema_version") != 1 or manifest.get("generation_mode") != "built_in_imagegen_chroma_key":
        raise ValueError("PartyDeck route-card art manifest has an unsupported schema or generation mode")
    records = manifest.get("records")
    if not isinstance(records, list) or len(records) != 4:
        raise ValueError("PartyDeck route-card art manifest must contain exactly four category records")
    by_key = {record.get("key"): record for record in records if isinstance(record, dict)}
    expected_keys = {record.key for record in route_records}
    if set(by_key) != expected_keys:
        raise ValueError(f"PartyDeck route-card art manifest keys changed: {set(by_key)}")
    for route_record in route_records:
        manifest_record = by_key[route_record.key]
        if manifest_record.get("asset_name") != route_record.asset_name:
            raise ValueError(f"route-card manifest asset mismatch for {route_record.key}")
        alpha_source = _manifest_path(str(manifest_record.get("alpha_source", "")), "alpha_source")
        raw_source = _manifest_path(str(manifest_record.get("raw_chroma_source", "")), "raw_chroma_source")
        if alpha_source != route_record.source.resolve() or alpha_source.parent != ROUTE_ALPHA_ROOT.resolve():
            raise ValueError(f"route-card manifest alpha source mismatch for {route_record.key}")
        if raw_source.parent != ROUTE_SOURCE_ROOT.resolve():
            raise ValueError(f"route-card manifest chroma source is outside route-source for {route_record.key}")
        if tuple(manifest_record.get("source_size", ())) != ROUTE_SOURCE_SIZE:
            raise ValueError(f"route-card manifest raw source dimensions changed for {route_record.key}")
        if tuple(manifest_record.get("alpha_source_size", ())) != ROUTE_SOURCE_SIZE:
            raise ValueError(f"route-card manifest alpha source dimensions changed for {route_record.key}")
        if _sha256(raw_source) != manifest_record.get("raw_chroma_sha256"):
            raise ValueError(f"route-card chroma source hash changed for {route_record.key}")
        if _sha256(alpha_source) != manifest_record.get("alpha_sha256"):
            raise ValueError(f"route-card alpha source hash changed for {route_record.key}")


def _verify_record(record: PortraitRecord) -> None:
    if not record.source.is_file():
        raise FileNotFoundError(f"PartyDeck card portrait source is missing: {record.source}")
    actual_hash = _sha256(record.source)
    if actual_hash != record.source_sha256:
        raise ValueError(f"PartyDeck card portrait source hash changed for {record.key}: {actual_hash}")
    if record.source_mode == "role_south_cell":
        if _png_size(record.source) != ROLE_IDLE_ATLAS_SIZE:
            raise ValueError(f"PartyDeck role idle atlas dimensions changed for {record.key}")
    elif record.source_mode == "generated_alpha":
        if _png_size(record.source) != ROUTE_SOURCE_SIZE or not _png_has_alpha(record.source):
            raise ValueError(f"PartyDeck generated route alpha source is invalid for {record.key}")
    elif record.source_mode not in {"original_alpha", "original_opaque"}:
        raise ValueError(f"unknown PartyDeck card portrait source mode: {record.source_mode}")


def validate_portrait_plan() -> dict[str, Any]:
    """Read-only verification for the exact seventeen owner/category portraits."""
    if len(PORTRAITS) != 17:
        raise RuntimeError(f"PartyDeck card portrait plan must contain exactly 17 records, got {len(PORTRAITS)}")
    route_records = tuple(record for record in PORTRAITS if record.key.startswith("Route."))
    _validate_route_art_manifest(route_records)
    keys: set[str] = set()
    assets: set[str] = set()
    records: list[dict[str, Any]] = []
    for record in PORTRAITS:
        _verify_record(record)
        if record.key in keys or record.asset_path in assets:
            raise RuntimeError(f"duplicate PartyDeck card portrait record: {record.key}")
        keys.add(record.key)
        assets.add(record.asset_path)
        records.append({
            "key": record.key,
            "asset_name": record.asset_name,
            "asset_path": record.asset_path,
            "source_path": record.source,
            "source_mode": record.source_mode,
            "derived_path": record.derived_path,
            "portrait_size": PORTRAIT_SIZE,
        })
    return {
        "ok": True,
        "portrait_count": len(records),
        "destination_root": DESTINATION_ROOT,
        "derived_root": DERIVED_ROOT,
        "records": records,
    }


def _transparent_fit(source: Any) -> Any:
    # UE's embedded Python deliberately has no Pillow.  This import remains local
    # to source preparation so the commandlet can import already-reviewed PNGs.
    from PIL import Image

    rgba = source.convert("RGBA")
    bbox = rgba.getchannel("A").getbbox()
    if bbox:
        rgba = rgba.crop(bbox)
    rgba.thumbnail((151, 181), Image.Resampling.LANCZOS)
    output = Image.new("RGBA", PORTRAIT_SIZE, (0, 0, 0, 0))
    output.alpha_composite(rgba, ((PORTRAIT_SIZE[0] - rgba.width) // 2, PORTRAIT_SIZE[1] - rgba.height - 8))
    return output


def _build_one_portrait(record: PortraitRecord) -> Any:
    from PIL import Image, ImageOps

    with Image.open(record.source) as raw:
        if record.source_mode == "role_south_cell":
            return raw.convert("RGBA").crop((0, 0, PORTRAIT_SIZE[0], PORTRAIT_SIZE[1]))
        if record.source_mode in {"original_alpha", "generated_alpha"}:
            return _transparent_fit(raw)
        # Preserve an opaque original illustration, centered to a card-facing crop.
        return ImageOps.fit(raw.convert("RGB"), PORTRAIT_SIZE, method=Image.Resampling.LANCZOS).convert("RGBA")


def prepare_portrait_sources(destination_root: Path = DERIVED_ROOT) -> dict[str, Any]:
    """Write deterministic card portrait derivatives without changing any source art."""
    plan = validate_portrait_plan()
    destination_root = Path(destination_root)
    destination_root.mkdir(parents=True, exist_ok=True)
    written: list[str] = []
    for record in PORTRAITS:
        output = destination_root / record.derived_name
        image = _build_one_portrait(record)
        if image.size != PORTRAIT_SIZE:
            raise RuntimeError(f"PartyDeck derived portrait has wrong dimensions for {record.key}: {image.size}")
        image.save(output, format="PNG", optimize=True)
        if _png_size(output) != PORTRAIT_SIZE:
            raise RuntimeError(f"PartyDeck derived portrait could not be verified: {output}")
        written.append(str(output))
    return {**plan, "prepared_count": len(written), "prepared": written, "derived_root": destination_root}


def validate_prepared_portrait_sources() -> dict[str, Any]:
    """Verify the PNG derivatives without requiring Pillow in Unreal Python."""
    plan = validate_portrait_plan()
    prepared: list[str] = []
    for record in PORTRAITS:
        derived = record.derived_path
        if not derived.is_file():
            raise RuntimeError(
                f"PartyDeck portrait derivative is missing: {derived}. "
                "Run this pipeline with --prepare in the workspace Python first."
            )
        if _png_size(derived) != PORTRAIT_SIZE:
            raise RuntimeError(f"PartyDeck portrait derivative dimensions are invalid: {derived}")
        if record.source_mode == "generated_alpha" and not _png_has_alpha(derived):
            raise RuntimeError(f"PartyDeck route portrait derivative lost alpha: {derived}")
        prepared.append(str(derived))
    return {**plan, "prepared_count": len(prepared), "prepared": prepared}


def _require_unreal() -> None:
    if unreal is None:
        raise RuntimeError("UE Python is required to import PartyDeck card portrait textures")


def _configure_ui_texture(texture: object) -> None:
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("never_stream", True)


def _validate_imported_texture(texture: object, record: PortraitRecord, source: Path) -> None:
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"PartyDeck portrait is not a Texture2D: {record.asset_path}")
    if str(texture.get_path_name()) not in {record.asset_path, f"{record.asset_path}.{record.asset_name}"}:
        raise RuntimeError(f"PartyDeck portrait resolved outside its approved asset path: {texture.get_path_name()}")
    width = int(texture.blueprint_get_size_x())
    height = int(texture.blueprint_get_size_y())
    if (width, height) != PORTRAIT_SIZE:
        raise RuntimeError(f"PartyDeck portrait has wrong imported dimensions at {record.asset_path}: {(width, height)}")
    import_data = texture.get_editor_property("asset_import_data")
    imported_filename = str(import_data.get_first_filename()) if import_data else ""
    if not imported_filename or Path(imported_filename).resolve() != source.resolve():
        raise RuntimeError(f"PartyDeck portrait import source mismatch at {record.asset_path}")


def import_verified_portraits() -> dict[str, Any]:
    """Import only missing card portrait textures into their isolated UI root."""
    _require_unreal()
    prepared = validate_prepared_portrait_sources()
    if not unreal.EditorAssetLibrary.does_directory_exist(DESTINATION_ROOT):
        if not unreal.EditorAssetLibrary.make_directory(DESTINATION_ROOT):
            raise RuntimeError(f"failed to create PartyDeck card portrait directory: {DESTINATION_ROOT}")
    imported: list[str] = []
    validated_existing: list[str] = []
    for record in PORTRAITS:
        source = DERIVED_ROOT / record.derived_name
        if unreal.EditorAssetLibrary.does_asset_exist(record.asset_path):
            texture = unreal.EditorAssetLibrary.load_asset(record.asset_path)
            _validate_imported_texture(texture, record, source)
            validated_existing.append(record.asset_path)
            continue
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = DESTINATION_ROOT
        task.destination_name = record.asset_name
        task.automated = True
        task.save = False
        task.replace_existing = False
        task.replace_existing_settings = False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.EditorAssetLibrary.load_asset(record.asset_path)
        if texture is None:
            raise RuntimeError(f"failed to import PartyDeck card portrait: {record.asset_path}")
        _configure_ui_texture(texture)
        if not unreal.EditorAssetLibrary.save_loaded_asset(texture):
            raise RuntimeError(f"failed to save PartyDeck card portrait: {record.asset_path}")
        _validate_imported_texture(texture, record, source)
        imported.append(record.asset_path)
    return {
        **prepared,
        "imported_count": len(imported),
        "validated_existing_count": len(validated_existing),
        "imported": imported,
        "validated_existing": validated_existing,
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
    parser.add_argument("--prepare", action="store_true", help="Create only deterministic local portrait PNG derivatives.")
    parser.add_argument("--execute-import", action="store_true", help="Import only previously prepared isolated UI Texture2D assets.")
    args = parser.parse_args(argv)
    if args.execute_import:
        result = import_verified_portraits()
    elif args.prepare:
        result = prepare_portrait_sources()
    else:
        result = validate_portrait_plan()
    print(json.dumps(_jsonable(result), ensure_ascii=False, indent=2))
    return result


if __name__ == "__main__":
    main()
