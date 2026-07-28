#!/usr/bin/env python3
"""Read-only validation for the PartyDeck eight-direction sprite preparation manifest."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
import zipfile
from pathlib import Path
from typing import Any


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "character-references" / "character-sheet-manifest.json"

ROW_ORDER = ["S", "SW", "W", "NW", "N", "NE", "E", "SE"]
ENGINE_ROW_ORDER = ["South", "SouthWest", "West", "NorthWest", "North", "NorthEast", "East", "SouthEast"]
EXPECTED_TARGET_IDS = [
    "Npc.TusiChief",
    "Npc.SongJinBao",
    "Npc.YueBai",
    "Npc.ZhouGuangZu",
    "Npc.JinGui",
    "Npc.QiongMeiEr",
    "PartnerRole.Blade",
    "PartnerRole.Guard",
    "PartnerRole.Healer",
    "PartnerRole.Hunter",
    "PartnerRole.Sorcerer",
    "PartnerRole.FormationMaster",
]
EXPECTED_NPC_MEDIA = {
    "Npc.TusiChief": ("ppt/media/image38.jpeg", "ddaa1e5d9e5ba6c6edfad2937e477fe53909a37c8d14d5d78ae1c86f0a219e17"),
    "Npc.SongJinBao": ("ppt/media/image37.jpeg", "64206e9b8909c46d4aef209ab8abf0b8d2302f5470351bb2ec1e66a9a1d53cbd"),
    "Npc.YueBai": ("ppt/media/image35.jpeg", "eac69e6e4dcd1e93b194738dae9f3be9792ed49436621cc76deec5d547bf1cb9"),
    "Npc.ZhouGuangZu": ("ppt/media/image32.jpeg", "41d661961a954fd1c497ee6dc90190b4b0b2c5511addc3c7243d586655b0d3b5"),
    "Npc.JinGui": ("ppt/media/image33.jpeg", "941a2a56e62e09a7b5749e34d2b07e407a12e2894a51cc62599a4808a2568caf"),
    "Npc.QiongMeiEr": ("ppt/media/image34.jpeg", "c6b72f3e50b78f37cbb0a6d77b71485a1bba4d9a742cfc803f48116fb198b529"),
}
FORBIDDEN_IDENTITIES = {"Npc.NiuHuan", "Npc.SiQingNiang"}
FORBIDDEN_EXISTING_ROOTS = (
    "/Game/GameXXK/Characters/Hero",
    "/Game/GameXXK/Characters/Follower",
    "/Game/GameXXK/Characters/Merchant",
    "/Game/GameXXK/Characters/Enemies",
)


def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _sha256_file(file_path: Path) -> str:
    digest = hashlib.sha256()
    with file_path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _png_dimensions(file_path: Path) -> tuple[int, int]:
    with file_path.open("rb") as stream:
        header = stream.read(24)
    if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError("not a PNG with an IHDR header")
    return struct.unpack(">II", header[16:24])


def _jpeg_dimensions(data: bytes) -> tuple[int, int]:
    if len(data) < 4 or data[:2] != b"\xff\xd8":
        raise ValueError("not a JPEG stream")
    offset = 2
    sof_markers = {
        0xC0, 0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7,
        0xC9, 0xCA, 0xCB, 0xCD, 0xCE, 0xCF,
    }
    while offset < len(data):
        while offset < len(data) and data[offset] != 0xFF:
            offset += 1
        while offset < len(data) and data[offset] == 0xFF:
            offset += 1
        if offset >= len(data):
            break
        marker = data[offset]
        offset += 1
        if marker in (0xD8, 0xD9) or 0xD0 <= marker <= 0xD7:
            continue
        if offset + 2 > len(data):
            break
        segment_size = struct.unpack(">H", data[offset:offset + 2])[0]
        if segment_size < 2 or offset + segment_size > len(data):
            break
        if marker in sof_markers:
            if segment_size < 7:
                break
            height, width = struct.unpack(">HH", data[offset + 3:offset + 7])
            return width, height
        offset += segment_size
    raise ValueError("no JPEG start-of-frame marker")


def _path_is_safe_source_relative(raw_path: object) -> bool:
    if not isinstance(raw_path, str) or not raw_path:
        return False
    normalized = raw_path.replace("\\", "/")
    return (
        not Path(raw_path).is_absolute()
        and not normalized.startswith("../")
        and "/../" not in normalized
        and normalized.startswith("SourceAssets/PartyDeck/")
    )


def _add_error(errors: list[dict[str, str]], scope: str, reason: str) -> None:
    errors.append({"scope": scope, "reason": reason})


def _validate_atlas_contract(contract: object, errors: list[dict[str, str]]) -> None:
    if not isinstance(contract, dict):
        _add_error(errors, "atlas_contract", "missing_object")
        return
    expected_values: dict[str, object] = {
        "cell_pixels": [171, 205],
        "idle_atlas_pixels": [171, 1640],
        "walk_atlas_pixels": [1026, 1640],
        "walk_frames_per_direction": 6,
        "row_order": ROW_ORDER,
        "engine_row_order": ENGINE_ROW_ORDER,
        "pivot": "bottom-center",
        "pixels_per_unreal_unit": 1,
        "direct_diagonal_art_required": True,
    }
    for key, expected in expected_values.items():
        if contract.get(key) != expected:
            _add_error(errors, f"atlas_contract.{key}", f"expected_{expected!r}_got_{contract.get(key)!r}")


def _validate_existing_hero(manifest: dict[str, Any], errors: list[dict[str, str]]) -> None:
    templates = manifest.get("read_only_templates")
    if not isinstance(templates, list):
        _add_error(errors, "read_only_templates", "missing_list")
        return
    hero = next((item for item in templates if isinstance(item, dict) and item.get("id") == "Hero.Player"), None)
    if not isinstance(hero, dict):
        _add_error(errors, "read_only_templates.Hero.Player", "missing")
        return
    expected_files = {
        "idle_atlas": ((171, 1640), "ce93b82239f1846f45144cffbd48605f26faf6cf99310627766b374ae1e3d92a"),
        "walk_atlas": ((1026, 1640), "e90a5629778bd2b8c482293065bd00123906c3da256a48f31cdcc1d9975a7467"),
    }
    for key, (expected_dimensions, expected_hash) in expected_files.items():
        raw_path = hero.get(key)
        file_path = PROJECT_ROOT / str(raw_path)
        if not file_path.is_file():
            _add_error(errors, f"read_only_templates.Hero.Player.{key}", "missing_existing_template")
            continue
        try:
            dimensions = _png_dimensions(file_path)
        except ValueError as exc:
            _add_error(errors, f"read_only_templates.Hero.Player.{key}", str(exc))
            continue
        if dimensions != expected_dimensions:
            _add_error(errors, f"read_only_templates.Hero.Player.{key}", f"expected_dimensions_{expected_dimensions}_got_{dimensions}")
        actual_hash = _sha256_file(file_path)
        if actual_hash != expected_hash or hero.get(f"{key}_sha256") != expected_hash:
            _add_error(errors, f"read_only_templates.Hero.Player.{key}", "hash_mismatch")


def _validate_source_reference(
    reference: object,
    ppt_members: dict[str, bytes],
    errors: list[dict[str, str]],
    scope: str,
) -> None:
    if not isinstance(reference, dict):
        _add_error(errors, scope, "reference_is_not_an_object")
        return
    kind = reference.get("kind")
    expected_pixels = reference.get("pixels")
    if kind == "ppt_embedded_media":
        member = reference.get("member")
        if not isinstance(member, str) or member not in ppt_members:
            _add_error(errors, scope, "ppt_member_missing")
            return
        data = ppt_members[member]
        if _sha256_bytes(data) != reference.get("sha256"):
            _add_error(errors, scope, "ppt_member_sha256_mismatch")
        try:
            pixels = list(_jpeg_dimensions(data))
        except ValueError as exc:
            _add_error(errors, scope, str(exc))
        else:
            if pixels != expected_pixels:
                _add_error(errors, scope, f"expected_pixels_{expected_pixels}_got_{pixels}")
    elif kind == "psd_clean_cutout":
        raw_path = reference.get("path")
        if not isinstance(raw_path, str):
            _add_error(errors, scope, "missing_psd_path")
            return
        file_path = Path(raw_path)
        if not file_path.is_file():
            _add_error(errors, scope, "psd_cutout_missing")
            return
        if _sha256_file(file_path) != reference.get("sha256"):
            _add_error(errors, scope, "psd_cutout_sha256_mismatch")
        try:
            pixels = list(_png_dimensions(file_path))
        except ValueError as exc:
            _add_error(errors, scope, str(exc))
        else:
            if pixels != expected_pixels:
                _add_error(errors, scope, f"expected_pixels_{expected_pixels}_got_{pixels}")
    else:
        _add_error(errors, scope, f"unsupported_reference_kind_{kind!r}")


def _validate_targets(manifest: dict[str, Any], ppt_members: dict[str, bytes], errors: list[dict[str, str]]) -> tuple[list[dict[str, Any]], list[dict[str, str]]]:
    targets = manifest.get("production_targets")
    ready_blockers: list[dict[str, str]] = []
    if not isinstance(targets, list):
        _add_error(errors, "production_targets", "missing_list")
        return [], ready_blockers
    target_ids = [item.get("id") for item in targets if isinstance(item, dict)]
    if target_ids != EXPECTED_TARGET_IDS:
        _add_error(errors, "production_targets", "target_ids_do_not_match_the_approved_twelve")
    if len(target_ids) != len(set(target_ids)):
        _add_error(errors, "production_targets", "duplicate_target_id")

    for target in targets:
        if not isinstance(target, dict):
            _add_error(errors, "production_targets", "target_is_not_an_object")
            continue
        target_id = str(target.get("id", ""))
        scope = f"production_targets.{target_id}"
        if target_id in FORBIDDEN_IDENTITIES:
            _add_error(errors, scope, "forbidden_identity")
        kind = target.get("kind")
        if target_id in EXPECTED_NPC_MEDIA:
            if kind != "task_npc" or target.get("identity_policy") != "locked_existing_identity":
                _add_error(errors, scope, "task_npc_identity_policy_mismatch")
            references = target.get("reference_sources")
            if not isinstance(references, list):
                _add_error(errors, scope, "missing_locked_identity_references")
            else:
                matching = [reference for reference in references if isinstance(reference, dict) and reference.get("kind") == "ppt_embedded_media"]
                member, expected_hash = EXPECTED_NPC_MEDIA[target_id]
                if len(matching) != 1 or matching[0].get("member") != member or matching[0].get("sha256") != expected_hash:
                    _add_error(errors, scope, "approved_ppt_identity_reference_mismatch")
                for index, reference in enumerate(references):
                    _validate_source_reference(reference, ppt_members, errors, f"{scope}.reference_sources[{index}]")
        else:
            if kind != "permanent_partner_role":
                _add_error(errors, scope, "partner_role_kind_mismatch")
            if target.get("identity_policy") != "one_generated_archetype_shared_by_four_recruit_templates":
                _add_error(errors, scope, "partner_role_identity_policy_mismatch")
            palette = target.get("role_palette")
            if not isinstance(palette, dict) or not isinstance(palette.get("hex"), str):
                _add_error(errors, scope, "missing_role_palette")
        output = target.get("output")
        if not isinstance(output, dict):
            _add_error(errors, scope, "missing_output")
            continue
        for atlas_key, expected_dimensions in (("idle_atlas", (171, 1640)), ("walk_atlas", (1026, 1640))):
            raw_path = output.get(atlas_key)
            if not _path_is_safe_source_relative(raw_path):
                _add_error(errors, f"{scope}.output.{atlas_key}", "must_be_a_safe_SourceAssets_PartyDeck_relative_path")
                continue
            file_path = PROJECT_ROOT / str(raw_path)
            if not file_path.is_file():
                ready_blockers.append({"id": target_id, "artifact": atlas_key, "reason": "missing_packed_atlas"})
            else:
                try:
                    dimensions = _png_dimensions(file_path)
                except ValueError as exc:
                    ready_blockers.append({"id": target_id, "artifact": atlas_key, "reason": str(exc)})
                else:
                    if dimensions != expected_dimensions:
                        ready_blockers.append({"id": target_id, "artifact": atlas_key, "reason": f"expected_dimensions_{expected_dimensions}_got_{dimensions}"})
        texture_root = output.get("texture_root")
        if texture_root != "/Game/GameXXK/Sprites/Generated/PartyDeck":
            _add_error(errors, f"{scope}.output.texture_root", "must_use_isolated_PartyDeck_texture_root")
        character_root = output.get("character_root")
        if not isinstance(character_root, str) or not character_root.startswith("/Game/GameXXK/Characters/PartyDeck"):
            _add_error(errors, f"{scope}.output.character_root", "must_use_isolated_PartyDeck_character_root")
        elif any(character_root.startswith(root) for root in FORBIDDEN_EXISTING_ROOTS):
            _add_error(errors, f"{scope}.output.character_root", "must_not_touch_existing_character_roots")
    return targets, ready_blockers


def validate_manifest(manifest_path: Path) -> dict[str, Any]:
    errors: list[dict[str, str]] = []
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        return {"ok": False, "manifest": str(manifest_path), "errors": [{"scope": "manifest", "reason": "missing"}]}
    except json.JSONDecodeError as exc:
        return {"ok": False, "manifest": str(manifest_path), "errors": [{"scope": "manifest", "reason": f"invalid_json_{exc}"}]}
    if not isinstance(manifest, dict):
        return {"ok": False, "manifest": str(manifest_path), "errors": [{"scope": "manifest", "reason": "root_is_not_an_object"}]}

    if manifest.get("schema_version") != 1:
        _add_error(errors, "schema_version", "expected_1")
    _validate_atlas_contract(manifest.get("atlas_contract"), errors)
    _validate_existing_hero(manifest, errors)

    presentation = manifest.get("source_presentation")
    ppt_members: dict[str, bytes] = {}
    if not isinstance(presentation, dict):
        _add_error(errors, "source_presentation", "missing_object")
    else:
        ppt_path = Path(str(presentation.get("pptx_path", "")))
        if not ppt_path.is_file():
            _add_error(errors, "source_presentation.pptx_path", "pptx_missing")
        else:
            if _sha256_file(ppt_path) != presentation.get("pptx_sha256"):
                _add_error(errors, "source_presentation.pptx_sha256", "pptx_hash_mismatch")
            try:
                with zipfile.ZipFile(ppt_path) as archive:
                    for name in archive.namelist():
                        if name.startswith("ppt/media/"):
                            ppt_members[name] = archive.read(name)
            except zipfile.BadZipFile:
                _add_error(errors, "source_presentation.pptx_path", "pptx_is_not_a_zip_archive")

    targets, ready_blockers = _validate_targets(manifest, ppt_members, errors)
    excluded = manifest.get("excluded_identities")
    excluded_ids = [item.get("id") for item in excluded if isinstance(item, dict)] if isinstance(excluded, list) else []
    for required in sorted(FORBIDDEN_IDENTITIES):
        if required not in excluded_ids:
            _add_error(errors, "excluded_identities", f"missing_required_exclusion_{required}")
    production_ready = not ready_blockers and all(
        target.get("production_state") == "reviewed_ready_for_import"
        for target in targets
        if isinstance(target, dict)
    )
    return {
        "ok": not errors,
        "manifest": str(manifest_path),
        "production_target_count": len(targets),
        "target_ids": [target.get("id") for target in targets if isinstance(target, dict)],
        "excluded_ids": excluded_ids,
        "verified_ppt_media_count": len(ppt_members),
        "production_ready": production_ready,
        "ready_blocker_count": len(ready_blockers),
        "ready_blockers": ready_blockers,
        "planned_generation_ids": [
            target.get("id") for target in targets
            if isinstance(target, dict) and target.get("production_state") == "planned_generation_not_started"
        ],
        "errors": errors,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--require-ready", action="store_true", help="Return non-zero until all twelve reviewed atlases exist.")
    parser.add_argument("--json", action="store_true", help="Kept for scripts; output is always JSON and the command is read-only.")
    args = parser.parse_args()

    report = validate_manifest(args.manifest)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report.get("ok") and (not args.require_ready or report.get("production_ready")) else 1


if __name__ == "__main__":
    sys.exit(main())
