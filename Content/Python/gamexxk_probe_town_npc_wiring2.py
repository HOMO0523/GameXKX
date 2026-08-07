from __future__ import annotations

import unreal


def _props(obj) -> list[str]:
    try:
        return [p.get_editor_property("name") for p in obj.get_editor_property_names()]
    except Exception:
        try:
            return list(obj.get_editor_property_names())
        except Exception as exc:
            return [f"error:{exc}"]


def main() -> None:
    lib = unreal.EditorAssetLibrary

    # Flipbook internals: list actual property names
    fb = unreal.load_asset("/Game/GameXXK/Characters/PartyDeckNPC/TusiChief/Flipbooks/FB_PartyDeckNPC_TusiChief_Idle_South")
    if isinstance(fb, unreal.PaperFlipbook):
        print("flipbook props:", [p for p in _props(fb) if "sprite" in p.lower() or "frame" in p.lower() or "material" in p.lower()])

    # Level placed NPC actors
    level = "/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"
    editor_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    old_level = editor_subsys.get_current_level()
    print("current level:", old_level.get_name() if old_level else None)
    try:
        editor_subsys.load_level(level)
        world = editor_subsys.get_editor_world()
        print("loaded:", world.get_name() if world else None)
        actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
        npc_actors = []
        for a in actors:
            name = a.get_name()
            cls = a.get_class().get_name()
            if "Npc" in name or "Npc" in cls or "npc" in name:
                npc_actors.append((name, cls))
        for name, cls in sorted(npc_actors):
            print("PLACED:", name, "->", cls)
    except Exception as exc:
        print("level error:", exc)


if __name__ == "__main__":
    main()
