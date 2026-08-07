from __future__ import annotations

import unreal


TEXTURE_ROOT = "/Game/GameXXK/Characters/Follower/Textures"
SPRITE_ROOT = "/Game/GameXXK/Characters/Follower/Sprites/FirstFrame"
OLD_CELL_HEIGHT = 205.0

# bbox analysis of SourceAssets/AnimationProcessing/Production/character_*_idle/frames/frame_0000.png (512x512)
# char box: (width_px, height_px, center_x, bottom_y)
CHAR_BOX = {
    "TusiChief": (326, 355, 223.5, 470),
    "SongJinBao": (250, 426, 276.5, 470),
    "YueBai": (251, 351, 242.0, 453),
    "ZhouGuangZu": (362, 417, 245.5, 470),
    "JinGui": (284, 351, 272.5, 469),
    "QiongMeiEr": (308, 412, 263.5, 470),
}


def _set(asset, prop, value) -> None:
    asset.set_editor_property(prop, value)


def _pivot_mode():
    try:
        return unreal.SpritePivotMode.CUSTOM
    except Exception:
        return unreal.SpritePivotMode.BOTTOM_CENTER


def main() -> None:
    unreal.EditorAssetLibrary.make_directory(SPRITE_ROOT)
    pivot_mode = _pivot_mode()
    print("pivot_mode:", pivot_mode)
    results = []
    for stem, (char_w, char_h, center_x, bottom_y) in CHAR_BOX.items():
        texture = unreal.load_asset(f"{TEXTURE_ROOT}/T_Npc_{stem}_IdleFirst")
        if not isinstance(texture, unreal.Texture2D):
            results.append((stem, "missing-texture"))
            continue

        sprite_name = f"SPR_Npc_{stem}_IdleFirst"
        sprite_path = f"{SPRITE_ROOT}/{sprite_name}"
        sprite = unreal.EditorAssetLibrary.load_asset(sprite_path)
        if sprite is None:
            sprite = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                sprite_name, SPRITE_ROOT, unreal.PaperSprite, unreal.PaperSpriteFactory())
        if not isinstance(sprite, unreal.PaperSprite):
            results.append((stem, "sprite-create-failed"))
            continue

        ppu = char_h / OLD_CELL_HEIGHT
        _set(sprite, "source_texture", texture)
        _set(sprite, "source_uv", unreal.Vector2D(0.0, 0.0))
        _set(sprite, "source_dimension", unreal.Vector2D(512.0, 512.0))
        _set(sprite, "source_texture_dimension", unreal.Vector2D(512.0, 512.0))
        _set(sprite, "pixels_per_unreal_unit", ppu)
        _set(sprite, "pivot_mode", pivot_mode)
        _set(sprite, "custom_pivot_point", unreal.Vector2D(center_x, bottom_y))
        unreal.EditorAssetLibrary.save_loaded_asset(sprite)

        flipbook_path = f"/Game/GameXXK/Characters/PartyDeckNPC/{stem}/Flipbooks/FB_PartyDeckNPC_{stem}_Idle_South"
        flipbook = unreal.EditorAssetLibrary.load_asset(flipbook_path)
        if not isinstance(flipbook, unreal.PaperFlipbook):
            results.append((stem, "flipbook-missing"))
            continue
        keyframe = unreal.PaperFlipbookKeyFrame()
        keyframe.set_editor_property("sprite", sprite)
        keyframe.set_editor_property("frame_run", 1)
        _set(flipbook, "frames_per_second", 1.0)
        _set(flipbook, "key_frames", [keyframe])
        invalidate = getattr(flipbook, "invalidate_cached_data", None)
        if callable(invalidate):
            invalidate()
        unreal.EditorAssetLibrary.save_loaded_asset(flipbook)

        size = texture.blueprint_get_size_x()
        results.append((stem, f"ok ppu={ppu:.4f} pivot=({center_x},{bottom_y}) tex={size}x{size}"))
    for name, status in results:
        print(name, status)


if __name__ == "__main__":
    main()
