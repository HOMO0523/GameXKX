from __future__ import annotations

import unreal


def main() -> None:
    factory = unreal.PaperSpriteFactory()
    print("PaperSpriteFactory props:", [m for m in dir(factory) if not m.startswith("_")])
    for prop in ["source_texture", "SourceTexture"]:
        try:
            print(f"  factory.{prop} =", factory.get_editor_property(prop))
        except Exception as exc:
            print(f"  factory.{prop}: n/a")

    kf_struct = getattr(unreal, "PaperFlipbookKeyFrame", None)
    print("PaperFlipbookKeyFrame exists:", kf_struct is not None)
    if kf_struct:
        try:
            kf = kf_struct()
            print("  keyframe members:", [m for m in dir(kf) if not m.startswith("_")])
            for prop in ["sprite", "frame_run", "Sprite", "FrameRun"]:
                try:
                    print(f"  kf.{prop} =", kf.get_editor_property(prop))
                except Exception as exc:
                    print(f"  kf.{prop}: n/a")
        except Exception as exc:
            print("  kf err:", exc)

    # pivot property name candidates on a live sprite
    spr = unreal.load_asset("/Game/GameXXK/Characters/PartyDeckNPC/TusiChief/Sprites/SPR_PartyDeckNPC_TusiChief_Idle_South_00")
    for prop in ["pivot", "Pivot", "pivot_offset"]:
        try:
            print(f"sprite.{prop} =", spr.get_editor_property(prop))
        except Exception as exc:
            print(f"sprite.{prop}: n/a")

    # how was this sprite created? check factory class name via asset metadata
    data = unreal.EditorAssetLibrary.get_metadata_tag(spr, "AssociatedAssetType")
    print("metadata tag:", data)
    print("sprite class:", spr.get_class().get_name())


if __name__ == "__main__":
    main()
