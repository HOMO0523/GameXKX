"""Downscale-render an atlas texture and emit a perceptual hash.

Driven through UE MCP. Draws the named atlas into a 64x64 render target via a
transient material, reads the render target pixels, and returns a compact hash.
"""

from __future__ import annotations

import json
import sys

import unreal


def main(argv: list[str]) -> dict:
    name = argv[0] if argv else "character_00_hero_attack"
    package = f"/Game/GameXXK/BattleAnimations/Atlases/T_{name}_atlas"
    texture = unreal.load_asset(package)
    info = {"name": name, "loaded": texture is not None}
    if texture is None:
        return info

    editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor_subsystem.get_game_world()
    if world is None:
        world = editor_subsystem.get_editor_world()
    if world is None:
        info["error"] = "no_world"
        return info

    mat = None
    try:
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        mat = asset_tools.create_asset(
            f"M_Probe_{name}", "/Game/Temp", unreal.Material, unreal.MaterialFactoryNew())
        ts = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionTextureSample, 0, 0)
        ts.set_editor_property("texture", texture)
        unreal.MaterialEditingLibrary.connect_material_property(
            ts, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        unreal.MaterialEditingLibrary.recompile_material(mat)

        size = 64
        rt = unreal.RenderingLibrary.create_render_target2d(world, size, size)
        unreal.RenderingLibrary.clear_render_target2d(world, rt, [0, 0, 0, 1])
        unreal.RenderingLibrary.draw_material_to_render_target(world, rt, mat)

        pixels = unreal.RenderingLibrary.read_render_target(world, rt)
        info["pixel_count"] = len(pixels)
        cells = []
        for p in pixels:
            cells.append(f"{p.b >> 4:x}{p.g >> 4:x}{p.r >> 4:x}")
        info["hash"] = "".join(cells)
    except Exception as exc:  # pragma: no cover
        info["error"] = str(exc)
    finally:
        if mat is not None:
            try:
                unreal.EditorAssetLibrary.delete_asset(mat.get_path_name())
            except Exception:  # pragma: no cover
                pass
    return info


if __name__ == "__main__":
    print(json.dumps(main(sys.argv[1:]), ensure_ascii=False))
