"""Create simplified invisible gameplay collision proxies for the Asian Village town."""

from __future__ import annotations

import json

import unreal


MAP = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"
MANAGED_TAG = "GameXXKGameplayProxy"
CUBE = "/Engine/BasicShapes/Cube.Cube"

# Initial acceptance slice.  Dimensions are authored from measured visual bounds,
# with small margins so the Paper2D capsule cannot enter roofs/walls.
BOXES = [
    {
        "label": "GXXK_Blocker_Building_ThatchedRoof11",
        "kind": "Blocker_Building",
        "center": (16126.2, 7150.6, 1348.0),
        "extent": (310.0, 325.0, 350.0),
    },
]


def _tagged(actor) -> bool:
    return MANAGED_TAG in [str(tag) for tag in actor.tags]


def main() -> None:
    world = unreal.EditorLevelLibrary.get_editor_world()
    if world is None or str(world.get_outermost().get_path_name()).split(".", 1)[0] != MAP:
        raise RuntimeError(f"open {MAP} before building gameplay proxies")

    for actor in list(unreal.EditorLevelLibrary.get_all_level_actors()):
        if _tagged(actor):
            unreal.EditorLevelLibrary.destroy_actor(actor)

    cube = unreal.load_asset(CUBE)
    if cube is None:
        raise RuntimeError(f"missing cube mesh: {CUBE}")

    created = []
    for spec in BOXES:
        center = unreal.Vector(*spec["center"])
        extent = unreal.Vector(*spec["extent"])
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, center)
        actor.set_actor_label(spec["label"])
        actor.tags = [unreal.Name(MANAGED_TAG), unreal.Name(spec["kind"])]
        component = actor.static_mesh_component
        component.set_static_mesh(cube)
        component.set_world_scale3d(unreal.Vector(extent.x / 50.0, extent.y / 50.0, extent.z / 50.0))
        component.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        component.set_collision_profile_name("BlockAll")
        component.set_hidden_in_game(True)
        component.set_visibility(False, True)
        created.append({
            "label": spec["label"],
            "kind": spec["kind"],
            "center": list(spec["center"]),
            "extent": list(spec["extent"]),
        })

    unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    print("[GAMEXXK] " + json.dumps({"ok": True, "map": MAP, "created": created}, ensure_ascii=False))


if __name__ == "__main__":
    main()
