"""Read-only report of nearby static-mesh material slots around the PIE hero."""

from __future__ import annotations

import json
import sys
import unreal


CUTOUTS = {
    "/Game/Asian_Village/materials/building_materials/MI_building_wood_06_Inst.MI_building_wood_06_Inst": "/Game/GameXXK/Materials/Occlusion/AsianVillage/MI_building_wood_06_Inst_Cutout.MI_building_wood_06_Inst_Cutout",
    "/Game/Asian_Village/materials/building_materials/M_thatched_roof.M_thatched_roof": "/Game/GameXXK/Materials/Occlusion/AsianVillage/M_thatched_roof_Cutout.M_thatched_roof_Cutout",
    "/Game/Asian_Village/materials/building_materials/base_materials/MI_wooden_board_03_Inst.MI_wooden_board_03_Inst": "/Game/GameXXK/Materials/Occlusion/AsianVillage/MI_wooden_board_03_Inst_Cutout.MI_wooden_board_03_Inst_Cutout",
}


def main():
    subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = subsystem.get_game_world() if subsystem else None
    if world is None:
        raise RuntimeError("no active PIE world")
    pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
    if pawn is None:
        raise RuntimeError("no PIE pawn")
    origin = pawn.get_actor_location()
    entries = []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        distance = (actor.get_actor_location() - origin).length()
        if distance > 900.0 or actor == pawn:
            continue
        label = str(actor.get_actor_label())
        if "--hide-nearby-wall-actors" in sys.argv[1:] and distance < 700.0 and (
            label.startswith("SM_wall") or label.startswith("SM_thatched_roof_wall")
        ):
            actor.set_actor_hidden_in_game(True)
        components = []
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            if "--apply-all-thatched-cutouts" in sys.argv[1:] and str(actor.get_actor_label()).startswith("SM_thatched_roof"):
                for index in range(component.get_num_materials()):
                    material = component.get_material(index)
                    cutout_path = CUTOUTS.get("" if material is None else str(material.get_path_name()))
                    if cutout_path:
                        component.set_material(index, unreal.load_asset(cutout_path))
            materials = []
            for index in range(component.get_num_materials()):
                material = component.get_material(index)
                materials.append("" if material is None else str(material.get_path_name()))
            mesh = component.get_editor_property("static_mesh")
            components.append({
                "name": str(component.get_name()),
                "mesh": "" if mesh is None else str(mesh.get_path_name()),
                "collision": str(component.get_collision_enabled()),
                "materials": materials,
            })
        if components:
            entries.append({
                "label": str(actor.get_actor_label()),
                "distance": round(float(distance), 2),
                "components": components,
            })
    entries.sort(key=lambda item: item["distance"])
    report = {"pawn": [round(float(v), 2) for v in origin.to_tuple()], "nearby": entries}
    print(json.dumps(report, ensure_ascii=False))
    return report


if __name__ == "__main__":
    main()
