from __future__ import annotations

import unreal


STEMS = ["TusiChief", "SongJinBao", "YueBai", "ZhouGuangZu", "JinGui", "QiongMeiEr"]
TEXTURE_ROOT = "/Game/GameXXK/Characters/Follower/Textures"
SPRITE_ROOT = "/Game/GameXXK/Characters/Follower/Sprites/FirstFrame"


def main() -> None:
    ok = True
    for stem in STEMS:
        flipbook = unreal.load_asset(f"/Game/GameXXK/Characters/PartyDeckNPC/{stem}/Flipbooks/FB_PartyDeckNPC_{stem}_Idle_South")
        sprite = unreal.load_asset(f"{SPRITE_ROOT}/SPR_Npc_{stem}_IdleFirst")
        texture = unreal.load_asset(f"{TEXTURE_ROOT}/T_Npc_{stem}_IdleFirst")
        fb_sprite = None
        if isinstance(flipbook, unreal.PaperFlipbook):
            fb_sprite = flipbook.get_sprite_at_frame(0)
        spr_tex = None
        spr_ppu = None
        spr_pivot = None
        if isinstance(sprite, unreal.PaperSprite):
            spr_tex = sprite.get_editor_property("source_texture")
            spr_ppu = sprite.get_editor_property("pixels_per_unreal_unit")
            try:
                spr_pivot = sprite.get_editor_property("custom_pivot_point")
                spr_pivot = (round(spr_pivot.x, 1), round(spr_pivot.y, 1))
            except Exception:
                spr_pivot = None
        fb_ok = fb_sprite is not None and fb_sprite.get_path_name() == sprite.get_path_name()
        spr_ok = spr_tex is not None and spr_tex.get_path_name() == texture.get_path_name()
        print(f"{stem}: flipbook->sprite {'OK' if fb_ok else 'MISMATCH'} sprite->tex {'OK' if spr_ok else 'MISMATCH'} ppu={spr_ppu} pivot={spr_pivot}")
        ok = ok and fb_ok and spr_ok
    print("ALL OK" if ok else "FAILURES PRESENT")


if __name__ == "__main__":
    main()
