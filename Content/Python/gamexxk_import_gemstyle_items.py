"""Import only the requested relic/hunt item set; preserve old sources and other assets."""
import argparse
import hashlib
import json
import shutil
import struct
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[2]
REPORT_ROOT = ROOT / "Saved/Codex/UIGuidanceLocalization-20260910"


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def import_one(source, destination):
    package, name = destination.rsplit("/", 1)
    file = ROOT / "Content" / (destination.removeprefix("/Game/") + ".uasset")
    backup = REPORT_ROOT / "asset-baseline" / file.relative_to(ROOT)
    if file.exists() and not backup.exists():
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(file, backup)
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = package
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset = unreal.EditorAssetLibrary.load_asset(destination)
    if not isinstance(asset, unreal.Texture2D): raise RuntimeError("Import failed: " + destination)
    settings = {
        "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        "compression_settings": unreal.TextureCompressionSettings.TC_BC7,
        "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
        "filter": unreal.TextureFilter.TF_BILINEAR,
        "address_x": unreal.TextureAddress.TA_CLAMP,
        "address_y": unreal.TextureAddress.TA_CLAMP,
        "srgb": True, "never_stream": True, "compression_no_alpha": False,
        "max_texture_size": 0, "lod_bias": 0,
        "resize_during_build_x": 512, "resize_during_build_y": 512,
    }
    for key, value in settings.items(): asset.set_editor_property(key, value)
    audit = json.loads(unreal.GameXXKTextureAuditLibrary.inspect_texture(asset, True))
    if [int(asset.blueprint_get_size_x()), int(asset.blueprint_get_size_y())] != [512, 512]:
        raise RuntimeError("Imported dimensions differ: " + destination + " " + json.dumps(audit))
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset): raise RuntimeError("Save failed: " + destination)
    wrong = [key for key, value in settings.items() if asset.get_editor_property(key) != value]
    if wrong: raise RuntimeError("Texture settings differ: " + destination + " " + ",".join(wrong))
    filenames = asset.get_editor_property("asset_import_data").extract_filenames()
    if not filenames or Path(filenames[0]).resolve() != source.resolve():
        raise RuntimeError("Unexpected source mapping: " + destination)
    if [audit["source_width"], audit["source_height"]] != [512, 512]:
        raise RuntimeError("Source dimensions differ: " + destination)
    return {"asset": asset.get_path_name(), "source": str(source.relative_to(ROOT)), "audit": audit, "saved": True}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--group", choices=["relic", "hunt"], required=True)
    args = parser.parse_args()
    directory = ROOT / ("SourceArt/UI/Relics/gemstyle-20260910" if args.group == "relic" else "SourceArt/UI/Hunt/gemstyle-20260910")
    manifest_path = directory / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    jobs = json.loads((directory / "art-jobs.json").read_text(encoding="utf-8"))["jobs"]
    by_slug = {entry["slug"]: entry for entry in manifest["icons"]}
    if len(by_slug) != len(jobs) or set(by_slug) != {job["slug"] for job in jobs}:
        raise RuntimeError("The requested icon set is incomplete")
    # Validate every input before importing any package.
    for record in by_slug.values():
        source = (ROOT / record["icon"]).resolve()
        if not source.is_relative_to(ROOT.resolve()): raise RuntimeError("Source left project root")
        raw = source.read_bytes()
        if raw[:8] != b"\x89PNG\r\n\x1a\n" or struct.unpack(">II", raw[16:24]) != (512, 512) or raw[25] != 6:
            raise RuntimeError("Expected 512 RGBA PNG: " + str(source))
        if hashlib.sha256(raw).hexdigest() != record["sha256"]: raise RuntimeError("Source hash drift")
    report = {"group": args.group, "expected": len(jobs), "imported": [], "complete": False, "reloadVerified": False}
    report_path = REPORT_ROOT / (args.group + "-icon-import.json")
    for job in jobs:
        record = by_slug[job["slug"]]
        source = (ROOT / record["icon"]).resolve()
        target = "/Game/GameXXK/UI/Relics/Icons/T_Relic_" + job["slug"] if args.group == "relic" else job["asset"]
        results = [import_one(source, target)]
        if job.get("additionalConsumer"): results.append(import_one(source, job["additionalConsumer"]))
        record["imported"] = True
        record["assetPaths"] = [result["asset"] for result in results]
        report["imported"].append({"slug": job["slug"], "packages": results})
        write_json(report_path, report)
        write_json(manifest_path, manifest)
    report["complete"] = True
    write_json(report_path, report)
    print(json.dumps({"ok": True, "group": args.group, "count": len(jobs), "report": str(report_path)}, ensure_ascii=False))


if __name__ == "__main__": main()
