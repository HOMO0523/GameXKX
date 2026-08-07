from __future__ import annotations

import unreal


def main() -> None:
    # 1. What does BP_NpcCharacter (Follower) use by default?
    bp = unreal.load_asset("/Game/GameXXK/Characters/Follower/BP_NpcCharacter")
    print("BP_NpcCharacter:", type(bp).__name__ if bp else None)
    if bp:
        generated = bp.generated_class()
        print("  generated:", generated.get_name() if generated else None)
        try:
            cdo = unreal.get_default_object(generated)
            if cdo:
                print("  NpcId:", cdo.get_editor_property("NpcId"))
                vis = cdo.get_editor_property("Visual")
                if vis:
                    fb = vis.get_editor_property("flipbook")
                    print("  default flipbook:", fb.get_path_name() if fb else None)
        except Exception as exc:
            print("  cdo error:", exc)

    # 2. PartyDeckNPC idle flipbook -> sprite -> source texture
    for stem in ["TusiChief", "SongJinBao", "YueBai", "ZhouGuangZu", "JinGui", "QiongMeiEr"]:
        fb_path = f"/Game/GameXXK/Characters/PartyDeckNPC/{stem}/Flipbooks/FB_PartyDeckNPC_{stem}_Idle_South"
        fb = unreal.load_asset(fb_path)
        tex_name = "None"
        if isinstance(fb, unreal.PaperFlipbook):
            try:
                mats = fb.get_editor_property("materials")
                sprites = []
                for m in mats:
                    spr = m.get_editor_property("sprite") if m else None
                    if spr:
                        sprites.append(spr)
                if sprites:
                    src = sprites[0].get_editor_property("source_texture")
                    tex_name = src.get_name() if src else "no-source"
                else:
                    tex_name = "no-sprite"
            except Exception as exc:
                tex_name = f"error:{exc}"
        print(f"{stem}: idle flipbook sprite texture = {tex_name}")

    # 3. Blueprints under PartyDeckNPC per NPC
    lib = unreal.EditorAssetLibrary
    for stem in ["TusiChief", "SongJinBao", "YueBai", "ZhouGuangZu", "JinGui", "QiongMeiEr"]:
        base = f"/Game/GameXXK/Characters/PartyDeckNPC/{stem}"
        assets = lib.list_assets(base, recursive=True)
        bps = [a for a in assets if "BP_" in a]
        print(f"{stem}: PartyDeckNPC assets={len(assets)} bps={bps[:5]}")

    # 4. Town level placed NPC actors
    editor_subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    level_paths = [
        "/Game/GameXXK/Levels/L_Qingshan_AsianVillage_Demo",
        "/Game/GameXXK/Maps/L_Qingshan_AsianVillage_Demo",
        "/Game/Levels/L_Qingshan_AsianVillage_Demo",
    ]
    found_level = None
    for lp in level_paths:
        if lib.does_asset_exist(lp):
            found_level = lp
            break
    print("town level:", found_level)
    if found_level:
        try:
            editor_subsys.load_level(found_level)
            world = editor_subsys.get_editor_world()
            print("editor world:", world.get_name() if world else None)
        except Exception as exc:
            print("level load error:", exc)


if __name__ == "__main__":
    main()
