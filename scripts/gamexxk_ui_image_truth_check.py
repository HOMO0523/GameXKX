"""Read-only validator for the explicit-user-confirmed UI image truth store."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any


PROJECT_ROOT = Path(__file__).resolve().parents[1]
TRUTH_ROOT = PROJECT_ROOT / "SourceArt" / "UI" / "ImageTruth"
CONFIRMED_ROOT = TRUTH_ROOT / "confirmed"
MANIFEST_PATH = TRUTH_ROOT / "manifest.json"
IMAGE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".webp", ".tif", ".tiff"}
REQUIRED_FIELDS = {
    "id",
    "path",
    "sha256",
    "width",
    "height",
    "hasAlpha",
    "approvedBy",
    "approvedAt",
    "approvalReference",
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def image_metadata(path: Path) -> tuple[int, int, bool]:
    try:
        from PIL import Image
    except ImportError as exc:  # pragma: no cover - only relevant after images are approved
        raise RuntimeError("Pillow is required to validate confirmed image metadata.") from exc

    with Image.open(path) as image:
        has_alpha = "A" in image.getbands() or "transparency" in image.info
        return image.width, image.height, has_alpha


def validate() -> dict[str, Any]:
    findings: list[str] = []
    if not MANIFEST_PATH.is_file():
        return {"ok": False, "confirmedCount": 0, "findings": ["manifest.json is missing"]}

    payload = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    if payload.get("schemaVersion") != 1:
        findings.append("schemaVersion must be 1")
    if payload.get("authority") != "explicit-user-confirmation-only":
        findings.append("authority must remain explicit-user-confirmation-only")
    if payload.get("confirmedRoot") != "SourceArt/UI/ImageTruth/confirmed":
        findings.append("confirmedRoot does not match the canonical truth directory")

    entries = payload.get("images")
    if not isinstance(entries, list):
        findings.append("images must be an array")
        entries = []

    actual_images = {
        path.relative_to(PROJECT_ROOT).as_posix(): path
        for path in CONFIRMED_ROOT.rglob("*")
        if path.is_file() and path.suffix.lower() in IMAGE_EXTENSIONS
    }
    unexpected_non_images = [
        path.relative_to(PROJECT_ROOT).as_posix()
        for path in CONFIRMED_ROOT.rglob("*")
        if path.is_file() and path.name != ".gitkeep" and path.suffix.lower() not in IMAGE_EXTENSIONS
    ]
    findings.extend(f"non-image file is not allowed in confirmed/: {path}" for path in unexpected_non_images)

    manifest_paths: set[str] = set()
    semantic_ids: set[str] = set()
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            findings.append(f"images[{index}] must be an object")
            continue
        missing = sorted(REQUIRED_FIELDS - set(entry))
        if missing:
            findings.append(f"images[{index}] is missing fields: {', '.join(missing)}")
            continue
        semantic_id = str(entry["id"])
        relative_path = str(entry["path"]).replace("\\", "/")
        if semantic_id in semantic_ids:
            findings.append(f"duplicate semantic id: {semantic_id}")
        semantic_ids.add(semantic_id)
        if relative_path in manifest_paths:
            findings.append(f"duplicate manifest path: {relative_path}")
        manifest_paths.add(relative_path)
        if not relative_path.startswith("SourceArt/UI/ImageTruth/confirmed/"):
            findings.append(f"manifest image is outside confirmed/: {relative_path}")
            continue
        resolved = (PROJECT_ROOT / relative_path).resolve()
        if CONFIRMED_ROOT.resolve() not in resolved.parents:
            findings.append(f"manifest path escapes confirmed/: {relative_path}")
            continue
        if not resolved.is_file():
            findings.append(f"confirmed image is missing: {relative_path}")
            continue
        if str(entry["approvedBy"]).lower() != "user":
            findings.append(f"approvedBy must be user: {relative_path}")
        if not str(entry["approvedAt"]).strip() or not str(entry["approvalReference"]).strip():
            findings.append(f"approval evidence is empty: {relative_path}")
        actual_hash = sha256_file(resolved)
        if str(entry["sha256"]).upper() != actual_hash:
            findings.append(f"SHA256 mismatch: {relative_path}")
        width, height, has_alpha = image_metadata(resolved)
        if int(entry["width"]) != width or int(entry["height"]) != height:
            findings.append(f"dimension mismatch: {relative_path}")
        if bool(entry["hasAlpha"]) != has_alpha:
            findings.append(f"alpha metadata mismatch: {relative_path}")

    actual_paths = set(actual_images)
    for path in sorted(actual_paths - manifest_paths):
        findings.append(f"unregistered image exists in confirmed/: {path}")
    for path in sorted(manifest_paths - actual_paths):
        if (PROJECT_ROOT / path).suffix.lower() in IMAGE_EXTENSIONS:
            findings.append(f"manifest references a missing confirmed image: {path}")

    return {
        "ok": not findings,
        "confirmedCount": len(actual_images),
        "manifestCount": len(entries),
        "findings": findings,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", help="Emit machine-readable JSON")
    args = parser.parse_args()
    report = validate()
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    else:
        print("PASS" if report["ok"] else "FAIL")
        for finding in report["findings"]:
            print(f"- {finding}")
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
