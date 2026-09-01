from __future__ import annotations

import json

import unreal


PAPERZD_DIR = "/Game/GameXXK/Characters/Hero/PaperZD"
PAPERZD_SOURCE = f"{PAPERZD_DIR}/AS_Hero_Flipbook"
PAPERZD_ANIM_BP = f"{PAPERZD_DIR}/ABP_Hero_PaperZD"
PAPERZD_CLIPS = [
    (
        f"{PAPERZD_DIR}/PZD_Hero_Town_Idle",
        "/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_Idle_Left.FB_Hero_Town_Idle_Left",
        "town idle",
    ),
    (
        f"{PAPERZD_DIR}/PZD_Hero_Town_WalkStart",
        "/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_WalkStart_Left.FB_Hero_Town_WalkStart_Left",
        "town walk-start",
    ),
    (
        f"{PAPERZD_DIR}/PZD_Hero_Town_WalkLoop",
        "/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_WalkLoop_Left.FB_Hero_Town_WalkLoop_Left",
        "town walk-loop",
    ),
    (
        f"{PAPERZD_DIR}/PZD_Hero_Town_WalkStop",
        "/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_WalkStop_Left.FB_Hero_Town_WalkStop_Left",
        "town walk-stop",
    ),
    (
        f"{PAPERZD_DIR}/PZD_Hero_Town_DeepBreath",
        "/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_DeepBreath_Left.FB_Hero_Town_DeepBreath_Left",
        "town deep-breath",
    ),
    (
        f"{PAPERZD_DIR}/PZD_Hero_Town_AdjustBackpack",
        "/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_AdjustBackpack_Left.FB_Hero_Town_AdjustBackpack_Left",
        "town adjust-backpack",
    ),
    (
        f"{PAPERZD_DIR}/PZD_Hero_Town_CollectItem",
        "/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_CollectItem_Left.FB_Hero_Town_CollectItem_Left",
        "town collect-item",
    ),
    (
        f"{PAPERZD_DIR}/PZD_Hero_Town_CombatIdle",
        "/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_CombatIdle_Left.FB_Hero_Town_CombatIdle_Left",
        "town combat-idle",
    ),
    (
        f"{PAPERZD_DIR}/PZD_Hero_Town_Punch",
        "/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_Punch_Left.FB_Hero_Town_Punch_Left",
        "town punch",
    ),
    (
        f"{PAPERZD_DIR}/PZD_Hero_Town_Kick",
        "/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_Kick_Left.FB_Hero_Town_Kick_Left",
        "town kick",
    ),
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


def _validate_sequence(sequence_path: str, expected_flipbook: str, label: str) -> list[dict]:
    checks = []
    sequence = _load_asset(sequence_path)
    checks.append(_result(
        sequence is not None and _class_name(sequence) == "PaperZDAnimSequence_Flipbook",
        f"paperzd {label} sequence exists",
        path=sequence_path,
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
        f"paperzd {label} sequence is bound to hero source",
        actual=_object_path(anim_source),
        expected=f"{PAPERZD_SOURCE}.AS_Hero_Flipbook",
    ))
    checks.append(_result(
        directional is False,
        f"paperzd {label} sequence is non-directional",
        actual=directional,
        expected=False,
    ))
    checks.append(_result(
        str(category) == "Locomotion",
        f"paperzd {label} sequence category is locomotion",
        actual=str(category),
        expected="Locomotion",
    ))

    actual_flipbooks = []
    if not (isinstance(anim_data, dict) and "__error__" in anim_data):
        for entry in anim_data:
            animation = _get_prop(entry, "animation")
            actual_flipbooks.append(_object_path(animation))

    checks.append(_result(
        actual_flipbooks == [expected_flipbook],
        f"paperzd {label} sequence references exactly one authored left-facing flipbook",
        actual=actual_flipbooks,
        expected=[expected_flipbook],
    ))
    return checks


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

    for sequence_path, expected_flipbook, label in PAPERZD_CLIPS:
        checks.extend(_validate_sequence(sequence_path, expected_flipbook, label))

    return {
        "ok": all(check["ok"] for check in checks),
        "paperzd_dir": PAPERZD_DIR,
        "direction_policy": "one left-facing source; runtime mirrors right",
        "clip_count": len(PAPERZD_CLIPS),
        "checks": checks,
    }


def main() -> None:
    print(json.dumps(validate(), ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
