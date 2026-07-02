from __future__ import annotations

import json

import unreal


PAPERZD_DIR = "/Game/GameXXK/Characters/Hero/PaperZD"
PAPERZD_SOURCE = f"{PAPERZD_DIR}/AS_Hero_Flipbook"
PAPERZD_ANIM_BP = f"{PAPERZD_DIR}/ABP_Hero_PaperZD"
PAPERZD_WALK_SEQUENCE = f"{PAPERZD_DIR}/PZD_Hero_Walk_8Dir"

DIRECTIONS = [
    "South",
    "SouthWest",
    "West",
    "NorthWest",
    "North",
    "NorthEast",
    "East",
    "SouthEast",
]

EXPECTED_FLIPBOOKS = [
    f"/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Walk_{direction}.FB_Hero_Walk_{direction}"
    for direction in DIRECTIONS
]


def _load_asset(path: str):
    return unreal.EditorAssetLibrary.load_asset(path)


def _class_name(asset) -> str:
    if asset is None:
        return ""
    try:
        return asset.get_class().get_name()
    except Exception:
        return ""


def _object_path(asset) -> str:
    if asset is None:
        return ""
    try:
        return asset.get_path_name()
    except Exception:
        return ""


def _get_prop(asset, prop_name: str):
    try:
        return asset.get_editor_property(prop_name)
    except Exception as exc:
        return {"__error__": str(exc)}


def _get_first_prop(asset, prop_names: list[str]):
    errors = {}
    for prop_name in prop_names:
        value = _get_prop(asset, prop_name)
        if not (isinstance(value, dict) and "__error__" in value):
            return value
        errors[prop_name] = value["__error__"]
    return {"__error__": errors}


def _result(ok: bool, name: str, **extra) -> dict:
    entry = {"ok": bool(ok), "name": name}
    entry.update(extra)
    return entry


def validate() -> dict:
    checks = []

    source = _load_asset(PAPERZD_SOURCE)
    checks.append(_result(
        source is not None and _class_name(source) == "PaperZDAnimationSource_Flipbook",
        "paperzd flipbook animation source exists",
        path=PAPERZD_SOURCE,
        actual_class=_class_name(source),
        expected_class="PaperZDAnimationSource_Flipbook",
    ))

    anim_bp = _load_asset(PAPERZD_ANIM_BP)
    supported_source = _get_prop(anim_bp, "supported_animation_source") if anim_bp else None
    checks.append(_result(
        anim_bp is not None and _class_name(anim_bp) == "PaperZDAnimBP",
        "paperzd anim blueprint exists",
        path=PAPERZD_ANIM_BP,
        actual_class=_class_name(anim_bp),
        expected_class="PaperZDAnimBP",
    ))
    checks.append(_result(
        _object_path(supported_source) == f"{PAPERZD_SOURCE}.AS_Hero_Flipbook",
        "paperzd anim blueprint is bound to hero source",
        actual=_object_path(supported_source),
        expected=f"{PAPERZD_SOURCE}.AS_Hero_Flipbook",
    ))

    sequence = _load_asset(PAPERZD_WALK_SEQUENCE)
    checks.append(_result(
        sequence is not None and _class_name(sequence) == "PaperZDAnimSequence_Flipbook",
        "paperzd 8-direction walk sequence exists",
        path=PAPERZD_WALK_SEQUENCE,
        actual_class=_class_name(sequence),
        expected_class="PaperZDAnimSequence_Flipbook",
    ))

    if sequence is not None:
        anim_source = _get_prop(sequence, "anim_source")
        anim_data = _get_prop(sequence, "anim_data")
        directional = _get_first_prop(sequence, ["directional_sequence", "bDirectionalSequence", "b_directional_sequence"])
        category = _get_prop(sequence, "category")
    else:
        anim_source = None
        anim_data = []
        directional = None
        category = None

    checks.append(_result(
        _object_path(anim_source) == f"{PAPERZD_SOURCE}.AS_Hero_Flipbook",
        "paperzd walk sequence is bound to hero source",
        actual=_object_path(anim_source),
        expected=f"{PAPERZD_SOURCE}.AS_Hero_Flipbook",
    ))
    checks.append(_result(
        directional is True,
        "paperzd walk sequence is directional",
        actual=directional,
        expected=True,
    ))
    checks.append(_result(
        str(category) == "Locomotion",
        "paperzd walk sequence category is locomotion",
        actual=str(category),
        expected="Locomotion",
    ))

    actual_flipbooks = []
    if not (isinstance(anim_data, dict) and "__error__" in anim_data):
        for entry in anim_data:
            animation = _get_prop(entry, "animation")
            actual_flipbooks.append(_object_path(animation))

    checks.append(_result(
        actual_flipbooks == EXPECTED_FLIPBOOKS,
        "paperzd walk sequence references all 8 Paper2D flipbooks in direction order",
        actual=actual_flipbooks,
        expected=EXPECTED_FLIPBOOKS,
    ))

    return {
        "ok": all(check["ok"] for check in checks),
        "paperzd_dir": PAPERZD_DIR,
        "checks": checks,
    }


def main() -> None:
    print(json.dumps(validate(), ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
