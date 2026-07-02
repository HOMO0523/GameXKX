from __future__ import annotations

import json

import unreal


BLUEPRINT_PACKAGE_PATH = "/Game/GameXXK/Characters/Hero"
BLUEPRINT_NAME = "BP_HeroCharacter"
BLUEPRINT_ASSET_PATH = f"{BLUEPRINT_PACKAGE_PATH}/{BLUEPRINT_NAME}"
HERO_NATIVE_CLASS_PATH = "/Script/GameXXK.GameXXKHeroCharacter"


def _load_generated_class(path: str):
    generated_path = f"{path}.{BLUEPRINT_NAME}_C"
    return unreal.load_object(None, generated_path)


def _get_generated_class(asset):
    try:
        generated_class = asset.get_editor_property("generated_class")
        if generated_class is not None:
            return generated_class
    except Exception:
        pass

    return _load_generated_class(BLUEPRINT_ASSET_PATH)


def _is_child_of(candidate, parent) -> bool:
    if candidate is None or parent is None:
        return False

    if hasattr(unreal, "MathLibrary"):
        try:
            return bool(unreal.MathLibrary.class_is_child_of(candidate, parent))
        except TypeError:
            pass

    if hasattr(candidate, "is_child_of"):
        try:
            return bool(candidate.is_child_of(parent))
        except TypeError:
            pass

    current = candidate
    while current:
        if current == parent:
            return True
        if hasattr(current, "get_super_struct"):
            current = current.get_super_struct()
        else:
            return False
    return False


def _load_hero_parent_class():
    parent_class = unreal.load_class(None, HERO_NATIVE_CLASS_PATH)
    if parent_class is not None:
        return parent_class

    parent_type = getattr(unreal, "GameXXKHeroCharacter", None)
    if parent_type is None:
        raise RuntimeError("GameXXKHeroCharacter is not loaded; rebuild GameXXKEditor first")
    return parent_type.static_class()


def _compile_blueprint(asset) -> None:
    if hasattr(unreal, "BlueprintEditorLibrary"):
        unreal.BlueprintEditorLibrary.compile_blueprint(asset)
        return
    if hasattr(unreal, "KismetEditorUtilities"):
        unreal.KismetEditorUtilities.compile_blueprint(asset)
        return
    raise RuntimeError("No Blueprint compile API is available in this Unreal Python environment")


def _reparent_blueprint(asset, parent_class) -> None:
    if not hasattr(unreal, "BlueprintEditorLibrary"):
        raise RuntimeError("BlueprintEditorLibrary is required to reparent BP_HeroCharacter")
    unreal.BlueprintEditorLibrary.reparent_blueprint(asset, parent_class)


def _repair_existing_asset(asset, parent_class) -> bool:
    if not isinstance(asset, unreal.Blueprint):
        return False

    _compile_blueprint(asset)
    existing_generated_class = _get_generated_class(asset)
    if existing_generated_class is None or _is_child_of(existing_generated_class, parent_class):
        return False

    _reparent_blueprint(asset, parent_class)
    return True


def main() -> None:
    parent_class = _load_hero_parent_class()

    unreal.EditorAssetLibrary.make_directory(BLUEPRINT_PACKAGE_PATH)
    asset = None
    if unreal.EditorAssetLibrary.does_asset_exist(BLUEPRINT_ASSET_PATH):
        asset = unreal.EditorAssetLibrary.load_asset(BLUEPRINT_ASSET_PATH)

    created = False
    repaired = _repair_existing_asset(asset, parent_class)

    if asset is None:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            BLUEPRINT_NAME,
            BLUEPRINT_PACKAGE_PATH,
            unreal.Blueprint.static_class(),
            factory,
        )
        created = True

    if asset is None:
        raise RuntimeError(f"Failed to create {BLUEPRINT_ASSET_PATH}")
    if not isinstance(asset, unreal.Blueprint):
        raise RuntimeError(f"{BLUEPRINT_ASSET_PATH} exists but is not a Blueprint")

    _reparent_blueprint(asset, parent_class)
    _compile_blueprint(asset)
    generated_class = _get_generated_class(asset)
    if generated_class is None:
        raise RuntimeError(f"Failed to load generated class for {BLUEPRINT_ASSET_PATH}")
    if not _is_child_of(generated_class, parent_class):
        parent_path = parent_class.get_path_name()
        super_path = "<none>"
        if hasattr(generated_class, "get_super_struct") and generated_class.get_super_struct():
            super_path = generated_class.get_super_struct().get_path_name()
        raise RuntimeError(
            f"{BLUEPRINT_ASSET_PATH} is not a child of GameXXKHeroCharacter; "
            f"generated={generated_class.get_path_name()} super={super_path} expected_parent={parent_path}"
        )

    _compile_blueprint(asset)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(asset)

    report = {
        "ok": bool(saved),
        "created": created,
        "repaired": repaired,
        "asset_path": BLUEPRINT_ASSET_PATH,
        "generated_class": generated_class.get_path_name(),
        "parent_class": parent_class.get_path_name(),
        "saved": bool(saved),
    }
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
