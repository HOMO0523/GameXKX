"""Read-only UE validation for the 30 imported route-relic textures."""

from __future__ import annotations

import json

import unreal


DESTINATION = "/Game/GameXXK/UI/Relics/Icons"
SLUGS = (
    "AncientCoin", "JadeBell", "BambooTally", "TigerSeal", "MedicineGourd",
    "InkTalisman", "CloudMirror", "StoneBead", "CraneFeather", "IronKnot",
    "TeaBrick", "Compass", "RedCord", "BronzeNeedle", "RainCape",
    "ChessStone", "DrumCharm", "LotusSeed", "SwordGuard", "OldMap",
    "PineCone", "RiverPearl", "CandleStub", "FoxMask", "StoneLion",
    "WineCup", "HerbBasket", "PaperCrane", "BrokenArrow", "MoonDisc",
)
EXPECTED_SIZE = [512, 512]


def main() -> None:
    missing: list[str] = []
    wrong_class: list[dict[str, str]] = []
    wrong_size: list[dict[str, object]] = []
    validated: list[str] = []
    for slug in SLUGS:
        path = f"{DESTINATION}/T_Relic_{slug}"
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not asset:
            missing.append(path)
            continue
        if not isinstance(asset, unreal.Texture2D):
            wrong_class.append({"path": path, "class": asset.get_class().get_name()})
            continue
        size = [int(asset.blueprint_get_size_x()), int(asset.blueprint_get_size_y())]
        if size != EXPECTED_SIZE:
            wrong_size.append({"path": path, "size": size})
            continue
        validated.append(path)
    result = {
        "ok": not missing and not wrong_class and not wrong_size and len(validated) == len(SLUGS),
        "expected_count": len(SLUGS),
        "validated_count": len(validated),
        "missing": missing,
        "wrong_class": wrong_class,
        "wrong_size": wrong_size,
    }
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
