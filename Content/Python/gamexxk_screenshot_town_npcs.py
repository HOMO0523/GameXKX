from __future__ import annotations

import os
from pathlib import Path

import unreal


LEVEL = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"
OUTPUT_DIR = str(Path(r"D:\UE5 demo\GameXXK\outputs\NpcCheck").resolve())


def main() -> None:
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    editor_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    editor_subsys.load_level(LEVEL)
    world = unreal.UnrealEditorSubsystem().get_editor_world()
    if not world:
        print("no editor world")
        return

    npc_ids = ["Npc.TusiChief", "Npc.SongJinBao", "Npc.YueBai", "Npc.ZhouGuangZu", "Npc.JinGui", "Npc.QiongMeiEr"]
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.GameXXKTownNpcCharacter)
    by_id = {}
    for a in actors:
        try:
            npc_id = a.get_editor_property("NpcId")
        except Exception:
            npc_id = None
        if npc_id in npc_ids:
            by_id[str(npc_id)] = a

    import math
    import time

    for npc_id in npc_ids:
        a = by_id.get(npc_id)
        if not a:
            print(npc_id, "actor missing")
            continue
        loc = a.get_actor_location()
        yaw = -90.0
        pitch = -45.0
        dist = 320.0
        cam_loc = unreal.Vector(loc.x + dist * math.cos(math.radians(-yaw)) * math.cos(math.radians(-pitch)),
                                loc.y + dist * math.sin(math.radians(-yaw)) * math.cos(math.radians(-pitch)),
                                loc.z + 180.0 + dist * math.sin(math.radians(-pitch)))
        rot = unreal.Rotator(-pitch, yaw, 0.0)
        for key in ("LevelViewport", "Viewport", "EditorViewport"):
            try:
                editor_subsys.set_level_viewport_camera_info(cam_loc, rot, key)
                print(npc_id, "camera key:", key)
                break
            except Exception as exc:
                print(npc_id, "camera err(", key, "):", exc)
        path = f"{OUTPUT_DIR}/{npc_id.split('.')[1]}.png"
        try:
            unreal.AutomationLibrary.take_high_res_screenshot(1280, 720, path)
            print(npc_id, "shot ->", path)
        except Exception as exc:
            print(npc_id, "shot err:", exc)
        time.sleep(1.5)


if __name__ == "__main__":
    main()
