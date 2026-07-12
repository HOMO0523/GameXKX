"""Create a non-destructive, lightweight review map for Eastern Village assets.

Do not load the vendor demo map here: it eagerly streams the whole 5.4 scene and
its texture set, which is excessive for selecting a town kit in UE 5.8.  This map
only loads a curated selection of complete, usable assets.
"""

from __future__ import annotations

import json
from pathlib import Path

import unreal


MAP_PATH = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Integration"
MANAGED_TAG = "QingshanAsianVillageReview"
EVIDENCE_PATH = (
    Path(__file__).resolve().parents[2]
    / "docs/production/evidence/asian-village-integration/creation-report.json"
)

# Each entry is deliberately a whole, readable asset rather than tiny roof/window
# fragments.  The gallery is a short-list for the next town dressing pass.
GALLERY = (
    ("Building_MainRoof", "/Game/Asian_Village/meshes/building/SM_roof_03.SM_roof_03", (0, 12000, 0)),
    ("Building_ThatchedRoof", "/Game/Asian_Village/meshes/building/SM_thatched_roof_01.SM_thatched_roof_01", (1800, 12000, 0)),
    ("Building_Tower", "/Game/Asian_Village/meshes/building/SM_tower_01.SM_tower_01", (3600, 12000, 0)),
    ("Building_WallWindow", "/Game/Asian_Village/meshes/building/SM_wall_window_03.SM_wall_window_03", (5400, 12000, 0)),
    ("Building_Doorway", "/Game/Asian_Village/meshes/building/SM_doorway_01.SM_doorway_01", (7200, 12000, 0)),
    ("Building_WoodenFence", "/Game/Asian_Village/meshes/building/SM_wooden_fence_01.SM_wooden_fence_01", (9000, 12000, 0)),
    ("Bridge_Complete", "/Game/Asian_Village/meshes/building/SM_bridge_01.SM_bridge_01", (0, 14500, 0)),
    ("Water_Waterfall", "/Game/Asian_Village/meshes/water/SM_waterfall.SM_waterfall", (2200, 14500, 0)),
    ("Prop_MarketTent", "/Game/Asian_Village/meshes/props/SM_market_tent_01.SM_market_tent_01", (4200, 14500, 0)),
    ("Prop_Lantern", "/Game/Asian_Village/meshes/props/SM_lantern_01.SM_lantern_01", (6000, 14500, 0)),
    ("Prop_LampPost", "/Game/Asian_Village/meshes/props/SM_lamppost_01.SM_lamppost_01", (7600, 14500, 0)),
    ("Prop_Banner", "/Game/Asian_Village/meshes/props/SM_flag_banner_01.SM_flag_banner_01", (9200, 14500, 0)),
    ("Prop_Table", "/Game/Asian_Village/meshes/props/SM_table_01.SM_table_01", (0, 17000, 0)),
    ("Prop_BambooBox", "/Game/Asian_Village/meshes/props/SM_bamboo_box_01.SM_bamboo_box_01", (1800, 17000, 0)),
    ("Prop_FlowerBed", "/Game/Asian_Village/meshes/props/SM_stone_flower_bed_01.SM_stone_flower_bed_01", (3600, 17000, 0)),
    ("Plant_Tree", "/Game/Asian_Village/meshes/trees/SM_tree_01.SM_tree_01", (5600, 17000, 0)),
    ("Plant_Bamboo", "/Game/Asian_Village/meshes/trees/SM_bamboo_01.SM_bamboo_01", (7200, 17000, 0)),
    ("Plant_Bush", "/Game/Asian_Village/meshes/plants/SM_bush_01.SM_bush_01", (8800, 17000, 0)),
    ("Plant_Flowers", "/Game/Asian_Village/meshes/plants/SM_flowers_01.SM_flowers_01", (10400, 17000, 0)),
    ("Terrain_Cliff", "/Game/Asian_Village/meshes/cliff/SM_cliff_01.SM_cliff_01", (0, 19800, 0)),
    ("Terrain_CliffTall", "/Game/Asian_Village/meshes/cliff/SM_cliff_03.SM_cliff_03", (2600, 19800, 0)),
)


def _current_map() -> str:
    world = unreal.EditorLevelLibrary.get_editor_world()
    return "" if world is None else str(world.get_outermost().get_name()).split(".", 1)[0]


def _load_level(path: str) -> None:
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not subsystem.load_level(path):
        raise RuntimeError(f"failed to load level {path}")


def _load_mesh(path: str):
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    if mesh is None:
        raise RuntimeError(f"required mesh is missing: {path}")
    return mesh


def _managed_actors():
    return [
        actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()
        if MANAGED_TAG in {str(tag) for tag in actor.get_editor_property("tags")}
    ]


def _clear_gallery() -> None:
    for actor in _managed_actors():
        if not unreal.EditorLevelLibrary.destroy_actor(actor):
            raise RuntimeError(f"could not remove prior review actor {actor.get_actor_label()}")


def _spawn_gallery() -> list[str]:
    labels = []
    for label, mesh_path, position in GALLERY:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor,
            unreal.Vector(*map(float, position)),
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        if actor is None:
            raise RuntimeError(f"failed to spawn gallery actor {label}")
        actor.set_actor_label(f"AV_Review_{label}")
        actor.set_actor_scale3d(unreal.Vector(1.0, 1.0, 1.0))
        actor.static_mesh_component.set_static_mesh(_load_mesh(mesh_path))
        actor.set_editor_property("tags", [unreal.Name(MANAGED_TAG), unreal.Name("AsianVillage")])
        labels.append(actor.get_actor_label())
    return labels


def _save() -> None:
    if not unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True):
        raise RuntimeError("failed to save Asian Village integration map")


def main() -> None:
    # Mesh existence is the lightweight dependency check.  It avoids opening the
    # vendor demo world, which can force compilation of its entire texture set.
    _load_mesh(GALLERY[0][1])
    created = False
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        if not unreal.EditorAssetLibrary.does_directory_exist("/Game/GameXXK/Maps/Prototype"):
            unreal.EditorAssetLibrary.make_directory("/Game/GameXXK/Maps/Prototype")
        if not unreal.EditorLevelLibrary.new_level(MAP_PATH):
            raise RuntimeError(f"failed to create review map: {MAP_PATH}")
        created = True
    else:
        _load_level(MAP_PATH)
    if _current_map() != MAP_PATH:
        raise RuntimeError(f"wrong current map after loading: {_current_map()}")
    _clear_gallery()
    labels = _spawn_gallery()
    _save()
    EVIDENCE_PATH.parent.mkdir(parents=True, exist_ok=True)
    report = {
        "ok": True,
        "map": MAP_PATH,
        "created_new_lightweight_map": created,
        "gallery_count": len(labels),
        "gallery_labels": labels,
        "categories": ["building", "bridge_water", "props", "3d_plants", "terrain"],
    }
    EVIDENCE_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("[GAMEXXK] " + json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
