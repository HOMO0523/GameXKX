"""Switch B1 review cameras without reloading the editor world."""

from __future__ import annotations

import sys

import unreal


B1_MAP = "/Game/GameXXK/Maps/Dev/L_Qingshan_PCG_Dress_B1"


def main() -> None:
    label = sys.argv[1] if len(sys.argv) > 1 else "CAM_QS_B1_TOWN_CORE"
    world = unreal.EditorLevelLibrary.get_editor_world()
    current = "" if world is None else str(world.get_outermost().get_path_name()).split(".", 1)[0]
    if current != B1_MAP:
        raise RuntimeError(f"B1 must already be open; refusing to reload map from {current}")
    matches = [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors() if str(actor.get_actor_label()) == label]
    if len(matches) != 1 or not isinstance(matches[0], unreal.CameraActor):
        raise RuntimeError(f"expected exactly one B1 camera {label}, got {len(matches)}")
    camera = matches[0]
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(camera.get_actor_location(), camera.get_actor_rotation())
    print(f"[GAMEXXK] current={current} camera={label}")


if __name__ == "__main__":
    main()
