from __future__ import annotations

import unreal


def main() -> None:
    lib = unreal.EditorAssetLibrary

    fb = unreal.load_asset("/Game/GameXXK/Characters/PartyDeckNPC/TusiChief/Flipbooks/FB_PartyDeckNPC_TusiChief_Idle_South")
    if isinstance(fb, unreal.PaperFlipbook):
        members = [m for m in dir(fb) if not m.startswith("_")]
        interesting = [m for m in members if "frame" in m.lower() or "material" in m.lower() or "sprite" in m.lower()]
        print("flipbook interesting members:", interesting)
        try:
            print("num_frames:", fb.get_num_frames() if hasattr(fb, "get_num_frames") else "n/a")
        except Exception as exc:
            print("num_frames err:", exc)
        try:
            spr = fb.get_sprite_at_frame(0)
            print("sprite at frame 0:", spr.get_path_name() if spr else None)
            if spr:
                tex = spr.get_editor_property("source_texture")
                print("source texture:", tex.get_path_name() if tex else None)
        except Exception as exc:
            print("sprite err:", exc)

    # Level placed NPC actors via UnrealEditorSubsystem
    level = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"
    editor_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    try:
        editor_subsys.load_level(level)
        print("level loaded:", level)
    except Exception as exc:
        print("level load error:", exc)
        return

    world = unreal.UnrealEditorSubsystem().get_editor_world()
    if not world:
        print("no editor world")
        return
    print("world:", world.get_name())
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    print("total actors:", len(actors))
    for a in actors:
        name = a.get_name()
        cls = a.get_class().get_name()
        if "Npc" in name or "Npc" in cls or "npc" in name or "Quest" in cls:
            print("PLACED:", name, "->", cls)
            try:
                npc_id = a.get_editor_property("NpcId")
                print("   NpcId:", npc_id)
            except Exception:
                pass
            try:
                vis_class = a.get_editor_property("VisualCharacterClass")
                print("   VisualCharacterClass:", vis_class.get_path_name() if vis_class else None)
            except Exception:
                pass


if __name__ == "__main__":
    main()
