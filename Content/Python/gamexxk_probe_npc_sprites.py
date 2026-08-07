from __future__ import annotations

import unreal


def main() -> None:
    sprite_path = "/Game/GameXXK/Characters/Follower/Sprites/SPR_Npc_Idle_South_00"
    sprite = unreal.load_asset(sprite_path)
    print("sprite:", type(sprite).__name__ if sprite else None)
    if isinstance(sprite, unreal.PaperSprite):
        tex = sprite.get_editor_property("source_texture")
        print("source_texture:", tex.get_path_name() if tex else None)
        if tex:
            print("texture size:", tex.blueprint_get_size_x(), "x", tex.blueprint_get_size_y())

    flip_path = "/Game/GameXXK/Characters/Follower/Flipbooks/FB_Npc_Idle_South"
    flip = unreal.load_asset(flip_path)
    print("flipbook:", type(flip).__name__ if flip else None)
    if isinstance(flip, unreal.PaperFlipbook):
        try:
            mats = flip.get_editor_property("materials")
            for m in mats:
                spr = m.get_editor_property("sprite") if m else None
                print("flipbook sprite:", spr.get_path_name() if spr else None)
        except Exception as exc:
            print("flipbook materials error:", exc)

    # Any other per-NPC sprite overrides on the follower blueprint?
    bp = unreal.load_asset("/Game/GameXXK/Characters/Follower/BP_NpcCharacter")
    print("blueprint:", type(bp).__name__ if bp else None)
    if bp:
        generated = bp.generated_class()
        print("generated class:", generated.get_name() if generated else None)


if __name__ == "__main__":
    main()
