"""Read-only output-structure probe for project-owned cutout material roots."""

import json
import unreal


PATHS = (
    "/Game/GameXXK/Materials/Occlusion/AsianVillage/M_thatched_roof_Cutout",
    "/Game/GameXXK/Materials/Occlusion/AsianVillage/M_building_wood_Cutout",
    "/Game/GameXXK/Materials/Occlusion/AsianVillage/M_concrete_Cutout",
    "/Game/GameXXK/Materials/Occlusion/AsianVillage/MI_building_wood_06_Inst_Cutout",
    "/Game/GameXXK/Materials/Occlusion/AsianVillage/MI_wooden_board_03_Inst_Cutout",
    "/Game/GameXXK/Materials/Occlusion/AsianVillage/MI_bamboo_wall_01_Inst_Cutout",
    "/Game/GameXXK/Materials/Occlusion/AsianVillage/MI_building_wood_01_Inst_Cutout",
)


def main():
    rows = []
    for path in PATHS:
        asset_name = path.rsplit("/", 1)[-1]
        material = unreal.load_asset(f"{path}.{asset_name}")
        if material is None:
            rows.append({"path": path, "missing": True})
            continue
        opacity_node = None
        attributes_node = None
        if isinstance(material, unreal.Material):
            opacity_node = unreal.MaterialEditingLibrary.get_material_property_input_node(
                material, unreal.MaterialProperty.MP_OPACITY_MASK
            )
            attributes_node = unreal.MaterialEditingLibrary.get_material_property_input_node(
                material, unreal.MaterialProperty.MP_MATERIAL_ATTRIBUTES
            )
        rows.append({
            "path": path,
            "blend": str(material.get_blend_mode()),
            "raw_blend": str(material.get_editor_property("blend_mode")) if isinstance(material, unreal.Material) else "",
            "use_material_attributes": bool(material.get_editor_property("use_material_attributes")) if isinstance(material, unreal.Material) else None,
            "opacity_node": "" if opacity_node is None else opacity_node.get_class().get_name(),
            "attributes_node": "" if attributes_node is None else attributes_node.get_class().get_name(),
            "base_overrides": str(material.get_editor_property("base_property_overrides")) if isinstance(material, unreal.MaterialInstanceConstant) else "",
        })
    report = {"materials": rows}
    print(json.dumps(report, ensure_ascii=False))
    return report


if __name__ == "__main__":
    main()
