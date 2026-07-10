from __future__ import annotations

import argparse
import json
from pathlib import Path

import unreal


ASSET_ROOT = "/Game/GameXXK/Environment/TownPCG/Prototype/QingshanShopA"
MESH_NAME = "SM_Qingshan_Shop_A_HQ_Retop50K"
TEXTURE_NAME = "T_Qingshan_Shop_A_Albedo"
MATERIAL_NAME = "M_Qingshan_Building_Toon"
TOON_PROFILE_NAME = "TP_Qingshan_Building_Toon"
IMPORT_UNIFORM_SCALE = 6.5

MESH_PATH = f"{ASSET_ROOT}/{MESH_NAME}"
TEXTURE_PATH = f"{ASSET_ROOT}/{TEXTURE_NAME}"
MATERIAL_PATH = f"{ASSET_ROOT}/{MATERIAL_NAME}"
TOON_PROFILE_PATH = f"{ASSET_ROOT}/{TOON_PROFILE_NAME}"


def _parse_args(argv=None):
    parser = argparse.ArgumentParser(description="Import the Tripo Qingshan shop and build a UE 5.8 Toon BSDF material")
    parser.add_argument("--fbx", required=True)
    parser.add_argument("--albedo", required=True)
    return parser.parse_args(argv)


def _require_sources(args):
    fbx = Path(args.fbx).expanduser().resolve()
    albedo = Path(args.albedo).expanduser().resolve()
    if not fbx.is_file():
        raise RuntimeError(f"Missing FBX source: {fbx}")
    if not albedo.is_file():
        raise RuntimeError(f"Missing albedo source: {albedo}")
    return fbx, albedo


def _ensure_asset_root():
    if not unreal.EditorAssetLibrary.does_directory_exist(ASSET_ROOT):
        unreal.EditorAssetLibrary.make_directory(ASSET_ROOT)


def _load_asset(asset_path):
    return unreal.EditorAssetLibrary.load_asset(asset_path)


def _create_or_load(asset_name, asset_class, factory):
    asset_path = f"{ASSET_ROOT}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        asset = _load_asset(asset_path)
        if asset is None:
            raise RuntimeError(f"Could not load existing asset: {asset_path}")
        return asset
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(asset_name, ASSET_ROOT, asset_class, factory)
    if asset is None:
        raise RuntimeError(f"Could not create asset: {asset_path}")
    return asset


def _run_import_task(source_file, destination_name, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source_file))
    task.set_editor_property("destination_path", ASSET_ROOT)
    task.set_editor_property("destination_name", destination_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", False)
    if options is not None:
        task.set_editor_property("options", options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    imported = _load_asset(f"{ASSET_ROOT}/{destination_name}")
    if imported is None:
        raise RuntimeError(f"Import did not create {ASSET_ROOT}/{destination_name}")
    return imported


def _import_texture(albedo_file):
    texture = _run_import_task(albedo_file, TEXTURE_NAME)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("filter", unreal.TextureFilter.TF_TRILINEAR)
    texture.modify()
    return texture


def _import_static_mesh(fbx_file):
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    static_options = options.get_editor_property("static_mesh_import_data")
    static_options.set_editor_property("combine_meshes", True)
    static_options.set_editor_property("import_uniform_scale", IMPORT_UNIFORM_SCALE)
    return _run_import_task(fbx_file, MESH_NAME, options)


def _constant(material, value, x, y):
    expression = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionConstant,
        x,
        y,
    )
    expression.set_editor_property("r", float(value))
    return expression


def _connect_with_candidates(source, source_output, destination, input_candidates):
    for input_name in input_candidates:
        if unreal.MaterialEditingLibrary.connect_material_expressions(
            source,
            source_output,
            destination,
            input_name,
        ):
            return input_name
    raise RuntimeError(f"Could not connect material input candidates: {input_candidates}")


def _build_toon_material(texture):
    toon_profile = _create_or_load(TOON_PROFILE_NAME, unreal.ToonProfile, unreal.ToonProfileFactory())
    material = _create_or_load(MATERIAL_NAME, unreal.Material, unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

    texture_node = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionTextureSampleParameter2D,
        -650,
        -100,
    )
    texture_node.set_editor_property("texture", texture)
    texture_node.set_editor_property("parameter_name", "BuildingAlbedo")

    metallic = _constant(material, 0.0, -650, 100)
    specular = _constant(material, 0.15, -650, 220)
    roughness = _constant(material, 0.85, -650, 340)

    toon = unreal.MaterialEditingLibrary.create_material_expression(
        material,
        unreal.MaterialExpressionSubstrateToonBSDF,
        -150,
        0,
    )
    toon.set_editor_property("toon_profile", toon_profile)

    _connect_with_candidates(texture_node, "RGB", toon, ("Base Color", "BaseColor"))
    _connect_with_candidates(metallic, "", toon, ("Metallic",))
    _connect_with_candidates(specular, "", toon, ("Specular",))
    _connect_with_candidates(roughness, "", toon, ("Roughness",))
    if not unreal.MaterialEditingLibrary.connect_material_property(
        toon,
        "",
        unreal.MaterialProperty.MP_FRONT_MATERIAL,
    ):
        raise RuntimeError("Could not connect Substrate Toon BSDF to Front Material")

    unreal.MaterialEditingLibrary.layout_material_expressions(material)
    unreal.MaterialEditingLibrary.recompile_material(material)
    material.modify()
    toon_profile.modify()
    return toon_profile, material


def _configure_static_mesh(static_mesh, material):
    static_mesh.set_material(0, material)
    try:
        nanite_settings = static_mesh.get_editor_property("nanite_settings")
        nanite_settings.set_editor_property("enabled", True)
        static_mesh.set_editor_property("nanite_settings", nanite_settings)
    except Exception as exc:
        unreal.log_warning(f"QingshanShop: Nanite configuration skipped: {exc}")
    static_mesh.modify()


def _ensure_simple_collision(static_mesh):
    existing_count = int(unreal.EditorStaticMeshLibrary.get_simple_collision_count(static_mesh))
    if existing_count == 0:
        unreal.EditorStaticMeshLibrary.add_simple_collisions(
            static_mesh,
            unreal.ScriptingCollisionShapeType.BOX,
        )
        static_mesh.modify()
    final_count = int(unreal.EditorStaticMeshLibrary.get_simple_collision_count(static_mesh))
    if final_count < 1:
        raise RuntimeError("Qingshan shop mesh must have at least one simple collision shape")
    return final_count


def _save_assets(assets):
    for asset in assets:
        asset_path = unreal.EditorAssetLibrary.get_path_name_for_loaded_asset(asset)
        if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
            raise RuntimeError(f"Failed to save asset: {asset_path}")


def _bounds_payload(static_mesh):
    bounds = static_mesh.get_bounds()
    extent = bounds.box_extent
    return {
        "extent": [float(extent.x), float(extent.y), float(extent.z)],
        "sphere_radius": float(bounds.sphere_radius),
    }


def run(argv=None):
    args = _parse_args(argv)
    fbx_file, albedo_file = _require_sources(args)
    _ensure_asset_root()
    texture = _import_texture(albedo_file)
    static_mesh = _import_static_mesh(fbx_file)
    toon_profile, material = _build_toon_material(texture)
    _configure_static_mesh(static_mesh, material)
    simple_collision_count = _ensure_simple_collision(static_mesh)
    _save_assets((texture, toon_profile, material, static_mesh))

    asset_editor = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
    if asset_editor is not None:
        asset_editor.open_editor_for_assets([static_mesh])

    result = {
        "success": True,
        "asset_root": ASSET_ROOT,
        "mesh": MESH_PATH,
        "texture": TEXTURE_PATH,
        "material": MATERIAL_PATH,
        "toon_profile": TOON_PROFILE_PATH,
        "material_class": material.get_class().get_name(),
        "source_topology": "Quad",
        "source_face_count": 51110,
        "material_slots": len(static_mesh.get_editor_property("static_materials")),
        "simple_collision_count": simple_collision_count,
        "bounds": _bounds_payload(static_mesh),
    }
    try:
        result["nanite_enabled"] = bool(static_mesh.get_editor_property("nanite_settings").enabled)
    except Exception:
        result["nanite_enabled"] = None
    print(json.dumps(result, ensure_ascii=False, sort_keys=True))
    return result


if __name__ == "__main__":
    run()
