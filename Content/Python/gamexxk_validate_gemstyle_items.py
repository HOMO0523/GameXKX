"""Read back the saved icon assets; use after a cold editor restart for reload evidence."""
import argparse
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--group", choices=["relic", "hunt"], required=True)
    args = parser.parse_args()
    folder = ROOT / ("SourceArt/UI/Relics/gemstyle-20260910" if args.group == "relic" else "SourceArt/UI/Hunt/gemstyle-20260910")
    manifest_path = folder / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    verified, errors = [], []
    for record in manifest["icons"]:
        for path in record.get("assetPaths", []):
            texture = unreal.EditorAssetLibrary.load_asset(path)
            if not isinstance(texture, unreal.Texture2D): errors.append(path + ": missing Texture2D"); continue
            # A freshly loaded texture may expose the asynchronous placeholder size.
            audit = json.loads(unreal.GameXXKTextureAuditLibrary.inspect_texture(texture, True))
            settings = {
                "compression_settings": unreal.TextureCompressionSettings.TC_BC7,
                "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
                "filter": unreal.TextureFilter.TF_BILINEAR,
                "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
                "compression_no_alpha": False,
            }
            bad = [name for name, value in settings.items() if texture.get_editor_property(name) != value]
            if [int(texture.blueprint_get_size_x()), int(texture.blueprint_get_size_y())] != [512, 512]: bad.append("size")
            if [audit.get("source_width"), audit.get("source_height")] != [512, 512]: bad.append("sourceSize")
            files = texture.get_editor_property("asset_import_data").extract_filenames()
            if not files or Path(files[0]).resolve() != (ROOT / record["icon"]).resolve(): bad.append("source")
            if bad: errors.append(path + ": " + ",".join(bad))
            else: verified.append(path)
    expected = sum(len(record.get("assetPaths", [])) for record in manifest["icons"])
    if expected == 0: errors.append("No imported asset mappings")
    report = {"group": args.group, "expected": expected, "verified": len(verified), "errors": errors, "ok": expected == len(verified) and not errors}
    output = ROOT / "Saved/Codex/UIGuidanceLocalization-20260910" / (args.group + "-icon-reload.json")
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))
    if errors: raise RuntimeError("Saved icon validation failed")

if __name__ == "__main__": main()
