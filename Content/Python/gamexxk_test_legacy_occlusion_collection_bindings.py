"""Regression check for the legacy Cutout material MPC bindings.

The runtime falls back to UE's default grey material when a collection node
has a missing ParameterId.  Keep this check editor-only and read-only so the
generated material family cannot silently regress to invalid bindings.
"""

from __future__ import annotations

import json

import unreal


MPC_OBJECT_PATH = (
    "/Game/GameXXK/Materials/Occlusion/MPC_PlayerOcclusion.MPC_PlayerOcclusion"
)
ROOTS = (
    "/Game/GameXXK/Materials/Occlusion/AsianVillage/M_concrete_Cutout.M_concrete_Cutout",
    "/Game/GameXXK/Materials/Occlusion/AsianVillage/M_building_wood_Cutout.M_building_wood_Cutout",
)


def _collection_parameter_names(collection):
    values = set()
    for property_name in ("scalar_parameters", "vector_parameters"):
        for parameter in collection.get_editor_property(property_name):
            values.add(str(parameter.get_editor_property("parameter_name")))
    return values


def main():
    collection = unreal.load_asset(MPC_OBJECT_PATH)
    if collection is None:
        raise RuntimeError(f"missing MPC: {MPC_OBJECT_PATH}")
    valid_names = _collection_parameter_names(collection)
    errors = []
    checked = 0

    for path in ROOTS:
        material = unreal.load_asset(path)
        if material is None:
            errors.append(f"missing material: {path}")
            continue
        nodes = [
            node
            for node in unreal.MaterialEditingLibrary.get_material_expressions(material)
            if isinstance(node, unreal.MaterialExpressionCollectionParameter)
        ]
        if not nodes:
            errors.append(f"no collection nodes: {path}")
            continue
        for node in nodes:
            checked += 1
            name = str(node.get_editor_property("parameter_name"))
            if name not in valid_names:
                errors.append(f"invalid MPC parameter name: {path}:{name}")

    report = {"checked": checked, "errors": errors}
    print(json.dumps(report, ensure_ascii=False, sort_keys=True))
    if errors:
        raise RuntimeError("legacy Cutout MPC binding regression: " + " | ".join(errors))
    return report


if __name__ == "__main__":
    main()
