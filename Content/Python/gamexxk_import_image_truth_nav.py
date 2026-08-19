"""Import only user-confirmed ImageTruth training assets into UE.

This script deliberately refuses any source outside ImageTruth/confirmed and
verifies manifest SHA256 before creating or replacing a texture asset.
"""

from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

import unreal


PROJECT_ROOT = Path(unreal.Paths.project_dir()).resolve()
MANIFEST_PATH = PROJECT_ROOT / "SourceArt" / "UI" / "ImageTruth" / "manifest.json"
DESTINATION = "/Game/GameXXK/UI/ImageTruth/Training"
IMPORT_MAP = {
    "training.idle_strip.background.seamless.v003": "T_TrainingIdleStrip_Background",
    "training.nav.warehouse.ink.monochrome.v002": "T_TrainingNavWarehouse",
    "training.nav.formation.ink.v002": "T_TrainingNavFormation",
    "training.nav.talents.ink.knot.v004": "T_TrainingNavTalents",
    "training.nav.tools.ink.hammer.v005": "T_TrainingNavTools",
    "training.nav.training.ink.v001": "T_TrainingNavTraining",
}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def run() -> dict[str, object]:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    entries = {entry["id"]: entry for entry in manifest["images"]}
    results: list[dict[str, object]] = []
    for semantic_id, asset_name in IMPORT_MAP.items():
        entry = entries.get(semantic_id)
        if not entry or entry.get("approvedBy") != "user":
            raise RuntimeError(f"ImageTruth entry is not user-confirmed: {semantic_id}")
        relative_path = str(entry["path"]).replace("\\", "/")
        if not relative_path.startswith("SourceArt/UI/ImageTruth/confirmed/"):
            raise RuntimeError(f"Import path is outside confirmed truth: {relative_path}")
        source = (PROJECT_ROOT / relative_path).resolve()
        if not source.is_file():
            raise FileNotFoundError(source)
        actual_hash = sha256_file(source)
        if actual_hash != str(entry["sha256"]).upper():
            raise RuntimeError(f"SHA256 mismatch for {semantic_id}: {actual_hash}")

        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = DESTINATION
        task.destination_name = asset_name
        task.automated = True
        task.replace_existing = True
        task.save = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

        object_path = f"{DESTINATION}/{asset_name}.{asset_name}"
        texture = unreal.load_asset(object_path)
        if not texture:
            raise RuntimeError(f"UE failed to load imported texture: {object_path}")
        try:
            texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        except Exception:
            pass
        try:
            unreal.EditorAssetLibrary.save_loaded_asset(texture)
        except Exception:
            pass
        results.append(
            {
                "id": semantic_id,
                "source": str(source),
                "asset": object_path,
                "sha256": actual_hash,
                "size": [entry["width"], entry["height"]],
            }
        )
    return {"ok": True, "destination": DESTINATION, "assets": results}


if __name__ == "__main__":
    print(json.dumps(run(), ensure_ascii=False, indent=2))
