from __future__ import annotations

import json

import unreal


NPC_NATIVE_CLASS = "/Script/GameXXK.GameXXKTownNpcCharacter"
BLUEPRINTS = [
    "/Game/GameXXK/Characters/Merchant/BP_MerchantCharacter",
    "/Game/GameXXK/Characters/Follower/BP_NpcCharacter",
]


def _identity(value) -> str:
    if value is None:
        return ""
    for name in ("get_path_name", "get_name"):
        method = getattr(value, name, None)
        if method:
            try:
                return str(method())
            except Exception:
                pass
    return str(value)


def _call(value, names):
    for name in names:
        method = getattr(value, name, None)
        if not method:
            continue
        try:
            return method()
        except Exception:
            pass
    return None


def _prop(value, name):
    try:
        return value.get_editor_property(name)
    except Exception:
        return None


def _is_child_of(candidate, parent) -> str:
    if candidate is None or parent is None:
        return "missing"
    method = getattr(candidate, "is_child_of", None)
    if method:
        try:
            return str(bool(method(parent)))
        except Exception as exc:
            return f"is_child_of_error:{exc}"
    return "no_is_child_of"


def main() -> None:
    native_class = unreal.load_class(None, NPC_NATIVE_CLASS)
    report = {
        "native_class": _identity(native_class),
        "blueprint_editor_library_parent_methods": [
            name for name in dir(getattr(unreal, "BlueprintEditorLibrary", object)) if "parent" in name.lower()
        ],
        "class_methods": [],
        "blueprints": [],
    }
    for blueprint_path in BLUEPRINTS:
        class_path = f"{blueprint_path}.{blueprint_path.rsplit('/', 1)[-1]}_C"
        asset = unreal.EditorAssetLibrary.load_asset(blueprint_path)
        blueprint_parent = None
        if asset and hasattr(unreal, "BlueprintEditorLibrary"):
            try:
                blueprint_parent = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(asset)
            except Exception:
                blueprint_parent = None
        generated = _prop(asset, "generated_class") if asset else None
        loaded_class = unreal.load_class(None, class_path)
        if loaded_class and not report["class_methods"]:
            report["class_methods"] = [
                name for name in dir(loaded_class)
                if any(token in name.lower() for token in ("child", "parent", "super", "class"))
            ][:80]
        report["blueprints"].append({
            "asset": blueprint_path,
            "asset_class": _identity(asset.get_class()) if asset else "",
            "blueprint_parent_class": _identity(blueprint_parent),
            "asset_parent_class": _identity(_prop(asset, "parent_class")) if asset else "",
            "generated_class": _identity(generated),
            "generated_super_class": _identity(_call(generated, ["get_super_class", "get_super_struct"])),
            "generated_is_npc_child": _is_child_of(generated, native_class),
            "loaded_class": _identity(loaded_class),
            "loaded_super_class": _identity(_call(loaded_class, ["get_super_class", "get_super_struct"])),
            "loaded_is_npc_child": _is_child_of(loaded_class, native_class),
        })
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
