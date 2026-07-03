from __future__ import annotations

import json

import unreal


BLUEPRINT_PACKAGE_PATH = "/Game/GameXXK/Characters/Hero"
BLUEPRINT_NAME = "BP_HeroCharacter"
BLUEPRINT_ASSET_PATH = f"{BLUEPRINT_PACKAGE_PATH}/{BLUEPRINT_NAME}"
HERO_NATIVE_CLASS_PATH = "/Script/GameXXK.GameXXKHeroCharacter"
CAMERA_BOOM_NAME = "CameraBoom"
CAMERA_NAME = "TopDownCamera"
LEGACY_CAMERA_BOOM_NAME = "TopDownCameraBoom"


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


def _get_subobject(asset, component_name: str = "", component_class=None):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(asset):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        obj = None
        try:
            obj = library.get_object_for_blueprint(data, asset)
        except Exception:
            try:
                obj = library.get_object(data)
            except Exception:
                obj = None
        if obj is None:
            continue

        variable_name = ""
        try:
            value = library.get_variable_name(data)
            variable_name = "" if value is None else str(value)
        except Exception:
            pass

        matches_name = not component_name or variable_name == component_name or obj.get_name() == component_name
        matches_class = component_class is None or isinstance(obj, component_class)
        if matches_name and matches_class:
            return handle, obj
    return None, None


def _required_subobject_api() -> None:
    if not hasattr(unreal, "SubobjectDataSubsystem") or not hasattr(unreal, "SubobjectDataBlueprintFunctionLibrary"):
        raise RuntimeError("SubobjectDataSubsystem is required to clean old BP_HeroCharacter camera components")


def _remove_legacy_blueprint_camera_components(asset) -> dict:
    _required_subobject_api()
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary

    legacy = []
    seen = set()
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(asset):
        data = subsystem.k2_find_subobject_data_from_handle(handle)
        try:
            obj = library.get_object_for_blueprint(data, asset) or library.get_object(data)
        except Exception:
            obj = None
        if obj is None:
            continue
        try:
            variable_name = str(library.get_variable_name(data))
        except Exception:
            variable_name = ""
        object_name = obj.get_name()
        object_path = obj.get_path_name()
        if variable_name in (LEGACY_CAMERA_BOOM_NAME, CAMERA_NAME) and object_name.endswith("_GEN_VARIABLE"):
            if object_path in seen:
                continue
            seen.add(object_path)
            try:
                parent_handle = library.get_parent_handle(data)
            except Exception:
                parent_handle = None
            legacy.append({
                "handle": handle,
                "parent_handle": parent_handle,
                "variable_name": variable_name,
                "object_name": object_name,
            })

    removed = []
    failed = []
    legacy.sort(key=lambda item: 0 if item["variable_name"] == CAMERA_NAME else 1)
    for item in legacy:
        try:
            ok = bool(item["parent_handle"] and subsystem.delete_subobject(item["parent_handle"], item["handle"], asset) > 0)
        except Exception as exc:
            ok = False
            item["error"] = str(exc)
        if ok:
            removed.append({key: item[key] for key in ("variable_name", "object_name")})
        else:
            failed.append({key: item.get(key, "") for key in ("variable_name", "object_name", "error")})

    if failed:
        raise RuntimeError(f"Failed to remove legacy BP_HeroCharacter camera components: {failed}")

    return {
        "removed_legacy_components": removed,
        "native_camera_boom_name": CAMERA_BOOM_NAME,
        "native_camera_name": CAMERA_NAME,
    }


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

    camera_report = _remove_legacy_blueprint_camera_components(asset)
    _compile_blueprint(asset)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(asset)

    report = {
        "ok": bool(saved),
        "created": created,
        "repaired": repaired,
        "ocean_style_camera": camera_report,
        "asset_path": BLUEPRINT_ASSET_PATH,
        "generated_class": generated_class.get_path_name(),
        "parent_class": parent_class.get_path_name(),
        "saved": bool(saved),
    }
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
