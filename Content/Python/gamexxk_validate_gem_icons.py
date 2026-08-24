"""Read-only validation for the 30 imported gem textures."""

from __future__ import annotations

import json
from pathlib import Path

import unreal


PROJECT_ROOT = Path(__file__).resolve().parents[2]
MANIFEST = PROJECT_ROOT / "SourceArt" / "UI" / "Items" / "Gems" / "gem_icon_manifest.json"


def main() -> None:
    records = json.loads(MANIFEST.read_text(encoding="utf-8")).get("records", [])
    problems: list[dict[str, object]] = []
    validated: list[str] = []
    for record in records:
        object_path = str(record["texture_path"])
        asset = unreal.EditorAssetLibrary.load_asset(object_path)
        if not isinstance(asset, unreal.Texture2D):
            problems.append({"path": object_path, "problem": "missing_or_wrong_class"})
            continue
        size = [int(asset.blueprint_get_size_x()), int(asset.blueprint_get_size_y())]
        if size != [512, 512]:
            problems.append({"path": object_path, "problem": "wrong_size", "actual": size})
            continue
        expected = {
            "mip_gen_settings": unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
            "compression_settings": unreal.TextureCompressionSettings.TC_EDITOR_ICON,
            "lod_group": unreal.TextureGroup.TEXTUREGROUP_UI,
            "filter": unreal.TextureFilter.TF_BILINEAR,
            "address_x": unreal.TextureAddress.TA_CLAMP,
            "address_y": unreal.TextureAddress.TA_CLAMP,
            "srgb": True,
            "never_stream": True,
            "compression_no_alpha": False,
        }
        wrong = {name: str(asset.get_editor_property(name)) for name, value in expected.items() if asset.get_editor_property(name) != value}
        if wrong:
            problems.append({"path": object_path, "problem": "wrong_settings", "actual": wrong})
            continue
        validated.append(object_path)
    result = {
        "ok": len(records) == 30 and len(validated) == 30 and not problems,
        "expected_count": 30,
        "validated_count": len(validated),
        "problems": problems,
    }
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
