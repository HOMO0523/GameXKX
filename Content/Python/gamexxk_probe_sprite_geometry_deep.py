from __future__ import annotations

import unreal


def _dump(obj, indent: str = "", max_depth: int = 2) -> None:
    members = [m for m in dir(obj) if not m.startswith("_")]
    print(f"{indent}{type(obj).__name__} members: {members}")
    if max_depth <= 0:
        return
    for prop in members:
        try:
            val = obj.get_editor_property(prop)
        except Exception:
            continue
        if val is None or isinstance(val, (bool, int, float, str)):
            print(f"{indent}  {prop} = {val}")
        elif isinstance(val, (list, tuple)):
            print(f"{indent}  {prop}: list[{len(val)}]")
            if len(val) and max_depth > 1:
                first = val[0]
                if hasattr(first, "get_editor_property"):
                    _dump(first, indent + "    ", max_depth - 1)
        elif hasattr(val, "get_editor_property"):
            print(f"{indent}  {prop}: struct {type(val).__name__}")
            _dump(val, indent + "    ", max_depth - 1)
        else:
            print(f"{indent}  {prop}: {type(val).__name__}")


def main() -> None:
    spr = unreal.load_asset("/Game/GameXXK/Characters/PartyDeckNPC/TusiChief/Sprites/SPR_PartyDeckNPC_TusiChief_Idle_South_00")
    try:
        geo = spr.get_editor_property("render_geometry")
        print("render_geometry:", type(geo).__name__)
        _dump(geo, "  ", 3)
    except Exception as exc:
        print("render_geometry err:", exc)

    # also check bounding / collision source? check what other editor props exist
    for prop in ["pixels_per_unreal_unit", "source_uv", "source_dimension", "source_texture"]:
        try:
            val = spr.get_editor_property(prop)
            if isinstance(val, unreal.Vector2D):
                print(f"{prop} = ({val.x}, {val.y})")
            else:
                print(f"{prop} = {val}")
        except Exception as exc:
            print(f"{prop}: err {exc}")


if __name__ == "__main__":
    main()
