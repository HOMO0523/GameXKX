#!/usr/bin/env python3
"""List or explicitly extract the six approved task-NPC references from the source PPTX.

The default is a dry run.  ``--write`` only creates new files under
SourceAssets/PartyDeck/character-references; it never edits the source PPTX or
overwrites a non-identical extracted reference.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
import zipfile
from pathlib import Path
from typing import Any


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "character-references" / "character-sheet-manifest.json"
DEFAULT_OUTPUT_DIR = PROJECT_ROOT / "SourceAssets" / "PartyDeck" / "character-references" / "ppt-extract"
APPROVED_NPC_IDS = [
    "Npc.TusiChief",
    "Npc.SongJinBao",
    "Npc.YueBai",
    "Npc.ZhouGuangZu",
    "Npc.JinGui",
    "Npc.QiongMeiEr",
]


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _safe_output_dir(raw_path: Path) -> Path:
    output_dir = raw_path if raw_path.is_absolute() else PROJECT_ROOT / raw_path
    output_dir = output_dir.resolve()
    allowed_root = (PROJECT_ROOT / "SourceAssets" / "PartyDeck").resolve()
    if not output_dir.is_relative_to(allowed_root):
        raise ValueError(f"output directory must remain under {allowed_root}: {output_dir}")
    return output_dir


def _filename_for(target_id: str, media_member: str) -> str:
    identity = target_id.replace("Npc.", "npc_")
    identity = "".join((character.lower() if character.isalnum() else "_") for character in identity)
    identity = identity.replace("__", "_").strip("_")
    extension = Path(media_member).suffix.lower()
    return f"{identity}_{Path(media_member).stem}{extension}"


def _load_references(manifest_path: Path) -> tuple[Path, list[dict[str, str]]]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    presentation = manifest.get("source_presentation")
    if not isinstance(presentation, dict) or not isinstance(presentation.get("pptx_path"), str):
        raise ValueError("manifest has no source_presentation.pptx_path")
    targets = manifest.get("production_targets")
    if not isinstance(targets, list):
        raise ValueError("manifest has no production_targets list")
    by_id = {target.get("id"): target for target in targets if isinstance(target, dict)}
    references: list[dict[str, str]] = []
    for target_id in APPROVED_NPC_IDS:
        target = by_id.get(target_id)
        if not isinstance(target, dict) or target.get("kind") != "task_npc":
            raise ValueError(f"missing approved task NPC target: {target_id}")
        sources = target.get("reference_sources")
        if not isinstance(sources, list):
            raise ValueError(f"missing references for {target_id}")
        ppt_sources = [source for source in sources if isinstance(source, dict) and source.get("kind") == "ppt_embedded_media"]
        if len(ppt_sources) != 1:
            raise ValueError(f"{target_id} must have exactly one PPT media source")
        source = ppt_sources[0]
        member = source.get("member")
        source_hash = source.get("sha256")
        if not isinstance(member, str) or not isinstance(source_hash, str):
            raise ValueError(f"invalid PPT source data for {target_id}")
        references.append({"id": target_id, "member": member, "sha256": source_hash})
    return Path(presentation["pptx_path"]), references


def extract_references(manifest_path: Path, output_dir: Path, write_mode: bool) -> dict[str, Any]:
    pptx_path, references = _load_references(manifest_path)
    if not pptx_path.is_file():
        raise FileNotFoundError(f"source PPTX is missing: {pptx_path}")
    safe_output = _safe_output_dir(output_dir)
    report_references: list[dict[str, str]] = []
    with zipfile.ZipFile(pptx_path) as archive:
        for reference in references:
            member = reference["member"]
            try:
                source_bytes = archive.read(member)
            except KeyError as exc:
                raise FileNotFoundError(f"PPTX has no media member {member}") from exc
            actual_hash = _sha256(source_bytes)
            if actual_hash != reference["sha256"]:
                raise ValueError(f"PPTX media hash mismatch for {reference['id']}: {actual_hash}")
            destination = safe_output / _filename_for(reference["id"], member)
            action = "would_extract"
            if write_mode:
                if destination.exists():
                    existing_hash = _sha256(destination.read_bytes())
                    if existing_hash != actual_hash:
                        raise FileExistsError(f"refusing to overwrite non-identical reference: {destination}")
                    action = "already_verified"
                else:
                    safe_output.mkdir(parents=True, exist_ok=True)
                    destination.write_bytes(source_bytes)
                    action = "extracted"
            report_references.append({
                "id": reference["id"],
                "member": member,
                "sha256": actual_hash,
                "destination": str(destination.relative_to(PROJECT_ROOT)).replace("\\", "/"),
                "action": action,
            })
    return {
        "ok": True,
        "manifest": str(manifest_path),
        "source_pptx": str(pptx_path),
        "write_mode": write_mode,
        "reference_count": len(report_references),
        "references": report_references,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--write", action="store_true", help="Extract verified references; omission performs a dry run.")
    parser.add_argument("--json", action="store_true", help="Kept for scripts; output is always JSON.")
    args = parser.parse_args()
    try:
        report = extract_references(args.manifest, args.output_dir, args.write)
        exit_code = 0
    except Exception as exc:
        report = {"ok": False, "write_mode": bool(args.write), "error": str(exc)}
        exit_code = 1
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
