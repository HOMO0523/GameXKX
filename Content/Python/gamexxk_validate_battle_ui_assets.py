from __future__ import annotations

import json

import unreal

TEXTURE_PATHS = [
    "/Game/GameXXK/UI/Battle/Textures/T_BattleTargetArrowHead",
] + [
    f"/Game/GameXXK/UI/Battle/Textures/T_BattleTargetInkDab_{index:02d}"
    for index in range(12)
]


def main() -> None:
    missing: list[str] = []
    wrong_class: list[str] = []
    for path in TEXTURE_PATHS:
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not asset:
            missing.append(path)
        elif not isinstance(asset, unreal.Texture2D):
            wrong_class.append(path)

    result = {
        "ok": not missing and not wrong_class,
        "missing": missing,
        "wrong_class": wrong_class,
        "validated_count": len(TEXTURE_PATHS) - len(missing) - len(wrong_class),
        "expected_count": len(TEXTURE_PATHS),
    }
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
