"""Import the route-map artwork manifest and the user's original circle atlas."""
from pathlib import Path
import json
import shutil
import unreal

from gamexxk_texture_budget import save_loaded_asset

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / "SourceArt/UI/RouteMap"
DESTINATION = "/Game/GameXXK/UI/RouteMap"


def main(manifest_path=None, report_path=None):
    manifest = json.loads((manifest_path or ART / "manifest.json").read_text(encoding="utf-8"))
    imported = []
    for entry in manifest["textures"]:
        source = ROOT / entry["source"]
        if not source.is_file():
            raise RuntimeError(f"missing route artwork: {source}")
        task = unreal.AssetImportTask()
        destination = entry.get("destination", DESTINATION)
        existing_package = ROOT / "Content" / destination.removeprefix("/Game/") / (entry["name"] + ".uasset")
        backup = ROOT / "Saved/RouteNodeArt/before" / existing_package.name
        if existing_package.is_file() and not backup.exists():
            shutil.copy2(existing_package, backup)
        task.filename = str(source)
        task.destination_path = destination
        task.destination_name = entry["name"]
        task.automated = True
        task.replace_existing = True
        task.replace_existing_settings = True
        task.save = False
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.EditorAssetLibrary.load_asset(f"{destination}/{entry['name']}")
        if not isinstance(texture, unreal.Texture2D):
            raise RuntimeError(f"route texture import failed: {entry['name']}")
        for key, value in {
            "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
            "compression_settings": unreal.TextureCompressionSettings.TC_BC7,
            "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
            "filter": unreal.TextureFilter.TF_BILINEAR,
            "address_x": unreal.TextureAddress.TA_CLAMP,
            "address_y": unreal.TextureAddress.TA_CLAMP,
            "srgb": True,
            "never_stream": True,
            "compression_no_alpha": not entry["alpha"],
        }.items():
            texture.set_editor_property(key, value)
        if entry["source_size"] != entry["size"]:
            texture.set_editor_property("power_of_two_mode", unreal.TexturePowerOfTwoSetting.RESIZE_TO_SPECIFIC_RESOLUTION)
            texture.set_editor_property("resize_during_build_x", entry["size"][0])
            texture.set_editor_property("resize_during_build_y", entry["size"][1])
        if not save_loaded_asset(texture, unreal_module=unreal):
            raise RuntimeError(f"route texture save failed: {entry['name']}")
        audit = json.loads(unreal.GameXXKTextureAuditLibrary.inspect_texture(texture, False))
        source_size = [audit["source_width"], audit["source_height"]]
        if source_size != entry["source_size"]:
            raise RuntimeError(f"route source pixels changed: {entry['name']}: {source_size}")
        if [audit["width"], audit["height"]] != entry["size"] or audit["format"] != "BC7":
            raise RuntimeError(f"route texture build mismatch: {entry['name']}: {audit}")
        imported.append({"asset": texture.get_path_name(), "expected_render_size": entry["size"], "audit": audit})
    report = {"ok": True, "imported": imported}
    (report_path or ROOT / "Saved/RouteNodeArt/import-report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
