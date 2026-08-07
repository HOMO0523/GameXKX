from __future__ import annotations

import unreal


def main() -> None:
    for stem in ["TusiChief"]:
        spr = unreal.load_asset(f"/Game/GameXXK/Characters/PartyDeckNPC/{stem}/Sprites/SPR_PartyDeckNPC_{stem}_Idle_South_00")
        if not isinstance(spr, unreal.PaperSprite):
            print("not a sprite:", spr)
            continue
        print("sprite:", spr.get_name())
        members = [m for m in dir(spr) if not m.startswith("_")]
        print("members:", [m for m in members if "uv" in m.lower() or "dimension" in m.lower() or "texture" in m.lower() or "pivot" in m.lower() or "unit" in m.lower() or "bounding" in m.lower()])
        for prop in ["source_texture", "source_uv", "source_dimension", "pivot", "unreal_units_per_pixel", "source_uvs"]:
            try:
                val = spr.get_editor_property(prop)
                if val is not None:
                    print(f"  {prop}:", val)
            except Exception as exc:
                print(f"  {prop}: error {exc}")

        tex = spr.get_editor_property("source_texture")
        if tex:
            print("  texture size:", tex.blueprint_get_size_x(), "x", tex.blueprint_get_size_y())
        # bounding rect / render geometry
        try:
            geo = spr.get_editor_property("render_geometry")
            print("  render_geometry type:", type(geo).__name__)
        except Exception as exc:
            print("  render_geometry:", exc)

    # PartyDeck 8-dir texture size
    tex = unreal.load_asset("/Game/GameXXK/Sprites/Generated/PartyDeck/T_PartyDeck_Npc_TusiChief_Idle8Dir")
    if tex:
        print("T_PartyDeck_Npc_TusiChief_Idle8Dir size:", tex.blueprint_get_size_x(), "x", tex.blueprint_get_size_y())

    # What factory options exist for PaperSprite?
    print("PaperSpriteFactory exists:", hasattr(unreal, "PaperSpriteFactory"))
    print("PaperFlipbookFactory exists:", hasattr(unreal, "PaperFlipbookFactory"))


if __name__ == "__main__":
    main()
