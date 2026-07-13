"""Open the existing B1 town scene at its authored core review camera."""

from __future__ import annotations

import sys

import unreal


MAP_PATH = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1"
DEFAULT_CAMERA_LABEL = "CAM_QS_B1_TOWN_CORE"


def main() -> None:
    camera_label = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_CAMERA_LABEL
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError(f"cannot load B1 map {MAP_PATH}")
    matches = [
        actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()
        if str(actor.get_actor_label()) == camera_label
    ]
    if len(matches) != 1 or not isinstance(matches[0], unreal.CameraActor):
        raise RuntimeError(f"expected one B1 review camera {camera_label}, got {len(matches)}")
    camera = matches[0]
    component = camera.get_editor_property("camera_component")
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(
        camera.get_actor_location(),
        camera.get_actor_rotation(),
    )
    print(f"[GAMEXXK] opened={MAP_PATH} camera={camera_label} fov={component.get_editor_property('field_of_view')}")


if __name__ == "__main__":
    main()
