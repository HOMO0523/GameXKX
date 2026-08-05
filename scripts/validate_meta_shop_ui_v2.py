from __future__ import annotations

import hashlib
import json
from pathlib import Path

from PIL import Image

PROJECT_ROOT = Path(__file__).resolve().parents[1]
CALIBRATION_ROOT = PROJECT_ROOT / "SourceArt/UI/PSD/gamexxk-v4/calibration-v2"
MANIFEST_PATH = CALIBRATION_ROOT / "content-manifest.json"

EXPECTED = {
    "pojun_weapon": ("/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_PoJunPack", (512, 512)),
    "xuanjia_weapon": ("/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_XuanJiaPack", (512, 512)),
    "qingnang_weapon": ("/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_QingNangPack", (512, 512)),
    "zhuifeng_weapon": ("/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_ZhuiFengPack", (512, 512)),
    "shigu_weapon": ("/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_ShiGuPack", (512, 512)),
    "shanhe_weapon": ("/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_ShanHePack", (512, 512)),
    "nav_companion": ("/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_CompanionPack", (256, 256)),
}


def validate_sources() -> list[dict[str, object]]:
    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    indexed = {entry["name"]: entry for entry in manifest["content"]}
    validated: list[dict[str, object]] = []
    for name, (target, expected_size) in EXPECTED.items():
        if name not in indexed:
            raise ValueError(f"approved content manifest is missing {name}")
        entry = indexed[name]
        manifest_size = tuple(entry.get("size", ()))
        if manifest_size != expected_size:
            raise ValueError(f"manifest size mismatch for {name}: {manifest_size}")
        source = (CALIBRATION_ROOT / entry["file"]).resolve()
        if not source.is_relative_to(CALIBRATION_ROOT.resolve()) or not source.is_file():
            raise ValueError(f"approved source is missing or escapes calibration root: {name}")
        with Image.open(source) as image:
            if image.size != expected_size:
                raise ValueError(f"source dimensions mismatch for {name}: {image.size}")
            if "A" not in image.getbands():
                raise ValueError(f"source has no alpha channel: {name}")
            alpha_min, alpha_max = image.getchannel("A").getextrema()
            if alpha_min >= 255 or alpha_max <= 0:
                raise ValueError(f"source alpha is empty or fully opaque: {name}")
        validated.append(
            {
                "name": name,
                "source": str(source),
                "target": target,
                "size": list(expected_size),
                "sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            }
        )
    return validated


def main() -> None:
    validated = validate_sources()
    print(json.dumps({"ok": True, "validated_count": len(validated), "assets": validated}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
