"""Import the versioned imagegen sprite and author its UI chroma-key material."""
import traceback
try:
    import json
    from pathlib import Path

    import unreal
    from gamexxk_texture_budget import save_loaded_asset

    root = Path(str(unreal.Paths.project_dir())).resolve()
    source = root / "SourceArt/UI/Battle/Targeting/gem-arrow-20260914/T_BattleTargetArrowHead_GemV1_Keyed.png"
    texture_folder = "/Game/GameXXK/UI/Battle/Textures"
    material_folder = "/Game/GameXXK/UI/Battle/Materials"
    texture_name = "T_BattleTargetArrowHead_GemV1"
    material_name = "M_BattleTargetArrowHead_GemV1"
    assert source.is_file()
    for folder in (texture_folder, material_folder):
        unreal.EditorAssetLibrary.make_directory(folder)

    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = texture_folder
    task.destination_name = texture_name
    task.automated = True
    task.replace_existing = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(texture_folder + "/" + texture_name)
    assert isinstance(texture, unreal.Texture2D)
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("never_stream", True)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("power_of_two_mode", unreal.TexturePowerOfTwoSetting.RESIZE_TO_SPECIFIC_RESOLUTION)
    texture.set_editor_property("resize_during_build_x", 512)
    texture.set_editor_property("resize_during_build_y", 512)
    assert save_loaded_asset(texture)

    material_path = material_folder + "/" + material_name
    material = unreal.load_asset(material_path) if unreal.EditorAssetLibrary.does_asset_exist(material_path) else None
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            material_name, material_folder, unreal.Material, unreal.MaterialFactoryNew())
    assert material
    lib = unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(material)
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    def node(cls):
        return lib.create_material_expression(material, cls)

    def connect(source_node, source_pin, target_node, target_pin):
        assert lib.connect_material_expressions(source_node, source_pin, target_node, target_pin)

    def custom(code, output, inputs):
        expression = node(unreal.MaterialExpressionCustom)
        expression.set_editor_property("code", code)
        expression.set_editor_property("output_type", output)
        entries = []
        for name in inputs:
            entry = unreal.CustomInput()
            entry.set_editor_property("input_name", name)
            entries.append(entry)
        expression.set_editor_property("inputs", entries)
        return expression

    sample = node(unreal.MaterialExpressionTextureSampleParameter2D)
    sample.set_editor_property("parameter_name", "ArrowTexture")
    sample.set_editor_property("texture", texture)
    mask = custom("float key=max(0,min(Color.r,Color.b)-Color.g); return 1-smoothstep(0.04,0.60,key);",
                  unreal.CustomMaterialOutputType.CMOT_FLOAT1, ["Color"])
    connect(sample, "RGB", mask, "Color")
    despill = custom("return saturate((Color-float3(1,0,1)*(1-Alpha))/max(Alpha,0.001));",
                     unreal.CustomMaterialOutputType.CMOT_FLOAT3, ["Color", "Alpha"])
    connect(sample, "RGB", despill, "Color")
    connect(mask, "", despill, "Alpha")
    vertex = node(unreal.MaterialExpressionVertexColor)
    tint = node(unreal.MaterialExpressionMultiply)
    connect(despill, "", tint, "A")
    connect(vertex, "", tint, "B")
    opacity = node(unreal.MaterialExpressionMultiply)
    connect(mask, "", opacity, "A")
    connect(vertex, "A", opacity, "B")
    assert lib.connect_material_property(tint, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert lib.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    lib.layout_material_expressions(material)
    lib.recompile_material(material)
    assert save_loaded_asset(material)
    report = {"ok": True, "texture": texture.get_path_name(), "material": material.get_path_name(),
              "source": str(source), "runtime_resolution": [512,512], "opacity": "chroma key in UI material"}
    out = root / "Saved/GemTargetArrow"
    out.mkdir(parents=True, exist_ok=True)
    (out / "import.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))
except Exception as error:
    raise RuntimeError(traceback.format_exc()) from error
