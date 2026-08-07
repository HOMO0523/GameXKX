from __future__ import annotations

import unreal


def main() -> None:
    spr = unreal.load_asset("/Game/GameXXK/Characters/PartyDeckNPC/TusiChief/Sprites/SPR_PartyDeckNPC_TusiChief_Idle_South_00")
    members = [m for m in dir(spr) if not m.startswith("_")]
    print("SPRITE members:", members)

    fb = unreal.load_asset("/Game/GameXXK/Characters/PartyDeckNPC/TusiChief/Flipbooks/FB_PartyDeckNPC_TusiChief_Idle_South")
    members = [m for m in dir(fb) if not m.startswith("_")]
    print("FLIPBOOK members:", members)

    # try reading PPU-like props on sprite
    for prop in ["unreal_units_per_pixel", "pixels_per_unreal_unit", "pixels_per_unit"]:
        try:
            print(f"sprite.{prop} =", spr.get_editor_property(prop))
        except Exception as exc:
            print(f"sprite.{prop}: n/a")

    # try reading KeyFrames on flipbook
    for prop in ["key_frames", "KeyFrames", "frames_per_second"]:
        try:
            print(f"flipbook.{prop} =", fb.get_editor_property(prop))
        except Exception as exc:
            print(f"flipbook.{prop}: n/a")


if __name__ == "__main__":
    main()
